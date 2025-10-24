# Geometric Properties of Entities

This section describes various geometric properties that can be calculated for geometry-related entities (curves and surfaces) defined in this library. These properties are mainly based on fundamental concepts of differential geometry and play an important role in analyzing and understanding the shape of curves and surfaces.

## Table of Contents

- [Table of Contents](#table-of-contents)
- [Geometric Properties of Curves](#geometric-properties-of-curves)
  - [Derivative (Curve)](#derivative-curve)
  - [Tangent Vector (Curve)](#tangent-vector-curve)
  - [Curvature (Curve)](#curvature-curve)
  - [Length (Curve)](#length-curve)
  - [Frenet Frame](#frenet-frame)
- [Geometric Properties of Surfaces](#geometric-properties-of-surfaces)
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

| Name | Definition | Description |
|---|:---:|---|
| **Unit Tangent Vector** | $T(u) = \frac{C'(u)}{\|C'(u)\|}$ | Direction of the tangent to the curve |
| **Curvature** | $\kappa(u) = \frac{\|C'(u) \times C''(u)\|}{\|C'(u)\|^3}$ | Measure of how much the curve bends |
| **Length** | $L = \int_{u_{\text{start}}}^{u_{\text{end}}} \|C'(t)\| dt$ | Distance between two points on the curve |
| **Unit Normal Vector** | $N(u) = \frac{\frac{dT(u)}{du}}{\|\frac{dT(u)}{du}\|}$ <br> ( $=\frac{C''(u)\|C'(u)\|^2 - C'(u)(C'(u)\cdot C''(u))}{\|C'(u)\|^3}$ )| Direction in which the curve bends |
| **Binormal Vector** | $B(u) = T(u) \times N(u)$ | Direction of the curve's torsion |

> Torsion is omitted here because it requires the third derivative.

> Some curve classes, such as [Composite Curve (type 102)](./entities_ja.md#compositecurve-type-102) and [Copious Data (type 106)](./entities_ja.md#copious-data-type-106) entities, may have non-smooth points. Derivatives and second derivatives are generally defined even at such points. However, [Copious Data (type 106, forms 1-3)](./entities_ja.md#copiousdata-type-106-forms-1-3) is defined as a completely discontinuous curve (point cloud), so derivatives are not defined over the entire region.

### Derivative (Curve)

For a parametric curve $C(u)$, the **derivatives** at $u$ are defined as follows:

$$\begin{aligned}
        &C'(u) = \frac{dC}{du}, \quad \left(C'(u) \in \mathbb{R}^3\right) \\\
        &C''(u) = \frac{d^2C}{du^2}, \quad \left(C''(u) \in \mathbb{R}^3\right)
\end{aligned}$$

These derivatives play an important role in analyzing the shape of the curve. For example, $C'(u)$ represents the tangent vector on the curve, and normalizing it gives the unit tangent vector $T(u)$. The second derivative $C''(u)$ is used to calculate the curvature of the curve.

### Tangent Vector (Curve)

The **tangent vector** at a point on the curve $C(u)$ is given by the derivative $C'(u)$. Normalizing this vector, which represents the tangent line to the curve, yields the **unit tangent vector** $T(u)$:

$$T(u) = \frac{C'(u)}{\|C'(u)\|}$$

$$\left(T(u) \in \mathbb{R}^3, \; \|T(u)\| = 1\right)$$

### Curvature (Curve)

The **curvature** $\kappa(u) \geq 0$ of a curve is a measure of how much the curve bends at a given point, defined by the following equation:

$$\kappa(u) = \frac{\|C'(u) \times C''(u)\|}{\|C'(u)\|^3}$$

This value is interpreted as follows: the larger the curvature, the more sharply the curve is bending.

- $\kappa = 0$: The curve is a straight line.
- $\kappa > 0$: The curve is curved.

The **radius of curvature** $\rho(u) = \frac{1}{\kappa(u)}$, which is the reciprocal of the curvature, is another measure of the curve's bending at that point. The larger the radius of curvature, the more gently the curve is bending.

### Length (Curve)

The length of a curve can be calculated using the derivative. The length $L$ of the curve $C(u)$ in the parameter interval $[u_{\text{start}}, u_{\text{end}}]$ is expressed as:

$$L = \int_{u_{\text{start}}}^{u_{\text{end}}} \|C'(t)\| dt$$

### Frenet Frame

The **Frenet frame** is used to describe the local geometric properties of a curve. This is an orthogonal coordinate system defined at each point on the curve, consisting of the unit tangent vector, **unit normal vector**, and **unit binormal vector**. As can be seen from the equation for the unit normal vector below, this coordinate system is defined only at points where the curvature $\kappa(u)$ is non-zero.

The **unit normal vector** $N(u)$ represents the direction of the rate of change of the tangent vector. It is defined as:

$$N(u) = \frac{\frac{dT(u)}{du}}{\|\frac{dT(u)}{du}\|} = \frac{C''(u)\|C'(u)\|^2 - C'(u)(C'(u)\cdot C''(u))}{\|C'(u)\|^3}$$

$$\left(N(u) \in \mathbb{R}^3, \; \|N(u)\| = 1\right)$$

The **unit binormal vector** $B(u)$ is defined by the cross product of the tangent vector and the normal vector:

$$B(u) = T(u) \times N(u)$$

$$\left(B(u) \in \mathbb{R}^3, \; \|B(u)\| = 1\right)$$

## Geometric Properties of Surfaces

Consider a parametric surface:

$$S(u,v) \in \mathbb{R}^3 \quad (u \in [u_{\text{min}}, u_{\text{max}}], v \in [v_{\text{min}}, v_{\text{max}}])$$

The surface classes defined in this library (derived from `ISurface`) represent surfaces defined by two parameters $u$ and $v$. Surface classes are generally defined as smooth shapes, except on the boundaries of the surface (the outer periphery of the surface, or the inner holes of Trimmed Surface (type 144)).

The partial derivatives and second partial derivatives of this function are:

- Partial Derivatives: $S_u(u,v) := \frac{\partial S}{\partial u}, \quad S_v(u,v) := \frac{\partial S}{\partial v}$
- Second Partial Derivatives: $S_{uu}(u,v) := \frac{\partial^2 S}{\partial u^2}, \quad S_{uv}(u,v) := \frac{\partial^2 S}{\partial u \partial v}, \quad S_{vv}(u,v) := \frac{\partial^2 S}{\partial v^2}$

These are used to define and calculate the following geometric properties:

| Name | Definition | Description |
|---|:---:|---|
| **Unit Tangent Vector** | $T_u(u,v) = \frac{S_u(u,v)}{\|S_u(u,v)\|}$ <br> $T_v(u,v) = \frac{S_v(u,v)}{\|S_v(u,v)\|}$ | Tangent direction to the surface in the u and v directions |
| **Unit Normal Vector** | $N(u,v) = T_u(u,v) \times T_v(u,v)$ | Normal direction of the surface |
| **First Fundamental Form** | $E(u,v) = S_u(u,v) \cdot S_u(u,v)$ <br> $F(u,v) = S_u(u,v) \cdot S_v(u,v)$ <br> $G(u,v) = S_v(u,v) \cdot S_v(u,v)$ | Represents the intrinsic properties of the surface |
| **Second Fundamental Form** | $L(u,v) = S_{uu}(u,v) \cdot N(u,v)$ <br> $M(u,v) = S_{uv}(u,v) \cdot N(u,v)$ <br> $N(u,v) = S_{vv}(u,v) \cdot N(u,v)$ | Represents the extrinsic properties of the surface |
| **Area** | $A = \int_{v_{\text{min}}}^{v_{\text{max}}} \int_{u_{\text{min}}}^{u_{\text{max}}} \|S_u(u,v) \times S_v(u,v)\| du dv$ <br> $\left(\|S_u(u,v) \times S_v(u,v)\| = \sqrt{EG-F^2}\right)$ | Area of the rectangular region |
| **Gaussian Curvature** | $K(u,v) = \frac{LN - M^2}{EG - F^2}$ | Overall curvature of the surface |
| **Mean Curvature** | $H(u,v) = \frac{GL - 2FM + EN}{2(EG - F^2)}$ | Average curvature of the surface |
| **Principal Curvatures** | $\kappa_1(u,v), \kappa_2(u,v) $ <br> $= H \pm \sqrt{H^2 - K}$ | Maximum and minimum curvatures of the surface |

### Partial Derivative (Surface)

For a parametric surface $S(u,v)$, the (**first-order**) **partial derivatives** at $(u, v)$ are defined as follows:

$$S_u(u,v) = \frac{\partial S}{\partial u}, \quad S_v(u,v) = \frac{\partial S}{\partial v}$$

$$\left(S_u(u,v), S_v(u,v) \in \mathbb{R}^3\right)$$

Based on the first-order partial derivatives, the **second-order partial derivatives** are defined as follows. Since the surfaces defined in IGES are all smooth, the mixed partial derivatives do not depend on the order (i.e., $S_{uv} = S_{vu}$).

$$S_{uu}(u,v) = \frac{\partial^2 S}{\partial u^2}, \quad S_{uv}(u,v) = \frac{\partial^2 S}{\partial u \partial v}, \quad S_{vv}(u,v) = \frac{\partial^2 S}{\partial v^2}$$

$$\left(S_{uu}(u,v), S_{uv}(u,v), S_{vv}(u,v) \in \mathbb{R}^3\right)$$

These partial derivatives play an important role in analyzing the shape of the surface. For example, $S_u$ and $S_v$ represent tangent vectors on the surface, and $N(u,v) = S_u(u,v) \times S_v(u,v)$ represents the normal vector. By using these vectors, the tangent plane and normal direction of the surface can be calculated. The second partial derivatives are used to calculate the curvature of the surface.

### Tangent and Normal Vectors (Surface)

The **tangent vectors** at a point on the surface $S(u,v)$ are given by the partial derivatives $S_u$ and $S_v$. These vectors represent the tangents to the surface in the u and v directions, respectively. Normalizing these tangent vectors yields the **unit tangent vectors** $T_u$ and $T_v$.

$$T_u(u,v) = \frac{S_u(u,v)}{\|S_u(u,v)\|}, \quad T_v(u,v) = \frac{S_v(u,v)}{\|S_v(u,v)\|}$$

$$\left(T_u(u,v), T_v(u,v) \in \mathbb{R}^3, \; \|T_u(u,v)\| = \|T_v(u,v)\| = 1\right)$$

The **normal vector** of the surface is obtained by the cross product of the tangent vectors. This yields the unit normal vector $N(u,v)$, which represents the normal direction at each point on the surface. In this library, when calculating the normal vector, the u-direction tangent vector is placed on the left side of the cross product, and the v-direction tangent vector is placed on the right side.

$$N(u,v) = T_u(u,v) \times T_v(u,v)$$

$$\left(N(u,v) \in \mathbb{R}^3, \; \|N(u,v)\| = 1\right)$$

### First and Second Fundamental Forms

The **first fundamental form** represents the intrinsic properties of the surface (length, angle, area, etc. on the surface) and is calculated from the first-order partial derivatives as follows:

$$E(u,v) = S_u(u,v) \cdot S_u(u,v), \quad F(u,v) = S_u(u,v) \cdot S_v(u,v), \quad G(u,v) = S_v(u,v) \cdot S_v(u,v)$$

From this, it is possible to calculate the metric properties of the surface. For example, the length for a minute movement on the surface $dS = S_u du + S_v dv$ is expressed as follows:

$$ds = \sqrt{E du^2 + 2F du dv + G dv^2}$$

The **second fundamental form** describes how the surface curves in three-dimensional space $\mathbb{R}^3$, which is an extrinsic property. It is defined using the second-order partial derivatives and the normal vector as follows:

$$L(u,v) = S_{uu}(u,v) \cdot N(u,v), \quad M(u,v) = S_{uv}(u,v) \cdot N(u,v), \quad N(u,v) = S_{vv}(u,v) \cdot N(u,v)$$

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
