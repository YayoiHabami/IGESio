# 描画システム クラスリファレンス

## コアクラス

### `IEntityGraphics` (`include/igesio/graphics/core/i_entity_graphics.h`)

全描画クラスの型消去基底クラス。具体型は`EntityGraphics<T>`(複合ノードもこれが担う)が実装する。

**主要な純粋仮想関数**

| 関数 | 説明 |
|---|---|
| `Draw(shader, shader_type, viewport, ctx)` | 指定したShaderTypeに一致する場合に描画する(選択等はctxからPULL) |
| `Draw(shader, viewport, ctx)` | ShaderTypeを問わず描画する |
| `Synchronize()` | エンティティの状態に基づいてOpenGLリソースを再構築する |
| `SyncTexture()` | テクスチャリソースを同期する |
| `GetShaderTypes()` | このオブジェクト(と子)が使用するShaderTypeの集合を返す |
| `Cleanup()` | OpenGLリソースを解放する |
| `IsDrawable()` | 描画可能な状態かを返す |

**設定・取得関数**

| 関数 | 説明 |
|---|---|
| `SetWorldTransform(matrix)` | ワールド変換行列を設定する |
| `GetWorldTransform()` | ワールド変換行列を取得する |
| `SetColor(color)` | 描画色をオーバーライドする (IGES側の色情報は変更しない) |
| `GetColor()` | 現在の描画色を返す |
| `ResetColor()` | 色をエンティティのIGES色に戻す |
| `GetLineWidth()` | 線幅を返す |
| `SetGlobalParam(param)` | 描画グローバルパラメータを設定する |
| `MaterialProperty()` | マテリアルプロパティへの参照を返す |

### `EntityGraphics<T, has_surfaces>` (`include/igesio/graphics/core/entity_graphics.h`)

`IEntityGraphics`の汎用テンプレート実装。具体的なGraphicsクラスはこれを継承する。子のGraphicsを
持てば複合ノードも担う。使用する`ShaderType`はテンプレート引数ではなく、コンストラクタ引数として
渡してメンバ`shader_type_`に保持する。

**テンプレートパラメータ**

| パラメータ | 説明 |
|---|---|
| `T` | 対象エンティティ型。`IEntityIdentifier`を継承していること |
| `has_surfaces` | サーフェスを持つ場合`true`(デフォルト`false`) |

**サブクラスで実装が必要なもの**

| 対象 | 説明 |
|---|---|
| `DrawImpl(shader, viewport)` | 常にオーバーライドが必要。OpenGL描画コマンドを実行する |
| `Synchronize()` | 常にオーバーライドが必要。VAO/VBOを構築する |
| `Cleanup()` | VAO以外のOpenGLリソース (VBO, SSBO等) がある場合にオーバーライド |
| `IsDrawable()` | `vao_ != 0` 以外の条件が必要な場合にオーバーライド |

**サブクラスからアクセスできる protected メンバ**

| メンバ | 説明 |
|---|---|
| `entity_` | エンティティへの `shared_ptr<const T>` |
| `gl_` | OpenGLラッパー (`IOpenGL`) への `shared_ptr` |
| `vao_` | VAO ID (基底クラスの `Cleanup()` で自動解放) |
| `draw_mode_` | 描画モード (`GL_LINE_STRIP`, `GL_LINE_LOOP` 等) |
| `texture_id_` | テクスチャID (`has_surfaces = true` の場合に使用) |
| `world_transform_` | ワールド変換行列 |
| `use_entity_transform_` | `GetWorldTransform()` でエンティティ変換行列を合成するか |
| `global_param_` | 描画グローバルパラメータ |
| `material_property_` | マテリアルプロパティ |
| `shader_type_` | このオブジェクトの主ShaderType (コンストラクタで指定) |
| `child_graphics_` | 子GraphicsをShaderType別に保持する (複合ノード時) |

### 複合ノード (子要素を持つ `EntityGraphics`)

