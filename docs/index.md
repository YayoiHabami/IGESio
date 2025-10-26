# Index

## Table of Contents

- [Table of Contents](#table-of-contents)
- [Key Documents](#key-documents)
- [Module-Specific Documents](#module-specific-documents)
  - [common module](#common-module)
  - [entities module](#entities-module)
    - [Individual Entity Classes](#individual-entity-classes)
  - [graphics module](#graphics-module)
  - [models module](#models-module)
  - [numerics module](#numerics-module)
  - [utils module](#utils-module)
- [Other Documents](#other-documents)

## Key Documents

The following documents are particularly important. Please refer to these first.

- **[Build Instructions](build.md)**: How to build this library
- **[Examples](examples.md)**: Overview of the sample code in the `examples` directory
- **[Overview of Classes Used](./class_structure.md)**: Explanation of the relationships of the main classes used in this library
- **[Implementation Progress](./implementation_progress.md)**: Overview of the implementation status of various components

## Module-Specific Documents

The following are module-specific documents. For files not listed, please refer to the comments in the source code.

### common module

- **[Matrix](common/matrix.md)**: Fixed/dynamic size matrix classes

### entities module

- **[Overview of Entity Classes](entities/entities.md)**
    - Explanation of the relationships between all entity classes based on `IEntityIdentifier`.
    - Description of `EntityBase`, the base class for individual entity classes.
- **[Individual Entity Classes](entities/entities.md)**
    - Explanation of each interface.
    - Description of each entity class (code examples, diagrams, etc.).
- **[Geometric Properties of Geometries](entities/geometric_properties_en.md)**
    - Description of various geometric properties that can be calculated for curve and surface entities.
    - Also describes how to calculate various properties (code examples).
- **[Intermediate Data Structure](intermediate_data_structure.md)**
    - Explanation of the intermediate data structure used internally when inputting/outputting IGES files in this library.
    - Intermediate data structure of the Directory Entry section and Parameter Data section of IGES files.
- **[DE Field](entities/de_field.md)**
    - Brief explanation of the Directory Entry section.
    - Explanation of the class structure for handling DE fields.

#### Individual Entity Classes

Individual entity class documents are as follows. Currently, they include mathematical definitions of each parametric curve/surface and definitions of their derivatives/partial derivatives.

| Type | Document |
|---|---|
| curves | [Circular Arc (Type 100)](entities/curves/100_circular_arc.md) <br> Circle/arc entity |
|   ^    | [Composite Curve (Type 102)](entities/curves/102_composite_curve.md) <br> Entity that can combine multiple curves (`ICurve` derived classes) |
|   ^    | [Conic Arc (Type 104)](entities/curves/104_conic_arc.md) <br> Conic curve entity (ellipse, parabola, hyperbola) |
|   ^    | [Copious Data (Type 106)](entities/curves/106_copious_data.md) <br> Point sequence/polyline entity, can also set additional vectors for each point/vertex |
|   ^    | [Line (Type 110)](entities/curves/110_line.md) <br> Line segment, ray, or straight line entity |
|   ^    | [Parametric Spline Curve (Type 112)](entities/curves/112_parametric_spline_curve.md) <br> Curve entity consisting of multiple polynomial segments (up to cubic) |
| surfaces | [Ruled Surface (Type 118)](entities/surfaces/118_ruled_surface.md) <br> Ruled surface entity defined by connecting two curves with straight lines |
|    ^     | [Surface of Revolution (Type 120)](entities/surfaces/120_surface_of_revolution.md) <br> Surface of revolution entity defined by rotating a curve around an axis |
|    ^     | [Tabulated Cylinder (Type 122)](entities/surfaces/122_tabulated_cylinder.md) <br> Surface entity defined by translating a generatrix (curve) in a fixed direction |
|    ^     | [Rational B-Spline Surface (Type 128)](entities/surfaces/128_rational_b_spline_surface.md) <br> Rational B-spline surface entity (including NURBS surfaces) |

### graphics module

- Currently, there are no documents.

### models module

- **[Intermediate Data Structure](intermediate_data_structure.md)**: Intermediate data structure when inputting/outputting IGES files

### numerics module

- Currently, there are no documents.

### utils module

- Currently, there are no documents.

## Other Documents

- **[Policy (ja)](policy_ja.md)**: About the library's design policy and interpretation of IGES specifications
- **[Flow/Reader (ja)](flow/reader_ja.md)**: About the flow of the reading process
- **[Entity Analysis (en)](entity-analysis.md)**: Analysis of the classification and parameters of each entity in IGES 5.3
- **[Additional Notes (ja)](additional_notes_ja.md)**: Other supplementary notes
- **[TODO (ja)](todo.md)**: TODO list
