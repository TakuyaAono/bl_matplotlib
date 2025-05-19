/* -*- mode: c++; c-basic-offset: 4 -*- */

#ifndef MPL_PATH_CONVERTERS_H
#define MPL_PATH_CONVERTERS_H

#include <pybind11/pybind11.h>

#include <cmath>
#include <cstdint>
#include <limits>

#include "agg_clip_liang_barsky.h"
#include "mplutils.h"
#include "agg_conv_segmentator.h"

/*
 * パス変換の主要な機能
 *
 * このファイルには、パスを修正する頂点コンバーターが含まれています。
 * これらはすべてイテレーターとして機能し、出力はオンザフライで生成され、
 * 完全なデータのコピーは必要ありません。
 *
 * 各クラスは「パスクリーニング」パイプラインの個別のステップを表します。
 * これらは現在、AGGバックエンドで以下の順序で適用されています：
 *
 * 1. アフィン変換（AGGで実装、ここには含まれません）
 *
 * 2. PathNanRemover: NaNを含むセグメントをスキップし、
 *    MOVETOコマンドを挿入します
 *
 * 3. PathClipper: 線分を指定された矩形にクリップします。
 *    これはデータ削減に役立ち、またAGGの座標が24ビット符号付き整数を
 *    超えることができないという制限を回避するのにも役立ちます。
 *
 * 4. PathSnapper: パスを最も近い中心ピクセルに丸めます。
 *    これにより、直線的な曲線がより良く見えます。
 *
 * 5. PathSimplifier: 高密度パスから見た目に影響を与えない線分を削除します。
 *    レンダリングを高速化し、ファイルサイズを削減します。
 *
 * 6. 曲線から線分への変換（AGGで実装、ここには含まれません）
 *
 * 7. ストローク（AGGで実装、ここには含まれません）
 */

/************************************************************
 * 出力をキューに入れる必要がある頂点コンバーターの基底クラスです。
 * STLのキューよりも柔軟性は低いですが、可能な限り高速に動作するように
 * 設計されています。
 */
template <int QueueSize>
class EmbeddedQueue
{
  protected:
    // コンストラクタ：キューを初期化
    EmbeddedQueue() : m_queue_read(0), m_queue_write(0)
    {
        // 空の実装
    }

    // キューアイテムの構造体
    struct item
    {
        item()
        {
        }

        // アイテムの値を設定
        inline void set(const unsigned cmd_, const double x_, const double y_)
        {
            cmd = cmd_;
            x = x_;
            y = y_;
        }
        unsigned cmd;  // コマンド
        double x;      // X座標
        double y;      // Y座標
    };
    int m_queue_read;   // キュー読み取り位置
    int m_queue_write;  // キュー書き込み位置
    item m_queue[QueueSize];  // キュー配列

    // キューにアイテムを追加
    inline void queue_push(const unsigned cmd, const double x, const double y)
    {
        m_queue[m_queue_write++].set(cmd, x, y);
    }

    // キューが空でないかチェック
    inline bool queue_nonempty()
    {
        return m_queue_read < m_queue_write;
    }

    // キューからアイテムを取り出す
    inline bool queue_pop(unsigned *cmd, double *x, double *y)
    {
        if (queue_nonempty()) {
            const item &front = m_queue[m_queue_read++];
            *cmd = front.cmd;
            *x = front.x;
            *y = front.y;

            return true;
        }

        m_queue_read = 0;
        m_queue_write = 0;

        return false;
    }

    // キューをクリア
    inline void queue_clear()
    {
        m_queue_read = 0;
        m_queue_write = 0;
    }
};

/* パスセグメントタイプごとの追加頂点数を定義 */
static const size_t num_extra_points_map[] =
    {0, 0, 0, 1,
     2, 0, 0, 0,
     0, 0, 0, 0,
     0, 0, 0, 0
    };

/* 単純な線形合同法による乱数生成器の実装
 * これは「古典的」で高速なRNGで、線のスケッチには適していますが、
 * 暗号化など重要な用途には使用すべきではありません。
 * サードパーティのコードとシード状態を共有しないように、
 * C標準ライブラリではなく独自に実装しています。
 * 最近のC++オプションもありますが、互換性の理由から
 * C++98以降の機能は使用していません。
 */