複数の子要素を独立して描画するエンティティ(`CompositeCurve`(Type 102)等)は、専用基底ではなく
統合`EntityGraphics`が子のGraphicsを`child_graphics_`(ShaderType別)で保持して表現する。主ShaderTypeは
`kComposite`で、自身は固有のシェーダーコードを持たず、描画を子へ委譲する。

| 関数 | 説明 |
|---|---|
| `AddChildGraphics(graphics)` | 子要素のGraphicsオブジェクトを追加する |
| `GetShaderTypes()` | 自身と全子要素のShaderTypeの集合を返す |

`SetWorldTransform()` / `SetColor()` / `ResetColor()` は子要素にも伝播する。

### `EntityRenderer` (`include/igesio/graphics/renderer.h`)

描画全体を管理するクラス。

**主要な公開関数**

| 関数 | 説明 |
|---|---|
| `Initialize()` | シェーダーのコンパイル・リンクを行う |
| `AddEntity(entity, global_param, material_property)` | エンティティを描画対象に追加する |
| `RemoveEntity(id)` | エンティティを描画対象から削除する |
| `HasEntity(id)` | エンティティが登録済みかを返す |
| `Draw()` | 全エンティティを描画する |
| `CaptureScreenshot()` | 現在の描画状態をキャプチャして `Texture` を返す |
| `Camera()` | カメラへの参照を返す |
| `Light()` | 光源への参照を返す |
| `SetDisplaySize(w, h)` | 描画対象サイズを設定する |
| `SetBackgroundColor(r, g, b, a)` | 背景色を設定する |
| `SetScene(scene)` | 描画対象の`models::Scene`を設定する (rootと選択を一元管理) |
| `MarkSceneDirty()` | ツリー変更を通知し、次回描画で再走査させる |
| `PickEntities(...)` / `PickEntitiesInRect(...)` | レイ/矩形でヒットしたエンティティIDを返す |

選択状態はレンダラではなく`models::Scene`/`SelectionSet`が保持する。レンダラはピッキングでIDを返すのみで、選択の確定は`Scene`(対話では`TrySelectWithLock`)が行う。

## 状態関連クラス (PULL元)

描画はこれらから状態をPULLする。詳細は`overview.md`§8を参照。

### `models::Scene` (`include/igesio/models/scene.h`)

| 関数 | 説明 |
|---|---|
| `Root()` / `RootPtr()` | モデルのルートAssemblyを取得する |
| `ActiveSelection()` | アクティブな選択セットを取得する |
| `CreateSelectionSet()` / `ActivateSelectionSet(id)` | 選択セットの作成・切替 |
| `Filter()` | ピックフィルタ(`PickFilter`)を取得する |
| `TrySelectWithLock(set, id)` | ロック/フィルタを尊重して選択する(対話ピック用) |

### `models::SelectionSet` (`include/igesio/models/selection_set.h`)

| 関数 | 説明 |
|---|---|
| `Select` / `Deselect` / `Toggle` / `Replace` / `Clear` | 選択集合の操作 |
| `Contains(id)` / `Items()` | 選択状態の照会 |
| `Active()` | アンカー(主選択)を取得する |

### `models::PickFilter` (`include/igesio/models/pick_filter.h`)

`faces` / `edges` / `vertices` / `bodies` の各ブール値。v1は`bodies`(エンティティ単位)のみ尊重する。

### `graphics::DrawContext` (`include/igesio/graphics/core/draw_context.h`)

| メンバ | 説明 |
|---|---|
| `selection` | 参照する`SelectionSet`(非所有) |
| `highlight_color` | 選択ハイライト色 (RGBA) |
| `IsHighlighted(id)` | 指定IDが選択中(ハイライト対象)かを返す |

## ファクトリ関数 (`include/igesio/graphics/factory.h`)

