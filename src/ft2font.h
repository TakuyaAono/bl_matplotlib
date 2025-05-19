/* -*- mode: c++; c-basic-offset: 4 -*- */

/* FreeTypeを使用したPythonインターフェース */
#pragma once

#ifndef MPL_FT2FONT_H
#define MPL_FT2FONT_H

#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

extern "C" {
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H
#include FT_SFNT_NAMES_H
#include FT_TYPE1_TABLES_H
#include FT_TRUETYPE_TABLES_H
}

/*
 * FreeTypeフォント処理の主要な機能
 *
 * このファイルは、FreeTypeライブラリを使用したフォント処理の実装を定義しています。
 * FreeTypeは、高品質なフォントレンダリングのためのオープンソースライブラリです。
 *
 * 主な機能：
 * 1. フォントの読み込みと管理
 *    - システムフォントの検索と読み込み
 *    - フォントファイルのキャッシュ管理
 *    - フォントフェイスの制御
 *
 * 2. グリフ処理
 *    - グリフのアウトライン抽出
 *    - グリフのメトリクス計算
 *    - カーニング情報の取得
 *
 * 3. テキストレンダリング
 *    - グリフのラスタライズ
 *    - サブピクセルレンダリング
 *    - アンチエイリアシング
 *
 * 4. フォント情報
 *    - フォントメトリクスの取得
 *    - 文字マッピング
 *    - フォントスタイル情報
 *
 * 5. 高度な機能
 *    - フォントの変形と変換
 *    - カスタムフォントのサポート
 *    - フォントの合成
 *
 * このモジュールは、Matplotlibのテキスト描画機能の基盤となり、
 * 高品質なテキストレンダリングを実現します。
 */

/*
 * FT_FIXEDは、1つのlong型に2つの16ビット値を格納する定義です。
 */
#define FIXED_MAJOR(val) (signed short)((val & 0xffff0000) >> 16)
#define FIXED_MINOR(val) (unsigned short)(val & 0xffff)

// FreeTypeでレンダリングされた文字列を格納する幅と高さのバッファ
class FT2Image
{
  public:
    // コンストラクタ
    FT2Image();
    // 指定された幅と高さでバッファを初期化
    FT2Image(unsigned long width, unsigned long height);
    // デストラクタ
    virtual ~FT2Image();

    // バッファのサイズを変更
    void resize(long width, long height);
    // ビットマップを指定位置に描画
    void draw_bitmap(FT_Bitmap *bitmap, FT_Int x, FT_Int y);
    // 指定範囲を塗りつぶし
    void draw_rect_filled(unsigned long x0, unsigned long y0, unsigned long x1, unsigned long y1);

    // バッファへのポインタを取得
    unsigned char *get_buffer()
    {
        return m_buffer;
    }
    // バッファの幅を取得
    unsigned long get_width()
    {
        return m_width;
    }
    // バッファの高さを取得
    unsigned long get_height()
    {
        return m_height;
    }

  private:
    unsigned char *m_buffer;  // 画像バッファ
    unsigned long m_width;    // バッファの幅
    unsigned long m_height;   // バッファの高さ

    // コピーを防止
    FT2Image(const FT2Image &);
    FT2Image &operator=(const FT2Image &);
};

// FreeTypeライブラリのグローバルインスタンス
extern FT_Library _ft2Library;

// FreeTypeフォントを管理するクラス
class FT2Font
{
    // 警告関数の型定義
    typedef void (*WarnFunc)(FT_ULong charcode, std::set<FT_String*> family_names);

