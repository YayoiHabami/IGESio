# `Matrix`クラス

## 概要

　`Matrix`クラスは、本ライブラリにおいて行列・ベクトルデータを扱うための基本的なクラスです。通常はEigenライブラリの行列クラスを使用することを推奨していますが（`IGESIO_ENABLE_EIGEN`マクロを定義することで有効化）、Eigenに依存しない環境でも動作するよう、独自の行列・ベクトルクラスも提供しています。

## 目次

- [概要](#概要)
- [目次](#目次)
- [設計思想](#設計思想)
  - [Eigen 互換性](#eigen-互換性)
  - [制約事項](#制約事項)
- [Matrixクラス](#matrixクラス-1)
  - [型エイリアス](#型エイリアス)
  - [基本的な使用方法](#基本的な使用方法)
    - [固定サイズ行列の作成](#固定サイズ行列の作成)
    - [動的サイズ行列の作成](#動的サイズ行列の作成)
  - [静的メソッド](#静的メソッド)
    - [ゼロ行列・単位行列の作成](#ゼロ行列単位行列の作成)
    - [定数行列の作成](#定数行列の作成)
- [演算機能](#演算機能)
  - [基本演算](#基本演算)
  - [行列・ベクトル積 / 行列・行列積](#行列ベクトル積--行列行列積)

## 設計思想

### Eigen 互換性

　本ライブラリでは`IGESIO_ENABLE_EIGEN`マクロによって、以下の2つのモードを切り替えます：

- **Eigen モード**: Eigenライブラリの型エイリアスを使用
- **独自実装モード**: 独自の`Matrix`クラステンプレートを使用

```cpp
#ifdef IGESIO_ENABLE_EIGEN
    using Eigen::Matrix2d;
    using Eigen::Vector3d;
#else
    using Matrix2d = Matrix<2, 2>;
    using Vector3d = Matrix<3, 1>;
#endif
```

### 制約事項

　独自実装の`Matrix`クラスには以下の制約があります：

- 行数は2または3のみサポート
- 列数は動的（`igesio::Dynamic` (= -1)）または1～3の固定値
- データ格納はcolumn-major形式
- 行数の変更は不可

## Matrixクラス

　本ライブラリでは、行数が2または3の行列を扱うための`igesio::detail::Matrix`クラスを定義しています。このクラスは、Eigenの行列クラスと同様のインターフェースを（いくつか）持ち、行列演算やベクトル演算をサポートします。**本クラスは**`Eigen::Matrix`**との互換性がないため、使用しないでください**。代わりに、以下の[型エイリアス](#型エイリアス)を使用してください。

**テンプレートパラメータ**

- `N`: 行数（2または3）
- `M`: 列数（`igesio::Dynamic`または 1～3）

```cpp
namespace igesio::detail {

template<int N, int M>
class Matrix

}  // namespace igesio::detail
```

### 型エイリアス

　よく使用される型のエイリアスを提供しています。基本的に`igesio::detail::Matrix`クラスは使用せず、**以下の型エイリアスを使用してください**。

```cpp
namespace igesio {

using Matrix2d  = detail::Matrix<2, 2>;       // 2x2行列
using Matrix23d = detail::Matrix<2, 3>;       // 2x3行列
using Matrix32d = detail::Matrix<3, 2>;       // 3x2行列
using Matrix3d  = detail::Matrix<3, 3>;       // 3x3行列
using Matrix2Xd = detail::Matrix<2, Dynamic>; // 2行×動的列数行列
using Matrix3Xd = detail::Matrix<3, Dynamic>; // 3行×動的列数行列
using Vector2d  = detail::Matrix<2, 1>;       // 2次元ベクトル
using Vector3d  = detail::Matrix<3, 1>;       // 3次元ベクトル

}  // namespace igesio
```

### 基本的な使用方法

#### 固定サイズ行列の作成

```cpp
// 2x2行列の作成
// [1 2]
// [3 4]
igesio::Matrix2d mat2x2;
mat2x2(0, 0) = 1.0; mat2x2(0, 1) = 2.0;
mat2x2(1, 0) = 3.0; mat2x2(1, 1) = 4.0;

// 初期化子リストによる初期化も可能
// [3 4]
// [5 6]
mat2x2 = {{3, 4}, {5, 6}};

// 3次元ベクトルの作成
// [1 2 3]^T
igesio::Vector3d vec3d;
vec3d(0) = 1.0;  // ベクトルは単一インデックスアクセス可能
vec3d(1) = 2.0;
vec3d(2) = 3.0;

// 初期化子リストによる初期化も可能
// [1 2 3]^T
vec3d = {1.0, 2.0, 3.0};
```

#### 動的サイズ行列の作成

　動的サイズの行列の場合は、`conservativeResize`メソッドを使用してサイズを変更できます。第一引数では、常に`igesio::NoChange`を指定し、第二引数で列数を指定します。行数は固定（2または3）で変更できません。

```cpp
// 3行×動的列数行列の作成
igesio::Matrix3Xd mat3xN(3, 5);  // 3行5列
mat3xN = {{ 1.0,  2.0,  3.0,  4.0,  5.0},
          { 6.0,  7.0,  8.0,  9.0,  10.0},
          {11.0, 12.0, 13.0, 14.0, 15.0}};

// 3行8列にリサイズ
// [ 1  2  3  4  5  0  0  0]
// [ 6  7  8  9 10  0  0  0]
// [11 12 13 14 15  0  0  0]
mat3xN.conservativeResize(igesio::NoChange, 8);

// 3行4列にリサイズ
// [ 1  2  3  4]
// [ 6  7  8  9]
// [11 12 13 14]
mat3xN.conservativeResize(igesio::NoChange, 4);
```

### 静的メソッド

#### ゼロ行列・単位行列の作成

```cpp
// 固定サイズのゼロ行列
// [0 0]
// [0 0]
auto zero_mat = igesio::Matrix2d::Zero();

// 動的サイズのゼロ行列
// [0 0 0 0]
// [0 0 0 0]
// [0 0 0 0]
auto zero_dyn = igesio::Matrix3Xd::Zero(3, 4);

// 単位行列
// [1 0 0]
// [0 1 0]
// [0 0 1]
auto identity = igesio::Matrix3d::Identity();
```

#### 定数行列の作成

```cpp
// 全要素が5.0の3x3行列
// [5 5 5]
// [5 5 5]
// [5 5 5]
auto const_mat = igesio::Matrix3d::Constant(5.0);

// 動的サイズの定数行列 (2x4行列として3.14で初期化)
// [3.14 3.14 3.14 3.14]
// [3.14 3.14 3.14 3.14]
auto const_dyn = igesio::Matrix2Xd::Constant(2, 4, 3.14);
```

## 演算機能

### 基本演算

```cpp
igesio::Matrix2d a = igesio::Matrix2d::Identity();
igesio::Matrix2d b = igesio::Matrix2d::Constant(2.0);

// 行列加算・減算
auto sum = a + b;
auto diff = a - b;
a += b;  // 代入演算も可能

// スカラー乗算・除算
auto scaled = a * 3.0;
auto divided = a / 2.0;
scaled *= 0.5;  // 代入演算も可能
```

### 行列・ベクトル積 / 行列・行列積

```cpp
igesio::Matrix3d transform = igesio::Matrix3d::Identity();
igesio::Vector3d point = {1.0, 2.0, 3.0};

// 行列・ベクトル積
// [1 0 0]   [1]   [1]
// [0 1 0] * [2] = [2]
// [0 0 1]   [3]   [3]
igesio::Vector3d transformed = transform * point;
```

　また、行列・行列積もサポートしています。以下の例では、2×3行列と3×2行列の積を計算し、結果として2×2行列を得ます。なお、行列積の結果サイズは、左側行列の行数×右側行列の列数となり、**右側行列の列数が動的サイズの場合は結果も動的サイズになります**。

$$\begin{bmatrix} 1 & 2 & 3 \\ 4 & 5 & 6 \end{bmatrix} \begin{bmatrix} 7 & 8 \\ 9 & 10 \\ 11 & 12 \end{bmatrix} = \begin{bmatrix} 58 & 64 \\ 139 & 154 \end{bmatrix}$$

```cpp
// 行列・行列積 (両方固定サイズ)
// => 結果は固定サイズの2x2行列
igesio::Matrix23d mat23 = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};
igesio::Matrix32d mat32 = {{7.0, 8.0}, {9.0, 10.0}, {11.0, 12.0}};
igesio::Matrix2d result1 = mat23 * mat32;

// 行列・行列積 (左側が動的サイズ)
// => 結果は固定サイズの2x2行列
igesio::Matrix2Xd mat2xN(2, 3);  // 2行3列の動的サイズ行列
mat2xN = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};
igesio::Matrix2d result2 = mat2xN * mat32;

// 行列・行列積 (右側が動的サイズ)
// => 結果は動的サイズの2x2行列
igesio::Matrix3Xd mat3xN(3, 2);  // 3行2列の動的サイズ行列
mat3xN = {{7.0, 8.0}, {9.0, 10.0}, {11.0, 12.0}};
igesio::Matrix2Xd result3 = mat23 * mat3xN;
```

<!-- TODO: エンティティの座標変換を実装したら、以下にその説明を追加する -->
