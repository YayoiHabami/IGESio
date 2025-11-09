# Curve on a Parametric Surface (Type 142)

Defined at [curves/curve_on_a_parametric_surface.h](./../../../include/igesio/entities/curves/curve_on_a_parametric_surface.h)

## 目次

- [目次](#目次)
- [Parameters](#parameters)
  - [固有のパラメータ](#固有のパラメータ)
    - [`CurveCreationType`](#curvecreationtype)
    - [`PreferredRepresentation`](#preferredrepresentation)
  - [曲線の定義](#曲線の定義)
    - [曲線 $C(t)$](#曲線-ct)
    - [導関数 $C'(t), C''(t)$](#導関数-ct-ct)

## Parameters

### 固有のパラメータ

| メンバ関数 | 説明 |
|---|---|
| `GetSurface()` <br> `SetSurface(surface)` <br> (`std::shared_ptr<ICurve>`) | 曲面エンティティ $S(u,v)$ の取得・設定 |
| `GetBaseCurve()` <br> (`std::shared_ptr<ICurve>`) | 曲面上の曲線の基底曲線 $B(t)$ の取得 |
| `GetCurve()` <br> (`std::shared_ptr<ICurve>`) | 曲面上の曲線 $C(t)$ の取得 |
| `SetCurves(base, curve)` <br> (`std::shared_ptr<ICurve>`) | 曲面上の曲線の基底曲線 $B(t)$ と曲面上の曲線 $C(t)$ の設定 <br> `curve`を指定しない場合は内部で自動生成し、戻り値として返す |
| `GetCreationType()` <br> `SetCreationType(type)` <br> (`CurveCreationType`) | 曲面上の曲線の作成方法 <br> デフォルトは `kUnspecified` |
| `GetPreferredRepresentation()` <br> `SetPreferredRepresentation(pref)` <br> (`PreferredRepresentation`) | 送信システムにおける優先表現 <br> デフォルトは `kUnspecified` |

#### `CurveCreationType`

　曲面上の曲線がどのように作成されたかを示す列挙型です。Parameter Dataセクションの1つ目の整数値（`CRTN`）に対応します。

| 数値 | 列挙子 | 説明 |
|---|---|---|
| 0 | `kUnspecified` | 未指定 |
| 1 | `kProjection` | 与えられた曲線の曲面上への射影 |
| 2 | `kIntersection` | 2つの曲面の交線 |
| 3 | `kIsoparametric` | 曲面のアイソパラメトリック曲線（u-パラメトリック曲線またはv-パラメトリック曲線） |

#### `PreferredRepresentation`

　送信システムにおける優先表現を示す列挙型です。Parameter Dataセクションの5つ目の整数値（`PREF`）に対応します。

| 数値 | 列挙子 | 説明 |
|---|---|---|
| 0 | `kUnspecified` | 未指定 |
| 1 | `kSofB` | S(B(t)) が優先 |
| 2 | `kC` | C(t) が優先 |
| 3 | `kEquallyPreferred` | C(t) と S(B(t)) を同等に優先 |

### 曲線の定義

#### 曲線 $C(t)$

　パラメトリックサーフェス上の曲線エンティティは、パラメトリック曲面 $S(u,v)$ 上に定義されたパラメトリック曲線 $C(t)$ を表します。具体的な定義は、以下の通りです。

　曲面 $S(u,v)$ と、その定義域 $D$ が次のように与えられるとします。以下の定義の通り、定義域 $D$ は、パラメータ $u$ と $v$ のそれぞれの最小値と最大値によって定義される長方形領域です。

$$\begin{aligned}
    S(u, v) &= \begin{bmatrix} x(u,v) \\\ y(u,v) \\\ z(u,v) \end{bmatrix}, \\
    D &= \lbrace (u,v) \mid u_{\text{min}} \leq u \leq u_{\text{max}}, v_{\text{min}} \leq v \leq v_{\text{max}} \rbrace
\end{aligned}$$

　また, $D$ 上に定義されたパラメトリック曲線 $B(t)$ が次のように与えられるとします。ここで、曲線 $B$ は、サーフェス $S$ の領域である2次元空間 $D$ 上に位置するため、平面上に定義された曲線である必要があります。本ライブラリでは、`SetCurves(B)`で指定された曲線の $x,y$ 成分をそれぞれ $u,v$ 成分として扱います。

$$B(t) = \begin{bmatrix} u(t) \\\ v(t) \end{bmatrix}, \quad t \in [t_{\text{min}}, t_{\text{max}}]$$

　このとき、パラメトリックサーフェス上の曲線エンティティが表すパラメトリック曲線 $C(t)$ は、次のように定義されます。

$$\begin{aligned}
    C(t) &= S \circ B(t) \\
    &= S(u(t), v(t)) \\
    &= \begin{bmatrix} x(u(t), v(t)) \\\ y(u(t), v(t)) \\\ z(u(t), v(t)) \end{bmatrix}, \quad t \in [t_{\text{min}}, t_{\text{max}}]
\end{aligned}$$

#### 導関数 $C'(t), C''(t)$

　曲面 $S(u, v)$ の偏導関数、および曲線 $B(t)$ の導関数を以下のように定義します。

$$S_u := \frac{\partial S}{\partial u}, \quad s_v := \frac{\partial S}{\partial v}, \quad S_{uu} := \frac{\partial^2 S}{\partial u^2}, \quad S_{uv} := \frac{\partial^2 S}{\partial u \partial v}, \quad S_{vv} := \frac{\partial^2 S}{\partial v^2}$$

$$B'(t) := \frac{dB}{dt} = \begin{bmatrix} u'(t) \\\ v'(t) \end{bmatrix}, \quad B''(t) := \frac{d^2B}{dt^2} = \begin{bmatrix} u''(t) \\\ v''(t) \end{bmatrix}$$

　このとき、曲線 $C(t)$ の1階および2階の導関数は、次のように定義されます。

**1階導関数**

$$\begin{aligned}
    \frac{d}{dt} C(t) &= \frac{d}{dt} S(u(t), v(t)) \\
    &= \frac{\partial S}{\partial u} \frac{du}{dt} + \frac{\partial S}{\partial v} \frac{dv}{dt} \\
    &= S_u \cdot u' + S_v \cdot v'
\end{aligned}$$

**2階導関数**

$$\begin{aligned}
    \frac{d^2}{dt^2} C(t) &= \frac{d}{dt} \left( S_u \cdot u' + S_v \cdot v' \right) \\
    &= S_{uu} \cdot (u')^2 + 2 S_{uv} \cdot u' v' + S_{vv} \cdot (v')^2 + S_u \cdot u'' + S_v \cdot v''
\end{aligned}$$