class RandomNumberGenerator
{
private:
    /* これらはMS Visual C++と同じ定数で、
     * モジュラスが2^32であるという利点があり、
     * 明示的なモジュロ演算を省略できます
    */
    static const uint32_t a = 214013;
    static const uint32_t c = 2531011;
    uint32_t m_seed;  // シード値

public:
    // デフォルトコンストラクタ
    RandomNumberGenerator() : m_seed(0) {}
    // シードを指定するコンストラクタ
    RandomNumberGenerator(int seed) : m_seed(seed) {}

    // シードを設定
    void seed(int seed)
    {
        m_seed = seed;
    }

    // 0から1の間の乱数を生成
    double get_double()
    {
        m_seed = (a * m_seed + c);
        return (double)m_seed / (double)(1LL << 32);
    }
};

/*
 * PathNanRemoverは、頂点リストから非有限値（NaN）を削除し、
 * 必要に応じてMOVETOコマンドを挿入する頂点コンバーターです。
 * 曲線セグメントに少なくとも1つの非有限値が含まれている場合、
 * その曲線セグメント全体がスキップされます。
 */
template <class VertexSource>
class PathNanRemover : protected EmbeddedQueue<4>
{
    VertexSource *m_source;  // 頂点ソース
    bool m_remove_nans;      // NaNを削除するかどうか
    bool m_has_codes;        // パスにコードが含まれているかどうか
    bool valid_segment_exists;  // 有効なセグメントが存在するかどうか
    bool m_last_segment_valid;  // 最後のセグメントが有効かどうか
    bool m_was_broken;          // パスが途切れているかどうか
    double m_initX;             // 初期X座標
    double m_initY;             // 初期Y座標

  public:
    /* has_codesは、パスにベジェ曲線セグメントや閉じたループが
     * 含まれている場合にtrueに設定する必要があります。
     * これにより、NaNを削除するためのより遅いアルゴリズムが
     * 使用されます。不明な場合はtrueに設定してください。
     */
    PathNanRemover(VertexSource &source, bool remove_nans, bool has_codes)
        : m_source(&source), m_remove_nans(remove_nans), m_has_codes(has_codes),
          m_last_segment_valid(false), m_was_broken(false),
          m_initX(nan("")), m_initY(nan(""))
    {
        // 最初の有効な（NaNを含まない）コマンドが検出されるまで、
        // すべてのclose/end_polyコマンドを無視
        valid_segment_exists = false;
    }

    // パスを最初から再開
    inline void rewind(unsigned path_id)
    {
        queue_clear();
        m_source->rewind(path_id);
    }

    // 次の頂点を取得
    inline unsigned vertex(double *x, double *y)
    {
        unsigned code;

        if (!m_remove_nans) {
            return m_source->vertex(x, y);
        }

        if (m_has_codes) {
            /* 曲線や閉じたループが含まれる可能性がある場合の
             * 遅いメソッド */
            if (queue_pop(&code, x, y)) {
                return code;
            }

            bool needs_move_to = false;
            while (true) {
                /* ここでのアプローチは、各曲線セグメント全体を
                 * キューにプッシュすることです。途中で非有限値が
                 * 見つかった場合、キューは空になり、次の曲線
                 * セグメントが処理されます。 */
                code = m_source->vertex(x, y);
                /* STOPとCLOSEPOLYに付随する頂点は使用されないため、
                 * NaNであってもそのままにします。 */
                if (code == agg::path_cmd_stop) {
                    return code;
                } else if (code == (agg::path_cmd_end_poly |
                                    agg::path_flags_close) &&
                           valid_segment_exists) {
                    /* ただし、CLOSEPOLYは有効なMOVETOコマンドが
                     * 既に出力されている場合にのみ意味があります。
                     * パスでNaNが削除された場合、ループではなくなった
                     * ため、閉じることができません。代わりにLINETOを
                     * 挿入してエミュレートする必要があります。 */
                    if (m_was_broken) {
                        if (m_last_segment_valid && (
                                std::isfinite(m_initX) &&
                                std::isfinite(m_initY))) {
                            /* 両端が有効な場合は開始点に接続 */
                            queue_push(agg::path_cmd_line_to, m_initX, m_initY);
                            break;
                        } else {
                            /* 追加のサブパスがある場合に備えて
                             * クローズをスキップ */
                            continue;
                        }
                        m_was_broken = false;
                        break;
                    } else {
                        return code;
                    }
                } else if (code == agg::path_cmd_move_to) {
                    /* ループを途切れた場合に最後のセグメントを
                     * 閉じるために初期点を保存 */
                    m_initX = *x;
                    m_initY = *y;
                    valid_segment_exists = true;
                    m_was_broken = false;
                    m_last_segment_valid = true;
                    return code;
                }

                if (needs_move_to) {
                    queue_push(agg::path_cmd_move_to, *x, *y);
                }

                size_t num_extra_points = num_extra_points_map[code & 0xF];
                m_last_segment_valid = (std::isfinite(*x) && std::isfinite(*y));
                queue_push(code, *x, *y);

                /* Note: this test cannot be short-circuited, since we need to
                   advance through the entire curve no matter what */
                for (size_t i = 0; i < num_extra_points; ++i) {
                    m_source->vertex(x, y);
                    m_last_segment_valid = m_last_segment_valid &&
                        (std::isfinite(*x) && std::isfinite(*y));
                    queue_push(code, *x, *y);
                }

                if (m_last_segment_valid) {
                    valid_segment_exists = true;
                    break;
                }

                m_was_broken = true;
                queue_clear();

                /* If the last point is finite, we use that for the
                   moveto, otherwise, we'll use the first vertex of
                   the next curve. */
                if (std::isfinite(*x) && std::isfinite(*y)) {
                    queue_push(agg::path_cmd_move_to, *x, *y);
                    needs_move_to = false;
                } else {
                    needs_move_to = true;
                }
            }

            if (queue_pop(&code, x, y)) {
                return code;
            } else {
                return agg::path_cmd_stop;
            }
        } else // !m_has_codes
        {
            /* This is the fast path for when we know we have no codes. */
            code = m_source->vertex(x, y);

            if (code == agg::path_cmd_stop ||
                (code == (agg::path_cmd_end_poly | agg::path_flags_close) &&
                 valid_segment_exists)) {
                return code;
            }

            if (!(std::isfinite(*x) && std::isfinite(*y))) {
                do {
                    code = m_source->vertex(x, y);
                    if (code == agg::path_cmd_stop ||
                        (code == (agg::path_cmd_end_poly | agg::path_flags_close) &&
                         valid_segment_exists)) {
                        return code;
                    }
                } while (!(std::isfinite(*x) && std::isfinite(*y)));
                return agg::path_cmd_move_to;
            }
            valid_segment_exists = true;
            return code;
        }
    }
};

