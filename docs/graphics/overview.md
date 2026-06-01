# 描画システム概要

## 1. 全体構成

IGESioの描画システムはOpenGL 4.0以上を使用し、エンティティごとに適切なGLSLシェーダーを割り当てて描画を行う。
主要なコンポーネントは以下の通り。

```
EntityRenderer                  ← 描画全体の管理 (GPUリソースとビュー状態のみ保持)
├── scene_                      ← models::Sceneへの非所有参照 (rootと選択を一元管理)
├── shader_programs_            ← ShaderTypeごとのGLSLプログラム
├── draw_objects_               ← ID → IEntityGraphics (GPUリソースのキャッシュ)
└── draw_list_                  ← シェーダー別の描画対象 (ツリー走査で組む一時リスト)
        ↑
        | CreateEntityGraphics() (factory.h/factory.cpp)
        |
IEntityGraphics (型消去基底クラス)
└── EntityGraphics<T>           ← 具体エンティティに対応する汎用テンプレート
    │                             (子を持てば複合ノードとしても機能する)
    ├── ICurveGraphics              ← ICurve汎用 (CPU離散化)
    ├── CircularArcGraphics         ← Type 100 (テッセレーション)
    ├── EllipseGraphics             ← Type 104, 楕円のみ (GPU)
    ├── CopiousDataGraphics         ← Type 106
    ├── SegmentGraphics             ← Type 110, 線分/半直線 (GPU)
    ├── LineGraphics                ← Type 110, 直線 (ジオメトリシェーダー)
    ├── RationalBSplineCurveGraphics← Type 126 (テッセレーション+SSBO)
    ├── CurveOnAParametricSurfaceGraphics ← Type 142
    ├── PointGraphics               ← Type 116
    ├── CompositeCurveGraphics      ← Type 102 (child_graphics_に子曲線を保持)
    └── ISurfaceGraphics            ← ISurface汎用 (CPU離散化)
        └── RationalBSplineSurfaceGraphics ← Type 128 (テッセレーション+SSBO)
```

ShaderTypeはコンパイル時のテンプレート引数ではなく、実行時のメンバ(`shader_type_`)として保持する。
複合エンティティ(`CompositeCurve`等)は専用基底ではなく、統合された`EntityGraphics`が
`child_graphics_`に子のGraphicsを持つことで表現する。選択・色などのドメイン状態は描画対象
オブジェクトに保持させず、描画時に`models`層からPULLする(§8)。

## 2. ファイル構成

```
include/igesio/graphics/
├── core/
│   ├── i_entity_graphics.h       ← IEntityGraphics: 型消去基底、ShaderType列挙体
│   ├── entity_graphics.h         ← EntityGraphics<T>: テンプレート基底 (複合も担う)
│   ├── draw_context.h            ← DrawContext: 描画時にPULLする選択等の参照束
│   ├── i_open_gl.h               ← OpenGLラッパーインターフェース
│   ├── open_gl.h                 ← OpenGLラッパー実装
│   ├── camera.h                  ← カメラ (透視/正投影/斜投影)
│   ├── light.h                   ← 光源 (平行光源/点光源)
│   ├── material_property.h       ← マテリアルプロパティ
│   └── texture.h                 ← テクスチャ (スクリーンショット含む)
├── curves/
│   ├── i_curve_graphics.h        ← ICurveGraphics: ICurve汎用
│   ├── circular_arc_graphics.h   ← Type 100
│   ├── conic_arc_graphics.h      ← Type 104 (楕円のみ特殊化)
│   ├── composite_curve_graphics.h← Type 102
│   ├── copious_data_graphics.h   ← Type 106
│   ├── line_graphics.h           ← Type 110
│   ├── point_graphics.h          ← Type 116
│   ├── rational_b_spline_curve_graphics.h ← Type 126
│   └── curve_on_a_parametric_surface_graphics.h ← Type 142
├── surfaces/
│   ├── i_surface_graphics.h      ← ISurfaceGraphics: ISurface汎用
│   └── rational_b_spline_surface_graphics.h ← Type 128
├── factory.h                     ← CreateEntityGraphics() 等
└── renderer.h                    ← EntityRenderer

src/graphics/
├── shaders.h                     ← シェーダーコード読み込み・インクルード展開
├── shaders/
│   ├── shader_code.h             ← ShaderCode構造体
│   ├── curves.h                  ← 各曲線シェーダーのパス/ソース定義
│   ├── surfaces.h                ← 各曲面シェーダーのパス/ソース定義
│   └── glsl/
│       ├── common/               ← 共通GLSLコード (NURBS補間ロジック等)
│       ├── curves/               ← 曲線用GLSLファイル
│       └── surfaces/             ← 曲面用GLSLファイル
├── core/                         ← IEntityGraphics他のコア実装
├── curves/                       ← 各曲線GraphicsクラスのCPP
├── surfaces/                     ← 各曲面GraphicsクラスのCPP
├── factory.cpp                   ← ファクトリ実装
└── renderer.cpp                  ← EntityRenderer実装
```

