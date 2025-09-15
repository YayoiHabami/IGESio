# Entities

　本ドキュメントでは、具体的なエンティティのインターフェースと、個別のエンティティクラスを扱います。すべてのクラスが継承する`IEntityIdentifier`については、[こちら](entity_base_class_architecture_ja.md#ientityidentifier)を参照してください。また、個別のエンティティクラスが継承する`EntityBase`については、[こちら](entity_base_class_architecture_ja.md#entitybase)を参照してください。

## 目次

- [目次](#目次)
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
- [Transformations](#transformations)
  - [`TransformationMatrix` (type 124)](#transformationmatrix-type-124)

## Interfaces

　IGESファイルに記述されるエンティティは、その種類に応じて対応するインターフェースを実装します。これらのインターフェースは、エンティティの基本的な特性や操作を定義し、具体的なエンティティクラスがこれらのインターフェースを実装することで、統一的な方法でエンティティを扱うことができます。

　また、これらのインターフェースは、エンティティが特定の機能を目的として参照するエンティティへのポインタの型としても使用されます。例えば`CompositeCurve`は複数の曲線を参照しますが、各エンティティへの参照は`ICurve`型のポインタとして保持されます。

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

　`ITransformation`クラスは、Transformation Matrix (Type 124) エンティティのインターフェースです。このクラスを継承するのは`TransformationMatrix`クラスのみですが、`DETransformationMatrix`クラス内においてTransformation Matrixエンティティへの参照を保持するために使用されます。`TransformationMatrix`クラスの前方定義を用いる設計では、抽象化の原則に反するため、このようなインターフェースを定義しています。

### `IColorDefinition`

> Defined at [i_color_definition.h](../../include/igesio/entities/interfaces/i_color_definition.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <─── IColorDefinition
> ```

## Annotations

　本節では、IGESファイルにおける注釈エンティティについて解説します。注釈エンティティは、寸法、注記、記号など、モデルに追加情報を提供するために使用されます。

## Structures

　本節では、IGESファイルにおける構造エンティティについて解説します。構造エンティティは、モデル内のエンティティの配置、接続性、外部ファイル参照、図面とビューの関係、有限要素モデリング、属性テーブルなど、エンティティ間の関係性や構造を定義するために使用されます。

　また、本ライブラリでは、未実装のエンティティタイプを表現するための`UnsupportedEntity`クラスも定義しています。このクラスは、特定のエンティティタイプに対応する具体的なエンティティクラスが存在しない場合に使用されます。

### `UnsupportedEntity`

> Defined at [unsupported_entity.h](../../include/igesio/entities/structures/unsupported_entity.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <── EntityBase <── UnsupportedEntity
> ```

　`UnsupportedEntity`クラスは、指定されたエンティティタイプに対応する具体的なエンティティクラスの実装が、存在しない（または未実装の）場合に使用されるクラスです。このクラスは`EntityBase`を継承し、エンティティの識別情報を保持しますが、具体的なエンティティのための機能は実装されません。

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

    // 例えばEntityTypeがkCircularArcであった場合でも、
    // 以下のような処理はエラーになります。
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

　`ColorDefinition`クラスは、エンティティの色を定義するためのエンティティクラスです。IGESファイル内で定義された色を表現し、他のエンティティに適用することができます。以下のコード例は、RGB値 (30, 50, 100) を持つColorDefinitionエンティティを生成し、`CircularArc`エンティティに適用しています（図参照）。

　注意点として、通常のRGB値は (0-255) の範囲で指定されますが、`ColorDefinition`クラスでは (0.0-100.0) の範囲で指定します。例えば、RGB値 (76, 127, 255) は (30.0, 50.0, 100.0) として指定します。

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

**図: ColorDefinitionエンティティの例**. 左はデフォルト色(黒)、右は#4C7FFF (明るい青).

## Curves

　本節では、IGESファイルに記述される曲線エンティティについて解説します。曲線エンティティは、2次元または3次元空間内の曲線を表現するために使用されます。

### `CircularArc` (type 100)

> Defined at [circular_arc.h](../../include/igesio/entities/curves/circular_arc.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <─┬─────────── EntityBase <─┬─ CircularArc
>                     └─ ICurve  <── ICurve2D <─┘
> ```

　`CircularArc`は2次元円弧・円を表現するためのクラスです。以下のコード例は $(-1.25, 0)$ を中心とする半径 $1$ の円と、$(1.25, 0)$ を中心とする半径 $1$ の円弧を生成します（図参照）。

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

**図: CircularArcエンティティの例**

### `CompositeCurve` (type 102)

> Defined at [composite_curve.h](../../include/igesio/entities/curves/composite_curve.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <─┬─────────── EntityBase <─┬─ CompositeCurve
>                     └─ ICurve  <── ICurve3D <─┘
> ```

　`CompositeCurve`は複数の曲線エンティティを連結して一つの曲線として表現するためのクラスです。以下のコード例は、3つの曲線エンティティ（円弧、直線、円弧）を連結したCompositeCurveエンティティを生成します（図参照）。

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

**図: CompositeCurveエンティティの例**（右下の円弧が`comp_1`）

### `ConicArc` (type 104)

> Defined at [conic_arc.h](../../include/igesio/entities/curves/conic_arc.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <─┬─────────── EntityBase <─┬─ ConicArc
>                     └─ ICurve  <── ICurve2D <─┘
> ```

　`ConicArc`は2次元の円錐曲線（楕円、放物線、双曲線）を表現するためのクラスです。以下のコード例は、中心が $(0, 3)$、長軸・短軸がそれぞれ $3$、$2$ の楕円弧を生成します（図参照）。

　現在、放物線・双曲線は未実装です（エンティティクラスは共通ですが、内部処理が未対応です）。

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

**図: ConicArcエンティティの例** (楕円のみ)

### Copious Data (type 106)

#### `CopiousDataBase` (type 106)

> Defined at [copious_data_base.h](../../include/igesio/entities/curves/copious_data_base.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <── EntityBase <── CopiousDataBase
> ```

　すべてのCopious Data関連クラスの基底クラスです。`ICurve`等のインターフェースは実装していません。本ライブラリで未実装のフォーム番号に対しては、このクラスのインスタンスが生成されます。

> 一部のCopious Dataエンティティ（Form 20～40）は、[Curve](#curves) and [Surface](#surfaces) Geometryエンティティではなく、[Annotation](#annotation)エンティティに分類されます。

<img src="./images/copious_data.png" width=600px alt="CopiousData Example" />

**図: Copious Dataエンティティの例**. 左はフォーム2（点群）、右はフォーム12（折れ線）であり、右側は左側と同じ順序で定義された点を右方向にシフトしたもの.

#### `CopiousData` (type 106, forms 1-3)

> Defined at [copious_data.h](../../include/igesio/entities/curves/copious_data.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <── EntityBase <─┬────── CopiousDataBase <─┬─ CopiousData
>                                    └─ ICurve  <── ICurve3D <─┘
> ```

　`CopiousData`は、フォーム1～3 (座標2つ組、座標3つ組、座標6つ組) の点群データを表現するためのクラスです。以下のコード例は、5つの3次元座標点からなる点群を生成します（[CopiousDataBaseの図参照](#copiousdatabase-type-106)）。

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

　`LinearPath`は、フォーム11～13 (座標2つ組、座標3つ組、座標6つ組) の折れ線データを表現するためのクラスです。以下のコード例は、5つの3次元座標点からなる折れ線を生成します（[CopiousDataBaseの図参照](#copiousdatabase-type-106)）。与える点群は[CopiousData](#copiousdata-type-106-forms-1-3)と同じですが、右方向に $5$ だけシフトしています。

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

　`Line`は、3次元空間内の直線、半直線、線分を表現するためのクラスです。以下のコード例は、3種類のLineエンティティ（線分、半直線、直線）を生成します（図参照）。

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

**図: Lineエンティティの例**. 左から線分、半直線、直線.

### `RationalBSplineCurve` (type 126)

> Defined at [rational_b_spline_curve.h](../../include/igesio/entities/curves/rational_b_spline_curve.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <─┬─────────── EntityBase <─┬─ RationalBSplineCurve
>                     └─ ICurve  <── ICurve3D <─┘
> ```

　`RationalBSplineCurve`は、3次元空間内の有理Bスプライン曲線を表現するためのクラスです。以下のコード例は、4つの制御点、3次の非周期的開放NURBS曲線を生成します（図参照）。以下に示すように、`IGESParameterVector`構造体を用いてパラメータをまとめて渡し、インスタンスを生成します。

　この際の注意点として、`IGESParameterVector`には、intとdoubleを明確に区別して渡してください。例えば以下の7つ目のパラメータ (1つ目のノットベクトル値; `0.0`) を、`0` (int) として渡すと、エラーが発生します。

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

**図: RationalBSplineCurveエンティティの例**

## Surfaces

　本節では、IGESファイルに記述される曲面エンティティについて解説します。曲面エンティティは、3次元空間内の曲面を表現するために使用されます。

## Transformations

　本節では、IGESファイルに記述される変換行列エンティティについて解説します。変換行列は、座標系の変換やエンティティの位置、向き、スケールを変更するために使用されます。

> IGES 5.3 の規格においては、Transformation Matrix (Type 124) はCurve and Surface Geometry entity、すなわち[Curves](#curves)や[Surfaces](#surfaces)などの一種として定義されています。

### `TransformationMatrix` (type 124)

> Defined at [transformation_matrix.h](../../include/igesio/entities/transformations/transformation_matrix.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <─┬────── EntityBase <─┬─ TransformationMatrix
>                     └─ ITransformation <─┘
> ```
