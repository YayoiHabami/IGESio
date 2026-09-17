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
 *       - DisplayFilterで除外された型・アセンブリはヒットしない
 *       - 表示座標系 (SetViewFrame) を設定すると、レイと交点が表示座標系の値で
 *         整合する (ワールド座標のレイは外れ、表示座標系のレイが当たる)
 *       - 子アセンブリの大域変換 (並進・回転) が曲線/曲面のピックに反映される
 *         (交点・距離はワールド空間の値で返る)
 *       - Point (116) 自身の変換行列 (M_entity) がピック位置に反映される
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
#include "igesio/entities/curves/line.h"
#include "igesio/entities/curves/point.h"
#include "igesio/entities/surfaces/tabulated_cylinder.h"
#include "igesio/entities/transformations/transformation_matrix.h"
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

/// @brief 交点座標・距離の比較許容誤差
constexpr double kTol = 1e-6;

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

/// @brief 指定の始点から-Z方向に貫くレイでピックを実行する
/// @param renderer 対象のレンダラ
/// @param origin レイの始点 (ワールド空間)
std::vector<i_graph::EntityHit> PickAt(i_graph::EntityRenderer& renderer,
                                       const Vector3d& origin) {
    return renderer.PickEntities(
            i_graph::Ray{origin, Vector3d(0.0, 0.0, -1.0)}, 640.0, 360.0);
}

/// @brief 平行移動の同次変換を作る
igesio::Matrix4d Translate(double x, double y, double z) {
    igesio::Matrix4d m = igesio::Matrix4d::Identity();
    m(0, 3) = x; m(1, 3) = y; m(2, 3) = z;
    return m;
}

/// @brief Z軸まわりに+90°回転する同次変換を作る ((x, y) → (-y, x))
igesio::Matrix4d RotateZ90() {
    igesio::Matrix4d m = igesio::Matrix4d::Identity();
    m(0, 0) = 0.0; m(0, 1) = -1.0;
    m(1, 0) = 1.0; m(1, 1) = 0.0;
    return m;
}

/// @brief 唯一のヒットが指定エンティティ・指定座標・指定距離であることを検証する
/// @param hits ピック結果
/// @param id 期待するエンティティのID
/// @param position 期待する交点 (ワールド空間)
/// @param distance 期待するレイ始点からの距離
void ExpectSingleHit(const std::vector<i_graph::EntityHit>& hits,
                     const igesio::ObjectID& id,
                     const Vector3d& position, const double distance) {
    ASSERT_FALSE(hits.empty());
    EXPECT_EQ(hits.front().id, id);
    EXPECT_NEAR(hits.front().hit.position.x(), position.x(), kTol);
    EXPECT_NEAR(hits.front().hit.position.y(), position.y(), kTol);
    EXPECT_NEAR(hits.front().hit.position.z(), position.z(), kTol);
    EXPECT_NEAR(hits.front().hit.distance, distance, kTol);
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

// DisplayFilterで隠したアセンブリ (部分木) のエンティティはヒットしない
TEST(PickingReconcileTest, Pick_HiddenAssemblyNotHit) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);

    auto root = i_mod::MakeAssembly();
    auto child = i_mod::MakeAssembly();
    root->AddChildAssembly(child);
    child->AddEntity(MakeArc());
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    ASSERT_FALSE(Pick(renderer).empty());  // フィルタ無しではヒットする

    i_graph::DisplayFilter filter;
    filter.hidden_assemblies.insert(child->GetID());
    renderer.SetDisplayFilter(filter);
    EXPECT_TRUE(Pick(renderer).empty());

    renderer.SetDisplayFilter(i_graph::DisplayFilter{});
    EXPECT_FALSE(Pick(renderer).empty());  // 解除で復帰する
}

// 表示座標系を設定すると、ピックは表示座標系の値で整合する
// (物体は frame⁻¹ の位置に見え、レイ・交点も表示座標系で与える/返る)
TEST(PickingReconcileTest, Pick_ViewFrameTransformsHits) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);

    auto root = i_mod::MakeAssembly();
    auto arc = MakeArc();
    root->AddEntity(arc);
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    // 表示座標系を(2,0,0)へ置く: 原点の円弧は表示座標系では(-2,0,0)中心に見える
    igesio::Matrix4d frame = igesio::Matrix4d::Identity();
    frame(0, 3) = 2.0;
    renderer.SetViewFrame(frame);

    // ワールド座標のままのレイは外れる
    EXPECT_TRUE(Pick(renderer).empty());

    // 表示座標系で-2だけずらしたレイは当たり、交点も表示座標系の値になる
    const double c = std::sqrt(0.5);
    const i_graph::Ray shifted{Vector3d(c - 2.0, c, 5.0), Vector3d(0.0, 0.0, -1.0)};
    const auto hits = renderer.PickEntities(shifted, 640.0, 360.0);
    ExpectSingleHit(hits, arc->GetID(), Vector3d(c - 2.0, c, 0.0), 5.0);

    // 単位行列へ戻すと元のレイで当たる
    renderer.SetViewFrame(igesio::Matrix4d::Identity());
    EXPECT_FALSE(Pick(renderer).empty());
}

