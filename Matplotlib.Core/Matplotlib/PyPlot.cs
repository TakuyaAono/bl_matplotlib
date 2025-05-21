using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;

namespace Matplotlib
{
    /// <summary>
    /// Matplotlibのプロット機能を提供するクラス
    /// </summary>
    public static class PyPlot
    {
        private static Figure? _currentFigure;

        /// <summary>
        /// 現在のFigureを取得
        /// </summary>
        public static Figure CurrentFigure => _currentFigure ??= new Figure();

        /// <summary>
        /// 新しいFigureを作成
        /// </summary>
        /// <param name="figsize">図のサイズ（インチ）</param>
        /// <param name="dpi">DPI</param>
        /// <returns>作成されたFigure</returns>
        public static Figure Figure(SizeF? figsize = null, float? dpi = null)
        {
            _currentFigure = new Figure();
            if (figsize.HasValue)
                _currentFigure.Size = figsize.Value;
            if (dpi.HasValue)
                _currentFigure.DPI = dpi.Value;
            return _currentFigure;
        }

        /// <summary>
        /// 現在のFigureをクリア
        /// </summary>
        public static void Clf()
        {
            CurrentFigure.Clear();
        }

        /// <summary>
        /// 現在のFigureを閉じる
        /// </summary>
        public static void Close()
        {
            _currentFigure = null;
        }

        /// <summary>
        /// 現在のFigureを保存
        /// </summary>
        /// <param name="filename">保存先のファイル名</param>
        /// <param name="format">保存形式</param>
        /// <param name="dpi">保存時のDPI</param>
        public static void SaveFig(string filename, string format = "png", float? dpi = null)
        {
            CurrentFigure.Save(filename, format, dpi);
        }

        /// <summary>
        /// サブプロットを作成
        /// </summary>
        /// <param name="nrows">行数</param>
        /// <param name="ncols">列数</param>
        /// <param name="index">インデックス</param>
        /// <returns>作成されたAxes</returns>
        public static Axes Subplot(int nrows, int ncols, int index)
        {
            var figure = CurrentFigure;
            var rect = new RectangleF(
                (index % ncols) / (float)ncols,
                (index / ncols) / (float)nrows,
                1.0f / ncols,
                1.0f / nrows
            );
            return figure.AddAxes(rect);
        }

        /// <summary>
        /// 折れ線グラフを描画
        /// </summary>
        /// <param name="x">X座標データ</param>
        /// <param name="y">Y座標データ</param>
        /// <param name="format">線のスタイル指定文字列</param>
        /// <param name="label">凡例ラベル</param>
        /// <returns>作成されたLine2D</returns>
        public static Line2D Plot(IEnumerable<double> x, IEnumerable<double> y, string? format = null, string? label = null)
        {
            var axes = CurrentFigure.Axes.FirstOrDefault() ?? Subplot(1, 1, 1);
            var line = new Line2D(x, y);
            
            if (!string.IsNullOrEmpty(format))
            {
                // TODO: フォーマット文字列の解析
            }

            if (!string.IsNullOrEmpty(label))
            {
                line.Label = label;
            }

            // TODO: 線をAxesに追加

            return line;
        }

        /// <summary>
        /// 散布図を描画
        /// </summary>
        /// <param name="x">X座標データ</param>
        /// <param name="y">Y座標データ</param>
        /// <param name="s">マーカーのサイズ</param>
        /// <param name="c">マーカーの色</param>
        /// <param name="marker">マーカーのスタイル</param>
        /// <param name="label">凡例ラベル</param>
        /// <returns>作成されたLine2D</returns>
        public static Line2D Scatter(IEnumerable<double> x, IEnumerable<double> y, float s = 20, Color? c = null, MarkerStyle marker = MarkerStyle.Circle, string? label = null)
        {
            var line = Plot(x, y);
            line.Marker = marker;
            line.MarkerSize = s;
            if (c.HasValue)
                line.MarkerColor = c.Value;
            if (!string.IsNullOrEmpty(label))
                line.Label = label;
            return line;
        }

        /// <summary>
        /// タイトルを設定
        /// </summary>
        /// <param name="title">タイトル</param>
        public static void Title(string title)
        {
            // TODO: タイトルの設定
        }

        /// <summary>
        /// X軸のラベルを設定
        /// </summary>
        /// <param name="label">ラベル</param>
        public static void XLabel(string label)
        {
            // TODO: X軸ラベルの設定
        }

        /// <summary>
        /// Y軸のラベルを設定
        /// </summary>
        /// <param name="label">ラベル</param>
        public static void YLabel(string label)
        {
            // TODO: Y軸ラベルの設定
        }

        /// <summary>
        /// 凡例を表示
        /// </summary>
        public static void Legend()
        {
            // TODO: 凡例の表示
        }

        /// <summary>
        /// グリッドを表示
        /// </summary>
        /// <param name="visible">表示するかどうか</param>
        public static void Grid(bool visible = true)
        {
            // TODO: グリッドの表示
        }
    }
} 