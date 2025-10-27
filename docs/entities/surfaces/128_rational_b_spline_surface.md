# Rational B-Spline Surface (Type 128)

Defined in [surfaces/rational_b_spline_surface.h](./../../../include/igesio/entities/surfaces/rational_b_spline_surface.h)

## Table of Contents

- [Table of Contents](#table-of-contents)
- [Parameters](#parameters)
  - [Intrinsic Parameters](#intrinsic-parameters)
    - [`RationalBSplineSurfaceType`](#rationalbsplinesurfacetype)
  - [Surface Definition](#surface-definition)
    - [Surface $S(u, v)$](#surface-su-v)
    - [Derivatives $S\_u, S\_v, S\_{uu}, S\_{uv}, S\_{vv}$](#derivatives-s_u-s_v-s_uu-s_uv-s_vv)
- [Appendix](#appendix)
  - [Computation in Code](#computation-in-code)
    - [`src/entities/curves/nurbs_basis_function.h`](#srcentitiescurvesnurbs_basis_functionh)
    - [`RationalBSplineSurface::TryGetDefinedPointAt(u, v)`](#rationalbsplinesurfacetrygetdefinedpointatu-v)
    - [`RationalBSplineSurface::TryGetDerivatives(u, v, n)`](#rationalbsplinesurfacetrygetderivativesu-v-n)

## Parameters

### Intrinsic Parameters

| Member Function | Description |
|---|---|
| `GetSurfaceType()` <br> (`RationalBSplineSurfaceType`) | Returns the type of the rational B-spline surface |
| `Degrees()` <br> (`std::pair<unsigned int, unsigned int>`) | Degrees $m_u, m_v$ in the $u$ and $v$ directions |
| `NumControlPoints()` <br> (`std::pair<unsigned int, unsigned int>`) | Number of control points in $u$ and $v$ directions: $k_u + 1, k_v + 1$ |
| `UKnots()` <br> (`std::vector<double>`) | Knot vector in the $u$ direction <br> $u_{-m_u}, \ldots, u_{1+k_u}$ |
| `VKnots()` <br> (`std::vector<double>`) | Knot vector in the $v$ direction <br> $v_{-m_v}, \ldots, v_{1+k_v}$ |
| `ControlPoints()` <br> (`Matrix3Xd`) | Control points $P_{i,j} = (x_{i,j}, y_{i,j}, z_{i,j})\ (i = 0, \ldots, k_u; j = 0, \ldots, k_v)$ <br> $P_{i,j}$ is stored in column `i*(k_v + 1) + j` |
| `ControlPointAt(i, j)` <br> (`Vector3d`) | Control point $P_{i,j} = (x_{i,j}, y_{i,j}, z_{i,j})$ |
| `Weights()` <br> (`MatrixXd`) | Weights $w_{i,j}\ (i = 0, \ldots, k_u; j = 0, \ldots, k_v)$ <br> $w_{i,j}$ is stored at index `i + j*(k_u + 1)` |
| `WeightAt(i, j)` <br> (`double`) | Weight $w_{i,j}$ |

#### `RationalBSplineSurfaceType`

This is an enumeration that corresponds one-to-one with the form number (`GetFormNumber()`), representing the type of rational B-spline surface. If not specified, `kUndetermined` is returned.

| form | Enumerator | Description |
|---|---|---|
| 0 | `kUndetermined` | The surface type is determined from the B-spline parameters |
| 1 | `kPlane` | Plane |
| 2 | `kRightCircularCylinder` | Right circular cylinder |
| 3 | `kCone` | Cone |
| 4 | `kSphere` | Sphere |
| 5 | `kTorus` | Torus |
| 6 | `kSurfaceOfRevolution` | Surface of revolution |
| 7 | `kTabulatedCylinder` | Tabulated cylinder |
| 8 | `kRuledSurface` | Ruled surface |
| 9 | `kGeneralQuadricSurface` | General quadric surface |

### Surface Definition

#### Surface $S(u, v)$

A rational B-spline surface $S(u, v)$ is defined by the following seven parameters:

- Degrees $m_u, m_v$ in the $u$ and $v$ directions
- Control points $P_{i,j} = (x_{i,j}, y_{i,j}, z_{i,j})\ (i = 0, \ldots, k_u; j = 0, \ldots, k_v)$ (total of $(k_u + 1) \times (k_v + 1)$)
- Knot vector in the $u$ direction: $u_{-m_u}, \ldots, u_{1+k_u}$ (total of $k_u + m_u + 2$)
- Knot vector in the $v$ direction: $v_{-m_v}, \ldots, v_{1+k_v}$ (total of $k_v + m_v + 2$)
- Weights $w_{i,j}\ (i = 0, \ldots, k_u; j = 0, \ldots, k_v)$ (total of $(k_u + 1) \times (k_v + 1)$)
- Parameter ranges $u \in [u_{\text{start}}, u_{\text{end}}], \ v \in [v_{\text{start}}, v_{\text{end}}]$

The parameter ranges $[u_{\text{start}}, u_{\text{end}}]$ and $[v_{\text{start}}, v_{\text{end}}]$ can be obtained via the `GetURange()` and `GetVRange()` functions, and satisfy the following:

$$\begin{aligned}
    &u_0 \leq u_{\text{start}} < u_{\text{end}} \leq u_{1+k_u} \\\
    &v_0 \leq v_{\text{start}} < v_{\text{end}} \leq v_{1+k_v}
\end{aligned}$$

Based on these parameters, the B-spline basis functions $b_{i,m}(t)$ are defined, and the rational B-spline surface $S(u, v)$ is given by (see [Appendix - Computation in Code](#computation-in-code) for derivation):

$$S(u, v) = \frac{A(u, v)}{w(u, v)}$$

$$\begin{aligned}
    A(u, v) &= \sum_{p=k - m_u}^{k} \sum_{q=l - m_v}^{l} b_{p, m_u}(u) b_{q, m_v}(v) w_{p, q} P_{p, q} \\\
    w(u, v) &= \sum_{p=k - m_u}^{k} \sum_{q=l - m_v}^{l} b_{p, m_u}(u) b_{q, m_v}(v) w_{p, q}
\end{aligned}$$

Here, $k, l$ are the indices such that $u \in [u_k, u_{k+1})$, $v \in [v_l, v_{l+1})$.

#### Derivatives $S_u, S_v, S_{uu}, S_{uv}, S_{vv}$

Let $A(u, v)$ and $w(u, v)$ be as above. The $(n_u, n_v)$-th derivatives with respect to $u$ and $v$ are defined as:

$$\begin{aligned}
    A^{(n_u, n_v)}(u, v) &= \sum_{p=k - m_u}^{k} \sum_{q=l - m_v}^{l} b_{p, m_u}^{(n_u)}(u) b_{q, m_v}^{(n_v)}(v) w_{p, q} P_{p, q} \\\
    w^{(n_u, n_v)}(u, v) &= \sum_{p=k - m_u}^{k} \sum_{q=l - m_v}^{l} b_{p, m_u}^{(n_u)}(u) b_{q, m_v}^{(n_v)}(v) w_{p, q}
\end{aligned}$$

Here, $b_{p, m_u}^{(n_u)}(u)$ and $b_{q, m_v}^{(n_v)}(v)$ are the $n_u$-th and $n_v$-th derivatives of the basis functions, respectively. See [Appendix - Computation in Code](#computation-in-code) for details.

The derivatives of the rational B-spline surface $S(u, v)$ are then computed as follows (see [Appendix - Computation in Code](#computation-in-code) for derivation):

**First derivatives**

$$\begin{aligned}
    S_u(u, v) &= \frac{1}{w(u, v)} \left( A^{(1, 0)}(u, v) - w^{(1, 0)}(u, v) S(u, v) \right) \\\
    S_v(u, v) &= \frac{1}{w(u, v)} \left( A^{(0, 1)}(u, v) - w^{(0, 1)}(u, v) S(u, v) \right)
\end{aligned}$$

**Second derivatives**

$$\begin{aligned}
    S_{uu}(u, v) &= \frac{1}{w(u, v)} \left( A^{(2, 0)}(u, v) - 2 w^{(1, 0)}(u, v) S_u(u, v) - w^{(2, 0)}(u, v) S(u, v) \right) \\\
    S_{uv}(u, v) &= \frac{1}{w(u, v)} \left( A^{(1, 1)}(u, v) - w^{(1, 0)}(u, v) S_v(u, v) - w^{(0, 1)}(u, v) S_u(u, v) - w^{(1, 1)}(u, v) S(u, v) \right) \\\
    S_{vv}(u, v) &= \frac{1}{w(u, v)} \left( A^{(0, 2)}(u, v) - 2 w^{(0, 1)}(u, v) S_v(u, v) - w^{(0, 2)}(u, v) S(u, v) \right)
\end{aligned}$$

For geometric properties that can be computed from $S(u,v)$ and its partial derivatives, see [geometric_properties.md](./../geometric_properties.md#geometric-properties-of-surfaces).

## Appendix

### Computation in Code

#### `src/entities/curves/nurbs_basis_function.h`

Various computations for rational B-spline curves $C(t)$ and surfaces $S(u, v)$ are implemented in the (internal) function `TryComputeBasisFunctions` in `src/entities/curves/nurbs_basis_function.h`. This function computes the B-spline basis functions $b_{i,m}(t)$ and their derivatives for a given parameter $t$. The return value is either a `BasisFunctionResult` struct or `std::nullopt`:

```cpp
struct BasisFunctionResult {
    // Index j of the knot span such that t ∈ [t_j, t_{j+1})
    int knot_span;
    // Values of the basis functions b_{j-m,m}(t), ..., b_{j,m}(t)
    std::vector<double> values;
    // Derivatives of the basis functions b_{j-m,m}^(i)(t), ..., b_{j,m}^(i)(t)
    // derivatives[i] contains the (i+1)-th derivatives
    std::vector<std::vector<double>> derivatives;
};
```

#### `RationalBSplineSurface::TryGetDefinedPointAt(u, v)`

To obtain a point on the rational B-spline surface $S(u, v)$ (within its domain), use the `RationalBSplineSurface::TryGetDefinedPointAt(u, v)` function. This function takes parameter values $u, v$ and returns the corresponding point $S(u, v)$ on the surface. The computation uses the `TryComputeBasisFunctions` function described above.

For $u \in [u_k, u_{k+1})$, $v \in [v_l, v_{l+1})$, the local basis functions $b_{k - m_u + i, m_u}(u)$ and $b_{l - m_v + j, m_v}(v)$ are computed (corresponding to `BasisFunctionResult::values`):

$$\begin{aligned}
    &b_{k - m_u + i, m_u}(u), \quad i = 0, 1, \ldots, m_u \\\
    &b_{l - m_v + j, m_v}(v), \quad j = 0, 1, \ldots, m_v
\end{aligned}$$

Using these basis functions, the point $S(u, v)$ on the rational B-spline surface is computed as:

$$\begin{aligned}
    S(u, v) &= \frac{\sum_{i=0}^{m_u} \sum_{j=0}^{m_v} b_{k - m_u + i, m_u}(u) b_{l - m_v + j, m_v}(v) w_{k - m_u + i, l - m_v + j} P_{k - m_u + i, l - m_v + j}}{\sum_{i=0}^{m_u} \sum_{j=0}^{m_v} b_{k - m_u + i, m_u}(u) b_{l - m_v + j, m_v}(v) w_{k - m_u + i, l - m_v + j}} \\\
    &= \frac{\sum_{p=k - m_u}^{k} \sum_{q=l - m_v}^{l} b_{p, m_u}(u) b_{q, m_v}(v) w_{p, q} P_{p, q}}{\sum_{p=k - m_u}^{k} \sum_{q=l - m_v}^{l} b_{p, m_u}(u) b_{q, m_v}(v) w_{p, q}} \\\
    &\quad \left(\text{if } \sum_{p=k - m_u}^{k} \sum_{q=l - m_v}^{l} b_{p, m_u}(u) b_{q, m_v}(v) w_{p, q} \neq 0\right)
\end{aligned}$$

Hereafter, we denote the above as follows, where $k, l$ are the indices such that $u \in [u_k, u_{k+1})$, $v \in [v_l, v_{l+1})$:

$$S(u, v) = \frac{A(u, v)}{w(u, v)}$$

$$\begin{aligned}
    A(u, v) &= \sum_{p=k - m_u}^{k} \sum_{q=l - m_v}^{l} b_{p, m_u}(u) b_{q, m_v}(v) w_{p, q} P_{p, q} \\\
    w(u, v) &= \sum_{p=k - m_u}^{k} \sum_{q=l - m_v}^{l} b_{p, m_u}(u) b_{q, m_v}(v) w_{p, q}
\end{aligned}$$

#### `RationalBSplineSurface::TryGetDerivatives(u, v, n)`

To obtain partial derivatives of the rational B-spline surface $S(u, v)$ up to order $n$ (within its domain), use the `RationalBSplineSurface::TryGetDerivatives(u, v, n)` function. This function takes parameter values $u, v$ and the derivative order $n$, and returns the computed partial derivatives. The computation uses the `TryComputeBasisFunctions` function described above. For example, when $n = 2$, the following six partial derivatives are computed:

$$S(u, v), \quad S_u(u, v), \quad S_v(u, v), \quad S_{uu}(u, v), \quad S_{uv}(u, v), \quad S_{vv}(u, v)$$

First, the partial derivatives of $A(u, v)$ and $w(u, v)$ are defined as:

$$\begin{aligned}
    A^{(n_u, n_v)}(u, v) &= \frac{\partial^{n_u + n_v}}{\partial u^{n_u} \partial v^{n_v}} A(u, v) &&= \sum_{p=k - m_u}^{k} \sum_{q=l - m_v}^{l} b_{p, m_u}^{(n_u)}(u) b_{q, m_v}^{(n_v)}(v) w_{p, q} P_{p, q} \\\
    w^{(n_u, n_v)}(u, v) &= \frac{\partial^{n_u + n_v}}{\partial u^{n_u} \partial v^{n_v}} w(u, v) &&= \sum_{p=k - m_u}^{k} \sum_{q=l - m_v}^{l} b_{p, m_u}^{(n_u)}(u) b_{q, m_v}^{(n_v)}(v) w_{p, q}
\end{aligned}$$

To compute the partial derivatives $S^{(n_u, n_v)}(u, v) = \frac{\partial^{n_u + n_v}}{\partial u^{n_u} \partial v^{n_v}} S(u, v)$, differentiate both sides of $S = A/w \ \Rightarrow \ S\cdot w = A$ $n_u$ times with respect to $u$ and $n_v$ times with respect to $v$. Applying the generalized Leibniz rule for product differentiation, we have:

$$\frac{\partial^{n_u + n_v}}{\partial u^{n_u} \partial v^{n_v}} (S(u, v) \cdot w(u, v)) = \sum_{i=0}^{n_u} \sum_{j=0}^{n_v} \binom{n_u}{i} \binom{n_v}{j} S^{(i, j)}(u, v) \cdot w^{(n_u - i, n_v - j)}(u, v)$$

The right-hand side is simply $A^{(n_u, n_v)}(u, v)$. Thus,

$$\sum_{i=0}^{n_u} \sum_{j=0}^{n_v} \binom{n_u}{i} \binom{n_v}{j} S^{(i, j)}(u, v) \cdot w^{(n_u - i, n_v - j)}(u, v) = A^{(n_u, n_v)}(u, v)$$

Extracting the term for $S^{(n_u, n_v)}(u, v)$:

$$\binom{n_u}{n_u} \binom{n_v}{n_v} S^{(n_u, n_v)}(u, v) \cdot w^{(0, 0)}(u, v) = 1\cdot 1\cdot S^{(n_u, n_v)}(u, v) \cdot w(u, v) = S^{(n_u, n_v)}(u, v) \cdot w(u, v)$$

Therefore, solving for $S^{(n_u, n_v)}(u, v)$ gives:

$$\begin{aligned}
    &\quad S^{(n_u, n_v)}(u, v) \cdot w(u, v) + \underset{(i, j) \neq (n_u, n_v)}{\sum_{i = 0}^{n_u} \sum_{j = 0}^{n_v}} \binom{n_u}{i} \binom{n_v}{j} S^{(i, j)}(u, v) \cdot w^{(n_u - i, n_v - j)}(u, v) = A^{(n_u, n_v)}(u, v) \\\
    &\Rightarrow S^{(n_u, n_v)}(u, v) = \frac{1}{w(u, v)} \left( A^{(n_u, n_v)}(u, v) - \underset{(i, j) \neq (n_u, n_v)}{\sum_{i = 0}^{n_u} \sum_{j = 0}^{n_v}} \binom{n_u}{i} \binom{n_v}{j} S^{(i, j)}(u, v) \cdot w^{(n_u - i, n_v - j)}(u, v) \right)
\end{aligned}$$

**Example: First derivatives**

$$\begin{aligned}
    S_u(u, v) &= \frac{1}{w(u, v)} \left( A^{(1, 0)}(u, v) - S(u, v) \cdot w^{(1, 0)}(u, v) \right) \\\
    &\quad \\\
    S_v(u, v) &= \frac{1}{w(u, v)} \left( A^{(0, 1)}(u, v) - S(u, v) \cdot w^{(0, 1)}(u, v) \right)
\end{aligned}$$

**Example: Second derivatives**

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

