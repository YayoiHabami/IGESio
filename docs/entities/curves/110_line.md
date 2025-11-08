# Line (Type 110)

Defined at [curves/line.h](./../../../include/igesio/entities/curves/line.h)

## Contents

- [Contents](#contents)
- [Parameters](#parameters)
  - [Specific Parameters](#specific-parameters)
    - [`LineType`](#linetype)
  - [Curve Definition](#curve-definition)
    - [Curve $C(t)$](#curve-ct)
    - [Derivatives $C'(t), C''(t)$](#derivatives-ct-ct)

## Parameters

### Specific Parameters

| Member Function | Description |
|---|---|
| `GetLineType()` <br> (`LineType`) | Type of line segment |
| `GetDefinedAnchorPoints()` <br> (`std::pair<Vector3d, Vector3d>`) | Start point $P_1$ and end point $P_2$ of the line segment in the defined space |
| `GetAnchorPoints()` <br> (`std::pair<Vector3d, Vector3d>`) | Start point $P_1$ and end point $P_2$ of the line segment |

#### `LineType`

The `LineType` enumeration represents the type of line segment, corresponding one-to-one with the entity's form number (`GetFormNumber()`). Details are as follows:

| Form | Enumeration Value | Description |
|---|---|---|
| 0 | `kSegment` | Line segment with two endpoints: start point $P_1$ and end point $P_2$ |
| 1 | `kRay` | Half-line starting at point $P_1$ and extending infinitely in the direction of point $P_2$ |
| 2 | `kLine` | Infinite line passing through start point $P_1$ and end point $P_2$ |

### Curve Definition

#### Curve $C(t)$

The parametric curve $C(t)$ of the line segment is defined as follows:

$$C(t) = P_1 + t (P_2 - P_1)$$

Here, the range of the parameter $t$ varies depending on the `LineType` as follows:

$$t \in \begin{cases}
    [0, 1] & \text{if kSegment} \\\
    [0, \infty) & \text{if kRay} \\\
    (-\infty, \infty) & \text{if kLine}
\end{cases}$$

#### Derivatives $C'(t), C''(t)$

The first and second derivatives of the line segment are defined as follows:

$$C'(t) = P_2 - P_1, \quad C''(t) = 0$$

For various features calculated from the curve $C(t)$ and its derivatives $C'(t), C''(t)$, please refer to [Geometric Properties](./../geometric_properties.md).
