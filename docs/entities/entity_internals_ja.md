# エンティティ実装の内部詳細：参照解決とシリアライズ

本ドキュメントは、エンティティクラスを実装する際に理解しておく必要がある、二つの内部機構についてまとめる。

- **参照解決**：あるエンティティが別のエンティティを参照する場合の、DEポインタからポインタへの解決フロー
- **シリアライズ**：エンティティをIGESファイルに書き戻す際のパラメータ生成の仕組み

[エンティティの追加手順](add_entity_ja.md)のステップ3（ソースファイル実装）と合わせて参照すること。

## 目次

- [目次](#目次)
- [参照解決](#参照解決)
  - [IGESファイルにおけるエンティティ間参照](#igesファイルにおけるエンティティ間参照)
  - [pointer2IDとObjectID](#pointer2idとobjectid)
  - [参照解決の全体フロー](#参照解決の全体フロー)
  - [SetMainPDParametersでの実装パターン](#setmainpdparametersでの実装パターン)
  - [オプショナルな参照（未設定IDの扱い）](#オプショナルな参照未設定idの扱い)
    - [ObjectIDの未設定状態](#objectidの未設定状態)
    - [実装パターン](#実装パターン)
  - [GetUnresolvedPDReferencesとSetUnresolvedPDReferences](#getunresolvedpdreferencesとsetunresolvedpdreferences)
  - [PointerContainerのテンプレート引数の選択](#pointercontainerのテンプレート引数の選択)
  - [DEフィールドからの参照（変換行列など）](#deフィールドからの参照変換行列など)
  - [GetChildIDsとGetChildEntityの役割](#getchildidsとgetchildentityの役割)
- [IGESParameterVector](#igesparametervector)
  - [格納できる型](#格納できる型)
  - [値の追加・設定](#値の追加設定)
  - [値の取得：getとaccess\_asの違い](#値の取得getとaccess_asの違い)
  - [GetObjectIDFromParameters](#getobjectidfromparameters)
  - [型確認とフォーマット操作](#型確認とフォーマット操作)
  - [その他の操作](#その他の操作)
- [シリアライズ](#シリアライズ)
  - [pd\_parameters\_の役割](#pd_parameters_の役割)
  - [GetMainPDParametersの実装パターン](#getmainpdparametersの実装パターン)
  - [GetParametersの役割](#getparametersの役割)
  - [フォーマット保持](#フォーマット保持)
  - [GetRawEntityDEの役割](#getrawentitydeの役割)

## 参照解決

### IGESファイルにおけるエンティティ間参照

IGESファイルでは、あるエンティティが別のエンティティを参照する場合、Parameter Dataセクション（またはDirectory Entryセクション）に「DEポインタ」の数値を記述する。DEポインタとは、参照先エンティティのDirectory EntryセクションにおけるIGESシーケンス番号（奇数値）である。

例えば、CompositeCurve（Type 102）のPDセクションは次のような形式になる。

```
102, 3, 1, 5, 9;    ← 3本の曲線を参照、DEポインタは 1, 5, 9
```

この数値「1」「5」「9」はIGESファイル上でのみ意味を持ち、プログラム内部では使用しない。プログラムでは各エンティティに対して`ObjectID`（⇒`uint64_t`型との互換性を持つ）が割り当てられ、これを使って参照を管理する。

### pointer2IDとObjectID

`pointer2ID`は`std::unordered_map<unsigned int, ObjectID>`の型エイリアスで、**IGESファイル上のDEポインタ番号 → プログラム内部のObjectID** への変換表である。

この変換表は`IgesReader`がIGESファイルを読み込む際に構築され、各エンティティのコンストラクタに`de2id`引数として渡される。

```cpp
// pointer2ID の具体的な内容のイメージ
// DEポインタ1  → ObjectID(xxx)
// DEポインタ5  → ObjectID(yyy)
// DEポインタ9  → ObjectID(zzz)
```

`de2id`が空（`{}`）の場合は、PDパラメータ上のポインタ値をそのまま`ObjectID`として扱う。これはプログラム側でエンティティを直接構築する場合（ファイル読み込みを介さない場合）に該当する。

### 参照解決の全体フロー

IGESファイルを読み込む際の参照解決は、以下の順序で行われる。

```
IGESファイル
  │
  ▼
IgesReader が全エンティティをEntityFactory::CreateEntity() で生成
  │  ├─ de2id を渡す
  │  └─ コンストラクタ内で InitializePD(de2id) を呼ぶ
  │       └─ SetMainPDParameters(de2id) を呼ぶ
  │            └─ DEポインタ番号 → de2id.at(番号) → ObjectID
  │                 └─ PointerContainer(id) を構築 (IDのみ保持、ポインタは未設定)
  │
  ▼
全エンティティの生成が完了
  │
  ▼
IgesData が各エンティティの SetUnresolvedReference(entity) を呼び出す
  └─ EntityBase::SetUnresolvedReference()
       ├─ DEフィールド（変換行列・色定義など）の参照を解決 (EntityBase側で自動処理)
       └─ SetUnresolvedPDReferences(entity) を呼ぶ
            └─ 各エンティティの実装でPDフィールドの参照を解決
                 └─ PointerContainer::SetPointer(ptr) でポインタを設定
```

コンストラクタ呼び出し時点では`PointerContainer`はIDしか保持していない。全エンティティが生成された後、`IgesData`が`SetUnresolvedReference()`を繰り返し呼び出すことで、IDからポインタへの解決が完了する。

### SetMainPDParametersでの実装パターン

他のエンティティへの参照を持つエンティティは、`SetMainPDParameters()`の中で`de2id`を使ってDEポインタ番号を`ObjectID`に変換し、`PointerContainer`を構築する。

```cpp
// CompositeCurveのSetMainPDParametersの実装（簡略化）
size_t CompositeCurve::SetMainPDParameters(const pointer2ID& de2id) {
    auto& pd = pd_parameters_;
    auto num_curves = static_cast<size_t>(pd.access_as<int>(0));

    curves_.clear();
    for (size_t i = 0; i < num_curves; ++i) {
        if (!de2id.empty()) {
            // ファイル読み込み時: DEポインタ番号 → ObjectIDに変換
            auto de_ptr = static_cast<unsigned int>(pd.access_as<int>(i + 1));
            curves_.emplace_back(de2id.at(de_ptr));  // ObjectIDでPointerContainerを構築
        } else {
            // プログラム側での直接構築時: 値をそのままObjectIDとして使う
            curves_.emplace_back(pd.access_as<ObjectID>(i + 1));
        }
    }
    return num_curves + 1;
}
```

`de2id.empty()`の分岐が必要な理由は、プログラム側でエンティティを直接構築する際にはDEポインタ番号が存在せず、パラメータに直接`ObjectID`が格納されているためである。

### オプショナルな参照（未設定IDの扱い）

IGESの仕様では、PDフィールドの一部は省略可能である。省略されたフィールドはIGESファイル上でポインタ値`0`として記述され、「未設定（unset）」と呼ぶ。`ObjectID`・`PointerContainer`・`IGESParameterVector`の三者で一貫した方法で扱う。

#### ObjectIDの未設定状態

`ObjectID`は`identifier`フィールドに`nullptr`を保持する場合、未設定状態となる。

```cpp
ObjectID id;               // デフォルトコンストラクタ → 未設定
id.IsSet()                 // → false
id.ToInt()                 // → kInvalidIntID (= 0)
IDGenerator::UnsetID()     // → 未設定ObjectIDのシングルトン（kInvalidIntID = 0 に対応）
```

IGESファイル上でポインタフィールドが`0`の場合、参照なし（省略）を意味する。

#### 実装パターン

`Point`（Type 116）の`subfigure_`フィールド（省略可能なSubfigure Definition Entityへの参照）が典型的な実装例である。

**コンストラクタ**：`IDGenerator::UnsetID()`で初期化する。

```cpp
subfigure_(IDGenerator::UnsetID())
```

**`SetMainPDParameters`**：`GetObjectIDFromParameters`の第4引数（`allow_unset_id`）に`true`を渡す。これにより値`0`が`UnsetID`に変換され、例外が投げられない。

```cpp
subfigure_ = PointerContainer<false, ISubfigureDefinition>(
        GetObjectIDFromParameters(pd, 3, de2id, true));  // allow_unset_id = true
```

必須フィールドには第4引数を省略（デフォルトは`false`）し、省略可能フィールドには`true`を指定する。なお`access_as<ObjectID>`は`0`を`UnsetID`に変換できない（`IDGenerator::TryGetByIntID(0)`が失敗する）ため、省略可能なポインタフィールドには`access_as`ではなく`GetObjectIDFromParameters`を使うこと。

**`GetUnresolvedPDReferences`**：`IsPointerSet()`のみでチェックする。未設定IDはunresolvedセットに残るが、実在するエンティティのIDと一致することはないため、参照解決のループに影響を与えない。

```cpp
if (!subfigure_.IsPointerSet()) {
    unresolved.insert(subfigure_.GetID());  // UnsetIDも挿入されうるが問題なし
}
```

**`GetMainPDParameters`**：IDをそのまま`push_back`する。未設定IDの場合、`ToInt()`が`0`を返すため、IGESファイル上で`0`と出力される。

```cpp
return { position_[0], position_[1], position_[2],
         subfigure_.GetID() };  // 未設定なら 0 として出力
```

**利用側**：`GetID().IsSet()`でガードしてからアクセスする。

```cpp
if (subfigure_.GetID().IsSet()) {
    // 参照が設定されている場合のみアクセス
    if (!subfigure_.IsPointerSet()) {
        // IDは存在するが参照が未解決（エラー）
    } else {
        auto ptr = subfigure_.TryGetEntity<EntityBase>();
    }
}
```

### GetUnresolvedPDReferencesとSetUnresolvedPDReferences

参照を持つエンティティは、`EntityBase`の以下の2つのprotectedメンバ関数をオーバーライドする。

| 関数 | 役割 |
| --- | --- |
| `GetUnresolvedPDReferences()` | PD由来のポインタのうち未設定のものの`ObjectID`集合を返す |
| `SetUnresolvedPDReferences(entity)` | 指定されたエンティティが参照先の一つに合致する場合、ポインタを設定して`true`を返す |

```cpp
// GetUnresolvedPDReferences の実装例 (CompositeCurve)
std::unordered_set<ObjectID>
CompositeCurve::GetUnresolvedPDReferences() const {
    std::unordered_set<ObjectID> ids;
    for (const auto& curve : curves_) {
        if (!curve.IsPointerSet()) {
            ids.insert(curve.GetID());
        }
    }
    return ids;
}

// SetUnresolvedPDReferences の実装例 (CompositeCurve)
bool CompositeCurve::SetUnresolvedPDReferences(
        const std::shared_ptr<const EntityBase>& entity) {
    if (!entity) return false;
    for (auto& curve : curves_) {
        if (curve.GetID() == entity->GetID() && !curve.IsPointerSet()) {
            // ICurve へのダウンキャストを試みてポインタを設定
            if (auto ptr = std::dynamic_pointer_cast<const ICurve>(entity)) {
                return curve.SetPointer(ptr);
            }
        }
    }
    return false;
}
```

`GetUnresolvedReferences()`（`EntityBase`の公開メンバ）はDEフィールド由来の参照とPDフィールド由来の参照の両方を集約するため、`GetUnresolvedPDReferences()`はPD由来のもののみを返せばよい。DEフィールド側は`EntityBase`が自動的に処理する。

### PointerContainerのテンプレート引数の選択

```cpp
// PointerContainer<UseWeakPtr, Types...>
PointerContainer<false, ICurve>    // shared_ptr を使う
PointerContainer<true, EntityBase> // weak_ptr を使う
```

`UseWeakPtr`の選択基準は循環参照の有無によって決まる。

| UseWeakPtr | 使用するポインタ | 選ぶべき状況 |
| --- | --- | --- |
| `false` | `shared_ptr` | 親が子の所有権を持つ（物理的に従属する）場合。循環参照が生じない。 |
| `true` | `weak_ptr` | 論理的な参照のみで所有しない場合。参照先が別の場所で管理されている。 |

一般的なルールとして：

- エンティティAがエンティティBを物理的に従属として含む（BはAなしでは意味をなさない）場合は`false`
- エンティティAがエンティティBを参照するが、BはAとは独立して存在しうる場合は`true`

例えば、`CompositeCurve`の子要素である曲線は物理的に従属するが、IGESの仕様上`CompositeCurve`自体は子を所有せず`IgesData`が全エンティティを管理するため、`weak_ptr`（`true`）を使う実装もある。実際には既存エンティティの実装を参考にすること。

### DEフィールドからの参照（変換行列など）

DEレコードのフィールド（変換行列・色・ビューなど）が他のエンティティを参照している場合は、`EntityBase`が自動的に処理するためエンティティ側での実装は不要である。

具体的には、`SetDERecord()`がDEフィールドの参照解決済み`ObjectID`を`DEFieldWrapper`に格納し、`EntityBase::SetUnresolvedReference()`がそれらを自動的に解決する。

エンティティ側での実装が必要なのは、**PDレコードで参照する他エンティティ**のみである。

### GetChildIDsとGetChildEntityの役割

`GetChildIDs()`と`GetChildEntity()`は、**物理的に従属するエンティティ**（親なしでは存在意義のないもの）を返すためにオーバーライドする。

```cpp
// 物理的に従属する子要素を持つエンティティのみオーバーライドする
std::vector<ObjectID> CompositeCurve::GetChildIDs() const {
    std::vector<ObjectID> ids;
    for (const auto& curve : curves_) {
        ids.push_back(curve.GetID());
    }
    return ids;
}
```

`GetReferencedEntityIDs()`（`EntityBase`の公開メンバ）は`GetChildIDs()`の結果も含めて全参照のIDを返すため、両者を混同しないこと。論理的な参照のみで物理的従属がない場合は`GetChildIDs()`のオーバーライドは不要である。

## IGESParameterVector

`IGESParameterVector`は、PDレコードのパラメータ列（異なる型が混在する）を一つの配列として保持するクラスである。`pd_parameters_`（`EntityBase`のprotectedメンバ）の型であり、`SetMainPDParameters`（読み込み）と`GetMainPDParameters`（書き込み）の両方で中心的な役割を果たす。

### 格納できる型

IGESパラメータの6種類の型は、以下のC++型に対応する。

| C++型 | IGESパラメータ型 |
| --- | --- |
| `bool` | Logical |
| `int` | Integer |
| `double` | Real |
| `ObjectID` | Pointer（他エンティティへの参照） |
| `std::string` | String / Language Statement |

これらの型の値を一つの配列として混在させて保持できる。

### 値の追加・設定

```cpp
// 初期化子リストによる構築（GetMainPDParameters内での典型的な使い方）
IGESParameterVector params{1, 2.5, std::string("hello")};

// push_back（型は自動推論）
params.push_back(static_cast<int>(3));
params.push_back(1.0);
params.push_back(some_object_id);  // ObjectID（他エンティティへの参照）

// 既存インデックスへの上書き
params.set(0, 42);

// サイズ変更・容量確保
params.resize(10);         // int(0) で埋める
params.resize(10, 0.0);    // double(0.0) で埋める
params.reserve(20);
```

### 値の取得：getとaccess_asの違い

`get<T>(index)`は格納型が厳密に`T`でなければ`std::bad_variant_access`を投げる。

```cpp
double v = pd.get<double>(2);  // index 2 が double でなければ例外
```

`access_as<T>(index)`は型変換を伴う柔軟な取得である。`SetMainPDParameters`内で典型的に使う。

```cpp
auto n  = pd.access_as<int>(0);       // int として取得
auto id = pd.access_as<ObjectID>(1);  // 登録済みint IDから ObjectID に変換
```

変換規則は以下の通り。

| 取得型T | 変換が許容される場合 |
| --- | --- |
| `bool` | 格納値が`int`型かつ値が0または1 |
| `ObjectID` | 格納値が`int`型かつIDGeneratorに登録済みのID |
| `int`、`double`、`std::string` | 変換不可（厳密一致のみ） |
| （いずれの型でも） | `ValueFormat`の`is_default`が`true`の場合（省略値・空白）は常に変換可能 |

注意：`access_as`は変換が行われた場合、内部状態（格納型とフォーマット情報）を更新する。

ファイルから読み込んだ値の型が不定になりうる（例：省略値が`int`の0として格納される）ため、`SetMainPDParameters`では`get`ではなく`access_as`を使う。省略可能なポインタフィールドには`access_as<ObjectID>`ではなく`GetObjectIDFromParameters`を使うこと（詳細は[オプショナルな参照（未設定IDの扱い）](#オプショナルな参照未設定idの扱い)を参照）。

### GetObjectIDFromParameters

PDフィールドからポインタを読み込む際の推奨ユーティリティ関数である。`int`型（IGESファイル由来のDEポインタ）と`ObjectID`型の両方を透過的に扱う。

```cpp
// GetObjectIDFromParameters(parameters, index, de2id, allow_unset_id)
ObjectID id = GetObjectIDFromParameters(pd_parameters_, 1, de2id);
```

動作の詳細は以下の通り。

- 格納値が`int`型の場合：`std::abs(value)`を`de2id`で`ObjectID`に変換する
- 格納値が`ObjectID`型の場合：そのまま返す
- 値が`0`（`kInvalidIntID`）の場合：`allow_unset_id=true`なら`UnsetID`を返す、`false`なら例外
- `de2id`が空かつ`int`型の場合：例外（プログラム側構築時は`ObjectID`型で格納されているべき）

### 型確認とフォーマット操作

```cpp
pd.is_type<double>(i)      // そのインデックスが double かどうかを確認
pd.get_type(i)             // CppParameterType 列挙値を返す

pd.get_format(i)           // ValueFormat を取得
pd.set_format(i, fmt)      // ValueFormat を更新（型の変更は不可）
```

`ValueFormat`はIGESファイル上の数値表現（符号の有無・小数部の有無・指数形式・単精度/倍精度など）を保持する。`GetMainPDParameters`でのフォーマット引き継ぎパターンは[フォーマット保持](#フォーマット保持)を参照。

### その他の操作

```cpp
pd.size()                      // 要素数
pd.empty()                     // 空かどうか
pd.clear()                     // 全要素削除
pd.copy(idx_start, count)      // 部分コピーして新しいIGESParameterVectorを返す
pd.get_as_string(i, config)    // IGES文字列形式で取得（書き込み処理が内部で使用）
```

## シリアライズ

### pd_parameters_の役割

`pd_parameters_`（`IGESParameterVector`型、`EntityBase`のprotectedメンバ）は、PDレコードの「メインパラメータ部分」の生データを保持する。

- コンストラクタの引数`parameters`がそのまま`pd_parameters_`に設定される（`EntityBase`のコンストラクタで実施）
- その後`InitializePD()`が呼ばれ、`SetMainPDParameters()`で各メンバ変数に値が展開される
- この時点で`pd_parameters_`は追加ポインタ部分を除いた長さに切り詰められる

`pd_parameters_`は読み込み時のパラメータフォーマット情報（整数か実数か、表記の精度など）を保持しており、書き戻し時にそのフォーマットを再現するために参照する。

### GetMainPDParametersの実装パターン

`GetMainPDParameters()`は、メンバ変数の現在値を`IGESParameterVector`として返す。これが書き込み処理で使用されるエンティティのPDパラメータの本体となる。

```cpp
// CircularArc の実装例
IGESParameterVector CircularArc::GetMainPDParameters() const {
    IGESParameterVector params{
        center_[2],            // z_t: z座標
        center_[0],            // x_c: 中心x
        center_[1],            // y_c: 中心y
        start_point_[0],       // x_s: 始点x
        start_point_[1],       // y_s: 始点y
        terminate_point_[0],   // x_t: 終点x
        terminate_point_[1]};  // y_t: 終点y

    // 元のフォーマット情報を保持して返す（後述）
    for (size_t i = 0; i < std::min(params.size(), pd_parameters_.size()); ++i) {
        try { params.set_format(i, pd_parameters_.get_format(i)); }
        catch (const std::invalid_argument&) {}
    }
    return params;
}
```

他のエンティティへの参照を含む場合は、`ObjectID`をそのまま`push_back`する。書き込み処理が、`ObjectID` → DEポインタ番号の変換を担う。

```cpp
// CompositeCurve の実装例（参照を含む場合）
IGESParameterVector CompositeCurve::GetMainPDParameters() const {
    IGESParameterVector params;
    params.push_back(static_cast<int>(curves_.size()));

    for (const auto& curve : curves_) {
        params.push_back(curve.GetID());  // ObjectID を積む。書き込み時に変換される
    }
    // （フォーマット保持の処理は省略）
    return params;
}
```

### GetParametersの役割

`GetParameters()`（`EntityBase`の公開メンバ）は`GetMainPDParameters()`の結果に「追加ポインタ（Associativity Instance Entity・General Note等へのポインタ）」を連結して返す。

個別エンティティクラスでこの関数をオーバーライドする必要はない。`GetMainPDParameters()`のみを正しく実装すれば、追加ポインタの処理は`EntityBase`が行う。

```
GetParameters() の内部動作:
  GetMainPDParameters()     → エンティティ固有のパラメータ列
  + former_additional_pointers_ の数 + 各ID  （Type 402, 212, 312 等へのポインタ）
  + latter_additional_pointers_ の数 + 各ID  （プロパティ/属性テーブルへのポインタ）
```

### フォーマット保持

`IGESParameterVector`は各要素に「フォーマット情報」を付与できる。これはIGESファイル上の数値表現（例：`1.0` vs `1`、小数点以下の桁数など）を保持するためのものである。

`GetMainPDParameters()`では以下のパターンで元のフォーマットを引き継ぐことを推奨する。

```cpp
// 要素数が一定のエンティティ（CircularArc等）: 全要素にフォーマットを適用
for (size_t i = 0; i < std::min(params.size(), pd_parameters_.size()); ++i) {
    try { params.set_format(i, pd_parameters_.get_format(i)); }
    catch (const std::invalid_argument&) {}
}

// 要素数が可変のエンティティ（CompositeCurve等）: サイズが一致する場合のみ適用
if (params.size() == pd_parameters_.size()) {
    for (size_t i = 0; i < pd_parameters_.size(); ++i) {
        try { params.set_format(i, pd_parameters_.get_format(i)); }
        catch (const std::invalid_argument&) {}
    }
}
```

フォーマット保持はラウンドトリップ精度（読み込んで書き出した際の差異の最小化）のために重要である。新規生成のエンティティ（ファイルから読み込まずプログラムで生成したもの）については`pd_parameters_`が適切な初期値を持つため、このパターンを適用しても問題はない。

### GetRawEntityDEの役割

`GetRawEntityDE(id2de)`（`EntityBase`の公開メンバ）は、現在のDEフィールドの状態を`RawEntityDE`として返す。書き込み処理（`ConvertToIntermediate()`）がこれを呼び出す。

`id2de`引数（`id2pointer`型）は**ObjectID → DEポインタ番号**への変換表で、`pointer2ID`の逆変換である。`IgesData`が書き込み前に構築してこれを渡す。個別エンティティクラスで`GetRawEntityDE()`をオーバーライドする必要はない。

```
書き込み時の変換:
  IgesData::ConvertToIntermediate()
    ├─ 各エンティティに対して GetRawEntityDE(id2de) を呼ぶ
    │    └─ DEフィールド内のObjectID → id2de を使ってDEポインタ番号に変換
    └─ GetParameters() を呼ぶ
         ├─ GetMainPDParameters() 内のObjectID は書き込み側が変換
         └─ 追加ポインタのObjectID も同様
```
