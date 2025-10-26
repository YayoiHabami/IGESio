# Tabulated Cylinder (Type 122)

Defined in [surfaces/tabulated_cylinder.h](./../../../include/igesio/entities/surfaces/tabulated_cylinder.h)

## Table of Contents

- [Table of Contents](#table-of-contents)
- [Parameters](#parameters)
  - [Specific Parameters](#specific-parameters)
  - [Surface Definition](#surface-definition)
    - [Surface $S(u, v)$](#surface-su-v)
    - [Partial Derivatives $S\_u, S\_v, S\_{uu}, S\_{uv}, S\_{vv}$](#partial-derivatives-s_u-s_v-s_uu-s_uv-s_vv)
- [Appendix](#appendix)
  - [Derivation of the Surface Equation](#derivation-of-the-surface-equation)
  - [Derivation of Partial Derivatives](#derivation-of-partial-derivatives)

## Parameters

### Specific Parameters

| Member Function | Description |
|---|---|
| `GetDirectrix()` <br> `SetDirectrix(curve)` <br> (`std::shared_ptr<const ICurve>`) | Pointer to the directrix curve $C(t)$ |
| `GetDirection()` <br> `SetDirection(vec, length)` <br> (`Vector3d`) | Generator direction vector $D$ |
| `GetLocationVector()` <br> `SetLocationVector(vec)` <br> (`Vector3d`) | Location vector $P_0$ |

### Surface Definition

#### Surface $S(u, v)$

The Tabulated Cylinder (Type 122) entity defines a surface by translating a directrix curve $C(t)\ (t \in [t_{\text{min}}, t_{\text{max}}])$ along a generator direction vector $D$. The parametric surface $S(u, v)$ is given by:

$$S(u, v) = C(t) + v D, \quad u \in [0, 1], \quad v \in [0, 1]$$

Here, the parameter $t$ of the directrix curve $C(t)$ is mapped from $u$ as follows:

$$t = t_{\text{min}} + u (t_{\text{max}} - t_{\text{min}})$$

#### Partial Derivatives $S_u, S_v, S_{uu}, S_{uv}, S_{vv}$

The partial derivatives $S_u, S_v, S_{uu}, S_{uv}, S_{vv}$ of the Tabulated Cylinder are defined as follows. For detailed derivations, see [Appendix - Derivation of Partial Derivatives](#derivation-of-partial-derivatives).

**First-order partial derivatives:**

$$\begin{aligned}
    S_u(u, v) &= C'(t) (\Delta t) \\\
    S_v(u, v) &= D
\end{aligned}$$

**Second-order partial derivatives:**

$$\begin{aligned}
    S_{uu}(u, v) &= C''(t) (\Delta t)^2 \\\
    S_{uv}(u, v) &= 0 \\\
    S_{vv}(u, v) &= 0
\end{aligned}$$

## Appendix

### Derivation of the Surface Equation

The surface equation for the Tabulated Cylinder (Type 122) entity is derived by translating the directrix curve $C(t)$ along the generator direction vector $D$. There are two ways to define the generator direction vector $D$:

(1) Defining $D$ as the vector from the start point of the curve $C(t_{\text{min}})$ to the location vector $P_0$:

This is the standard method specified in IGES 5.3, and only a pointer to the curve $C(t)$ and the location vector $P_0$ are stored in the IGES file. You can set this using `SetLocationVector(vec)`, and the generator direction vector $D$ is calculated as:

$$D = P_0 - C(t_{\text{min}})$$

(2) Directly specifying the generator direction vector $D$:

In this library, you can also directly specify the generator direction vector $D$ using the `SetDirection(vec, length)` function. The `length` argument specifies the magnitude of the generator direction vector, and the `vec` argument specifies its direction. Note that `vec` is not normalized; the generator direction vector $D$ is given by `length * vec`.

Using either of these methods to define $D$, the surface $S(u, v)$ of the Tabulated Cylinder is expressed as:

$$S(u, v) = C(t) + v D, \quad u \in [0, 1], \quad v \in [0, 1]$$

Here, the parameter $t$ of the directrix curve $C(t)$ is mapped from $u$ as follows:

$$t = t_{\text{min}} + u (t_{\text{max}} - t_{\text{min}})$$

### Derivation of Partial Derivatives

Before deriving the partial derivatives, we define the following notation:

$$\Delta t = t_{\text{max}} - t_{\text{min}}$$

Using this, the partial derivatives of the Tabulated Cylinder surface $S(u, v)$ are derived as follows.

**First-order partial derivatives:**

$$\begin{aligned}
    S_u(u, v) &= \frac{\partial}{\partial u} (C(t) + v D) \\\
    &= \frac{dC}{dt} \frac{dt}{du} + 0 \\\
    &= C'(t) (\Delta t) \\\
    &\quad \\\
    S_v(u, v) &= \frac{\partial}{\partial v} (C(t) + v D) \\\
    &= D
\end{aligned}$$

**Second-order partial derivatives:**

$$\begin{aligned}
    S_{uu}(u, v) &= \frac{\partial}{\partial u} S_u(u, v) \\\
    &= C''(t) (\Delta t)^2 \\\
    &\quad \\\
    S_{uv}(u, v) &= \frac{\partial}{\partial v} S_u(u, v) \\\
    &= 0 \\\
    &\quad \\\
    S_{vv}(u, v) &= \frac{\partial}{\partial v} S_v(u, v) \\\
    &= 0
\end{aligned}$$

For more information on geometric properties that can be computed from the surface $S(u,v)$ and its partial derivatives, see [geometric_properties.md](./../geometric_properties.md#geometric-properties-of-surfaces).

**n-th Order Partial Derivatives:**

The $n_u$-th and $n_v$-th order partial derivatives of the parametric surface $S(u,v)$ with respect to $u$ and $v$ are given by:

$$S^{(n_u, n_v)}(u, v) = \begin{cases}
    C(t) + v D, & n_u = 0, n_v = 0 \\\
    C^{(n_u)}(t) (\Delta t)^{n_u}, & n_v = 0 \\\
    D, & n_u = 0, n_v = 1 \\\
    0, & \text{otherwise}
\end{cases}$$
