# Trimmed (Parametric) Surface (Type 144)

Defined at [surfaces/trimmed_surface.h](./../../../include/igesio/entities/surfaces/trimmed_surface.h)

## Table of Contents

- [Table of Contents](#table of Contents)
- [Parameters](#parameters)
  - [Specific Parameters](#specific-parameters)
  - [Effective Domain](#effective-domain)
    - [Parameter Rectangle D](#parameter-rectangle-d)
    - [Outer and Inner Boundaries](#outer-and-inner-boundaries)
    - [Effective Domain Ω](#effective-domain-ω)
  - [Surface Definition](#surface-definition)
- [Appendix](#appendix)
  - [Implementation of Containment Testing](#implementation-of-containment-testing)

## Parameters

### Specific Parameters

| Member Function | Description |
|---|---|
| `GetSurface()` <br> `SetSurface(surface)` <br> (`std::shared_ptr<const ISurface>`) | Pointer to the surface $S(u, v)$ to be trimmed. |
| `IsOuterBoundaryOfD()` <br> (`bool`) | Returns `true` if the outer boundary coincides with the boundary of the parameter rectangle $D$ ($N_1 = 0$). |
| `GetOuterBoundary()` <br> `SetOuterBoundary(outer)` <br> (`std::shared_ptr<const CurveOnAParametricSurface>`) | Pointer to the outer boundary curve (Type 142). Returns `nullptr` if `IsOuterBoundaryOfD()` is `true`. |
| `GetInnerBoundaryCount()` <br> (`size_t`) | The number of inner boundary curves, $N_2$. |
| `GetInnerBoundaryAt(index)` <br> `AddInnerBoundary(boundary)` <br> `RemoveInnerBoundaryAt(index)` <br> (`std::shared_ptr<const CurveOnAParametricSurface>`) | Pointer to an inner boundary curve (Type 142). Used to get, add, or remove the inner boundary at the specified 0-based `index`. |

### Effective Domain

#### Parameter Rectangle D

The surface $S(u, v)$ to be trimmed has a rectangular domain $D$ defined by the parameter ranges $[a, b] \times [c, d]$. Specifically:

$$D = \{\ (u, v)\ |\ a \leq u \leq b,\ c \leq v \leq d\ \}$$

#### Outer and Inner Boundaries

The effective domain of a trimmed surface is defined by its outer and inner boundaries. Every boundary curve is a simple closed curve in the $(u, v)$ parameter space, represented as a [`CurveOnAParametricSurface (Type 142)`](../curves/142_curve_on_a_parametric_surface.md).

The specification of the outer boundary depends on the value of $N_1$:

- **$N_1 = 0$ (`IsOuterBoundaryOfD()` = `true`)**: The outer boundary is identical to the boundary of the parameter rectangle $D$. In this case, the pointer to the outer boundary curve is null.
- **$N_1 = 1$ (`IsOuterBoundaryOfD()` = `false`)**: The outer boundary is given as a simple closed curve $\partial\Omega_{\text{out}}$ defined within $D$.

The inner boundaries consist of $N_2$ simple closed curves $\partial\Omega_{\text{in},1},\ \ldots,\ \partial\Omega_{\text{in},N_2}$. Each inner boundary must satisfy the following conditions:

- The closed regions of the inner boundaries must be mutually disjoint (they do not share any points).
- Each inner boundary must be completely contained within the interior of the outer boundary.

#### Effective Domain Ω

Let $\mathrm{int}(C)$ denote the interior region of a simple closed curve $C$, and $\mathrm{ext}(C)$ denote its exterior region. The effective domain $\Omega$ of the trimmed surface is defined as follows:

$$\Omega = \overline{\mathrm{int}(\partial\Omega_{\text{out}})} \cap \bigcap_{i=1}^{N_2} \overline{\mathrm{ext}(\partial\Omega_{\text{in},i})}$$

Where $\overline{(\cdot)}$ represents the closure (the closed region including the boundary curve). In other words, $\Omega$ is the region inside the outer boundary and outside all inner boundaries (including the boundaries themselves).

If $N_1 = 0$ and $N_2 = 0$ (meaning the outer boundary is $D$ and there are no inner boundaries), the effective domain is $\Omega = D$, which is equivalent to an untrimmed surface.

### Surface Definition

The parameterization $S(u, v)$ of a trimmed surface is identical to that of its underlying surface. The `TryGetDerivatives` method of `TrimmedSurface` determines whether a given point $(u, v)$ lies within the effective domain $\Omega$. If it is inside, the method returns the partial derivatives of the underlying surface; otherwise, it returns `std::nullopt`.

```
TryGetDerivatives(u, v, n)
  ├─ If (u, v) is outside Ω → return std::nullopt
  └─ If (u, v) is inside Ω  → delegate to surface_->TryGetDerivatives(u, v, n)
```

`GetParameterRange()` returns the parameter range of the underlying surface, as trimming does not alter the parameterization.

`GetDefinedBoundingBox()` returns the bounding box of the underlying surface. While this may be larger than the actual 3D region after trimming, it serves as a valid conservative upper bound.

## Appendix

### Implementation of Containment Testing

The `IsInTrimmedDomain(u, v)` function, called internally by `TryGetDerivatives`, determines if a point in the $(u, v)$ parameter space lies within the effective domain $\Omega$. This test utilizes `ComputeContainmentPolygons` (`entities/curves/algorithms.h`) and `IsPointInPolygon` (`numerics/polygon.h`).

The logic flow is as follows:

```
IsInTrimmedDomain(u, v)
  │
  ├─ If N1=0 and N2=0 → Always return true (fastest path)
  │
  ├─ If domain_cache_ is not built → Call BuildDomainCache()
  │    ├─ If N1=1:
  │    │    outer_boundary_.GetBaseCurve() → Get B_out(t)
  │    │    ComputeContainmentPolygons(B_out, 32, (0,0,1)) → Store in cache.outer
  │    └─ For each inner_boundaries_[i]:
  │         inner.GetBaseCurve() → Get B_in_i(t)
  │         ComputeContainmentPolygons(B_in_i, 32, (0,0,1)) → Store in cache.inner[i]
  │
  ├─ If N1=1:
  │    If IsPointInPolygon((u, v, 0), cache.outer) is false → return false (outside outer boundary)
  │
  └─ For each inner boundary:
       If IsPointInPolygon((u, v, 0), cache.inner[i]) is true → return false (inside a hole)

  If none of the above conditions are met → return true
```

The `DomainCache` generated by `BuildDomainCache()` is constructed only during the first containment test and is reused thereafter (lazy initialization). If the surface or boundary curves are modified, the cache is invalidated and reconstructed during the next `IsInTrimmedDomain` call.

Note that the base curve $B(t) = (u(t), v(t))$ of a boundary curve exists in the $(u, v)$ parameter space. Consequently, containment testing is performed within the parameter space rather than 3D space. The normal vector passed to `ComputeContainmentPolygons` is $(0, 0, 1)$, treating the parameter space as the $z=0$ plane.