/************************************************************
 PathClipper uses the Liang-Barsky line clipping algorithm (as
 implemented in Agg) to clip the path to a given rectangle.  Lines
 will never extend outside of the rectangle.  Curve segments are not
 clipped, but are always included in their entirety.
 */
template <class VertexSource>
class PathClipper : public EmbeddedQueue<3>
{
    VertexSource *m_source;
    bool m_do_clipping;
    agg::rect_base<double> m_cliprect;
    double m_lastX;
    double m_lastY;
    bool m_moveto;
    double m_initX;
    double m_initY;
    bool m_has_init;
    bool m_was_clipped;

  public:
    PathClipper(VertexSource &source, bool do_clipping, double width, double height)
        : m_source(&source),
          m_do_clipping(do_clipping),
          m_cliprect(-1.0, -1.0, width + 1.0, height + 1.0),
          m_lastX(nan("")),
          m_lastY(nan("")),
          m_moveto(true),
          m_initX(nan("")),
          m_initY(nan("")),
          m_has_init(false),
          m_was_clipped(false)
    {
        // empty
    }

    PathClipper(VertexSource &source, bool do_clipping, const agg::rect_base<double> &rect)
        : m_source(&source),
          m_do_clipping(do_clipping),
          m_cliprect(rect),
          m_lastX(nan("")),
          m_lastY(nan("")),
          m_moveto(true),
          m_initX(nan("")),
          m_initY(nan("")),
          m_has_init(false),
          m_was_clipped(false)
    {
        m_cliprect.x1 -= 1.0;
        m_cliprect.y1 -= 1.0;
        m_cliprect.x2 += 1.0;
        m_cliprect.y2 += 1.0;
    }

    inline void rewind(unsigned path_id)
    {
        m_has_init = false;
        m_was_clipped = false;
        m_moveto = true;
        m_source->rewind(path_id);
    }

    int draw_clipped_line(double x0, double y0, double x1, double y1,
                          bool closed=false)
    {
        unsigned moved = agg::clip_line_segment(&x0, &y0, &x1, &y1, m_cliprect);
        // moved >= 4 - Fully clipped
        // moved & 1 != 0 - First point has been moved
        // moved & 2 != 0 - Second point has been moved
        m_was_clipped = m_was_clipped || (moved != 0);
        if (moved < 4) {
            if (moved & 1 || m_moveto) {
                queue_push(agg::path_cmd_move_to, x0, y0);
            }
            queue_push(agg::path_cmd_line_to, x1, y1);
            if (closed && !m_was_clipped) {
                // Close the path only if the end point hasn't moved.
                queue_push(agg::path_cmd_end_poly | agg::path_flags_close,
                           x1, y1);
            }

            m_moveto = false;
            return 1;
        }

        return 0;
    }