## 3. ShaderType 列挙体

`ShaderType` (定義: `include/igesio/graphics/core/i_entity_graphics.h`) は、どのGLSLシェーダープログラムを使用するかを決定する。

| ShaderType | 対象エンティティ | 備考 |
|---|---|---|
| `kGeneralCurve` | ICurveを継承する全クラス (汎用) | CPU上で曲線を離散化してVBO転送 |
| `kCircularArc` | Type 100: CircularArc | テッセレーションシェーダー使用 |
| `kEllipse` | Type 104, 楕円のみ | GPU上でパラメトリック描画 |
| `kCopiousData` | Type 106: CopiousData | 汎用曲線シェーダーと共用 |
| `kSegment` | Type 110, Form 0-1 (線分/半直線) | 汎用曲線シェーダーと共用 |
| `kLine` | Type 110, Form 2 (直線) | ジオメトリシェーダーで無限延長 |
| `kPoint` | Type 116: Point | 点描画専用 |
| `kRationalBSplineCurve` | Type 126: NURBS曲線 | テッセレーション + SSBO |
| `kGeneralSurface` | ISurfaceを継承する全クラス (汎用) | CPU上でメッシュを生成して転送、光源計算あり |
| `kRationalBSplineSurface` | Type 128: NURBS曲面 | テッセレーション + SSBO、光源計算あり |
| `kComposite` | 複数子要素を持つエンティティ | 統合`EntityGraphics`が`child_graphics_`で子を保持; 描画は子へ委譲 |
| `kNone` | (番兵値) | ループ上限として使用 |

`kGeneralSurface` 以降 (`kComposite`, `kNone` 除く) のシェーダーは光源計算 (`UsesLighting() == true`) を行う。

## 4. 描画フロー

`EntityRenderer::Draw()`は、`SetScene()`で設定された`models::Scene`のAssemblyツリーを走査して
描画する。状態(変換・可視性・色・選択)はツリーとセッションから描画時にPULLし、描画対象オブジェクト
には保持させない。

```
EntityRenderer::Draw()
  ├─ scene_ が未設定なら何もしない
  ├─ DrawContext を構築 (scene_->ActiveSelection() を参照)
  ├─ scene_dirty_ なら RebuildDrawList() でツリーを走査し draw_list_ を再構築
  │   └─ WalkAssembly(node, 累積変換, 色/不透明度の最近接オーバーライド)
  │       ├─ 非表示/抑制サブツリーはスキップ
  │       ├─ 各エンティティの world_transform_・色をリフレッシュ (PULLした値を反映)
  │       └─ GetShaderTypes() でシェーダー別に draw_list_ へ収集 (複合は子の各型へ)
  └─ ExecuteDrawList(ctx)
      └─ (シェーダーごとにループ)
          ├─ glUseProgram + view/projection/light の共通uniformを設定
          └─ draw_list_[shader_type] の各 IEntityGraphics::Draw(program, type, viewport, ctx)
              ├─ ApplyRenderState(shader, ctx) ← model/mainColor/材質をPULLして設定
              │     (選択中は ctx.highlight_color、非選択は GetColor())
              └─ DrawImpl() ← サブクラスが実装; VAOをbindして描画
                            (複合ノードは型一致の子へ委譲)
```

カメラ操作や選択変更だけの再描画では`scene_dirty_`が立たないため、ツリー走査を行わず`draw_list_`を
再利用する(`ExecuteDrawList`は毎フレーム`DrawContext`から選択を読み直すため、ハイライトは再走査
なしに更新される)。ツリー編集・エンティティ追加削除時は`MarkSceneDirty()`または内部処理でdirty化し、
次回描画で再走査する。

## 5. エンティティ追加時の登録経路

