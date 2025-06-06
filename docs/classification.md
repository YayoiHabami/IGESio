# Classification of IGES Entities

```plaintext
IGES Entity Classification
├── 1. Structure Entities
│   ├── 1.1 Basic Structure
│   │   ├── 000 - Null
│   │   ├── 123 - Direction
│   │   ├── 132 - Connect Point ✅
│   │   └── 134 - Node ✅
│   ├── 1.2 Analysis Structure
│   │   ├── 136 - Finite Element ✅
│   │   ├── 138 - Nodal Displacement and Rotation
│   │   ├── 146 - Nodal Results ✅
│   │   └── 148 - Element Results ✅
│   ├── 1.3 Definition Structure
│   │   ├── 302 - Associativity Definition
│   │   ├── 304 - Line Font Definition ✅
│   │   ├── 306 - Macro Definition
│   │   ├── 308 - Subfigure Definition ✅
│   │   ├── 310 - Text Font ✅
│   │   ├── 312 - Text Display Template
│   │   ├── 314 - Color Definition
│   │   ├── 316 - Units Data
│   │   ├── 320 - Network Subfigure Definition ✅
│   │   └── 322 - Attribute Table Definition ✅
│   └── 1.4 Instance Structure
│       ├── 402 - Associativity Instance ✅
│       ├── 404 - Drawing ✅
│       ├── 406 - Property
│       ├── 408 - Singular Subfigure Instance ✅
│       ├── 410 - View ✅
│       ├── 412 - Rectangular Array Subfigure Instance ✅
│       ├── 414 - Circular Array Subfigure Instance
│       ├── 416 - External Reference
│       ├── 418 - Nodal Load Constraint ✅
│       ├── 420 - Network Subfigure Instance ✅
│       └── 422 - Attribute Table Instance
├── 2. Geometry Entities
│   ├── 2.1 Curve and Surface Geometry (C&S)
│   │   ├── 2.1.1 Basic Curves
│   │   │   ├── 100 - Circular Arc
│   │   │   ├── 104 - Conic Arc
│   │   │   ├── 110 - Line
│   │   │   └── 116 - Point ✅
│   │   ├── 2.1.2 Advanced Curves
│   │   │   ├── 102 - Composite Curve ✅
│   │   │   ├── 112 - Parametric Spline Curve
│   │   │   ├── 126 - Rational B Spline Curve
│   │   │   ├── 130 - Offset Curve ✅
│   │   │   └── 142 - Curve on a Parametric Surface
│   │   ├── 2.1.3 Basic Surfaces
│   │   │   ├── 108 - Plane ✅
│   │   │   ├── 114 - Parametric Spline Surface
│   │   │   ├── 118 - Ruled Surface ✅
│   │   │   ├── 120 - Surface Of Revolution
│   │   │   ├── 122 - Tabulated Cylinder
│   │   │   ├── 128 - Rational B Spline Surface
│   │   │   ├── 140 - Offset Surface
│   │   │   ├── 190 - Plane Surface
│   │   │   ├── 192 - Right Circular Cylinder Surface
│   │   │   ├── 194 - Right Circular Conical Surface
│   │   │   ├── 196 - Spherical Surface
│   │   │   └── 198 - Toroidal Surface
│   │   ├── 2.1.4 Bounded/Composite Surfaces
│   │   │   ├── 141 - Boundary
│   │   │   ├── 143 - Bounded Surface
│   │   │   └── 144 - Trimmed Surface ✅
│   │   └── 2.1.5 Utility Entities
│   │       ├── 124 - Transformation Matrix
│   │       └── 125 - Flash ✅
│   ├── 2.2 Constructive Solid Geometry (CSG)
│   │   ├── 2.2.1 Primitive Solids
│   │   │   ├── 150 - Block
│   │   │   ├── 152 - Right Angular Wedge
│   │   │   ├── 154 - Right Circular Cylinder
│   │   │   ├── 156 - Right Circular Cone
│   │   │   ├── 158 - Sphere
│   │   │   ├── 160 - Torus
│   │   │   └── 168 - Ellipsoid
│   │   ├── 2.2.2 Derived Solids
│   │   │   ├── 162 - Solid Of Revolution ✅
│   │   │   └── 164 - Solid Of Linear Extrusion
│   │   └── 2.2.3 Boolean Operations
│   │       ├── 180 - Boolean Tree ✅
│   │       ├── 182 - Selected Component ✅
│   │       ├── 184 - Solid Assembly
│   │       └── 430 - Solid Instance ✅
│   └── 2.3 Boundary Representation (B-Rep)
│       ├── 502 - Vertex
│       ├── 504 - Edge ✅
│       ├── 508 - Loop
│       ├── 510 - Face
│       ├── 514 - Shell
│       └── 186 - Manifold Solid B-Rep Object
└── 3. Annotation Entities
    ├── 3.1 Dimensions
    │   ├── 202 - Angular Dimension ✅
    │   ├── 204 - Curve Dimension
    │   ├── 206 - Diameter Dimension
    │   ├── 216 - Linear Dimension ✅
    │   ├── 218 - Ordinate Dimension ✅
    │   ├── 220 - Point Dimension
    │   └── 222 - Radius Dimension ✅
    ├── 3.2 Text and Labels
    │   ├── 208 - Flag Note ✅
    │   ├── 210 - General Label
    │   ├── 212 - General Note ✅
    │   └── 213 - New General Note
    ├── 3.3 Symbols and Graphics
    │   ├── 214 - Leader Arrow
    │   ├── 228 - General Symbol ✅
    │   └── 230 - Sectioned Area
    └── 3.4 Special Annotation
        └── 106 - Copious Data (annotation when form #20-40, C&S otherwise)
```

Legend:
✅ = Entity references other entities (Pointer column)


## 2. Geometry Entities

### 2.1 Curve and Surface Geometry (C&S)

| Class | Entity Type | 2D/3D | Parameters |
| --- | --- | --- | --- |
| 2.1.1<br>Basic<br>Curves | 100 Circular Arc | 2D | 円弧中心、開始点、終了点
| ^ | 104 Conic Arc | 2D | 係数、開始点、終了点
| ^ | 110 Line | 3D | 始点、終点 (無限: Forms 1-2)
| ^ | 116 Point |
| 2.1.2<br>Advanced<br>Curves | 102 Composite Curve | 2D/3D | 複数の曲線のポインタ
| ^ | 112 Parametric Spline Curve | 3D | 
| ^ | 126 Rational B Spline Curve |
| ^ | 130 Offset Curve |
| ^ | 142 Curve on a Parametric Surface |