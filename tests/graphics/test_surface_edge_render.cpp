/**
 * @file tests/graphics/test_surface_edge_render.cpp
 * @brief サーフェス境界エッジ(kSurfaceEdge)描画と表示モード(DisplayMode)のテスト
 * @author Yayoi Habami
 * @date 2026-06-03
 * @copyright 2026 Yayoi Habami
 *
 * 対象:
 * - 層A (オブジェクト単位): サーフェスグラフィックスがGetShaderTypesにkSurfaceEdgeを
 *   含み、kSurfaceEdge指定時に線(DrawArrays)とエッジ色を発行すること.
 * - 層B (レンダラ単位・要Initialize): DisplayModeに応じてExecuteDrawListが面塗り
 *   (DrawElements) と面エッジ (DrawArrays) を取捨すること.
 *   面=DrawElements・エッジ=DrawArraysで区別できる未トリムTrimmedSurfaceを用いる.
 *
 * TODO: shaded時の面とエッジのZファイティング (深度バイアス) は本テストの対象外.
 */
#include <gtest/gtest.h>

#include <memory>
#include <utility>

#include "mock_open_gl.h"

#include "igesio/common/errors.h"
#include "igesio/entities/surfaces/trimmed_surface.h"
#include "igesio/models/assembly.h"
#include "igesio/models/scene.h"
#include "igesio/graphics/core/draw_context.h"
#include "igesio/graphics/core/i_entity_graphics.h"
#include "igesio/graphics/core/surface_edge_buffer.h"
#include "igesio/graphics/factory.h"
#include "igesio/graphics/renderer.h"

#include "../entities/surfaces/surfaces_for_testing.h"



namespace {

namespace i_graph = igesio::graphics;
namespace i_ent = igesio::entities;
namespace i_mod = igesio::models;
namespace i_test = igesio::tests;
using i_graph::test::MockOpenGL;
using i_graph::ShaderType;

/// @brief 未トリムのTrimmedSurface (フリーフォームNURBS基底) を生成する
/// @note 面はメッシュ(DrawElements)、エッジは線分(DrawArrays)で発行されるため、
///       表示モードによる取捨を描画コマンド種別で判別できる.
///       AssemblyへはEntityBase派生の具象型として追加するため、TrimmedSurfaceを返す.
std::shared_ptr<i_ent::TrimmedSurface> MakeUntrimmedSurface() {
    auto base = i_test::CreateRationalBSplineSurfaces()[1].surface;  // freeform
    return i_ent::MakeTrimmedSurface(base, nullptr, {});
}

}  // namespace



/**
 * 層A: オブジェクト単位
 */

TEST(SurfaceEdgeRenderTest, GetShaderTypes_IncludesSurfaceEdge) {
    auto gl = std::make_shared<MockOpenGL>();
    auto graphics = i_graph::CreateEntityGraphics(MakeUntrimmedSurface(), gl);
    ASSERT_NE(graphics, nullptr);

    const auto types = graphics->GetShaderTypes();
    EXPECT_EQ(types.count(ShaderType::kGeneralSurface), 1u);
    EXPECT_EQ(types.count(ShaderType::kSurfaceEdge), 1u);
}

TEST(SurfaceEdgeRenderTest, Draw_DispatchesMeshAndEdgeByShaderType) {
    auto gl = std::make_shared<MockOpenGL>();
    auto graphics = i_graph::CreateEntityGraphics(MakeUntrimmedSurface(), gl);
    ASSERT_NE(graphics, nullptr);
    const i_graph::DrawContext ctx{};
    const std::pair<float, float> vp{800.0f, 600.0f};

    // 面シェーダー: メッシュ(DrawElements)のみ、線は発行しない
    graphics->Draw(1u, ShaderType::kGeneralSurface, vp, ctx);
    EXPECT_GT(gl->draw_elements_calls, 0);
    EXPECT_EQ(gl->draw_arrays_calls, 0);

    // エッジシェーダー: 線(DrawArrays)のみ
    gl->draw_arrays_calls = 0;
    gl->draw_elements_calls = 0;
    graphics->Draw(1u, ShaderType::kSurfaceEdge, vp, ctx);
    EXPECT_GT(gl->draw_arrays_calls, 0);
    EXPECT_EQ(gl->draw_elements_calls, 0);

    // 非選択時のエッジ色はkSurfaceEdgeColor
    ASSERT_EQ(gl->last_vec_by_name.count("mainColor"), 1u);
    const auto& color = gl->last_vec_by_name.at("mainColor");
    ASSERT_EQ(color.size(), 4u);
    for (int i = 0; i < 4; ++i) {
        EXPECT_NEAR(color[i], i_graph::kSurfaceEdgeColor[i], 1e-6f)
                << "component " << i;
    }
}



/**
 * 層B: レンダラ単位 (要Initialize・失敗時はskip)
 */

TEST(SurfaceEdgeRenderTest, DisplayMode_FiltersFillAndEdge) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);
    try {
        renderer.Initialize();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可 (GLSL未解決の可能性): " << e.what();
    }

    auto surf = MakeUntrimmedSurface();
    ASSERT_TRUE(renderer.AddEntity(surf));
    auto root = i_mod::MakeAssembly();
    root->AddEntity(surf);
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    // kShaded: 面(DrawElements)とエッジ(DrawArrays)の両方を描画する
    gl->draw_arrays_calls = 0;
    gl->draw_elements_calls = 0;
    renderer.SetDisplayMode(i_graph::DisplayMode::kShaded);
    renderer.Draw();
    EXPECT_GT(gl->draw_elements_calls, 0);
    EXPECT_GT(gl->draw_arrays_calls, 0);

    // kNoEdge: 面のみ (エッジは描画しない)
    gl->draw_arrays_calls = 0;
    gl->draw_elements_calls = 0;
    renderer.SetDisplayMode(i_graph::DisplayMode::kNoEdge);
    renderer.Draw();
    EXPECT_GT(gl->draw_elements_calls, 0);
    EXPECT_EQ(gl->draw_arrays_calls, 0);

    // kWireFrame: エッジのみ (面は描画しない)
    gl->draw_arrays_calls = 0;
    gl->draw_elements_calls = 0;
    renderer.SetDisplayMode(i_graph::DisplayMode::kWireFrame);
    renderer.Draw();
    EXPECT_EQ(gl->draw_elements_calls, 0);
    EXPECT_GT(gl->draw_arrays_calls, 0);
}
