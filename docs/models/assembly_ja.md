# Assembly（エンティティの階層グルーピング）

## 目次

- [目次](#目次)
- [概要](#概要)
- [Assemblyの構成](#assemblyの構成)
- [所有と参照の二層](#所有と参照の二層)
- [論理ツリーに入れないもの](#論理ツリーに入れないもの)
- [子要素のクエリ](#子要素のクエリ)
- [構造編集](#構造編集)
- [空間検索](#空間検索)
- [Sceneとの関係](#sceneとの関係)
- [入出力との関係](#入出力との関係)

## 概要

`Assembly`は、複数のエンティティをまとめる塊としての責務を担う、`igesio::models`名前空間のクラスである。再帰的なツリーノードであり、子`Assembly`を通じて入れ子構造（論理ツリー）を表現する。メモリ上の構造であり、IGESファイルへは直接シリアライズしない。

座標系・配置の扱い（変換ビュー、フレーム指定、書き出し時のフラット化）については[座標フレームと変換ビュー](./coordinate_frames_ja.md)を参照する。

> 類似の`IgesData`はIGESファイル固有の情報（スタートセクション、グローバルセクション）も兼ねているが、エンティティの塊としての責務は`Assembly`が担い、`IgesData`はIGESファイルI/Oのための器として扱っている。`IgesData`は以下の要素を保持するラッパーである。
>
> - `description`: IGESファイルのスタートセクションに対応する説明文
> - `global_section`: グローバルセクションのパラメータ（`GlobalParam`）
> - ルート`Assembly`: エンティティの集合を保持する（`std::shared_ptr<Assembly>`として合成。継承ではない）
>
> エンティティの追加・取得・参照解決・検証は、すべてルート`Assembly`を介して操作する。
>
> ```cpp
> igesio::models::IgesData iges = igesio::ReadIges("model.iges");
> 
> // ルートAssemblyを介してエンティティを操作する
> auto& root = iges.Root();
> auto ids = root.GetEntityIDs(/*recursive=*/true);
> auto entity = root.GetEntity(ids.front());
> ```
>
> `IgesData`はルート`Assembly`を`shared_ptr`で保持し、`RootPtr()`でそれを共有できる。実行時のセッション状態（選択など）を載せる[`Scene`](#sceneとの関係)は、このルートを共有して構築する。

## Assemblyの構成

`Assembly`（`include/igesio/models/assembly.h`）は次を保持する。

- 所有エンティティの平坦マップ`std::unordered_map<ObjectID, std::shared_ptr<entities::EntityBase>>`
- 子`Assembly`のリスト（物理的な所有。入れ子＝論理ツリー）
- 親`Assembly`への弱参照（`std::weak_ptr`。ルートでは空。経路探索用）
- 大域変換行列`Matrix4d`（Solid Assembly Type 184相当の配置。既定は単位行列）
- メタ情報`AssemblyMetadata`と表示状態`DisplayState`
- モデルリビジョン（ルートに集約される変更カウンタ）

メタ情報`AssemblyMetadata`は、参照時にlive読みされる注釈・対話ロックである。描画の派生キャッシュへ影響しないため、`Metadata()`のmutable参照経由で自由に編集してよい（変更通知は不要）。

| メンバ | 意味 |
| --- | --- |
| `name` | アセンブリの名前 |
| `role_tag` | 役割を示す任意の分類用文字列 |
| `lock` | ロック状態（`selectable`・`editable`） |

表示状態`DisplayState`は、描画の派生キャッシュに影響する状態である。`Display()`のconst参照で読み、変更は`SetVisible`・`SetSuppressed`・`SetColorOverride`・`SetOpacityOverride`の各setter経由でのみ行う（値が変化したときのみモデルリビジョンが進む）。

| メンバ | 意味 |
| --- | --- |
| `visible` | 可視性（表示トグル）。非表示でも論理的には存在し、BBox・検証・出力・クエリには含まれる |
| `suppressed` | 抑制。論理的にモデルから除外する（子孫も連鎖）。抑制時は描画されず、BBox・検証・出力・クエリからも除外される |
| `color_override` | 色のオーバーライド（RGB; $[0, 1]$ ）。未設定ならメンバの色を使用 |
| `opacity_override` | 不透明度のオーバーライド（ $[0, 1]$ ）。未設定ならメンバの値を使用 |

描画条件は`visible`かつ`!suppressed`である。

モデルリビジョンは、構造（エンティティ・子`Assembly`の追加/削除/移動）・大域変換・表示状態の変更毎にルートへ集約されて単調増加するカウンタであり、`Revision()`で取得する（非ルートノードでもルートの値を返す）。描画層はこの値の変化で再同期の要否を判定するため、編集後の手動通知は不要である。

## 所有と参照の二層

一つのエンティティは厳密に一つの`Assembly`にのみ所属する（専属＝厳密なツリー）。この帰結として、次の二層を明確に分ける。

- 所属（membership）: 平坦マップへの登録。一つの`Assembly`に排他的に属する。
- 参照（reference）: DE/PDフィールドのポインタ。ツリー位置に無関係に張れる。

エンティティID→所属`Assembly`の逆引きは、ルート`Assembly`のみが保持する逆引きインデックス（`ObjectID → Assembly*`）が`FindOwner`によりO(1)で解決する。構造編集時のみ更新される。

```cpp
// ピッキング結果などのエンティティIDから、所有Assemblyを引く
igesio::models::Assembly* owner = root.FindOwner(picked_id);
```

エンティティ側に親`Assembly`へのポインタは持たせない。`EntityBase`は`igesio::entities`、`Assembly`は`igesio::models`に属し、エンティティが`Assembly`を指すと`models → entities`という一方向依存が逆流して幾何層がコンテナ層へ結合してしまうためである。専属であっても、ルートまでの経路探索は`Assembly`ノード間の親リンク（models層内で完結）で足り、back-pointerは不要である。

## 論理ツリーに入れないもの

物理従属の子（複合曲線の構成要素、Trimmed Surface 144→面、B-Rep 186→…→502、寸法の従属要素など）は、`Assembly`の直接メンバにしない。親エンティティの`GetChildIDs`や共有所有を経由して辿る。論理ツリー（子`Assembly`）は、独立（`kIndependent`）エンティティと、明示的にグループ化・インスタンス化された束ねのみを表す。

共有される定義エンティティ（変換行列124、色定義314、プロパティ406）は、ルート直下に置き、ツリーのどこからでも参照させる。

## 子要素のクエリ

所有エンティティの列挙・フィルタ・述語検索は、いずれも`recursive`引数（既定`false`）で「直接の子のみ」か「全子孫」かを切り替える。

| メンバ関数 | 内容 |
| --- | --- |
| `GetEntityIDs(recursive)` | 所有エンティティのIDリスト |
| `FindEntitiesByType(type, recursive)` | 指定`EntityType`のエンティティ |
| `FindEntitiesByUseFlag(flag, recursive)` | 指定`EntityUseFlag`のエンティティ |
| `FindEntities(predicate, recursive)` | 述語に合致するエンティティ |
| `GetEntity(id)` | 直接所有するエンティティ（IDが無ければ`nullptr`） |
| `GetEntities()` | 直接所有するエンティティのマップ |

参照解決・検証も`Assembly`が担う（`AreAllReferencesSet`・`GetUnresolvedReferences`・`IsReady`・`Validate`）。これらは当該ノードのみを対象とする（非再帰）。各`Assembly`が自己完結的に検証可能であることを不変条件とする。

## 構造編集

構造編集APIは、削除時に他から参照されているエンティティの扱いを`RemovalPolicy`で指定する。

| `RemovalPolicy` | 意味 |
| --- | --- |
| `kReject` | 参照が残るなら削除を拒否する（自己完結を保つ既定） |
| `kCascade` | 参照元・物理従属子も連鎖削除する |
| `kOrphan` | 参照を未解決のまま削除する（被参照の`weak_ptr`は自然失効） |

参照元の探索には`FindReferrers(id)`を用いる（idを参照する全エンティティのIDを集める逆方向検索）。

主な編集メンバを次に示す。

| メンバ関数 | 内容 |
| --- | --- |
| `RemoveEntity(id, policy)` | エンティティを削除する（このサブツリー内が対象） |
| `RemoveChildAssembly(child_id, policy)` | 直接の子`Assembly`を削除する |
| `Clear()` | このノードの全エンティティと全子`Assembly`を除去する（一斉リセット） |
| `MoveEntityTo(id, dest)` | エンティティを別ノードへ移動する（同一ルートであること） |
| `MoveChildAssemblyTo(child_id, dest)` | 直接の子`Assembly`を別ノードへ移動する |
| `AddChildAssembly(child)` | 子`Assembly`を追加する |
| `SetVisible(visible)`等の表示状態setter | 自ノードの表示状態（`DisplayState`）を設定する |
| `SetVisibleRecursive(visible)` | 自ノードと全子孫の可視性を一括設定する |
| `SetSuppressedRecursive(suppressed)` | 自ノードと全子孫の抑制状態を一括設定する |
| `SetColorOverrideRecursive(color)` | 自ノードと全子孫の色オーバーライドを一括設定する |
| `SetOpacityOverrideRecursive(opacity)` | 自ノードと全子孫の不透明度オーバーライドを一括設定する |
| `ComposeGlobalTransform(transform)` | このノードの大域変換へ追加変換を合成する（非再帰） |
| `Revision()` | ルートに集約されたモデルリビジョンを取得する |
| `ValidateSelfContainedRecursive()` | 各ノードが参照を自己完結的に解決できるかを再帰検証する |

`ComposeGlobalTransform`は`global_transform_ = transform * global_transform_`（親フレームでの後付け）とし、再帰しない。子孫へは描画時の累積変換で自然に波及するためである。変換はスケールなし（124相当）を前提とする。

`ValidateSelfContainedRecursive`は`GetUnresolvedReferences`の再帰版であり、各編集の事後条件（自己完結の不変条件）として用いる。

ロックの強制は`IsEffectivelySelectable(id)`・`IsEffectivelyEditable(id)`が担う。これらは所有ノードからルートまでの`lock.selectable`・`lock.editable`を祖先方向へANDで畳む。ツリーに属さないIDは制限なし（`true`）とみなす。

なお、構造・変換・表示状態の変更はモデルリビジョン（`Revision()`）へ集約され、描画層が次回描画時に自動検知する。`Assembly`はレンダラを知らず、編集側からレンダラへの通知も不要である（[Sceneとの関係](#sceneとの関係)を参照）。

## 空間検索

`GetWorldBoundingBox()`は、子孫の幾何メンバを包含する軸平行なバウンディングボックスを、ワールド空間（このノードを含むルートまでの全大域変換を適用）で返す。幾何かつ物理従属でないメンバのみを対象とし、退化したバウンディングボックス（点・直線状）は既存ガードに倣って除外する。幾何メンバが無い、または全体が退化してAABBを構成できない場合は`std::nullopt`を返す。

## Sceneとの関係

`Assembly`はモデルの構造（ツリー・所有・配置・メタ情報）を保持する。一方、セッション状態（選択・選択粒度・ピックフィルタ）は`Scene`（`include/igesio/models/scene.h`）が一元管理する。`Assembly`自身は選択状態を持たない。

- `Scene`は`IgesData`のルート`Assembly`を`shared_ptr`で共有して構築する。
- 描画層（`EntityRenderer`）は`Scene`を非所有参照（const）し、ツリーと選択を描画時にPULLする。
- Assembly粒度の選択（あるエンティティの所有グループを一括選択）は`Scene::SelectOwningAssembly` / `DeselectOwningAssembly`が`FindOwner`と`GetEntityIDs(recursive)`を用いて実現する。

選択・描画の詳細は[描画システム概要](../graphics/overview.md)を参照する。

## 入出力との関係

既定（構造編集なし）では、読み込んだ元エンティティをフラットにそのまま出力する。`Assembly`・変換ビューはファイルへ出力しない（非永続）。全エンティティが平坦マップに残るため、ラウンドトリップの忠実性が自動的に保たれる。

アセンブリの配置を永続化したい場合は、大域変換をエンティティのDE変換（124）へ畳み込む`Flatten` / `Materialize`を用いる。詳細は[座標フレームと変換ビュー](./coordinate_frames_ja.md)を参照する。
