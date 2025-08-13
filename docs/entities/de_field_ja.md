# Directory Entry Section Implementation

　この項は、`igesio/entities/de.h`（および`igesio/entities/de/*`）に実装されている、IGESファイルのDirectory Entry (DE) セクションに関連するクラスの概要を説明します。

## 目次

- [目次](#目次)
- [概要](#概要)
  - [Directory Entryセクション](#directory-entryセクション)
  - [関連する実装](#関連する実装)
- [RawEntityDE](#rawentityde)
  - [フィールドの一覧](#フィールドの一覧)
  - [補助関数と設計上の考慮事項](#補助関数と設計上の考慮事項)
    - [デフォルト値の管理](#デフォルト値の管理)
    - [ファクトリメソッド](#ファクトリメソッド)
    - [内部での使用](#内部での使用)
- [DEFieldWrapper](#defieldwrapper)
  - [DEFieldWrapperとその派生クラス](#defieldwrapperとその派生クラス)
    - [主要な設計特徴](#主要な設計特徴)
    - [例: 色フィールドの操作](#例-色フィールドの操作)
  - [クラスインターフェースの主要メンバ](#クラスインターフェースの主要メンバ)
    - [コンストラクタ](#コンストラクタ)
    - [値の取得](#値の取得)
    - [ポインタとIDの操作](#ポインタとidの操作)
    - [例: ポインタとIDの操作](#例-ポインタとidの操作)

## 概要

### Directory Entryセクション

　IGESファイルのDirectory Entry（DE）セクションは、すべてのエンティティに共通の情報を格納するセクションで、各エンティティのメタデータを定義します。DEセクションは、エンティティのタイプ、パラメータデータへのポインタなどを含む20個のフィールドで構成されます。これらのフィールドは、エンティティの識別や属性を定義するために使用されます。

> パラメータデータ: IGESファイル内では、1つのエンティティのパラメータは、共通部分がDEセクションに格納され、エンティティ固有のパラメータはParameter Data (PD) セクションに格納されます。例えば、円弧エンティティでは、中心点や円弧端点の座標などのパラメータがPDセクションに格納されます。その位置を示すポインタが2つ目のフィールドに格納されます。

　Directory Entryセクション内の各フィールドは、デフォルト値（空白またはゼロ）、整数値、または他のエンティティへのポインタを持つことができます。フィールド1、2、10、11、14、20は、圧縮フォーマットファイルを除いてデフォルト値が適用されません。

　以下の表に、Directory Entryセクションの20個のフィールドの詳細を示します：

| 番号 | フィールド名 | 説明 |
|---------------|-------------|------|
| 1 | Entity Type Number | エンティティタイプを識別する番号 |
| 2 | Parameter Data | エンティティのパラメータデータレコードの最初の行へのポインタ |
| 3 | Structure | エンティティの意味を指定する定義エンティティのDirectory Entryへの負のポインタ、またはゼロ（デフォルト） |
| 4 | Line Font Pattern | 線種パターン番号、Line Font Definition Entity（Type 304）のDirectory Entryへの負のポインタ、またはゼロ（デフォルト） |
| 5 | Level | エンティティが存在するレベル番号、Definition Levels Property Entity（Type 406, Form 1）のDirectory Entryへの負のポインタ、またはゼロ（デフォルト） |
| 6 | View | View Entity（Type 410）のDirectory Entryへのポインタ、Views Visible Associativity Instance（Type 402, Form 3,4, or 19）へのポインタ、またはゼロ（デフォルト） |
| 7 | Transformation Matrix | エンティティの定義に使用されるTransformation Matrix Entity（Type 124）のDirectory Entryへのポインタ、またはゼロ（デフォルト） |
| 8 | Label Display Associativity | Label Display Associativity（Type 402, Form 5）のDirectory Entryへのポインタ、またはゼロ（デフォルト） |
| 9 | Status Number | 4つの2桁値を連結した8桁の番号 |
| 10 | Sequence Number | Directory Entryのシーケンス番号（D#） |
| 11 | Entity Type Number | フィールド1と同じ値 |
| 12 | Line Weight Number | システム表示線幅。0から最大値（Global Sectionのパラメータ16）の範囲の段階値 |
| 13 | Color Number | 色番号、Color Definition Entity（Type 314）のDirectory Entryへの負のポインタ、またはゼロ（デフォルト） |
| 14 | Parameter Line Count | エンティティのパラメータデータレコードの行数 |
| 15 | Form Number | パラメータ値の解釈が複数あるエンティティのフォーム番号、またはゼロ（デフォルト） |
| 16 | Reserved | 将来の使用のため予約済み |
| 17 | Reserved | 将来の使用のため予約済み |
| 18 | Entity Label | 最大8文字の英数字（右詰め）、またはNULL（デフォルト） |
| 19 | Entity Subscript Number | エンティティラベルに関連付けられた1～8桁の符号なし番号 |
| 20 | Sequence Number | Directory Entryのシーケンス番号（D#+1） |

### 関連する実装

　本ライブラリでは、以下の2系統のDEセクション関連のクラスを実装しています。どちらの系統のクラスも、ユーザーが明示的にインスタンスを作成することはほとんどなく、ファイルの入出力時やエンティティの作成時に自動的に作成されたものを使用することがほとんどです。`DEFieldWrapper`の派生クラスは、プログラム上で個別エンティティクラスからDEセクションに関連した情報（例えば色や線種、変換行列など）を取得したり、設定する際に使用されます。

| クラス名 | 説明 |
| --- | --- |
| `RawEntityDE` | IGESファイルとの入出力時に使用される中間生成物です。IGESファイルを読み込んだ際は、`IntermediateIgesData`の`directory_entry_section`メンバとして、まずこの形式のデータが生成されます。 |
| `DEFieldWrapper` | DEセクションのうち、ポインタを持つ可能性のあるフィールドを保持するためのクラスです。内部に各エンティティオブジェクトへのポインタを保持することができます。これらのクラスは、`EntityBase`を継承した各個別エンティティクラスにおいて、メンバ変数として保持されます。 |

## RawEntityDE

　`RawEntityDE`は、IGESファイルのDirectory Entry (DE) セクションの20個のフィールドから、本ライブラリの実装に不可欠な情報を抽出して保持する構造体です。この構造体は、IGESファイルの読み込み時や書き出し時の中間データとして使用されます。
　
　この構造体には、以下に示す主要なメンバが含まれています。各メンバは、対応するDEフィールドの値を直接表現しており、`int`や`std::string`といったプリミティブ型や、`EntityStatus`のような複合型で定義されています。これらのメンバには、IGESファイルで指定されるデフォルト値が、コンストラクタ内で設定されます。

　その他の詳細については、[Intermediate Data Structure](../intermediate_data_structure_ja.md)を参照してください。

### フィールドの一覧

　`RawEntityDE`が保持するフィールドは以下の通りです。IGESファイルからの読み込み時は、"Parameter Data" (2nd field) や "Parameter Line Count" (14th field) など、プログラム上で意味を持たないフィールドも含めて、すべてのフィールドが設定されます。

| メンバ変数 | 説明 |
| --- | --- |
| `entity_type` | DEフィールド1、11に対応する**エンティティタイプ番号**です。`EntityType`列挙型で表現されます。 |
| `parameter_data_pointer` | DEフィールド2に対応する**パラメータデータへのポインタ**です。エンティティのパラメータデータが始まる行番号を示します。 |
| `structure` | DEフィールド3に対応する**構造**です。負の値の場合は構造定義エンティティへのポインタ、0の場合はデフォルト値を示します。 |
| `line_font_pattern` | DEフィールド4に対応する**線のフォントパターン**です。負の値の場合は線種定義エンティティへのポインタ、正の値はIGESで定義されたパターン番号、0はデフォルト値を示します。 |
| `level` | DEフィールド5に対応する**エンティティレベル**です。負の値の場合は定義レベルプロパティエンティティへのポインタ、正の値はレベル番号、0はデフォルト値を示します。 |
| `view` | DEフィールド6に対応する**ビュー**です。ViewエンティティまたはAssociativityInstanceへのポインタを示し、0の場合はデフォルト値となります。 |
| `transformation_matrix` | DEフィールド7に対応する**変換行列**です。Transformation Matrixエンティティへのポインタを示し、0の場合はデフォルト値となります。 |
| `label_display_associativity` | DEフィールド8に対応する**ラベル表示関連付け**です。AssociativityInstanceへのポインタを示し、0の場合はデフォルト値となります。 |
| `status` | DEフィールド9に対応する**ステータス番号**です。表示状態、従属スイッチ、用途フラグ、階層の4つの情報を持つ`EntityStatus`構造体で表現されます。 |
| `sequence_number` | DEフィールド10、20に対応する**シーケンス番号**です。DEセクション内でのエンティティの行番号（ID）を示します。 |
| `line_weight_number` | DEフィールド12に対応する**線の太さ番号**です。0から最大値までの整数で、線の太さを相対的に指定します。 |
| `color_number` | DEフィールド13に対応する**色番号**です。負の値の場合はColorDefinitionエンティティへのポインタ、正の値はIGESで定義された色番号、0はデフォルト値を示します。 |
| `parameter_line_count` | DEフィールド14に対応する**パラメータ行数**です。PDセクション内のエンティティのパラメータデータが占める行数を示します。 |
| `form_number` | DEフィールド15に対応する**フォーム番号**です。エンティティの特定の種類やバリエーションを示します。デフォルト値は0です。 |
| `entity_label` | DEフィールド18に対応する**エンティティラベル**です。エンティティの識別子となる文字列です。 |
| `entity_subscript_number` | DEフィールド19に対応する**エンティティ添字番号**です。エンティティラベルの数値修飾子として機能します。 |

### 補助関数と設計上の考慮事項

#### デフォルト値の管理

　`RawEntityDE`には、各フィールドがIGESファイルから読み込まれた際にデフォルト値（空白またはゼロ）であったかを追跡するための内部配列`is_default_`が用意されています。この情報は、`A conforming editor shall not affect entities that the user did not edit` (IGES 5.3 Section 1.4.7.1)に準拠するための情報です。`IsDefault()`メソッドを通じて取得できます。

#### ファクトリメソッド

　静的メンバ関数である`ByDefault(entity_type, form_number)`は、指定されたエンティティタイプとフォーム番号に基づいて、デフォルト値で初期化された`RawEntityDE`のインスタンスを生成します。このメソッドは、新しいエンティティを作成する際に、最小限の情報で構造体を初期化するための便利な手段となります。

#### 内部での使用

　`RawEntityDE`構造体は、主にIGESファイルの読み込みおよび書き出しロジックで利用されます。読み込み時には、IGESファイルのテキストデータからこの構造体へ情報が解析され、その後、対応する`EntityBase`派生クラス（例：`CircularArc`）のオブジェクトに変換されます。書き出し時にはこの逆のプロセスを辿ります。これにより、ファイルI/Oの複雑な部分を抽象化し、エンティティ固有のロジックから分離しています。

## DEFieldWrapper

### DEFieldWrapperとその派生クラス

　本ライブラリでは、Directory Entry (DE) セクションの各フィールドを以下の3種類に分類し、このうち (2) のフィールドを表現するクラスとして`DEFieldWrapper`継承クラスを定義しています。

1. **列挙体やプリミティブ型で表現されるフィールド**
    - Entity Type (1st/11th fields)、Status Number (9th field)、Entity Label (18th field) など
2. **ポインタを持つ可能性のあるフィールド**
    - Structure (3rd field)、Transformation Matrix (7th field)、Color (13th field) など
3. **`EntityBase`継承クラスにおいて意味を持たないフィールド**
    - Parameter Data (2nd field)、Parameter Line Count (14th field)、Reserved (16th/17th fields) など

　フィールド分類 (2) では、値としてデフォルト値、プリミティブ型の値、またはポインタのいずれかを持つことができます。各フィールドにどの種類の値が設定されているかを意識せずに使用できるよう、`DEFieldWrapper`クラスを設計しています。分類 (1) のフィールドは、列挙体やプリミティブ型で直接表現されるため、`DEFieldWrapper`クラスは使用されません。`EntityBase`継承クラスでは、これら分類 (1) と (2) のフィールドをメンバ変数として保持し、必要に応じて値を取得・設定します。

#### 主要な設計特徴

**値の種類の管理**
　フィールドが保持する値の状態を`DEFieldValueType`列挙体（`kDefault`、`kPointer`、`kPositive`）で管理します。これにより、値がデフォルトなのか、ポインタなのか、固有の正の値なのかを判別できます。

**型安全なポインタ**
　テンプレートパラメータ`Args...`を使用して、そのフィールドが参照できるエンティティの型をコンパイル時に指定します。これにより、意図しない型のエンティティへのポインタ設定を防ぎます。

**循環参照の防止**
　エンティティへの参照を`std::weak_ptr`で保持することで、エンティティ間の相互参照によるメモリリークを防ぎます。

**IDとポインタの一貫性**
　内部で保持するエンティティID（`id_`）と実際のポインタ（`weak_ptrs_`）が指すエンティティのIDの一致を保証します。IDが一致しないポインタを設定しようとすると実行時例外がスローされます。
　
> 「IDとポインタの一貫性」の機能はIGESファイル読み込み時の逐次処理に対応するためのものです。DEフィールドが参照するエンティティのインスタンス化がまだ行われていない場合、IDのみを先に設定し、ポインタを後で設定できます。

#### 例: 色フィールドの操作

　以下は`CircularArc`エンティティの色フィールド（13th DE Field）を操作する例です。デフォルト値、列挙体値、ポインタ値をそれぞれ設定し、値を取得しています。

```cpp
#include <igesio/entities/de.h>
#include <igesio/entities/curves/circular_arc.h>

using Vector2d = iges::Vector2d;
namespace ent = iges::entities;

// 色の値を保持するための配列: IGESではRGB値を0-100の範囲で表現
// 格納されている値の種類に関わらず、double型の配列で取得可能
std::array<double, 3> color;

// 半径5.0、原点中心の円エンティティを作成
// 各DEフィールドはデフォルト値で初期化される
auto center = Vector2d(0.0, 0.0);
auto circle = std::make_shared<ent::CircularArc>(center, 5.0);

// デフォルトの色を取得
circle->GetColor().GetValueType();    // DEFieldValueType::kDefault
color = circle->GetColor().GetRGB();  // {0, 0, 0}

// 規定色に設定（列挙体値）
circle->OverwriteColor(ent::ColorNumber::kCyan);
circle->GetColor().GetValueType();    // DEFieldValueType::kPositive
color = circle->GetColor().GetRGB();  // {0, 100, 100}

// 色エンティティを作成して設定（ポインタ値）
auto color_def = std::make_shared<ent::ColorDefinition>(
     std::array<double, 3>{50.0, 100.0, 30.0}, "Light Green");
circle->OverwriteColor(color_def);
circle->GetColor().GetValueType();    // DEFieldValueType::kPointer
color = circle->GetColor().GetRGB();  // {50, 100, 30}
```

### クラスインターフェースの主要メンバ

　`DEFieldWrapper`クラスは、1つ以上のテンプレートパラメータを取るクラスとして定義されています。`typename... Args`はこのフィールドがポインタとして参照する可能性のあるエンティティの型を指定する可変長テンプレート引数です。

```cpp
template<typename... Args>
class DEFieldWrapper { ... };
```

　以下は`DEFieldWrapper`派生クラスの一覧です。

| 番号 | クラス名 | テンプレートパラメータ |
|:--:| --- | ----------------------- |
| 3 | `DEStructure` | `IStructure` |
| 4 | `DELineFontPattern` | `ILineFontDefinition` |
| 5 | `DELevel` | `IDefinitionLevelsProperty` |
| 6 | `DEView` | `IView`, `IViewsVisibleAssociativity` |
| 7 | `DETransformationMatrix` | `ITransformation` |
| 8 | `DELabelDisplayAssociativity` | `ILabelDisplayAssociativity` |
| 13 | `DEColor` | `IColorDefinition` |

#### コンストラクタ

| コンストラクタ | 説明 |
| --- | --- |
| `DEFieldWrapper()` | フィールドをデフォルト値（値が0の状態）で初期化します。|
| `explicit DEFieldWrapper(const uint64_t id)` | ポインタとして参照すべきエンティティのIDを指定して初期化します。IGESファイルからDEセクションを読み込む際に、まずIDのみを保持するために使用されます。|

このほかにコピーコンストラクタやムーブコンストラクタ、代入演算子なども定義されています。

#### 値の取得

| 関数名 | 戻り値 | 説明 |
| --- | --- | --- |
| `GetValueType()` | `DEFieldValueType` | 現在のフィールド値の種類を返します。 |
| `GetValue()` | `int` | IGESファイルに書き出すための整数値を取得します。値がポインタの場合、IDは**負数に変換**されて返されます。 |
| `GetPointer<T>()` | `std::shared_ptr<const T>` | 型 `T` を指定して、参照されているエンティティへのスマートポインタを取得します。ポインタが無効な場合は `nullptr` を返します。 |
| `GetID()` | `uint64_t` | このフィールドが参照している、または参照しようとしているエンティティのIDを返します。 |

#### ポインタとIDの操作

| 関数名 | 引数 | 説明 |
| --- | --- | --- |
| `HasValidPointer()` | なし | 現在のフィールドが有効なポインタを持つかどうかを返します。ポインタが設定済みであれば`true`、そうでなければ`false`を返します。 |
| `SetPointer(ptr)` | `ptr`<br>(`std::shared_ptr<const T>`) | エンティティへのポインタを設定します。引数 `ptr` が持つIDと、このインスタンスが保持しているIDが一致する場合にのみポインタが有効になります。<br>一致しない場合、`std::invalid_argument`例外がスローされます。新しいポインタを設定したい場合は、`OverwritePointer(ptr)` を使用してください。 |
| `OverwritePointer(ptr)` | `ptr`<br>(`std::shared_ptr<const T>`) | 現在のIDやポインタを、引数 `ptr` のもので完全に上書きします。ポインタが無効な場合は、IDも無効になります。 |
| `OverwriteID(new_id)` | `new_id`<br>(`uint64_t`) | 現在のポインタを無効にし、新しいIDを設定します。ポインタは `nullptr` になります。 |
| `GetUnsetID()` | なし | IDは設定されているが、対応するポインタの実体がまだ設定されていない場合に、そのIDを返します。これは、ファイル読み込み後に全てのエンティティオブジェクトが生成されてから、ポインタの関連付け（解決）を行う処理で使用されます。IDが設定されていない場合は `std::nullopt` を返します。 |

#### 例: ポインタとIDの操作

　以下は、[例: 色フィールドの操作](#例-色フィールドの操作)で作成したコードの続きです。

```cpp
circle->GetColor().HasValidPointer();  // true - Light GreenのColorDefinitionが設定されている

// 新しい色定義エンティティを作成
auto new_color_def = std::make_shared<ent::ColorDefinition>(
     std::array<double, 3>{100.0, 50.0, 0.0}, "Orange");

// 新しい色定義エンティティのポインタで上書き
// `OverwritePointer`の内部では、`DEColor::OverwriteID`と`DEColor::SetPointer`が呼ばれる
circle.OverwriteColor(new_color_def);

// 新しい色を取得
color = circle->GetColor().GetRGB();  // {100, 50, 0}
```

　`EntityBase`継承クラスでは、各DEフィールドへのconst参照しか提供されません。したがって、上の例のように新しい値を設定する場合は、そのフィールドに対応する`Overwrite`系関数（色フィールドの場合は`EntityBase::OverwriteColor`）を使用します。

> ユーザー側の操作量を減らすため、エンティティクラスから`DEFieldWrapper`への非const参照を取得できないように設計しています。`DEFieldWrapper::SetPointer`のような関数は、IGESファイルからの逐次読み込みの際に使用する（参照先のインスタンスが未生成の状態でIDのみを設定する）ためのものです。したがって、通常の使用では、フィールドの値を取得するためのconst参照と、値を設定するための`Overwrite`系関数を使用することになります。
