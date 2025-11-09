# Curve on a Parametric Surface (Type 142)

Defined at [curves/curve_on_a_parametric_surface.h](./../../../include/igesio/entities/curves/curve_on_a_parametric_surface.h)

## Table of Contents

- [Table of Contents](#table-of-contents)
- [Parameters](#parameters)
  - [Entity-Specific Parameters](#entity-specific-parameters)
    - [`CurveCreationType`](#curvecreationtype)
    - [`PreferredRepresentation`](#preferredrepresentation)
  - [Curve Definition](#curve-definition)
    - [Curve $C(t)$](#curve-ct)
    - [Derivatives $C'(t), C''(t)$](#derivatives-ct-ct)

## Parameters

### Entity-Specific Parameters

| Member Functions | Description |
|---|---|
| `GetSurface()` <br> `SetSurface(surface)` <br> (`std::shared_ptr<ICurve>`) | Get/set the surface entity $S(u,v)$ |
| `GetBaseCurve()` <br> (`std::shared_ptr<ICurve>`) | Get the base curve $B(t)$ on the surface |
| `GetCurve()` <br> (`std::shared_ptr<ICurve>`) | Get the curve $C(t)$ on the surface |
| `SetCurves(base, curve)` <br> (`std::shared_ptr<ICurve>`) | Set the base curve $B(t)$ and the curve $C(t)$ on the surface <br> If `curve` is not specified, it is automatically generated internally and returned |
| `GetCreationType()` <br> `SetCreationType(type)` <br> (`CurveCreationType`) | Curve creation method <br> Default is `kUnspecified` |
| `GetPreferredRepresentation()` <br> `SetPreferredRepresentation(pref)` <br> (`PreferredRepresentation`) | Preferred representation in the sending system <br> Default is `kUnspecified` |

#### `CurveCreationType`

An enumeration type indicating how the curve on the parametric surface was created. It corresponds to the first integer value (`CRTN`) in the Parameter Data section.

| Value | Enumerator | Description |
|---|---|---|
| 0 | `kUnspecified` | Unspecified |
| 1 | `kProjection` | Projection of a given curve onto the surface |
| 2 | `kIntersection` | Intersection curve of two surfaces |
| 3 | `kIsoparametric` | Isoparametric curve of the surface (u-parametric curve or v-parametric curve) |

#### `PreferredRepresentation`

An enumeration type indicating the preferred representation in the sending system. It corresponds to the fifth integer value (`PREF`) in the Parameter Data section.

| Value | Enumerator | Description |
|---|---|---|
| 0 | `kUnspecified` | Unspecified |
| 1 | `kSofB` | S(B(t)) is preferred |
| 2 | `kC` | C(t) is preferred |
| 3 | `kEquallyPreferred` | C(t) and S(B(t)) are equally preferred |

### Curve Definition

#### Curve $C(t)$

The curve entity on a parametric surface represents a parametric curve $C(t)$ defined on a parametric surface $S(u,v)$. The specific definition is as follows.

Let the surface $S(u,v)$ and its domain $D$ be given as follows. As defined below, the domain $D$ is a rectangular region defined by the minimum and maximum values of parameters $u$ and $v$.

$$\begin{aligned}
    S(u, v) &= \begin{bmatrix} x(u,v) \\\ y(u,v) \\\ z(u,v) \end{bmatrix}, \\
    D &= \lbrace (u,v) \mid u_{\text{min}} \leq u \leq u_{\text{max}}, v_{\text{min}} \leq v \leq v_{\text{max}} \rbrace
\end{aligned}$$

Also, let a parametric curve $B(t)$ defined on $D$ be given as follows. Since the curve $B$ is located on the two-dimensional space $D$, which is the domain of the surface $S$, it must be a curve defined on a plane. In this library, the $x$ and $y$ components of the curve specified by `SetCurves(B)` are treated as the $u$ and $v$ components, respectively.

$$B(t) = \begin{bmatrix} u(t) \\\ v(t) \end{bmatrix}, \quad t \in [t_{\text{min}}, t_{\text{max}}]$$

Then, the parametric curve $C(t)$ represented by the curve entity on the parametric surface is defined as follows.

$$\begin{aligned}
    C(t) &= S \circ B(t) \\
    &= S(u(t), v(t)) \\
    &= \begin{bmatrix} x(u(t), v(t)) \\\ y(u(t), v(t)) \\\ z(u(t), v(t)) \end{bmatrix}, \quad t \in [t_{\text{min}}, t_{\text{max}}]
\end{aligned}$$

#### Derivatives $C'(t), C''(t)$

Define the partial derivatives of the surface $S(u, v)$ and the derivatives of the curve $B(t)$ as follows.

$$S_u := \frac{\partial S}{\partial u}, \quad S_v := \frac{\partial S}{\partial v}, \quad S_{uu} := \frac{\partial^2 S}{\partial u^2}, \quad S_{uv} := \frac{\partial^2 S}{\partial u \partial v}, \quad S_{vv} := \frac{\partial^2 S}{\partial v^2}$$

$$B'(t) := \frac{dB}{dt} = \begin{bmatrix} u'(t) \\\ v'(t) \end{bmatrix}, \quad B''(t) := \frac{d^2B}{dt^2} = \begin{bmatrix} u''(t) \\\ v''(t) \end{bmatrix}$$

Then, the first and second derivatives of the curve $C(t)$ are defined as follows.

**First Derivative**

$$\begin{aligned}
    \frac{d}{dt} C(t) &= \frac{d}{dt} S(u(t), v(t)) \\
    &= \frac{\partial S}{\partial u} \frac{du}{dt} + \frac{\partial S}{\partial v} \frac{dv}{dt} \\
    &= S_u \cdot u' + S_v \cdot v'
\end{aligned}$$

**Second Derivative**

$$\begin{aligned}
    \frac{d^2}{dt^2} C(t) &= \frac{d}{dt} \left( S_u \cdot u' + S_v \cdot v' \right) \\
    &= S_{uu} \cdot (u')^2 + 2 S_{uv} \cdot u' v' + S_{vv} \cdot (v')^2 + S_u \cdot u'' + S_v \cdot v''
\end{aligned}$$