| 関数 | 説明 |
|---|---|
| `CreateEntityGraphics(entity, gl)` | 任意の `IEntityIdentifier` から適切なGraphicsオブジェクトを作成する |
| `CreateCurveGraphics(entity, gl)` | `ICurve` から曲線Graphicsオブジェクトを作成する |
| `CreateSurfaceGraphics(entity, gl)` | `ISurface` から曲面Graphicsオブジェクトを作成する |

## 各エンティティのGraphicsクラス一覧

### 曲線系

| クラス | 対象 | ShaderType | 方式 |
|---|---|---|---|
| `ICurveGraphics` | ICurve 汎用 | `kGeneralCurve` | CPU離散化, VBO |
| `CircularArcGraphics` | Type 100 | `kCircularArc` | GPUパラメトリック, テッセレーション |
| `CompositeCurveGraphics` | Type 102 | `kComposite` | 子要素に委譲 |
| `EllipseGraphics` | Type 104 (楕円のみ) | `kEllipse` | GPUパラメトリック |
| `CopiousDataGraphics` | Type 106 | `kCopiousData` | CPU離散化 (汎用シェーダー流用) |
| `SegmentGraphics` | Type 110 Form 0-1 | `kSegment` | CPU離散化 (汎用シェーダー流用) |
| `LineGraphics` | Type 110 Form 2 | `kLine` | ジオメトリシェーダーで無限延長 |
| `PointGraphics` | Type 116 | `kPoint` | 点描画 |
| `RationalBSplineCurveGraphics` | Type 126 | `kRationalBSplineCurve` | GPUパラメトリック, テッセレーション + SSBO |
| `CurveOnAParametricSurfaceGraphics` | Type 142 | (独自 or 汎用) | 対応するシェーダーによる |

### 曲面系

| クラス | 対象 | ShaderType | 方式 |
|---|---|---|---|
| `ISurfaceGraphics` | ISurface 汎用 | `kGeneralSurface` | CPU離散化, VBO + EBO, 光源計算あり |
| `RationalBSplineSurfaceGraphics` | Type 128 | `kRationalBSplineSurface` | GPUパラメトリック, テッセレーション + SSBO, 光源計算あり |

## MaterialProperty (`include/igesio/graphics/core/material_property.h`)

IGESが保持しない、描画専用のマテリアルプロパティ。

| メンバ | 型 | 説明 |
|---|---|---|
| `ambient_strength` | `float` | 環境光強度 (デフォルト: 0.1) |
| `specular_strength` | `float` | 鏡面反射強度 (デフォルト: 0.5) |
| `shininess` | `int` | 輝度 (デフォルト: 32) |
| `opacity` | `float` | 不透明度 (デフォルト: 1.0) |
| `texture` | `Texture` | テクスチャ |

## Camera (`include/igesio/graphics/core/camera.h`)

| 投影モード | 説明 |
|---|---|
| `Perspective` | 透視投影 (デフォルト) |
| `Orthographic` | 正投影 |
| `Oblique` | 斜投影 |

### クリッピング面

- 近接/遠方クリッピング面(near/far)は、通常は自動クリッピングにより決定される。`SetAutoClipSphere`で登録されたシーンの外接球と現在のカメラ位置からnear/farが毎回導出されるため、ズームやパンでカメラが移動してもシーンがクリップされない。
- 外接球はレンダラがシーンの設定・変更時(`SetScene`/`MarkSceneDirty`)に自動で更新するほか、`Camera::FitToBoundingBox`(FitView)でも登録される。
- `SetClippingPlanes`を呼ぶと自動クリッピングは解除され、設定した手動値に固定される。シーン未設定時のフォールバック値は`kDefaultNearPlane`/`kDefaultFarPlane`である。

## Light (`include/igesio/graphics/core/light.h`)

| 光源タイプ | 説明 |
|---|---|
| `Directional` | 平行光源 (方向のみ、位置なし) |
| `Point` | 点光源 (位置と減衰係数あり) |
