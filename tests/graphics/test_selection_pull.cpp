/**
 * @file tests/graphics/test_selection_pull.cpp
 * @brief 選択ハイライトをDrawContextからPULLすることの検証 (焼き込み廃止の確認)
 * @author Yayoi Habami
 * @date 2026-05-29
 * @copyright 2026 Yayoi Habami
 *
 * 対象: EntityGraphics::Draw が DrawContext (SelectionSet参照) を見て
 *       mainColorをハイライト色/エンティティ色で出し分けること. 選択状態は
 *       描画オブジェクトへ焼き込まれず、SelectionSetの変化に追従すること.
 *       加えて、委譲ノード(複合: CompositeCurve 102)を選択した際、親の選択が
 *       force_highlightとして子へ伝播し、子(別ID)もハイライトされること(回帰).
 * TODO: 祖先Assembly選択による実効ハイライトはP4以降 (本テストはエンティティ/複合単位).
 */
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <memory>
#include <utility>

#include "mock_open_gl.h"

#include "igesio/numerics/matrix.h"
#include "igesio/entities/curves/circular_arc.h"
#include "igesio/entities/curves/composite_curve.h"
#include "igesio/entities/curves/line.h"
#include "igesio/models/selection_set.h"
#include "igesio/graphics/core/draw_context.h"
#include "igesio/graphics/core/i_entity_graphics.h"
#include "igesio/graphics/factory.h"



namespace {

namespace i_graph = igesio::graphics;
namespace i_ent = igesio::entities;
using i_graph::test::MockOpenGL;

/// @brief 浮動小数比較の許容誤差
constexpr float kTol = 1e-6f;
/// @brief テスト用のハイライト色 (エンティティ既定色と異なる)
constexpr std::array<float, 4> kHighlight = {1.0f, 0.6f, 0.0f, 1.0f};

/// @brief スモーク用の単純な円弧 (100)
std::shared_ptr<i_ent::CircularArc> MakeArc() {
    return std::make_shared<i_ent::CircularArc>(
        igesio::Vector2d(0.0, 0.0), igesio::Vector2d(1.0, 0.0),
        igesio::Vector2d(0.0, 1.0), 0.0);
}

/// @brief 2線分(直角折れ)からなるCompositeCurve(102)を生成する
/// @note 各Lineは線分(kSegment)のため、子描画オブジェクトはShaderType::kSegment.
///       親(102)と子(各Line)はそれぞれ別IDを持つ.
std::shared_ptr<i_ent::CompositeCurve> MakeComposite() {
    auto cc = std::make_shared<i_ent::CompositeCurve>();
    cc->AddCurve(std::make_shared<i_ent::Line>(
        igesio::Vector3d{0.0, 0.0, 0.0}, igesio::Vector3d{1.0, 0.0, 0.0}));
    cc->AddCurve(std::make_shared<i_ent::Line>(
        igesio::Vector3d{1.0, 0.0, 0.0}, igesio::Vector3d{1.0, 1.0, 0.0}));
    return cc;
}

}  // namespace



// 選択中はmainColorにハイライト色がPULLされる
TEST(SelectionPullTest, Draw_UsesHighlightColorWhenSelected) {
    auto gl = std::make_shared<MockOpenGL>();
    auto arc = MakeArc();
    auto graphics = i_graph::CreateEntityGraphics(arc, gl);
    ASSERT_NE(graphics, nullptr);

    igesio::models::SelectionSet selection;
    selection.Select(arc->GetID());
    i_graph::DrawContext ctx{};
    ctx.selection = &selection;
    ctx.highlight_color = kHighlight;

    graphics->Draw(1u, std::pair<float, float>{800.0f, 600.0f}, ctx);

    ASSERT_EQ(gl->last_vec_by_name.count("mainColor"), 1u);
    const auto& c = gl->last_vec_by_name.at("mainColor");
    ASSERT_EQ(c.size(), 4u);
    for (int i = 0; i < 4; ++i) {
        EXPECT_NEAR(c[i], kHighlight[i], kTol) << "component " << i;
    }
}

