#ifndef MPL_AGG_WORKAROUND_H
#define MPL_AGG_WORKAROUND_H

#include "agg_pixfmt_rgba.h"

/**********************************************************************
 * 回避策：このクラスは、AGG SVNのバグを回避するためのものです。
 * RGBA32ピクセルのブレンディングで十分な精度が保持されない問題を
 * 解決します。
*/

// RGBAプリマルチプライドブレンダー
template<class ColorT, class Order>
struct fixed_blender_rgba_pre : agg::conv_rgba_pre<ColorT, Order>
{
    typedef ColorT color_type;           // 色の型
    typedef Order order_type;            // 順序の型
    typedef typename color_type::value_type value_type;  // 値の型
    typedef typename color_type::calc_type calc_type;    // 計算用の型
    typedef typename color_type::long_type long_type;    // 長整数型
    enum base_scale_e
    {
        base_shift = color_type::base_shift,  // 基本シフト値
        base_mask  = color_type::base_mask    // 基本マスク値
    };

    //--------------------------------------------------------------------
    // カバレッジを考慮したピクセルブレンド
    static AGG_INLINE void blend_pix(value_type* p,
                                     value_type cr, value_type cg, value_type cb,
                                     value_type alpha, agg::cover_type cover)
    {
        blend_pix(p,
                  color_type::mult_cover(cr, cover),
                  color_type::mult_cover(cg, cover),
                  color_type::mult_cover(cb, cover),
                  color_type::mult_cover(alpha, cover));
    }

    //--------------------------------------------------------------------
    // ピクセルブレンドの基本実装
    static AGG_INLINE void blend_pix(value_type* p,
                                     value_type cr, value_type cg, value_type cb,
                                     value_type alpha)
    {
        alpha = base_mask - alpha;
        p[Order::R] = (value_type)(((p[Order::R] * alpha) >> base_shift) + cr);
        p[Order::G] = (value_type)(((p[Order::G] * alpha) >> base_shift) + cg);
        p[Order::B] = (value_type)(((p[Order::B] * alpha) >> base_shift) + cb);
        p[Order::A] = (value_type)(base_mask - ((alpha * (base_mask - p[Order::A])) >> base_shift));
    }
};

// RGBAプレーンブレンダー
template<class ColorT, class Order>
struct fixed_blender_rgba_plain : agg::conv_rgba_plain<ColorT, Order>
{
    typedef ColorT color_type;           // 色の型
    typedef Order order_type;            // 順序の型
    typedef typename color_type::value_type value_type;  // 値の型
    typedef typename color_type::calc_type calc_type;    // 計算用の型
    typedef typename color_type::long_type long_type;    // 長整数型
    enum base_scale_e { base_shift = color_type::base_shift };  // 基本シフト値

    //--------------------------------------------------------------------
    // カバレッジを考慮したピクセルブレンド
    static AGG_INLINE void blend_pix(value_type* p,
                                     value_type cr, value_type cg, value_type cb, value_type alpha, agg::cover_type cover)
    {
        blend_pix(p, cr, cg, cb, color_type::mult_cover(alpha, cover));
    }

    //--------------------------------------------------------------------
    // ピクセルブレンドの基本実装
    static AGG_INLINE void blend_pix(value_type* p,
                                     value_type cr, value_type cg, value_type cb, value_type alpha)
    {
        if(alpha == 0) return;  // アルファが0の場合は何もしない
        calc_type a = p[Order::A];  // 現在のアルファ値
        calc_type r = p[Order::R] * a;  // 現在の赤成分
        calc_type g = p[Order::G] * a;  // 現在の緑成分
        calc_type b = p[Order::B] * a;  // 現在の青成分
        a = ((alpha + a) << base_shift) - alpha * a;  // 新しいアルファ値の計算
        p[Order::A] = (value_type)(a >> base_shift);  // アルファ値の更新
        p[Order::R] = (value_type)((((cr << base_shift) - r) * alpha + (r << base_shift)) / a);  // 赤成分の更新
        p[Order::G] = (value_type)((((cg << base_shift) - g) * alpha + (g << base_shift)) / a);  // 緑成分の更新
        p[Order::B] = (value_type)((((cb << base_shift) - b) * alpha + (b << base_shift)) / a);  // 青成分の更新
    }
};

#endif
