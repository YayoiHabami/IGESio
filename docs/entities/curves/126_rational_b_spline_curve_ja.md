# Rational B-Spline Curve (Entity Type 126)

Defined at [curves/rational_b_spline_curve.h](./../../../include/igesio/entities/curves/rational_b_spline_curve.h)

## 目次

- [目次](#目次)
- [Parameters](#parameters)
  - [固有のパラメータ](#固有のパラメータ)
    - [`RationalBSplineCurveType`](#rationalbsplinecurvetype)
  - [曲線の定義](#曲線の定義)
    - [曲線 $C(t)$](#曲線-ct)
    - [導関数 $C'(t), C''(t)$](#導関数-ct-ct)
- [Appendix](#appendix)
  - [プログラム上での計算](#プログラム上での計算)
    - [`src/entities/curves/nurbs_basis_function.h`](#srcentitiescurvesnurbs_basis_functionh)
    - [`RationalBSplineCurve::TryGetDefinedPointAt(t)`](#rationalbsplinecurvetrygetdefinedpointatt)
    - [`RationalBSplineCurve::TryGetDerivatives(t, n)`](#rationalbsplinecurvetrygetderivativest-n)

## Parameters

### 固有のパラメータ

| メンバ関数 | 説明 |
|---|---|
| `GetCurveType()` <br> (`RationalBSplineCurveType`) | B-スプライン曲線の種類 |
| `Degree()` <br> (`int`) | B-スプライン曲線の次数 $m$ |
| `ControlPoints()` <br> (`Matrix3Xd`) | 制御点 $P_i = (x_i, y_i, z_i)\ (i = 0, 1, \ldots, k)$ |
| `Knots()` <br> (`std::vector<double>`) | ノットベクトル $t_{-m}, \ldots, t_{1+k}$ |
| `Weights()` <br> (`std::vector<double>`) | 重み $w_i\ (i = 0, 1, \ldots, k)$ |

#### `RationalBSplineCurveType`

　フォーム番号（`GetFormNumber()`）と一対一で対応する、Rational B-スプライン曲線の種類を表す列挙型です。特に指定がない場合、`kUndetermined`が返されます。

| form | 列挙子 | 説明 |
|---|---|---|
| 0 | `kUndetermined` | 曲線の形状はパラメータにより決定 |
| 1 | `kLine` | 直線 |
| 2 | `kCircularArc` | 円弧 or 円 |
| 3 | `kEllipticArc` | 楕円弧 or 楕円 |
| 4 | `kParabolicArc` | 放物線弧 or 放物線 |
| 5 | `kHyperbolicArc` | 双曲線弧 or 双曲線 |

### 曲線の定義

#### 曲線 $C(t)$

　Rational B-スプライン曲線 $C(t)$ は、以下の5種類のパラメータにより定義されます。

- 次数 $m$
- 制御点 $P_i = (x_i, y_i, z_i)\ (i = 0, 1, \ldots, k)$ ( $k + 1$ 個 )
- ノットベクトル $t_{-m}, \ldots, t_{1+k}$ ( $k + m + 2$ 個 )
- 重み $w_i\ (i = 0, 1, \ldots, k)$ ( $k + 1$ 個 )
- パラメータ範囲 $t \in [t_{start}, t_{end}]$

ここで、パラメータ範囲 $[t_{start}, t_{end}]$ は`GetParameterRange()`関数で取得でき、以下の条件を満たします。

$$T_0 \leq t_{start} < t_{end} \leq T_{1+k}$$

　また、上記のパラメータからB-スプライン基底関数 $b_{i,m}(t)$ が定義され、Rational B-スプライン曲線 $C(t)$ は次のように表されます。

$$C(t) = \frac{\sum_{i=0}^{k} b_{i,m}(t) w_i P_i}{\sum_{i=0}^{k} b_{i,m}(t) w_i}, \quad t \in [t_{start}, t_{end}]$$

ここで、B-スプライン基底関数 $b_{i,m}(t)$ は、次数 $m$ とノットベクトル $t_{-m}, \ldots, t_{1+k}$ に基づき、以下の漸化式により定義されます。

$$b_{i,0}(t) = \begin{cases}
  1 & t_i \leq t < t_{i+1} \\\
  0 & \text{otherwise}
\end{cases}$$

$$b_{i,l}(t) = \frac{t - t_i}{t_{i+l} - t_i} b_{i,l-1}(t) + \frac{t_{i+l+1} - t}{t_{i+l+1} - t_{i+1}} b_{i+1,l-1}(t), \quad l = 1, 2, \ldots, m$$

#### 導関数 $C'(t), C''(t)$

　Rational B-スプライン曲線 $C(t)$ の1階および2階の導関数 $C'(t), C''(t)$ は、以下のように定義されます。記号の定義および計算方法については、[Appendix-プログラム上での計算](#appendix)を参照してください。

$$C'(t) = \frac{1}{w(t)} \left( A'(t) - w'(t) C(t) \right)$$

$$C''(t) = \frac{1}{w(t)} \left( A''(t) - 2 w'(t) C'(t) - w''(t) C(t) \right)$$

　曲線 $C(t)$ および導関数 $C'(t), C''(t)$ から計算される各種特徴量については、[Geometric Properties (曲線の幾何学的性質)](./../geometric_properties_ja.md) を参照してください。

## Appendix

### プログラム上での計算

#### `src/entities/curves/nurbs_basis_function.h`

　Rational B-スプライン曲線 $C(t)$ のための各種計算は、（公開APIではありませんが）`src/entities/curves/nurbs_basis_function.h`内の`TryComputeBasisFunctions`関数において実装されています。この関数では、与えられたパラメータ値 $t$ に対して、B-スプライン基底関数 $b_{i,m}(t)$ およびその導関数を計算します。戻り値は以下の`BasisFunctionResult`構造体または`std::nullopt`です。

```cpp
struct BasisFunctionResult {
    // t ∈ [t_j, t_{j+1}) なるノット区間のインデックス j
    int knot_span;
    // 基底関数の値
    std::vector<double> values;
    // 基底関数の導関数の値
    std::vector<std::vector<double>> derivatives;
};
```

#### `RationalBSplineCurve::TryGetDefinedPointAt(t)`

　（定義空間における）Rational B-スプライン曲線 $C(t)$ 上の点を取得するには、`RationalBSplineCurve::TryGetDefinedPointAt(t)`関数を使用します。この関数は、引数としてパラメータ値 $t$ を受け取り、曲線上の点 $C(t)$ を計算して返します。計算には上記の`TryComputeBasisFunctions`関数が利用されます。

　上記の $t \in [t_{j}, t_{j+1})$ なる $j$ に対して、以下の局所的な基底関数 $b_{j-m+i, m}(t)$ が計算されます（`BasisFunctionResult::values`に対応）。

$$b_{j - m + i, m}(t), \quad i = 0, 1, \ldots, m$$

　この基底関数を用いて、Rational B-スプライン曲線 $C(t)$ 上の点は次のように計算されます。

$$C(t) = \frac{\sum_{i=0}^{m} b_{j - m + i, m}(t) w_{j - m + i} P_{j - m + i}}{\sum_{i=0}^{m} b_{j - m + i, m}(t) w_{j - m + i}} = \frac{\sum_{l=j-m}^{j} b_{l, m}(t) w_{l} P_{l}}{\sum_{l=j-m}^{j} b_{l, m}(t) w_{l}},\quad \text{if } \sum_{l=j-m}^{j} b_{l, m}(t) w_{l} \neq 0$$

　以降は、上の式を以下のように記述するものとします。ただし, $j$ は各 $t$ に対して $t \in [t_j, t_{j+1})$ を満たすインデックスです。

$$C(t) = \frac{A(t)}{w(t)}, \quad \text{where } \left\lbrace\begin{aligned}
    A(t) &= \sum_{l=j-m}^{j} b_{l, m}(t) w_{l} P_{l} \\\
    w(t) &= \sum_{l=j-m}^{j} b_{l, m}(t) w_{l}
\end{aligned}\right.$$

#### `RationalBSplineCurve::TryGetDerivatives(t, n)`

　（定義空間における）Rational B-スプライン曲線 $C(t)$ の $n$ 階までの導関数 $C'(t), C''(t), \ldots, C^{(n)}(t)$ を取得するには、`RationalBSplineCurve::TryGetDerivatives(t, n)`関数を使用します。この関数は、引数としてパラメータ値 $t$ と導関数の階数 $n$ を受け取り、曲線の導関数を計算して返します。計算には上記の`TryComputeBasisFunctions`関数が利用されます。

　まず、上式の分子 $A(t)$ および分母 $w(t)$ の $n$ 階の導関数を次のように定義します。

$$A^{(n)}(t) = \sum_{l=j-m}^{j} b_{l, m}^{(n)}(t) w_{l} P_{l}, \quad w^{(n)}(t) = \sum_{l=j-m}^{j} b_{l, m}^{(n)}(t) w_{l}$$

ここで, $b_{l, m}^{(n)}(t)$ は基底関数 $b_{l, m}(t)$ の $n$ 階の導関数を表し、`BasisFunctionResult::derivatives[n - 1][l - (j - m)]`に対応します。ただし, $n = 0$ の場合は $b_{l, m}^{(0)}(t) = b_{l, m}(t)$ であり、`BasisFunctionResult::values[l - (j - m)]`に対応します。

　このとき、Rational B-スプライン曲線 $C(t)$ の $n$ 階の導関数は、商の微分法則を適用して次のように計算されます。

$$C^{(n)}(t) = \frac{1}{w(t)} \left( A^{(n)}(t) - \sum_{k=0}^{n-1} \binom{n}{k} w^{(n-k)}(t) C^{(k)}(t) \right)$$

ここで、$\binom{n}{k}$ は二項係数を表し、`igesio/numerics/combinatorics.h`内の`BinomialCoefficient`関数で計算されます。

**計算例**

　0階から2階までの導関数を計算する場合、以下のようになります。

$$C^{(0)}(t) = C(t) = \frac{A(t)}{w(t)}$$

$$C^{(1)}(t) = \frac{1}{w(t)} \left( A^{(1)}(t) - w^{(1)}(t) C^{(0)}(t) \right)$$

$$C^{(2)}(t) = \frac{1}{w(t)} \left( A^{(2)}(t) - 2 w^{(1)}(t) C^{(1)}(t) - w^{(2)}(t) C^{(0)}(t) \right)$$