    unsigned vertex(double *x, double *y)
    {
        unsigned code;
        bool emit_moveto = false;

        if (!m_do_clipping) {
            // If not doing any clipping, just pass along the vertices verbatim
            return m_source->vertex(x, y);
        }

        /* This is the slow path where we actually do clipping */

        if (queue_pop(&code, x, y)) {
            return code;
        }

        while ((code = m_source->vertex(x, y)) != agg::path_cmd_stop) {
            emit_moveto = false;

            switch (code) {
            case (agg::path_cmd_end_poly | agg::path_flags_close):
                if (m_has_init) {
                    // Queue the line from last point to the initial point, and
                    // if never clipped, add a close code.
                    draw_clipped_line(m_lastX, m_lastY, m_initX, m_initY,
                                      true);
                } else {
                    // An empty path that is immediately closed.
                    queue_push(
                        agg::path_cmd_end_poly | agg::path_flags_close,
                        m_lastX, m_lastY);
                }
                // If paths were not clipped, then the above code queued
                // something, and we should exit the loop. Otherwise, continue
                // to the next point, as there may be a new subpath.
                if (queue_nonempty()) {
                    goto exit_loop;
                }
                break;

            case agg::path_cmd_move_to:

                // was the last command a moveto (and we have
                // seen at least one command ?
                // if so, shove it in the queue if in clip box
                if (m_moveto && m_has_init &&
                    m_lastX >= m_cliprect.x1 &&
                    m_lastX <= m_cliprect.x2 &&
                    m_lastY >= m_cliprect.y1 &&
                    m_lastY <= m_cliprect.y2) {
                    // push the last moveto onto the queue
                    queue_push(agg::path_cmd_move_to, m_lastX, m_lastY);
                    // flag that we need to emit it
                    emit_moveto = true;
                }
                // update the internal state for this moveto
                m_initX = m_lastX = *x;
                m_initY = m_lastY = *y;
                m_has_init = true;
                m_moveto = true;
                m_was_clipped = false;
                // if the last command was moveto exit the loop to emit the code
                if (emit_moveto) {
                    goto exit_loop;
                }
                // else, break and get the next point
                break;

            case agg::path_cmd_line_to:
                if (draw_clipped_line(m_lastX, m_lastY, *x, *y)) {
                    m_lastX = *x;
                    m_lastY = *y;
                    goto exit_loop;
                }
                m_lastX = *x;
                m_lastY = *y;
                break;

            default:
                if (m_moveto) {
                    queue_push(agg::path_cmd_move_to, m_lastX, m_lastY);
                    m_moveto = false;
                }

                queue_push(code, *x, *y);
                m_lastX = *x;
                m_lastY = *y;
                goto exit_loop;
            }
        }

        exit_loop:

            if (queue_pop(&code, x, y)) {
                return code;
            }

            if (m_moveto && m_has_init &&
                m_lastX >= m_cliprect.x1 &&
                m_lastX <= m_cliprect.x2 &&
                m_lastY >= m_cliprect.y1 &&
                m_lastY <= m_cliprect.y2) {
                *x = m_lastX;
                *y = m_lastY;
                m_moveto = false;
                return agg::path_cmd_move_to;
            }

            return agg::path_cmd_stop;
    }
};

/************************************************************
 PathSnapper rounds vertices to their nearest center-pixels.  This
 makes rectilinear paths (rectangles, horizontal and vertical lines
 etc.) look much cleaner.
*/
enum e_snap_mode {
    SNAP_AUTO,
    SNAP_FALSE,
    SNAP_TRUE
};

namespace PYBIND11_NAMESPACE { namespace detail {
    template <> struct type_caster<e_snap_mode> {
    public:
        PYBIND11_TYPE_CASTER(e_snap_mode, const_name("e_snap_mode"));

        bool load(handle src, bool) {
            if (src.is_none()) {
                value = SNAP_AUTO;
                return true;
            }

            value = src.cast<bool>() ? SNAP_TRUE : SNAP_FALSE;

            return true;
        }
    };
}} // namespace PYBIND11_NAMESPACE::detail

template <class VertexSource>
class PathSnapper
{
  private:
    VertexSource *m_source;
    bool m_snap;
    double m_snap_value;

