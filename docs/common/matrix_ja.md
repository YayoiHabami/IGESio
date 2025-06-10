# `Matrix`クラス

## 概要

　IGESioライブラリは、IGES（Initial Graphics Exchange Specification）ファイルの処理において行列やベクトルを扱うため、線形代数機能を提供しています。IGESioでは、高性能な線形代数ライブラリであるEigenの使用を推奨していますが、依存関係を最小化したい場合や特定の環境制約がある場合に備えて、独自の行列・ベクトルクラスも提供しています。

> 対象ファイル:
>
> - `igesio/common/matrix.h`

## 目次

- [概要](#概要)
- [目次](#目次)
- [設計思想](#設計思想)
  - [Eigen 互換性](#eigen-互換性)
    - [API互換性](#api互換性)
  - [制約事項](#制約事項)
    - [性能特性](#性能特性)
- [IGESio Matrixクラスの詳細仕様](#igesio-matrixクラスの詳細仕様)
  - [テンプレートパラメータ](#テンプレートパラメータ)
  - [型エイリアス](#型エイリアス)
- [メンバ関数](#メンバ関数)
  - [コンストラクタ](#コンストラクタ)
  - [演算機能](#演算機能)
    - [要素アクセスなど](#要素アクセスなど)
    - [四則演算など](#四則演算など)
    - [その他の計算](#その他の計算)
    - [出力機能](#出力機能)
- [使用例](#使用例)
  - [Matrixクラスを使用した処理の例](#matrixクラスを使用した処理の例)
  - [コンストラクタ・初期化](#コンストラクタ初期化)
    - [固定サイズ行列の作成](#固定サイズ行列の作成)
    - [動的サイズ行列の作成](#動的サイズ行列の作成)
  - [静的メンバ関数](#静的メンバ関数)
    - [ゼロ行列・単位行列の作成](#ゼロ行列単位行列の作成)
    - [定数行列の作成](#定数行列の作成)
  - [基本演算](#基本演算)
  - [行列・ベクトル積 / 行列・行列積](#行列ベクトル積--行列行列積)
- [なぜこのクラスが必要なのか](#なぜこのクラスが必要なのか)
  - [背景と目的](#背景と目的)
  - [IGES処理における行列の役割](#iges処理における行列の役割)
  - [ライブラリの切り替え方法](#ライブラリの切り替え方法)
    - [Eigenを使用する場合（推奨）](#eigenを使用する場合推奨)
    - [独自実装を使用する場合](#独自実装を使用する場合)

## 設計思想

### Eigen 互換性

　本ライブラリでは`IGESIO_ENABLE_EIGEN`マクロによって、以下の2つのモードを切り替えます：

- **Eigen モード**: Eigenライブラリの型エイリアスを使用
- **独自実装モード**: 独自の`Matrix`クラステンプレートを使用

詳しくは、[なぜこのクラスが必要なのか](#なぜこのクラスが必要なのか)を参照してください。

#### API互換性

　IGESioの独自Matrixクラスは、Eigenの基本的なAPIとの互換性を保つよう設計されています：

```cpp
// 以下のコードは、EigenでもIGESio独自実装でも同じように動作します
igesio::Vector3d v1{1.0, 2.0, 3.0};
igesio::Vector3d v2{4.0, 5.0, 6.0};

double dot_result = v1.dot(v2);                // 内積
igesio::Vector3d cross_result = v1.cross(v2);  // 外積（3Dベクトルのみ）
double norm_value = v1.norm();                 // ノルム

// 行列演算
igesio::Matrix3d mat = igesio::Matrix3d::Identity();
igesio::Vector3d result = mat * v1;  // 行列・ベクトル積
```

### 制約事項

　独自実装の`Matrix`クラスには以下の制約があります：

| 機能 | Eigen | IGESio独自実装 |
|------|-------|----------------|
| 行数 | 任意 | 2または3のみ |
| 列数 | 任意 | 1～3または動的 |
| 四則演算 | ✅ | ✅ |
| [その他簡単な線形代数演算](#その他の計算) | ✅ | ✅ |
| LU分解 | ✅ | ❌ |
| 固有値計算 | ✅ | ❌ |
| 高度な線形代数 | ✅ | ❌ |

#### 性能特性

- **Eigen**: 高度に最適化されており、SIMD命令やキャッシュ効率を考慮した実装
- **IGESio独自実装**: シンプルな実装で、遅延評価などを行わないため、性能はEigenに劣る

## IGESio Matrixクラスの詳細仕様

　本ライブラリでは、行数が2または3の行列を扱うための`igesio::detail::Matrix`クラスを定義しています。このクラスは、Eigenの行列クラスと同様のインターフェースを（いくつか）持ち、行列演算やベクトル演算をサポートします。**本クラスは**`Eigen::Matrix`**とのAPI互換性がないため、使用しないでください**。代わりに、以下の[型エイリアス](#型エイリアス)を使用してください。

- **行数**: 2 または 3 (テンプレートパラメータ `N` で指定)
- **列数**: 固定 (1～3) または動的 (テンプレートパラメータ `M` で指定)
- **内部データ**: column-major 形式で格納
- **サポートされる演算**
  - 要素アクセス
  - 行列/ベクトルの加算、減算
  - スカラーとの乗算、除算
  - 行列・行列積、行列・ベクトル積
  - サイズ変更 (動的列数の場合; 列数のみ`conservativeResize`で変更可能)
  - その他いくつかの基本的な線形代数演算

### テンプレートパラメータ

- `N`: 行数（2または3）
- `M`: 列数 (以下のいずれか)
  - `igesio::Dynamic` (-1): 動的な列数
  - 1～3: 固定の列数

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

## メンバ関数

### コンストラクタ

　本ライブラリの独自実装である`igesio::Matrix`クラスのコンストラクタは、以下の4種類が使用可能です。以下の表において、`M`は行列

| コンストラクタ | 説明 |
|---|---|
| `Matrix()` | デフォルトコンストラクタです。<br>固定列数の場合は指定された列数で初期化され、動的列数の場合は0列の行列が作成されます。要素の値は未定義です。 |
| `Matrix(rows, cols)` | 動的列数の行列の場合に使用可能です。<br>`rows`は行列の行数`N`（`Matrix2d`なら2）に一致する必要があります。 |
| `Matrix2d m = {{1, 2}, {3, 4}}` | 初期化子リストを使用した行列の初期化です。<br>動的サイズ行列の場合、列数は初期化子リストの要素数に基づいて決定されます。 |
| `Vector2d v = {1.0, 2.0}` | 初期化子リストを使用したベクトルの初期化です。<br>初期化子リストの要素数は、ベクトルの行数`N`に一致する必要があります。 |

　また、以下の静的メンバ関数により、ゼロ行列や単位行列を簡単に作成できます。

| メンバ関数 | 説明 |
|---|---|
| `Matrix Zero()` | ゼロ行列を作成します（固定サイズ版）。<br>全ての要素が0.0で初期化された行列を返します。 |
| `Matrix Zero(int rows, int cols)` | ゼロ行列を作成します（動的サイズ版）。<br>`rows`は行数（`N`と一致する必要）、`cols`は列数を指定します。 |
| `Matrix Identity()` | 単位行列を作成します（固定サイズ版）。<br>対角要素が1.0、その他が0.0の正方行列を返します。<br>**正方行列の場合のみ使用可能です**。 |
| `Matrix Identity(int rows, int cols)` | 単位行列を作成します（動的サイズ版）。<br>`rows`は行数（`N`と一致する必要）、`cols`は列数を指定します。<br>**正方行列の場合のみ使用可能です**。 |
| `Matrix Constant(double value)` | 定数行列を作成します（固定サイズ版）。<br>全ての要素が`value`で初期化された行列を返します。 |
| `Matrix Constant(int rows, int cols, double value)` | 定数行列を作成します（動的サイズ版）。<br>`rows`は行数（`N`と一致する必要）、`cols`は列数、`value`は初期値を指定します。 |

### 演算機能

　本節では、`igesio::Matrix`クラスでサポートされている演算機能を説明します。以下の表では、行列とベクトルの演算を区別して記載しています。

**表記について**
- 行列: $M$ (コード例では `m`)
- ベクトル: $\bm{v}$ (コード例では `v`)
- スカラー: $s$ (コード例では `s`)

**対応範囲**
- 明示的に「ベクトル」と記載されている演算：ベクトルのみで使用可能
- 「行列」と記載されている演算：行列とベクトルの両方で使用可能

#### 要素アクセスなど

| コード例 | 戻り値 | 説明 |
|:--:|:--:|---|
| `m.rows()` | `size_t` | 行数を返します。 |
| `m.cols()` | `size_t` | 列数を返します。動的列数の場合は、現在の列数を返します。 |
| `m.size()` | `size_t` | 行列の要素数（行数 × 列数）を返します。 |
| `m(i, j)` | `double&`<br>`const double&` | 行列の要素にアクセスします。 `i`は行インデックス、`j`は列インデックスです。 |
| `m(i)`<br>`m[i]` | `double&`<br>`const double&` | ベクトルの要素にアクセスします。 `i`はインデックスです。<br>**ベクトル (1列行列) の場合のみ使用可能です**。 |
| `m.col(j)` | `Vector2d`<br>or<br>`Vector3d` | `j`列目の要素を持つベクトルを返します。 |

　また、以下のメンバ関数により行列のサイズを変更できます。独自実装でサポートするのは固定行数の行列のみであるため、第一引数には常に`igesio::NoChange`を指定し、第二引数で列数を指定します。

- `m.conservativeResize(igesio::NoChange, new_cols)`

#### 四則演算など

　以下は、本ライブラリの独自実装である`igesio::Matrix`クラスでサポートされている四則演算の一覧です。加算・減算については、固定列数と動的列数を混在して使用することはできません。

| 演算 | 計算式 | 説明・コード例 |
|:--:|:--:|---|
| 加算 | $M_1 + M_2$ | $M_1$ と $M_2$ のサイズが同じ場合に使用可能<br>`m1 + m2`<br>`m1 += m2` |
| 減算 | $M_1 - M_2$ | $M_1$ と $M_2$ のサイズが同じ場合に使用可能<br>`m1 - m2`<br>`m1 -= m2` |
| スカラー乗算 | $s \cdot M$ <br> $M \cdot s$ | `s * m`<br>`m * s`<br>`m *= s` |
| スカラー除算 | $M / s$ | `m / s`<br>`m /= s` |
| 行列・ベクトル積 | $M \bm{v}$ | 行列の列数とベクトルの行数が一致する場合に使用可能<br>`m * v` |
| 行列・行列積 | $M_1 M_2$ | $M_1$ の列数と $M_2$ の行数が一致する場合に使用可能<br>[結果のサイズは $M_1$ の行数 × $M_2$ の列数](#行列ベクトル積--行列行列積)<br>`m1 * m2` |
| 内積 | $\bm{v}_1 \cdot \bm{v}_2$ | ベクトルのサイズが同じ場合に使用可能<br>`v1.dot(v2)` |
| 外積 | $\bm{v}_1 \times \bm{v}_2$ | 3次元ベクトルのみ使用可能<br>`v1.cross(v2)` |
| アダマール積 | $M_1 \circ M_2$ | $M_1$ と $M_2$ のサイズが同じ場合に使用可能<br>行列/ベクトルの要素ごとの積<br>`m1.cwiseProduct(m2)` |
| アダマール商 | $M_1 \oslash M_2$ | $M_1$ と $M_2$ のサイズが同じ場合に使用可能<br>行列/ベクトルの要素ごとの除算<br>`m1.cwiseQuotient(m2)` |

#### その他の計算

　以下の表は、`igesio::Matrix`クラスでサポートされているその他の計算機能を示しています。以下では、$m_{i,j}$ を行列 $M$ の $i$ 行 $j$ 列目の要素とします。

| 戻り値 | 計算式 | 説明・コード例 |
|:--:|:--:|---|
| `Matrix` | $1 / m_{i,j}$ | 要素ごとの逆数を計算します。<br>`m.cwiseInverse()` |
| `Matrix` | $\sqrt{m_{i,j}}$ | 要素ごとの平方根を計算します。<br>`m.cwiseSqrt()` |
| `Matrix` | $\|m_{i,j}\|$ | 要素ごとの絶対値を計算します。<br>`m.cwiseAbs()` |
| `double` | $\sum m_{i,j}^2$ | 行列の要素の二乗和を計算します。<br>`m.squaredNorm()` |
| `double` | $\sqrt{\sum m_{i,j}^2}$ | 行列の要素のノルム（ユークリッド距離）を計算します。<br>`m.norm()` |
| `double` | $\sum m_{i,j}$ | 行列の要素の総和を計算します。<br>`m.sum()` |
| `double` | $\prod m_{i,j}$ | 行列の要素の積を計算します。<br>`m.prod()` |

#### 出力機能

```cpp
// 行列の出力
igesio::Matrix2d mat = {{1.0, 2.0}, {3.0, 4.0}};
std::cout << mat << std::endl;
// 出力: ((1, 2), (3, 4))

// 動的サイズ行列の出力
igesio::Matrix2Xd dynMat(2, 3);
dynMat = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};
std::cout << dynMat << std::endl;
// 出力: ((1, 2, 3), (4, 5, 6))
```

## 使用例

### Matrixクラスを使用した処理の例

```cpp
#include <iostream>
#include "igesio/common/matrix.h"

int main() {
    using namespace igesio;

    // 2×2回転行列の作成
    double angle = M_PI / 4;  // 45度
    Matrix2d rotation{{std::cos(angle), -std::sin(angle)},
                      {std::sin(angle),  std::cos(angle)}};

    // 2次元点の作成
    Vector2d point{1.0, 0.0};

    // 回転変換の適用
    Vector2d rotated_point = rotation * point;
    std::cout << "回転後の点: " << rotated_point << std::endl;

    // 3次元ベクトルの外積計算
    Vector3d v1{1.0, 0.0, 0.0};
    Vector3d v2{0.0, 1.0, 0.0};
    Vector3d cross_product = v1.cross(v2);
    std::cout << "外積: " << cross_product << std::endl;

    // 動的サイズ行列の使用
    Matrix3Xd points(3, 4);  // 3次元の4つの点を格納
    points << 1, 2, 3, 4,
              0, 1, 2, 3,
              0, 0, 1, 2;

    // 各列（点）に変換を適用
    Matrix3d transform = Matrix3d::Identity() * 2.0;  // 2倍スケーリング
    Matrix3Xd transformed_points = transform * points;

    return 0;
}
```

### コンストラクタ・初期化

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

　動的サイズの行列の場合は、`conservativeResize`メンバ関数を使用してサイズを変更できます。第一引数では、常に`igesio::NoChange`を指定し、第二引数で列数を指定します。行数は固定（2または3）で変更できません。

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

### 静的メンバ関数

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

## なぜこのクラスが必要なのか

### 背景と目的

　IGESioライブラリでは、以下の理由から二重の実装を提供しています：

1. **依存関係の選択性**: Eigenは優秀なライブラリですが、依存関係を減らしたいプロジェクトも存在します
2. **組み込み環境での利用**: リソース制約のある環境では、軽量な実装が必要な場合があります
3. **学習・デバッグ目的**: 独自実装により、内部動作を理解しやすくなります
4. **ライセンス要件**: プロジェクトによっては、特定のライセンス条件を満たす必要があります

### IGES処理における行列の役割

　IGES形式では、3D幾何データの変換（移動、回転、スケーリング）や座標系変換に行列演算が頻繁に使用されます。IGESioでは主に以下の用途で行列を使用します：

- 2D/3D座標値の表現
- 3D点群の座標変換
- 回転行列による向きの変更
- スケーリング変換
- 投影変換

### ライブラリの切り替え方法

#### Eigenを使用する場合（推奨）

```cpp
// CMakeListsでEigenを有効にする場合
target_compile_definitions(your_target PRIVATE IGESIO_ENABLE_EIGEN)

// または、コンパイル時にマクロを定義
g++ -DIGESIO_ENABLE_EIGEN your_code.cpp
```

この場合、以下のEigenクラスが使用されます：

```cpp
#include "igesio/common/matrix.h"

// これらは実際にはEigenクラスのエイリアス
igesio::Matrix2d    // Eigen::Matrix2d
igesio::Vector3d    // Eigen::Vector3d
igesio::Matrix3Xd   // Eigen::Matrix3Xd
```

#### 独自実装を使用する場合

```cpp
// IGESIO_ENABLE_EIGENを定義しない場合、自動的に独自実装が使用されます
#include "igesio/common/matrix.h"

// 独自のMatrixクラスが使用される
igesio::Matrix2d    // igesio::detail::Matrix<2, 2>
igesio::Vector3d    // igesio::detail::Matrix<3, 1>
```
