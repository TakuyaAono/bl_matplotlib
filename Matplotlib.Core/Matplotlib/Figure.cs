using System;
using System.Collections.Generic;
using System.Drawing;

namespace Matplotlib
{
    /// <summary>
    /// プロットの最上位コンテナ
    /// </summary>
    public class Figure : Artist
    {
        /// <summary>
        /// 図のサイズ（インチ）
        /// </summary>
        public SizeF Size { get; set; } = new SizeF(8, 6);

        /// <summary>
        /// 図のDPI
        /// </summary>
        public float DPI { get; set; } = 100;

        /// <summary>
        /// 図の背景色
        /// </summary>
        public Color BackgroundColor { get; set; } = Color.White;

        /// <summary>
        /// 図に含まれるAxesのリスト
        /// </summary>
        private readonly List<Axes> _axes = new();

        /// <summary>
        /// 図に含まれるAxesを取得
        /// </summary>
        public IReadOnlyList<Axes> Axes => _axes.AsReadOnly();

        /// <summary>
        /// 新しいAxesを追加
        /// </summary>
        /// <param name="rect">Axesの位置とサイズ（正規化座標）</param>
        /// <returns>作成されたAxes</returns>
        public Axes AddAxes(RectangleF rect)
        {
            var axes = new Axes(this, rect);
            _axes.Add(axes);
            return axes;
        }

        /// <summary>
        /// 図を描画
        /// </summary>
        /// <param name="renderer">レンダラー</param>
        public override void Draw(IRenderer renderer)
        {
            if (!Visible) return;

            // 背景を描画
            // TODO: 背景の描画処理

            // 各Axesを描画
            foreach (var axes in _axes)
            {
                axes.Draw(renderer);
            }
        }

        /// <summary>
        /// 図を保存
        /// </summary>
        /// <param name="filename">保存先のファイル名</param>
        /// <param name="format">保存形式</param>
        /// <param name="dpi">保存時のDPI</param>
        public void Save(string filename, string format = "png", float? dpi = null)
        {
            // TODO: ファイル保存の実装
        }

        /// <summary>
        /// 図をクリア
        /// </summary>
        public void Clear()
        {
            _axes.Clear();
        }
    }

    /// <summary>
    /// 座標軸を持つ描画領域
    /// </summary>
    public class Axes : Artist
    {
        /// <summary>
        /// 親のFigure
        /// </summary>
        public Figure Figure { get; }

        /// <summary>
        /// Axesの位置とサイズ（正規化座標）
        /// </summary>
        public RectangleF Position { get; set; }

        /// <summary>
        /// 新しいAxesを作成
        /// </summary>
        /// <param name="figure">親のFigure</param>
        /// <param name="position">位置とサイズ</param>
        public Axes(Figure figure, RectangleF position)
        {
            Figure = figure;
            Position = position;
        }

        /// <summary>
        /// Axesを描画
        /// </summary>
        /// <param name="renderer">レンダラー</param>
        public override void Draw(IRenderer renderer)
        {
            if (!Visible) return;

            // TODO: Axesの描画処理
        }
    }
} 