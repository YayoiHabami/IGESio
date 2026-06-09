# Plane (Type 108)

Defined at [surfaces/plane.h](./../../../include/igesio/entities/surfaces/plane.h)

## Table of Contents

- [Table of Contents](#table-of-contents)
- [Parameters](#parameters)
  - [Specific Parameters](#specific-parameters)
    - [`Plane` (Form 0)](#plane-form-0)
    - [`BoundedPlane` (Form 1 / -1)](#boundedplane-form-1---1)
  - [Plane Definition](#plane-definition)
    - [Plane Equation](#plane-equation)
    - [Definition Space Frame](#definition-space-frame)
  - [Form Numbers](#form-numbers)
- [Surface Definition](#surface-definition)
  - [Unbounded Plane (Form 0)](#unbounded-plane-form-0)
  - [Bounded Plane (Form 1 / -1)](#bounded-plane-form-1---1)
  - [Display Symbol](#display-symbol)
- [Appendix](#appendix)
  - [Handling of Infinite Parameter Ranges](#handling-of-infinite-parameter-ranges)

## Parameters

The plane entity (Type 108) is split into two classes by its form number. Form 0 is the unbounded plane `Plane`, whose domain is the entire plane, while Forms 1 and -1 are the bounded plane `BoundedPlane`, whose domain is restricted by a simple closed curve lying in the plane. In all cases, the plane is defined within definition space by four coefficients $A, B, C, D$.

### Specific Parameters

#### `Plane` (Form 0)

| Member Function | Description |
|---|---|
| `GetCoefficients()` <br> (`const std::array<double, 4>&`) | The plane coefficients $\lbrace A, B, C, D\rbrace$. |
| `GetFrame()` <br> (`PlaneFrame`) | The definition space frame (origin and basis vectors). See [Definition Space Frame](#definition-space-frame). |
| `GetSymbolLocation()` <br> (`Vector3d`) | The location point $(X, Y, Z)$ of the display symbol (display only). |
| `GetSymbolSize()` <br> (`double`) | The display symbol size $SIZE$ (0 means no symbol). |

#### `BoundedPlane` (Form 1 / -1)

| Member Function | Description |
|---|---|
| `GetCoefficients()` <br> (`const std::array<double, 4>&`) | The plane coefficients $\lbrace A, B, C, D\rbrace$. |
| `GetFrame()` <br> (`PlaneFrame`) | The definition space frame (origin and basis vectors). |
| `GetSymbolLocation()` <br> (`Vector3d`) | The location point $(X, Y, Z)$ of the display symbol (display only). |
| `GetSymbolSize()` <br> (`double`) | The display symbol size $SIZE$ (0 means no symbol). |
| `IsNegative()` <br> (`bool`) | Whether the region is negative (Form -1, a hole). |
| `GetBoundaryCurve()` <br> `SetBoundaryCurve(boundary)` <br> (`std::shared_ptr<const ICurve>`) | The simple closed curve ($PTR$) lying in the plane. |

### Plane Definition

#### Plane Equation

The plane is defined within definition space by four coefficients $A, B, C, D$. The following equation holds for every point on the plane with definition space coordinates $(X_T, Y_T, Z_T)$:

$$A \cdot X_T + B \cdot Y_T + C \cdot Z_T = D$$

Here, at least one of $A, B, C$ must be nonzero. The vector $(A, B, C)$ represents the normal direction of the plane. If all of $(A, B, C)$ are zero, the constructors and factory functions throw `igesio::EntityValueError`.

#### Definition Space Frame

`GetFrame()` returns the definition space frame `PlaneFrame`, which is uniquely determined from the plane coefficients. The frame consists of an origin $\text{origin}$, two in-plane unit vectors $e_u, e_v$, and a unit normal $\hat{n}$.

$$\hat{n} = \frac{(A, B, C)}{\|(A, B, C)\|}, \quad \text{origin} = \frac{D}{\|(A, B, C)\|^2} (A, B, C)$$

Here $\text{origin}$ is the foot of the perpendicular dropped from the definition space origin to the plane. $e_u$ is a unit vector orthogonal to $\hat{n}$, and $e_v = \hat{n} \times e_u$, forming a right-handed system that satisfies $e_u \times e_v = \hat{n}$.

### Form Numbers

The form number of Type 108 indicates whether the domain is restricted and, for a bounded region, its sense (positive or negative).

| Form | Class | Meaning | $PTR$ |
|:---:|---|---|---|
| $1$ | `BoundedPlane` | Bounded plane (positive region) | Pointer to the simple closed curve lying in the plane |
| $0$ | `Plane` | Unbounded plane | Null pointer (zero) |
| $-1$ | `BoundedPlane` | Bounded plane (negative region / hole) | Pointer to the simple closed curve lying in the plane |

Forms 1 and -1 share the same geometry; only the sense of the region differs. Form 1 treats the bounded planar region as positive (material), while Form -1 treats it as negative (a hole).

## Surface Definition

### Unbounded Plane (Form 0)

`Plane` parameterizes the surface in definition space using the definition space frame as follows:

$$S(u, v) = \text{origin} + u \cdot e_u + v \cdot e_v$$

The parameter range is infinite in both directions; that is, `GetParameterRange()` returns $\lbrace -\infty, +\infty, -\infty, +\infty \rbrace$. The partial derivatives are as follows, and `TryGetDefinedDerivatives` returns a value over the entire domain:

$$S_u = e_u, \quad S_v = e_v, \quad S^{(i, j)} = 0 \ (i + j \geq 2)$$

`GetDefinedBoundingBox()` returns a bounding box that treats the two in-plane directions ($e_u, e_v$) as infinite lines and sets the size in the normal direction to zero (a zero-thickness, two-dimensional infinite slab).

### Bounded Plane (Form 1 / -1)

`BoundedPlane` is a surface whose domain of the plane $S(u, v)$ is restricted by a simple closed curve ($PTR$) lying in the plane. This boundary curve must be a simple closed curve whose only coincident points are its start and end points. `BoundedPlane` has no inner boundaries (holes), so `GetInnerBoundaryCount()` always returns 0.

`BoundedPlane` implements `IRestrictedSurface`, handling its domain within the same framework as a trimmed surface. Specifically, it is constructed as follows:

- The base surface ($\text{GetBaseSurface}$) is a definition space `Plane` (identity transform) built from the same coefficients.
- The outer boundary in $(u, v)$ space ($\text{GetOuterUVBoundary}$) is constructed by discretizing the boundary curve in model space and projecting each point onto the model space frame via dot products to obtain $(u, v)$ values, forming a closed polyline.

Because the boundary curve lies on the plane in model space, this dot-product projection makes $S(B(t)) = C(t)$ hold exactly, where $B(t)$ is the boundary curve in $(u, v)$ space and $C(t)$ is the boundary curve in model space.

`GetDefinedBoundingBox()` returns the axis-aligned bounding box of the point set obtained by mapping the UV boundary samples into definition space. If the boundary curve is unresolved, it falls back to the bounding box of the base surface (the unbounded plane).

### Display Symbol

The parameters $X, Y, Z, SIZE$ are display-only parameters used to position a system-dependent display symbol within definition space; they do not affect the geometry of the plane itself. This library merely retains the values and preserves them across the read/write round trip. Setting $SIZE$ to zero (or omitting it) indicates that no display symbol is intended.

## Appendix

### Handling of Infinite Parameter Ranges

Because an unbounded plane has an infinite parameter range, sampling ranges must be clamped to a finite extent during mesh generation and intersection testing. For a standalone `Plane`, the shared constant `kInfiniteParamClamp` (`entities/interfaces/i_surface.h`) is used to clamp the infinite ends.

For restricted surfaces whose base surface has an infinite parameter range, such as `BoundedPlane`, `GetRestrictedDomainUVBounds` (`entities/interfaces/i_restricted_surface.h`) returns a finite parameter window: the UV bounding box of the domain (the containment polygons of the outer and inner boundaries) plus a margin. This yields a grid range and intersection sampling range that match the bounded region.
