using System;
using System.Collections.Generic;

namespace Matplotlib
{
    /// <summary>
    /// すべての描画オブジェクトの基底クラス
    /// </summary>
    public abstract class Artist
    {
        /// <summary>
        /// アーティストの一意のID
        /// </summary>
        public Guid Id { get; } = Guid.NewGuid();

        /// <summary>
        /// アーティストの表示/非表示状態
        /// </summary>
        public bool Visible { get; set; } = true;

        /// <summary>
        /// アーティストのZオーダー（重なり順）
        /// </summary>
        public int ZOrder { get; set; }

        /// <summary>
        /// アーティストのラベル
        /// </summary>
        public string? Label { get; set; }

        /// <summary>
        /// アーティストのプロパティ
        /// </summary>
        protected Dictionary<string, object> Properties { get; } = new();

        /// <summary>
        /// アーティストを描画
        /// </summary>
        /// <param name="renderer">レンダラー</param>
        public abstract void Draw(IRenderer renderer);

        /// <summary>
        /// アーティストのプロパティを設定
        /// </summary>
        /// <param name="name">プロパティ名</param>
        /// <param name="value">プロパティ値</param>
        public virtual void SetProperty(string name, object value)
        {
            Properties[name] = value;
        }

        /// <summary>
        /// アーティストのプロパティを取得
        /// </summary>
        /// <param name="name">プロパティ名</param>
        /// <returns>プロパティ値</returns>
        public virtual object? GetProperty(string name)
        {
            return Properties.TryGetValue(name, out var value) ? value : null;
        }
    }

    /// <summary>
    /// レンダラーインターフェース
    /// </summary>
    public interface IRenderer
    {
        // TODO: レンダラーの実装
    }
} 