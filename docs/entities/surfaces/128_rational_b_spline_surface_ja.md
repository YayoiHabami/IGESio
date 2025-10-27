# Rational B-Spline Surface (Type 128)

Defined at [surfaces/rational_b_spline_surface.h](./../../../include/igesio/entities/surfaces/rational_b_spline_surface.h)

## 目次

- [目次](#目次)
- [Parameters](#parameters)
  - [固有のパラメータ](#固有のパラメータ)
    - [`RationalBSplineSurfaceType`](#rationalbsplinesurfacetype)
  - [曲面の定義](#曲面の定義)
    - [曲面 $S(u, v)$](#曲面-su-v)
    - [導関数 $S\_u, S\_v, S\_{uu}, S\_{uv}, S\_{vv}$](#導関数-s_u-s_v-s_uu-s_uv-s_vv)
- [Appendix](#appendix)
  - [プログラム上での計算](#プログラム上での計算)
    - [`src/entities/curves/nurbs_basis_function.h`](#srcentitiescurvesnurbs_basis_functionh)
    - [`RationalBSplineSurface::TryGetDefinedPointAt(u, v)`](#rationalbsplinesurfacetrygetdefinedpointatu-v)
    - [`RationalBSplineSurface::TryGetDerivatives(u, v, n)`](#rationalbsplinesurfacetrygetderivativesu-v-n)

## Parameters

### 固有のパラメータ

| メンバ関数 | 説明 |
|---|---|
| `GetSurfaceType()` <br> (`RationalBSplineSurfaceType`) | 有理B-スプライン曲面の種類 |
| `Degrees()` <br> (`std::pair<unsigned int, unsigned int>`) | $u,v$ 方向の次数 $m_u, m_v$ |
| `NumControlPoints()` <br> (`std::pair<unsigned int, unsigned int>`) | $u,v$ 方向の制御点数 $k_u + 1, k_v + 1$ |
| `UKnots()` <br> (`std::vector<double>`) | $u$ 方向のノットベクトル <br> $u_{-m_u}, \ldots, u_{1+k_u}$ |
| `VKnots()` <br> (`std::vector<double>`) | $v$ 方向のノットベクトル <br> $v_{-m_v}, \ldots, v_{1+k_v}$ |
| `ControlPoints()` <br> (`Matrix3Xd`) | 制御点 $P_{i,j} = (x_{i,j}, y_{i,j}, z_{i,j})\ (i = 0, \ldots, k_u; j = 0, \ldots, k_v)$ <br> $P_{i,j}$ は`i*(k_v + 1) + j`列目に格納される |
| `ControlPointAt(i, j)` <br> (`Vector3d`) | 制御点 $P_{i,j} = (x_{i,j}, y_{i,j}, z_{i,j})$ |
| `Weights()` <br> (`MatrixXd`) | 重み $w_{i,j}\ (i = 0, \ldots, k_u; j = 0, \ldots, k_v)$ <br> $w_{i,j}$ は`i + j*(k_u + 1)`要素に格納される |
| `WeightAt(i, j)` <br> (`double`) | 重み $w_{i,j}$ |

#### `RationalBSplineSurfaceType`

　フォーム番号（`GetFormNumber()`）と一対一で対応する、有理B-スプライン曲面の種類を表す列挙型です。特に指定がない場合、`kUndetermined`が返されます。

| form | 列挙子 | 説明 |
|---|---|---|
| 0 | `kUndetermined` | 曲面の形状はBスプラインのパラメータから決定される |
| 1 | `kPlane` | 平面 |
| 2 | `kRightCircularCylinder` | 直円柱 |
| 3 | `kCone` | 円錐 |
| 4 | `kSphere` | 球 |
| 5 | `kTorus` | トーラス |
| 6 | `kSurfaceOfRevolution` | 回転曲面 |
| 7 | `kTabulatedCylinder` | 柱状曲面 |
| 8 | `kRuledSurface` | ルールドサーフェス |
| 9 | `kGeneralQuadricSurface` | 一般的な二次曲面 |

### 曲面の定義

#### 曲面 $S(u, v)$

　有理B-スプライン曲面 $S(u, v)$ は、以下の7種類のパラメータにより定義されます。

- $u,v$ 方向の次数 $m_u, m_v$
- 制御点 $P_{i,j} = (x_{i,j}, y_{i,j}, z_{i,j})\ (i = 0, \ldots, k_u; j = 0, \ldots, k_v)$ ( $(k_u + 1) \times (k_v + 1)$ 個 )
- $u$ 方向のノットベクトル $u_{-m_u}, \ldots, u_{1+k_u}$ ( $k_u + m_u + 2$ 個 )
- $v$ 方向のノットベクトル $v_{-m_v}, \ldots, v_{1+k_v}$ ( $k_v + m_v + 2$ 個 )
- 重み $w_{i,j}\ (i = 0, \ldots, k_u; j = 0, \ldots, k_v)$ ( $(k_u + 1) \times (k_v + 1)$ 個 )
- パラメータ範囲 $u \in [u_{\text{start}}, u_{\text{end}}], \ v \in [v_{\text{start}}, v_{\text{end}}]$

ここで、パラメータ範囲 $[u_{\text{start}}, u_{\text{end}}]$ および $[v_{\text{start}}, v_{\text{end}}]$ は`GetURange()`および`GetVRange()`関数で取得でき、以下の条件を満たします。

$$\begin{aligned}
    &u_0 \leq u_{\text{start}} < u_{\text{end}} \leq u_{1+k_u} \\\
    &v_0 \leq v_{\text{start}} < v_{\text{end}} \leq v_{1+k_v}
\end{aligned}$$

　また、上記のパラメータからB-スプライン基底関数 $b_{i,m}(t)$ が定義され、有理B-スプライン曲面 $S(u, v)$ は次のように表されます。導出については、[Appendix-プログラム上での計算](#プログラム上での計算)を参照してください。

$$S(u, v) = \frac{A(u, v)}{w(u, v)}$$

$$\begin{aligned}
    A(u, v) &= \sum_{p=k - m_u}^{k} \sum_{q=l - m_v}^{l} b_{p, m_u}(u) b_{q, m_v}(v) w_{p, q} P_{p, q} \\\
    w(u, v) &= \sum_{p=k - m_u}^{k} \sum_{q=l - m_v}^{l} b_{p, m_u}(u) b_{q, m_v}(v) w_{p, q}
\end{aligned}$$

ここで, $k, l$ はそれぞれ $u \in [u_k, u_{k+1})$, $v \in [v_l, v_{l+1})$ を満たすインデックスです。

#### 導関数 $S_u, S_v, S_{uu}, S_{uv}, S_{vv}$

　まず、上式の分子 $A(u, v)$ および分母 $w(u, v)$ の $u, v$ に対する $(n_u, n_v)$ 階の導関数を次のように定義します。

$$\begin{aligned}
    A^{(n_u, n_v)}(u, v) &= \sum_{p=k - m_u}^{k} \sum_{q=l - m_v}^{l} b_{p, m_u}^{(n_u)}(u) b_{q, m_v}^{(n_v)}(v) w_{p, q} P_{p, q} \\\
    w^{(n_u, n_v)}(u, v) &= \sum_{p=k - m_u}^{k} \sum_{q=l - m_v}^{l} b_{p, m_u}^{(n_u)}(u) b_{q, m_v}^{(n_v)}(v) w_{p, q}
\end{aligned}$$

ここで, $b_{p, m_u}^{(n_u)}(u)$ および $b_{q, m_v}^{(n_v)}(v)$ はそれぞれ基底関数 $b_{p, m_u}(u)$ および $b_{q, m_v}(v)$ の $n_u$ 階および $n_v$ 階の導関数を表します。計算方法については、[Appendix-プログラム上での計算](#プログラム上での計算)を参照してください。

　このとき、有理B-スプライン曲面 $S(u, v)$ の各種導関数は次のように計算されます。導出については、[Appendix-プログラム上での計算](#プログラム上での計算)を参照してください。

**一階導関数**

$$\begin{aligned}
    S_u(u, v) &= \frac{1}{w(u, v)} \left( A^{(1, 0)}(u, v) - w^{(1, 0)}(u, v) S(u, v) \right) \\\
    S_v(u, v) &= \frac{1}{w(u, v)} \left( A^{(0, 1)}(u, v) - w^{(0, 1)}(u, v) S(u, v) \right)
\end{aligned}$$

**二階導関数**

$$\begin{aligned}
    S_{uu}(u, v) &= \frac{1}{w(u, v)} \left( A^{(2, 0)}(u, v) - 2 w^{(1, 0)}(u, v) S_u(u, v) - w^{(2, 0)}(u, v) S(u, v) \right) \\\
    S_{uv}(u, v) &= \frac{1}{w(u, v)} \left( A^{(1, 1)}(u, v) - w^{(1, 0)}(u, v) S_v(u, v) - w^{(0, 1)}(u, v) S_u(u, v) - w^{(1, 1)}(u, v) S(u, v) \right) \\\
    S_{vv}(u, v) &= \frac{1}{w(u, v)} \left( A^{(0, 2)}(u, v) - 2 w^{(0, 1)}(u, v) S_v(u, v) - w^{(0, 2)}(u, v) S(u, v) \right)
\end{aligned}$$

　曲面 $S(u,v)$ およびその偏導関数から計算可能な曲面の幾何学的特性については、[geometric_properties_ja.md](./../geometric_properties_ja.md#曲面の幾何学的特性) を参照してください。

## Appendix

### プログラム上での計算

#### `src/entities/curves/nurbs_basis_function.h`

　有理B-スプライン曲線 $C(t)$, 曲面 $S(u, v)$ のための各種計算は、（公開APIではありませんが）`src/entities/curves/nurbs_basis_function.h`内の`TryComputeBasisFunctions`関数において実装されています。この関数では、与えられたパラメータ値 $t$ に対して、B-スプライン基底関数 $b_{i,m}(t)$ およびその導関数を計算します。戻り値は以下の`BasisFunctionResult`構造体または`std::nullopt`です。

```cpp
struct BasisFunctionResult {
    // t ∈ [t_j, t_{j+1}) なるノット区間のインデックス j
    int knot_span;
    // 基底関数の値 b_{j-m,m}(t), ..., b_{j,m}(t)
    std::vector<double> values;
    // 基底関数の導関数の値 b_{j-m,m}^(i)(t), ..., b_{j,m}^(i)(t)
    // derivatives[i]は(i+1)次導関数の値
    std::vector<std::vector<double>> derivatives;
};
```

#### `RationalBSplineSurface::TryGetDefinedPointAt(u, v)`

　（定義空間における）有理B-スプライン曲面 $S(u, v)$ 上の点を取得するには、`RationalBSplineSurface::TryGetDefinedPointAt(u, v)`関数を使用します。この関数は、引数としてパラメータ値 $u, v$ を受け取り、曲面上の点 $S(u, v)$ を計算して返します。計算には上記の`TryComputeBasisFunctions`関数が利用されます。

　上記の $u \in [u_k, u_{k+1})$, $v \in [v_l, v_{l+1})$ なる $k, l$ に対して、以下の局所的な基底関数 $b_{k - m_u + i, m_u}(u)$ および $b_{l - m_v + j, m_v}(v)$ が計算されます（`BasisFunctionResult::values`に対応）。

$$\begin{aligned}
    &b_{k - m_u + i, m_u}(u), \quad i = 0, 1, \ldots, m_u \\\
    &b_{l - m_v + j, m_v}(v), \quad j = 0, 1, \ldots, m_v
\end{aligned}$$

　この基底関数を用いて、有理B-スプライン曲面 $S(u, v)$ 上の点は次のように計算されます。

$$\begin{aligned}
    S(u, v) &= \frac{\sum_{i=0}^{m_u} \sum_{j=0}^{m_v} b_{k - m_u + i, m_u}(u) b_{l - m_v + j, m_v}(v) w_{k - m_u + i, l - m_v + j} P_{k - m_u + i, l - m_v + j}}{\sum_{i=0}^{m_u} \sum_{j=0}^{m_v} b_{k - m_u + i, m_u}(u) b_{l - m_v + j, m_v}(v) w_{k - m_u + i, l - m_v + j}} \\\
    &= \frac{\sum_{p=k - m_u}^{k} \sum_{q=l - m_v}^{l} b_{p, m_u}(u) b_{q, m_v}(v) w_{p, q} P_{p, q}}{\sum_{p=k - m_u}^{k} \sum_{q=l - m_v}^{l} b_{p, m_u}(u) b_{q, m_v}(v) w_{p, q}} \\\
    &\quad \left(\text{if } \sum_{p=k - m_u}^{k} \sum_{q=l - m_v}^{l} b_{p, m_u}(u) b_{q, m_v}(v) w_{p, q} \neq 0\right)
\end{aligned}$$

　以降は、上の式を以下のように表記します。ただし、先述のとおり $k, l$ はそれぞれ $u \in [u_k, u_{k+1})$, $v \in [v_l, v_{l+1})$ を満たすインデックスです。

$$S(u, v) = \frac{A(u, v)}{w(u, v)}$$

$$\begin{aligned}
    A(u, v) &= \sum_{p=k - m_u}^{k} \sum_{q=l - m_v}^{l} b_{p, m_u}(u) b_{q, m_v}(v) w_{p, q} P_{p, q} \\\
    w(u, v) &= \sum_{p=k - m_u}^{k} \sum_{q=l - m_v}^{l} b_{p, m_u}(u) b_{q, m_v}(v) w_{p, q}
\end{aligned}$$

#### `RationalBSplineSurface::TryGetDerivatives(u, v, n)`

　（定義空間における）有理B-スプライン曲面 $S(u, v)$ の $n$ 階までの偏導関数を取得するには、`RationalBSplineSurface::TryGetDerivatives(u, v, n)`関数を使用します。この関数は、引数としてパラメータ値 $u, v$ と導関数の階数 $n$ を受け取り、曲面の偏導関数を計算して返します。計算には上記の`TryComputeBasisFunctions`関数が利用されます。例えば $n = 2$ の場合、以下の6つの偏導関数が計算されます。

$$S(u, v), \quad S_u(u, v), \quad S_v(u, v), \quad S_{uu}(u, v), \quad S_{uv}(u, v), \quad S_{vv}(u, v)$$

　まず、上式の分子 $A(u, v)$ および分母 $w(u, v)$ の各偏導関数を次のように定義します。

$$\begin{aligned}
    A^{(n_u, n_v)}(u, v) &= \frac{\partial^{n_u + n_v}}{\partial u^{n_u} \partial v^{n_v}} A(u, v) &&= \sum_{p=k - m_u}^{k} \sum_{q=l - m_v}^{l} b_{p, m_u}^{(n_u)}(u) b_{q, m_v}^{(n_v)}(v) w_{p, q} P_{p, q} \\\
    w^{(n_u, n_v)}(u, v) &= \frac{\partial^{n_u + n_v}}{\partial u^{n_u} \partial v^{n_v}} w(u, v) &&= \sum_{p=k - m_u}^{k} \sum_{q=l - m_v}^{l} b_{p, m_u}^{(n_u)}(u) b_{q, m_v}^{(n_v)}(v) w_{p, q}
\end{aligned}$$

　曲面 $S(u, v)$ の偏導関数 $S^{(n_u, n_v)}(u, v) = \frac{\partial^{n_u + n_v}}{\partial u^{n_u} \partial v^{n_v}} S(u, v)$ を計算するために、まず $S = A/w \ \Rightarrow \ S\cdot w = A$ の両辺を $u, v$ で $n_u, n_v$ 回偏微分します。まず、積の微分に関する一般化ライプニッツ則を適用することで、左辺 $S \cdot w$ の偏微分は

$$\frac{\partial^{n_u + n_v}}{\partial u^{n_u} \partial v^{n_v}} (S(u, v) \cdot w(u, v)) = \sum_{i=0}^{n_u} \sum_{j=0}^{n_v} \binom{n_u}{i} \binom{n_v}{j} S^{(i, j)}(u, v) \cdot w^{(n_u - i, n_v - j)}(u, v)$$

と表されます。一方、右辺 $A$ の偏微分はそのまま $A^{(n_u, n_v)}(u, v)$ です。したがって、以下の関係式が得られます。

$$\sum_{i=0}^{n_u} \sum_{j=0}^{n_v} \binom{n_u}{i} \binom{n_v}{j} S^{(i, j)}(u, v) \cdot w^{(n_u - i, n_v - j)}(u, v) = A^{(n_u, n_v)}(u, v)$$

　ここで、上の式から $S^{(n_u, n_v)}(u, v)$ の項を取り出すと

$$\binom{n_u}{n_u} \binom{n_v}{n_v} S^{(n_u, n_v)}(u, v) \cdot w^{(0, 0)}(u, v) = 1\cdot 1\cdot S^{(n_u, n_v)}(u, v) \cdot w^{(0, 0)}(u, v) = S^{(n_u, n_v)}(u, v) \cdot w(u, v)$$

となります。したがって、上の式を $S^{(n_u, n_v)}(u, v)$ について解くと、以下のように表されます。

$$\begin{aligned}
    &\quad S^{(n_u, n_v)}(u, v) \cdot w(u, v) + \underset{(i, j) \neq (n_u, n_v)}{\sum_{i = 0}^{n_u} \sum_{j = 0}^{n_v}} \binom{n_u}{i} \binom{n_v}{j} S^{(i, j)}(u, v) \cdot w^{(n_u - i, n_v - j)}(u, v) = A^{(n_u, n_v)}(u, v) \\\
    &\Rightarrow S^{(n_u, n_v)}(u, v) = \frac{1}{w(u, v)} \left( A^{(n_u, n_v)}(u, v) - \underset{(i, j) \neq (n_u, n_v)}{\sum_{i = 0}^{n_u} \sum_{j = 0}^{n_v}} \binom{n_u}{i} \binom{n_v}{j} S^{(i, j)}(u, v) \cdot w^{(n_u - i, n_v - j)}(u, v) \right)
\end{aligned}$$

**具体例：一階導関数**

$$\begin{aligned}
    S_u(u, v) &= \frac{1}{w(u, v)} \left( A^{(1, 0)}(u, v) - S(u, v) \cdot w^{(1, 0)}(u, v) \right) \\\
    &\quad \\\
    S_v(u, v) &= \frac{1}{w(u, v)} \left( A^{(0, 1)}(u, v) - S(u, v) \cdot w^{(0, 1)}(u, v) \right)
\end{aligned}$$

**具体例：二階導関数**

$$\begin{aligned}
    S_{uu}(u, v) &= \frac{1}{w(u, v)} \left( A^{(2, 0)}(u, v) - \underset{(i, j) \neq (2, 0)}{\sum_{i = 0}^{2} \sum_{j = 0}^{0}} \binom{2}{i} \binom{0}{j} S^{(i, j)}(u, v) \cdot w^{(2 - i, 0 - j)}(u, v) \right) \\\
    &= \frac{1}{w(u, v)} \left( A^{(2, 0)}(u, v) - 2 S_u(u, v) \cdot w^{(1, 0)}(u, v) - S(u, v) \cdot w^{(2, 0)}(u, v) \right) \\\
    &\quad \\\
    S_{uv}(u, v) &= \frac{1}{w(u, v)} \left( A^{(1, 1)}(u, v) - \underset{(i, j) \neq (1, 1)}{\sum_{i = 0}^{1} \sum_{j = 0}^{1}} \binom{1}{i} \binom{1}{j} S^{(i, j)}(u, v) \cdot w^{(1 - i, 1 - j)}(u, v) \right) \\\
    &= \frac{1}{w(u, v)} \left( A^{(1, 1)}(u, v) - S_u(u, v) \cdot w^{(0, 1)}(u, v) - S_v(u, v) \cdot w^{(1, 0)}(u, v) - S(u, v) \cdot w^{(1, 1)}(u, v) \right) \\\
    &\quad \\\
    S_{vv}(u, v) &= \frac{1}{w(u, v)} \left( A^{(0, 2)}(u, v) - \underset{(i, j) \neq (0, 2)}{\sum_{i = 0}^{0} \sum_{j = 0}^{2}} \binom{0}{i} \binom{2}{j} S^{(i, j)}(u, v) \cdot w^{(0 - i, 2 - j)}(u, v) \right) \\\
    &= \frac{1}{w(u, v)} \left( A^{(0, 2)}(u, v) - 2 S_v(u, v) \cdot w^{(0, 1)}(u, v) - S(u, v) \cdot w^{(0, 2)}(u, v) \right)
\end{aligned}$$
