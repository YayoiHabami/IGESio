# Parametric Spline Curve (Type 112)

Defined at [curves/parametric_spline_curve.h](./../../../include/igesio/entities/curves/parametric_spline_curve.h)

## 目次

- [目次](#目次)
- [Parameters](#parameters)
  - [固有のパラメータ](#固有のパラメータ)
    - [`ParametricSplineCurveType`](#parametricsplinecurvetype)
  - [曲線の定義](#曲線の定義)
    - [曲線 $C(t)$](#曲線-ct)
    - [導関数 $C'(t), C''(t)$](#導関数-ct-ct)
- [Bounding Box](#bounding-box)
  - [セグメントごとのAABB計算](#セグメントごとのaabb計算)
  - [曲線全体のAABB](#曲線全体のaabb)

## Parameters

### 固有のパラメータ

| メンバ関数 | 説明 |
|---|---|
| `GetCurveType` <br> `ParametricSplineCurveType` | 曲線の種類 |
| `Degree()` <br> `unsigned int` | 曲線の次数 $h\ (0 \leq h \leq 3)$ |
| `NumberOfSegments()` <br> `unsigned int` | セグメント数 $n$ |
| `Breakpoints()` <br> `std::vector<double>` | ブレークポイント $T(0), \ldots, T(n)$ |
| `Coefficients(i)` <br> `Matrix34d` | セグメント $i$ における係数行列 $A_i \in \mathbb{R}^{3\times 4}$ |
| `Coefficients()` <br> `Matrix3Xd` | 各セグメントにおける係数行列を横に結合した $3 \times 4n$ の行列 |

#### `ParametricSplineCurveType`

　`ParametricSplineCurveType`は、本エンティティの形状を示す列挙体です。Parameter Dataセクションの1番目のパラメータである`CTYPE`に対応します。以下の種類がありますが、`GetCurveType()`の戻り値が現在の形状を表現しているかは保証されません。

| `CTYPE` | 列挙体値 | 説明 |
|---|---|---|
| 1 | `kLinear` | 直線 |
| 2 | `kQuadratic` | 2次曲線 |
| 3 | `kCubic` | 3次曲線 |
| 4 | `kWilsonFowler` | Wilson-Fowler曲線 |
| 5 | `kModifiedWilsonFowler` | 修正Wilson-Fowler曲線 |
| 6 | `kBSpline` | Bスプライン曲線 |

### 曲線の定義

#### 曲線 $C(t)$

　Parametric Spline Curveは、最大3次の多項式セグメント $n$ 個から構成される曲線です。セグメント $i$ におけるパラメトリック曲線 $C_i(t)$ は、次のように定義されます。

$$C_i(t) = \begin{bmatrix} a_{x,i} & b_{x,i} & c_{x,i} & d_{x,i} \\\ a_{y,i} & b_{y,i} & c_{y,i} & d_{y,i} \\\ a_{z,i} & b_{z,i} & c_{z,i} & d_{z,i} \end{bmatrix} \begin{bmatrix} 1 \\\ s \\\ s^2 \\\ s^3 \end{bmatrix} = A_i \begin{bmatrix} 1 \\\ s \\\ s^2 \\\ s^3 \end{bmatrix}$$

$$(s = t - T(i), \quad T(i) \leq t < T(i+1))$$

ここで、係数行列 $A_i \in \mathbb{R}^{3\times 4}$ はセグメント $i$ における多項式係数を含み、以下の条件を満たします。

1. 縮退を避けるため, $b_{p,i}, c_{p,i}, d_{p,i} \ (p = x, y, z)$ の9つの係数のうち少なくとも1つは非ゼロ
2. 2次元的な（平面内に定義される）スプラインについては, $z = z_t$ 平面上において定義される (すなわち, $a_{z,i} = z_t, b_{z,i} = c_{z,i} = d_{z,i} = 0$ )
3. パラメータ $h$ は、曲線の滑らかさの指標として使用され、以下の条件を満たします。

| $h$ | 説明 |
|---|---|
| 0 | 曲線はすべてのブレークポイントで連続 |
| 1 | 曲線はすべてのブレークポイントで連続かつ1階導関数も連続 <br> かつ $c_{p,i} = d_{p,i} = 0\ (p = x, y, z), i = 0, \ldots, n-1$ |
| 2 | 曲線はすべてのブレークポイントで連続かつ1・2階導関数も連続 <br> かつ $d_{p,i} = 0\ (p = x, y, z), i = 0, \ldots, n-1$ |
| 3 | (IGES 5.3において、この値の場合の連続性条件の指定はなし) |

　全体のパラメトリック曲線 $C(t)$ は、各セグメント $C_i(t)$ を結合して次のように定義されます。

$$C(t) = \begin{cases}
  C_0(t), & T(0) \leq t < T(1) \\\
  C_1(t), & T(1) \leq t < T(2) \\\
  \vdots & \vdots \\\
  C_{n-1}(t), & T(n-1) \leq t \leq T(n)
\end{cases}$$

曲線 $C(t)$ の定義域は $[T(0), T(n)]$ です。

#### 導関数 $C'(t), C''(t)$

　各セグメント $C_i(t)$ の1階および2階の導関数は、次のように定義されます。

$$C_i'(t) = A_i \begin{bmatrix} 0 \\\ 1 \\\ 2s \\\ 3s^2 \end{bmatrix}, \quad C_i''(t) = A_i \begin{bmatrix} 0 \\\ 0 \\\ 2 \\\ 6s \end{bmatrix}$$

定義域、曲線全体での結合に関しては、曲線 $C(t)$ の定義に準じます。

　曲線 $C(t)$ および導関数 $C'(t), C''(t)$ から計算される各種特徴量については、[Geometric Properties (曲線の幾何学的性質)](./../geometric_properties_ja.md) を参照してください。

## Bounding Box

　Parametric Spline Curveの軸平行バウンディングボックス（AABB）は、曲線全体の $x, y, z$ 各座標の最小値と最大値によって定義されます。

$$P_{min} = (x_{min}, y_{min}, z_{min}), \quad P_{max} = (x_{max}, y_{max}, z_{max})$$

　曲線 $C(t)$ は $n$ 個のセグメント $C_i(t)$ で構成されているため、まず各セグメントのAABBを求め、次にすべてのセグメントのAABBを包含するAABBを計算します。

### セグメントごとのAABB計算

　各セグメント $C_i(t)$ の各座標成分は、局所パラメータ $s$ ($s \in [0, L_i]$、ただし $L_i = T(i+1) - T(i)$) に関する最大3次の多項式です。例えば $x$ 座標は $x_i(s) = a_{x,i} + b_{x,i} s + c_{x,i} s^2 + d_{x,i} s^3$ と表されます。

　区間 $[0, L_i]$ における最小値・最大値を求めるため、以下の候補点における値を調べます。

1. **区間の両端**: $s = 0$ および $s = L_i$
2. **区間内部の極値**: $s \in (0, L_i)$ において導関数 $x_i'(s) = b_{x,i} + 2c_{x,i} s + 3d_{x,i} s^2 = 0$ を満たす点

　導関数が0になる $s$ は、以下の場合分けで解析的に求められます。

- **$d_{x,i} \neq 0$ の場合**: 判別式 $D = c_{x,i}^2 - 3b_{x,i}d_{x,i}$ を計算し、$D \geq 0$ ならば $s = \frac{-c_{x,i} \pm \sqrt{D}}{3d_{x,i}}$ が解。区間 $(0, L_i)$ 内の解のみを候補点に追加
- **$d_{x,i} = 0, c_{x,i} \neq 0$ の場合**: $s = -\frac{b_{x,i}}{2c_{x,i}}$ が解。区間内ならば候補点に追加
- **$d_{x,i} = c_{x,i} = 0$ の場合**: 単調関数または定数関数のため、区間内部に極値なし

　候補点の集合 $S_{x,i}$ における $x_i(s)$ の値から $\min(x_i), \max(x_i)$ を求めます。$y, z$ 座標についても同様に計算し、セグメント $i$ のAABBを決定します。

### 曲線全体のAABB

　すべてのセグメント $i = 0, \ldots, n-1$ のAABBから、最小値・最大値を取ることで曲線全体のAABBが得られます。

$$x_{min} = \min_{i} (\min(x_i)), \quad x_{max} = \max_{i} (\max(x_i))$$

$y, z$ 座標についても同様に計算し、最終的なAABBは $P_{min} = (x_{min}, y_{min}, z_{min})$ と $P_{max} = (x_{max}, y_{max}, z_{max})$ によって定義されます。
