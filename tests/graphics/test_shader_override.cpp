/**
 * @file tests/graphics/test_shader_override.cpp
 * @brief レンダラ単位のシェーダー差し替えと材質オーバーライドのgetterの検証
 * @author Yayoi Habami
 * @date 2026-09-25
 * @copyright 2026 Yayoi Habami
 * @note テスト対象:
 *       - `EntityRenderer::SetShaderOverride`の検査 (同一ID、未登録、センチネル、
 *         カテゴリ不一致)
 *       - 差し替え時の描画プログラム、`setup`の呼び出し、光源uniformの要否
 *       - 解除 (`ClearShaderOverride`/`ClearShaderOverrides`) と`FindShaderOverride`
 *       - 表示モード (kWireFrame) の取捨が元IDのカテゴリで行われること
 *       - `FindMaterialProperty`と、テクスチャの貼り直し/解除での旧テクスチャ解放
 * @note ShaderRegistryに登録解除は無いため、登録は関数内staticで1回に限る.
 *       プログラムIDは実装の採番に依存するため、差し替えの有無で使われた
 *       プログラム集合の差として観測する.
 */
#include <gtest/gtest.h>

#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "mock_open_gl.h"

#include "igesio/common/errors.h"
#include "igesio/entities/surfaces/rational_b_spline_surface.h"
#include "igesio/models/assembly.h"
#include "igesio/models/scene.h"
#include "igesio/graphics/core/material_property.h"
#include "igesio/graphics/core/texture.h"
#include "igesio/graphics/renderer.h"
#include "igesio/graphics/shader_registry.h"

namespace {

namespace i_graph = igesio::graphics;
namespace i_ent = igesio::entities;
namespace i_mod = igesio::models;
using igesio::Vector3d;
using i_graph::ShaderCode;
using i_graph::ShaderDrawCategory;
using i_graph::ShaderId;
using i_graph::ShaderInfo;
using i_graph::ShaderOverride;
using i_graph::ShaderRegistry;
using i_graph::test::MockOpenGL;

/// @brief 差し替え先として登録するシェーダーを取得する (初回のみ登録)
/// @param name 一意なシェーダー名
/// @param uses_lighting 光源を使用するか
/// @param category 表示モードによる取捨のカテゴリ
/// @return 登録済みのShaderId
/// @note MockOpenGLはコンパイルを常に成功させるため、コードはダミーでよい
ShaderId GetOrRegister(const std::string& name, const bool uses_lighting,
                       const ShaderDrawCategory category) {
    if (const auto found = ShaderRegistry::Find(name)) return *found;
    ShaderInfo info;
    info.name = name;
    info.code = ShaderCode("void main() {}", "void main() {}");
    info.uses_lighting = uses_lighting;
    info.category = category;
    return ShaderRegistry::Register(info);
}

/// @brief 光源を使う面塗りの差し替え先を取得する
/// @return 登録済みのShaderId
ShaderId LitFillShader() {
    return GetOrRegister("TestOverrideLitFill", true,
                         ShaderDrawCategory::kSurfaceFill);
}

/// @brief 光源を使わない面塗りの差し替え先を取得する
/// @return 登録済みのShaderId
ShaderId UnlitFillShader() {
    return GetOrRegister("TestOverrideUnlitFill", false,
                         ShaderDrawCategory::kSurfaceFill);
}

/// @brief 双一次NURBS平面 (z=0, ドメイン0.0〜1.0の正方形) を作成する
/// @return 128の平面
std::shared_ptr<i_ent::RationalBSplineSurface> MakePlane() {
    const std::vector<std::vector<Vector3d>> grid{
        {{0, 0, 0}, {0, 1, 0}},
        {{1, 0, 0}, {1, 1, 0}}};
    return i_ent::MakeBezierSurface(grid);
}

/// @brief 2x2のRGBAテクスチャを作成する
/// @param value 全画素の値
/// @return テクスチャ
i_graph::Texture MakeTexture(const unsigned char value) {
    const std::vector<unsigned char> data(2 * 2 * 4, value);
    return i_graph::Texture(2, 2, true, data.data());
}

/// @brief 平面1つのSceneと初期化済みレンダラをまとめたテスト用の構成
struct Fixture {
    /// @brief GLモック
    std::shared_ptr<MockOpenGL> gl = std::make_shared<MockOpenGL>();
    /// @brief 平面
    std::shared_ptr<i_ent::RationalBSplineSurface> plane = MakePlane();
    /// @brief ルートアセンブリ
    std::shared_ptr<i_mod::Assembly> root = i_mod::MakeAssembly();
    /// @brief シーン
    std::unique_ptr<i_mod::Scene> scene;
    /// @brief レンダラ
    std::unique_ptr<i_graph::EntityRenderer> renderer;