// 子アセンブリの並進がピックに反映される (元位置のレイは外れ、移動先のレイが
// 当たり、交点・距離はワールド空間の値で返る)
TEST(PickingReconcileTest, Pick_ChildAssemblyTranslationAppliesToHits) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);

    auto root = i_mod::MakeAssembly();
    auto child = i_mod::MakeAssembly();
    child->SetGlobalTransform(Translate(3.0, 0.0, 0.0));
    root->AddChildAssembly(child);
    auto arc = MakeArc();
    child->AddEntity(arc);
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    // 定義位置 (原点中心) を貫くレイは外れる
    EXPECT_TRUE(Pick(renderer).empty());

    // 移動先の45°点を貫くレイが当たる
    const double c = std::sqrt(0.5);
    ExpectSingleHit(PickAt(renderer, Vector3d(c + 3.0, c, 5.0)),
                    arc->GetID(), Vector3d(c + 3.0, c, 0.0), 5.0);
}

// 子アセンブリの回転がピックに反映される (回転成分の逆変換を検証)
TEST(PickingReconcileTest, Pick_ChildAssemblyRotationAppliesToHits) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);

    auto root = i_mod::MakeAssembly();
    auto child = i_mod::MakeAssembly();
    child->SetGlobalTransform(RotateZ90());
    root->AddChildAssembly(child);
    auto arc = MakeArc();
    child->AddEntity(arc);
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    // 回転後の円弧は第2象限 (x≦0, y≧0) にあるため、元の45°点のレイは外れる
    EXPECT_TRUE(Pick(renderer).empty());

    // 45°点 (c, c, 0) は回転で (-c, c, 0) に移る
    const double c = std::sqrt(0.5);
    ExpectSingleHit(PickAt(renderer, Vector3d(-c, c, 5.0)),
                    arc->GetID(), Vector3d(-c, c, 0.0), 5.0);
}

// 曲面 (ISurface経路) でも子アセンブリの変換がピックに反映される
TEST(PickingReconcileTest, Pick_SurfaceInTransformedChild) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);

    auto root = i_mod::MakeAssembly();
    auto child = i_mod::MakeAssembly();
    child->SetGlobalTransform(Translate(3.0, 0.0, 0.0));
    root->AddChildAssembly(child);
    // z=0平面上の単位正方形 [0,1]×[0,1] (準線は物理従属のため単独ピックされない)
    auto surface = i_ent::MakeExtrudedSurface(
            i_ent::MakeLine(Vector3d(0.0, 0.0, 0.0), Vector3d(1.0, 0.0, 0.0)),
            Vector3d(0.0, 1.0, 0.0));
    child->AddEntity(surface);
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    // 定義位置の中心を貫くレイは外れる
    EXPECT_TRUE(PickAt(renderer, Vector3d(0.5, 0.5, 5.0)).empty());

    // 移動先の中心を貫くレイが当たる
    ExpectSingleHit(PickAt(renderer, Vector3d(3.5, 0.5, 5.0)),
                    surface->GetID(), Vector3d(3.5, 0.5, 0.0), 5.0);
}

// Point (116) 自身の変換行列 (M_entity) がピック位置に反映される
TEST(PickingReconcileTest, Pick_PointAppliesEntityTransformationMatrix) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);

    auto root = i_mod::MakeAssembly();
    auto point = i_ent::MakePoint(Vector3d(0.0, 0.0, 0.0));
    // DEフィールドは非所有参照 (weak_ptr) のため、変換行列はテスト側で保持する
    const auto m_entity = i_ent::MakeTranslation(Vector3d(5.0, 0.0, 0.0));
    ASSERT_TRUE(point->OverwriteTransformationMatrix(m_entity));
    root->AddEntity(point);
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    // 定義位置 (原点) を貫くレイは外れる
    EXPECT_TRUE(PickAt(renderer, Vector3d(0.0, 0.0, 5.0)).empty());

    // M_entity適用後の位置を貫くレイが当たる
    ExpectSingleHit(PickAt(renderer, Vector3d(5.0, 0.0, 5.0)),
                    point->GetID(), Vector3d(5.0, 0.0, 0.0), 5.0);
}
