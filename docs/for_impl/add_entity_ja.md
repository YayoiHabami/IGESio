# エンティティの追加手順

本ドキュメントでは、本ライブラリに新たなIGESエンティティを追加する際の手順を説明する。エンティティクラスの作成から、パーサーへの登録、描画対応まで、必要な作業を順に示す。

## 目次

- [目次](#目次)
- [概要](#概要)
  - [エンティティの分類](#エンティティの分類)
  - [追加が必要なファイル一覧](#追加が必要なファイル一覧)
- [ステップ1: EntityType列挙体への追加](#ステップ1-entitytype列挙体への追加)
- [ステップ2: エンティティクラスのヘッダー作成](#ステップ2-エンティティクラスのヘッダー作成)
  - [基本構造](#基本構造)
  - [インターフェースの選択](#インターフェースの選択)
  - [実装が必要な純粋仮想関数](#実装が必要な純粋仮想関数)
- [ステップ3: エンティティクラスのソースファイル作成](#ステップ3-エンティティクラスのソースファイル作成)
  - [コンストラクタ](#コンストラクタ)
  - [PDパラメータの実装](#pdパラメータの実装)
  - [ICurve / ISurfaceの実装](#icurve--isurfaceの実装)
- [ステップ4: EntityFactoryへの登録](#ステップ4-entityfactoryへの登録)
- [ステップ5: 描画コードの追加](#ステップ5-描画コードの追加)
  - [描画システムの構成](#描画システムの構成)
  - [描画対応の方針](#描画対応の方針)
- [ステップ6: CMakeLists.txtへの追加](#ステップ6-cmakeliststxtへの追加)
- [チェックリスト](#チェックリスト)
  - [基本実装](#基本実装)
  - [描画対応 (汎用クラスで十分な場合)](#描画対応-汎用クラスで十分な場合)
  - [描画対応 (専用Graphicsクラスを作成する場合)](#描画対応-専用graphicsクラスを作成する場合)

## 概要

IGESファイルの各エンティティは、`EntityBase`を継承した個別クラスとして実装する。読み込み時には`EntityFactory`がエンティティタイプ番号に基づいて適切なクラスのインスタンスを生成し、`IgesData`が`std::unordered_map<ObjectID, std::shared_ptr<EntityBase>>`として管理する。

エンティティを追加する際の主な流れは以下の通り。

```
entity_type.h  →  エンティティ/クラスのヘッダー  →  ソース実装  →  factory.cpp登録
                                                                   ↓
                                              (描画対応が必要な場合) graphics/factory.cpp等
```

### エンティティの分類

IGESのエンティティは大きく以下の種別に分類される。追加するエンティティがどれに該当するかを最初に確認すること。

| 種別 | 代表例 | 継承するインターフェース |
| --- | --- | --- |
| 曲線 (2次元定義空間) | CircularArc, ConicArc | `EntityBase` + `ICurve2D` |
| 曲線 (3次元定義空間) | Line, RationalBSplineCurve | `EntityBase` + `ICurve3D` |
| 曲面 | RuledSurface, RationalBSplineSurface | `EntityBase` + `ISurface` |
| 変換 | TransformationMatrix | `EntityBase` + `ITransformation` |
| 構造/その他 | ColorDefinition, NullEntity | `EntityBase` のみ |

### 追加が必要なファイル一覧

| ファイル | 変更内容 |
| --- | --- |
| `include/igesio/entities/entity_type.h` | `EntityType`列挙体に値を追加 |
| `include/igesio/entities/curves/` or `surfaces/` | エンティティクラスのヘッダー (新規作成) |
| `src/entities/curves/` or `surfaces/` | エンティティクラスの実装 (新規作成) |
| `src/entities/factory.cpp` | `EntityFactory::Initialize()`への登録 |
| `CMakeLists.txt` | ソースファイルの追加 |
| (描画対応する場合) 複数ファイル | 下記ステップ5を参照 |

## ステップ1: EntityType列挙体への追加

`include/igesio/entities/entity_type.h`の`EntityType`列挙体に新しい値を追加する。値はIGES 5.3仕様書に記載されたエンティティタイプ番号に合わせる。

```cpp
// entity_type.h (抜粋)
enum class EntityType : uint16_t {
    // ...既存の定義...
    kMyNewEntity = NNN,   // NNN: IGES仕様書のエンティティタイプ番号
    // ...
};
```

同ファイルにある`ToEntityType()`および`ToString()`の実装ファイル(`src/entities/entity_type.cpp`等)も対応して更新が必要な場合は修正すること。

## ステップ2: エンティティクラスのヘッダー作成

`include/igesio/entities/curves/`（曲線）または`include/igesio/entities/surfaces/`（曲面）にヘッダーファイルを作成する。

### 基本構造

```cpp
// include/igesio/entities/curves/my_new_entity.h

#ifndef IGESIO_ENTITIES_CURVES_MY_NEW_ENTITY_H_
#define IGESIO_ENTITIES_CURVES_MY_NEW_ENTITY_H_

#include "igesio/entities/entity_base.h"
#include "igesio/entities/interfaces/i_curve.h"  // 曲線の場合

namespace igesio::entities {

/// @brief MyNewEntity (Type NNN) の説明
class MyNewEntity : public EntityBase, public virtual ICurve3D {
 protected:
    // エンティティのパラメータを格納するメンバ変数
    Vector3d some_point_;
    double some_value_;

    /// @brief PDパラメータのメイン部分を取得する (シリアライズ用)
    IGESParameterVector GetMainPDParameters() const override;

    /// @brief PDパラメータを解析してメンバ変数に設定する
    /// @return 処理したパラメータの終端インデックス
    size_t SetMainPDParameters(const pointer2ID&) override;

 public:
    /// @brief IGESファイル読み込み用コンストラクタ
    MyNewEntity(const RawEntityDE&, const IGESParameterVector&,
                const pointer2ID& = {}, const ObjectID& = IDGenerator::UnsetID());

    // (必要に応じて) パラメータ指定による直接生成コンストラクタ
    // MyNewEntity(const Vector3d&, double, ...);

    /// @brief PDパラメータの妥当性検証
    ValidationResult ValidatePD() const override;

    // --- ICurve / ISurface 由来の純粋仮想関数 ---
    std::array<double, 2> GetParameterRange() const override;
    bool IsClosed() const override;
    std::optional<CurveDerivatives>
    TryGetDerivatives(const double, const unsigned int) const override;
    numerics::BoundingBox GetDefinedBoundingBox() const override;

    // --- 描画用アクセサ (必要な場合) ---
    Vector3d SomePoint() const { return some_point_; }
    double SomeValue() const { return some_value_; }

 protected:
    std::optional<Vector3d> Transform(
            const std::optional<Vector3d>& input,
            const bool is_point) const override {
        return TransformImpl(input, is_point);
    }
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_CURVES_MY_NEW_ENTITY_H_
```

### インターフェースの選択

追加するエンティティの種類に応じて、継承するインターフェースを選択する。

| 種別 | 継承するインターフェース | 定義ファイル |
| --- | --- | --- |
| 2次元定義空間の曲線 (例: 円弧) | `ICurve2D` | `interfaces/i_curve.h` |
| 3次元定義空間の曲線 (例: 直線、NURBS曲線) | `ICurve3D` | `interfaces/i_curve.h` |
| 曲面 | `ISurface` | `interfaces/i_surface.h` |
| 変換行列 | `ITransformation` | `interfaces/i_transformation.h` |
| 上記以外 (構造エンティティ等) | なし (`EntityBase`のみ) | - |

`ICurve2D`と`ICurve3D`はどちらも`ICurve`を継承している。描画システムでは`ICurve`ポインタによる統一的な扱いが行われるため、曲線であれば必ずこれらのいずれかを継承すること。

### 実装が必要な純粋仮想関数

`EntityBase`由来で必ず実装が必要なもの:

| 関数 | 説明 |
| --- | --- |
| `ValidatePD() const` | PDパラメータの妥当性を検証し、`ValidationResult`を返す |
| `GetMainPDParameters() const` | PDパラメータのメイン部分を`IGESParameterVector`として返す |
| `SetMainPDParameters(const pointer2ID&)` | PDパラメータを解析してメンバ変数に設定し、終端インデックスを返す |

`ICurve`由来で必ず実装が必要なもの:

| 関数 | 説明 |
| --- | --- |
| `GetParameterRange() const` | パラメータ範囲 `{t_start, t_end}` を返す |
| `IsClosed() const` | 曲線が閉じているかを返す |
| `TryGetDerivatives(double, unsigned int) const` | パラメータ $t$ における $n$ 階導関数を返す |
| `GetDefinedBoundingBox() const` | 定義空間における軸平行バウンディングボックスを返す |
| `Transform(optional<Vector3d>, bool) const` (protected) | 変換行列を適用して座標/ベクトルを変換する |

`ISurface`由来で必ず実装が必要なもの:

| 関数 | 説明 |
| --- | --- |
| `GetParameterRange() const` | パラメータ範囲 `{{u_min, u_max}, {v_min, v_max}}` を返す |
| `IsUClosed() const` | u方向に閉じているかを返す |
| `IsVClosed() const` | v方向に閉じているかを返す |
| `TryGetDerivatives(double, double, unsigned int) const` | パラメータ $(u, v)$ における偏導関数を返す |
| `GetDefinedBoundingBox() const` | 定義空間における軸平行バウンディングボックスを返す |
| `Transform(optional<Vector3d>, bool) const` (protected) | 変換行列を適用して座標/ベクトルを変換する |

## ステップ3: エンティティクラスのソースファイル作成

`src/entities/curves/`（または`src/entities/surfaces/`）にCPPファイルを作成する。

### コンストラクタ

IGESファイル読み込み用のコンストラクタは、DEレコードと`IGESParameterVector`を受け取る形式とする。内部実装は`EntityBase`のコンストラクタに引数を渡した後、`SetMainPDParameters()`を呼び出す形式が標準的。

> 参照解決・シリアライズの内部機構の詳細は[エンティティ実装の内部詳細](entity_internals_ja.md)を参照すること。

```cpp
// src/entities/curves/my_new_entity.cpp

#include "igesio/entities/curves/my_new_entity.h"

#include "igesio/common/error.h"

namespace {
namespace i_ent = igesio::entities;
}

i_ent::MyNewEntity::MyNewEntity(
        const RawEntityDE& de, const IGESParameterVector& parameters,
        const pointer2ID& de2id, const ObjectID& iges_id)
        : EntityBase(de, parameters, de2id, iges_id) {
    // SetMainPDParameters は EntityBase コンストラクタ内で呼ばれる場合と
    // 明示的に呼ぶ場合がある。既存実装を参照すること。
}
```

### PDパラメータの実装

`SetMainPDParameters()`では、`IGESParameterVector`から順に値を取り出してメンバ変数に設定する。`GetMainPDParameters()`では逆の操作でメンバ変数から`IGESParameterVector`を生成する。

既存の`CircularArc`（`src/entities/curves/circular_arc.cpp`）などを参考にすること。

### ICurve / ISurfaceの実装

`TryGetDerivatives()`が最も重要な実装となる。パラメータ $t$ における位置 $\mathbf{C}(t)$ と各次の導関数 $\mathbf{C}'(t), \mathbf{C}''(t), \ldots$ を計算して返す。計算できない場合は`std::nullopt`を返す。

`GetDefinedBoundingBox()`では、定義空間における幾何形状を包む軸平行な直方体を返す。ただし、定義空間はDEレコードの変換行列を適用する前の座標系であることに注意すること。

## ステップ4: EntityFactoryへの登録

`src/entities/factory.cpp`に以下の2箇所を修正する。

**1. インクルードの追加**

```cpp
// factory.cpp の先頭付近
#include "igesio/entities/curves/my_new_entity.h"  // type NNN
```

**2. Initialize()への追加**

```cpp
void i_ent::EntityFactory::Initialize() {
    // ... 既存の登録 ...

    // NNN - MyNewEntity
    creators_[ET::kMyNewEntity] = [](const DE& de, const IVec& p,
                                     const p2I& d2i, const ObjectID& iid) {
        return std::make_shared<i_ent::MyNewEntity>(de, p, d2i, iid);
    };

    // ...
    initialized_ = true;
}
```

フォーム番号によって異なるクラスを生成する必要がある場合（CopiousDataの例を参照）は、`de.form_number`を参照してラムダ内で分岐すること。

## ステップ5: 描画コードの追加

### 描画システムの構成

描画システムは以下の構成になっている。詳細は[`docs/graphics/overview.md`](../graphics/overview.md)を参照すること。

```
EntityRenderer
  └─ draw_objects_: ShaderType → EntityID → IEntityGraphics
                                              ↑
                               CreateEntityGraphics()  (factory.cpp)
                                 ├─ ICurve → CreateCurveGraphics()
                                 │    └─ dynamic_cast で具体型を判定
                                 │       マッチしない場合は ICurveGraphics (汎用)
                                 └─ ISurface → CreateSurfaceGraphics()
                                      └─ 同様
```

`IEntityGraphics`は型消去基底クラスであり、各エンティティに対応した`EntityGraphics<T, ShaderType_>`テンプレートクラスの具体型として実装される。

描画ループでは`EntityRenderer::Draw()`が`ShaderType`ごとにGLSLシェーダープログラムを切り替えながら、登録された`IEntityGraphics`の`Draw()`を順に呼び出す。

### 描画対応の方針

追加するエンティティが`ICurve`または`ISurface`を継承している場合は、汎用クラスが自動で適用されるため、追加作業が不要なケースがある。

| 方針 | 適用条件 |
| --- | --- |
| **追加作業なし** | `ICurve`を継承し、`GetParameterRange()`と`TryGetDerivatives()`が正しく実装されていれば`ICurveGraphics`が自動的に使用される。`ISurface`に対しては`ISurfaceGraphics`。まず汎用描画で動作を確認し、必要に応じて専用実装に移行することを推奨する。 |
| **専用Graphicsクラスを作成** | 特殊な描画モード（GPU上でのテッセレーション、ジオメトリシェーダー等）が必要な場合や、汎用クラスでは精度・パフォーマンスが不十分な場合。 |
| **CompositeEntityGraphicsを使用** | 子エンティティをそれぞれ独立して描画する複合エンティティの場合。 |

専用`Graphics`クラスを作成する場合の手順は[`docs/graphics/add_entity_graphics.md`](../graphics/add_entity_graphics.md)を参照すること。そちらのドキュメントに、ヘッダーの作成からGLSLシェーダーの登録、`factory.cpp`への追加まで詳細な手順が記載されている。

## ステップ6: CMakeLists.txtへの追加

`src/entities/curves/`（または`src/entities/surfaces/`）に追加したCPPファイルをプロジェクトのCMakeLists.txtに追加する。

描画クラスのCPPを追加した場合も同様に`src/graphics/curves/`等のファイルを追加する必要がある。

---

## チェックリスト

### 基本実装

- [ ] `EntityType`列挙体への値の追加 (`include/igesio/entities/entity_type.h`)
- [ ] エンティティクラスのヘッダー作成 (`include/igesio/entities/curves/` or `surfaces/`)
  - [ ] `EntityBase` + 適切なインターフェースの継承
  - [ ] IGESファイル読み込み用コンストラクタの宣言
  - [ ] `ValidatePD()`・`GetMainPDParameters()`・`SetMainPDParameters()`の宣言
  - [ ] `ICurve` or `ISurface`の純粋仮想関数の宣言
  - [ ] `Transform()`のオーバーライド (protected)
  - [ ] 描画用アクセサメンバ関数の宣言 (描画クラスが必要とするもの)
- [ ] エンティティクラスのソースファイル作成 (`src/entities/curves/` or `surfaces/`)
  - [ ] コンストラクタの実装
  - [ ] `SetMainPDParameters()`の実装 (PDデータの解析・メンバ変数への設定)
  - [ ] `GetMainPDParameters()`の実装 (シリアライズ用)
  - [ ] `ValidatePD()`の実装
  - [ ] `TryGetDerivatives()`の実装
  - [ ] `GetParameterRange()`の実装
  - [ ] `IsClosed()` or `IsUClosed()` / `IsVClosed()`の実装
  - [ ] `GetDefinedBoundingBox()`の実装
- [ ] `src/entities/factory.cpp`への登録
  - [ ] インクルードの追加
  - [ ] `Initialize()`内のラムダ関数の追加
- [ ] CMakeLists.txtへのソースファイルの追加

### 描画対応 (汎用クラスで十分な場合)

- [ ] 追加作業なし (フォールバックが自動適用される)

### 描画対応 (専用Graphicsクラスを作成する場合)

[`add_entity_graphics.md`](../graphics/add_entity_graphics.md)のチェックリストを参照すること。