    /// @brief 構成を作成してレンダラを初期化する
    /// @throw igesio::ImplementationError シェーダーの初期化に失敗した場合
    Fixture() {
        root->AddEntity(plane);
        scene = std::make_unique<i_mod::Scene>(root);
        renderer = std::make_unique<i_graph::EntityRenderer>(gl);
        renderer->Initialize();
        renderer->SetScene(scene.get());
    }

    /// @brief 1回描画し、使われたプログラムIDの集合を取得する
    /// @return UseProgramに渡されたIDの集合
    std::set<igesio::graphics::gl::Uint> DrawAndCollectPrograms() {
        gl->used_programs.clear();
        renderer->Draw();
        return std::set<igesio::graphics::gl::Uint>(
                gl->used_programs.begin(), gl->used_programs.end());
    }
};

}  // namespace



/**
 * 異常系: 差し替えの検査
 */

// 元と差し替え先が同一IDの場合は例外 (失敗した設定は残らない)
TEST(ShaderOverrideTest, SetShaderOverride_ThrowsInvalidArgumentWhenSameId) {
    i_graph::EntityRenderer renderer;
    const auto base = ShaderId::kRationalBSplineSurface;
    EXPECT_THROW(renderer.SetShaderOverride(base, ShaderOverride{base, {}}),
                 std::invalid_argument);
    EXPECT_EQ(renderer.FindShaderOverride(base), nullptr);
}

// 差し替え先が未登録の場合は例外 (登録済みなら有効)
TEST(ShaderOverrideTest, SetShaderOverride_ThrowsInvalidArgumentWhenUnregistered) {
    i_graph::EntityRenderer renderer;
    const auto base = ShaderId::kRationalBSplineSurface;
    EXPECT_THROW(renderer.SetShaderOverride(
                         base, ShaderOverride{ShaderId(0x00FFFFFFu), {}}),
                 std::invalid_argument);
    EXPECT_NO_THROW(renderer.SetShaderOverride(
            base, ShaderOverride{LitFillShader(), {}}));
}

// 差し替え先がコードを持たないセンチネルの場合は例外
TEST(ShaderOverrideTest, SetShaderOverride_ThrowsInvalidArgumentWhenSentinel) {
    i_graph::EntityRenderer renderer;
    EXPECT_THROW(renderer.SetShaderOverride(
                         ShaderId::kRationalBSplineSurface,
                         ShaderOverride{ShaderId::kComposite, {}}),
                 std::invalid_argument);
}

// カテゴリが一致しない場合は例外 (面塗り同士なら有効)
TEST(ShaderOverrideTest, SetShaderOverride_ThrowsInvalidArgumentWhenCategoryDiffers) {
    i_graph::EntityRenderer renderer;
    const auto base = ShaderId::kRationalBSplineSurface;
    const auto always = GetOrRegister(
            "TestOverrideAlways", false, ShaderDrawCategory::kAlways);
    EXPECT_THROW(renderer.SetShaderOverride(base, ShaderOverride{always, {}}),
                 std::invalid_argument);
    EXPECT_NO_THROW(renderer.SetShaderOverride(
            base, ShaderOverride{UnlitFillShader(), {}}));
}



/**
 * 正常系: 描画への反映
 */

// 差し替え中は元のプログラムの代わりに差し替え先のプログラムで描かれ、
// setupには差し替え先のプログラムIDが渡される
TEST(ShaderOverrideTest, Draw_UsesOverrideProgramAndCallsSetup) {
    std::unique_ptr<Fixture> f;
    try {
        f = std::make_unique<Fixture>();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可: " << e.what();
    }
    const auto before = f->DrawAndCollectPrograms();

    int setup_calls = 0;
    igesio::graphics::gl::Uint setup_program = 0;
    f->renderer->SetShaderOverride(
            ShaderId::kRationalBSplineSurface,
            ShaderOverride{LitFillShader(),
                           [&](i_graph::IOpenGL&,
                               const igesio::graphics::gl::Uint program) {
                               ++setup_calls;
                               setup_program = program;
                           }});
    const auto after = f->DrawAndCollectPrograms();

    EXPECT_EQ(setup_calls, 1);
    EXPECT_EQ(after.count(setup_program), 1u);
    EXPECT_EQ(before.count(setup_program), 0u);
    // 元のプログラムだけが使われなくなる (エッジ等の他のバケットは不変)
    std::vector<igesio::graphics::gl::Uint> dropped;
    for (const auto p : before) {
        if (after.count(p) == 0) dropped.push_back(p);
    }
    EXPECT_EQ(dropped.size(), 1u);
    EXPECT_EQ(after.size(), before.size());
}

