# Conic Arc (Type 104)

Defined at [curves/conic_arc.h](./../../../include/igesio/entities/curves/conic_arc.h)

## 目次

- [目次](#目次)
- [Parameters](#parameters)
  - [固有のパラメータ](#固有のパラメータ)
  - [曲線の定義](#曲線の定義)
    - [曲線 $C(t)$](#曲線-ct)
    - [導関数 $C'(t), C''(t)$](#導関数-ct-ct)
- [Appendix](#appendix)
  - [双曲線のパラメトリック曲線 $C(t)$ の導出](#双曲線のパラメトリック曲線-ct-の導出)

## Parameters

### 固有のパラメータ

| メンバ関数 | 説明 |
|---|---|
| `ConicCoefficients()` <br> (`std::array<double, 6>`) | 2次方程式の係数 $(A, B, C, D, E, F)$ |
| `ConicStartPoint()` <br> (`Vector3d`) | 始点の座標 $P_s = (x_s, y_s, z_t)$ |
| `ConicTerminatePoint()` <br> (`Vector3d`) | 終点の座標 $P_e = (x_e, y_e, z_t)$ |
| `EllipseCenter()` <br> (`Vector3d`) | 楕円中心 $O_e = (x_e, y_e, z_t)$ (楕円の場合) |
| `EllipseRadii()` <br> (`std::array<double, 2>`) | 楕円のX軸・Y軸半径 $(r_x, r_y)$ (楕円の場合) |
| `EllipseStartAngle()` <br> (`double`) | 楕円の開始角度 $\theta_s$ [rad] (楕円の場合) |
| `EllipseEndAngle()` <br> (`double`) | 楕円の終了角度 $\theta_e$ [rad] (楕円の場合) |

> Conic Arc (Type 104) エンティティは、定義空間上の $z = z_t$ 平面内に位置します (`NDim() == 2`)。

### 曲線の定義

#### 曲線 $C(t)$

　円錐曲線のパラメトリック曲線 $C(t)$ は、次の $x, y$ に関する2次方程式を満たす点の集合として定義されます。

$$A x^2 + B xy + C y^2 + D x + E y + F = 0$$

ここで, $A, B, C, D, E, F$ は2次方程式の係数です。IGESでは、以下の3種類の円錐曲線のみがサポートされています。

- 楕円 (Ellipse): 標準形 $\frac{x^2}{{r_x}^2} + \frac{y^2}{{r_y}^2} = 1$ に一致する楕円
- 放物線 (Parabola): 標準形 $y^2 = 4px$ または $x^2 = 4py$ に一致する放物線
- 双曲線 (Hyperbola): 標準形 $\frac{x^2}{{a}^2} - \frac{y^2}{{b}^2} = 1$ または $\frac{y^2}{{b}^2} - \frac{x^2}{{a}^2} = 1$ に一致する双曲線

したがって、曲線の種類は以下のように分類されます。

$$AC - \frac{B^2}{4} \begin{cases}
    > 0 & \to\text{Ellipse} \\\
    = 0 & \to\text{Parabola} \\\
    < 0 & \to\text{Hyperbola}
\end{cases}$$

　また、曲線 $C(t)$ は始点 $P_s = (x_s, y_s, z_t)$ から終点 $P_e = (x_e, y_e, z_t)$ までの部分曲線として定義されます。

**楕円**

　楕円の場合、曲線 $C(t)$ は次のようにパラメトリックに表されます。

$$C(t) = \begin{bmatrix} r_x \cos{t} \\\ r_y \sin{t} \\\ z_t \end{bmatrix}, \quad t \in [\theta_s, \theta_e]$$

ここで, $r_x, r_y$ はそれぞれX軸・Y軸方向の半径, $\theta_s, \theta_e$ は開始角度と終了角度であり、次のように定義されます。

$$r_x = \sqrt{- F/A}, \quad r_y = \sqrt{- F/C}$$

$$\left\lbrace\begin{aligned}
  &\quad\quad\  \theta_s < \theta_e \\\
  0 &\leq \quad \theta_s &< 2\pi \\\
  0 &\leq \quad \theta_e - \theta_s &< 2\pi
\end{aligned}\right.$$

**放物線**

　放物線の場合、曲線 $C(t)$ は次のようにパラメトリックに表されます。

| 条件 | 曲線 $C(t)$ および $t$ の範囲 |
|---|---|
| $A,E \neq 0$ <br> $x_s < x_e$ | $C(t) = \big[t, \  -(A/E) t^2, \  z_t\big]^\top, \quad t \in [x_s, x_e]$ |
| $A,E \neq 0$ <br> $x_s > x_e$ | $C(t) = \big[-t, \  -(A/E) t^2, \  z_t\big]^\top, \quad t \in [-x_s, -x_e]$ |
| $C,D \neq 0$ <br> $y_s < y_e$ | $C(t) = \big[-(C/D) t^2, \  t, \  z_t\big]^\top, \quad t \in [y_s, y_e]$ |
| $C,D \neq 0$ <br> $y_s > y_e$ | $C(t) = \big[-(C/D) t^2, \  -t, \  z_t\big]^\top, \quad t \in [-y_s, -y_e]$ |

**双曲線**

　双曲線の場合、曲線 $C(t)$ は次のようにパラメトリックに表されます。

| 条件 1 | 条件 2 | 曲線 $C(t)$ |
|---|---|---|
| $FA < 0$ <br> $FC > 0$ | $y_s < y_e$ | $C(t) = \left[\sqrt{-F/A} \sec{t}, \ \sqrt{F/C} \tan{t}, \ z_t\right]^\top$ <br> $t \in \left[\arctan{\left(y_s\sqrt{C/F}\right)}, \arctan{\left(y_e\sqrt{C/F}\right)}\right]$ |
| ^ | $y_s > y_e$ | $C(t) = \left[\sqrt{-F/A} \sec{t}, \ -\sqrt{F/C} \tan{t}, \ z_t\right]^\top$ <br> $t \in \left[-\arctan{\left(y_s\sqrt{C/F}\right)}, -\arctan{\left(y_e\sqrt{C/F}\right)}\right]$ |
| $FA > 0$ <br> $FC < 0$ | $x_s < x_e$ | $C(t) = \left[\sqrt{F/A} \tan{t}, \ \sqrt{-F/C} \sec{t}, \ z_t\right]^\top$ <br> $t \in \left[\arctan{\left(x_s\sqrt{A/F}\right)}, \arctan{\left(x_e\sqrt{A/F}\right)}\right]$ |
| ^ | $x_s > x_e$ | $C(t) = -\left[\sqrt{F/A} \tan{t}, \ \sqrt{-F/C} \sec{t}, \ z_t\right]^\top$ <br> $t \in \left[-\arctan{\left(x_s\sqrt{A/F}\right)}, -\arctan{\left(x_e\sqrt{A/F}\right)}\right]$ |


#### 導関数 $C'(t), C''(t)$

**楕円**

　楕円の場合、曲線の1階および2階の導関数は、次のように定義されます。定義域については、曲線 $C(t)$ と同様です。

$$
C'(t)  = \begin{bmatrix} -r_x \sin{t} \\\ r_y \cos{t} \\\ 0 \end{bmatrix}, \quad
C''(t) = \begin{bmatrix} -r_x \cos{t} \\\ -r_y \sin{t} \\\ 0 \end{bmatrix}
$$

**放物線**

　放物線の場合、曲線の1階および2階の導関数は、次のように定義されます。定義域については、曲線 $C(t)$ と同様です。

| 条件 | 導関数 $C'(t), C''(t)$ |
|---|---|
| $A,E \neq 0$ <br> $x_s < x_e$ | $C'(t)  = \big[1,\  -2(A/E) t,\  0 \big]$ <br> $C''(t) = \big[0,\ -2(A/E),\ 0 \big]$ |
| $A,E \neq 0$ <br> $x_s > x_e$ | $C'(t)  = \big[-1,\ -2(A/E) t,\ 0 \big]$ <br> $C''(t) = \big[0,\ -2(A/E),\ 0 \big]$ |
| $C,D \neq 0$ <br> $y_s < y_e$ | $C'(t)  = \big[-2(C/D) t,\ 1,\ 0 \big]$ <br> $C''(t) = \big[-2(C/D),\ 0,\ 0 \big]$ |
| $C,D \neq 0$ <br> $y_s > y_e$ | $C'(t)  = \big[-2(C/D) t,\ -1,\ 0 \big]$ <br> $C''(t) = \big[-2(C/D),\ 0,\ 0 \big]$ |

**双曲線**

　双曲線の場合、曲線の1階および2階の導関数は、次のように定義されます。定義域については、曲線 $C(t)$ と同様です。

| 条件 1 | 条件 2 | 導関数 $C'(t), C''(t)$ |
|---|---|---|
| $FA < 0$ <br> $FC > 0$ | $y_s < y_e$ | $C'(t)  = \left[\sqrt{-F/A} \sec{t} \tan{t}, \ \sqrt{F/C} \sec^2{t}, \ 0\right]^\top$ <br> $C''(t) = \left[\sqrt{-F/A} (\sec^3{t} + \sec{t} \tan^2{t}), \ 2\sqrt{F/C} \sec^2{t} \tan{t}, \ 0\right]^\top$ |
| ^ | $y_s > y_e$ | $C'(t)  = \left[\sqrt{-F/A} \sec{t} \tan{t}, \ -\sqrt{F/C} \sec^2{t}, \ 0\right]^\top$ <br> $C''(t) = \left[\sqrt{-F/A} (\sec^3{t} + \sec{t} \tan^2{t}), \ - 2\sqrt{F/C} \sec^2{t} \tan{t}, \ 0\right]^\top$ |
| $FA > 0$ <br> $FC < 0$ | $x_s < x_e$ | $C'(t)  = \left[\sqrt{F/A} \sec^2{t}, \ \sqrt{-F/C} \sec{t} \tan{t}, \ 0\right]^\top$ <br> $C''(t) = \left[2\sqrt{F/A} \sec^2{t} \tan{t}, \ \sqrt{-F/C} (\sec^3{t} + \sec{t} \tan^2{t}), \ 0\right]^\top$ |
| ^ | $x_s > x_e$ | $C'(t)  = \left[-\sqrt{F/A} \sec^2{t}, \ \sqrt{-F/C} \sec{t} \tan{t}, \ 0\right]^\top$ <br> $C''(t) = \left[-2\sqrt{F/A} \sec^2{t} \tan{t}, \ \sqrt{-F/C} (\sec^3{t} + \sec{t} \tan^2{t}), \ 0\right]^\top$ |

曲線 $C(t)$ および導関数 $C'(t), C''(t)$ から計算される各種特徴量については、[Geometric Properties (曲線の幾何学的性質)](./../geometric_properties_ja.md) を参照してください。

## Appendix

### 双曲線のパラメトリック曲線 $C(t)$ の導出

　双曲線の場合、曲線 $C(t)$ は係数 $A, C, F$ の積の符号によって次の2通りに分類されます。

(1) $FA < 0 \land FC > 0$ の場合:

　標準形における係数 $a, b$ を次のように定義します。

$$(a, b) = \left(\sqrt{-F/A}, \sqrt{F/C}\right)$$

この $a, b$ を用いて、始点 $(x_s, y_s)$ と終点 $(x_e, y_e)$ に対応するパラメータ値 $t_i = t_s, t_e$ を次のように定義します。

$$\left\lbrace\begin{aligned}
   &(a \sec{t_i}, b \tan{t_i}) = (x_i, y_i) \\\
   &-\pi/2 < t_i < \pi/2
\end{aligned}\right. \quad (i = s, e)$$

$$\therefore t_i = \arctan{\frac{y_i}{b}} = \arctan{\left(y_i\sqrt{\frac{C}{F}}\right)}$$

| 条件 | 曲線 $C(t)$ |
|---|---|
| $t_s < t_e$ | $C(t) = \big[a \sec{t}, \ b \tan{t}, \ z_t\big]^\top, \quad t \in [t_s, t_e]$ |
| $t_s > t_e$ | $C(t) = \big[a \sec{(-t)}, \ b \tan{(-t)}, \ z_t\big]^\top, \quad t \in [-t_s, -t_e]$ |

(2) $FA > 0 \land FC < 0$ の場合:

　標準形における係数 $a, b$ を次のように定義します。

$$(a, b) = \left(\sqrt{F/A}, \sqrt{-F/C}\right)$$

この $a, b$ を用いて、始点 $(x_s, y_s)$ と終点 $(x_e, y_e)$ に対応するパラメータ値 $t_i = t_s, t_e$ を次のように定義します。

$$\left\lbrace\begin{aligned}
   &(a \tan{t_i}, b \sec{t_i}) = (x_i, y_i) \\\
   &-\pi/2 < t_i < \pi/2
\end{aligned}\right. \quad (i = s, e)$$

$$\therefore t_i = \arctan{\frac{x_i}{a}} = \arctan{\left(x_i\sqrt{\frac{A}{F}}\right)}$$

| 条件 | 曲線 $C(t)$ |
|---|---|
| $t_s < t_e$ | $C(t) = \big[a \tan{t}, \ b \sec{t}, \ z_t\big]^\top, \quad t \in [t_s, t_e]$ |
| $t_s > t_e$ | $C(t) = \big[a \tan{(-t)}, \ b \sec{(-t)}, \ z_t\big]^\top, \quad t \in [-t_s, -t_e]$ |

　以上の結果をまとめると、双曲線の場合の曲線 $C(t)$ は、前述の[曲線 (双曲線)](#曲線)の表のように決定されます（ $\arctan$ の単調増加性を利用）。
