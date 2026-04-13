# 新しいエンティティに描画コードを追加する手順

新たなエンティティクラスに描画対応を追加する際の手順をまとめる。描画方式には「CPU離散化」と「GPUパラメトリック」の2種類があり、エンティティの特性に応じてどちらを選ぶかを判断する。

## 目次

- [目次](#目次)
- [描画方式の選択](#描画方式の選択)
- [パターン A: 汎用クラスをそのまま使う (追加作業なし)](#パターン-a-汎用クラスをそのまま使う-追加作業なし)
- [パターン B: 専用のGraphicsクラスを新規作成する](#パターン-b-専用のgraphicsクラスを新規作成する)
  - [ステップ 1: ShaderTypeを追加する](#ステップ-1-shadertypeを追加する)
  - [ステップ 2: Graphicsクラスのヘッダーを作成する](#ステップ-2-graphicsクラスのヘッダーを作成する)
  - [ステップ 3: Graphicsクラスのソースファイルを作成する](#ステップ-3-graphicsクラスのソースファイルを作成する)
    - [CPU離散化方式の場合](#cpu離散化方式の場合)
    - [GPUパラメトリック方式の場合](#gpuパラメトリック方式の場合)
  - [ステップ 4: GLSLシェーダーを用意する](#ステップ-4-glslシェーダーを用意する)
  - [ステップ 5: シェーダーコード定義を登録する](#ステップ-5-シェーダーコード定義を登録する)
  - [ステップ 6: ファクトリ関数に登録する](#ステップ-6-ファクトリ関数に登録する)
  - [ステップ 7: CMakeLists.txt にソースファイルを追加する](#ステップ-7-cmakeliststxt-にソースファイルを追加する)
- [パターン C: CompositeEntityGraphics を使う](#パターン-c-compositeentitygraphics-を使う)
- [変換行列 (world\_transform\_) の扱いについて](#変換行列-world_transform_-の扱いについて)
  - [`use_entity_transform_ = false` の場合 (CPU離散化)](#use_entity_transform_--false-の場合-cpu離散化)
  - [`use_entity_transform_ = true` の場合 (GPUパラメトリック)](#use_entity_transform_--true-の場合-gpuパラメトリック)
- [まとめ: 最小作業チェックリスト](#まとめ-最小作業チェックリスト)
  - [汎用クラス (ICurveGraphics / ISurfaceGraphics) で十分な場合](#汎用クラス-icurvegraphics--isurfacegraphics-で十分な場合)
  - [専用Graphicsクラスを作成する場合](#専用graphicsクラスを作成する場合)


## 描画方式の選択

| 方式 | いつ使うか |
|---|---|
| **汎用クラス流用** (対応不要) | `ICurve`を継承しており`GetParameterRange()`と `TryGetDerivatives()`が正しく実装されていれば、追加作業なしで`ICurveGraphics` (汎用CPU離散化) が自動で使用される。同様に`ISurface`に対しては`ISurfaceGraphics`が使われる。まず汎用描画で確認してから専用実装に移行するのが推奨の進め方。 |
| **CPU離散化 (専用)** | 汎用と同じVBO方式を使いたいが、特殊な描画モード (`GL_POINTS`, `GL_LINES` 等) や前処理が必要な場合。`ICurveGraphics` / `ISurfaceGraphics`の継承も可能。 |
| **GPUパラメトリック** | 形状が解析式で表現でき、テッセレーションでGPU上の精密な描画が望ましい場合 (円弧、NURBS等)。 |
| **CompositeEntityGraphics** | 子エンティティの集合体であり、各子が独自のShaderTypeを持つ場合 (CompositeCurve等)。 |

## パターン A: 汎用クラスをそのまま使う (追加作業なし)

新しいエンティティが`ICurve`または`ISurface`を継承している場合は、`factory.cpp`の末尾フォールバックにより自動的に`ICurveGraphics` / `ISurfaceGraphics`が生成される。**描画コードの追加は不要。**

```cpp
// factory.cppの末尾 (既存コード)
// いずれにも当てはまらない場合はICurveGraphicsを作成
return std::make_unique<i_graph::ICurveGraphics>(entity, gl);
```

## パターン B: 専用のGraphicsクラスを新規作成する

### ステップ 1: ShaderTypeを追加する

`include/igesio/graphics/core/i_entity_graphics.h`の`ShaderType`列挙体に新しい値を追加する。

```cpp
// i_entity_graphics.h (抜粋)
enum class ShaderType {
    // ... 既存の値 ...
    kRationalBSplineCurve,

    // ↓ 新たに追加 (kGeneralSurface より前に曲線を挿入)
    kMyNewCurve,   // Type NNN: MyNewEntity 用

    kGeneralSurface,
    // ...
    kNone,  // 番兵値; 末尾に置くこと
};
```

> **注意**: 光源計算が必要な曲面シェーダーは`kGeneralSurface`以降に配置すること。`UsesLighting()`の判定は`kGeneralSurface` 以上かどうかで行われる。

### ステップ 2: Graphicsクラスのヘッダーを作成する

`include/igesio/graphics/curves/` (曲線) または`include/igesio/graphics/surfaces/` (曲面) に新しいヘッダーを作成する。

```cpp
// include/igesio/graphics/curves/my_new_entity_graphics.h

#ifndef IGESIO_GRAPHICS_CURVES_MY_NEW_ENTITY_GRAPHICS_H_
#define IGESIO_GRAPHICS_CURVES_MY_NEW_ENTITY_GRAPHICS_H_

#include <memory>
#include <utility>

#include "igesio/entities/curves/my_new_entity.h"  // 対象エンティティのヘッダー
#include "igesio/graphics/core/entity_graphics.h"

namespace igesio::graphics {

/// @brief MyNewEntityエンティティの描画情報の管理クラス
class MyNewEntityGraphics
    : public EntityGraphics<entities::MyNewEntity, ShaderType::kMyNewCurve> {
 public:
    /// @brief コンストラクタ
    explicit MyNewEntityGraphics(
            const std::shared_ptr<const entities::MyNewEntity>,
            const std::shared_ptr<IOpenGL>);

    /// @brief デストラクタ
    ~MyNewEntityGraphics();

    // コピー禁止
    MyNewEntityGraphics(const MyNewEntityGraphics&) = delete;
    MyNewEntityGraphics& operator=(const MyNewEntityGraphics&) = delete;

    /// @brief 描画用リソースをエンティティの状態に同期する
    void Synchronize() override;

    // VAO以外のリソース (VBO, SSBO等) がある場合はCleanup()もオーバーライドする
    // void Cleanup() override;

 protected:
    /// @brief 実際の描画処理
    void DrawImpl(GLuint, const std::pair<float, float>&) const override;
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CURVES_MY_NEW_ENTITY_GRAPHICS_H_
```

**テンプレートパラメータの説明**

```
EntityGraphics<T, ShaderType_, has_surfaces>
  T             : 対象のエンティティ型 (IEntityIdentifier を継承していること)
  ShaderType_   : 上で定義した ShaderType の値
  has_surfaces  : サーフェスを持つ場合 true (デフォルト false)
                  true にすると ambientStrength / specularStrength / shininess /
                  useTexture 等のマテリアルuniform変数が自動設定される
```

### ステップ 3: Graphicsクラスのソースファイルを作成する

`src/graphics/curves/` (または `src/graphics/surfaces/`) にCPPファイルを作成する。

#### CPU離散化方式の場合

```cpp
// src/graphics/curves/my_new_entity_graphics.cpp

#include "igesio/graphics/curves/my_new_entity_graphics.h"

#include <memory>
#include <vector>

namespace {
using igesio::graphics::MyNewEntityGraphics;
}

MyNewEntityGraphics::MyNewEntityGraphics(
        const std::shared_ptr<const entities::MyNewEntity> entity,
        const std::shared_ptr<IOpenGL> gl)
        : EntityGraphics(entity, gl, false) {  // false = エンティティ変換行列を含まない
    Synchronize();
}

MyNewEntityGraphics::~MyNewEntityGraphics() {
    Cleanup();
}

void MyNewEntityGraphics::Synchronize() {
    Cleanup();  // 既存リソースを解放

    // 頂点データをCPU上で生成する
    std::vector<float> vertices;
    auto [t_min, t_max] = entity_->GetParameterRange();
    const int n = 100;  // 分割数
    for (int i = 0; i <= n; ++i) {
        double t = t_min + (t_max - t_min) * i / n;
        auto [p, ok] = entity_->TryGetDerivatives(t);
        if (!ok) continue;
        vertices.push_back(static_cast<float>(p.position[0]));
        vertices.push_back(static_cast<float>(p.position[1]));
        vertices.push_back(static_cast<float>(p.position[2]));
    }

    // VAO / VBO を生成してデータを転送
    GLuint vbo;
    gl_->GenVertexArrays(1, &vao_);
    gl_->GenBuffers(1, &vbo);

    gl_->BindVertexArray(vao_);
    gl_->BindBuffer(GL_ARRAY_BUFFER, vbo);
    gl_->BufferData(GL_ARRAY_BUFFER,
                    static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                    vertices.data(), GL_STATIC_DRAW);

    // attribute: location 0 = vec3 (x, y, z)
    gl_->EnableVertexAttribArray(0);
    gl_->VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                             3 * sizeof(float), nullptr);

    gl_->BindVertexArray(0);
    gl_->DeleteBuffers(1, &vbo);  // VAOに関連付けられたので削除してよい

    vertex_count_ = static_cast<GLsizei>(vertices.size() / 3);
    draw_mode_ = entity_->IsClosed() ? GL_LINE_LOOP : GL_LINE_STRIP;
}

void MyNewEntityGraphics::DrawImpl(
        GLuint shader, [[maybe_unused]] const std::pair<float, float>&) const {
    gl_->BindVertexArray(vao_);
    gl_->DrawArrays(draw_mode_, 0, vertex_count_);
    gl_->BindVertexArray(0);
}
```

#### GPUパラメトリック方式の場合

```cpp
// src/graphics/curves/my_new_entity_graphics.cpp (GPUパラメトリック版)

MyNewEntityGraphics::MyNewEntityGraphics(
        const std::shared_ptr<const entities::MyNewEntity> entity,
        const std::shared_ptr<IOpenGL> gl)
        : EntityGraphics(entity, gl, true) {  // true = エンティティ変換行列を含む
    Synchronize();
}

MyNewEntityGraphics::~MyNewEntityGraphics() {
    Cleanup();
}

void MyNewEntityGraphics::Synchronize() {
    Cleanup();

    // テッセレーションシェーダー使用: 空のVAOを1つ生成するだけでよい
    gl_->GenVertexArrays(1, &vao_);
    gl_->PatchParameteri(GL_PATCH_VERTICES, 1);

    draw_mode_ = entity_->IsClosed() ? GL_LINE_LOOP : GL_LINE_STRIP;
}

void MyNewEntityGraphics::DrawImpl(
        GLuint shader, const std::pair<float, float>& viewport) const {
    // エンティティのパラメータをuniform変数でGPUに渡す
    gl_->Uniform3f(gl_->GetUniformLocation(shader, "someParam"),
                   static_cast<float>(entity_->SomeParam()[0]),
                   static_cast<float>(entity_->SomeParam()[1]),
                   static_cast<float>(entity_->SomeParam()[2]));
    gl_->Uniform1f(gl_->GetUniformLocation(shader, "anotherParam"),
                   static_cast<float>(entity_->AnotherParam()));
    gl_->Uniform2f(gl_->GetUniformLocation(shader, "viewportSize"),
                   viewport.first, viewport.second);

    gl_->BindVertexArray(vao_);
    gl_->DrawArrays(GL_PATCHES, 0, 1);  // テッセレーション: 1パッチ
    gl_->BindVertexArray(0);
}
```

### ステップ 4: GLSLシェーダーを用意する

`src/graphics/shaders/glsl/curves/` (または `glsl/surfaces/`) にGLSLファイルを作成する。

使用できるシェーダーステージの組み合わせは以下の通り。

| 組み合わせ | 用途 |
|---|---|
| vertex → fragment | 標準的な曲線 (CPU離散化) |
| vertex → geometry → fragment | 無限直線のような特殊な形状 |
| vertex → tcs → tes → fragment | テッセレーションを使う曲線・曲面 |

各シェーダーに必要な**共通 uniform 変数**は`EntityGraphics<T>::Draw()`が自動的に設定する。

| uniform 変数名 | 型 | 説明 |
|---|---|---|
| `model` | `mat4` | ワールド座標変換行列 |
| `view` | `mat4` | ビュー変換行列 (EntityRendererが設定) |
| `projection` | `mat4` | 射影変換行列 (EntityRendererが設定) |
| `mainColor` | `vec4` | エンティティの色 (RGBA, [0,1]) |

曲面 (`has_surfaces = true`) の場合は追加で以下が設定される。

| uniform 変数名 | 型 | 説明 |
|---|---|---|
| `ambientStrength` | `float` | 環境光強度 |
| `specularStrength` | `float` | 鏡面反射強度 |
| `shininess` | `int` | 輝度 |
| `useTexture` | `int` | テクスチャ使用フラグ (0 or 1) |
| `textureSampler` | `sampler2D` | テクスチャサンプラー |

`view`, `projection`, `lightPos`, `viewPos`等のカメラ/光源関連のuniform変数は`EntityRenderer::Draw()`が全シェーダープログラムに一括設定する。

共通コードを再利用する場合は`glsl/common/`にファイルを置き、`#include "glsl/common/xxx.glsl"`でインクルードできる。

### ステップ 5: シェーダーコード定義を登録する

`src/graphics/shaders/curves.h` (または `surfaces.h`) にシェーダーのパスを定義し、`GetCurveShaderCode()`のswitch文に追加する。

```cpp
// src/graphics/shaders/curves.h (抜粋)

/// @brief kMyNewCurve用のシェーダー (Type NNN)
constexpr std::array<const char*, 2> kMyNewCurveShader = {
    "glsl/curves/nnn_my_new_entity.vert",
    "glsl/curves/nnn_my_new_entity.frag"
};

// または、テッセレーション使用時:
constexpr std::array<const char*, 4> kMyNewCurveShader = {
    "glsl/curves/nnn_my_new_entity.vert",
    "glsl/curves/nnn_my_new_entity.tesc",
    "glsl/curves/nnn_my_new_entity.tese",
    "glsl/curves/nnn_my_new_entity.frag"
};

// GetCurveShaderCode() に追加
std::optional<ShaderCode> GetCurveShaderCode(const ShaderType shader_type) {
    switch (shader_type) {
        // ... 既存のcase ...
        case ShaderType::kMyNewCurve:
            return ShaderCode(kMyNewCurveShader);
        default:
            return std::nullopt;
    }
}
```

### ステップ 6: ファクトリ関数に登録する

`src/graphics/factory.cpp`の`CreateCurveGraphics()` (または `CreateSurfaceGraphics()`)に`dynamic_cast`による分岐を追加する。

```cpp
// src/graphics/factory.cpp (抜粋)

#include "igesio/graphics/curves/my_new_entity_graphics.h"  // ← 追加

std::unique_ptr<i_graph::IEntityGraphics> i_graph::CreateCurveGraphics(
        const std::shared_ptr<const i_ent::ICurve> entity,
        const std::shared_ptr<i_graph::IOpenGL> gl) {
    if (!entity) return nullptr;

    // ... 既存の分岐 ...

    } else if (auto my_entity = std::dynamic_pointer_cast<
               const i_ent::MyNewEntity>(entity)) {
        // Type NNN: MyNewEntityの描画オブジェクトを作成
        return std::make_unique<i_graph::MyNewEntityGraphics>(my_entity, gl);
    }

    // フォールバック: ICurveGraphicsを返す
    return std::make_unique<i_graph::ICurveGraphics>(entity, gl);
}
```

`ICurve` / `ISurface`を継承しない独立したエンティティ (Type 116: Point等) の場合は `CreateEntityGraphics()` に追加する。

### ステップ 7: CMakeLists.txt にソースファイルを追加する

`src/graphics/curves/` (または`src/graphics/surfaces/`) に追加したCPPファイルをCMakeLists.txtに登録する。

## パターン C: CompositeEntityGraphics を使う

複数の子エンティティをそれぞれ独立して描画するエンティティの場合。

1. `CompositeEntityGraphics<T>`を継承したクラスを作成する。
2. `Synchronize()`の中で各子要素のGraphicsオブジェクトを生成し、`AddChildGraphics()`で登録する。
3. `factory.cpp`で子要素のGraphicsを追加する処理を実装する (既存の`CreateCompositeCurveGraphics()`を参照)。
4. `ShaderType::kComposite`を使用するため、ShaderTypeの追加は不要。

## 変換行列 (world_transform_) の扱いについて

`use_entity_transform_`の設定によって`world_transform_`の意味が変わる。

### `use_entity_transform_ = false` の場合 (CPU離散化)

- CPU上で離散化する際に、エンティティの変換行列も含めてワールド座標に変換した点列をVBOに格納する。
- `world_transform_`には「エンティティの親までの変換行列 (DEレコード7の変換を含まない)」を渡す。
- `ICurveGraphics` / `ISurfaceGraphics`はこの方式。

### `use_entity_transform_ = true` の場合 (GPUパラメトリック)

- パラメータ (中心座標等) はエンティティの定義空間のままGPUに渡す。
- `world_transform_`にエンティティのDEレコード7変換行列と親の変換行列が自動的に合成される (`GetWorldTransform()`が内部で処理)。
- `CircularArcGraphics`等はこの方式。

---

## まとめ: 最小作業チェックリスト

### 汎用クラス (ICurveGraphics / ISurfaceGraphics) で十分な場合

- [ ] 追加作業なし。`factory.cpp`のフォールバックが自動で適用される。

### 専用Graphicsクラスを作成する場合

- [ ] `ShaderType`列挙体に値を追加 (`i_entity_graphics.h`)
- [ ] `XxxGraphics`クラスのヘッダー作成 (`include/igesio/graphics/curves/` または `surfaces/`)
- [ ] `XxxGraphics`クラスのCPP作成 (`src/graphics/curves/`または`surfaces/`)
  - [ ] `Synchronize()`の実装 (VAO/VBOの生成)
  - [ ] `DrawImpl()`の実装 (uniform変数設定 + 描画コマンド)
  - [ ] VAO以外のリソースがある場合は`Cleanup()`もオーバーライド
- [ ] GLSLシェーダーファイルの作成 (`src/graphics/shaders/glsl/`)
- [ ] `curves.h`または`surfaces.h`にシェーダーパス定数と`GetXxxShaderCode()`のcase を追加
- [ ] `factory.cpp`に`dynamic_cast`分岐を追加
- [ ] CMakeLists.txt にCPPファイルを追加
