# Geometric Properties of Entities

This section describes various geometric properties that can be calculated for geometry-related entities (curves and surfaces) defined in this library. These properties are mainly based on fundamental concepts of differential geometry and play an important role in analyzing and understanding the shape of curves and surfaces.

This section also describes the set of functions provided for calculating and obtaining various geometric properties programmatically. These functions are implemented as member functions of each entity class (derived from `ICurve` and `ISurface`). Users can easily obtain geometric properties by calling these functions. For implementation examples, see [examples/geometric_properties](./../../examples/geometric_properties.cpp).

## Table of Contents

- [Table of Contents](#table-of-contents)
- [Geometric Properties of Curves](#geometric-properties-of-curves)
  - [Curve $C(u)$](#curve-cu)
  - [Derivative (Curve)](#derivative-curve)
  - [Tangent Vector (Curve)](#tangent-vector-curve)
  - [Curvature (Curve)](#curvature-curve)
  - [Length (Curve)](#length-curve)
  - [Frenet Frame](#frenet-frame)
- [Geometric Properties of Surfaces](#geometric-properties-of-surfaces)
  - [Surface $S(u,v)$](#surface-suv)
  - [Partial Derivative (Surface)](#partial-derivative-surface)
  - [Tangent and Normal Vectors (Surface)](#tangent-and-normal-vectors-surface)
  - [First and Second Fundamental Forms](#first-and-second-fundamental-forms)
  - [Area (Surface)](#area-surface)
  - [Curvature (Surface)](#curvature-surface)

## Geometric Properties of Curves

Consider a parametric curve:

$$C(u) \in \mathbb{R}^3 \quad (u \in [u_{\text{min}}, u_{\text{max}}])$$

The curve classes defined in this library (derived from `ICurve`) represent curves defined by a single parameter $u$.

The first and second derivatives of this function are given by:

- Derivative: $C'(u) := \frac{dC}{du}$
- Second Derivative: $C''(u) := \frac{d^2C}{du^2}$

These are used to define and calculate the following geometric properties:

| Name | Definition | Description / Member Function |
|:-:|:-:|:-|
| [**Curve**](#curve-cu) | $C(u) \in \mathbb{R}^3$ | Parametric curve <br> `TryGetPointAt(u)` |
| [**Derivative**](#derivative-curve) | $C'(u) = \frac{dC}{du}$ <br> $C''(u) = \frac{d^2C}{du^2}$ | Vectors representing the rate of change of the curve <br> `TryGetDerivatives(u, n_deriv)` |
| [**Unit Tangent Vector**](#tangent-vector-curve) | $T(u) = \frac{C'(u)}{\|C'(u)\|}$ | Tangent direction of the curve <br> `TryGetTangentAt(u)` |
| [**Curvature**](#curvature-curve) | $\kappa(u) = \frac{\|C'(u) \times C''(u)\|}{\|C'(u)\|^3}$ | Measure of how much the curve bends <br> `GetCurvature(u)` |
| [**Length**](#length-curve) | $L = \int_{u_{\text{start}}}^{u_{\text{end}}} \|C'(t)\| dt$ | Distance between two points on the curve <br> `Length()` <br> `Length(u_start, u_end)` |
| [**Unit Normal Vector**](#frenet-frame) | $N(u) = \frac{\frac{dT(u)}{du}}{\|\frac{dT(u)}{du}\|}$ <br> ( $=\frac{C''(u)\|C'(u)\|^2 - C'(u)(C'(u)\cdot C''(u))}{\|C'(u)\|^3}$ ) | Direction in which the curve bends <br> `TryGetNormalAt(u)` |
| [**Binormal Vector**](#frenet-frame) | $B(u) = T(u) \times N(u)$ | Direction of the curve's torsion <br> `TryGetBinormalAt(u)` |

> Torsion is omitted here because it requires the third derivative.

> Some curve classes, such as [Composite Curve (type 102)](./entities_ja.md#compositecurve-type-102) and [Copious Data (type 106)](./entities_ja.md#copious-data-type-106) entities, may have non-smooth points. Derivatives and second derivatives are generally defined even at such points. However, [Copious Data (type 106, forms 1-3)](./entities_ja.md#copiousdata-type-106-forms-1-3) is defined as a completely discontinuous curve (point cloud), so derivatives are not defined over the entire region.

### Curve $C(u)$

In IGES 5.3, curves (classes derived from `ICurve`) are generally represented as parametric curves. This means they are sets of points in three-dimensional space $\mathbb{R}^3$ defined by a parameter $u$. If the valid range of $u$ is $[u_{\text{min}}, u_{\text{max}}]$, the curve $C(u)$ is expressed as:

$$C(u) \in \mathbb{R}^3 \quad (u \in [u_{\text{min}}, u_{\text{max}}])$$

**Code Example**

The following example demonstrates how to obtain the parameter range of an `ICurve`-derived instance `curve`, and how to compute the position vector of the curve at a specific parameter value $u$ within that range. In subsequent examples, abbreviations such as `i_ent` and `Vector3d` are used as shown here.

The function `TryGetPointAt` retrieves the point on the curve at the specified parameter $u$. This function returns the position vector if the computation is successful, or `std::nullopt` if it fails (for example, if $u$ is outside the domain of the curve).

```cpp
#include <iostream>
#include <memory>
#include <igesio/entities/curves/rational_b_spline_curve.h>

namespace i_ent = igesio::entities;
using igesio::Vector3d;

// Create a Rational B-Spline Curve instance
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

// Get the parameter range of the curve
auto [u_start, u_end] = curve->GetParameterRange();
std::cout << "Parameter range: [" << u_start << ", " << u_end << "]" << std::endl;

/// Compute the position vector of the curve at a specific parameter value
double u = (u_start + u_end) / 2.0;
auto point_opt = curve->TryGetPointAt(u);
if (point_opt) {
    Vector3d point = *point_opt;
    std::cout << "Point at u = " << u << ": " << point << std::endl;
} else {
    std::cout << "Failed to compute point at u = " << u << std::endl;
}
```

If the code runs successfully, it will produce output similar to the following. Each vector is defined as a column vector and is displayed in the format `((x), (y), (z))`. You can also display it as a row vector by outputting `point.transpose()`.

```
Parameter range: [0, 1]
Point at u = 0.5: ((0.9375), (1.5), (1.6875))
```

### Derivative (Curve)

For a parametric curve $C(u)$, the **derivatives** at $u$ are defined as follows:

$$\begin{aligned}
        &C'(u) = \frac{dC}{du}, \quad \left(C'(u) \in \mathbb{R}^3\right) \\\
        &C''(u) = \frac{d^2C}{du^2}, \quad \left(C''(u) \in \mathbb{R}^3\right)
\end{aligned}$$

These derivatives play an important role in analyzing the shape of the curve. For example, $C'(u)$ represents the tangent vector on the curve, and normalizing it gives the unit tangent vector $T(u)$. The second derivative $C''(u)$ is used to calculate the curvature of the curve.

**Code Example**

Below is an example of how to compute the 0th to 2nd derivatives at a specific parameter value $u$ for an instance `curve` derived from `ICurve`. The `TryGetDerivatives` function returns the derivatives at the specified parameter $u$ as a `CurveDerivatives` object or `std::nullopt` if the computation fails.

```cpp
// Calculate 0th to 2nd derivatives at a specific parameter value u
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

Output:

```
Derivative C^0(u): ((0.9375), (1.5), (1.6875))
Derivative C^1(u): ((10.125), (-1.5), (-1.875))
Derivative C^2(u): ((-7.5), (-12), (-13.5))
```

### Tangent Vector (Curve)

The **tangent vector** at a point on the curve $C(u)$ is given by the derivative $C'(u)$. Normalizing this vector, which represents the tangent line to the curve, yields the **unit tangent vector** $T(u)$:

$$T(u) = \frac{C'(u)}{\|C'(u)\|}$$

$$\left(T(u) \in \mathbb{R}^3, \; \|T(u)\| = 1\right)$$

**Code Example**

```cpp
// Calculate the unit tangent vector at a specific parameter value u
auto tangent_opt = curve->TryGetTangentAt(u);
if (tangent_opt) {
    Vector3d tangent = *tangent_opt;
    std::cout << "Tangent T(u): " << tangent << std::endl;
} else {
    std::cout << "Failed to compute tangent at u = " << u << std::endl;
}
```

Output:

```
Tangent T(u): ((0.973012), (-0.14415), (-0.180187))
```

### Curvature (Curve)

The **curvature** $\kappa(u) \geq 0$ of a curve is a measure of how much the curve bends at a given point, defined by the following equation:

$$\kappa(u) = \frac{\|C'(u) \times C''(u)\|}{\|C'(u)\|^3}$$

This value is interpreted as follows: the larger the curvature, the more sharply the curve is bending.

- $\kappa = 0$: The curve is a straight line.
- $\kappa > 0$: The curve is curved.

The **radius of curvature** $\rho(u) = \frac{1}{\kappa(u)}$, which is the reciprocal of the curvature, is another measure of the curve's bending at that point. The larger the radius of curvature, the more gently the curve is bending.

**Code Example**

The following is an example of calculating the curvature at a specific parameter value $u$ using the `GetCurvature` function. This function returns the curvature directly, and returns `-1` if the calculation fails.

```cpp
// Calculate the curvature at a specific parameter value u
double curvature = curve->GetCurvature(u);

if (curvature >= 0.0) {
    std::cout << "Curvature kappa(u): " << curvature << std::endl;
} else {
    std::cout << "Failed to compute curvature at u = " << u << std::endl;
}
```

Output:

```
Curvature kappa(u): 0.178283
```

### Length (Curve)

The length of a curve can be calculated using the derivative. The length $L$ of the curve $C(u)$ in the parameter interval $[u_{\text{start}}, u_{\text{end}}]$ is expressed as:

$$L = \int_{u_{\text{start}}}^{u_{\text{end}}} \|C'(t)\| dt$$

**Code Example**

Below is an example of calculating the total length of a curve using the `Length` function. This function directly returns the length of the curve. Normally, it returns a value greater than or equal to zero, but if the calculation fails or the curve is discontinuous ([Copious Data (type 106, forms 1-3)](./curves/106_copious_data.md#copiousdatatype)), it may return zero.

```cpp
// Calculate the total length of the curve
double length = curve->Length();
std::cout << "Curve length: " << length << std::endl;

// Calculate the length in the specified parameter range u_start to u_end
length = curve->Length(0.25, 0.75);
std::cout << "Curve length from u=0.25 to u=0.75: " << length << std::endl;
```

Output:

```
Curve length: 14.0092
Curve length from u=0.25 to u=0.75: 5.14887
```

### Frenet Frame

The **Frenet frame** is used to describe the local geometric properties of a curve. This is an orthogonal coordinate system defined at each point on the curve, consisting of the unit tangent vector, **unit normal vector**, and **unit binormal vector**. As can be seen from the equation for the unit normal vector below, this coordinate system is defined only at points where the curvature $\kappa(u)$ is non-zero.

The **unit normal vector** $N(u)$ represents the direction of the rate of change of the tangent vector. It is defined as:

$$N(u) = \frac{\frac{dT(u)}{du}}{\|\frac{dT(u)}{du}\|} = \frac{C''(u)\|C'(u)\|^2 - C'(u)(C'(u)\cdot C''(u))}{\|C'(u)\|^3}$$

$$\left(N(u) \in \mathbb{R}^3, \; \|N(u)\| = 1\right)$$

The **unit binormal vector** $B(u)$ is defined by the cross product of the tangent vector and the normal vector:

$$B(u) = T(u) \times N(u)$$

$$\left(B(u) \in \mathbb{R}^3, \; \|B(u)\| = 1\right)$$

**Code Example**

```cpp
// Calculate the Frenet frame at a specific parameter value u
// Unit normal vector N(u)
auto normal_opt = curve->TryGetNormalAt(u);
// Unit binormal vector B(u)
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

Output:

```
Normal N(u): ((-0.230481), (-0.645023), (-0.728577))
Binormal B(u): ((-0.0112007), (0.750444), (-0.660839))
```

## Geometric Properties of Surfaces

Consider a parametric surface:

$$S(u,v) \in \mathbb{R}^3 \quad (u \in [u_{\text{min}}, u_{\text{max}}], v \in [v_{\text{min}}, v_{\text{max}}])$$

The surface classes defined in this library (derived from `ISurface`) represent surfaces defined by two parameters $u$ and $v$. Surface classes are generally defined as smooth shapes, except on the boundaries of the surface (the outer periphery of the surface, or the inner holes of Trimmed Surface (type 144)).

The partial derivatives and second partial derivatives of this function are:

- Partial Derivatives: $S_u(u,v) := \frac{\partial S}{\partial u}, \quad S_v(u,v) := \frac{\partial S}{\partial v}$
- Second Partial Derivatives: $S_{uu}(u,v) := \frac{\partial^2 S}{\partial u^2}, \quad S_{uv}(u,v) := \frac{\partial^2 S}{\partial u \partial v}, \quad S_{vv}(u,v) := \frac{\partial^2 S}{\partial v^2}$

These are used to define and calculate the following geometric properties:

| Name | Definition | Description / Member Function |
|:-:|:-:|:-|
| [**Surface**](#surface-suv) | $S(u,v) \in \mathbb{R}^3$ | Parametric surface <br> `TryGetPointAt(u, v)` |
| [**Partial Derivatives**](#partial-derivative-surface) | $S^{(n,m)}(u,v) = \frac{\partial^{n+m} S}{\partial u^n \partial v^m}$ <br> $(n,m = 0,1,2)$ | Vectors representing the rate of change of the surface <br> `TryGetPartialDerivatives(u, v, n_deriv_u, n_deriv_v)` |
| [**Unit Tangent Vector**](#tangent-and-normal-vectors-surface) | $T_u(u,v) = \frac{S_u(u,v)}{\|S_u(u,v)\|}$ <br> $T_v(u,v) = \frac{S_v(u,v)}{\|S_v(u,v)\|}$ | Tangent direction to the surface in the u and v directions <br> `TryGetTangentAt(u, v)` |
| [**Unit Normal Vector**](#tangent-and-normal-vectors-surface) | $N(u,v) = T_u(u,v) \times T_v(u,v)$ | Normal direction of the surface <br> `TryGetNormalAt(u, v)` |
| [**First Fundamental Form**](#first-and-second-fundamental-forms) | $E(u,v) = S_u(u,v) \cdot S_u(u,v)$ <br> $F(u,v) = S_u(u,v) \cdot S_v(u,v)$ <br> $G(u,v) = S_v(u,v) \cdot S_v(u,v)$ | Represents the intrinsic properties of the surface |
| [**Area**](#area-surface) | $A = \int_{v_{\text{min}}}^{v_{\text{max}}} \int_{u_{\text{min}}}^{u_{\text{max}}} \|S_u(u,v) \times S_v(u,v)\| du dv$ <br> $\left(\|S_u(u,v) \times S_v(u,v)\| = \sqrt{EG-F^2}\right)$ | Area of the rectangular region |
| [**Second Fundamental Form**](#first-and-second-fundamental-forms) | $L(u,v) = S_{uu}(u,v) \cdot N(u,v)$ <br> $M(u,v) = S_{uv}(u,v) \cdot N(u,v)$ <br> $N(u,v) = S_{vv}(u,v) \cdot N(u,v)$ | Represents the extrinsic properties of the surface |
| [**Gaussian Curvature**](#curvature-surface) | $K(u,v) = \frac{LN - M^2}{EG - F^2}$ | Overall curvature of the surface |
| [**Mean Curvature**](#curvature-surface) | $H(u,v) = \frac{GL - 2FM + EN}{2(EG - F^2)}$ | Average curvature of the surface |
| [**Principal Curvatures**](#curvature-surface) | $\kappa_1(u,v), \kappa_2(u,v)$ <br> $= H \pm \sqrt{H^2 - K}$ | Maximum and minimum curvatures of the surface |

### Surface $S(u,v)$

In IGES 5.3, surfaces (classes derived from `ISurface`) are generally represented as parametric surfaces. This means they are sets of points in three-dimensional space $\mathbb{R}^3$ defined by two parameters $u$ and $v$. If the valid range of $u$ is $[u_{\text{min}}, u_{\text{max}}]$ and the valid range of $v$ is $[v_{\text{min}}, v_{\text{max}}]$, the surface $S(u,v)$ is expressed as:

$$S(u,v) \in \mathbb{R}^3 \quad (u \in [u_{\text{min}}, u_{\text{max}}], v \in [v_{\text{min}}, v_{\text{max}}])$$

**Code Example**

The following example demonstrates how to obtain the parameter ranges $u$ and $v$ for an instance `surface` derived from `ISurface`, and how to compute the position vector of the surface at a specific point $(u, v)$ within those ranges. In subsequent examples, abbreviations such as `i_ent` and `Vector3d` are used as shown here. For details on parameter values, see [examples/geometric_properties.cpp](./../../examples/geometric_properties.cpp).

The function `TryGetPointAt` retrieves the point on the surface at the specified parameters $u, v$. This function returns the position vector if the computation is successful, or `std::nullopt` if it fails (for example, if $u, v$ are outside the domain of the surface).

```cpp
#include <iostream>
#include <memory>
#include <igesio/entities/curves/rational_b_spline_surface.h>

namespace i_ent = igesio::entities;
using igesio::Vector3d;

// Create a Rational B-Spline Surface instance
auto param = igesio::IGESParameterVector{
    5, 5,  // K1, K2 (Number of control points - 1 in U and V)
    3, 3,  // M1, M2 (Degree in U and V)
    false, false, true, false, false,         // PROP1-5
    0., 0., 0., 0., 1., 2., 3., 3., 3., 3.,   // Knot vector in U
    0., 0., 0., 0., 1., 2., 3., 3., 3., 3.,   // Knot vector in V
    1., 1., 1., 1., 1., 1.,                   // Weights W(0,0) to W(0,5)
    // ...
    1., 1., 1., 1., 1., 1.,                   // Weights W(5,0) to W(5,5)
    // Control points (36 points, each with x, y, z)
    -25., -25., -10.,  // Control point (0,0)
    // ...
    25., 25., -10.,    // Control point (5,5)
    0., 3., 0., 3.     // Parameter range in U and V
};
auto surface = std::make_shared<i_ent::RationalBSplineSurface>(param);

// Get the parameter ranges of the surface
auto [u_start, u_end] = surface->GetURange();
auto [v_start, v_end] = surface->GetVRange();
std::cout << "Parameter range U: [" << u_start << ", " << u_end << "]" << std::endl;
std::cout << "Parameter range V: [" << v_start << ", " << v_end << "]" << std::endl;

/// Compute the position vector of the surface at a specific (u, v)
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

Output:

```
Parameter range U: [0, 3]
Parameter range V: [0, 3]
Point at (u, v) = (1.5, 1.5): ((0), (0), (-7.42773))
```

### Partial Derivative (Surface)

For a parametric surface $S(u,v)$, the (**first-order**) **partial derivatives** at $(u, v)$ are defined as follows:

$$S_u(u,v) = \frac{\partial S}{\partial u}, \quad S_v(u,v) = \frac{\partial S}{\partial v}$$

$$\left(S_u(u,v), S_v(u,v) \in \mathbb{R}^3\right)$$

Based on the first-order partial derivatives, the **second-order partial derivatives** are defined as follows. Since the surfaces defined in IGES are all smooth, the mixed partial derivatives do not depend on the order (i.e., $S_{uv} = S_{vu}$).

$$S_{uu}(u,v) = \frac{\partial^2 S}{\partial u^2}, \quad S_{uv}(u,v) = \frac{\partial^2 S}{\partial u \partial v}, \quad S_{vv}(u,v) = \frac{\partial^2 S}{\partial v^2}$$

$$\left(S_{uu}(u,v), S_{uv}(u,v), S_{vv}(u,v) \in \mathbb{R}^3\right)$$

These partial derivatives play an important role in analyzing the shape of the surface. For example, $S_u$ and $S_v$ represent tangent vectors on the surface, and $N(u,v) = S_u(u,v) \times S_v(u,v)$ represents the normal vector. By using these vectors, the tangent plane and normal direction of the surface can be calculated. The second partial derivatives are used to calculate the curvature of the surface.

**Code Example**

Below is an example of how to compute the 0th to 2nd order partial derivatives at a specific parameter value $(u, v)$ for an instance `surface` derived from `ISurface`. The `TryGetDerivatives` function returns the partial derivatives up to order `n_deriv` at the specified parameters $(u, v)$ as a `SurfaceDerivatives` object or `std::nullopt` if the computation fails. The partial derivative $S^{(n,m)}$ can be accessed as `derivs(n, m)`, where $n + m \leq \text{n\_deriv}$.

```cpp
// Calculate 0th to 2nd order partial derivatives at a specific (u, v)
// S, S_u, S_v, S_uu, S_uv, S_vv
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

Output:

```
Derivative S^(0,0)(u,v): ((0), (0), (-7.42773))
Derivative S^(0,1)(u,v): ((0), (11.25), (0))
Derivative S^(0,2)(u,v): ((1.11022e-16), (0), (7.73438))
Derivative S^(1,0)(u,v): ((11.25), (0), (0.0351562))
Derivative S^(1,1)(u,v): ((0), (0), (0))
Derivative S^(2,0)(u,v): ((0), (0), (5.48438))
```

### Tangent and Normal Vectors (Surface)

The **tangent vectors** at a point on the surface $S(u,v)$ are given by the partial derivatives $S_u$ and $S_v$. These vectors represent the tangents to the surface in the u and v directions, respectively. Normalizing these tangent vectors yields the **unit tangent vectors** $T_u$ and $T_v$.

$$T_u(u,v) = \frac{S_u(u,v)}{\|S_u(u,v)\|}, \quad T_v(u,v) = \frac{S_v(u,v)}{\|S_v(u,v)\|}$$

$$\left(T_u(u,v), T_v(u,v) \in \mathbb{R}^3, \; \|T_u(u,v)\| = \|T_v(u,v)\| = 1\right)$$

The **normal vector** of the surface is obtained by the cross product of the tangent vectors. This yields the unit normal vector $N(u,v)$, which represents the normal direction at each point on the surface. In this library, when calculating the normal vector, the u-direction tangent vector is placed on the left side of the cross product, and the v-direction tangent vector is placed on the right side.

$$N(u,v) = T_u(u,v) \times T_v(u,v)$$

$$\left(N(u,v) \in \mathbb{R}^3, \; \|N(u,v)\| = 1\right)$$

**Code Example**

```cpp
// Calculate the tangent and normal vectors at a specific (u, v)
auto tangent_opt = surface->TryGetTangentAt(u, v);  // Get (T_u, T_v)
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

Output:

```
Tangent T_u(u,v): ((0.999995), (0), (0.00312498))
Tangent T_v(u,v): ((0), (1), (0))
Normal N(u,v): ((-0.00312498), (0), (0.999995))
```

### First and Second Fundamental Forms

The **first fundamental form** represents the intrinsic properties of the surface (length, angle, area, etc. on the surface) and is calculated from the first-order partial derivatives as follows:

$$E(u,v) = S_u(u,v) \cdot S_u(u,v), \quad F(u,v) = S_u(u,v) \cdot S_v(u,v), \quad G(u,v) = S_v(u,v) \cdot S_v(u,v)$$

From this, it is possible to calculate the metric properties of the surface. For example, the length for a minute movement on the surface $dS = S_u du + S_v dv$ is expressed as follows:

$$ds = \sqrt{E du^2 + 2F du dv + G dv^2}$$

The **second fundamental form** describes how the surface curves in three-dimensional space $\mathbb{R}^3$, which is an extrinsic property. It is defined using the second-order partial derivatives and the normal vector as follows:

$$L(u,v) = S_{uu}(u,v) \cdot N(u,v), \quad M(u,v) = S_{uv}(u,v) \cdot N(u,v), \quad N(u,v) = S_{vv}(u,v) \cdot N(u,v)$$

**Code Example**

```cpp
// Calculate the first and second fundamental forms at a specific (u, v)
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

Output:

```
First Fundamental Form (E, F, G): (126.564, 0, 126.562)
Second Fundamental Form (L, M, N): (5.48435, 0, 7.73434)
```

### Area (Surface)

The area of a surface can be calculated using the first fundamental form. The area $A$ of the surface $S(u, v)$ is expressed as a double integral over the parameter region $D$ as follows:

$$\begin{aligned}
        A &= \iint_D \sqrt{EG - F^2} \, du \, dv  \\\
        &= \int_{v_{\text{min}}}^{v_{\text{max}}} \int_{u_{\text{min}}}^{u_{\text{max}}} \sqrt{E(u,v)G(u,v) - F(u,v)^2} \, du \, dv
\end{aligned}$$

Here, $\sqrt{EG - F^2}$ represents the area element and means the area of a minute region on the surface. This equation is based on the idea of ​​considering the surface as a collection of minute parallelograms and finding the total area by adding up those areas. In this library, this double integral is approximately calculated using a function that performs numerical integration, such as the `Integrate` function in `igesio/numerics/integration.h`.

### Curvature (Surface)

The curvature of a surface is characterized by the Gaussian curvature $K$, which represents the overall curvature, and the mean curvature $H$, which represents the average curvature. Also, using these values, the principal curvatures $\kappa_1$ and $\kappa_2$ at that point can be calculated.

**Gaussian curvature** $K(u, v) \in \mathbb{R}$ is a measure of the overall curvature of the surface at that point, defined by the following equation:

$$K(u,v) = \frac{LN - M^2}{EG - F^2}$$

This value is interpreted as follows:

- $K > 0$: The surface is curved roundly in the same direction (like a sphere).
- $K < 0$: The surface is curved in different directions (like a saddle).
- $K = 0$: The surface is flat or curved in only one direction.

**Mean curvature** $H(u, v) \in \mathbb{R}$ is a measure of the average curvature of the surface at that point, defined by the following equation. This value is an external quantity and depends on how the surface is embedded in space.

$$H = \frac{GL - 2FM + EN}{2(EG - F^2)}$$

In particular, a surface with $H = 0$ is called a minimal surface, which corresponds to a shape that tries to minimize the area for a given boundary (such as a soap bubble film).

At a point $S(u, v)$ on the surface, the curvature of the cross section ( **normal curvature** ) when the surface is cut by a plane including the normal vector $N(u, v)$ changes depending on the cutting direction. The **principal curvatures** $\kappa_1, \kappa_2$ are the maximum and minimum values ​​that this normal curvature takes. Since there is a relationship of $K = \kappa_1 \kappa_2, H = \frac{\kappa_1 + \kappa_2}{2}$ between the Gaussian curvature and the mean curvature described above, the principal curvatures are calculated as follows:

$$\lambda^2 - 2H\lambda + K = 0$$

$$\therefore \kappa_1, \kappa_2 = H \pm \sqrt{H^2 - K}$$

These $\kappa_1$ and $\kappa_2$ play an important role in understanding the local shape of the surface. The shape of the surface can be classified as follows based on the signs of $k_1$ and $k_2$ <sup>[*1](http://homepages.inf.ed.ac.uk/rbf/BOOKS/FSTO/node28.html)</sup>.

| - | $\kappa_1 < 0$ | $\kappa_1 = 0$ | $\kappa_1 > 0$ |
|:-:|:--------------:|:--------------:|:--------------:|
| $\kappa_2 < 0$ | Concave Ellipsoid | Concave Cylinder | Hyperboloid |
| $\kappa_2 = 0$ | Concave Cylinder | Plane | Convex Cylinder |
| $\kappa_2 > 0$ | Hyperboloid | Convex Cylinder | Convex Ellipsoid |

**コード例**

```cpp
// Calculate the curvatures at a specific (u, v)
// Gaussian curvature K
auto gaussian_curvature_opt = surface->TryGetGaussianCurvature(u, v);
// Mean curvature H
auto mean_curvature_opt = surface->TryGetMeanCurvature(u, v);
// Principal curvatures (k1, k2)
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

Output:

```
Gaussian Curvature K(u,v): 0.0026481
Mean Curvature H(u,v): 0.0522218
Principal Curvatures (k1, k2): (0.0611108, 0.0433327)
```
