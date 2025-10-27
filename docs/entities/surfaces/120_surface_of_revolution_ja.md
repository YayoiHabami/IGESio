# Surface of Revolution (Type 120)

Defined at [surfaces/surface_of_revolution.h](./../../../include/igesio/entities/surfaces/surface_of_revolution.h)

## 目次

- [目次](#目次)
- [Parameters](#parameters)
  - [固有のパラメータ](#固有のパラメータ)
  - [曲面の定義](#曲面の定義)
    - [曲面 $S(u, v)$](#曲面-su-v)
    - [偏導関数 $S\_u, S\_v, S\_{uu}, S\_{uv}, S\_{vv}$](#偏導関数-s_u-s_v-s_uu-s_uv-s_vv)
- [Appendix](#appendix)
  - [曲面の関数の導出](#曲面の関数の導出)
  - [偏導関数の導出](#偏導関数の導出)

## Parameters

### 固有のパラメータ

| メンバ関数 | 説明 |
|---|---|
| `GetAxis()` <br> `SetAxis(line)` <br> (`std::shared_ptr<const Line>`) | 回転軸 $L$ へのポインタ |
| `GetGeneratrix()` <br> `SetGeneratrix(curve)` <br> (`std::shared_ptr<const ICurve>`) | 母線曲線 $C(t)$ へのポインタ |
| `GetAngleRange()` <br> `SetAngleRange(range)` <br> (`std::array<double, 2>`) | 回転角度範囲 $[\theta_s, \theta_e]$ [rad] |

### 曲面の定義

#### 曲面 $S(u, v)$

　回転曲面のパラメトリック曲面 $S(u, v)$ は、母線曲線 $C(t)$ を回転軸 $L$ の周りに回転させることで生成されます。回転軸はLine (Type 110) エンティティとして定義され、始点 $P_0 \ (\in \mathbb{R}^3)$ と方向ベクトル $D \ (\in \mathbb{R}^3,\ \lVert D \rVert = 1)$ によって表されます（詳細については[Appendix-曲面の関数の導出](#曲面の関数の導出)を参照）。

$$S(u, v) = P_0 + C_P(u) \cos{v} + \left( D \times C_P(u) \right) \sin{v} + D \left( D \cdot C_P(u) \right) (1 - \cos{v})$$

ここで, $C_P(u) := C(u) - P_0$ です。媒介変数 $u$ の範囲は母線曲線 $C(t)$ のパラメータ範囲 $[t_{\text{min}}, t_{\text{max}}]$ に対応し、媒介変数 $v$ の範囲は回転角度範囲 $[\theta_s, \theta_e]$ に対応します。

#### 偏導関数 $S_u, S_v, S_{uu}, S_{uv}, S_{vv}$

　回転曲面の偏導関数 $S_u, S_v, S_{uu}, S_{uv}, S_{vv}$ は以下のように定義されます。詳細な導出については、[Appendix-偏導関数の導出](#偏導関数の導出)を参照してください。

**一階偏導関数：**

$$\begin{aligned}
    S_u(u, v) &= C'(u) \cos{v} + \left( D \times C'(u) \right) \sin{v} + D \left( D \cdot C'(u) \right) (1 - \cos{v}) \\\
    S_v(u, v) &= -C_P(u) \sin{v} + \left( D \times C_P(u) \right) \cos{v} + D \left( D \cdot C_P(u) \right) \sin{v}
\end{aligned}$$

**二階偏導関数：**

$$\begin{aligned}
    S_{uu}(u, v) &= C''(u) \cos{v} + \left( D \times C''(u) \right) \sin{v} + D \left( D \cdot C''(u) \right) (1 - \cos{v}) \\\
    S_{uv}(u, v) &= -C'(u) \sin{v} + \left( D \times C'(u) \right) \cos{v} + D \left( D \cdot C'(u) \right) \sin{v} \\\
    S_{vv}(u, v) &= -C_P(u) \cos{v} - \left( D \times C_P(u) \right) \sin{v} + D \left( D \cdot C_P(u) \right) \cos{v}
\end{aligned}$$

　曲面 $S(u,v)$ およびその偏導関数から計算可能な曲面の幾何学的特性については、[geometric_properties_ja.md](./../geometric_properties_ja.md#曲面の幾何学的特性) を参照してください。

## Appendix

### 曲面の関数の導出

　回転曲面は、母線曲線 $C(t)$ を回転軸 $L$ の周りに回転させることで生成されます。回転軸 $L$ [Line (Type 110) エンティティ](./../curves/110_line_ja.md)として定義され、始点 $P_1$ と終点 $P_2$ によってあらわされます。したがって、回転軸の始点 $P_0 \ (\in \mathbb{R}^3)$ と方向ベクトル $D \ (\in \mathbb{R}^3,\ \lVert D \rVert = 1)$ は次のように定義されます。

$$\begin{aligned}
    P_0 &= P_1 \\\
    D &= \frac{P_2 - P_1}{\lVert P_2 - P_1 \rVert}
\end{aligned}$$

　したがって、母線曲線 $C(t)$ の各点 $C(x) \ (x \in [t_{\text{min}}, t_{\text{max}}])$ を、回転軸 $L$ の周りに角度 $\theta \ ( \theta \in [\theta_s, \theta_e])$ だけ回転させた点 $S(x, \theta)$ は、ロドリゲスの回転公式を用いて次のように表されます。

$$\begin{aligned}
    S(x, \theta) &= P_0 + C_P(x) \cos{\theta} + \left( D \times C_P(x) \right) \sin{\theta} + D \left( D \cdot C_P(x) \right) (1 - \cos{\theta})  \\\
    &= \left( P_0 + D \left( D \cdot C_P(x) \right) \right) + \left( C_P(x) - D \left( D \cdot C_P(x) \right) \right) \cos{\theta} + \left( D \times C_P(x) \right) \sin{\theta}
\end{aligned}$$

ここで, $C_P(x) := C(x) - P_0$ です。以降, $u = x, v = \theta$ として、パラメトリック曲面 $S(u, v)$ と表記します。

### 偏導関数の導出

　母線曲線 $C(t)$ の1階および2階の導関数をそれぞれ $C'(t), C''(t)$ と表記します。 $C_P(u) := C(u) - P_0$ について、導関数は

$$\begin{aligned}
    C_P'(u) &= \frac{d}{du} (C(u) - P_0) = C'(u) \\\
    C_P''(u) &= \frac{d^2}{du^2} (C(u) - P_0) = C''(u)
\end{aligned}$$

です。したがって、曲面 $S(u, v)$ の偏導関数は以下のように導出されます。

**一階偏導関数：**

$$\begin{aligned}
    S_u(u, v) &= C'(u) \cos{v} + \left( D \times C'(u) \right) \sin{v} + D \left( D \cdot C'(u) \right) (1 - \cos{v}) \\\
    S_v(u, v) &= -C_P(u) \sin{v} + \left( D \times C_P(u) \right) \cos{v} + D \left( D \cdot C_P(u) \right) \sin{v}
\end{aligned}$$

**二階偏導関数：**

$$\begin{aligned}
    S_{uu}(u, v) &= C''(u) \cos{v} + \left( D \times C''(u) \right) \sin{v} + D \left( D \cdot C''(u) \right) (1 - \cos{v}) \\\
    S_{uv}(u, v) &= -C'(u) \sin{v} + \left( D \times C'(u) \right) \cos{v} + D \left( D \cdot C'(u) \right) \sin{v} \\\
    S_{vv}(u, v) &= -C_P(u) \cos{v} - \left( D \times C_P(u) \right) \sin{v} + D \left( D \cdot C_P(u) \right) \cos{v}
\end{aligned}$$

**n階導関数：**

$$S^{(n_u, n_v)}(u, v) = \begin{cases}
    P_0 + C_P(u) \cos{v} + \left( D \times C_P(u) \right) \sin{v} + D \left( D \cdot C_P(u) \right) (1 - \cos{v}) & (n_u = 0, n_v = 0) \\\
    C^{(n_u)}(u) \cos{v} + \left( D \times C^{(n_u)}(u) \right) \sin{v} + D \left( D \cdot C^{(n_u)}(u) \right) (1 - \cos{v}) & (n_u \geq 1, n_v = 0) \\\
    \left(C_P(u) - D \left( D \cdot C_P(u) \right)\right)\cos(v + \frac{n_v \pi}{2}) + \left(D \times C_P(u)\right) \sin(v + \frac{n_v \pi}{2}) & (n_u = 0, n_v \geq 1) \\\
    \left(C^{(n_u)}(u) - D \left( D \cdot C^{(n_u)}(u) \right)\right)\cos(v + \frac{n_v \pi}{2}) + \left(D \times C^{(n_u)}(u)\right) \sin(v + \frac{n_v \pi}{2}) & (n_u \geq 1, n_v \geq 1)
\end{cases}$$

