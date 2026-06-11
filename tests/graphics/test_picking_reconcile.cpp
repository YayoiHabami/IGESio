/**
 * @file tests/graphics/test_picking_reconcile.cpp
 * @brief ピッキングとSceneツリーの整合 (visible_list_走査) の検証
 * @author Yayoi Habami
 * @date 2026-06-06
 * @copyright 2026 Yayoi Habami
 *
 * 対象: PickEntitiesが描画キャッシュ直走査ではなく可視リストを走査することで、
 *       - 可視エンティティはヒットする (基準)
 *       - ツリーから削除されたエンティティはヒットしない (旧実装のバグの回帰)
 *       - 非表示 (visible=false)・抑制 (suppressed) のエンティティはヒットしない
 *       - DisplayFilterで除外された型はヒットしない
 *
 * NOTE: レイは円弧 (中心原点・半径1・z=0平面の1/4円) 上の45°点を-Z方向に貫く
 *       固定レイを用いる. ピック前のDraw呼び出しは不要 (EnsureSyncedが
 *       ピック冒頭でも実行される) ことの検証を兼ねて、Drawせずにピックする.
 * TODO: PickEntitiesInRectの整合は判定本体が共通 (visible_list_走査) のため
 *       PickEntities側で代表してカバーする.
 */
#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <vector>

#include "mock_open_gl.h"

#include "igesio/common/errors.h"
#include "igesio/entities/curves/circular_arc.h"
#include "igesio/models/assembly.h"
#include "igesio/models/scene.h"
#include "igesio/graphics/renderer.h"

namespace {

namespace i_graph = igesio::graphics;
namespace i_ent = igesio::entities;
namespace i_mod = igesio::models;
using igesio::Vector2d;
using igesio::Vector3d;
using i_graph::test::MockOpenGL;

/// @brief ピック対象の円弧 (中心原点・半径1・z=0平面の1/4円)
std::shared_ptr<i_ent::CircularArc> MakeArc() {
    return i_ent::MakeCircularArc(
        Vector2d(0.0, 0.0), Vector2d(1.0, 0.0), Vector2d(0.0, 1.0), 0.0);
}

/// @brief 円弧上の45°点を-Z方向へ貫くレイ
i_graph::Ray MakeRayThroughArc() {
    const double c = std::sqrt(0.5);  // cos(45°) = sin(45°)
    return i_graph::Ray{Vector3d(c, c, 5.0), Vector3d(0.0, 0.0, -1.0)};
}

/// @brief 指定レンダラで固定レイのピックを実行する
std::vector<i_graph::EntityHit> Pick(i_graph::EntityRenderer& renderer) {
    // スクリーン座標はヒット許容量のピクセル換算にのみ使われるため画面中央を渡す
    return renderer.PickEntities(MakeRayThroughArc(), 640.0, 360.0);
}

}  // namespace



// 基準: Sceneツリー上の可視エンティティはヒットする (事前のDraw不要)
TEST(PickingReconcileTest, Pick_HitsVisibleEntity) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);

    auto root = i_mod::MakeAssembly();
    auto arc = MakeArc();
    root->AddEntity(arc);
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    const auto hits = Pick(renderer);
    ASSERT_FALSE(hits.empty());
    EXPECT_EQ(hits.front().id, arc->GetID());
}

// ツリーから削除されたエンティティはヒットしない (旧実装の不整合の回帰)
TEST(PickingReconcileTest, Pick_RemovedEntityNotHit) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);

    auto root = i_mod::MakeAssembly();
    auto arc = MakeArc();
    root->AddEntity(arc);
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    ASSERT_FALSE(Pick(renderer).empty());  // 削除前はヒットする

    ASSERT_TRUE(root->RemoveEntity(arc->GetID()));
    EXPECT_TRUE(Pick(renderer).empty());
}

// 非表示・抑制サブツリーのエンティティはヒットしない
TEST(PickingReconcileTest, Pick_InvisibleAndSuppressedNotHit) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);

    auto root = i_mod::MakeAssembly();
    auto child = i_mod::MakeAssembly();
    root->AddChildAssembly(child);
    child->AddEntity(MakeArc());
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    ASSERT_FALSE(Pick(renderer).empty());  // 可視状態ではヒットする

    child->SetVisible(false);
    EXPECT_TRUE(Pick(renderer).empty());

    child->SetVisible(true);
    ASSERT_FALSE(Pick(renderer).empty());  // 復帰の確認

    child->SetSuppressed(true);
    EXPECT_TRUE(Pick(renderer).empty());
}

// DisplayFilterで除外された型はヒットしない
TEST(PickingReconcileTest, Pick_FilteredTypeNotHit) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);

    auto root = i_mod::MakeAssembly();
    root->AddEntity(MakeArc());
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    ASSERT_FALSE(Pick(renderer).empty());  // フィルタ無しではヒットする

    i_graph::DisplayFilter filter;
    filter.hidden_types.insert(i_ent::EntityType::kCircularArc);
    renderer.SetDisplayFilter(filter);
    EXPECT_TRUE(Pick(renderer).empty());

    renderer.SetDisplayFilter(i_graph::DisplayFilter{});
    EXPECT_FALSE(Pick(renderer).empty());  // 解除で復帰する
}
