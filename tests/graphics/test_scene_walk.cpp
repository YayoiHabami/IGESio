/**
 * @file tests/graphics/test_scene_walk.cpp
 * @brief レンダラのAssemblyツリー走査(累積変換のリフレッシュ・可視/抑制スキップ)の検証
 * @author Yayoi Habami
 * @date 2026-05-29
 * @copyright 2026 Yayoi Habami
 *
 * 対象: EntityRenderer::SetScene 設定時のDrawが、Assemblyツリーを走査して
 *       - 各エンティティのワールド変換を累積変換でリフレッシュし(model行列へ反映)、
 *       - 非表示/抑制サブツリーを描画から除外すること。
 * TODO: dirtyゲート(非dirty再描画で走査をスキップ)は走査回数の観測手段が無いため
 *       本テスト対象外(描画出力は不変のため正しさは他テストで担保).
 */
#include <gtest/gtest.h>

#include <array>
#include <memory>

#include "mock_open_gl.h"

#include "igesio/common/errors.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/entities/curves/circular_arc.h"
#include "igesio/models/assembly.h"
#include "igesio/models/scene.h"
#include "igesio/graphics/renderer.h"



namespace {

namespace i_graph = igesio::graphics;
namespace i_ent = igesio::entities;
namespace i_mod = igesio::models;
using i_graph::test::MockOpenGL;

/// @brief 浮動小数比較の許容誤差
constexpr float kTol = 1e-5f;

/// @brief スモーク用の単純な円弧 (M_entity=単位)
std::shared_ptr<i_ent::CircularArc> MakeArc() {
    return i_ent::MakeCircularArc(
        igesio::Vector2d(0.0, 0.0), igesio::Vector2d(1.0, 0.0),
        igesio::Vector2d(0.0, 1.0), 0.0);
}

/// @brief 平行移動の同次変換を作る
igesio::Matrix4d Translate(double x, double y, double z) {
    igesio::Matrix4d m = igesio::Matrix4d::Identity();
    m(0, 3) = x; m(1, 3) = y; m(2, 3) = z;
    return m;
}

}  // namespace



// 入れ子Assemblyの累積変換 (G_root·G_child) がmodel行列へ反映される
TEST(SceneWalkTest, NestedTransform_AccumulatedIntoModel) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);
    try {
        renderer.Initialize();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可: " << e.what();
    }

    auto root = std::make_shared<i_mod::Assembly>();
    root->SetGlobalTransform(Translate(1.0, 0.0, 0.0));
    auto child = std::make_shared<i_mod::Assembly>();
    child->SetGlobalTransform(Translate(0.0, 2.0, 0.0));
    root->AddChildAssembly(child);

    auto arc = MakeArc();
    child->AddEntity(arc);
    ASSERT_TRUE(renderer.AddEntity(arc));
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    renderer.Draw();

    ASSERT_EQ(gl->last_matrix_by_name.count("model"), 1u);
    const auto& m = gl->last_matrix_by_name.at("model");  // 列優先16要素
    // 累積 = translate(1,0,0)·translate(0,2,0) = translate(1,2,0) (M_entity=単位)
    EXPECT_NEAR(m[12], 1.0f, kTol);
    EXPECT_NEAR(m[13], 2.0f, kTol);
    EXPECT_NEAR(m[14], 0.0f, kTol);
}

// 非表示サブツリーは描画されない
TEST(SceneWalkTest, InvisibleSubtree_NotDrawn) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);
    try {
        renderer.Initialize();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可: " << e.what();
    }

    auto root = std::make_shared<i_mod::Assembly>();
    auto child = std::make_shared<i_mod::Assembly>();
    child->Metadata().visible = false;  // 非表示
    root->AddChildAssembly(child);

    auto arc = MakeArc();
    child->AddEntity(arc);
    ASSERT_TRUE(renderer.AddEntity(arc));
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    renderer.Draw();

    EXPECT_EQ(gl->draw_arrays_calls + gl->draw_elements_calls, 0);
}

// 抑制サブツリーは描画されない
TEST(SceneWalkTest, SuppressedSubtree_NotDrawn) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);
    try {
        renderer.Initialize();
    } catch (const igesio::ImplementationError& e) {
        GTEST_SKIP() << "シェーダー初期化不可: " << e.what();
    }

    auto root = std::make_shared<i_mod::Assembly>();
    auto child = std::make_shared<i_mod::Assembly>();
    child->Metadata().suppressed = true;  // 抑制
    root->AddChildAssembly(child);

    auto arc = MakeArc();
    child->AddEntity(arc);
    ASSERT_TRUE(renderer.AddEntity(arc));
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    renderer.Draw();

    EXPECT_EQ(gl->draw_arrays_calls + gl->draw_elements_calls, 0);
}