  public:
    // コンストラクタ：フォントを開き、初期化
    FT2Font(FT_Open_Args &open_args, long hinting_factor,
            std::vector<FT2Font *> &fallback_list, WarnFunc warn);
    // デストラクタ
    virtual ~FT2Font();
    // フォントのリソースを解放
    void clear();
    // フォントサイズとDPIを設定
    void set_size(double ptsize, double dpi);
    // 文字マップを設定
    void set_charmap(int i);
    // 文字マップを選択
    void select_charmap(unsigned long i);
    // テキストを設定し、位置情報を計算
    void set_text(std::u32string_view codepoints, double angle, FT_Int32 flags,
                  std::vector<double> &xys);
    // カーニング情報を取得（フォールバック対応）
    int get_kerning(FT_UInt left, FT_UInt right, FT_Kerning_Mode mode, bool fallback);
    // カーニング情報を取得（ベクトル形式）
    int get_kerning(FT_UInt left, FT_UInt right, FT_Kerning_Mode mode, FT_Vector &delta);
    // カーニング係数を設定
    void set_kerning_factor(int factor);
    // 文字を読み込み
    void load_char(long charcode, FT_Int32 flags, FT2Font *&ft_object, bool fallback);
    // フォールバックを使用して文字を読み込み
    bool load_char_with_fallback(FT2Font *&ft_object_with_glyph,
                                 FT_UInt &final_glyph_index,
                                 std::vector<FT_Glyph> &parent_glyphs,
                                 std::unordered_map<long, FT2Font *> &parent_char_to_font,
                                 std::unordered_map<FT_UInt, FT2Font *> &parent_glyph_to_font,
                                 long charcode,
                                 FT_Int32 flags,
                                 FT_Error &charcode_error,
                                 FT_Error &glyph_error,
                                 std::set<FT_String*> &glyph_seen_fonts,
                                 bool override);
    // グリフを読み込み（フォールバック対応）
    void load_glyph(FT_UInt glyph_index, FT_Int32 flags, FT2Font *&ft_object, bool fallback);
    // グリフを読み込み
    void load_glyph(FT_UInt glyph_index, FT_Int32 flags);
    // 幅と高さを取得
    void get_width_height(long *width, long *height);
    // ビットマップのオフセットを取得
    void get_bitmap_offset(long *x, long *y);
    // ディセント（下線の位置）を取得
    long get_descent();
    // グリフをビットマップに描画（アンチエイリアス対応）
    void draw_glyphs_to_bitmap(bool antialiased);
    // 個別のグリフをビットマップに描画
    void draw_glyph_to_bitmap(FT2Image &im, int x, int y, size_t glyphInd, bool antialiased);
    // グリフ名を取得
    void get_glyph_name(unsigned int glyph_number, std::string &buffer, bool fallback);
    // 名前からインデックスを取得
    long get_name_index(char *name);
    // 文字コードからグリフインデックスを取得
    FT_UInt get_char_index(FT_ULong charcode, bool fallback);
    // パス情報を取得
    void get_path(std::vector<double> &vertices, std::vector<unsigned char> &codes);
    // フォールバックフォントのインデックスを取得
    bool get_char_fallback_index(FT_ULong charcode, int& index) const;

    // フォントフェイスを取得
    FT_Face const &get_face() const
    {
        return face;
    }

    // 画像バッファを取得
    FT2Image &get_image()
    {
        return image;
    }
    // 最後のグリフを取得
    FT_Glyph const &get_last_glyph() const
    {
        return glyphs.back();
    }
    // 最後のグリフのインデックスを取得
    size_t get_last_glyph_index() const
    {
        return glyphs.size() - 1;
    }
    // グリフの総数を取得
    size_t get_num_glyphs() const
    {
        return glyphs.size();
    }
    // ヒンティング係数を取得
    long get_hinting_factor() const
    {
        return hinting_factor;
    }
    // カーニング情報の有無を確認
    FT_Bool has_kerning() const
    {
        return FT_HAS_KERNING(face);
    }

  private:
    WarnFunc ft_glyph_warn;  // 警告関数
    FT2Image image;          // 画像バッファ
    FT_Face face;           // フォントフェイス
    FT_Vector pen;          // 変換前の原点
    std::vector<FT_Glyph> glyphs;  // グリフの配列
    std::vector<FT2Font *> fallbacks;  // フォールバックフォントの配列
    std::unordered_map<FT_UInt, FT2Font *> glyph_to_font;  // グリフからフォントへのマッピング
    std::unordered_map<long, FT2Font *> char_to_font;      // 文字からフォントへのマッピング
    FT_BBox bbox;           // バウンディングボックス
    FT_Pos advance;         // 文字間隔
    long hinting_factor;    // ヒンティング係数
    int kerning_factor;     // カーニング係数

    // コピーを防止
    FT2Font(const FT2Font &);
    FT2Font &operator=(const FT2Font &);
};

#endif
