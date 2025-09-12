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

## Curves

　本節では、IGESファイルに記述される曲線エンティティについて解説します。曲線エンティティは、2次元または3次元空間内の曲線を表現するために使用されます。

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

### Copious Data (type 106)

#### `CopiousDataBase` (type 106)

> Defined at [copious_data_base.h](../../include/igesio/entities/curves/copious_data_base.h)

> Ancestor class:
> ```plaintext
> IEntityIdentifier <── EntityBase <── CopiousDataBase
> ```

　すべてのCopious Data関連クラスの基底クラスです。`ICurve`等のインターフェースは実装していません。本ライブラリで未実装のフォーム番号に対しては、このクラスのインスタンスが生成されます。

> 一部のCopious Dataエンティティ（Form 20～40）は、[Curve](#curves) and [Surface](#surfaces) Geometryエンティティではなく、[Annotation](#annotation)エンティティに分類されます。

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
