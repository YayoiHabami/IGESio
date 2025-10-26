# Ruled Surface (Type 118)

Defined in [surfaces/ruled_surface.h](./../../../include/igesio/entities/surfaces/ruled_surface.h)

## Table of Contents

- [Table of Contents](#table-of-contents)
- [Parameters](#parameters)
  - [Specific Parameters](#specific-parameters)
  - [Surface Definition](#surface-definition)
    - [Surface $S(u, v)$](#surface-su-v)
    - [Partial Derivatives $S\_u, S\_v, S\_{uu}, S\_{uv}, S\_{vv}$](#partial-derivatives-s_u-s_v-s_uu-s_uv-s_vv)
- [Appendix](#appendix)
  - [Derivation of Partial Derivatives](#derivation-of-partial-derivatives)

## Parameters

### Specific Parameters

| Member Function | Description |
|---|---|
| `GetCurve1()` <br> `SetCurve1(curve)` <br> (`std::shared_ptr<const ICurve>`) | Pointer to the first curve $C_1(t)$ |
| `GetCurve2()` <br> `SetCurve2(curve)` <br> (`std::shared_ptr<const ICurve>`) | Pointer to the second curve $C_2(s)$ |
| `IsReversed()` <br> `SetReversed(reversed)` <br> (`bool`) | Whether to reverse the parameter range of the two curves ($\text{DIR-FLAG}$) |
| `IsDevelopable()` <br> `SetDevelopable(developable)` <br> (`bool`) | Whether the surface is developable ($\text{DEV-FLAG}$) |

> Note: The $\text{DEV-FLAG}$ is currently ignored in the implementation. When reading from an IGES file, the flag is preserved, but when creating a new surface, it is always set to `false`.

### Surface Definition

#### Surface $S(u, v)$

A ruled surface is defined by connecting two curves, $C_1(t)\ (t \in [t_{\text{min}}, t_{\text{max}}])$ and $C_2(s)\ (s \in [s_{\text{min}}, s_{\text{max}}])$, with straight lines. The parametric surface $S(u, v)$ is given by:

$$S(u, v) = (1 - v) C_1(t) + v C_2(s), \quad u,v \in [0, 1]$$

The parameters $t$ and $s$ of $C_1$ and $C_2$ are mapped from $u$ as follows:

$$\begin{aligned}
    t &= t_{\text{min}} + u (t_{\text{max}} - t_{\text{min}}) \\\
    s &= \begin{cases}
        s_{\text{min}} + u (s_{\text{max}} - s_{\text{min}}), & \text{if } \text{DIR-FLAG} = false \\\
        s_{\text{max}} - u (s_{\text{max}} - s_{\text{min}}), & \text{if } \text{DIR-FLAG} = true
    \end{cases}
\end{aligned}$$

> According to IGES 5.3, there are two ways to map the parameter ranges: using the original parameter ranges of the curves ($t_{\text{min}}, t_{\text{max}}, s_{\text{min}}, s_{\text{max}}$) (form=1), or using ranges recalculated based on the curve lengths (form=0). Currently, only form 1 is implemented.

#### Partial Derivatives $S_u, S_v, S_{uu}, S_{uv}, S_{vv}$

The partial derivatives of the ruled surface $S_u, S_v, S_{uu}, S_{uv}, S_{vv}$ are defined as follows. For detailed derivations, see [Appendix - Derivation of Partial Derivatives](#derivation-of-partial-derivatives).

**First-order derivatives:**

$$\begin{aligned}
    S_u(u, v) &= (1 - v) C_1'(t) (\Delta t) + v C_2'(s) \sigma (\Delta s) \\\
    S_v(u, v) &= -C_1(t) + C_2(s)
\end{aligned}$$

**Second-order derivatives:**

$$\begin{aligned}
    S_{uu}(u, v) &= (1 - v) C_1''(t) (\Delta t)^2 + v C_2''(s) (\Delta s)^2 \\\
    S_{uv}(u, v) &= -C_1'(t) (\Delta t) + C_2'(s) \sigma (\Delta s) \\\
    S_{vv}(u, v) &= 0
\end{aligned}$$

Here, $\Delta t, \Delta s$, and $\sigma$ are defined as:

$$\Delta t = t_{\text{max}} - t_{\text{min}}, \quad \Delta s = s_{\text{max}} - s_{\text{min}}$$

$$\sigma = \begin{cases}
    1, & \text{if } \text{DIR-FLAG} = false \\\
    -1, & \text{if } \text{DIR-FLAG} = true
\end{cases}$$

For geometric properties that can be computed from $S(u,v)$ and its derivatives, see [geometric_properties.md](./../geometric_properties.md#geometric-properties-of-surfaces).

## Appendix

### Derivation of Partial Derivatives

Before defining the partial derivatives of the ruled surface, we introduce the following constants:

$$\Delta t = t_{\text{max}} - t_{\text{min}}, \quad \Delta s = s_{\text{max}} - s_{\text{min}}$$

$$\sigma = \begin{cases}
    1, & \text{if } \text{DIR-FLAG} = false \\\
    -1, & \text{if } \text{DIR-FLAG} = true
\end{cases}$$

With these, the derivatives of $t = t(u)$ and $s = s(u)$ with respect to $u$ are:

$$t(u) = t_{\text{min}} + u \Delta t, \Longrightarrow \frac{dt}{du} = \Delta t, \quad \frac{d^2 t}{du^2} = 0$$

$$s(u) = s_{\text{min}} + \sigma u \Delta s, \Longrightarrow \frac{ds}{du} = \sigma \Delta s, \quad \frac{d^2 s}{du^2} = 0$$

Let $C_1'(t), C_1''(t), C_2'(s), C_2''(s)$ denote the first and second derivatives of the curves with respect to $t$ and $s$. The partial derivatives of the ruled surface $S(u, v)$ are then:

**First-order derivatives:**

$$\begin{aligned}
    S_u(u, v) &= \frac{\partial}{\partial u} \left((1 - v) C_1(t(u)) + v C_2(s(u))\right) \\\
    &= (1 - v) \frac{d C_1}{d t} \frac{d t}{d u} + v \frac{d C_2}{d s} \frac{d s}{d u} \\\
    &= (1 - v) C_1'(t) \Delta t + v C_2'(s) \sigma \Delta s \\\
    &\quad \\\
    S_v(u, v) &= \frac{\partial}{\partial v} \left((1 - v) C_1(t(u)) + v C_2(s(u))\right) \\\
    &= -C_1(t) + C_2(s)
\end{aligned}$$

**Second-order derivatives:**

$$\begin{aligned}
    S_{uu}(u, v) &= \frac{\partial}{\partial u} \left((1 - v) C_1'(t) \Delta t + v C_2'(s) \sigma \Delta s\right) \\\
    &= (1 - v) \frac{d C_1'}{d t} \frac{d t}{d u} \Delta t + v \frac{d C_2'}{d s} \frac{d s}{d u} \sigma \Delta s \\\
    &= (1 - v) C_1''(t) (\Delta t)^2 + v C_2''(s) (\Delta s)^2 \\\
    &\quad \\\
    S_{uv}(u, v) &= \frac{\partial}{\partial v} \left((1 - v) C_1'(t) \Delta t + v C_2'(s) \sigma \Delta s\right) \\\
    &= -C_1'(t) \Delta t + C_2'(s) \sigma \Delta s \\\
    &\quad \\\
    S_{vv}(u, v) &= \frac{\partial}{\partial v} \left(-C_1(t) + C_2(s)\right) \\\
    &= 0
\end{aligned}$$

**Higher-order derivatives:**

In general, the $(n_u, n_v)$-th partial derivative of the ruled surface $S(u, v)$ is given by:

$$S^{(u^{n_u}, v^{n_v})}(u, v) = \begin{cases}
    (1 - v) C_1^{(n_u)}(t) (\Delta t)^{n_u} + v C_2^{(n_u)}(s) (\sigma \Delta s)^{n_u}, & n_v = 0 \\\
    -C_1^{(n_u)}(t) (\Delta t)^{n_u} + C_2^{(n_u)}(s) (\sigma \Delta s)^{n_u}, & n_v = 1 \\\
    0, & n_v \geq 2
\end{cases}$$