    static bool should_snap(VertexSource &path, e_snap_mode snap_mode, unsigned total_vertices)
    {
        // If this contains only straight horizontal or vertical lines, it should be
        // snapped to the nearest pixels
        double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        unsigned code;

        switch (snap_mode) {
        case SNAP_AUTO:
            if (total_vertices > 1024) {
                return false;
            }

            code = path.vertex(&x0, &y0);
            if (code == agg::path_cmd_stop) {
                return false;
            }

            while ((code = path.vertex(&x1, &y1)) != agg::path_cmd_stop) {
                switch (code) {
                case agg::path_cmd_curve3:
                case agg::path_cmd_curve4:
                    return false;
                case agg::path_cmd_line_to:
                    if (fabs(x0 - x1) >= 1e-4 && fabs(y0 - y1) >= 1e-4) {
                        return false;
                    }
                }
                x0 = x1;
                y0 = y1;
            }

            return true;
        case SNAP_FALSE:
            return false;
        case SNAP_TRUE:
            return true;
        }

        return false;
    }

  public:
    /*
      snap_mode should be one of:
        - SNAP_AUTO: Examine the path to determine if it should be snapped
        - SNAP_TRUE: Force snapping
        - SNAP_FALSE: No snapping
    */
    PathSnapper(VertexSource &source,
                e_snap_mode snap_mode,
                unsigned total_vertices = 15,
                double stroke_width = 0.0)
        : m_source(&source)
    {
        m_snap = should_snap(source, snap_mode, total_vertices);

        if (m_snap) {
            int is_odd = mpl_round_to_int(stroke_width) % 2;
            m_snap_value = (is_odd) ? 0.5 : 0.0;
        }

        source.rewind(0);
    }

    inline void rewind(unsigned path_id)
    {
        m_source->rewind(path_id);
    }

    inline unsigned vertex(double *x, double *y)
    {
        unsigned code;
        code = m_source->vertex(x, y);
        if (m_snap && agg::is_vertex(code)) {
            *x = floor(*x + 0.5) + m_snap_value;
            *y = floor(*y + 0.5) + m_snap_value;
        }
        return code;
    }

    inline bool is_snapping()
    {
        return m_snap;
    }
};

/************************************************************
 PathSimplifier reduces the number of vertices in a dense path without
 changing its appearance.
*/
template <class VertexSource>
class PathSimplifier : protected EmbeddedQueue<9>
{
  public:
    /* Set simplify to true to perform simplification */
    PathSimplifier(VertexSource &source, bool do_simplify, double simplify_threshold)
        : m_source(&source),
          m_simplify(do_simplify),
          /* we square simplify_threshold so that we can compute
             norms without doing the square root every step. */
          m_simplify_threshold(simplify_threshold * simplify_threshold),

          m_moveto(true),
          m_after_moveto(false),
          m_clipped(false),

          // whether the most recent MOVETO vertex is valid
          m_has_init(false),

          // the most recent MOVETO vertex
          m_initX(0.0),
          m_initY(0.0),

          // the x, y values from last iteration
          m_lastx(0.0),
          m_lasty(0.0),

          // the dx, dy comprising the original vector, used in conjunction
          // with m_currVecStart* to define the original vector.
          m_origdx(0.0),
          m_origdy(0.0),

          // the squared norm of the original vector
          m_origdNorm2(0.0),

          // maximum squared norm of vector in forward (parallel) direction
          m_dnorm2ForwardMax(0.0),
          // maximum squared norm of vector in backward (anti-parallel) direction
          m_dnorm2BackwardMax(0.0),

          // was the last point the furthest from lastWritten in the
          // forward (parallel) direction?
          m_lastForwardMax(false),
          // was the last point the furthest from lastWritten in the
          // backward (anti-parallel) direction?
          m_lastBackwardMax(false),

          // added to queue when _push is called
          m_nextX(0.0),
          m_nextY(0.0),

          // added to queue when _push is called if any backwards
          // (anti-parallel) vectors were observed
          m_nextBackwardX(0.0),
          m_nextBackwardY(0.0),

          // start of the current vector that is being simplified
          m_currVecStartX(0.0),
          m_currVecStartY(0.0)
    {
        // empty
    }

    inline void rewind(unsigned path_id)
    {
        queue_clear();
        m_moveto = true;
        m_source->rewind(path_id);
    }

