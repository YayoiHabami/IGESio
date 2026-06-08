# IGESio利用ガイド（オブジェクト管理と描画システム）

本書は、IGESioライブラリにおける①オブジェクトの管理方法（`Assembly`の構造と扱い方）と、②描画システムの概観と扱い方を、利用者視点で網羅的にまとめたドキュメントである。各機構の内部設計や個別エンティティの詳細は、本文中で参照する各ドキュメントを参照する。

- モデル構造の詳細: [Assembly](./models/assembly_ja.md)、[座標フレームと変換ビュー](./models/coordinate_frames_ja.md)
- 描画システムの内部構成: [描画システム概要](./graphics/overview.md)、[クラスリファレンス](./graphics/class_reference.md)
- 各エンティティの生成・編集API: [entities](./entities/entities.md)

## 目次

- [目次](#目次)
- [全体アーキテクチャ](#全体アーキテクチャ)
- [オブジェクトの管理（models層）](#オブジェクトの管理models層)
  - [1. IgesDataとファイルI/O](#1-igesdataとファイルio)
  - [2. Assemblyツリーの構造](#2-assemblyツリーの構造)
  - [3. エンティティの生成と追加](#3-エンティティの生成と追加)
  - [4. クエリ・検証・空間検索](#4-クエリ検証空間検索)
  - [5. 構造編集](#5-構造編集)
  - [6. メタ情報・表示状態・リビジョン](#6-メタ情報表示状態リビジョン)
  - [7. 座標系と変換](#7-座標系と変換)
  - [8. セッション状態（Scene）](#8-セッション状態scene)
- [描画システム（graphics層）](#描画システムgraphics層)
  - [9. レンダラのセットアップ](#9-レンダラのセットアップ)
  - [10. 描画の基本フロー（Reconcile）](#10-描画の基本フローreconcile)
  - [11. ビュー状態の制御](#11-ビュー状態の制御)
    - [DisplayFilter（エンティティ型単位の表示フィルタ）](#displayfilterエンティティ型単位の表示フィルタ)
    - [マテリアルオーバーライド（エンティティ毎の描画プロパティ）](#マテリアルオーバーライドエンティティ毎の描画プロパティ)
    - [GraphicsSettings・背景・サイズ](#graphicssettings背景サイズ)
  - [12. カメラと光源](#12-カメラと光源)
    - [カメラ](#カメラ)
    - [光源](#光源)
  - [13. ピッキングと選択](#13-ピッキングと選択)
    - [クリックピック](#クリックピック)
    - [矩形選択](#矩形選択)
    - [ハイライトと選択粒度](#ハイライトと選択粒度)
  - [14. スクリーンショットと複数ビュー](#14-スクリーンショットと複数ビュー)
- [付録: 典型レシピ](#付録-典型レシピ)
  - [最小ビューア](#最小ビューア)
  - [モデル編集の自動反映](#モデル編集の自動反映)
  - [プログラムによるモデル構築と部品配置](#プログラムによるモデル構築と部品配置)
  - [型フィルタと一時的な塗り分け](#型フィルタと一時的な塗り分け)
  - [ヘッドレスでの選択操作](#ヘッドレスでの選択操作)

## 全体アーキテクチャ

IGESioは以下の層で構成される。上の層が下の層に依存し、逆方向の依存はない。

| 層 | 名前空間 | 役割 |
| --- | --- | --- |
| entities | `igesio::entities` | IGESエンティティ（曲線・曲面・変換行列等）の表現と幾何演算 |
| models | `igesio::models` | エンティティの集合管理（`Assembly`）、ファイルI/O用ラッパー（`IgesData`）、セッション状態（`Scene`） |
| graphics | `igesio::graphics` | OpenGLによる描画・ピッキング（`IGESIO_ENABLE_GRAPHICS`時のみ） |

幾何形状や表示状態などと派生キャッシュの分離されていることが重要な特徴である。

- 幾何形状等: `Assembly`ツリー（構造・配置・表示状態）と各エンティティ（形状定義）
- 派生キャッシュ: レンダラが保持する描画オブジェクト（GPUリソース）

モデル側への変更（エンティティの追加・削除、変換、表示状態、形状の編集）はリビジョンカウンタとして自動的に集約され、描画側が次回の描画・ピック時に検知して同期する。このため、**編集後にレンダラへ通知するAPIは存在しない**。利用者はモデルを編集し、`Draw()`を呼ぶだけでよい。

典型的なデータフローは次の通りである。

```
IGESファイル → ReadIges() → IgesData（ルートAssembly）
                                │ 編集（構造・形状・表示状態）
                                ▼
                          Scene（root共有+選択）
                                │ SetScene()
                                ▼
                       EntityRenderer::Draw() / PickEntities()
                                │
IGESファイル ← WriteIges() ←────┘（Assemblyツリーは非永続。エンティティのみ出力）
```

ヘッダは全部入りの`<igesio/igesio.h>`を基本とし、ビューア状態（`Scene`等）は`<igesio/scene.h>`で追加する。頻出型は`igesio::IgesData`のようにトップレベル名前空間の別名で参照できる。

## オブジェクトの管理（models層）

### 1. IgesDataとファイルI/O

`IgesData`はIGESファイル1つに対応する器であり、以下を保持する。

- `description`: スタートセクションに対応する説明文（`std::string`）
- `global_section`: グローバルセクションのパラメータ（`GlobalParam`）
- ルート`Assembly`: 全エンティティの集合（`Root()` / `RootPtr()`でアクセス）

```cpp
#include <igesio/igesio.h>

// 読み込み
igesio::IgesData data = igesio::ReadIges("model.iges");

// エンティティの操作はすべてルートAssemblyを介して行う
auto& root = data.Root();
auto ids = root.GetEntityIDs(/*recursive=*/true);

// 書き出し（Assemblyツリー構造は出力されず、エンティティがフラットに出力される）
bool ok = igesio::WriteIges(data, "out.iges");
```

読み込み時には参照解決（DE/PDフィールドのポインタからエンティティポインタへの解決）が自動的に行われる。書き出しはラウンドトリップの忠実性を保つため、読み込んだ元エンティティをそのまま出力する。アセンブリの配置（大域変換）を永続化したい場合は[7. 座標系と変換](#7-座標系と変換)の`Flatten`を用いる。

### 2. Assemblyツリーの構造

`Assembly`は再帰的なツリーノードであり、次を保持する。

- 所有エンティティの平坦マップ（`ObjectID`→`std::shared_ptr<EntityBase>`）
- 子`Assembly`のリスト（物理的な所有。入れ子式の論理ツリー構造）
- 大域変換行列（ノードの配置）
- メタ情報`AssemblyMetadata`と表示状態`DisplayState`（[6章](#6-メタ情報表示状態リビジョン)）
- モデルリビジョン（ルートに集約される変更カウンタ）

重要な不変条件として、1つのエンティティは1つの`Assembly`にのみ所属する。一方、エンティティ間の参照（DE/PDフィールドのポインタ）はツリー位置と無関係に張れる。所属の逆引きはルートが保持するインデックスにより`FindOwner(id)`が解決する。

```cpp
auto root = igesio::models::MakeAssembly("model");
auto part = igesio::models::MakeAssembly("part-A");
root->AddChildAssembly(part);

// エンティティID→所有ノードの逆引き
igesio::models::Assembly* owner = root->FindOwner(some_id);
```

`Assembly`は必ず`shared_ptr`として生成する必要があるため、ファクトリ`MakeAssembly(name)`を使用する。

物理従属の子（複合曲線の構成要素、トリム面の境界曲線等）はツリーの直接メンバとして扱われるが、論理的には親エンティティ経由で辿る存在であり、描画でも親の描画オブジェクトが子として描画する。共有される定義エンティティ（変換行列124・色定義314等）はルート直下に置き、どこからでも参照させる運用を推奨する。

### 3. エンティティの生成と追加

エンティティはプログラムから`igesio::entities`の`Make*`ファクトリ関数で生成する。主なものを示す（全リストは[entities](./entities/entities.md)を参照）。

| 分類 | 主なファクトリ |
| --- | --- |
| 曲線 | `MakeCircle` / `MakeCircularArc` / `MakeLine` / `MakeRay` / `MakeEllipse` / `MakeLinearPath` / `MakeCompositeCurve` / `MakeBezierCurve` / `MakeClampedBSplineCurve` / `MakeRationalBSplineCurve` |
| 曲面 | `MakeBezierSurface` / `MakeClampedBSplineSurface` / `MakeRationalBSplineSurface` / `MakeRuledSurface` / `MakeSurfaceOfRevolution` / `MakeTabulatedCylinder` / `MakeTrimmedSurface` / `MakeCurveOnAParametricSurface` |
| その他 | `MakeTransformationMatrix` / `MakeTranslation` / `MakeRotation` / `MakeColorDefinition`系 |

追加は`AddEntity`（1件）または`AddEntities`（一括。参照解決を1回で行うため大量追加の際に高速）を用いる。

```cpp
auto circle = igesio::entities::MakeCircle({3.0, 0.0}, 1.0);
auto color = igesio::entities::MakeColorDefinitionFromHex("#4C7FFF");
circle->OverwriteColor(color);

root->AddEntity(color);
root->AddEntity(circle);
// 一括追加: root->AddEntities({color, circle});
```

追加時には同一ノード内で未解決参照の解決（双方向）が自動的に行われる。

### 4. クエリ・検証・空間検索

列挙・検索系のAPIはいずれも`recursive`引数（既定`false`）で「直接の子のみ」か「全子孫」かを切り替える。

| メンバ関数 | 内容 |
| --- | --- |
| `GetEntity(id)` / `GetEntities()` / `GetEntityCount()` | 直接所有するエンティティの取得 |
| `GetEntityIDs(recursive)` | IDの列挙 |
| `FindEntitiesByType(type, recursive)` | `EntityType`による検索 |
| `FindEntitiesByUseFlag(flag, recursive)` | 用途フラグによる検索 |
| `FindEntities(predicate, recursive)` | 述語による検索 |
| `FindReferrers(id)` | idを参照する全エンティティの逆方向検索 |

検証系は次の通りである。

- `AreAllReferencesSet()` / `GetUnresolvedReferences()`: 参照解決の状態
- `IsReady()` / `Validate()`: 参照+各エンティティの妥当性（当該ノードのみ）
- `ValidateSelfContainedRecursive()`: 各ノードが参照を自己完結的に解決できるかの再帰検証（編集後の事後条件として使用）

空間検索・事前計算には以下を用いる。

- `GetWorldBoundingBox()`: 子孫の幾何メンバを包含するワールド空間AABB（`FitView`等の基礎）
- `PrepareGeometryCaches(recursive)`: 重い遅延キャッシュ（トリム面の領域判定等）の並列事前構築。読み込み・構造編集の完了後、並列読み取りや描画を始める前に1回呼ぶと初回アクセスのスパイクを避けられる

### 5. 構造編集

削除系は、他から参照されているエンティティの扱いを`RemovalPolicy`で指定する。

| `RemovalPolicy` | 意味 |
| --- | --- |
| `kReject` | 参照が残るなら削除を拒否する（既定） |
| `kCascade` | 参照元・物理従属子も連鎖削除する |
| `kOrphan` | 参照を未解決のまま削除する |

主な編集APIを示す。

| メンバ関数 | 内容 |
| --- | --- |
| `RemoveEntity(id, policy)` | エンティティの削除（このサブツリー内が対象） |
| `RemoveChildAssembly(child_id, policy)` | 直接の子`Assembly`の削除（サブツリーごと） |
| `Clear()` | このノードの全エンティティ・全子の一斉除去 |
| `MoveEntityTo(id, dest)` / `MoveChildAssemblyTo(child_id, dest)` | 同一ルート内での移動 |
| `AddChildAssembly(child)` | 子`Assembly`の追加 |
| `SetGlobalTransform(m)` / `ComposeGlobalTransform(m)` | ノード配置の設定・後付け合成 |

ロックは`Metadata().lock`（`selectable` / `editable`）で表現し、`IsEffectivelySelectable(id)` / `IsEffectivelyEditable(id)`が所有ノードからルートまでANDで畳んで判定する。`RemoveEntity`等の編集APIは編集ロックを尊重して失敗（`false`）を返す。

これらの編集はすべてモデルリビジョンへ自動集約されるため、編集後に描画側への通知は不要である（[6章](#6-メタ情報表示状態リビジョン)）。

### 6. メタ情報・表示状態・リビジョン

`Assembly`ノードの付随情報は、変更通知の要否で2つに分かれる。

- `Metadata()`（`AssemblyMetadata`）: `name`・`role_tag`・`lock`。参照時に読み込まれるため、mutable参照経由で自由に編集してよい

```cpp
node->Metadata().name = "wheel";
node->Metadata().lock.editable = false;
```

- `Display()`（`DisplayState`）: `visible`・`suppressed`・`color_override`・`opacity_override`。描画の派生キャッシュに影響するため、読み取りはconst参照、変更はsetter経由でのみ行う（値が変化したときのみリビジョンが進む）

```cpp
node->SetVisible(false);                       // サブツリーを非表示
node->SetSuppressed(true);                     // 論理的に除外（BBox・出力等からも除外）
node->SetColorOverride({{1.0f, 0.0f, 0.0f}});  // サブツリーを赤に塗る
node->SetOpacityOverride(0.5f);                // 半透明化（解除はstd::nullopt）
// 全子孫へ一括適用するSet*Recursive版もある
```

描画条件は`visible`かつ`!suppressed`である。色・不透明度のオーバーライドは描画時に最近接ノード優先で解決される。

変更検知は2層のリビジョンで行われる。

- モデルリビジョン（`Assembly::Revision()`）: 構造・大域変換・表示状態の変更毎にルートへ集約されて単調増加する。レンダラはこの値でツリーとの再同期（Reconcile）の要否を判定する
- ジオメトリリビジョン（`IEntityIdentifier::GeometryRevision()`）: エンティティ毎に、形状定義を変更するsetter（`SetControlPointAt`・`SetKnots`・`SetRadius`相当の各種編集API、`SetReference`等）の成功時に単調増加する。レンダラはこの値（同期キー）で再テッセレーションの要否を判定する

つまり、次のようなコードがそのまま動く。

```cpp
// 描画済みのNURBS曲線の制御点を動かす
curve->SetControlPointAt(1, {1.0, 2.0, 0.0});
// 次のDraw()で自動的に再テッセレーションされて反映される（通知不要）
renderer.Draw();
```

自作のラッパーやエンティティに形状を変更するsetterを追加する場合は、その末尾（成功経路のみ）で`MarkGeometryModified()`を呼ぶ規約に従うこと。

### 7. 座標系と変換

各`Assembly`ノードは大域変換を持ち、描画・空間検索ではルートからの累積変換が適用される。エンティティ自身もDEフィールド7の変換行列（Type 124）を参照できる。特定フレームでの座標を得るには`CoordFrame`を指定する。

- `CoordFrame::Definition()`: 定義空間（変換を一切適用しない）
- `CoordFrame::EntityLocal()`: エンティティ自身のフレーム（DE変換のみ）
- `CoordFrame::World()`: ワールド空間（全Assembly変換+DE変換）
- `CoordFrame::RelativeTo(id)`: 指定Assemblyの定義空間

```cpp
// 配置行列の解決
auto placement = root->ResolvePlacement(id, igesio::models::CoordFrame::World());

// 変換ビュー: 元エンティティを書き換えずに、指定フレームの座標を返す読み取り専用ビュー
auto view = root->GetCurveView(id, igesio::models::CoordFrame::World());
auto pt = view->TryGetPointAt(0.5);
```

`CurveView` / `SurfaceView`はスナップショット意味論であり、生成時点の累積変換を固定して保持する（後でAssembly変換を変えても追従しない）。

アセンブリ配置の永続化には`Flatten` / `Materialize`（`include/igesio/models/flatten.h`）を用いる。`Flatten(src)`は大域変換を各エンティティのDE変換（124）へ畳み込んだ新しい`IgesData`を返す。詳細は[座標フレームと変換ビュー](./models/coordinate_frames_ja.md)を参照する。

### 8. セッション状態（Scene）

`Scene`（`include/igesio/models/scene.h`、`<igesio/scene.h>`で公開）は、実行時のセッション状態を一元管理する。モデル構造（`Assembly`）と分離されているため、同じモデルを複数のセッションで共有できる。

```cpp
igesio::IgesData data = igesio::ReadIges("model.iges");
igesio::models::Scene scene(data.RootPtr());  // rootを共有して構築
```

保持する状態と主なAPIは次の通りである。

- 選択セット（`SelectionSet`）: 選択中のID集合とアンカー（主選択）。`Select` / `Deselect` / `Toggle` / `Replace` / `Clear` / `Contains` / `Items` / `Active`を持つ純粋クラスで、GL非依存にヘッドレスでも操作できる
- 複数の選択セット: `CreateSelectionSet()`で保存用のセットを追加作成し、`ActivateSelectionSet(id)`で切り替えられる（`SelectionSetIds()`で列挙）。描画のハイライトはアクティブセット（`ActiveSelection()`）を参照する
- ロック・フィルタを尊重した選択: `TrySelectWithLock(set, id)`は`lock.selectable`と`PickFilter`を尊重する対話ピック用の経路である（`SelectionSet::Select`自体は制限しない）
- Assembly粒度の選択: `SelectOwningAssembly(set, picked)` / `DeselectOwningAssembly(set, picked)`は、ピックされたエンティティの所有`Assembly`のメンバを一括選択・解除する。`Granularity()` / `SetGranularity()`（`kBody` / `kAssembly`）はGUIがどちらの経路を使うかの設定置き場である
- `Filter()`（`PickFilter`）: `faces` / `edges` / `vertices` / `bodies`の選択対象フィルタ（現状は`bodies`=エンティティ単位のみ尊重される）

```cpp
auto& sel = scene.ActiveSelection();
scene.TrySelectWithLock(sel, picked_id);   // ロックを尊重して選択
sel.Toggle(other_id);                      // 直接操作も可能
sel.Clear();
```

## 描画システム（graphics層）

### 9. レンダラのセットアップ

描画システムはCMakeオプション`IGESIO_ENABLE_GRAPHICS`で有効化し、OpenGL 4.3以上のcoreプロファイルコンテキストを要求する（macOSは対象外）。

`EntityRenderer`のセットアップ手順は次の通りである。

```cpp
#include <igesio/igesio.h>
#include <igesio/scene.h>

// 1. GLコンテキストが無い段階でも構築できる（glはnullptr可）
igesio::graphics::EntityRenderer renderer(nullptr, width, height);

// 2. コンテキストをカレント化した後にGLバックエンドを設定する
//    (GLFW環境の例。CreateGLBackendはGL 4.3未満でImplementationErrorを送出)
renderer.SetGLBackend(igesio::graphics::CreateGLBackend(
        reinterpret_cast<igesio::graphics::GLProcLoader>(glfwGetProcAddress)));

// 3. シェーダーのコンパイル・リンク
renderer.Initialize();   // IsInitialized()で確認できる

// 4. 描画対象のSceneを設定する（非所有。Sceneはレンダラより長く生存すること）
renderer.SetScene(&scene);
```

終了時は`Cleanup()`が全GPUリソースを解放する（デストラクタからも呼ばれる）。`Draw()` / `PickEntities()`等はGPUリソースの生成・解放を行うため、GLコンテキストがカレントなスレッドから呼ぶこと（単一スレッド前提）。

### 10. 描画の基本フロー（Reconcile）

描画はモデルへの編集を自動検知するため、フレームループは次だけでよい。

```cpp
while (running) {
    // モデルの編集（任意のタイミング・任意のAPI）
    // root->RemoveEntity(...), node->SetVisible(false),
    // curve->SetControlPointAt(...) など

    renderer.Draw();   // 必要な同期はすべて内部で行われる
}
```

`Draw()`（および`PickEntities`系）の冒頭では次の同期が行われる。

1. モデルリビジョンの比較: 前回同期時と一致すれば走査をスキップする（カメラ操作や選択変更だけの再描画ではツリーを走査しない）
2. Sweep: ツリーから削除されたエンティティの描画オブジェクトを破棄し、GPUリソースを解放する
3. ツリー走査: 表示対象を収集し、キャッシュ未在席の描画オブジェクトを遅延生成する。非表示・抑制サブツリー、物理従属エンティティ、型フィルタ除外はスキップする
4. ジオメトリ再同期: 形状が編集されたエンティティの描画オブジェクトのみ再テッセレーションする

描画オブジェクトキャッシュの生存条件を次に示す。

| 状態 | キャッシュ | 描画 | ピック |
| --- | --- | --- | --- |
| ツリーに存在・可視 | 存在（なければ生成） | ○ | ○ |
| `visible=false` / `suppressed` | 温存（再表示が安価） | × | × |
| `DisplayFilter`で除外 | 温存（生成は抑止） | × | × |
| ツリーから削除 | Sweepで破棄 | × | × |
| 生成失敗（未サポート型等） | 負キャッシュ | × | × |

エンティティの追加・削除・形状編集を行うためのレンダラ側APIは存在しない（すべてモデル側の操作として行う）。これが本ライブラリの描画システムの基本である。

### 11. ビュー状態の制御

以下はモデル（セッション）ではなくレンダラ毎に保持されるビュー状態であり、複数ビューで異なる設定にできる。

#### DisplayFilter（エンティティ型単位の表示フィルタ）

```cpp
igesio::graphics::DisplayFilter filter;
filter.hidden_types.insert(igesio::entities::EntityType::kCircularArc);
renderer.SetDisplayFilter(filter);      // 円弧を非表示（描画もピックもされない）
renderer.SetDisplayFilter({});          // 解除（キャッシュは温存されており再生成しない）
```

#### マテリアルオーバーライド（エンティティ毎の描画プロパティ）

`MaterialProperty`（`ambient_strength` / `specular_strength` / `shininess` / `opacity` / `texture`）をエンティティ単位で上書きする。「このエンティティの素の見た目をこのビューでは何にするか」を表し、IGES側の色情報は変更しない。

```cpp
igesio::graphics::MaterialProperty mat;
mat.opacity = 0.3f;
mat.shininess = 128;
renderer.SetMaterialProperty(surface_id, mat);   // 適用は次回の描画/ピック時
renderer.ClearMaterialProperty(surface_id);      // デフォルトへ戻す
```

setterはGLコンテキスト前提を持たない（GL操作は次回描画時に遅延実行される）ため、任意のタイミングで呼べる。ツリーから削除されたIDの設定は自動的に破棄される。

#### GraphicsSettings・背景・サイズ

- `EnableAntialiasing(bool)` / `EnableTransparency(bool)`: MSAA・ブレンドの有効化（半透明描画には`EnableTransparency(true)`が必要）
- `SetDisplayMode(mode)`: `kShaded`（面+エッジ）/ `kWireFrame`（エッジのみ）/ `kNoEdge`（面のみ）。独立した曲線エンティティは常に描画される
- `SetBackgroundColor(r, g, b, a)` / `GetBackgroundColorRef()`
- `SetDisplaySize(w, h)`: ウィンドウリサイズ時に呼ぶ（反映は次回`Draw()`）

色の解決は「エンティティ固有色 → マテリアルオーバーライド → `DisplayState`の色/不透明度（最近接優先） → 選択ハイライト」の順に上書きされる。

### 12. カメラと光源

#### カメラ

`renderer.Camera()`で参照を取得して操作する。

- 配置: `SetPosition` / `SetTarget` / `SetUp` / `Reset()`
- 対話操作: `Rotate(dx, dy)`（ターゲット周りの軌道）/ `Pan(dx, dy)` / `Zoom(delta)`
- 定型ビュー: `SetStandardView(StandardView::kFront / kBack / kTop / kBottom / kRight / kLeft)`
- 投影: `SetProjectionMode(ProjectionMode::kPerspective / kOrthographic / kOblique)`、`SetFov`、斜投影の`SetObliqueFactors`
- フィット: `renderer.FitView()`がシーン全体のワールドBBoxと現在のアスペクト比から自動調整する（`Camera::FitToBoundingBox`を直接呼ぶこともできる）

near/farクリッピング面は通常、自動クリッピングで決定される。レンダラがシーンの外接球を自動登録し（`SetScene`時と描画時の同期で更新）、カメラ位置に応じてnear/farが毎回導出されるため、ズームやパンでシーンがクリップされない。`SetClippingPlanes(near, far)`を呼ぶと自動クリッピングは解除され手動値に固定される（`ClearAutoClipSphere()`はカメラ側の解除API）。

なお、`kOblique`投影ではピッキング（レイ生成・矩形選択）が未対応である。

#### 光源

`renderer.Lights()`で`std::vector<Light>`の参照を取得して編集する。既定では平行光源1つを保持し、最大`kMaxLights`（8個）まで描画に使用される。

```cpp
auto& lights = renderer.Lights();
lights[0].SetDirectional({-1.0f, -1.0f, -1.0f});   // 平行光源（方向）

igesio::graphics::Light point;
point.SetPoint({0.0f, 5.0f, 0.0f},                 // 点光源: 位置
               {1.0f, 0.1f, 0.0f},                 // 距離減衰係数 (C, L, Q)
               {1.0f, 0.9f, 0.8f, 1.0f});          // 色 (RGBA)
lights.push_back(point);
```

光源計算は曲面系シェーダーのみが行う（曲線・点の描画は光源と無関係）。

### 13. ピッキングと選択

ピッキングは「レンダラがヒットIDを返し、選択の確定は`Scene`が行う」という分担である。可視リストを走査するため、削除済み・非表示・抑制中・型フィルタ除外のエンティティは構造的にヒットしない。

#### クリックピック

```cpp
// スクリーン座標（左上原点）からワールド空間のレイを生成
auto ray = renderer.GetRayFromScreen(mouse_x, mouse_y);

// distance昇順のヒットリスト（曲線のヒット許容量はピクセル基準で自動換算）
auto hits = renderer.PickEntities(ray, mouse_x, mouse_y);
if (!hits.empty()) {
    const auto& nearest = hits.front();   // {id, hit}（hit.position / hit.distance）
    scene.TrySelectWithLock(scene.ActiveSelection(), nearest.id);
}
```

探索の細部（サンプル数・許容ピクセル・重複除去距離等）は`RayIntersectionParams`で調整できる。

#### 矩形選択

```cpp
igesio::graphics::ScreenRect rect{x_min, y_min, x_max, y_max};
auto ids = renderer.PickEntitiesInRect(
        rect, igesio::graphics::BoxSelectionMode::kCrossing);  // 交差選択
// kContained: エンティティ全体が矩形内に収まる場合のみ選択
for (const auto& id : ids) scene.TrySelectWithLock(scene.ActiveSelection(), id);
```

サンプリングは`SelectionSampleParams`で調整できる。

#### ハイライトと選択粒度

選択中のエンティティは次回`Draw()`から自動的にハイライト色で描画される（レンダラが`scene.ActiveSelection()`を毎フレーム参照するため、選択変更だけならツリー再走査は起きない）。Assembly単位の一括選択は`Scene::SelectOwningAssembly`を用いる（[8章](#8-セッション状態scene)）。

ピックはモデルを解析的に直接判定するため、`Draw()`を一度も呼んでいなくても正しく動作する（必要な内部同期はピック側でも行われる）。ただし描画オブジェクトの遅延生成コストがピック側に乗るため、通常は描画後に行うのが効率的である。

### 14. スクリーンショットと複数ビュー

- `CaptureScreenshot()`: 現在の描画状態をオフスクリーンFBOへ描画して`Texture`として返す。`SaveTextureToFile(path, texture)`でファイル保存できる
- 複数ビュー: 同一の`Scene`を複数の`EntityRenderer`へ`SetScene`してよい。各レンダラが独立に同期状態・ビュー状態（カメラ・フィルタ・マテリアル・表示モード）を保持するため、同じモデルを異なる見た目で同時表示できる。モデル編集は全ビューへ自動反映される

## 付録: 典型レシピ

### 最小ビューア

```cpp
igesio::IgesData data = igesio::ReadIges("model.iges");
igesio::models::Scene scene(data.RootPtr());

igesio::graphics::EntityRenderer renderer(gl, 1280, 720);
renderer.Initialize();
renderer.SetScene(&scene);
renderer.FitView();

while (running) {
    renderer.Draw();
    // バッファスワップ等はウィンドウシステム側で行う
}
```

### モデル編集の自動反映

```cpp
// 構造編集: 削除はAssemblyへ。描画オブジェクトは次のDrawでSweepされる
root.RemoveEntity(id, igesio::models::RemovalPolicy::kCascade);

// 表示状態: setter経由ならリビジョンが進み自動反映される
node->SetVisibleRecursive(false);

// 形状編集: ジオメトリリビジョンにより該当オブジェクトのみ再テッセレーション
surface->SetControlPointAt(0, 0, {0.0, 0.0, 0.5});

renderer.Draw();   // ここで全部まとめて反映される
```

### プログラムによるモデル構築と部品配置

```cpp
auto root = igesio::models::MakeAssembly("scene");
auto wheel = igesio::models::MakeAssembly("wheel");
root->AddChildAssembly(wheel);

wheel->AddEntity(igesio::entities::MakeCircle({0.0, 0.0}, 1.0));

// 部品をx=5へ配置（描画時に累積変換として子孫へ波及する）
igesio::Matrix4d m = igesio::Matrix4d::Identity();
m(0, 3) = 5.0;
wheel->SetGlobalTransform(m);

igesio::models::Scene scene(root);
renderer.SetScene(&scene);
```

### 型フィルタと一時的な塗り分け

```cpp
// このビューではNURBS曲面以外を隠す
igesio::graphics::DisplayFilter filter;
for (auto type : present_types) {
    if (type != igesio::entities::EntityType::kRationalBSplineSurface) {
        filter.hidden_types.insert(type);
    }
}
renderer.SetDisplayFilter(filter);

// 解析結果の強調: サブツリーを赤の半透明に
target_node->SetColorOverride({{1.0f, 0.0f, 0.0f}});
target_node->SetOpacityOverride(0.4f);
renderer.EnableTransparency(true);
```

### ヘッドレスでの選択操作

```cpp
// GUIなしでも、モデルとSceneだけで選択集合を構築・保存できる
igesio::models::Scene scene(data.RootPtr());
auto saved = scene.CreateSelectionSet();
auto& sel = scene.ActiveSelection();
for (const auto& e : root.FindEntitiesByType(
        igesio::entities::EntityType::kTrimmedSurface, true)) {
    sel.Select(e->GetID());
}
scene.ActivateSelectionSet(saved);   // 別の作業セットへ切替（元の選択は保持される）
```
