# 描画システム クラスリファレンス

## コアクラス

### `IEntityGraphics` (`include/igesio/graphics/core/i_entity_graphics.h`)

全描画クラスの型消去基底クラス。`EntityGraphics<T>` および `CompositeEntityGraphics<T>` が実装する。

**主要な純粋仮想関数**

| 関数 | 説明 |
|---|---|
| `Draw(shader, shader_type, viewport)` | 指定したShaderTypeが一致する場合に描画する |
| `Draw(shader, viewport)` | ShaderTypeを問わず描画する |
| `Synchronize()` | エンティティの状態に基づいてOpenGLリソースを再構築する |
| `SyncTexture()` | テクスチャリソースを同期する |
| `GetShaderType()` | このオブジェクトが使用するShaderTypeを返す |
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

---

### `EntityGraphics<T, ShaderType_, has_surfaces>` (`include/igesio/graphics/core/entity_graphics.h`)

`IEntityGraphics` の汎用テンプレート実装。具体的なGraphicsクラスはこれを継承する。

**テンプレートパラメータ**

| パラメータ | 説明 |
|---|---|
| `T` | 対象エンティティ型。`IEntityIdentifier` を継承していること |
| `ShaderType_` | このクラスが使用する `ShaderType` 値 |
| `has_surfaces` | サーフェスを持つ場合 `true` (デフォルト `false`) |

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

---

### `CompositeEntityGraphics<T>` (`include/igesio/graphics/core/composite_entity_graphics.h`)

複数の子要素を持つエンティティ用の基底クラス。ShaderTypeは常に `kComposite`。
子要素のGraphicsオブジェクトを `child_graphics_` マップで管理する。

**主要な公開関数**

| 関数 | 説明 |
|---|---|
| `AddChildGraphics(graphics)` | 子要素のGraphicsオブジェクトを追加する |
| `GetShaderTypes()` | 子要素を含む全ShaderTypeを返す |

`SetWorldTransform()` は子要素にも変換行列を伝播させる。
`SetColor()` / `ResetColor()` も子要素に伝播する。

---

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

---

## ファクトリ関数 (`include/igesio/graphics/factory.h`)

| 関数 | 説明 |
|---|---|
| `CreateEntityGraphics(entity, gl)` | 任意の `IEntityIdentifier` から適切なGraphicsオブジェクトを作成する |
| `CreateCurveGraphics(entity, gl)` | `ICurve` から曲線Graphicsオブジェクトを作成する |
| `CreateSurfaceGraphics(entity, gl)` | `ISurface` から曲面Graphicsオブジェクトを作成する |

---

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

---

## MaterialProperty (`include/igesio/graphics/core/material_property.h`)

IGESが保持しない、描画専用のマテリアルプロパティ。

| メンバ | 型 | 説明 |
|---|---|---|
| `ambient_strength` | `float` | 環境光強度 (デフォルト: 0.1) |
| `specular_strength` | `float` | 鏡面反射強度 (デフォルト: 0.5) |
| `shininess` | `int` | 輝度 (デフォルト: 32) |
| `opacity` | `float` | 不透明度 (デフォルト: 1.0) |
| `texture` | `Texture` | テクスチャ |

---

## Camera (`include/igesio/graphics/core/camera.h`)

| 投影モード | 説明 |
|---|---|
| `Perspective` | 透視投影 (デフォルト) |
| `Orthographic` | 正投影 |
| `Oblique` | 斜投影 |

---

## Light (`include/igesio/graphics/core/light.h`)

| 光源タイプ | 説明 |
|---|---|
| `Directional` | 平行光源 (方向のみ、位置なし) |
| `Point` | 点光源 (位置と減衰係数あり) |