    unsigned vertex(double *x, double *y)
    {
        unsigned cmd;

        /* The simplification algorithm doesn't support curves or compound paths
           so we just don't do it at all in that case... */
        if (!m_simplify) {
            return m_source->vertex(x, y);
        }

        /* メインの単純化ループ。
           ポイントは、出力キューに何かが追加されるまでに必要な数のポイントのみを消費することであり、
           一度にパス全体を実行することではありません。
           これにより、各描画時に追加のパス配列全体を割り当てて埋める必要がなくなります。 */
        while ((cmd = m_source->vertex(x, y)) != agg::path_cmd_stop) {
            /* 新しいパスセグメントを開始する場合、最初のポイントに移動して初期化 */

            if (m_moveto || cmd == agg::path_cmd_move_to) {
                /* m_movetoチェックは一般的には必要ありません。
                   m_sourceは初期のmovetoを生成するためですが、
                   これが真でない状況が発生する可能性があるため、
                   安全のために保持されています。 */
                if (m_origdNorm2 != 0.0 && !m_after_moveto) {
                    /* m_origdNorm2はベクトルがある場合のみ非ゼロです。
                       m_after_movetoチェックにより、
                       このベクトルをキューに一度だけプッシュすることを保証します。 */
                    _push(x, y);
                }
                m_after_moveto = true;

                if (std::isfinite(*x) && std::isfinite(*y)) {
                    m_has_init = true;
                    m_initX = *x;
                    m_initY = *y;
                } else {
                    m_has_init = false;
                }

                m_lastx = *x;
                m_lasty = *y;
                m_moveto = false;
                m_origdNorm2 = 0.0;
                m_dnorm2BackwardMax = 0.0;
                m_clipped = true;
                if (queue_nonempty()) {
                    /* プッシュを行った場合、キューを空にします。 */
                    break;
                }
                continue;
            }
            m_after_moveto = false;

            if(agg::is_close(cmd)) {
                if (m_has_init) {
                    /* If we have a valid initial vertex, then
                       replace the current vertex with the initial vertex */
                    *x = m_initX;
                    *y = m_initY;
                } else {
                    /* If we don't have a valid initial vertex, then
                       we can't close the path, so we skip the vertex */
                    continue;
                }
            }

            /* NOTE: We used to skip this very short segments, but if
               you have a lot of them cumulatively, you can miss
               maxima or minima in the data. */

            /* Don't render line segments less than one pixel long */
            /* if (fabs(*x - m_lastx) < 1.0 && fabs(*y - m_lasty) < 1.0) */
            /* { */
            /*     continue; */
            /* } */

            /* if we have no orig vector, set it to this vector and
               continue.  this orig vector is the reference vector we
               will build up the line to */
            if (m_origdNorm2 == 0.0) {
                if (m_clipped) {
                    queue_push(agg::path_cmd_move_to, m_lastx, m_lasty);
                    m_clipped = false;
                }

                m_origdx = *x - m_lastx;
                m_origdy = *y - m_lasty;
                m_origdNorm2 = m_origdx * m_origdx + m_origdy * m_origdy;

                // set all the variables to reflect this new orig vector
                m_dnorm2ForwardMax = m_origdNorm2;
                m_dnorm2BackwardMax = 0.0;
                m_lastForwardMax = true;
                m_lastBackwardMax = false;

                m_currVecStartX = m_lastx;
                m_currVecStartY = m_lasty;
                m_nextX = m_lastx = *x;
                m_nextY = m_lasty = *y;
                continue;
            }

            /* If got to here, then we have an orig vector and we just got
               a vector in the sequence. */

            /* Check that the perpendicular distance we have moved
               from the last written point compared to the line we are
               building is not too much. If o is the orig vector (we
               are building on), and v is the vector from the last
               written point to the current point, then the
               perpendicular vector is p = v - (o.v)o/(o.o)
               (here, a.b indicates the dot product of a and b). */

            /* get the v vector */
            double totdx = *x - m_currVecStartX;
            double totdy = *y - m_currVecStartY;

            /* get the dot product o.v */
            double totdot = m_origdx * totdx + m_origdy * totdy;

            /* get the para vector ( = (o.v)o/(o.o)) */
            double paradx = totdot * m_origdx / m_origdNorm2;
            double parady = totdot * m_origdy / m_origdNorm2;

            /* get the perp vector ( = v - para) */
            double perpdx = totdx - paradx;
            double perpdy = totdy - parady;

            /* get the squared norm of perp vector ( = p.p) */
            double perpdNorm2 = perpdx * perpdx + perpdy * perpdy;

            /* If the perpendicular vector is less than
               m_simplify_threshold pixels in size, then merge
               current x,y with the current vector */
            if (perpdNorm2 < m_simplify_threshold) {
                /* check if the current vector is parallel or
                   anti-parallel to the orig vector. In either case,
                   test if it is the longest of the vectors
                   we are merging in that direction. If it is, then
                   update the current vector in that direction. */
                double paradNorm2 = paradx * paradx + parady * parady;

                m_lastForwardMax = false;
                m_lastBackwardMax = false;
                if (totdot > 0.0) {
                    if (paradNorm2 > m_dnorm2ForwardMax) {
                        m_lastForwardMax = true;
                        m_dnorm2ForwardMax = paradNorm2;
                        m_nextX = *x;
                        m_nextY = *y;
                    }
                } else {
                    if (paradNorm2 > m_dnorm2BackwardMax) {
                        m_lastBackwardMax = true;
                        m_dnorm2BackwardMax = paradNorm2;
                        m_nextBackwardX = *x;
                        m_nextBackwardY = *y;
                    }
                }

                m_lastx = *x;
                m_lasty = *y;
                continue;
            }

            /* If we get here, then this vector was not similar enough to the
               line we are building, so we need to draw that line and start the
               next one. */

            /* If the line needs to extend in the opposite direction from the
               direction we are drawing in, move back to we start drawing from
               back there. */
            _push(x, y);

            break;
        }

        /* Fill the queue with the remaining vertices if we've finished the
           path in the above loop. */
        if (cmd == agg::path_cmd_stop) {
            if (m_origdNorm2 != 0.0) {
                queue_push((m_moveto || m_after_moveto) ? agg::path_cmd_move_to
                                                        : agg::path_cmd_line_to,
                           m_nextX,
                           m_nextY);
                if (m_dnorm2BackwardMax > 0.0) {
                    queue_push((m_moveto || m_after_moveto) ? agg::path_cmd_move_to
                                                            : agg::path_cmd_line_to,
                               m_nextBackwardX,
                               m_nextBackwardY);
                }
                m_moveto = false;
            }
            queue_push((m_moveto || m_after_moveto) ? agg::path_cmd_move_to : agg::path_cmd_line_to,
                       m_lastx,
                       m_lasty);
            m_moveto = false;
            queue_push(agg::path_cmd_stop, 0.0, 0.0);
        }

        /* Return the first item in the queue, if any, otherwise
           indicate that we're done. */
        if (queue_pop(&cmd, x, y)) {
            return cmd;
        } else {
            return agg::path_cmd_stop;
        }
    }

