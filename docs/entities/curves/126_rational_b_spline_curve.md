# Rational B-Spline Curve (Entity Type 126)

Defined in [curves/rational_b_spline_curve.h](./../../../include/igesio/entities/curves/rational_b_spline_curve.h)

## Table of Contents

- [Table of Contents](#table-of-contents)
- [Parameters](#parameters)
  - [Intrinsic Parameters](#intrinsic-parameters)
    - [`RationalBSplineCurveType`](#rationalbsplinecurvetype)
  - [Curve Definition](#curve-definition)
    - [Curve $C(t)$](#curve-ct)
    - [Derivatives $C'(t), C''(t)$](#derivatives-ct-ct)
- [Appendix](#appendix)
  - [Implementation Details](#implementation-details)
    - [`src/entities/curves/nurbs_basis_function.h`](#srcentitiescurvesnurbs_basis_functionh)
    - [`RationalBSplineCurve::TryGetDefinedPointAt(t)`](#rationalbsplinecurvetrygetdefinedpointatt)
    - [`RationalBSplineCurve::TryGetDerivatives(t, n)`](#rationalbsplinecurvetrygetderivativest-n)

## Parameters

### Intrinsic Parameters

| Member Function | Description |
|---|---|
| `GetCurveType()` <br> (`RationalBSplineCurveType`) | Type of B-spline curve |
| `Degree()` <br> (`int`) | Degree $m$ of the B-spline curve |
| `ControlPoints()` <br> (`Matrix3Xd`) | Control points $P_i = (x_i, y_i, z_i)\ (i = 0, 1, \ldots, k)$ |
| `Knots()` <br> (`std::vector<double>`) | Knot vector $t_{-m}, \ldots, t_{1+k}$ |
| `Weights()` <br> (`std::vector<double>`) | Weights $w_i\ (i = 0, 1, \ldots, k)$ |

#### `RationalBSplineCurveType`

This enumeration represents the type of Rational B-spline curve, corresponding one-to-one with the form number (`GetFormNumber()`). If not specified, `kUndetermined` is returned.

| Form | Enumerator | Description |
|---|---|---|
| 0 | `kUndetermined` | Curve shape is determined by parameters |
| 1 | `kLine` | Line |
| 2 | `kCircularArc` | Circular arc or circle |
| 3 | `kEllipticArc` | Elliptic arc or ellipse |
| 4 | `kParabolicArc` | Parabolic arc or parabola |
| 5 | `kHyperbolicArc` | Hyperbolic arc or hyperbola |

### Curve Definition

#### Curve $C(t)$

A Rational B-spline curve $C(t)$ is defined by the following five parameters:

- Degree $m$
- Control points $P_i = (x_i, y_i, z_i)\ (i = 0, 1, \ldots, k)$ (total $k + 1$)
- Knot vector $t_{-m}, \ldots, t_{1+k}$ (total $k + m + 2$)
- Weights $w_i\ (i = 0, 1, \ldots, k)$ (total $k + 1$)
- Parameter range $t \in [t_{start}, t_{end}]$

The parameter range $[t_{start}, t_{end}]$ can be obtained via the `GetParameterRange()` function and satisfies:

$$T_0 \leq t_{start} < t_{end} \leq T_{1+k}$$

The B-spline basis functions $b_{i,m}(t)$ are defined from the above parameters, and the Rational B-spline curve $C(t)$ is given by:

$$C(t) = \frac{\sum_{i=0}^{k} b_{i,m}(t) w_i P_i}{\sum_{i=0}^{k} b_{i,m}(t) w_i}, \quad t \in [t_{start}, t_{end}]$$

The B-spline basis functions $b_{i,m}(t)$ are recursively defined as follows, based on degree $m$ and the knot vector $t_{-m}, \ldots, t_{1+k}$:

$$b_{i,0}(t) = \begin{cases}
    1 & t_i \leq t < t_{i+1} \\
    0 & \text{otherwise}
\end{cases}$$

$$b_{i,l}(t) = \frac{t - t_i}{t_{i+l} - t_i} b_{i,l-1}(t) + \frac{t_{i+l+1} - t}{t_{i+l+1} - t_{i+1}} b_{i+1,l-1}(t), \quad l = 1, 2, \ldots, m$$

#### Derivatives $C'(t), C''(t)$

The first and second derivatives of the Rational B-spline curve $C(t)$, denoted $C'(t)$ and $C''(t)$, are defined as follows. For details on notation and computation, see [Appendix - Implementation Details](#appendix).

$$C'(t) = \frac{1}{w(t)} \left( A'(t) - w'(t) C(t) \right)$$

$$C''(t) = \frac{1}{w(t)} \left( A''(t) - 2 w'(t) C'(t) - w''(t) C(t) \right)$$

For various features calculated from the curve $C(t)$ and the derivatives $C'(t), C''(t)$, please refer to [Geometric Properties](./../geometric_properties.md).

## Appendix

### Implementation Details

#### `src/entities/curves/nurbs_basis_function.h`

Various computations for Rational B-spline curves $C(t)$ are implemented in the (internal API) function `TryComputeBasisFunctions` in `src/entities/curves/nurbs_basis_function.h`. This function computes the B-spline basis functions $b_{i,m}(t)$ and their derivatives for a given parameter value $t$. The return value is either the following `BasisFunctionResult` struct or `std::nullopt`:

```cpp
struct BasisFunctionResult {
    // Index j of the knot span where t ∈ [t_j, t_{j+1})
    int knot_span;
    // Values of the basis functions
    std::vector<double> values;
    // Values of the derivatives of the basis functions
    std::vector<std::vector<double>> derivatives;
};
```

#### `RationalBSplineCurve::TryGetDefinedPointAt(t)`

To obtain a point on the Rational B-spline curve $C(t)$ (within its domain), use the `RationalBSplineCurve::TryGetDefinedPointAt(t)` function. This function takes a parameter value $t$ and computes the point $C(t)$ on the curve, utilizing the above `TryComputeBasisFunctions` function.

For $t \in [t_{j}, t_{j+1})$ with index $j$, the local basis functions $b_{j-m+i, m}(t)$ are computed (corresponding to `BasisFunctionResult::values`):

$$b_{j - m + i, m}(t), \quad i = 0, 1, \ldots, m$$

Using these basis functions, the point on the Rational B-spline curve $C(t)$ is calculated as:

$$C(t) = \frac{\sum_{i=0}^{m} b_{j - m + i, m}(t) w_{j - m + i} P_{j - m + i}}{\sum_{i=0}^{m} b_{j - m + i, m}(t) w_{j - m + i}} = \frac{\sum_{l=j-m}^{j} b_{l, m}(t) w_{l} P_{l}}{\sum_{l=j-m}^{j} b_{l, m}(t) w_{l}},\quad \text{if } \sum_{l=j-m}^{j} b_{l, m}(t) w_{l} \neq 0$$

From here, we use the following notation, where $j$ is the index such that $t \in [t_j, t_{j+1})$:

$$C(t) = \frac{A(t)}{w(t)}, \quad \text{where } \left\lbrace\begin{aligned}
    A(t) &= \sum_{l=j-m}^{j} b_{l, m}(t) w_{l} P_{l} \\
    w(t) &= \sum_{l=j-m}^{j} b_{l, m}(t) w_{l}
\end{aligned}\right.$$

#### `RationalBSplineCurve::TryGetDerivatives(t, n)`

To obtain derivatives up to order $n$ ($C'(t), C''(t), \ldots, C^{(n)}(t)$) of the Rational B-spline curve $C(t)$ (within its domain), use the `RationalBSplineCurve::TryGetDerivatives(t, n)` function. This function takes a parameter value $t$ and derivative order $n$, and computes the derivatives using the above `TryComputeBasisFunctions` function.

First, the $n$-th derivatives of the numerator $A(t)$ and denominator $w(t)$ are defined as:

$$A^{(n)}(t) = \sum_{l=j-m}^{j} b_{l, m}^{(n)}(t) w_{l} P_{l}, \quad w^{(n)}(t) = \sum_{l=j-m}^{j} b_{l, m}^{(n)}(t) w_{l}$$

Here, $b_{l, m}^{(n)}(t)$ denotes the $n$-th derivative of the basis function $b_{l, m}(t)$, corresponding to `BasisFunctionResult::derivatives[n - 1][l - (j - m)]`. For $n = 0$, $b_{l, m}^{(0)}(t) = b_{l, m}(t)$, corresponding to `BasisFunctionResult::values[l - (j - m)]`.

The $n$-th derivative of the Rational B-spline curve $C(t)$ is then computed using the quotient rule:

$$C^{(n)}(t) = \frac{1}{w(t)} \left( A^{(n)}(t) - \sum_{k=0}^{n-1} \binom{n}{k} w^{(n-k)}(t) C^{(k)}(t) \right)$$

Here, $\binom{n}{k}$ is the binomial coefficient, computed by the `BinomialCoefficient` function in `igesio/numerics/combinatorics.h`.

**Example Calculations**

For derivatives up to order 2:

$$C^{(0)}(t) = C(t) = \frac{A(t)}{w(t)}$$

$$C^{(1)}(t) = \frac{1}{w(t)} \left( A^{(1)}(t) - w^{(1)}(t) C^{(0)}(t) \right)$$

$$C^{(2)}(t) = \frac{1}{w(t)} \left( A^{(2)}(t) - 2 w^{(1)}(t) C^{(1)}(t) - w^{(2)}(t) C^{(0)}(t) \right)$$