```
EntityRenderer::AddEntity(entity)
  └─ CreateEntityGraphics(entity, gl)  ← factory.cpp
      ├─ dynamic_cast で ICurve か ISurface か Point かを判定
      ├─ ICurveの場合 → CreateCurveGraphics()
      │   └─ Type ごとに dynamic_cast して具体的なXxxGraphics を生成
      │      どれにも当てはまらない場合は ICurveGraphics (汎用) を生成
      ├─ ISurfaceの場合 → CreateSurfaceGraphics()
      │   └─ Type ごとに dynamic_cast して具体的なXxxGraphics を生成
      │      どれにも当てはまらない場合は ISurfaceGraphics (汎用) を生成
      └─ Point の場合 → PointGraphics を生成
```

新しいエンティティの描画オブジェクトが生成された後、`EntityRenderer`は`AddGraphicsObject()`を通じて
`draw_objects_`(IDキーのGPUリソースキャッシュ)に登録する。実際の描画対象は、描画時に`Scene`の
ツリーを走査してこのキャッシュをIDで引くことで決まる(§4)。

## 6. シェーダーコードの管理

GLSLコードの指定方法は2通りある。

1. **ファイルパス参照方式**: `src/graphics/shaders/glsl/` 以下のGLSLファイルのパスを文字列定数として `curves.h` / `surfaces.h` に登録する。
   例: `"glsl/curves/100_circular_arc.vert"`

2. **インライン文字列方式**: C++生文字列リテラル (`R"(...)"`) としてシェーダーコードを `curves.h` / `surfaces.h` に直接記述する。
   例: `kLineShader` の頂点シェーダー・ジオメトリシェーダー

GLSLファイルは `#include "glsl/common/xxx.glsl"` の形で共通コードをインクルードできる。
インクルード展開は `shaders.h` の `ExpandShaderIncludes()` が実行時に自動処理する。

`renderer.cpp` の `InitShaders()` 内で `GetShaderCode(shader_type)` を呼び出し、
各 ShaderType に対応するシェーダープログラムをコンパイル・リンクする。

## 7. CPU離散化方式とGPUパラメトリック方式

描画には2種類のアプローチがある。

### CPU離散化方式 (`ICurveGraphics`, `ISurfaceGraphics`)

- CPU上でエンティティのパラメータ範囲を均等に分割して点列を生成する。
- VBO/VAOにデータを転送し、`GL_LINE_STRIP` (曲線) または三角形メッシュ (曲面) として描画する。
- `Synchronize()` でVBOを再構築するため、エンティティのパラメータが変化した場合は `Synchronize()` を呼ぶ必要がある。
- `world_transform_` にはエンティティ自身のDEレコード変換行列を含まない
  (頂点はすでにグローバル座標変換済みでVBOに格納されるため)。

### GPUパラメトリック方式 (`CircularArcGraphics`, `RationalBSplineCurveGraphics` 等)

- エンティティのパラメータ (中心座標・半径・角度範囲、制御点・ノットベクトル等) をuniform変数またはSSBOでGPUに渡す。
- GPUのテッセレーションシェーダーで曲線・曲面を生成する。
- `Synchronize()` では空のVAOのみ生成し、頂点データは渡さない。
- `use_entity_transform_ = true` を指定して `world_transform_` にエンティティの変換行列を含める。

## 8. 状態の分離 (Scene / SelectionSet / DrawContext)

描画対象オブジェクトはドメイン状態(選択・色オーバーライド等)を保持せず、描画時に`models`層からPULLする。
状態の置き場所は概ね次のように分かれる。

- `models::Scene`(非GUI側の一元管理元): モデルのルートAssembly、選択セット群とアクティブセット、ピックフィルタを保持する。`EntityRenderer`は`SetScene()`でこれを非所有参照し、描画・ピックで参照する。
- `models::SelectionSet`: 選択中のID集合とアンカー(主選択)を保持する純粋クラス。GL非依存でヘッドレスでも操作できる。
- `models::AssemblyMetadata`: 各ノードの可視/抑制/ロック/色・不透明度オーバーライドを保持する。走査時に祖先方向へ合成する(可視=AND、抑制=OR、色/不透明度=最近接優先、選択ロック=AND)。
- `graphics::DrawContext`: 描画時に降ろす参照の束(選択セットとハイライト色)。各描画オブジェクトはこれを通じて自分が選択中かを判定する。選択色はオブジェクトへ焼き込まない。

選択や色は非GUI層が保持・操作でき、外部がツリーや選択を書き換えても次フレームの描画へ自動反映される。
選択ハイライトの優先順位は「選択 > 最近接オーバーライド > エンティティ固有色」である。ロック(`lock.selectable=false`)されたサブツリーの要素は、対話選択経路(`Scene::TrySelectWithLock`)で拒否される(`SelectionSet::Select`自体は純粋なまま)。

