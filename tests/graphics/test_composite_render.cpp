/**
 * @file tests/graphics/test_composite_render.cpp
 * @brief 複合ノード(統合後のEntityGraphics)の描画委譲の検証
 * @author Yayoi Habami
 * @date 2026-05-29
 * @copyright 2026 Yayoi Habami
 *
 * 対象: CompositeEntityGraphicsをEntityGraphicsへ統合(P3b)した後の、複合ノード
 *       (CompositeCurve 102) の挙動:
 *       - GetShaderType=kComposite、GetShaderTypesが子(Line→Segment)の型を含む
 *       - IsDrawableが全子要素の描画可否を反映する
 *       - Draw(子の型)で子へ描画委譲され、非該当の型では何も描画しない
 * TODO: 祖先Assembly/複合選択による子ハイライトはP4以降 (本テストは描画委譲のみ).
 */
#include <gtest/gtest.h>

#include <memory>
#include <utility>

#include "mock_open_gl.h"

#include "igesio/numerics/core/matrix.h"
#include "igesio/entities/curves/composite_curve.h"
#include "igesio/entities/curves/line.h"
#include "igesio/graphics/core/draw_context.h"
#include "igesio/graphics/core/i_entity_graphics.h"
#include "igesio/graphics/factory.h"



namespace {

namespace i_graph = igesio::graphics;
namespace i_ent = igesio::entities;
using i_graph::test::MockOpenGL;
using i_graph::ShaderType;

/// @brief 2本の線分(直角折れ)からなるCompositeCurveを生成する
/// @note 各Lineは線分(kSegment)のため、子描画オブジェクトはShaderType::kSegment
std::shared_ptr<i_ent::CompositeCurve> MakeCompositeOfSegments() {
    return i_ent::MakeCompositeCurve({
        i_ent::MakeLine(
            igesio::Vector3d{0.0, 0.0, 0.0}, igesio::Vector3d{1.0, 0.0, 0.0}),
        i_ent::MakeLine(
            igesio::Vector3d{1.0, 0.0, 0.0}, igesio::Vector3d{1.0, 1.0, 0.0})});
}

}  // namespace



// 複合ノードはkComposite型で、GetShaderTypesに子(Segment)の型を含み描画可能
TEST(CompositeRenderTest, ShaderTypesIncludeChildTypesAndDrawable) {
    auto gl = std::make_shared<MockOpenGL>();
    auto graphics = i_graph::CreateEntityGraphics(MakeCompositeOfSegments(), gl);
    ASSERT_NE(graphics, nullptr);

    EXPECT_EQ(graphics->GetShaderType(), ShaderType::kComposite);

    const auto types = graphics->GetShaderTypes();
    EXPECT_EQ(types.count(ShaderType::kSegment), 1u);

    // 全子要素のVAOが生成済み (factoryで子のSynchronizeが走る) のため描画可能
    EXPECT_TRUE(graphics->IsDrawable());
}

// 子の型(kSegment)で描画すると、子要素へ委譲されdraw呼び出しが発行される
TEST(CompositeRenderTest, Draw_DelegatesToChildrenForMatchingType) {
    auto gl = std::make_shared<MockOpenGL>();
    auto graphics = i_graph::CreateEntityGraphics(MakeCompositeOfSegments(), gl);
    ASSERT_NE(graphics, nullptr);

    const i_graph::DrawContext ctx{};
    graphics->Draw(/*shader=*/1u, ShaderType::kSegment,
                   std::pair<float, float>{800.0f, 600.0f}, ctx);

    // 子(2本の線分)が描画され、draw呼び出しが発行された
    EXPECT_GT(gl->draw_arrays_calls + gl->draw_elements_calls, 0);
}

// 子が持たない型で描画しても何も描画しない (自己描画もしない)
TEST(CompositeRenderTest, Draw_NoOutputForNonChildType) {
    auto gl = std::make_shared<MockOpenGL>();
    auto graphics = i_graph::CreateEntityGraphics(MakeCompositeOfSegments(), gl);
    ASSERT_NE(graphics, nullptr);

    const i_graph::DrawContext ctx{};
    graphics->Draw(/*shader=*/1u, ShaderType::kCircularArc,
                   std::pair<float, float>{800.0f, 600.0f}, ctx);

    EXPECT_EQ(gl->draw_arrays_calls + gl->draw_elements_calls, 0);
}
