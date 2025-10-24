# Copious Data (Type 106)

Defined at:

- [curves/copious_data_base.h](./../../../include/igesio/entities/curves/copious_data_base.h): Base class for Copious Data entities
- [curves/copious_data.h](./../../../include/igesio/entities/curves/copious_data.h): Sequence of points (forms 1-3)
- [curves/linear_path.h](./../../../include/igesio/entities/curves/linear_path.h): Polyline (form 11-13)

## Contents

- [Contents](#contents)
- [Parameters](#parameters)
  - [Specific Parameters](#specific-parameters)
    - [`CopiousDataType`](#copiousdatatype)
  - [Curve Definition](#curve-definition)
    - [Curve $C(t)$](#curve-ct)
    - [Derivatives $C'(t), C''(t)$](#derivatives-ct-ct)

## Parameters

### Specific Parameters

| Member Function | Description |
|---|---|
| `GetDataType()` <br> (`CopiousDataType`) | Gets the type of point cloud |
| `GetIP()` <br> (`int`) | Gets the dimension IP of the point cloud. <br> - 1: 2D point sequence (all points have the same z-coordinate) <br> - 2: 3D point sequence <br> - 3: 3D point sequence + additional information vector |
| `GetCount()` <br> (`size_t`) | Number of points $n$ in the point sequence |
| `Coordinate(i)` <br> (`Vector3d`) | The `i`-th point $P_i = (x_i, y_i, z_i)$ in the point sequence |
| `Addition(i)` <br> (`Vector3d`) | Additional information vector $A_i = (a_{ix}, a_{iy}, a_{iz})$ corresponding to the `i`-th point in the point sequence |
| `Coordinates()` <br> (`Matrix3Xd`) | Matrix of all coordinate values $[P_0, P_1, \ldots, P_{n-1}] \in \mathbb{R}^{3 \times n}$ |
| `Additions()` <br> (`Matrix3Xd`) | Matrix of all additional information vectors $[A_0, A_1, \ldots, A_{n-1}] \in \mathbb{R}^{3 \times n}$ |
| `GetNearestVertexAt(t)` <br> (`std::pair<size_t, double>`) | Index of the point nearest to parameter $t$, and the distance to that point |

> The additional information vector corresponds to what is described as $I(1), J(1), K(1)$, etc. in the IGES 5.3 standard. In this document, we use the notation $A_i = (a_{ix}, a_{iy}, a_{iz})$ to avoid confusion with other symbols.

> The additional information vector is defined only when `IP=3`. Specifically, values are set only for `CopiousDataType::kSextuples` (form 3) and `CopiousDataType::kPolylineAndVectors` (form 13) (otherwise undefined).

#### `CopiousDataType`

The `CopiousDataType` enumeration represents the type of point cloud, corresponding one-to-one with the entity's form number (`GetFormNumber()`). Details are as follows:

| form | Enumeration Value | IP | Description |
|---|---|:-:|---|
| 1 | `kPlanarPoints` | 1 | Sequence of points on the plane $z = z_t$ |
| 2 | `kPoints3D` | 2 | Sequence of points in 3D space |
| 3 | `kSextuples` | 3 | Sequence of points in 3D space + additional information vector |
| 11 | `kPlanarPolyline` | 1 | Polyline on the plane $z = z_t$ |
| 12 | `kPolyline3D` | 2 | Polyline in 3D space |
| 13 | `kPolylineAndVectors` | 3 | Polyline in 3D space + additional information vector corresponding to each vertex |
| 20 | `kCenterlineByPoints` | 1 | Centerline passing through a point cloud |
| 21 | `kCenterlineByCircles` | 1 | Centerline passing through the center point cloud of circles |
| 31 | `kGeneralHatch` | 1 | Section hatching: General purpose for cast iron or malleable cast iron and all materials |
| 32 | `kSteelHatch` | 1 | Section hatching: Steel |
| 33 | `kBronzeHatch` | 1 | Section hatching: Bronze, brass, copper, and compositions |
| 34 | `kRubberHatch` | 1 | Section hatching: Rubber, plastic, and electrical insulation |
| 35 | `kTitaniumHatch` | 1 | Section hatching: Titanium and refractories |
| 36 | `kMarbleHatch` | 1 | Section hatching: Marble, slate, glass, porcelain |
| 37 | `kZincHatch` | 1 | Section hatching: White metal, zinc, lead, babbitt, and alloys |
| 38 | `kAluminumHatch` | 1 | Section hatching: Magnesium, aluminum, and aluminum alloys |
| 40 | `kWitnessLine` | 1 | Witness line |
| 63 | `kPlanarLoop` | 1 | Simple closed planar curve |

### Curve Definition

#### Curve $C(t)$

The parameter $t$ of the parametric curve $C(t)$ in the Copious Data entity is defined by linearly interpolating between each point in the point sequence. If the length of the $i$-th segment (from $P_i$ to $P_{i+1}$) is $L_i = \| P_{i+1} - P_i \|$, then the range of $t$ is as follows:

$$t \in \left[0, \quad \sum_{i=0}^{n-2} L_i \right]$$

Also, if the segment corresponding to the parameter value $t$ is the $i$-th segment ($i = 0, 1, \ldots n-2$), then $t$ satisfies the following condition:

$$\sum_{j=0}^{i-1} L_j \leq t < \sum_{j=0}^{i} L_j$$

**For forms 1-3:**

In this case, the curve $C(t)$ is defined as follows: That is, the coordinate value is defined only when the point corresponding to the current $t$ coincides with the start point $P_i$ in the segment $i$ (or the end point $P_{n-1}$ is also possible if $i = n-1$), and is undefined otherwise (linear interpolation is not performed). In this library, the point is returned only when the distance between the linearly interpolated coordinate at $t$ and $P_i$ is very small, and `std::nullopt` is returned otherwise.

$$C(t) = \begin{cases}
    P_i & (\text{if } t = \sum_{j=0}^{i-1} L_j) \\\
    P_{n-1} & (\text{if } t = \sum_{j=0}^{n-2} L_j) \\\
    \text{Undefined} & (\text{otherwise})
\end{cases}$$

**For forms 11-13:**

In this case, the curve $C(t)$ is defined as follows: That is, if the point corresponding to the current $t$ is located between the start point $P_i$ and the end point $P_{i+1}$ in the segment $i$, the coordinate value is defined by linear interpolation.

$$C(t) = P_i + \frac{t - \sum_{j=0}^{i-1} L_j}{L_i} (P_{i+1} - P_i)$$

**For forms 20, 21, 31-38, 40:**

Not implemented. Also, since these forms are all treated as Annotation entities, the geometric characteristics of the curve are not defined.

**For form 63:**

Copious Data (form 63) represents a simple closed planar curve. The end point $P_{n-1}\ (n \geq 2)$ is connected to the start point $P_0$, and the curve forms a closed loop. Considering the line segment connecting the end point to the start point as the $n-1$-th segment, and letting its distance be $L_{n-1} = \| P_0 - P_{n-1} \|$, the range of the parameter $t$ and the condition of $t$ in each segment are as follows:

$$t \in \left[0, \quad \sum_{i=0}^{n-1} L_i \right]$$

$$\sum_{j=0}^{i-1} L_j \leq t < \sum_{j=0}^{i} L_j \quad (i = 0, 1, \ldots, n-1)$$

In this case, the curve $C(t)$ is defined as follows:

$$C(t) = P_i + \frac{t - \sum_{j=0}^{i-1} L_j}{L_i} (P_{(i+1) \bmod n} - P_i)$$

#### Derivatives $C'(t), C''(t)$

The first and second derivatives of the parametric curve $C(t)$ in the Copious Data entity are defined as follows. The domain of definition of $t$ follows the one described above.

**For forms 1-3:**

For point sequences, the derivatives are always undefined. In the library, `std::nullopt` is always returned.

$$C'(t) = \text{Undefined}, \quad C''(t) = \text{Undefined}$$

**For forms 11-13:**

$$C'(t) = \frac{P_{i+1} - P_i}{L_i}, \quad C''(t) = 0$$

**For forms 20, 21, 31-38, 40:**

Not implemented. Also, since these forms are all treated as Annotation entities, the geometric characteristics of the curve are not defined.

**For form 63:**

$$C'(t) = \frac{P_{(i+1) \bmod n} - P_i}{L_i}, \quad C''(t) = 0$$

See [Geometric Properties](./../geometric_properties.md) for various features calculated from the curve $C(t)$ and the derivatives $C'(t), C''(t)$.
