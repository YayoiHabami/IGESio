# Conic Arc (Type 104)

Defined at [curves/conic_arc.h](./../../../include/igesio/entities/curves/conic_arc.h)

## Contents

- [Contents](#contents)
- [Parameters](#parameters)
  - [Specific Parameters](#specific-parameters)
  - [Curve Definition](#curve-definition)
    - [Curve $C(t)$](#curve-ct)
    - [Derivatives $C'(t), C''(t)$](#derivatives-ct-ct)
- [Appendix](#appendix)
  - [Derivation of the Parametric Curve $C(t)$ for Hyperbolas](#derivation-of-the-parametric-curve-ct-for-hyperbolas)

## Parameters

### Specific Parameters

| Member Function | Description |
|---|---|
| `ConicCoefficients()` <br> (`std::array<double, 6>`) | Coefficients of the quadratic equation $(A, B, C, D, E, F)$ |
| `ConicStartPoint()` <br> (`Vector3d`) | Coordinates of the start point $P_s = (x_s, y_s, z_t)$ |
| `ConicTerminatePoint()` <br> (`Vector3d`) | Coordinates of the end point $P_e = (x_e, y_e, z_t)$ |
| `EllipseCenter()` <br> (`Vector3d`) | Center of the ellipse $O_e = (x_e, y_e, z_t)$ (for ellipses) |
| `EllipseRadii()` <br> (`std::array<double, 2>`) | X and Y axis radii of the ellipse $(r_x, r_y)$ (for ellipses) |
| `EllipseStartAngle()` <br> (`double`) | Start angle of the ellipse $\theta_s$ [rad] (for ellipses) |
| `EllipseEndAngle()` <br> (`double`) | End angle of the ellipse $\theta_e$ [rad] (for ellipses) |

> The Conic Arc (Type 104) entity is located on the $z = z_t$ plane in the definition space (`NDim() == 2`).

### Curve Definition

#### Curve $C(t)$

The parametric curve $C(t)$ of a conic section is defined as the set of points that satisfy the following quadratic equation with respect to $x$ and $y$:

$$A x^2 + B xy + C y^2 + D x + E y + F = 0$$

Here, $(A, B, C, D, E, F)$ are the coefficients of the quadratic equation. IGES supports only the following three types of conic sections:

- Ellipse: An ellipse that matches the standard form $\frac{x^2}{{r_x}^2} + \frac{y^2}{{r_y}^2} = 1$
- Parabola: A parabola that matches the standard form $y^2 = 4px$ or $x^2 = 4py$
- Hyperbola: A hyperbola that matches the standard form $\frac{x^2}{{a}^2} - \frac{y^2}{{b}^2} = 1$ or $\frac{y^2}{{b}^2} - \frac{x^2}{{a}^2} = 1$

Therefore, the curve type is classified as follows:

$$AC - \frac{B^2}{4} \begin{cases}
        > 0 & \to\text{Ellipse} \\\
        = 0 & \to\text{Parabola} \\\
        < 0 & \to\text{Hyperbola}
\end{cases}$$

The curve $C(t)$ is defined as a partial curve from the start point $P_s = (x_s, y_s, z_t)$ to the end point $P_e = (x_e, y_e, z_t)$.

**Ellipse**

In the case of an ellipse, the curve $C(t)$ is parametrically expressed as:

$$C(t) = \begin{bmatrix} r_x \cos{t} \\\ r_y \sin{t} \\\ z_t \end{bmatrix}, \quad t \in [\theta_s, \theta_e]$$

Here, $r_x, r_y$ are the radii in the X and Y axis directions, respectively, and $\theta_s, \theta_e$ are the start and end angles, which are defined as:

$$r_x = \sqrt{- F/A}, \quad r_y = \sqrt{- F/C}$$

$$\left\lbrace\begin{aligned}
    &\quad\quad\  \theta_s < \theta_e \\\
    0 &\leq \quad \theta_s &< 2\pi \\\
    0 &\leq \quad \theta_e - \theta_s &< 2\pi
\end{aligned}\right.$$

**Parabola**

In the case of a parabola, the curve $C(t)$ is parametrically expressed as:

| Condition | Curve $C(t)$ and Range of $t$ |
|---|---|
| $A,E \neq 0$ <br> $x_s < x_e$ | $C(t) = \big[t, \  -(A/E) t^2, \  z_t\big]^\top, \quad t \in [x_s, x_e]$ |
| $A,E \neq 0$ <br> $x_s > x_e$ | $C(t) = \big[-t, \  -(A/E) t^2, \  z_t\big]^\top, \quad t \in [-x_s, -x_e]$ |
| $C,D \neq 0$ <br> $y_s < y_e$ | $C(t) = \big[-(C/D) t^2, \  t, \  z_t\big]^\top, \quad t \in [y_s, y_e]$ |
| $C,D \neq 0$ <br> $y_s > y_e$ | $C(t) = \big[-(C/D) t^2, \  -t, \  z_t\big]^\top, \quad t \in [-y_s, -y_e]$ |

**Hyperbola**

In the case of a hyperbola, the curve $C(t)$ is parametrically expressed as:

| Condition 1 | Condition 2 | Curve $C(t)$ |
|---|---|---|
| $FA < 0$ <br> $FC > 0$ | $y_s < y_e$ | $C(t) = \left[\sqrt{-F/A} \sec{t}, \ \sqrt{F/C} \tan{t}, \ z_t\right]^\top$ <br> $t \in \left[\arctan{\left(y_s\sqrt{C/F}\right)}, \arctan{\left(y_e\sqrt{C/F}\right)}\right]$ |
| ^ | $y_s > y_e$ | $C(t) = \left[\sqrt{-F/A} \sec{t}, \ -\sqrt{F/C} \tan{t}, \ z_t\right]^\top$ <br> $t \in \left[-\arctan{\left(y_s\sqrt{C/F}\right)}, -\arctan{\left(y_e\sqrt{C/F}\right)}\right]$ |
| $FA > 0$ <br> $FC < 0$ | $x_s < x_e$ | $C(t) = \left[\sqrt{F/A} \tan{t}, \ \sqrt{-F/C} \sec{t}, \ z_t\right]^\top$ <br> $t \in \left[\arctan{\left(x_s\sqrt{A/F}\right)}, \arctan{\left(x_e\sqrt{A/F}\right)}\right]$ |
| ^ | $x_s > x_e$ | $C(t) = -\left[\sqrt{F/A} \tan{t}, \ \sqrt{-F/C} \sec{t}, \ z_t\right]^\top$ <br> $t \in \left[-\arctan{\left(x_s\sqrt{A/F}\right)}, -\arctan{\left(x_e\sqrt{A/F}\right)}\right]$ |

#### Derivatives $C'(t), C''(t)$

**Ellipse**

In the case of an ellipse, the first and second derivatives of the curve are defined as follows. The domain is the same as for the curve $C(t)$.

$$
C'(t)  = \begin{bmatrix} -r_x \sin{t} \\\ r_y \cos{t} \\\ 0 \end{bmatrix}, \quad
C''(t) = \begin{bmatrix} -r_x \cos{t} \\\ -r_y \sin{t} \\\ 0 \end{bmatrix}
$$

**Parabola**

In the case of a parabola, the first and second derivatives of the curve are defined as follows. The domain is the same as for the curve $C(t)$.

| Condition | Derivatives $C'(t), C''(t)$ |
|---|---|
| $A,E \neq 0$ <br> $x_s < x_e$ | $C'(t)  = \big[1,\  -2(A/E) t,\  0 \big]$ <br> $C''(t) = \big[0,\ -2(A/E),\ 0 \big]$ |
| $A,E \neq 0$ <br> $x_s > x_e$ | $C'(t)  = \big[-1,\ -2(A/E) t,\ 0 \big]$ <br> $C''(t) = \big[0,\ -2(A/E),\ 0 \big]$ |
| $C,D \neq 0$ <br> $y_s < y_e$ | $C'(t)  = \big[-2(C/D) t,\ 1,\ 0 \big]$ <br> $C''(t) = \big[-2(C/D),\ 0,\ 0 \big]$ |
| $C,D \neq 0$ <br> $y_s > y_e$ | $C'(t)  = \big[-2(C/D) t,\ -1,\ 0 \big]$ <br> $C''(t) = \big[-2(C/D),\ 0,\ 0 \big]$ |

**Hyperbola**

In the case of a hyperbola, the first and second derivatives of the curve are defined as follows. The domain is the same as for the curve $C(t)$.

| Condition 1 | Condition 2 | Derivatives $C'(t), C''(t)$ |
|---|---|---|
| $FA < 0$ <br> $FC > 0$ | $y_s < y_e$ | $C'(t)  = \left[\sqrt{-F/A} \sec{t} \tan{t}, \ \sqrt{F/C} \sec^2{t}, \ 0\right]^\top$ <br> $C''(t) = \left[\sqrt{-F/A} (\sec^3{t} + \sec{t} \tan^2{t}), \ 2\sqrt{F/C} \sec^2{t} \tan{t}, \ 0\right]^\top$ |
| ^ | $y_s > y_e$ | $C'(t)  = \left[\sqrt{-F/A} \sec{t} \tan{t}, \ -\sqrt{F/C} \sec^2{t}, \ 0\right]^\top$ <br> $C''(t) = \left[\sqrt{-F/A} (\sec^3{t} + \sec{t} \tan^2{t}), \ - 2\sqrt{F/C} \sec^2{t} \tan{t}, \ 0\right]^\top$ |
| $FA > 0$ <br> $FC < 0$ | $x_s < x_e$ | $C'(t)  = \left[\sqrt{F/A} \sec^2{t}, \ \sqrt{-F/C} \sec{t} \tan{t}, \ 0\right]^\top$ <br> $C''(t) = \left[2\sqrt{F/A} \sec^2{t} \tan{t}, \ \sqrt{-F/C} (\sec^3{t} + \sec{t} \tan^2{t}), \ 0\right]^\top$ |
| ^ | $x_s > x_e$ | $C'(t)  = \left[-\sqrt{F/A} \sec^2{t}, \ \sqrt{-F/C} \sec{t} \tan{t}, \ 0\right]^\top$ <br> $C''(t) = \left[-2\sqrt{F/A} \sec^2{t} \tan{t}, \ \sqrt{-F/C} (\sec^3{t} + \sec{t} \tan^2{t}), \ 0\right]^\top$ |

For various features calculated from the curve $C(t)$ and the derivatives $C'(t), C''(t)$, please refer to [Geometric Properties](./../geometric_properties.md).

## Appendix

### Derivation of the Parametric Curve $C(t)$ for Hyperbolas

In the case of hyperbolas, the curve $C(t)$ is classified into the following two types depending on the sign of the product of the coefficients $A, C, F$:

(1) Case of $FA < 0 \land FC > 0$:

The coefficients $a, b$ in the standard form are defined as follows:

$$(a, b) = \left(\sqrt{-F/A}, \sqrt{F/C}\right)$$

Using these $a, b$, the parameter values $t_i = t_s, t_e$ corresponding to the start point $(x_s, y_s)$ and end point $(x_e, y_e)$ are defined as follows:

$$\left\lbrace\begin{aligned}
     &(a \sec{t_i}, b \tan{t_i}) = (x_i, y_i) \\\
     &-\pi/2 < t_i < \pi/2
\end{aligned}\right. \quad (i = s, e)$$

$$\therefore t_i = \arctan{\frac{y_i}{b}} = \arctan{\left(y_i\sqrt{\frac{C}{F}}\right)}$$

| Condition | Curve $C(t)$ |
|---|---|
| $t_s < t_e$ | $C(t) = \big[a \sec{t}, \ b \tan{t}, \ z_t\big]^\top, \quad t \in [t_s, t_e]$ |
| $t_s > t_e$ | $C(t) = \big[a \sec{(-t)}, \ b \tan{(-t)}, \ z_t\big]^\top, \quad t \in [-t_s, -t_e]$ |

(2) Case of $FA > 0 \land FC < 0$:

The coefficients $a, b$ in the standard form are defined as follows:

$$(a, b) = \left(\sqrt{F/A}, \sqrt{-F/C}\right)$$

Using these $a, b$, the parameter values $t_i = t_s, t_e$ corresponding to the start point $(x_s, y_s)$ and end point $(x_e, y_e)$ are defined as follows:

$$\left\lbrace\begin{aligned}
     &(a \tan{t_i}, b \sec{t_i}) = (x_i, y_i) \\\
     &-\pi/2 < t_i < \pi/2
\end{aligned}\right. \quad (i = s, e)$$

$$\therefore t_i = \arctan{\frac{x_i}{a}} = \arctan{\left(x_i\sqrt{\frac{A}{F}}\right)}$$

| Condition | Curve $C(t)$ |
|---|---|
| $t_s < t_e$ | $C(t) = \big[a \tan{t}, \ b \sec{t}, \ z_t\big]^\top, \quad t \in [t_s, t_e]$ |
| $t_s > t_e$ | $C(t) = \big[a \tan{(-t)}, \ b \sec{(-t)}, \ z_t\big]^\top, \quad t \in [-t_s, -t_e]$ |

Summarizing the above results, the curve $C(t)$ for hyperbolas is determined as shown in the table of [Curve (Hyperbola)](#curve) described above (using the monotonic increase of $\arctan$).
