# Coordinate Frames and Transform Views

## Table of Contents

- [Table of Contents](#table-of-contents)
- [Overview](#overview)
- [Transform Chain and Frames](#transform-chain-and-frames)
- [Placement-Aware Query API at the Entity Layer](#placement-aware-query-api-at-the-entity-layer)
- [Frame-Specification API at the Assembly Layer](#frame-specification-api-at-the-assembly-layer)
- [Transform Views (CurveView / SurfaceView)](#transform-views-curveview--surfaceview)
- [Persisting Placement (Flatten / Materialize)](#persisting-placement-flatten--materialize)
- [Example (Generating a Cutter-Contact Point Sequence)](#example-generating-a-cutter-contact-point-sequence)

## Overview

One of the main uses of IGESio is CAM, where curves and surfaces are often handled individually (fixing $u$ and sweeping $v$ to generate a sequence of cutter-contact points, discretizing a surface, and so on). For this reason, IGESio emphasizes that geometric queries can be made in the same way whether an entity is placed in an [assembly hierarchy](./assembly.md) or used standalone.

To this end, three mechanisms are provided.

- Placement-aware query API at the entity layer: adds an argument to the query functions of `ICurve` / `ISurface` that post-applies a placement matrix.
- Frame-specification API at the Assembly layer: the desired coordinate system is specified with `CoordFrame`, and `Assembly` resolves the placement matrix.
- Transform views: generate an `ICurve` / `ISurface` with the placement folded in, without modifying the original entity.

## Transform Chain and Frames

The transform chain from an entity's definition space to world space is as follows. Here $A_n$ denotes the nearest owning Assembly and $A_\text{root}$ denotes the topmost one.

$$ P_\text{world} = G_\text{root} \cdots G_{k+1} \cdots G_n \cdot M_\text{entity} \cdot P_\text{def} $$

- $P_\text{def}$ : the value in definition space
- $M_\text{entity}$ : the entity's own DE transform (124)
- $G_i$ : the global transform of the `Assembly` node $A_i$ on the path

The desired frame is a ladder in which the transforms to apply line up.

| Frame | Transform applied | Meaning |
| --- | --- | --- |
| Definition space | none | $P_\text{def}$ itself |
| Entity itself | $M_\text{entity}$ | only the entity's own DE transform; Assembly-independent |
| Definition space of ancestor $A_k$ | $M_\text{entity} \cdot G_n \cdots G_{k+1}$ | coordinates under $A_k$ (excluding $A_k$ 's own $G_k$ ) |
| World/model space | the full chain | the final placement |

Note that the "definition space of ancestor $A_k$ " does not include $A_k$ 's own global transform $G_k$ . When $A_k$ is the reference, the transforms applied are only those that place things under $A_k$ , and not $G_k$ , which places $A_k$ itself into the parent frame. To obtain world coordinates, take the root as the reference and include all the $G$ transforms up to the root.

## Placement-Aware Query API at the Entity Layer

The query functions of `ICurve` / `ISurface` consist of two families: a definition-space family and a placement-aware family. The naming is fixed as follows, and the difference in frame is expressed by arguments.

- `TryGetDefinedXxx()`: returns the definition space ( $P_\text{def}$ ).
- `TryGetXxx()`: returns the value with $M_\text{entity}$ applied (real space).
- `TryGetXxx(..., placement)`: post-applies the placement matrix `placement` to the value after $M_\text{entity}$ . The return value is $\text{placement} \cdot (M_\text{entity} \cdot P_\text{def})$ .

The default `placement` is the identity matrix, in which case the behavior matches the case where only $M_\text{entity}$ is applied. Points and vectors are transformed differently: a position (point) is transformed as $R \cdot p + T$ , and each-order derivative (vector) as $R \cdot v$ . No scale (equivalent to a 124 transform) is assumed.

```cpp
// Definition space
auto p_def = curve.TryGetDefinedPointAt(t);
// After M_entity (real space)
auto p_local = curve.TryGetPointAt(t);
// Post-apply placement (placement·M_entity·P_def)
auto p_placed = curve.TryGetPointAt(t, placement);
```

Derivatives likewise have a definition-space version `TryGetDerivatives(t, n)` and a placement-aware version `TryGetDerivatives(t, n, placement)`. The definition-space version returns the derivatives in definition space, and the placement-aware version returns the values transformed with order 0 as a point and order 1 and above as vectors.

## Frame-Specification API at the Assembly Layer

For a standalone entity, the reference layer must be specified as a matrix, but in a multi-level assembly it is easy to confuse that matrix (whether or not $G_k$ is included). The `Assembly` layer therefore introduces `CoordFrame`, which states the frame as a value, and `Assembly` resolves the placement matrix.

`CoordFrame` (`include/igesio/models/assembly.h`) is constructed with static factory functions.

| Factory function | Frame | Kind (`Kind`) |
| --- | --- | --- |
| `CoordFrame::Definition()` | Definition space | `kDefinition` |
| `CoordFrame::EntityLocal()` | Entity itself (only $M_\text{entity}$ ) | `kEntityLocal` |
| `CoordFrame::World()` | World/model space | `kWorld` |
| `CoordFrame::RelativeTo(base_id)` | Definition space of the specified Assembly (excluding its $G_k$ ) | `kRelative` |

The reference Assembly is held as an `ObjectID` rather than a raw pointer (to avoid dangling). `Assembly` walks the parent links upward from the owning Assembly and resolves the product of the $G$ transforms on the path by ID equality comparison.

`ResolvePlacement(id, frame)` returns the placement matrix to post-apply to the value after $M_\text{entity}$ for that frame. It returns `std::nullopt` if `id` is not in the tree, or if `frame` is `kDefinition` (which has no placement concept). It throws `std::invalid_argument` if the reference of `RelativeTo` is not an ancestor of the owning Assembly.

```cpp
// Resolve the world placement and pass it to an entity query
auto placement = root.ResolvePlacement(id, igesio::models::CoordFrame::World());
if (placement) {
    auto p_world = curve.TryGetPointAt(t, *placement);
}
```

## Transform Views (CurveView / SurfaceView)

Because the same entity may be obtained from multiple layers in different frames simultaneously, the placement must be expressed without modifying the original entity. To this end, transform views (the Decorator pattern) that inherit only from `ICurve` / `ISurface` are provided (`include/igesio/entities/views/`).

The properties of `CurveView` / `SurfaceView` are as follows.

- Non-destructive: the original entity is held only as a shared reference (`std::shared_ptr<const ICurve>`, etc.) and is not modified. Simultaneous acquisition from multiple frames is safe.
- No type explosion: because they depend only on `ICurve` / `ISurface`, two classes cover all curve and surface types.
- Substitutable: because they share the same interface, they can be passed directly to existing intersection, sampling, and discretization algorithms that take `const ICurve&` / `const ISurface&`.
- Read-only, snapshot semantics: they hold the cumulative transform `placement_` fixed at construction time. They do not follow later changes to the Assembly transform (recreate them if needed). They have no shape-editing API.
- Unique ID and delegation: a view holds its own `ObjectID` (`kEntityView`), but delegates its type and form number to the original entity. The original entity's ID is obtained via `GetSourceID()` (an ordinary entity returns the same value as `GetID()`). `GetSourceID()` is used to identify the original entity from a picking result (a view) and to aggregate selection.

The "definition space" of a view refers to the coordinates after applying the original entity's $M_\text{entity}$ (the local frame of the owning Assembly). `TryGet*` post-applies `placement_` to this and returns $\text{placement\_} \cdot (M_\text{entity} \cdot P_\text{def})$ . When a view of a view is created, the matrices are folded together and reconnected to the original `ICurve` / `ISurface`.

Views are generated by the factory functions of `Assembly`.

| Member function | Content |
| --- | --- |
| `GetCurveView(id, frame)` | Generate a `CurveView` for the specified frame (`nullptr` if not a curve) |
| `GetSurfaceView(id, frame)` | Generate a `SurfaceView` for the specified frame (`nullptr` if not a surface) |

If `kDefinition` is specified for `frame`, `std::invalid_argument` is thrown (because a view assumes $M_\text{entity}$ has been applied).

## Persisting Placement (Flatten / Materialize)

`Assembly` and transform views are not written to an IGES file (non-persistent). To persist the placement of an assembly to a file, fold the global transform into the entity's DE transform (124). This is provided by `include/igesio/models/flatten.h`.

- `Materialize(entity, placement)`: clones the entity and folds `placement` into the DE transform (124). If `placement` is the identity matrix, the original 124 is shared (no new 124); otherwise a new 124 $= \text{placement} \cdot M_\text{entity}$ is generated and the clone's transform is replaced. The clone is a shallow copy by `entities::CloneEntity` (references are shared with the original) and is assigned a new ID.
- `Flatten(src)`: folds the placement of the `Assembly` hierarchy and returns a flat `IgesData` with no child `Assembly` nodes. Only independent (non-physically-dependent) entities are materialized at the world placement of their owning Assembly; physically dependent children follow via the parent's folded 124. Because shared 124 transforms are individualized by `Materialize`, the DAG structure naturally resolves into a tree.

Both assume no scale (equivalent to a 124 transform). An entity realized by `Materialize` becomes an ordinary entity and is handled normally by the writer's ID→DE-pointer assignment.

## Example (Generating a Cutter-Contact Point Sequence)

A transform view is well suited to the "generate once, sample many times" use case. Once a view at the world placement is generated, it can be passed directly to an existing sampler as a `const ISurface&`.

```cpp
// Obtain the surface as a SurfaceView at the world placement
auto frame = igesio::models::CoordFrame::World();
auto surface = root.GetSurfaceView(surface_id, frame);
if (!surface) return;

// Fix u and sweep v to get a contact point sequence (*surface is const ISurface&)
const double u = 0.5;
for (int i = 0; i <= n; ++i) {
    const double v = static_cast<double>(i) / n;
    if (auto p = surface->TryGetPointAt(u, v)) {
        // *p is in world coordinates
    }
}
```

Because a view is a snapshot, it does not follow changes to the Assembly transform made after construction. If the placement changes, recreate the view.