  private:
    VertexSource *m_source;
    bool m_simplify;
    double m_simplify_threshold;

    bool m_moveto;
    bool m_after_moveto;
    bool m_clipped;
    bool m_has_init;
    double m_initX, m_initY;
    double m_lastx, m_lasty;

    double m_origdx;
    double m_origdy;
    double m_origdNorm2;
    double m_dnorm2ForwardMax;
    double m_dnorm2BackwardMax;
    bool m_lastForwardMax;
    bool m_lastBackwardMax;
    double m_nextX;
    double m_nextY;
    double m_nextBackwardX;
    double m_nextBackwardY;
    double m_currVecStartX;
    double m_currVecStartY;

    inline void _push(double *x, double *y)
    {
        bool needToPushBack = (m_dnorm2BackwardMax > 0.0);

        /* If we observed any backward (anti-parallel) vectors, then
           we need to push both forward and backward vectors. */
        if (needToPushBack) {
            /* If the last vector seen was the maximum in the forward direction,
               then we need to push the forward after the backward. Otherwise,
               the last vector seen was the maximum in the backward direction,
               or somewhere in between, either way we are safe pushing forward
               before backward. */
            if (m_lastForwardMax) {
                queue_push(agg::path_cmd_line_to, m_nextBackwardX, m_nextBackwardY);
                queue_push(agg::path_cmd_line_to, m_nextX, m_nextY);
            } else {
                queue_push(agg::path_cmd_line_to, m_nextX, m_nextY);
                queue_push(agg::path_cmd_line_to, m_nextBackwardX, m_nextBackwardY);
            }
        } else {
            /* If we did not observe any backwards vectors, just push forward. */
            queue_push(agg::path_cmd_line_to, m_nextX, m_nextY);
        }

        /* If we clipped some segments between this line and the next line
           we are starting, we also need to move to the last point. */
        if (m_clipped) {
            queue_push(agg::path_cmd_move_to, m_lastx, m_lasty);
        } else if ((!m_lastForwardMax) && (!m_lastBackwardMax)) {
            /* If the last line was not the longest line, then move
               back to the end point of the last line in the
               sequence. Only do this if not clipped, since in that
               case lastx,lasty is not part of the line just drawn. */

            /* Would be move_to if not for the artifacts */
            queue_push(agg::path_cmd_line_to, m_lastx, m_lasty);
        }

        /* Now reset all the variables to get ready for the next line */
        m_origdx = *x - m_lastx;
        m_origdy = *y - m_lasty;
        m_origdNorm2 = m_origdx * m_origdx + m_origdy * m_origdy;

        m_dnorm2ForwardMax = m_origdNorm2;
        m_lastForwardMax = true;
        m_currVecStartX = m_queue[m_queue_write - 1].x;
        m_currVecStartY = m_queue[m_queue_write - 1].y;
        m_lastx = m_nextX = *x;
        m_lasty = m_nextY = *y;
        m_dnorm2BackwardMax = 0.0;
        m_lastBackwardMax = false;

        m_clipped = false;
    }
};

