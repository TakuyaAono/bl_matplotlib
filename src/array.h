/* -*- mode: c++; c-basic-offset: 4 -*- */

/* numpy_cpp.hのNumPy配列ラッパーと同様の動作をする
 * スカラー値と空配列を作成するためのユーティリティ */

#ifndef MPL_SCALAR_H
#define MPL_SCALAR_H

#include <cstddef>
#include <stdexcept>

namespace array
{

// スカラー値を表すクラス
template <typename T, int ND>
class scalar
{
  public:
    T m_value;  // スカラー値

    // コンストラクタ：値を初期化
    scalar(const T value) : m_value(value)
    {
    }

    // 配列アクセス演算子：常に同じ値を返す
    T &operator()(int i, int j = 0, int k = 0)
    {
        return m_value;
    }

    // 定数配列アクセス演算子：常に同じ値を返す
    const T &operator()(int i, int j = 0, int k = 0) const
    {
        return m_value;
    }

    // 指定された次元のサイズを返す（常に1）
    int shape(size_t i)
    {
        return 1;
    }

    // 配列の総サイズを返す（常に1）
    size_t size()
    {
        return 1;
    }
};

// スカラー値の最初の次元のサイズを安全に取得
template <typename T, int ND>
size_t
safe_first_shape(scalar<T, ND>)
{
    return 1;
}

// 空配列を表すクラス
template <typename T>
class empty
{
  public:
    typedef empty<T> sub_t;  // サブ配列の型

    // コンストラクタ
    empty()
    {
    }

    // 配列アクセス演算子：アクセス時に例外をスロー
    T &operator()(int i, int j = 0, int k = 0)
    {
        throw std::runtime_error("Accessed empty array");
    }

    // 定数配列アクセス演算子：アクセス時に例外をスロー
    const T &operator()(int i, int j = 0, int k = 0) const
    {
        throw std::runtime_error("Accessed empty array");
    }

    // 配列インデックス演算子：新しい空配列を返す
    sub_t operator[](int i) const
    {
        return empty<T>();
    }

    // 指定された次元のサイズを返す（常に0）
    int shape(size_t i) const
    {
        return 0;
    }

    // 配列の総サイズを返す（常に0）
    size_t size() const
    {
        return 0;
    }
};

// 空配列の最初の次元のサイズを安全に取得
template <typename T>
size_t safe_first_shape(empty<T>)
{
    return 0;
}
}

#endif
