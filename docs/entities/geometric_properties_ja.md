# エンティティの幾何学的特性

　本項では、本ライブラリで定義されるジオメトリ関連のエンティティ（曲線・曲面）に対して計算可能な、各種幾何学的特性について説明します。これらの特性は、主に微分幾何学の基本的な概念に基づいており、曲線や曲面の形状を解析・理解するために重要な役割を果たします。

　また、本項では、各種特性をプログラム上で計算・取得するための関数群についても説明します。これらの関数は、各エンティティクラス（`ICurve`派生クラスおよび`ISurface`派生クラス）にメンバ関数として実装されており、ユーザーはこれらの関数を呼び出すことで、簡単に幾何学的特性を取得できます。コードの実装例については、[examples/geometric_properties](./../../examples/geometric_properties.cpp)を参照してください。

## 目次

- [目次](#目次)
- [曲線の幾何学的特性](#曲線の幾何学的特性)
  - [曲線 $C(u)$](#曲線-cu)
  - [導関数 (曲線)](#導関数-曲線)
  - [接線ベクトル (曲線)](#接線ベクトル-曲線)
  - [曲率 (曲線)](#曲率-曲線)
  - [長さ (曲線)](#長さ-曲線)
  - [フレネ標構](#フレネ標構)
- [曲面の幾何学的特性](#曲面の幾何学的特性)
  - [曲面 $S(u,v)$](#曲面-suv)
  - [偏導関数 (曲面)](#偏導関数-曲面)
  - [接線・法線ベクトル (曲面)](#接線法線ベクトル-曲面)
  - [第一・第二基本形式](#第一第二基本形式)
  - [面積 (曲面)](#面積-曲面)
  - [曲率 (曲面)](#曲率-曲面)

## 曲線の幾何学的特性

　パラメトリック曲線

$$C(u) \in \mathbb{R}^3 \quad (u \in [u_{\text{min}}, u_{\text{max}}])$$

を考えます。本ライブラリで定義される曲線クラス（`ICurve`派生クラス）は、このような１つの媒介変数 $u$ によって定義される曲線を表現します。

　この関数の導関数・二階導関数は

- 導関数: $C'(u) := \frac{dC}{du}$
- 二階導関数: $C''(u) := \frac{d^2C}{du^2}$

で与えられます。これらを用いて、以下のような幾何学的特性が定義・計算されます。


| 名称 | 定義式 | 説明・メンバ関数 |
|:-:|:-:|:-|
| [**曲線**](#曲線-cu) <br> (Curve) | $C(u) \in \mathbb{R}^3$ | パラメトリック曲線 <br> `TryGetPointAt(u)` |
| [**導関数**](#導関数-曲線) <br> (Derivative) | $C'(u) = \frac{dC}{du}$ <br> $C''(u) = \frac{d^2C}{du^2}$ | 曲線の変化率を表すベクトル <br> `TryGetDerivatives(u, n_deriv)` |
| [**単位接線ベクトル**](#接線ベクトル-曲線) <br> (Tangent) | $T(u) = \frac{C'(u)}{\|C'(u)\|}$ | 曲線の接線方向 <br> `TryGetTangentAt(u)` |
| [**曲率**](#曲率-曲線) <br> (Curvature) | $\kappa(u) = \frac{\|C'(u) \times C''(u)\|}{\|C'(u)\|^3}$ | 曲線の曲がり具合を表す尺度 <br> `GetCurvature(u)` |
| [**長さ**](#長さ-曲線) <br> (Length) | $L = \int_{u_{\text{start}}}^{u_{\text{end}}} \|C'(t)\| dt$ | 曲線の2点間の距離 <br> `Length()` <br> `Length(start, end)` |
| [**単位法線ベクトル**](#フレネ標構) <br> (Normal) | $N(u) = \frac{\frac{dT(u)}{du}}{\|\frac{dT(u)}{du}\|}$ <br> ( $=\frac{C''(u)\|C'(u)\|^2 - C'(u)(C'(u)\cdot C''(u))}{\|C'(u)\|^3}$ )| 曲線の曲がる方向 <br> `TryGetNormalAt(u)` |
| [**従法線ベクトル**](#フレネ標構) <br> (Binormal) | $B(u) = T(u) \times N(u)$ | 曲線のねじれ方向 <br> `TryGetBinormalAt(u)` |

> 捩率 (Torsion) については、3階導関数が必要となるため、ここでは省略します。

> 曲線クラスのうち、[Composite Curve (type 102)](./entities_ja.md#compositecurve-type-102)や[Copious Data (type 106)](./entities_ja.md#copious-data-type-106) エンティティなどでは、滑らかではない点が存在する場合があります。そのような点においても、基本的には導関数および二階導関数を定義していますが、[Copious Data (type 106, forms 1-3)](./entities_ja.md#copiousdata-type-106-forms-1-3)のみ、完全に不連続な曲線（点群）として定義されているため、全領域にわたり導関数を定義していません。

### 曲線 $C(u)$

　IGES5.3で定義される曲線（`ICurve`派生クラス）は、一般にパラメトリック曲線として表現されます。これは、媒介変数 $u$ によって定義される三次元空間 $\mathbb{R}^3$ 内の点の集合です。変数 $u$ の取りうる範囲を $[u_{\text{min}}, u_{\text{max}}]$ とすると、曲線 $C(u)$ は以下のように表されます。

$$C(u) \in \mathbb{R}^3 \quad (u \in [u_{\text{min}}, u_{\text{max}}])$$

**コード例**

　以下は、`ICurve`派生クラスのインスタンス `curve` に対して、媒介変数 $u$ の範囲を取得し、その範囲内の特定の点における曲線の位置ベクトルを計算する例です。以降では、以下のコード例で記述した省略形（`i_ent`, `Vector3d`など）を用いて説明を行います。

　曲線上の点を取得する関数`TryGetPointAt`は、指定した媒介変数 $u$ における曲線の位置ベクトルを計算します。この関数は、計算が成功した場合に位置ベクトルを返し、失敗した場合には`std::nullopt`を返します。例えば、指定した $u$ が曲線の定義域外である場合などに失敗します。

```cpp
#include <iostream>
#include <memory>
#include <igesio/entities/curves/rational_b_spline_curve.h>

namespace i_ent = igesio::entities;
using igesio::Vector3d;

// NURBS曲線の定義例
auto param = igesio::IGESParameterVector{
    3,  // number of control points - 1
    3,  // degree
    false, false, false, false,  // non-periodic open NURBS curve
    0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0,  // knot vector
    1.0, 1.0, 1.0, 1.0,  // weights
    -4.0, -4.0,  0.0,    // control point P(0)
    -1.5,  7.0,  3.5,    // control point P(1)
     4.0, -3.0,  1.0,    // control point P(2)
     4.0,  4.0,  0.0,    // control point P(3)
    0.0, 1.0,            // parameter range V(0), V(1)
    0.0, 0.0, 1.0        // normal vector of the defining plane
};
auto curve = std::make_shared<i_ent::RationalBSplineCurve>(param);

// 媒介変数uの範囲を取得
auto [u_start, u_end] = curve->GetParameterRange();
std::cout << "Parameter range: [" << u_start << ", " << u_end << "]" << std::endl;

/// 指定した媒介変数uにおける曲線の位置ベクトルを計算
double u = (u_start + u_end) / 2.0;
auto point_opt = curve->TryGetPointAt(u);
if (point_opt) {
    Vector3d point = *point_opt;
    std::cout << "Point at u = " << u << ": " << point << std::endl;
} else {
    std::cout << "Failed to compute point at u = " << u << std::endl;
}
```

　正常に動作した場合、上記のコードは以下のような出力を生成します。ここで、各ベクトルは列ベクトルとして定義されているため、以下のように`((x), (y), (z))`の形式で表示されます。`point.transpose()`を出力することで、行ベクトルとして表示することも可能です。

```
Parameter range: [0, 1]
Point at u = 0.5: ((0.9375), (1.5), (1.6875))
```

### 導関数 (曲線)

　パラメトリック曲線 $C(u)$ に対して、$u$ における**導関数**は以下のように定義されます。

$$\begin{aligned}
    &C'(u) = \frac{dC}{du}, \quad \left(C'(u) \in \mathbb{R}^3\right) \\\
    &C''(u) = \frac{d^2C}{du^2}, \quad \left(C''(u) \in \mathbb{R}^3\right)
\end{aligned}$$

　これらの導関数は、曲線の形状を解析する上で重要な役割を果たします。例えば、$C'(u)$ は曲線上の接ベクトルを表し、これを正規化することで単位接線ベクトル $T(u)$ を得ることができます。また、二階導関数 $C''(u)$ は曲線の曲率を計算するために用いられます。

**コード例**

　以下は、`ICurve`派生クラスのインスタンス `curve` に対して、特定の媒介変数 $u$ における0～2階導関数を計算する例です。`TryGetDerivatives`関数は、指定した媒介変数 $u$ における導関数を`CurveDerivatives`型の変数または`std::nullopt`として返します。

```cpp
// 指定した媒介変数uにおける0～2階導関数を計算
unsigned int n_deriv = 2;
auto derivs_opt = curve->TryGetDerivatives(u, n_deriv);

if (derivs_opt) {
    const auto& derivs = *derivs_opt;
    for (unsigned int i = 0; i <= n_deriv; ++i) {
        std::cout << "Derivative C^" << i << "(u): " << derivs[i] << std::endl;
    }
} else {
    std::cout << "Failed to compute derivatives at u = " << u << std::endl;
}
```

出力例：

```
Derivative C^0(u): ((0.9375), (1.5), (1.6875))
Derivative C^1(u): ((10.125), (-1.5), (-1.875))
Derivative C^2(u): ((-7.5), (-12), (-13.5))
```

### 接線ベクトル (曲線)

　曲線 $C(u)$ における**接線ベクトル**は、導関数 $C'(u)$ によって与えられます。曲線上の接線を表すこのベクトルを正規化することで、次の**単位接線ベクトル** $T(u)$ を得ることができます。

$$T(u) = \frac{C'(u)}{\|C'(u)\|}$$

$$\left(T(u) \in \mathbb{R}^3, \; \|T(u)\| = 1\right)$$

**コード例**

```cpp
// 指定した媒介変数uにおける単位接線ベクトルを計算
auto tangent_opt = curve->TryGetTangentAt(u);

if (tangent_opt) {
    Vector3d tangent = *tangent_opt;
    std::cout << "Tangent T(u): " << tangent << std::endl;
} else {
    std::cout << "Failed to compute tangent at u = " << u << std::endl;
}
```

出力例：

```
Tangent T(u): ((0.973012), (-0.14415), (-0.180187))
```

### 曲率 (曲線)

　曲線における**曲率** $\kappa(u) \geq 0$ は、以下の式で定義される、その点における曲線の曲がり具合を表す尺度です。

$$\kappa(u) = \frac{\|C'(u) \times C''(u)\|}{\|C'(u)\|^3}$$

　この値は、以下のように解釈されます。曲率が大きいほど、曲線が急激に曲がっていることを意味します。

- $\kappa = 0$ : 曲線は直線である。
- $\kappa > 0$ : 曲線は曲がっている。

　また、この曲率の逆数である**曲率半径** $\rho(u) = \frac{1}{\kappa(u)}$ は、その点における曲線の曲がり具合を表すもう一つの尺度です。曲率半径が大きいほど、曲線が緩やかに曲がっていることを意味します。

**コード例**

　以下は、`GetCurvature`関数を用いて、特定の媒介変数 $u$ における曲率を計算する例です。この関数は曲率を直接返し、計算に失敗した場合には`-1`を返します。

```cpp
// 指定した媒介変数uにおける曲率を計算
double curvature = curve->GetCurvature(u);

if (curvature >= 0.0) {
    std::cout << "Curvature kappa(u): " << curvature << std::endl;
} else {
    std::cout << "Failed to compute curvature at u = " << u << std::endl;
}
```

出力例：

```
Curvature kappa(u): 0.178283
```

### 長さ (曲線)

　曲線の長さは、導関数を用いて計算できます。曲線 $C(u)$ のパラメータ区間 $[u_{\text{start}}, u_{\text{end}}]$ における長さ $L$ は、次のように表されます。

$$L = \int_{u_{\text{start}}}^{u_{\text{end}}} \|C'(t)\| dt$$

**コード例**

　以下は、`Length`関数を用いて、曲線全体の長さを計算する例です。この関数は曲線の長さを直接返します。基本的には0以上の値が返されますが、計算に失敗した場合や、曲線が離散的な場合（[Copious Data (type 106, forms 1-3)](./curves/106_copious_data_ja.md#copiousdatatype)）にはゼロが返されることがあります。

```cpp
// 曲線全体の長さを計算
double length = curve->Length();
std::cout << "Curve length: " << length << std::endl;

// 指定したパラメータ範囲 u_start ～ u_end における長さを計算
length = curve->Length(0.25, 0.75);
std::cout << "Curve length from u=0.25 to u=0.75: " << length << std::endl;
```

出力例：

```
Curve length: 14.0092
Curve length from u=0.25 to u=0.75: 5.14887
```

### フレネ標構

　曲線の局所的な幾何学的性質を記述するために、**フレネ標構**（Frenet Frame）が用いられます。これは、曲線上の各点において定義される直交座標系であり、単位接線ベクトル、**単位法線ベクトル**、**単位従法線ベクトル**から構成されます。後述する単位法線ベクトルの式からわかるように、この座標系は曲率 $\kappa(u)$ が非ゼロである点でのみ定義されます。

　**単位法線ベクトル** $N(u)$ は、接線ベクトルの変化率の方向を表します。これは以下のように定義されます。

$$N(u) = \frac{\frac{dT(u)}{du}}{\|\frac{dT(u)}{du}\|} = \frac{C''(u)\|C'(u)\|^2 - C'(u)(C'(u)\cdot C''(u))}{\|C'(u)\|^3}$$

$$\left(N(u) \in \mathbb{R}^3, \; \|N(u)\| = 1\right)$$

　**単位従法線ベクトル** $B(u)$ は、接線ベクトルと法線ベクトルの外積によって定義されます。これは以下のように表されます。

$$B(u) = T(u) \times N(u)$$

$$\left(B(u) \in \mathbb{R}^3, \; \|B(u)\| = 1\right)$$

**コード例**

```cpp
// 指定した媒介変数uにおける単位法線ベクトルおよび単位従法線ベクトルを計算
auto normal_opt = curve->TryGetNormalAt(u);
auto binormal_opt = curve->TryGetBinormalAt(u);

if (normal_opt && binormal_opt) {
    Vector3d normal = *normal_opt;
    Vector3d binormal = *binormal_opt;
    std::cout << "Normal N(u): " << normal << std::endl;
    std::cout << "Binormal B(u): " << binormal << std::endl;
} else {
    std::cout << "Failed to compute Frenet frame at u = " << u << std::endl;
}
```

出力例：

```
Normal N(u): ((-0.230481), (-0.645023), (-0.728577))
Binormal B(u): ((-0.0112007), (0.750444), (-0.660839))
```

## 曲面の幾何学的特性

　パラメトリック曲面

$$S(u,v) \in \mathbb{R}^3 \quad (u \in [u_{\text{min}}, u_{\text{max}}], v \in [v_{\text{min}}, v_{\text{max}}])$$

を考えます。本ライブラリで定義される曲面クラス（`ISurface`派生クラス）は、このような２つの媒介変数 $u, v$ によって定義される曲面を表現します。また、曲面クラスに関しては、曲面の境界（曲面の外周や、Trimmed Surface (type 144) の内側の穴など）上を除き、一般に滑らかな形状として定義されています。

　この関数の偏導関数・二階偏導関数は

- 偏導関数: $S_u(u,v) := \frac{\partial S}{\partial u}, \quad S_v(u,v) := \frac{\partial S}{\partial v}$
- 二階偏導関数: $S_{uu}(u,v) := \frac{\partial^2 S}{\partial u^2}, \quad S_{uv}(u,v) := \frac{\partial^2 S}{\partial u \partial v}, \quad S_{vv}(u,v) := \frac{\partial^2 S}{\partial v^2}$

で与えられます。これらを用いて、以下のような幾何学的特性が定義・計算されます。

| 名称 | 定義式 | 説明 |
|:-:|:-:|:-|
| [**曲面**](#曲面-suv) <br> (Surface) | $S(u,v) \in \mathbb{R}^3$ | パラメトリック曲面 <br> `TryGetPointAt(u, v)` |
| [**偏導関数**](#偏導関数-曲面) <br> (Partial Derivatives) | $S^{(n,m)}(u,v) = \frac{\partial^{n+m} S}{\partial u^n \partial v^m}$ <br> $(n,m = 0,1,2)$ | 曲面の変化率を表すベクトル <br> `TryGetPartialDerivatives(u, v, n_deriv_u, n_deriv_v)` |
| [**単位接線ベクトル**](#接線法線ベクトル-曲面) <br> (Tangent) | $T_u(u,v) = \frac{S_u(u,v)}{\|S_u(u,v)\|}$ <br> $T_v(u,v) = \frac{S_v(u,v)}{\|S_v(u,v)\|}$ | 曲面のu,v方向への接線方向 <br> `TryGetTangentAt(u, v)` |
| [**単位法線ベクトル**](#接線法線ベクトル-曲面) <br> (Normal) | $N(u,v) = T_u(u,v) \times T_v(u,v)$ | 曲面の法線方向 <br> `TryGetNormalAt(u, v)` |
| [**第一基本形式**](#第一第二基本形式) <br> (First Fundamental Form) | $E(u,v) = S_u(u,v) \cdot S_u(u,v)$ <br> $F(u,v) = S_u(u,v) \cdot S_v(u,v)$ <br> $G(u,v) = S_v(u,v) \cdot S_v(u,v)$ | 曲面の内在的な性質を表す |
| [**面積**](#面積-曲面) <br> (Area) | $A = \int_{v_{\text{min}}}^{v_{\text{max}}} \int_{u_{\text{min}}}^{u_{\text{max}}} \|S_u(u,v) \times S_v(u,v)\| du dv$ <br> $\left(\|S_u(u,v) \times S_v(u,v)\| = \sqrt{EG-F^2}\right)$ | 矩形領域の面積 |
| [**第二基本形式**](#第一第二基本形式) <br> (Second Fundamental Form) | $L(u,v) = S_{uu}(u,v) \cdot N(u,v)$ <br> $M(u,v) = S_{uv}(u,v) \cdot N(u,v)$ <br> $N(u,v) = S_{vv}(u,v) \cdot N(u,v)$ | 曲線の外在的な性質を表す |
| [**ガウス曲率**](#曲率-曲面) <br> (Gaussian Curvature) | $K(u,v) = \frac{LN - M^2}{EG - F^2}$ | 曲面の総合的な曲がり方 |
| [**平均曲率**](#曲率-曲面) <br> (Mean Curvature) | $H(u,v) = \frac{GL - 2FM + EN}{2(EG - F^2)}$ | 曲面の平均的な曲がり方 |
| [**主曲率**](#曲率-曲面) <br> (Principal Curvatures) | $\kappa_1(u,v), \kappa_2(u,v) $ <br> $= H \pm \sqrt{H^2 - K}$ | 曲面の最大・最小の曲率 |

### 曲面 $S(u,v)$

　IGES5.3で定義される曲面（`ISurface`派生クラス）は、一般にパラメトリック曲面として表現されます。これは、２つの媒介変数 $u, v$ によって定義される三次元空間 $\mathbb{R}^3$ 内の点の集合です。変数 $u$ の取りうる範囲を $[u_{\text{min}}, u_{\text{max}}]$、変数 $v$ の取りうる範囲を $[v_{\text{min}}, v_{\text{max}}]$ とすると、曲面 $S(u,v)$ は以下のように表されます。

$$S(u,v) \in \mathbb{R}^3 \quad (u \in [u_{\text{min}}, u_{\text{max}}], v \in [v_{\text{min}}, v_{\text{max}}])$$

**コード例**

　以下は、`ISurface`派生クラスのインスタンス `surface` に対して、媒介変数 $u, v$ の範囲を取得し、その範囲内の特定の点における曲面の位置ベクトルを計算する例です。以降では、以下のコード例で記述した省略形（`i_ent`, `Vector3d`など）を用いて説明を行います。パラメータ値の詳細については、[examples/geometric_properties.cpp](./../../examples/geometric_properties.cpp)を参照してください。

　曲面上の点を取得する関数`TryGetPointAt`は、指定した媒介変数 $u, v$ における曲面の位置ベクトルを計算します。この関数は、計算が成功した場合に位置ベクトルを返し、失敗した場合には`std::nullopt`を返します。例えば、指定した $u, v$ が曲面の定義域外である場合などに失敗します。

```cpp
#include <iostream>
#include <memory>
#include <igesio/entities/curves/rational_b_spline_surface.h>

namespace i_ent = igesio::entities;
using igesio::Vector3d;

// NURBS曲線の定義例
auto param = igesio::IGESParameterVector{
    5, 5,  // K1, K2 (Number of control points - 1 in U and V)
    3, 3,  // M1, M2 (Degree in U and V)
    false, false, true, false, false,         // PROP1-5
    0., 0., 0., 0., 1., 2., 3., 3., 3., 3.,   // Knot vector in U
    0., 0., 0., 0., 1., 2., 3., 3., 3., 3.,   // Knot vector in V
    1., 1., 1., 1., 1., 1.,                   // Weights W(0,0) to W(0,5)
    // ... (中略) ...
    1., 1., 1., 1., 1., 1.,                   // Weights W(5,0) to W(5,5)
    // Control points (36 points, each with x, y, z)
    -25., -25., -10.,  // Control point (0,0)
    // ... (中略) ...
    25., 25., -10.,    // Control point (5,5)
    0., 3., 0., 3.     // Parameter range in U and V
};
auto surface = std::make_shared<i_ent::RationalBSplineSurface>(param);

// 媒介変数u,vの範囲を取得
auto [u_start, u_end] = surface->GetURange();
auto [v_start, v_end] = surface->GetVRange();
std::cout << "Parameter range U: [" << u_start << ", " << u_end << "]" << std::endl;
std::cout << "Parameter range V: [" << v_start << ", " << v_end << "]" << std::endl;

/// 指定した媒介変数u, vにおける曲線の位置ベクトルを計算
double u = (u_start + u_end) / 2.0;
double v = (v_start + v_end) / 2.0;
auto point_opt = surface->TryGetPointAt(u, v);
if (point_opt) {
    Vector3d point = *point_opt;
    std::cout << "Point at (u, v) = (" << u << ", " << v << "): " << point << std::endl;
} else {
    std::cout << "Failed to compute point at (u, v) = (" << u << ", " << v << ")" << std::endl;
}
```

出力例：

```
Parameter range U: [0, 3]
Parameter range V: [0, 3]
Point at (u, v) = (1.5, 1.5): ((0), (0), (-7.42773))
```

### 偏導関数 (曲面)

　パラメトリック曲面 $S(u,v)$ に対して、$(u, v)$ における（**一階**）**偏導関数**は以下のように定義されます。

$$S_u(u,v) = \frac{\partial S}{\partial u}, \quad S_v(u,v) = \frac{\partial S}{\partial v}$$

$$\left(S_u(u,v), S_v(u,v) \in \mathbb{R}^3\right)$$

一階偏導関数に基づき、**二階偏導関数**は以下のように定義されます。ここで、IGESで定義される曲面はいずれも滑らかであるため、混合偏導関数は順序に依存しません（すなわち $S_{uv} = S_{vu}$ です）。

$$S_{uu}(u,v) = \frac{\partial^2 S}{\partial u^2}, \quad S_{uv}(u,v) = \frac{\partial^2 S}{\partial u \partial v}, \quad S_{vv}(u,v) = \frac{\partial^2 S}{\partial v^2}$$

$$\left(S_{uu}(u,v), S_{uv}(u,v), S_{vv}(u,v) \in \mathbb{R}^3\right)$$

　これらの偏導関数は、曲面の形状を解析する上で重要な役割を果たします。例えば、$S_u$ と $S_v$ は曲面上の接ベクトルを表し、$N(u,v) = S_u(u,v) \times S_v(u,v)$ は法線ベクトルを表します。これらのベクトルを用いることで、曲面の接平面や法線方向を計算することができます。また、二階偏導関数は曲面の曲率を計算するために用いられます。

**コード例**

　以下は、`ISurface`派生クラスのインスタンス `surface` に対して、特定の媒介変数 $u, v$ における0～2階偏導関数を計算する例です。`TryGetDerivatives`関数は、指定した媒介変数 $u, v$ における`n_deriv`階の偏導関数を`SurfaceDerivatives`型の変数または`std::nullopt`として返します。偏導関数 $S^{(n,m)}$ は、`derivs(n, m)` の形式でアクセスでき, $n + m \leq \text{n-deriv}$ を満たします。

```cpp
// 指定した媒介変数u, vにおける0～2階偏導関数を計算
// S, S_u, S_v, S_uu, S_uv, S_vv を取得
unsigned int n_deriv = 2;
auto derivs_opt = surface->TryGetDerivatives(u, v, n_deriv);

if (derivs_opt) {
    const auto& derivs = *derivs_opt;
    for (unsigned int i = 0; i <= n_deriv; ++i) {
        for (unsigned int j = 0; j <= n_deriv - i; ++j) {
            std::cout << "Derivative S^(" << i << "," << j << ")(u,v): "
                      << derivs(i, j) << std::endl;
        }
    }
} else {
    std::cout << "Failed to compute derivatives at (u, v) = ("
              << u << ", " << v << ")" << std::endl;
}
```

出力例：

```
Derivative S^(0,0)(u,v): ((0), (0), (-7.42773))
Derivative S^(0,1)(u,v): ((0), (11.25), (0))
Derivative S^(0,2)(u,v): ((1.11022e-16), (0), (7.73438))
Derivative S^(1,0)(u,v): ((11.25), (0), (0.0351562))
Derivative S^(1,1)(u,v): ((0), (0), (0))
Derivative S^(2,0)(u,v): ((0), (0), (5.48438))
```

### 接線・法線ベクトル (曲面)

　曲面 $S(u,v)$ における**接線ベクトル**は、偏導関数 $S_u$ と $S_v$ によって与えられます。これらのベクトルは、それぞれ曲面上のu方向およびv方向への接線を表します。この接線ベクトルを正規化することで、**単位接線ベクトル** $T_u$ および $T_v$ を得ることができます。

$$T_u(u,v) = \frac{S_u(u,v)}{\|S_u(u,v)\|}, \quad T_v(u,v) = \frac{S_v(u,v)}{\|S_v(u,v)\|}$$

$$\left(T_u(u,v), T_v(u,v) \in \mathbb{R}^3, \; \|T_u(u,v)\| = \|T_v(u,v)\| = 1\right)$$

　曲面の**法線ベクトル**は、接線ベクトルの外積によって求められます。これにより、曲面上の各点における法線方向を表す単位法線ベクトル $N(u,v)$ を得ることができます。本ライブラリでは、法線ベクトルを計算する際、u方向接線ベクトルを外積の左側、v方向接線ベクトルを右側において計算します。

$$N(u,v) = T_u(u,v) \times T_v(u,v)$$

$$\left(N(u,v) \in \mathbb{R}^3, \; \|N(u,v)\| = 1\right)$$

**コード例**

```cpp
// 指定した媒介変数u, vにおける単位接線ベクトルおよび単位法線ベクトルを計算
// 接線ベクトルは T_u, T_v の順で取得
auto tangent_opt = surface->TryGetTangentAt(u, v);
auto normal_opt = surface->TryGetNormalAt(u, v);

if (tangent_opt && normal_opt) {
    auto [tangent_u, tangent_v] = *tangent_opt;
    Vector3d normal = *normal_opt;
    std::cout << "Tangent T_u(u,v): " << tangent_u << std::endl;
    std::cout << "Tangent T_v(u,v): " << tangent_v << std::endl;
    std::cout << "Normal N(u,v): " << normal << std::endl;
} else {
    std::cout << "Failed to compute tangent/normal at (u, v) = ("
              << u << ", " << v << ")" << std::endl;
}
```

出力例：

```
Tangent T_u(u,v): ((0.999995), (0), (0.00312498))
Tangent T_v(u,v): ((0), (1), (0))
Normal N(u,v): ((-0.00312498), (0), (0.999995))
```

### 第一・第二基本形式

　**第一基本形式**は、曲面の内在的な性質（曲面上での長さ、角度、面積など）を表すものであり、以下のように一階偏導関数から計算されます。

$$E(u,v) = S_u(u,v) \cdot S_u(u,v), \quad F(u,v) = S_u(u,v) \cdot S_v(u,v), \quad G(u,v) = S_v(u,v) \cdot S_v(u,v)$$

　ここから、曲面の計量的な性質を計算することが可能です。例えば、曲面上での微小な移動 $dS = S_u du + S_v dv$ に対する長さは、以下のように表されます。

$$ds = \sqrt{E du^2 + 2F du dv + G dv^2}$$

　**第二基本形式**は、曲面が三次元空間 $\mathbb{R}^3$ の中でどのように曲がっているかという、在外的な性質を記述します。 これは、二階偏導関数と法線ベクトルを用いて以下のように定義されます。

$$L(u,v) = S_{uu}(u,v) \cdot N(u,v), \quad M(u,v) = S_{uv}(u,v) \cdot N(u,v), \quad N(u,v) = S_{vv}(u,v) \cdot N(u,v)$$

**コード例**

```cpp
// 指定した媒介変数u, vにおける第一基本形式および第二基本形式を計算
auto first_form_opt = surface->TryGetFirstFundamentalForm(u, v);
auto second_form_opt = surface->TryGetSecondFundamentalForm(u, v);

if (first_form_opt && second_form_opt) {
    auto [E, F, G] = *first_form_opt;
    auto [L, M, N] = *second_form_opt;
    std::cout << "First Fundamental Form (E, F, G): ("
              << E << ", " << F << ", " << G << ")" << std::endl;
    std::cout << "Second Fundamental Form (L, M, N): ("
              << L << ", " << M << ", " << N << ")" << std::endl;
} else {
    std::cout << "Failed to compute fundamental forms at (u, v) = ("
              << u << ", " << v << ")" << std::endl;
}
```

出力例：

```
First Fundamental Form (E, F, G): (126.564, 0, 126.562)
Second Fundamental Form (L, M, N): (5.48435, 0, 7.73434)
```

### 面積 (曲面)

　曲面の面積は、第一基本形式を用いて計算できます。曲面 $S(u, v)$ の面積 $A$ は、パラメータ領域 $D$ 上の二重積分として次のように表されます。

$$\begin{aligned}
    A &= \iint_D \sqrt{EG - F^2} \, du \, dv  \\\
    &= \int_{v_{\text{min}}}^{v_{\text{max}}} \int_{u_{\text{min}}}^{u_{\text{max}}} \sqrt{E(u,v)G(u,v) - F(u,v)^2} \, du \, dv
\end{aligned}$$

ここで、$\sqrt{EG - F^2}$ は面積要素を表し、曲面上の微小な領域の面積を意味します。この式は、曲面を微小な平行四辺形の集まりとみなし、それらの面積を足し合わせることで全体の面積を求めるという考えに基づいています。本ライブラリでは、`igesio/numerics/integration.h`の`Integrate`関数のような、数値積分を行う関数を用いて、この二重積分を近似的に計算します。

### 曲率 (曲面)

　曲面における曲率は、総合的な曲がり方を表すガウス曲率 $K$ と、平均的な曲がり方を表す平均曲率 $H$ によって特徴付けられます。また、これらの値を用いて、その点における主曲率 $\kappa_1$ および $\kappa_2$ を計算することができます。

　**ガウス曲率** $K(u, v) \in \mathbb{R}$ は、以下の式で定義される、その点における曲面の総合的な曲がり方を表す尺度です。

$$K(u,v) = \frac{LN - M^2}{EG - F^2}$$

　この値は、以下のように解釈されます。

- $K > 0$ : 曲面は（球のように）同方向に丸く曲がっています。
- $K < 0$ : 曲面は（鞍のように）異なる方向に曲がっています。
- $K = 0$ : 曲面は平坦であるか、一方向にのみ曲がっています。

　**平均曲率** $H(u, v) \in \mathbb{R}$ は、以下の式で定義される、その点における曲面の平均的な曲がり方を表す尺度です。この値は外的な量であり、曲面が空間内でどのように埋め込まれているかに依存します。

$$H = \frac{GL - 2FM + EN}{2(EG - F^2)}$$

特に $H = 0$ となる曲面を極小曲面（Minimal Surface）と呼び、これは与えられた境界に対して面積を最小化しようとする形状（シャボン玉の膜など）に対応します。

　曲面上の点 $S(u, v)$ において、法線ベクトル $N(u, v)$ を含む平面で曲面を切断したときの断面の曲率（**法曲率**）は、切断する方向によって変化します。**主曲率** $\kappa_1, \kappa_2$ は、この法曲率が取る最大値と最小値です。先述したガウス曲率および平均曲率との間には $K = \kappa_1 \kappa_2, H = \frac{\kappa_1 + \kappa_2}{2}$ の関係があるため、主曲率は以下のように計算されます。

$$\lambda^2 - 2H\lambda + K = 0$$

$$\therefore \kappa_1, \kappa_2 = H \pm \sqrt{H^2 - K}$$

この $\kappa_1$ および $\kappa_2$ は、曲面の局所的な形状を理解する上で重要な役割を果たします。$k_1$ と $k_2$ の符号に基づいて、以下のように曲面の形状を分類することができます<sup>[*1](http://homepages.inf.ed.ac.uk/rbf/BOOKS/FSTO/node28.html)</sup>。

| - | $\kappa_1 < 0$ | $\kappa_1 = 0$ | $\kappa_1 > 0$ |
|:-:|:--------------:|:--------------:|:--------------:|
| $\kappa_2 < 0$ | 凹楕円体 | 凹円筒 | 双曲面 |
| $\kappa_2 = 0$ | 凹円筒 | 平面 | 凸円筒 |
| $\kappa_2 > 0$ | 双曲面 | 凸円筒 | 凸楕円体 |

**コード例**

```cpp
// 指定した媒介変数u, vにおける曲率を計算
// ガウス曲率 K
auto gaussian_curvature_opt = surface->TryGetGaussianCurvature(u, v);
// 平均曲率 H
auto mean_curvature_opt = surface->TryGetMeanCurvature(u, v);
// 主曲率 (k1, k2)
auto principal_curvatures_opt = surface->TryGetPrincipalCurvatures(u, v);

if (gaussian_curvature_opt && mean_curvature_opt && principal_curvatures_opt) {
    double K = *gaussian_curvature_opt;
    double H = *mean_curvature_opt;
    auto [k1, k2] = *principal_curvatures_opt;
    std::cout << "Gaussian Curvature K(u,v): " << K << std::endl;
    std::cout << "Mean Curvature H(u,v): " << H << std::endl;
    std::cout << "Principal Curvatures (k1, k2): ("
              << k1 << ", " << k2 << ")" << std::endl;
} else {
    std::cout << "Failed to compute curvatures at (u, v) = ("
              << u << ", " << v << ")" << std::endl;
}
```

出力例：

```
Gaussian Curvature K(u,v): 0.0026481
Mean Curvature H(u,v): 0.0522218
Principal Curvatures (k1, k2): (0.0611108, 0.0433327)
```
