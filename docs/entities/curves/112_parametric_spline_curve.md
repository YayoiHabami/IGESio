# Parametric Spline Curve (Type 112)

Defined at [curves/parametric_spline_curve.h](./../../../include/igesio/entities/curves/parametric_spline_curve.h)

## Contents

- [Contents](#contents)
- [Parameters](#parameters)
  - [Specific Parameters](#specific-parameters)
    - [`ParametricSplineCurveType`](#parametricsplinecurvetype)
  - [Curve Definition](#curve-definition)
    - [Curve $C(t)$](#curve-ct)
    - [Derivatives $C'(t), C''(t)$](#derivatives-ct-ct)

## Parameters

### Specific Parameters

| Member Function | Description |
|---|---|
| `GetCurveType` <br> `ParametricSplineCurveType` | Type of curve |
| `Degree()` <br> `unsigned int` | Degree of the curve $h\ (0 \leq h \leq 3)$ |
| `NumberOfSegments()` <br> `unsigned int` | Number of segments $n$ |
| `Breakpoints()` <br> `std::vector<double>` | Breakpoints $T(0), \ldots, T(n)$ |
| `Coefficients(i)` <br> `Matrix34d` | Coefficient matrix $A_i \in \mathbb{R}^{3\times 4}$ for segment $i$ |
| `Coefficients()` <br> `Matrix3Xd` | Matrix of $3 \times 4n$ formed by horizontally concatenating the coefficient matrices for each segment |

#### `ParametricSplineCurveType`

`ParametricSplineCurveType` is an enumeration that indicates the shape of this entity. It corresponds to the first parameter, `CTYPE`, in the Parameter Data section. The following types exist, but it is not guaranteed that the return value of `GetCurveType()` represents the current shape.

| `CTYPE` | Enumeration Value | Description |
|---|---|---|
| 1 | `kLinear` | Straight line |
| 2 | `kQuadratic` | Quadratic curve |
| 3 | `kCubic` | Cubic curve |
| 4 | `kWilsonFowler` | Wilson-Fowler curve |
| 5 | `kModifiedWilsonFowler` | Modified Wilson-Fowler curve |
| 6 | `kBSpline` | B-spline curve |

### Curve Definition

#### Curve $C(t)$

A Parametric Spline Curve is a curve composed of $n$ polynomial segments of up to the third degree. The parametric curve $C_i(t)$ in segment $i$ is defined as follows:

$$C_i(t) = \begin{bmatrix} a_{x,i} & b_{x,i} & c_{x,i} & d_{x,i} \\\ a_{y,i} & b_{y,i} & c_{y,i} & d_{y,i} \\\ a_{z,i} & b_{z,i} & c_{z,i} & d_{z,i} \end{bmatrix} \begin{bmatrix} 1 \\\ s \\\ s^2 \\\ s^3 \end{bmatrix} = A_i \begin{bmatrix} 1 \\\ s \\\ s^2 \\\ s^3 \end{bmatrix}$$

$$(s = t - T(i), \quad T(i) \leq t < T(i+1))$$

Here, the coefficient matrix $A_i \in \mathbb{R}^{3\times 4}$ contains the polynomial coefficients in segment $i$ and satisfies the following conditions:

1.  To avoid degeneracy, at least one of the nine coefficients $b_{p,i}, c_{p,i}, d_{p,i} \ (p = x, y, z)$ must be non-zero.
2.  For 2D (planar) splines, the curve is defined on the $z = z_t$ plane (i.e., $a_{z,i} = z_t, b_{z,i} = c_{z,i} = d_{z,i} = 0$ ).
3.  The parameter $h$ is used as an indicator of the smoothness of the curve and satisfies the following conditions:

| $h$ | Description |
|---|---|
| 0 | The curve is continuous at all breakpoints. |
| 1 | The curve is continuous and has a continuous first derivative at all breakpoints, and $c_{p,i} = d_{p,i} = 0\ (p = x, y, z), i = 0, \ldots, n-1$ |
| 2 | The curve is continuous and has continuous first and second derivatives at all breakpoints, and $d_{p,i} = 0\ (p = x, y, z), i = 0, \ldots, n-1$ |
| 3 | (IGES 5.3 does not specify continuity conditions for this value) |

The overall parametric curve $C(t)$ is defined by combining each segment $C_i(t)$ as follows:

$$C(t) = \begin{cases}
    C_0(t), & T(0) \leq t < T(1) \\\
    C_1(t), & T(1) \leq t < T(2) \\\
    \vdots & \vdots \\\
    C_{n-1}(t), & T(n-1) \leq t \leq T(n)
\end{cases}$$

The domain of the curve $C(t)$ is $[T(0), T(n)]$.

#### Derivatives $C'(t), C''(t)$

The first and second derivatives of each segment $C_i(t)$ are defined as follows:

$$C_i'(t) = A_i \begin{bmatrix} 0 \\\ 1 \\\ 2s \\\ 3s^2 \end{bmatrix}, \quad C_i''(t) = A_i \begin{bmatrix} 0 \\\ 0 \\\ 2 \\\ 6s \end{bmatrix}$$

The domain and combination over the entire curve follow the definition of the curve $C(t)$.

For various features calculated from the curve $C(t)$ and the derivatives $C'(t), C''(t)$, please refer to [Geometric Properties](./../geometric_properties.md).
