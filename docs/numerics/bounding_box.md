# Bounding Box

Defined in [`igesio/numerics/bounding_box.h`](./../../include/igesio/numerics/bounding_box.h)

## Table of Contents

- [Table of Contents](#table-of-contents)
- [Overview](#overview)
- [Member Functions](#member-functions)
  - [Constructors](#constructors)
  - [Setting, Getting, and Modifying Parameters](#setting-getting-and-modifying-parameters)
  - [Querying State](#querying-state)
  - [Containment and Intersection Tests](#containment-and-intersection-tests)
- [Appendix](#appendix)
  - [Mathematical Derivation of BoundingBox Operations](#mathematical-derivation-of-boundingbox-operations)
    - [Variable Definitions](#variable-definitions)
    - [BoundingBox Region and Vertices](#boundingbox-region-and-vertices)
    - [Containment Test Between BoundingBoxes](#containment-test-between-boundingboxes)
    - [Merging BoundingBoxes](#merging-boundingboxes)
    - [Point Containment Test](#point-containment-test)
    - [Line Intersection Test](#line-intersection-test)

## Overview

This document describes the Bounding Box class used in numerical computations. A bounding box defines the minimal boundary of an object or dataset, enabling various applications such as spatial relationship queries, collision detection, and rendering optimization.

In this library, the `BoundingBox` class and related functions are implemented in [`igesio/numerics/bounding_box.h`](./../../include/igesio/numerics/bounding_box.h). The `BoundingBox` class supports creating, manipulating, and querying bounding boxes in both 2D and 3D space.

The class defines a bounding box using the following four types of parameters:

| Parameter | Description |
|:---|:---|
| Control Point $P_0$ | A corner point of the bounding box <br> Specifically, when aligned with the x, y, z axes, this is the point with the smallest coordinate values |
| Extension Directions $D_0, D_1, D_2$ | Three orthogonal unit vectors that form the bounding box (rectangular prism) |
| Edge Lengths $s_0, s_1, s_2$ | Lengths of the bounding box edges along each extension direction <br> $s_0, s_1 > 0, \quad s_2 \geq 0$ (for 2D cases, $s_2 = 0$) |
| Edge Types $\text{type}_0, \text{type}_1, \text{type}_2$ | Enumeration type representing the type of edge along each extension direction <br> - `kSegment`: Finite line segment <br> - `kRay`: Half-infinite line (extends infinitely from the starting point) <br> - `kLine`: Infinite line (extends infinitely in both directions) |

> Only when created with the default constructor will a special bounding box be generated with $P_0 = (0,0,0)$ and $s_0 = s_1 = s_2 = 0$. In this case, `BoundingBox::IsEmpty()` returns `true`.

<img src="./images/bounding_boxes.svg" alt="Bounding Boxes Illustration" width="500"/>

**Figure 1.** Conceptual diagram of 2D/3D bounding boxes. Edge lengths $s_0, s_1, s_2$ are defined along each extension direction $D_0, D_1, D_2$ from the control point $P_0$. The figure shows the case where $\text{type}_i = \text{kSegment}\ (i = 0,1,2)$.

## Member Functions

### Constructors

| Function | Description |
|:---|:---|
| `BoundingBox()` | Default constructor. Creates an empty bounding box with the control point at the origin and size of 0. |
| `BoundingBox(control, directions, sizes, is_line)` | Creates a bounding box in arbitrary directions (2D or 3D).<br> - `control`: Control point $P_0$<br> - `directions`: Array of extension directions (2 elements for 2D, 3 for 3D; each element is a unit vector)<br> - `sizes`: Size along each direction<br> - `is_line`: Array indicating whether each direction is kLine (defaults to all `false`)<br> ※Overloaded for 2D/3D. |
| `BoundingBox(control, sizes, is_line)` | Creates a bounding box aligned with the x, y, z axes (or x, y axes).<br> - `control`: Control point $P_0$<br> - `sizes`: Size along each direction (3 elements for 3D, 2 for 2D)<br> - `is_line`: Array indicating whether each direction is kLine (defaults to all `false`) |

### Setting, Getting, and Modifying Parameters

| Function | Description |
|:---|:---|
| `GetControl()` <br> `SetControl(point)` | Get and set the control point $P_0$ |
| `GetDirections()` <br> `SetDirections(directions, types)` | Get and set the extension directions $D_0, D_1, D_2$ <br> (`types` is an array of `BoundingBox::DirectionType`) |
| `GetSizes()` <br> `SetSizes(sizes, is_line)` <br> `SetSize(index, size, is_line)` | Get and set the edge lengths $s_0, s_1, s_2$ <br> For `kRay` and `kLine`, specify $+\infty$; for `kSegment`, specify a value in $[0, +\infty)$ <br> If `is_line` is `true`, treated as `kLine`; if `false`, treated as `kRay` or `kSegment` |
| `GetDirectionTypes()` | Get the edge types $\text{type}_0, \text{type}_1, \text{type}_2$ <br> (`types` is an array of `BoundingBox::DirectionType`) |
| `Translate(vector)` <br> `Rotate(matrix)` <br> `Transform(matrix, vector)` | Transform the bounding box <br> - Translation by vector `vector`: $P_0 \leftarrow P_0 + \text{vector}$ <br> - Rotation by matrix `matrix`: $D_i \leftarrow \text{matrix} D_i$ |
| `ExpandToInclude(box)` | Expand to fully contain another `BoundingBox` (`box`) <br> The basis vectors $D_0, D_1, D_2$ remain unchanged; only the control point $P_0$ and edge lengths $s_0, s_1, s_2$ are modified |

### Querying State

| Function | Description |
|:---|:---|
| `IsEmpty()` | Check if empty <br> (Returns `true` when edge lengths $s_0 = s_1 = s_2 = 0$) |
| `Is2D()` <br> `Is3D()` | Check if 2D or 3D <br> (Determined as 2D when $s_2 = 0$) |
| `IsOnZPlane()` | Check if on the Z=0 plane <br> (Returns `true` when `Is2D()==true` and $D_2 \parallel e_z$ and $P_0.z = 0$) |
| `IsFinite()` | Check if (volume is) finite <br> (Returns `true` when all edge types are `kSegment`) |
| `GetVertices()` | Get all vertices <br> (Vertex coordinates may be $\pm \infty$ when infinite edges exist) |
| `GetFiniteVertices()` | Get only finite vertices <br> (Returns an empty vector when infinite edges exist) |

### Containment and Intersection Tests

| Function | Description |
|:---|:---|
| `Contains(point)` | Check if a point `point` in 3D space is contained |
| `Contains(box)` | Check if another `BoundingBox` (`box`) is fully contained <br> Returns `false` if any part extends outside |
| `Intersects(start, end, type)` | Check if a line (including line segments and rays) intersects <br> `type` is one of `BoundingBox::DirectionType` |

## Appendix

### Mathematical Derivation of BoundingBox Operations

#### Variable Definitions

The `BoundingBox` class defines a bounding box using the following four types of parameters:

- Control Point $P_0 \in \mathbb{R}^3$: A corner point of the bounding box; specifically, when aligned with the x, y, z axes, this is the point with the smallest coordinate values
- Extension Directions $D_0, D_1, D_2 \in \mathbb{R}^3$: Three orthogonal direction vectors that form the bounding box (rectangular prism)
    - $\|D_i\| = 1\ (i=0,1,2)$
    - $D_0 \cdot D_1 = 0, \quad D_0 \times D_1 = D_2$
- Edge Lengths $s_0, s_1, s_2 \in \mathbb{R}$: Lengths of the bounding box edges along each extension direction
    - $s_0, s_1 > 0, \quad s_2 \geq 0$ (for 2D cases, $s_2 = 0$)
- Edge Types $\text{type}_0, \text{type}_1, \text{type}_2$: Enumeration type representing the type of edge along each extension direction
    - `kSegment`: Finite line segment
    - `kRay`: Half-infinite line (extends infinitely from the starting point)
    - `kLine`: Infinite line (extends infinitely in both directions)

> When nothing is specified in the constructor (default constructor), the edge lengths satisfy $s_0 = s_1 = s_2 = 0$. This is a special case where `BoundingBox::IsEmpty()` returns `true`. Although not explicitly handled below, in this case `BoundingBox::Merge(box)` returns `box`, `BoundingBox::Contains(point)` always returns `false`, etc.
>
> Including this case, in `BoundingBox::GetDirectionTypes()`, $\text{type}_i$ is calculated as follows:
>
> $$\text{type}_i = \begin{cases} \text{kRay} & (s_i = +\infty) \\\ \text{kLine} & (s_i = -\infty) \\\ \text{kSegment} & (\text{otherwise}) \end{cases}$$

#### BoundingBox Region and Vertices

The region represented by a bounding box is finite only when all $\text{type}_i$ are `kSegment`. When $s_2 = 0$, this region represents a 2D rectangle; when $s_2 > 0$, it represents a 3D rectangular prism. When any edge has infinite length (`kRay` or `kLine`), the bounding box region extends infinitely. In this case, `BoundingBox::GetFiniteVertices()` returns an empty vector. On the other hand, `BoundingBox::GetVertices()` calculates vertices including those with coordinate values of $\pm \infty$.

When the bounding box region is a rectangle or rectangular prism, the vertices are calculated as follows:

| Vertex Name <br> $V_{D}$ | Rectangle <br> (4 vertices) | Rectangular Prism <br> (8 vertices) |
|:----|:---:|:---:|
| $V_{LFB}$ | $P_0$ | $P_0$ |
| $V_{RFB}$ | $P_0 + s_1 D_1$ | $P_0 + s_1 D_1$ |
| $V_{RBB}$ | $P_0 + s_2 D_2$ | $P_0 + s_2 D_2$ |
| $V_{LBB}$ | $P_0 + s_1 D_1 + s_2 D_2$ | $P_0 + s_1 D_1 + s_2 D_2$ |
| $V_{LFT}$ | - | $P_0 + s_3 D_3$ |
| $V_{RFT}$ | - | $P_0 + s_1 D_1 + s_3 D_3$ |
| $V_{RBT}$ | - | $P_0 + s_2 D_2 + s_3 D_3$ |
| $V_{LBT}$ | - | $P_0 + s_1 D_1 + s_2 D_2 + s_3 D_3$ |

> For coordinate values returned by `GetVertices()`, for example, if $D_0 = (1,0,0), s_0 = +\infty$ and $s_1, s_2 < \infty$, the x-component of vertices $V_{RFB}, V_{RBB}, V_{RFT}, V_{RBT}$ will be $+\infty$. Also, in this library, when $s_i = \infty$ and $D_{i,j} = 0\ (j = x,y,z)$, the corresponding vertex component is replaced with 0.

#### Containment Test Between BoundingBoxes

Consider two bounding boxes $\mathcal{B}\_A$ and $\mathcal{B}\_B$ given as follows. Let these be `box_a` and `box_b` respectively, and consider the operation `box_a.Contains(box_b)`. Note that this operation is **asymmetric** except when $\mathcal{B}_A \equiv \mathcal{B}_B$. That is, `box_a.Contains(box_b)` and `box_b.Contains(box_a)` generally return different results.

$$\begin{aligned}
    \mathcal{B}_A &= (P_{0,A}, D_{0,A}, D_{1,A}, D_{2,A}, s_{0,A}, s_{1,A}, s_{2,A}, \text{type}_{0,A}, \text{type}_{1,A}, \text{type}_{2,A}) \\\
    \mathcal{B}_B &= (P_{0,B}, D_{0,B}, D_{1,B}, D_{2,B}, s_{0,B}, s_{1,B}, s_{2,B}, \text{type}_{0,B}, \text{type}_{1,B}, \text{type}_{2,B})
\end{aligned}$$

First, to determine whether each vertex of $\mathcal{B}\_{B}$ is contained in $\mathcal{B}\_{A}$, we transform to the coordinate system $\Sigma_A$ with basis $D_{0, A}, D_{1, A}, D_{2, A}$. Let the coordinates of point $P$ in the world coordinate system $\Sigma_W = \lbrace e_x, e_y, e_z \rbrace$ be $(x_W, y_W, z_W)$, and the coordinates in $\Sigma_A$ be $(x_A, y_A, z_A)$. Since the three axes $D_{0,A}, D_{1,A}, D_{2,A}$ form an orthogonal coordinate system, the following transformation holds:

$$\begin{aligned}
    &\quad\begin{bmatrix} x_W \\\ y_W \\\ z_W \\\ 1 \end{bmatrix} =
    \begin{bmatrix} D_{0,A} & D_{1,A} & D_{2,A} & P_{0,A} \\\ 0 & 0 & 0& 1\end{bmatrix}
    \begin{bmatrix} x_A \\\ y_A \\\ z_A \\\ 1 \end{bmatrix} =: T_A \\\
    &\Rightarrow \begin{bmatrix} x_A \\\ y_A \\\ z_A \\\ 1 \end{bmatrix} =
    T_A^{-1} \begin{bmatrix} x_W \\\ y_W \\\ z_W \\\ 1 \end{bmatrix}
\end{aligned}$$

Next, transform each vertex $V_{D,B}$ of $\mathcal{B}\_B$ to $\Sigma_A$, and let its coordinates be $V_{D,B}'(x_{D,A}, y_{D,A}, z_{D,A})$. The condition for $x_{D,A}$ to be contained in $\mathcal{B}_A$ is as follows:

$$\begin{cases}
    \text{true} & (\text{type}_{0,A} = \text{kLine}) \\\
    0 \leq x_{D,A} \leq s_{0,A} & (\text{otherwise})
\end{cases}$$

Similarly, the same conditions hold for $y_{D,A}$ and $z_{D,A}$. Therefore, the necessary and sufficient condition for all vertices of $\mathcal{B}_B$ to be contained in $\mathcal{B}_A$ is as follows:

$$\begin{aligned}
    &\forall D: \\\
    &\quad\begin{cases}
        \text{true} & (\text{type}_{0,A} = \text{kLine}) \\\
        0 \leq x_{D,A} \leq s_{0,A} & (\text{otherwise})
    \end{cases} \\\
    &\land \begin{cases}
        \text{true} & (\text{type}_{1,A} = \text{kLine}) \\\
        0 \leq y_{D,A} \leq s_{1,A} & (\text{otherwise})
    \end{cases} \\\
    &\land \begin{cases}
        \text{true} & (\text{type}_{2,A} = \text{kLine}) \\\
        0 \leq z_{D,A} \leq s_{2,A} & (\text{otherwise})
    \end{cases}
\end{aligned}$$

#### Merging BoundingBoxes

Consider the two bounding boxes $\mathcal{B}_A$ and $\mathcal{B}_B$ described in the [previous section](#containment-test-between-boundingboxes). The operation `box_a.Merge(box_b)` is defined as expanding the caller $\mathcal{B}_A$ to fully contain the argument $\mathcal{B}_B$. Note that this operation is **not commutative**. That is, `box_a.Merge(box_b)` and `box_b.Merge(box_a)` generally return different results.

The merged bounding box $\mathcal{B}\_M$ has the same extension directions as $\mathcal{B}\_A$: $D_{0,M} = D_{0,A}, D_{1,M} = D_{1,A}, D_{2,M} = D_{2,A}$, but the control point $P_{0,M}$ and edge lengths $s_{0,M}, s_{1,M}, s_{2,M}$ change.

**Computing the Interval of $\mathcal{B}_B$ in $\Sigma_A$**

First, the interval $I_{i,A}$ in the $i$-axis direction of the original $\mathcal{B}\_A$ in $\Sigma_A$ is defined as follows:

$$I_{i,A} = \begin{cases}
    (-\infty, +\infty) & (\text{type}_{i,A} = \text{kLine}) \\\
    [0, +\infty) & (\text{type}_{i,A} = \text{kRay}) \\\
    [0, s_{i,A}] & (\text{type}_{i,A} = \text{kSegment})
\end{cases}$$

Next, the vertices $\text{vertices}_B$ of $\mathcal{B}_B$ are defined as follows:

$$\text{vertices}_B = \begin{cases}
    \lbrace V_{LFB}, V_{RFB}, V_{RBB}, V_{LBB} \rbrace & (s_{2,B} = 0) \\\
    \lbrace V_{LFB}, V_{RFB}, V_{RBB}, V_{LBB}, V_{LFT}, V_{RFT}, V_{RBT}, V_{LBT} \rbrace & (s_{2,B} > 0)
\end{cases}$$

Transform each vertex to $\Sigma_A$ with coordinates $V_{D,B}'(x_{D,A}, y_{D,A}, z_{D,A})$. Among these vertices, define the minimum and maximum values in each axis direction as follows:

$$\begin{aligned}
    x_{\text{min},B} &= \min_{V_{D,B}' \in \text{vertices}_B} x_{D,A}, \quad x_{\text{max},B} = \max_{V_{D,B}' \in \text{vertices}_B} x_{D,A} \\\
    y_{\text{min},B} &= \min_{V_{D,B}' \in \text{vertices}_B} y_{D,A}, \quad y_{\text{max},B} = \max_{V_{D,B}' \in \text{vertices}_B} y_{D,A} \\\
    z_{\text{min},B} &= \min_{V_{D,B}' \in \text{vertices}_B} z_{D,A}, \quad z_{\text{max},B} = \max_{V_{D,B}' \in \text{vertices}_B} z_{D,A}
\end{aligned}$$

The interval $I_{i,M}$ that $\mathcal{B}\_M$ should contain in the $i$-axis direction in $\Sigma_A$ is then defined as follows:

$$\begin{aligned}
    I_{i,M} &= [i_{\text{min},M}, i_{\text{max},M}] \\\
    &= I_{i,A} \cup [i_{\text{min},B}, i_{\text{max},B}] \\\
    &= \begin{cases}
        (-\infty, +\infty) & (\text{type}_{i,A} = \text{kLine}) \\\
        [\min(0, i_{\text{min},B}), \max(s_{i,A}, i_{\text{max},B})] & (\text{otherwise})
    \end{cases}
\end{aligned}$$

> This formula is not strictly correct and requires handling cases where $i_{\text{min},B}$ is $-\infty$ or $i_{\text{max},B}$ is $+\infty$ (for example, $[0, \infty]$ should properly be $[0, \infty)$). However, since the implementation handles these cases correctly, we omit this detail here for brevity.

**Computing the Control Point and Edge Lengths of $\mathcal{B}_M$**

Finally, the control point $P_{0,M}$ and edge lengths $s_{0,M}, s_{1,M}, s_{2,M}$ of $\mathcal{B}\_M$ are calculated as follows. Note that in the program, if any $s_{i,M}$ becomes `undefined`, `box_a.Merge(box_b)` throws an exception (because the basis vectors would need to be changed).

$$\begin{aligned}
    P_{0,M} &= P_{0,A} + x_{\text{min},M} D_{0,A} + y_{\text{min},M} D_{1,A} + z_{\text{min},M} D_{2,A} \\\
    s_{0,M} &= \begin{cases}
        \text{undefined} & (x_{\text{min},B} = -\infty \ \land\ x_{\text{max},M} < \infty) \\\
        +\infty & (\text{type}_{0,A} = \text{kLine}) \\\
        \max(0, x_{\text{max},M} - x_{\text{min},M}) & (\text{otherwise})
    \end{cases} \\\
    s_{1,M} &= \begin{cases}
        \text{undefined} & (y_{\text{min},B} = -\infty \ \land\ y_{\text{max},M} < \infty) \\\
        +\infty & (\text{type}_{1,A} = \text{kLine}) \\\
        \max(0, y_{\text{max},M} - y_{\text{min},M}) & (\text{otherwise})
    \end{cases} \\\
    s_{2,M} &= \begin{cases}
        \text{undefined} & (z_{\text{min},B} = -\infty \ \land\ z_{\text{max},M} < \infty) \\\
        +\infty & (\text{type}_{2,A} = \text{kLine}) \\\
        \max(0, z_{\text{max},M} - z_{\text{min},M}) & (\text{otherwise})
    \end{cases}
\end{aligned}$$

#### Point Containment Test

Consider a bounding box $\mathcal{B}$ and a point $P$ given as `box` and `point` respectively, for the operation `box.Contains(point)`. This is implemented by treating `point` as $V_{LFB}$ and applying the method described in the [previous section](#containment-test-between-boundingboxes) for a single vertex only.

#### Line Intersection Test

Consider a bounding box $\mathcal{B}$ and a line $\mathcal{L}$ (including line segments and rays). The line $\mathcal{L}$ is defined by a start point $P_{\text{start}}$, end point $P_{\text{end}}$, and line type $\text{line-type}$ (`kSegment`, `kRay`, `kLine`). For rays, $P_{\text{end}}$ is treated as a passing point; for infinite lines, both points are treated as passing points. The direction vector $V_{\text{dir}} \in \mathbb{R}^3$ and parametric equation $L(t)$ of the line are defined as follows:

$$\begin{aligned}
    V_{\text{dir}} &= P_{\text{end}} - P_{\text{start}} \\\
    L(t) &= P_{\text{start}} + t V_{\text{dir}}, \quad t \in T_{\text{line}} \\\
    T_{\text{line}} &= \begin{cases}
        [0, 1] & (\text{line-type} = \text{kSegment}) \\\
        [0, +\infty) & (\text{line-type} = \text{kRay}) \\\
        (-\infty, +\infty) & (\text{line-type} = \text{kLine})
    \end{cases}
\end{aligned}$$

Consider the operation `box.Intersects(start, end, line_type)` where $\mathcal{B}$ is `box` and the line's start and end points are `start` and `end`. First, transform the line $\mathcal{L}$ to its representation $L'(t)$ in the local coordinate system $\Sigma$ of $\mathcal{B}$ (control point $P_0$, extension directions $D_0, D_1, D_2$).

$$\begin{aligned}
    \begin{bmatrix} L'(t) \\\ 1 \end{bmatrix} &= T^{-1} \begin{bmatrix} L(t) \\\ 1 \end{bmatrix} \\\
    &= T^{-1} \begin{bmatrix} P_{\text{start}} \\\ 1 \end{bmatrix} + t T^{-1} \begin{bmatrix} V_{\text{dir}} \\\ 0 \end{bmatrix} \\\
    &= P_{\text{start}}' + t V_{\text{dir}}'
\end{aligned}$$

**Slab Method for Intersection Testing**

As described in [Merging BoundingBoxes](#merging-boundingboxes), the bounding box $\mathcal{B}$ is defined in its local coordinate system $\Sigma$ as the intersection of intervals $I_i$ for each axis $i$ (equation below). The slab method computes the interval $[t_{\text{near},i}, t_{\text{far},i}]$ of parameter $t$ where the line $L'(t)$ intersects the slab (interval) $I_i$ for each axis $i$, and determines the intersection of the line with the bounding box by finding the intersection of these intervals.

$$I_i = [i_{\text{min}}, i_{\text{max}}] = \begin{cases}
    (-\infty, +\infty) & (\text{type}_i = \text{kLine}) \\\
    [0, +\infty) & (\text{type}_i = \text{kRay}) \\\
    [0, s_i] & (\text{type}_i = \text{kSegment})
\end{cases}$$

**Case A: $V_{\text{dir},i}' \neq 0$**

The parameter $t$ where the line intersects the slab boundaries $i_{\text{min}}$ and $i_{\text{max}}$ is calculated as follows:

$$t_1 = \frac{i_{\text{min}} - P_{\text{start},i}'}{V_{\text{dir},i}'}, \quad t_2 = \frac{i_{\text{max}} - P_{\text{start},i}'}{V_{\text{dir},i}'}$$

When $V_{\text{dir},i}' > 0$, $t_1$ is the near point and $t_2$ is the far point; when $V_{\text{dir},i}' < 0$, the reverse is true. Therefore, the interval $[t_{\text{near},i}, t_{\text{far},i}]$ is defined as follows. Note that when $\text{type}\_i$ is `kRay` or `kLine`, $i_{\text{min}}$ or $i_{\text{max}}$ becomes infinite, so $t_1$ or $t_2$ also becomes infinite.

$$[t_{\text{near},i}, t_{\text{far},i}] = \begin{cases}
    [t_1, t_2] & (V_{\text{dir},i}' > 0) \\\
    [t_2, t_1] & (V_{\text{dir},i}' < 0)
\end{cases}$$

**Case B: $V_{\text{dir},i}' = 0$**

In this case, the line is parallel to the slab, and whether it intersects depends on whether the line's start point $P_{\text{start},i}'$ is within the slab. Therefore, the interval is defined as follows:

$$[t_{\text{near},i}, t_{\text{far},i}] = \begin{cases}
    [-\infty, +\infty] & (i_{\text{min}} \leq P_{\text{start},i}' \leq i_{\text{max}}) \\\
    \text{undefined} & (\text{otherwise})
\end{cases}$$

**Final Intersection Test**

Since the bounding box $\mathcal{B}$ is defined as the intersection of three slabs $I_0, I_1, I_2$, the interval $[t_{\text{inf,min}}, t_{\text{inf,max}}]$ of $t$ where the (infinite) line $L'(t)$ intersects the bounding box $\mathcal{B}$ is calculated as follows:

$$\begin{aligned}
    t_{\text{inf,min}} &= \max_{i=0,1,2} t_{\text{near},i} \\\
    t_{\text{inf,max}} &= \min_{i=0,1,2} t_{\text{far},i}
\end{aligned}$$

If any $t_{\text{near},i}$ or $t_{\text{far},i}$ is `undefined`, or if $t_{\text{inf,min}} > t_{\text{inf,max}}$, the line $L'(t)$ does not intersect the bounding box $\mathcal{B}$. Otherwise, the line $L'(t)$ intersects the bounding box $\mathcal{B}$. However, since this interval represents the intersection as an infinite line, we must also check whether it intersects the original line $\mathcal{L}$'s parameter range $T_{\text{line}}$. The final intersection test is performed as follows:

$$\begin{aligned}
    t_{\text{final,min}} &= \max(t_{\text{inf,min}}, \min T_{\text{line}}) \\\
    t_{\text{final,max}} &= \min(t_{\text{inf,max}}, \max T_{\text{line}})
\end{aligned}$$

Therefore, `box.Intersects(start, end, line_type)` returns `true` when this interval is valid, i.e., when $t_{\text{final,min}} \leq t_{\text{final,max}}$, and returns `false` otherwise.
