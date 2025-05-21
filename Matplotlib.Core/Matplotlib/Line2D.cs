using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;

namespace Matplotlib
{
    /// <summary>
    /// 2次元の線を描画するクラス
    /// </summary>
    public class Line2D : Artist
    {
        /// <summary>
        /// 線のX座標データ
        /// </summary>
        public IReadOnlyList<double> XData { get; private set; }

        /// <summary>
        /// 線のY座標データ
        /// </summary>
        public IReadOnlyList<double> YData { get; private set; }

        /// <summary>
        /// 線の色
        /// </summary>
        public Color Color { get; set; } = Color.Blue;

        /// <summary>
        /// 線の太さ
        /// </summary>
        public float LineWidth { get; set; } = 1.0f;

        /// <summary>
        /// 線のスタイル
        /// </summary>
        public LineStyle LineStyle { get; set; } = LineStyle.Solid;

        /// <summary>
        /// マーカーのスタイル
        /// </summary>
        public MarkerStyle Marker { get; set; } = MarkerStyle.None;

        /// <summary>
        /// マーカーのサイズ
        /// </summary>
        public float MarkerSize { get; set; } = 6.0f;

        /// <summary>
        /// マーカーの色
        /// </summary>
        public Color MarkerColor { get; set; } = Color.Blue;

        /// <summary>
        /// 新しいLine2Dを作成
        /// </summary>
        /// <param name="xData">X座標データ</param>
        /// <param name="yData">Y座標データ</param>
        public Line2D(IEnumerable<double> xData, IEnumerable<double> yData)
        {
            XData = xData.ToList();
            YData = yData.ToList();
        }

        /// <summary>
        /// 線を描画
        /// </summary>
        /// <param name="renderer">レンダラー</param>
        public override void Draw(IRenderer renderer)
        {
            if (!Visible) return;

            // TODO: 線の描画処理
        }

        /// <summary>
        /// データを設定
        /// </summary>
        /// <param name="xData">X座標データ</param>
        /// <param name="yData">Y座標データ</param>
        public void SetData(IEnumerable<double> xData, IEnumerable<double> yData)
        {
            XData = xData.ToList();
            YData = yData.ToList();
        }
    }

    /// <summary>
    /// 線のスタイル
    /// </summary>
    public enum LineStyle
    {
        Solid,      // 実線
        Dashed,     // 破線
        Dotted,     // 点線
        DashDot,    // 一点鎖線
        None        // 線なし
    }

    /// <summary>
    /// マーカーのスタイル
    /// </summary>
    public enum MarkerStyle
    {
        None,       // マーカーなし
        Point,      // 点
        Circle,     // 円
        Square,     // 四角
        Diamond,    // ひし形
        Triangle,   // 三角形
        Plus,       // プラス
        Cross,      // バツ
        Star        // 星
    }
} 