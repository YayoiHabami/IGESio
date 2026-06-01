# Assembly (Hierarchical Entity Grouping)

## Table of Contents

- [Table of Contents](#table-of-contents)
- [Overview](#overview)
- [Structure of Assembly](#structure-of-assembly)
- [Two Layers: Membership and Reference](#two-layers-membership-and-reference)
- [What Is Not Placed in the Logical Tree](#what-is-not-placed-in-the-logical-tree)
- [Querying Children](#querying-children)
- [Structural Editing](#structural-editing)
- [Spatial Query](#spatial-query)
- [Relationship with Scene](#relationship-with-scene)
- [Relationship with Input/Output](#relationship-with-inputoutput)

## Overview

`Assembly` is a class in the `igesio::models` namespace that is responsible for grouping multiple entities as a "collection". It is a recursive tree node and expresses a nested structure (a logical tree) through child `Assembly` nodes. It is an in-memory structure and is not serialized directly to an IGES file.

For coordinate systems and placement (transform views, frame specification, and flattening on write), see [Coordinate Frames and Transform Views](./coordinate_frames.md).

> The similar class `IgesData` also carries information specific to an IGES file (the Start and Global sections). The responsibility of being a collection of entities is held by `Assembly`, while `IgesData` serves as a container for IGES file I/O. `IgesData` is a wrapper that holds the following elements.
>
> - `description`: the explanatory text corresponding to the Start section of the IGES file
> - `global_section`: the parameters of the Global section (`GlobalParam`)
> - the root `Assembly`: holds the collection of entities (composed as `std::shared_ptr<Assembly>`, not inherited)
>
> Adding, retrieving, resolving references for, and validating entities are all performed through the root `Assembly`.
>
> ```cpp
> igesio::models::IgesData iges = igesio::ReadIges("model.iges");
>
> // Operate on entities through the root Assembly
> auto& root = iges.Root();
> auto ids = root.GetEntityIDs(/*recursive=*/true);
> auto entity = root.GetEntity(ids.front());
> ```
>
> `IgesData` holds the root `Assembly` as a `shared_ptr` and can share it via `RootPtr()`. The [`Scene`](#relationship-with-scene), which carries runtime session state (such as selection), is constructed by sharing this root.

## Structure of Assembly

`Assembly` (`include/igesio/models/assembly.h`) holds the following.

- A flat map of owned entities `std::unordered_map<ObjectID, std::shared_ptr<entities::EntityBase>>`
- A list of child `Assembly` nodes (physical ownership; nesting = the logical tree)
- A weak reference to the parent `Assembly` (`std::weak_ptr`; empty at the root; used for path traversal)
- A global transform matrix `Matrix4d` (placement equivalent to Solid Assembly Type 184; the identity matrix by default)
- Metadata `AssemblyMetadata`

The metadata `AssemblyMetadata` is auxiliary information referenced during rendering and editing.

| Member | Meaning |
| --- | --- |
| `name` | The name of the assembly |
| `visible` | Visibility (display toggle). Even when hidden, it exists logically and is included in bounding box, validation, output, and queries |
| `suppressed` | Suppression. Logically excludes the node from the model (cascades to descendants). When suppressed, it is not drawn and is also excluded from bounding box, validation, output, and queries |
| `role_tag` | An arbitrary classification string indicating a role |
| `lock` | Lock state (`selectable` / `editable`) |
| `color_override` | Color override (RGB; $[0, 1]$ ). If unset, the member's color is used |
| `opacity_override` | Opacity override ( $[0, 1]$ ). If unset, the member's value is used |

An entity is drawn when `visible` and `!suppressed`.

## Two Layers: Membership and Reference

One entity belongs to exactly one `Assembly` (exclusive ownership = a strict tree). As a consequence, the following two layers are clearly separated.

- Membership: registration in the flat map. An entity belongs exclusively to one `Assembly`.
- Reference: pointers in DE/PD fields. These can be set regardless of tree position.

The reverse lookup from an entity ID to its owning `Assembly` is resolved in O(1) by `FindOwner`, using the reverse-lookup index (`ObjectID → Assembly*`) held only by the root `Assembly`. It is updated only during structural editing.

```cpp
// From an entity ID (e.g. a picking result), find the owning Assembly
igesio::models::Assembly* owner = root.FindOwner(picked_id);
```

An entity does not hold a pointer to its parent `Assembly`. `EntityBase` belongs to `igesio::entities` and `Assembly` belongs to `igesio::models`; if an entity pointed to an `Assembly`, the one-directional dependency `models → entities` would be reversed, coupling the geometry layer to the container layer. Even with exclusive ownership, path traversal up to the root is satisfied by the parent links between `Assembly` nodes (closed within the models layer), so a back-pointer is unnecessary.

## What Is Not Placed in the Logical Tree

Physically dependent children (the constituents of a composite curve, the surface of a Trimmed Surface 144, the B-Rep chain 186→…→502, the dependent elements of dimensions, and so on) are not made direct members of an `Assembly`. They are reached via the parent entity's `GetChildIDs` or via shared ownership. The logical tree (child `Assembly` nodes) represents only independent (`kIndependent`) entities and explicitly grouped or instanced bundles.

Shared definition entities (Transformation Matrix 124, Color Definition 314, Property 406) are placed directly under the root and referenced from anywhere in the tree.

## Querying Children

Enumeration, filtering, and predicate search of owned entities all switch between "direct children only" and "all descendants" via the `recursive` argument (default `false`).

| Member function | Content |
| --- | --- |
| `GetEntityIDs(recursive)` | List of IDs of owned entities |
| `FindEntitiesByType(type, recursive)` | Entities of the specified `EntityType` |
| `FindEntitiesByUseFlag(flag, recursive)` | Entities with the specified `EntityUseFlag` |
| `FindEntities(predicate, recursive)` | Entities matching a predicate |
| `GetEntity(id)` | A directly owned entity (`nullptr` if the ID is absent) |
| `GetEntities()` | The map of directly owned entities |

Reference resolution and validation are also handled by `Assembly` (`AreAllReferencesSet`, `GetUnresolvedReferences`, `IsReady`, `Validate`). These target the current node only (non-recursive). It is an invariant that each `Assembly` can be validated in a self-contained manner.

## Structural Editing

The structural editing API specifies, via `RemovalPolicy`, how to handle an entity that is still referenced by others at deletion time.

| `RemovalPolicy` | Meaning |
| --- | --- |
| `kReject` | Reject deletion if a reference remains (the default that preserves self-containedness) |
| `kCascade` | Also cascade-delete the referrers and physically dependent children |
| `kOrphan` | Delete while leaving references unresolved (the referencing `weak_ptr` expires naturally) |

Use `FindReferrers(id)` to search for referrers (a reverse search that gathers the IDs of all entities pointing to `id`).

The main editing members are shown below.

| Member function | Content |
| --- | --- |
| `RemoveEntity(id, policy)` | Delete an entity (targets this subtree) |
| `RemoveChildAssembly(child_id, policy)` | Delete a direct child `Assembly` |
| `Clear()` | Remove all entities and all child `Assembly` nodes of this node (bulk reset) |
| `MoveEntityTo(id, dest)` | Move an entity to another node (must share the same root) |
| `MoveChildAssemblyTo(child_id, dest)` | Move a direct child `Assembly` to another node |
| `AddChildAssembly(child)` | Add a child `Assembly` |
| `SetVisibleRecursive(visible)` | Set visibility for this node and all descendants at once |
| `SetSuppressedRecursive(suppressed)` | Set suppression for this node and all descendants at once |
| `SetColorOverrideRecursive(color)` | Set the color override for this node and all descendants at once |
| `SetOpacityOverrideRecursive(opacity)` | Set the opacity override for this node and all descendants at once |
| `ComposeGlobalTransform(transform)` | Compose an additional transform onto this node's global transform (non-recursive) |
| `ValidateSelfContainedRecursive()` | Recursively verify that each node can resolve its references in a self-contained manner |

`ComposeGlobalTransform` computes `global_transform_ = transform * global_transform_` (post-application in the parent frame) and does not recurse, because the change propagates to descendants naturally through the cumulative transform at render time. The transform is assumed to have no scale (equivalent to a 124 transform).

`ValidateSelfContainedRecursive` is the recursive version of `GetUnresolvedReferences` and is used as a postcondition (the self-containedness invariant) of each edit.

Lock enforcement is handled by `IsEffectivelySelectable(id)` and `IsEffectivelyEditable(id)`. These fold `lock.selectable` / `lock.editable` from the owning node up to the root with AND toward the ancestors. An ID not in the tree is treated as unrestricted (`true`).

Note that re-walking for rendering after structural editing is the caller's responsibility (`Assembly` does not know about the renderer; see [Relationship with Scene](#relationship-with-scene)).

## Spatial Query

`GetWorldBoundingBox()` returns an axis-aligned bounding box that encloses the geometric members of the descendants, computed in world space (applying all global transforms up to the root, including this node). It targets only geometric, non-physically-dependent members, and excludes degenerate bounding boxes (point- or line-shaped) following the existing guard. It returns `std::nullopt` if there are no geometric members, or if the whole is degenerate and cannot form an AABB.

## Relationship with Scene

`Assembly` holds the model structure (tree, ownership, placement, metadata). On the other hand, the session state (selection, selection granularity, pick filter) is managed by `Scene` (`include/igesio/models/scene.h`). `Assembly` itself does not hold selection state.

- `Scene` is constructed by sharing the root `Assembly` of `IgesData` as a `shared_ptr`.
- The rendering layer (`EntityRenderer`) holds a non-owning (const) reference to `Scene` and pulls the tree and selection at render time.
- Assembly-granularity selection (selecting the owning group of an entity at once) is realized by `Scene::SelectOwningAssembly` / `DeselectOwningAssembly` using `FindOwner` and `GetEntityIDs(recursive)`.

For details of selection and rendering, see [Rendering System Overview](../graphics/overview.md) (in Japanese).

## Relationship with Input/Output

By default (no structural editing), the loaded original entities are written out flat as they are. `Assembly` and transform views are not written to the file (non-persistent). Because all entities remain in the flat map, round-trip fidelity is preserved automatically.

To persist the placement of an assembly, use `Flatten` / `Materialize`, which fold the global transform into the entity's DE transform (124). For details, see [Coordinate Frames and Transform Views](./coordinate_frames.md).
