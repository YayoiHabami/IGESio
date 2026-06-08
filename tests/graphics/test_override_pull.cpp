/**
 * @file tests/graphics/test_override_pull.cpp
 * @brief レンダラのオーバーライド最近接PULL(色/不透明度)の検証
 * @author Yayoi Habami
 * @date 2026-05-29
 * @copyright 2026 Yayoi Habami
 *
 * 対象: SetScene 設定時のDrawが、Assemblyツリーの色/不透明度オーバーライドを
 *       最近接優先で解決し、各エンティティへフレーム毎にPUSHして mainColor へ反映すること.
 *       - 親の color_override が子孫へ継承される
 *       - 子の color_override が祖先を上書きする (最近接優先)
 *       - opacity_override は alpha のみ差し替え、RGB は entity 固有色を維持する
 *       - オーバーライド無しでは entity 固有色をそのまま使う
 */
#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <vector>

#include "mock_open_gl.h"

#include "igesio/common/errors.h"
#include "igesio/entities/curves/circular_arc.h"
#include "igesio/entities/interfaces/i_entity_identifier.h"
#include "igesio/models/assembly.h"
#include "igesio/models/scene.h"
#include "igesio/graphics/factory.h"
#include "igesio/graphics/renderer.h"



namespace {

namespace i_graph = igesio::graphics;
namespace i_ent = igesio::entities;
namespace i_mod = igesio::models;
using i_graph::test::MockOpenGL;

/// @brief 浮動小数比較の許容誤差
constexpr float kTol = 1e-4f;

/// @brief スモーク用の単純な円弧 (M_entity=単位)
std::shared_ptr<i_ent::CircularArc> MakeArc() {
    return i_ent::MakeCircularArc(
        igesio::Vector2d(0.0, 0.0), igesio::Vector2d(1.0, 0.0),
        igesio::Vector2d(0.0, 1.0), 0.0);
}

/// @brief エンティティ固有色 (オーバーライド無し時の mainColor) のオラクルを得る
/// @note レンダラに投入するものとは別の描画オブジェクトから取得する.
///       GetColor()は{base_rgb, material_opacity}を返し、グローバルパラメータに依存しない.
std::array<float, 4> NaturalColor(
        const std::shared_ptr<i_ent::CircularArc>& arc,
        const std::shared_ptr<MockOpenGL>& gl) {
    auto ref = i_graph::CreateEntityGraphics(
        std::static_pointer_cast<const i_ent::IEntityIdentifier>(arc), gl);
    auto c = ref->GetColor();
    return {c[0], c[1], c[2], c[3]};
}

}  // namespace



// 親の color_override が子孫エンティティへ継承される
TEST(OverridePullTest, ParentColorOverride_AppliesToDescendant) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);
    try {
        renderer.Initialize();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可: " << e.what();
    }

    auto root = i_mod::MakeAssembly();
    root->SetColorOverride(std::array<float, 3>{1.0f, 0.0f, 0.0f});
    auto child = i_mod::MakeAssembly();
    root->AddChildAssembly(child);

    auto arc = MakeArc();
    child->AddEntity(arc);
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    renderer.Draw();

    ASSERT_EQ(gl->last_vec_by_name.count("mainColor"), 1u);
    const auto& c = gl->last_vec_by_name.at("mainColor");
    ASSERT_EQ(c.size(), 4u);
    EXPECT_NEAR(c[0], 1.0f, kTol);
    EXPECT_NEAR(c[1], 0.0f, kTol);
    EXPECT_NEAR(c[2], 0.0f, kTol);
}

// 子の color_override が祖先を上書きする (最近接優先)
TEST(OverridePullTest, NearestColorOverrideWins) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);
    try {
        renderer.Initialize();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可: " << e.what();
    }

    auto root = i_mod::MakeAssembly();
    root->SetColorOverride(std::array<float, 3>{1.0f, 0.0f, 0.0f});
    auto child = i_mod::MakeAssembly();
    child->SetColorOverride(std::array<float, 3>{0.0f, 1.0f, 0.0f});
    root->AddChildAssembly(child);

    auto arc = MakeArc();
    child->AddEntity(arc);
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    renderer.Draw();

    ASSERT_EQ(gl->last_vec_by_name.count("mainColor"), 1u);
    const auto& c = gl->last_vec_by_name.at("mainColor");
    ASSERT_EQ(c.size(), 4u);
    EXPECT_NEAR(c[0], 0.0f, kTol);
    EXPECT_NEAR(c[1], 1.0f, kTol);
    EXPECT_NEAR(c[2], 0.0f, kTol);
}

// opacity_override は alpha のみ差し替え、RGB は entity 固有色を維持する
TEST(OverridePullTest, OpacityOverride_ChangesAlphaKeepsRgb) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);
    try {
        renderer.Initialize();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可: " << e.what();
    }

    auto arc = MakeArc();
    const auto natural = NaturalColor(arc, gl);

    auto root = i_mod::MakeAssembly();
    root->SetOpacityOverride(0.5f);
    root->AddEntity(arc);
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    renderer.Draw();

    ASSERT_EQ(gl->last_vec_by_name.count("mainColor"), 1u);
    const auto& c = gl->last_vec_by_name.at("mainColor");
    ASSERT_EQ(c.size(), 4u);
    EXPECT_NEAR(c[0], natural[0], kTol);
    EXPECT_NEAR(c[1], natural[1], kTol);
    EXPECT_NEAR(c[2], natural[2], kTol);
    EXPECT_NEAR(c[3], 0.5f, kTol);
}

// オーバーライド無しでは entity 固有色をそのまま使う
TEST(OverridePullTest, NoOverride_UsesEntityColor) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);
    try {
        renderer.Initialize();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可: " << e.what();
    }

    auto arc = MakeArc();
    const auto natural = NaturalColor(arc, gl);

    auto root = i_mod::MakeAssembly();
    root->AddEntity(arc);
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    renderer.Draw();

    ASSERT_EQ(gl->last_vec_by_name.count("mainColor"), 1u);
    const auto& c = gl->last_vec_by_name.at("mainColor");
    ASSERT_EQ(c.size(), 4u);
    EXPECT_NEAR(c[0], natural[0], kTol);
    EXPECT_NEAR(c[1], natural[1], kTol);
    EXPECT_NEAR(c[2], natural[2], kTol);
    EXPECT_NEAR(c[3], natural[3], kTol);
}