// 非選択時はmainColorにエンティティ色が使われる
TEST(SelectionPullTest, Draw_UsesEntityColorWhenNotSelected) {
    auto gl = std::make_shared<MockOpenGL>();
    auto arc = MakeArc();
    auto graphics = i_graph::CreateEntityGraphics(arc, gl);
    ASSERT_NE(graphics, nullptr);

    const std::array<float, 4> expected = graphics->GetColor();

    igesio::models::SelectionSet selection;  // 空
    i_graph::DrawContext ctx{};
    ctx.selection = &selection;
    ctx.highlight_color = kHighlight;

    graphics->Draw(1u, std::pair<float, float>{800.0f, 600.0f}, ctx);

    ASSERT_EQ(gl->last_vec_by_name.count("mainColor"), 1u);
    const auto& c = gl->last_vec_by_name.at("mainColor");
    ASSERT_EQ(c.size(), 4u);
    for (int i = 0; i < 4; ++i) {
        EXPECT_NEAR(c[i], expected[i], kTol) << "component " << i;
    }
}

// 選択を解除すると色はエンティティ色へ戻る (オブジェクトへ焼き込まれていない)
TEST(SelectionPullTest, Draw_HighlightFollowsSelectionState) {
    auto gl = std::make_shared<MockOpenGL>();
    auto arc = MakeArc();
    auto graphics = i_graph::CreateEntityGraphics(arc, gl);
    ASSERT_NE(graphics, nullptr);

    const std::array<float, 4> base = graphics->GetColor();

    igesio::models::SelectionSet selection;
    selection.Select(arc->GetID());
    i_graph::DrawContext ctx{};
    ctx.selection = &selection;
    ctx.highlight_color = kHighlight;

    // 選択中に描画 (ハイライト色)
    graphics->Draw(1u, std::pair<float, float>{800.0f, 600.0f}, ctx);
    // 選択を解除して再描画 (エンティティ色へ戻る)
    selection.Clear();
    graphics->Draw(1u, std::pair<float, float>{800.0f, 600.0f}, ctx);

    ASSERT_EQ(gl->last_vec_by_name.count("mainColor"), 1u);
    const auto& c = gl->last_vec_by_name.at("mainColor");
    ASSERT_EQ(c.size(), 4u);
    for (int i = 0; i < 4; ++i) {
        EXPECT_NEAR(c[i], base[i], kTol) << "component " << i;
    }
}



// 複合(102)を選択すると、委譲先の子の描画でmainColorがハイライト色になる
// (親の選択がforce_highlightとして子へ伝播する; 委譲ノードの選択ハイライト回帰)
TEST(SelectionPullTest, Draw_CompositeHighlightsChildrenWhenParentSelected) {
    auto gl = std::make_shared<MockOpenGL>();
    auto composite = MakeComposite();
    auto graphics = i_graph::CreateEntityGraphics(composite, gl);
    ASSERT_NE(graphics, nullptr);

    igesio::models::SelectionSet selection;
    selection.Select(composite->GetID());  // 親(102)のIDを選択
    i_graph::DrawContext ctx{};
    ctx.selection = &selection;
    ctx.highlight_color = kHighlight;

    // 子の型(kSegment)で描画 -> 子へ委譲される
    graphics->Draw(/*shader=*/1u, i_graph::ShaderType::kSegment,
                   std::pair<float, float>{800.0f, 600.0f}, ctx);

    ASSERT_EQ(gl->last_vec_by_name.count("mainColor"), 1u);
    const auto& c = gl->last_vec_by_name.at("mainColor");
    ASSERT_EQ(c.size(), 4u);
    for (int i = 0; i < 4; ++i) {
        EXPECT_NEAR(c[i], kHighlight[i], kTol) << "component " << i;
    }
}

// 親(102)が非選択なら、子はハイライトされない (force_highlightが立たない)
TEST(SelectionPullTest, Draw_CompositeChildrenNotHighlightedWhenParentNotSelected) {
    auto gl = std::make_shared<MockOpenGL>();
    auto composite = MakeComposite();
    auto graphics = i_graph::CreateEntityGraphics(composite, gl);
    ASSERT_NE(graphics, nullptr);

    igesio::models::SelectionSet selection;  // 空 (親も子も非選択)
    i_graph::DrawContext ctx{};
    ctx.selection = &selection;
    ctx.highlight_color = kHighlight;

    graphics->Draw(/*shader=*/1u, i_graph::ShaderType::kSegment,
                   std::pair<float, float>{800.0f, 600.0f}, ctx);

    ASSERT_EQ(gl->last_vec_by_name.count("mainColor"), 1u);
    const auto& c = gl->last_vec_by_name.at("mainColor");
    ASSERT_EQ(c.size(), 4u);
    // ハイライト色とは異なる (子はエンティティ色のまま)
    bool differs = false;
    for (int i = 0; i < 4; ++i) {
        if (std::fabs(c[i] - kHighlight[i]) > kTol) { differs = true; break; }
    }
    EXPECT_TRUE(differs)
            << "non-selected composite child must not be highlighted";
}
