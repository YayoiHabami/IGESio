/**
 * @file tests/graphics/test_renderer_smoke.cpp
 * @brief graphics再設計の安全網となる回帰スモークテスト
 * @author Yayoi Habami
 * @date 2026-05-29
 * @copyright 2026 Yayoi Habami
 *
 * 対象:
 * - 層A (オブジェクト単位・無条件): `CreateEntityGraphics`で生成した描画オブジェクトの
 *   `Draw(program, viewport)`が、`model`行列・`mainColor`・draw呼び出しを発行すること.
 *   GL文脈もシェーダー初期化も不要 (P2で焼き込み廃止→PULLへ変える際の基準).
 * - 層B (レンダラ単位・要Initialize): `Initialize()`→`AddEntity`→`SetScene`→`Draw()`が
 *   シェーダーバッチ経路 (UseProgram/共通uniform/型ループ) を通ること. シェーダー初期化
 *   (実GLSLの`__FILE__`基準読込) に失敗した環境では`GTEST_SKIP`する.
 *
 * TODO: エラーケース (無効エンティティ等) は本スモークの対象外. 詳細検証は後続フェーズで追加.
 */
#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <utility>

#include "mock_open_gl.h"

#include "igesio/common/errors.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/entities/curves/circular_arc.h"
#include "igesio/models/assembly.h"
#include "igesio/models/scene.h"
#include "igesio/graphics/core/draw_context.h"
#include "igesio/graphics/core/i_entity_graphics.h"
#include "igesio/graphics/factory.h"
#include "igesio/graphics/renderer.h"



namespace {

namespace i_graph = igesio::graphics;
namespace i_ent = igesio::entities;
namespace i_mod = igesio::models;
using i_graph::test::MockOpenGL;

/// @brief 浮動小数比較の許容誤差
constexpr float kTol = 1e-6f;

/// @brief スモーク用の単純な円弧 (100) を生成する
/// @return 中心(0,0)・始点(1,0)・終点(0,1)・z=0 の四分円
/// @note CircularArcはShaderType::kCircularArc (実装済み) に対応するため、層Bでも描画される
std::shared_ptr<i_ent::CircularArc> MakeArc() {
    return i_ent::MakeCircularArc(
        igesio::Vector2d(0.0, 0.0), igesio::Vector2d(1.0, 0.0),
        igesio::Vector2d(0.0, 1.0), 0.0);
}

}  // namespace



/**
 * 層A: オブジェクト単位 (無条件・GL/シェーダー初期化不要)
 */

// 描画オブジェクトのDrawがmodel/mainColor/draw呼び出しを発行する
TEST(GraphicsObjectSmokeTest, Draw_EmitsModelColorAndDrawCall) {
    auto gl = std::make_shared<MockOpenGL>();
    auto graphics = i_graph::CreateEntityGraphics(MakeArc(), gl);
    ASSERT_NE(graphics, nullptr);

    // ハイライト前 (非選択) の色 = エンティティ色
    const std::array<float, 4> expected = graphics->GetColor();

    // 任意のプログラムID (モックはlocationを名前で採番するため実プログラム不要)
    // 非選択の表示コンテキスト (ハイライトしない)
    const i_graph::DrawContext ctx{};
    graphics->Draw(/*shader=*/1u, std::pair<float, float>{800.0f, 600.0f}, ctx);

    // model行列が設定された
    EXPECT_EQ(gl->last_matrix_by_name.count("model"), 1u);

    // mainColorがエンティティ色で設定された (焼き込みではない基準値)
    ASSERT_EQ(gl->last_vec_by_name.count("mainColor"), 1u);
    const auto& color = gl->last_vec_by_name.at("mainColor");
    ASSERT_EQ(color.size(), 4u);
    for (int i = 0; i < 4; ++i) {
        EXPECT_NEAR(color[i], expected[i], kTol) << "component " << i;
    }

    // draw呼び出しが1回以上発行された (DrawArrays/DrawElementsいずれか)
    EXPECT_GT(gl->draw_arrays_calls + gl->draw_elements_calls, 0);
}



/**
 * 層B: レンダラ単位 (要Initialize・失敗時はskip)
 */

// Draw()がシェーダーバッチ経路を通り、登録エンティティを描画する
TEST(RendererSmokeTest, Draw_RunsShaderBucketingPath) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);

    try {
        renderer.Initialize();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可 (GLSL未解決の可能性): " << e.what();
    }

    auto arc = MakeArc();
    ASSERT_TRUE(renderer.AddEntity(arc));

    // 描画はScene走査に一本化されたため、rootへarcを入れSceneを束ねる
    auto root = std::make_shared<i_mod::Assembly>();
    root->AddEntity(arc);
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    renderer.Draw();

    // シェーダープログラムが使用され、modelとdraw呼び出しが発行された
    EXPECT_FALSE(gl->used_programs.empty());
    EXPECT_EQ(gl->last_matrix_by_name.count("model"), 1u);
    EXPECT_GT(gl->draw_arrays_calls + gl->draw_elements_calls, 0);
}
