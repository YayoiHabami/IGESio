# Entities

This document covers the interfaces for specific entities and individual entity classes. For `IEntityIdentifier`, which all classes inherit, see [here](entity_base_class_architecture.md#ientityidentifier). Also, for `EntityBase`, which individual entity classes inherit, see [here](entity_base_class_architecture.md#entitybase).

## Table of Contents

- [Table of Contents](#table-of-contents)
- [Interfaces](#interfaces)
  - [`ICurve`](#icurve)
  - [`ICurve2D`](#icurve2d)
  - [`ICurve3D`](#icurve3d)
  - [`ISurface`](#isurface)
  - [`ITransformation`](#itransformation)
  - [`IColorDefinition`](#icolordefinition)
- [Annotations](#annotations)
- [Structures](#structures)
  - [`UnsupportedEntity`](#unsupportedentity)
  - [`NullEntity` (type 0)](#nullentity-type-0)
  - [`ColorDefinition` (type 314)](#colordefinition-type-314)
- [Curves](#curves)
  - [`CircularArc` (type 100)](#circulararc-type-100)
  - [`CompositeCurve` (type 102)](#compositecurve-type-102)
  - [`ConicArc` (type 104)](#conicarc-type-104)
  - [Copious Data (type 106)](#copious-data-type-106)
    - [`CopiousDataBase` (type 106)](#copiousdatabase-type-106)
    - [`CopiousData` (type 106, forms 1-3)](#copiousdata-type-106-forms-1-3)
    - [`LinearPath` (type 106, forms 11-13)](#linearpath-type-106-forms-11-13)
  - [`Line` (type 110)](#line-type-110)
  - [`RationalBSplineCurve` (type 126)](#rationalbsplinecurve-type-126)
- [Surfaces](#surfaces)
  - [`RationalBSplineSurface` (type 128)](#rationalbsplinesurface-type-128)
- [Transformations](#transformations)
  - [`TransformationMatrix` (type 124)](#transformationmatrix-type-124)

## Interfaces

Entities described in IGES files implement corresponding interfaces according to their type. These interfaces define the basic characteristics and operations of the entities. By implementing these interfaces, concrete entity classes can handle entities in a unified manner.

These interfaces are also used as pointer types to entities that an entity refers to for a specific function. For example, `CompositeCurve` refers to multiple curves, and the references to each entity are held as pointers of type `ICurve`.

### `ICurve`

> Defined at [i_curve.h](../../include/igesio/entities/interfaces/i_curve.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <─── ICurve
> ```

### `ICurve2D`

> Defined at [i_curve.h](../../include/igesio/entities/interfaces/i_curve.h)

> Ancestor classes:
> ```plaintext
> IEntityIdentifier <─── ICurve <─── ICurve2D
> ```

### `ICurve3D`

> Defined at [i_curve.h](../../include/igesio/entities/interfaces/i_curve.h)

> Ancestor classes:
> ```plaintext
> IEntityIdentifier <─── ICurve <─── ICurve3D
> ```

### `ISurface`

> Defined at [i_surface.h](../../include/igesio/entities/interfaces/i_surface.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <─── ISurface
> ```

### `ITransformation`

> Defined at [i_transformation.h](../../include/igesio/entities/i_transformation.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <─── ITransformation
> ```

The `ITransformation` class is the interface for the Transformation Matrix (Type 124) entity. Only the `TransformationMatrix` class inherits this class, but it is used to hold references to Transformation Matrix entities within the `DETransformationMatrix` class. This interface is defined because a design using forward definitions of the `TransformationMatrix` class would violate the principle of abstraction.

### `IColorDefinition`

> Defined at [i_color_definition.h](../../include/igesio/entities/interfaces/i_color_definition.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <─── IColorDefinition
> ```

## Annotations

This section describes the annotation entities in IGES files. Annotation entities are used to add information to the model, such as dimensions, notes, and symbols.

## Structures

This section describes the structure entities in IGES files. Structure entities are used to define the relationships and structures between entities, such as the arrangement and connectivity of entities within a model, external file references, the relationship between drawings and views, finite element modeling, and attribute tables.

In this library, an `UnsupportedEntity` class is also defined to represent unimplemented entity types. This class is used when a concrete entity class corresponding to a specific entity type does not exist.

### `UnsupportedEntity`

> Defined at [unsupported_entity.h](../../include/igesio/entities/structures/unsupported_entity.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <── EntityBase <── UnsupportedEntity
> ```

The `UnsupportedEntity` class is used when a concrete entity class implementation corresponding to the specified entity type does not exist (or is not implemented). This class inherits `EntityBase` and holds entity identification information, but does not implement any specific entity functionality.

```cpp
namespace i_ent = igesio::entities;

auto entity = i_ent::CreateEntity(de, pd, {});

if (entity->IsSupported()) {
        // Supported entity
} else {
        // Unsupported entity
        auto unsupported_entity = std::dynamic_pointer_cast<i_ent::UnsupportedEntity>(entity);
        if (unsupported_entity) {
                std::cout << "Unsupported entity type: "
                                    << unsupported_entity->GetType() << std::endl;
        }

        // For example, even if EntityType is kCircularArc,
        // the following process will result in an error.
        // auto circular_arc = std::dynamic_pointer_cast<i_ent::CircularArc>(entity);
        // unsupported_entity->Radius();
}
```

### `NullEntity` (type 0)

> Defined at [null_entity.h](../../include/igesio/entities/structures/null_entity.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <── EntityBase <── NullEntity
> ```

### `ColorDefinition` (type 314)

> Defined at [color_definition.h](../../include/igesio/entities/structures/color_definition.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <─┬─────── EntityBase <─┬─ ColorDefinition
>                     └─ IColorDefinition <─┘
> ```

The `ColorDefinition` class is an entity class for defining the color of entities. It represents colors defined in IGES files and can be applied to other entities. The following code example creates a `ColorDefinition` entity with RGB values (30, 50, 100) and applies it to a `CircularArc` entity (see figure).

Note: While standard RGB values are specified in the range (0-255), the `ColorDefinition` class uses the range (0.0-100.0). For example, RGB (76, 127, 255) is specified as (30.0, 50.0, 100.0).

```cpp
auto circle = std::make_shared<igesio::entities::CircularArc>(
  igesio::Vector2d{0.0, 0.0}, 1.0);

auto blue_circle = std::make_shared<igesio::entities::CircularArc>(
  igesio::Vector2d{3.0, 0.0}, 1.0);

// Create a Color Definition entity (≈ #4C7FFF)
// and overwrite the color of the blue_circle entity.
auto color_def = std::make_shared<igesio::entities::ColorDefinition>(
  std::array<double, 3>{30.0, 50.0, 100.0}, "Bright Blue");
blue_circle->OverwriteColor(color_def);
```

<img src="./images/color_definition.png" width=600px alt="ColorDefinition Example" />

**Figure: Example of a ColorDefinition entity.** Left: default color (black), right: #4C7FFF (bright blue).

## Curves

This section describes the curve entities in IGES files. Curve entities are used to represent curves in 2D or 3D space.

### `CircularArc` (type 100)

> Defined at [circular_arc.h](../../include/igesio/entities/curves/circular_arc.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <─┬─────────── EntityBase <─┬─ CircularArc
>                     └─ ICurve  <── ICurve2D <─┘
> ```

The `CircularArc` class represents 2D circular arcs and circles. The following code example creates a circle with center $(-1.25, 0)$ and radius $1$, and a circular arc with center $(1.25, 0)$ and radius $1$ (see figure).

```cpp
// 1. circle: center (-1.25, 0), radius 1
auto circle = std::make_shared<igesio::entities::CircularArc>(
  igesio::Vector2d{-1.25, 0.0}, 1.0);

// 2. arc: center (1.25, 0), radius 1, start angle 4π/3, end angle 5π/2
double start_angle = 4.0 * igesio::kPi / 3.0;
double end_angle = 5.0 * igesio::kPi / 2.0;
auto arc_start = igesio::Vector2d{1.25 + cos(start_angle), sin(start_angle)};
auto arc_end   = igesio::Vector2d{1.25 + cos(end_angle),   sin(end_angle)};

auto arc = std::make_shared<igesio::entities::CircularArc>(
  igesio::Vector2d{1.25, 0.0}, arc_start, arc_end);
```

<img src="./images/circular_arc.png" width=600px alt="CircularArc Example" />

**Figure: Example of a CircularArc entity.**

### `CompositeCurve` (type 102)

> Defined at [composite_curve.h](../../include/igesio/entities/curves/composite_curve.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <─┬─────────── EntityBase <─┬─ CompositeCurve
>                     └─ ICurve  <── ICurve3D <─┘
> ```

The `CompositeCurve` class is used to represent a single curve composed by joining multiple curve entities together. The following code example creates a `CompositeCurve` entity by connecting three curve entities (arc, line, arc), as shown in the figure.

```cpp
using igesio::Vector2d;
using igesio::Vector3d;

// 1. arc: center (0.5, -1), radius 1.5, start (-1, -1), end (2, -1) (CCW)
// -> CircularArc is defined clockwise, so use transformation matrix to flip and translate
auto comp_1_trans = std::make_shared<igesio::entities::TransformationMatrix>(
  igesio::AngleAxisd(igesio::kPi, Vector3d::UnitY()), Vector3d{0.5, -1.0, 0.0});
auto comp_1 = std::make_shared<igesio::entities::CircularArc>(
  Vector2d{0.0, 0.0}, Vector2d{-1.5, 0.0}, Vector2d{1.5, 0.0});
comp_1->OverwriteTransformationMatrix(comp_1_trans);

// 2. line: start (-1, -1), end (1, 1)
auto comp_2 = std::make_shared<igesio::entities::Line>(
  Vector3d{-1.0, -1.0, 0.0}, Vector3d{1.0, 1.0, 0.0});

// 3. arc: center (-0.5, 1), radius 1.5, start (1, 1), end (-2, 1) (CW)
auto comp_3 = std::make_shared<igesio::entities::CircularArc>(
  Vector2d{-0.5, 1.0}, Vector2d{1.0, 1.0}, Vector2d{-2.0, 1.0});

// Composite curve
// arc1 --> line --> arc2
auto comp_curve = std::make_shared<igesio::entities::CompositeCurve>();
comp_curve->AddCurve(comp_1);
comp_curve->AddCurve(comp_2);
comp_curve->AddCurve(comp_3);
```

<img src="./images/composite_curve.png" width=300px alt="CompositeCurve Example" />

**Figure: Example of a CompositeCurve entity** (the lower right arc is `comp_1`)

### `ConicArc` (type 104)

> Defined at [conic_arc.h](../../include/igesio/entities/curves/conic_arc.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <─┬─────────── EntityBase <─┬─ ConicArc
>                     └─ ICurve  <── ICurve2D <─┘
> ```

The `ConicArc` class represents 2D conic curves (ellipse, parabola, hyperbola). The following code example creates an elliptical arc with center at $(0, 3)$, major axis $3$, and minor axis $2$ (see figure).

Currently, parabola and hyperbola are not implemented (the entity class is common, but internal processing is not supported).

```cpp
// 1. ellipse arc: center (0, 3), axis (x, y) = (3, 2),
//    start angle 7π/4, end angle 17π/6
auto ellipse_arc = std::make_shared<igesio::entities::ConicArc>(
  std::pair<double, double>{-3.0, 2.0},  // radii (rx, ry)
  7.0 * kPi / 4.0,    // start angle
  17.0 * kPi / 6.0);  // end angle

// Note: Since elliptical arc entities are defined with the origin
// as their center, use a transformation matrix entity to move the origin.
auto ellipse_trans = std::make_shared<igesio::entities::TransformationMatrix>(
  igesio::Matrix3d::Identity(), igesio::Vector3d{0.0, 3.0, 0.0});
ellipse_arc->OverwriteTransformationMatrix(ellipse_trans);
```

<img src="./images/conic_arc.png" width=400px alt="ConicArc Example" />

**Figure: Example of a ConicArc entity** (ellipse only)

### Copious Data (type 106)

#### `CopiousDataBase` (type 106)

> Defined at [copious_data_base.h](../../include/igesio/entities/curves/copious_data_base.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <── EntityBase <── CopiousDataBase
> ```

This is the base class for all Copious Data related classes. It does not implement interfaces such as `ICurve`. An instance of this class is generated for form numbers not implemented in this library.

> Some Copious Data entities (Forms 20–40) are classified as [Annotation](#annotations) entities, not [Curve](#curves) and [Surface](#surfaces) Geometry entities.

<img src="./images/copious_data.png" width=600px alt="CopiousData Example" />

**Figure: Example of Copious Data entities.** Left: Form 2 (point cloud), right: Form 12 (polyline). The right side is a polyline defined by shifting the same points as the left side to the right.

#### `CopiousData` (type 106, forms 1-3)

> Defined at [copious_data.h](../../include/igesio/entities/curves/copious_data.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <── EntityBase <─┬────── CopiousDataBase <─┬─ CopiousData
>                                    └─ ICurve  <── ICurve3D <─┘
> ```

`CopiousData` is a class for representing point cloud data of forms 1–3 (2D coordinates, 3D coordinates, 6D coordinates). The following code example creates a point cloud consisting of five 3D coordinate points (see [CopiousDataBase figure](#copiousdatabase-type-106)).

```cpp
igesio::Matrix3Xd copious_coords(3, 5);
copious_coords << 3.0,  2.0, 2.0, 0.0, -1.0,
          0.0,  1.0, 2.0, 3.0,  2.0,
          1.0, -1.0, 0.0, 1.0,  0.0;
auto copious = std::make_shared<igesio::entities::CopiousData>(
    igesio::entities::CopiousDataType::kPoints3D, copious_coords);
```

#### `LinearPath` (type 106, forms 11-13)

> Defined at [linear_path.h](../../include/igesio/entities/curves/linear_path.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <── EntityBase <─┬────── CopiousDataBase <─┬─ LinearPath
>                                    └─ ICurve  <── ICurve3D <─┘
> ```

`LinearPath` is a class for representing polyline data of forms 11–13 (2D coordinates, 3D coordinates, 6D coordinates). The following code example creates a polyline consisting of five 3D coordinate points (see [CopiousDataBase figure](#copiousdatabase-type-106)). The points used are the same as [CopiousData](#copiousdata-type-106-forms-1-3), but shifted $5$ units to the right.

```cpp
auto copious_trans = std::make_shared<igesio::entities::TransformationMatrix>(
    igesio::Matrix3d::Identity(), igesio::Vector3d{5.0, 0.0, 0.0});
auto linear_path = std::make_shared<igesio::entities::CopiousData>(
    igesio::entities::CopiousDataType::kPolyline3D, copious_coords);
linear_path->OverwriteTransformationMatrix(copious_trans);
```

### `Line` (type 110)

> Defined at [line.h](../../include/igesio/entities/curves/line.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <─┬─────────── EntityBase <─┬─ Line
>                     └─ ICurve  <── ICurve3D <─┘
> ```

The `Line` class represents straight lines, rays, and line segments in 3D space. The following code example creates three types of Line entities (segment, ray, line), as shown in the figure.

```cpp
using igesio::Vector3d;

// 1. segment: start (0, -1, 0), end (1, 1, 0)
auto start = Vector3d{0.0, -1.0, 0.0};
auto end = Vector3d{1.0, 1.0, 0.0};
auto line_segment = std::make_shared<igesio::entities::Line>(
  start, end, igesio::entities::LineType::kSegment);

// 2. semi-infinite line: start (2, -1, 0), direction (1, 2, 0)
start = Vector3d{2.0, -1.0, 0.0};
end = start + Vector3d{1.0, 2.0, 0.0};
auto ray = std::make_shared<igesio::entities::Line>(
  start, end, igesio::entities::LineType::kRay);

// 3. infinite line: point (4, -1, 0), direction (1, 2, 0)
start = Vector3d{4.0, -1.0, 0.0};
end = start + Vector3d{1.0, 2.0, 0.0};
auto line = std::make_shared<igesio::entities::Line>(
  start, end, igesio::entities::LineType::kLine);
```

<img src="./images/line.png" width=400px alt="Line Example" />

**Figure: Example of Line entities.** From left: segment, ray, line.

### `RationalBSplineCurve` (type 126)

> Defined at [rational_b_spline_curve.h](../../include/igesio/entities/curves/rational_b_spline_curve.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <─┬─────────── EntityBase <─┬─ RationalBSplineCurve
>                     └─ ICurve  <── ICurve3D <─┘
> ```

The `RationalBSplineCurve` class represents rational B-spline curves in 3D space. The following code example creates a non-periodic, open NURBS curve of degree 3 with four control points (see figure). You can use the `IGESParameterVector` structure to pass all parameters at once when creating an instance.

**Note:** Make sure to distinguish between `int` and `double` types when passing parameters to `IGESParameterVector`. For example, the seventh parameter (the first knot value; `0.0`) must be passed as `0.0` (double), not `0` (int), otherwise an error will occur.

```cpp
auto param = igesio::IGESParameterVector{
  3,  // degree
  3,  // number of control points - 1
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
auto nurbs_c = std::make_shared<igesio::entities::RationalBSplineCurve>(param);
```

<img src="./images/rational_b_spline_curve.png" width=400px alt="RationalBSplineCurve Example" />

**Figure: Example of a RationalBSplineCurve entity**

## Surfaces

This section describes the surface entities in IGES files. Surface entities are used to represent surfaces in 3D space.

### `RationalBSplineSurface` (type 128)

> Defined at [rational_b_spline_surface.h](../../include/igesio/entities/surfaces/rational_b_spline_surface.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <─┬─ EntityBase <─┬─ RationalBSplineSurface
>                     └─ ISurface <───┘
> ```

The `RationalBSplineSurface` class represents rational B-spline surfaces in 3D space. The following code example creates a non-periodic, open NURBS surface of degree 3 with 6x6 control points (see figure; for details on parameters, see the `CreateRationalBSplineSurface` function in [examples/sample_surfaces.cpp](../../examples/sample_surfaces.cpp)). As shown below, use the `IGESParameterVector` structure to pass parameters at once and generate an instance.

Note that when passing parameters to `IGESParameterVector`, be sure to clearly distinguish between `int` and `double`. For example, if the eighth parameter (the first U knot vector value; `0.0`) is passed as `0` (int), an error will occur.

```cpp
// Freeform surface
auto param = igesio::IGESParameterVector{
  5, 5,  // K1, K2 (Number of control points - 1 in U and V)
  3, 3,  // M1, M2 (Degree in U and V)
  false, false, true, false, false,         // PROP1-5
  0., 0., 0., 0., 1., 2., 3., 3., 3., 3.,   // Knot vector in U
  0., 0., 0., 0., 1., 2., 3., 3., 3., 3.,   // Knot vector in V
  1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1.,   // Weights W(0,0) to W(1,5)
  1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1.,   // Weights W(2,0) to W(3,5)
  1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1.,   // Weights W(4,0) to W(5,5)
  // Control points (36 points, each with x, y, z)
  -25., -25., -10.,  // Control point (0,0)
  -25., -15., -5.,   // Control point (0,1)
  // ...
  25., 25., -10.,    // Control point (5,5)
  0., 3., 0., 3.     // Parameter range in U and V
};
auto nurbs_freeform = std::make_shared<igesio::entities::RationalBSplineSurface>(param);
nurbs_freeform->OverwriteColor(igesio::entities::ColorNumber::kCyan);
```

<img src="./images/rational_b_spline_surface.png" width=400px alt="RationalBSplineSurface Example" />

**Figure: Example of a RationalBSplineSurface entity**

## Transformations

This section describes the transformation matrix entities in IGES files. Transformation matrices are used to transform coordinate systems and change the position, orientation, and scale of entities.

> According to the IGES 5.3 standard, Transformation Matrix (Type 124) is defined as a type of Curve and Surface Geometry entity, that is, [Curves](#curves) and [Surfaces](#surfaces).

### `TransformationMatrix` (type 124)

> Defined at [transformation_matrix.h](../../include/igesio/entities/transformations/transformation_matrix.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <─┬────── EntityBase <─┬─ TransformationMatrix
>                     └─ ITransformation <─┘
> ```
