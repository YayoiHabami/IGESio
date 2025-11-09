# Index (ドキュメント)

## 目次

- [目次](#目次)
- [特に重要なドキュメント](#特に重要なドキュメント)
- [機能別ドキュメント](#機能別ドキュメント)
  - [commonモジュール](#commonモジュール)
  - [entitiesモジュール](#entitiesモジュール)
    - [個別エンティティクラス](#個別エンティティクラス)
  - [graphicsモジュール](#graphicsモジュール)
  - [modelsモジュール](#modelsモジュール)
  - [numericsモジュール](#numericsモジュール)
  - [utilsモジュール](#utilsモジュール)
- [その他のドキュメント](#その他のドキュメント)

## 特に重要なドキュメント

　以下は、特に重要なドキュメントです。まずはこちらを参照してください。

- **[ビルド方法](build_ja.md)**: 本ライブラリのビルド方法
- **[Examples (ja)](examples_ja.md)**: `examples`ディレクトリに含まれるサンプルコードの概要
- **[使用するクラスの概観](./class_structure_ja.md)**: 本ライブラリで使用する主要なクラスの関係の説明
- **[実装状況の一覧](./implementation_progress.md)**: エンティティクラス等の実装状況の一覧

## 機能別ドキュメント

　以下は、機能別のドキュメントです。記載されていないファイルについては、ソースコード内のコメントを参照してください。

### commonモジュール

- 現在、ドキュメントはありません。

### entitiesモジュール

- **[エンティティクラスの概観](entities/entities_ja.md)**
  - `IEntityIdentifier`を基底とする、全エンティティクラスの関係の説明
  - 個別のエンティティクラスの基底クラスである、`EntityBase`の説明
- **[個別エンティティクラス](entities/entities_ja.md)**
  - 各インターフェースの説明
  - 各エンティティクラスの説明（コード例、図など）
- **[ジオメトリの幾何学的特性](entities/geometric_properties_ja.md)**
  - 曲線および曲面エンティティに対して計算可能な、各種幾何学的特性の説明
  - 各種特性の計算方法（コード例）についても記述
- **[中間データ構造](intermediate_data_structure_ja.md)**
  - 本ライブラリにおいて、IGESファイルの入出力の際に内部で使用される中間データ構造の説明
  - IGESファイルのDirectory EntryセクションとParameter Dataセクションの中間データ構造
- **[DEフィールド](entities/de_field_ja.md)**
  - Directory Entryセクションに関する簡単な説明
  - DEフィールドを扱うクラス構造の説明

#### 個別エンティティクラス

　個別のエンティティクラスのドキュメントは、以下の通りです。現時点では、それぞれのパラメトリック曲線/曲面に関する数式的な定義、およびその導関数/偏導関数の定義が記載されています。

| 種別 | ドキュメント |
|---|---|
| curves | [Circular Arc (Type 100)](entities/curves/100_circular_arc_ja.md) <br> 円・円弧エンティティ |
|   ^    | [Composite Curve (Type 102)](entities/curves/102_composite_curve_ja.md) <br> 複数の曲線（`ICurve`派生クラス）を組み合わせ可能なエンティティ |
|   ^    | [Conic Arc (Type 104)](entities/curves/104_conic_arc_ja.md) <br> 円錐曲線 (楕円・放物線・双曲線) エンティティ |
|   ^    | [Copious Data (Type 106)](entities/curves/106_copious_data_ja.md) <br> 点列、折れ線エンティティ, 各点/頂点に追加のベクトルを設定することも可能 |
|   ^    | [Line (Type 110)](entities/curves/110_line_ja.md) <br> 線分・半直線・直線エンティティ |
|   ^    | [Parametric Spline Curve (Type 112)](entities/curves/112_parametric_spline_curve_ja.md) <br> 複数の、最大3次の多項式セグメントからなる曲線エンティティ |
|   ^    | [Rational B-Spline Curve (Type 126)](entities/curves/126_rational_b_spline_curve_ja.md) <br> 有理Bスプライン曲線エンティティ (NURBS曲線を含む) |
|   ^    | [Curve on a Parametric Surface (Type 142)](entities/curves/142_curve_on_a_parametric_surface_ja.md) <br> パラメトリック曲面上の曲線エンティティ |
| surfaces | [Ruled Surface (Type 118)](entities/surfaces/118_ruled_surface_ja.md) <br> 2つの曲線を直線で結ぶことで定義されるルールド面エンティティ |
|    ^     | [Surface of Revolution (Type 120)](entities/surfaces/120_surface_of_revolution_ja.md) <br> 曲線を軸回りに回転させることで定義される回転面エンティティ |
|    ^     | [Tabulated Cylinder (Type 122)](entities/surfaces/122_tabulated_cylinder_ja.md) <br> 準線(曲線)を一定方向に平行移動させることで定義される曲面エンティティ |
|    ^     | [Rational B-Spline Surface (Type 128)](entities/surfaces/128_rational_b_spline_surface_ja.md) <br> 有理Bスプライン曲面エンティティ (NURBS曲面を含む) |

### graphicsモジュール

- 現在、ドキュメントはありません。

### modelsモジュール

- **[中間データ構造](intermediate_data_structure_ja.md)**: IGESファイル入出力時の中間データ構造

### numericsモジュール

- **[Matrix](numerics/matrix_ja.md)**: 固定/動的サイズの行列クラス
- **[Bounding Box](numerics/bounding_box_ja.md)**: 直交バウンディングボックス (OBB) クラス
  - `IGeometry`派生クラス（曲線・曲面エンティティクラス）において定義される、`GetBoundingBox`関数で返されるバウンディングボックスの説明

### utilsモジュール

- 現在、ドキュメントはありません。

## その他のドキュメント

- **[policy (ja)](policy_ja.md)**: ライブラリの設計方針やIGES仕様の解釈について
- **[flow/reader (ja)](flow/reader_ja.md)**: 読み込み処理のフローについて
- **[entity-analysis (en)](entity_analysis.md)**: IGES 5.3における、各エンティティの分類やパラメータについての分析
- **[additional-notes (ja)](additional_notes_ja.md)**: その他の補足事項
- **[todo](todo.md)**: TODOリスト