// 光源uniformの要否は差し替え先のメタ情報で決まる
TEST(ShaderOverrideTest, Draw_LightingFollowsOverrideShader) {
    std::unique_ptr<Fixture> f;
    try {
        f = std::make_unique<Fixture>();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可: " << e.what();
    }

    f->renderer->SetShaderOverride(ShaderId::kRationalBSplineSurface,
                                   ShaderOverride{UnlitFillShader(), {}});
    f->gl->last_vec_by_name.clear();
    f->renderer->Draw();
    EXPECT_EQ(f->gl->last_vec_by_name.count("viewPos_WorldSpace"), 0u);

    f->renderer->SetShaderOverride(ShaderId::kRationalBSplineSurface,
                                   ShaderOverride{LitFillShader(), {}});
    f->gl->last_vec_by_name.clear();
    f->renderer->Draw();
    EXPECT_EQ(f->gl->last_vec_by_name.count("viewPos_WorldSpace"), 1u);
}

// 解除後は元のプログラムで描かれ、Findはnullptrを返す
TEST(ShaderOverrideTest, ClearShaderOverride_RestoresOriginalProgram) {
    std::unique_ptr<Fixture> f;
    try {
        f = std::make_unique<Fixture>();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可: " << e.what();
    }
    const auto before = f->DrawAndCollectPrograms();

    const auto base = ShaderId::kRationalBSplineSurface;
    f->renderer->SetShaderOverride(base, ShaderOverride{LitFillShader(), {}});
    const auto* found = f->renderer->FindShaderOverride(base);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->shader, LitFillShader());
    EXPECT_EQ(f->renderer->FindShaderOverride(ShaderId::kGeneralSurface),
              nullptr);
    EXPECT_NE(f->DrawAndCollectPrograms(), before);

    EXPECT_TRUE(f->renderer->ClearShaderOverride(base));
    EXPECT_FALSE(f->renderer->ClearShaderOverride(base));
    EXPECT_EQ(f->renderer->FindShaderOverride(base), nullptr);
    EXPECT_EQ(f->DrawAndCollectPrograms(), before);

    // 一括解除も同じく元に戻す
    f->renderer->SetShaderOverride(base, ShaderOverride{LitFillShader(), {}});
    f->renderer->ClearShaderOverrides();
    EXPECT_EQ(f->DrawAndCollectPrograms(), before);
}

// kWireFrameでは差し替えの有無に関わらず面塗りを描かない (元IDのカテゴリで取捨)
TEST(ShaderOverrideTest, Draw_WireFrameSkipsOverriddenFill) {
    std::unique_ptr<Fixture> f;
    try {
        f = std::make_unique<Fixture>();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可: " << e.what();
    }
    f->renderer->SetDisplayMode(i_graph::DisplayMode::kWireFrame);
    const auto wire = f->DrawAndCollectPrograms();

    int setup_calls = 0;
    f->renderer->SetShaderOverride(
            ShaderId::kRationalBSplineSurface,
            ShaderOverride{LitFillShader(),
                           [&](i_graph::IOpenGL&, igesio::graphics::gl::Uint) {
                               ++setup_calls;
                           }});
    EXPECT_EQ(f->DrawAndCollectPrograms(), wire);
    EXPECT_EQ(setup_calls, 0);
}



/**
 * 材質オーバーライドのgetterとテクスチャの解放
 */

// Findは設定前nullptr、設定後は設定値、解除後nullptrを返す
TEST(MaterialOverrideTest, FindMaterialProperty_ReturnsCurrentOverride) {
    i_graph::EntityRenderer renderer;
    const auto plane = MakePlane();
    EXPECT_EQ(renderer.FindMaterialProperty(plane->GetID()), nullptr);

    i_graph::MaterialProperty mp;
    mp.metallic = 0.9f;
    renderer.SetMaterialProperty(plane->GetID(), mp);
    const auto* found = renderer.FindMaterialProperty(plane->GetID());
    ASSERT_NE(found, nullptr);
    EXPECT_FLOAT_EQ(found->metallic, 0.9f);

    renderer.ClearMaterialProperty(plane->GetID());
    EXPECT_EQ(renderer.FindMaterialProperty(plane->GetID()), nullptr);
}

// テクスチャの貼り直しと解除のいずれでも旧テクスチャが解放される
TEST(MaterialOverrideTest, SyncTexture_ReleasesPreviousTexture) {
    std::unique_ptr<Fixture> f;
    try {
        f = std::make_unique<Fixture>();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可: " << e.what();
    }
    const auto id = f->plane->GetID();

    i_graph::MaterialProperty mp;
    mp.texture = MakeTexture(10);
    mp.use_texture = true;
    f->renderer->SetMaterialProperty(id, mp);
    f->renderer->Draw();
    const int after_first = f->gl->delete_textures_calls;

    mp.texture = MakeTexture(20);
    f->renderer->SetMaterialProperty(id, mp);
    f->renderer->Draw();
    const int after_second = f->gl->delete_textures_calls;
    EXPECT_GE(after_second - after_first, 1);

    f->renderer->ClearMaterialProperty(id);
    f->renderer->Draw();
    EXPECT_GE(f->gl->delete_textures_calls - after_second, 1);
}
