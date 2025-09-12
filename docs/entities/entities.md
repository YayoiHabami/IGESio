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
  - [`Copious Data` (type 106)](#copious-data-type-106)
    - [`CopiousDataBase` (type 106)](#copiousdatabase-type-106)
    - [`CopiousData` (type 106, forms 1-3)](#copiousdata-type-106-forms-1-3)
    - [`LinearPath` (type 106, forms 11-13)](#linearpath-type-106-forms-11-13)
  - [`Line` (type 110)](#line-type-110)
  - [`RationalBSplineCurve` (type 126)](#rationalbsplinecurve-type-126)
- [Surfaces](#surfaces)
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

## Curves

This section describes the curve entities in IGES files. Curve entities are used to represent curves in 2D or 3D space.

### `CircularArc` (type 100)

> Defined at [circular_arc.h](../../include/igesio/entities/curves/circular_arc.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <─┬─────────── EntityBase <─┬─ CircularArc
>                     └─ ICurve  <── ICurve2D <─┘
> ```

### `CompositeCurve` (type 102)

> Defined at [composite_curve.h](../../include/igesio/entities/curves/composite_curve.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <─┬─────────── EntityBase <─┬─ CompositeCurve
>                     └─ ICurve  <── ICurve3D <─┘
> ```

### `ConicArc` (type 104)

> Defined at [conic_arc.h](../../include/igesio/entities/curves/conic_arc.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <─┬─────────── EntityBase <─┬─ ConicArc
>                     └─ ICurve  <── ICurve2D <─┘
> ```

### `Copious Data` (type 106)

#### `CopiousDataBase` (type 106)

> Defined at [copious_data_base.h](../../include/igesio/entities/curves/copious_data_base.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <── EntityBase <── CopiousDataBase
> ```

This is the base class for all Copious Data related classes. It does not implement interfaces such as `ICurve`. An instance of this class is generated for form numbers not implemented in this library.

> Some Copious Data entities (Forms 20-40) are classified as [Annotation](#annotation) entities, not [Curve](#curves) and [Surface](#surfaces) Geometry entities.

#### `CopiousData` (type 106, forms 1-3)

> Defined at [copious_data.h](../../include/igesio/entities/curves/copious_data.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <── EntityBase <─┬────── CopiousDataBase <─┬─ CopiousData
>                                    └─ ICurve  <── ICurve3D <─┘
> ```

#### `LinearPath` (type 106, forms 11-13)

> Defined at [linear_path.h](../../include/igesio/entities/curves/linear_path.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <── EntityBase <─┬────── CopiousDataBase <─┬─ LinearPath
>                                    └─ ICurve  <── ICurve3D <─┘
> ```

### `Line` (type 110)

> Defined at [line.h](../../include/igesio/entities/curves/line.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <─┬─────────── EntityBase <─┬─ Line
>                     └─ ICurve  <── ICurve3D <─┘
> ```

### `RationalBSplineCurve` (type 126)

> Defined at [rational_b_spline_curve.h](../../include/igesio/entities/curves/rational_b_spline_curve.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <─┬─────────── EntityBase <─┬─ RationalBSplineCurve
>                     └─ ICurve  <── ICurve3D <─┘
> ```

## Surfaces

This section describes the surface entities in IGES files. Surface entities are used to represent surfaces in 3D space.

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