template <class VertexSource>
class Sketch
{
  public:
    /*
       scale: the scale of the wiggle perpendicular to the original
       line (in pixels)

       length: the base wavelength of the wiggle along the
       original line (in pixels)

       randomness: the factor that the sketch length will randomly
       shrink and expand.
    */
    Sketch(VertexSource &source, double scale, double length, double randomness)
        : m_source(&source),
          m_scale(scale),
          m_length(length),
          m_randomness(randomness),
          m_segmented(source),
          m_last_x(0.0),
          m_last_y(0.0),
          m_has_last(false),
          m_p(0.0),
          m_rand(0)
    {
        rewind(0);
        const double d_M_PI = 3.14159265358979323846;
        // Set derived values to zero if m_length or m_randomness are zero to
        // avoid divide-by-zero errors when a sketch is created but not used.
        if (m_length <= std::numeric_limits<double>::epsilon() || m_randomness <= std::numeric_limits<double>::epsilon()) {
            m_p_scale = 0.0;
        } else {
            m_p_scale = (2.0 * d_M_PI) / (m_length * m_randomness);
        }
        if (m_randomness <= std::numeric_limits<double>::epsilon()) {
            m_log_randomness = 0.0;
        } else {
            m_log_randomness = 2.0 * log(m_randomness);
        }
    }

    unsigned vertex(double *x, double *y)
    {
        if (m_scale == 0.0) {
            return m_source->vertex(x, y);
        }

        unsigned code = m_segmented.vertex(x, y);

        if (code == agg::path_cmd_move_to) {
            m_has_last = false;
            m_p = 0.0;
        }

        if (m_has_last) {
            // We want the "cursor" along the sine wave to move at a
            // random rate.
            double d_rand = m_rand.get_double();
            // Original computation
            // p += pow(k, 2*rand - 1)
            // r = sin(p * c)
            // x86 computes pow(a, b) as exp(b*log(a))
            // First, move -1 out, so
            // p' += pow(k, 2*rand)
            // r = sin(p * c') where c' = c / k
            // Next, use x86 logic (will not be worse on other platforms as
            // the log is only computed once and pow and exp are, at worst,
            // the same)
            // So p+= exp(2*rand*log(k))
            // lk = 2*log(k)
            // p += exp(rand*lk)
            m_p += exp(d_rand * m_log_randomness);
            double den = m_last_x - *x;
            double num = m_last_y - *y;
            double len = num * num + den * den;
            m_last_x = *x;
            m_last_y = *y;
            if (len != 0) {
                len = sqrt(len);
                double r = sin(m_p * m_p_scale) * m_scale;
                double roverlen = r / len;
                *x += roverlen * num;
                *y -= roverlen * den;
            }
        } else {
            m_last_x = *x;
            m_last_y = *y;
        }

        m_has_last = true;

        return code;
    }

    inline void rewind(unsigned path_id)
    {
        m_has_last = false;
        m_p = 0.0;
        if (m_scale != 0.0) {
            m_rand.seed(0);
            m_segmented.rewind(path_id);
        } else {
            m_source->rewind(path_id);
        }
    }

  private:
    VertexSource *m_source;
    double m_scale;
    double m_length;
    double m_randomness;
    agg::conv_segmentator<VertexSource> m_segmented;
    double m_last_x;
    double m_last_y;
    bool m_has_last;
    double m_p;
    RandomNumberGenerator m_rand;
    double m_p_scale;
    double m_log_randomness;
};

#endif // MPL_PATH_CONVERTERS_H
