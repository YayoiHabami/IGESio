/**
 * @file tests/graphics/test_pick_registry.cpp
 * @brief PickRegistry (ピック関数のレジストリ) の検証
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 * @note テスト対象:
 *       - `PickRegistry::Register` / `TryRegister` / `Unregister` /
 *         `IsRegistered` / `Find`
 *       - `EntityGraphics`既定実装のディスパッチ順 (レジストリ最優先・
 *         組み込みピックの差し替え・能力ディスパッチへのフォールバック)
 *       - `PickAutoRegistrar` (静的初期化での自動登録)
 *       - MeshEntityの組み込みseed (レイ交差・範囲選択サンプル・レンダラ連携)
 * @note TODO: スレッド安全性は契約外 (登録はピック開始前) のため対象外.
 *       PickEntitiesInRectのレンダラ経路はカメラ設定が支配的なため、本ファイル
 *       では描画クラスのGetSelectionSamplesまでを対象とする (矩形判定本体は
 *       test_picking_reconcile等の既存テストでカバー済み).
 */
#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "mock_open_gl.h"

#include "igesio/entities/mesh_entity.h"
#include "igesio/entities/non_iges_entity_base.h"
#include "igesio/entities/curves/circular_arc.h"
#include "igesio/models/assembly.h"
#include "igesio/models/scene.h"
#include "igesio/numerics/mesh/triangle_mesh.h"
#include "igesio/graphics/core/pick_registry.h"
#include "igesio/graphics/factory.h"
#include "igesio/graphics/meshes/triangle_mesh_graphics.h"
#include "igesio/graphics/renderer.h"

namespace {

namespace i_graph = igesio::graphics;
namespace i_ent = igesio::entities;
namespace i_mod = igesio::models;
namespace i_num = igesio::numerics;
using igesio::Matrix4d;
using igesio::Vector2d;
using igesio::Vector3d;
using i_graph::PickRegistry;
using i_graph::test::MockOpenGL;

/// @brief 数値比較の許容誤差
constexpr double kTol = 1e-9;

/// @brief レジストリ登録テスト用の非IGESエンティティ (形状なしの最小実装)
class PickPinEntity : public i_ent::NonIgesEntityBase {
 public:
    /// @brief コンストラクタ
    PickPinEntity() = default;
};

/// @brief 静的初期化での自動登録テスト用のエンティティ (PickPinEntityとは別型)
class AutoPickPinEntity : public i_ent::NonIgesEntityBase {
 public:
    /// @brief コンストラクタ
    AutoPickPinEntity() = default;
};

/// @brief PickPinEntity用の最小描画クラス (GLリソースを持たない)
/// @note ピック系をオーバーライドしないため、EntityGraphicsの既定実装
///       (PickRegistry照会→能力ディスパッチ) がそのまま検証できる
class PickPinGraphics : public i_graph::EntityGraphics<PickPinEntity> {
 public:
    /// @brief コンストラクタ
    /// @param entity 対象エンティティ
    /// @param gl OpenGLラッパー
    PickPinGraphics(const std::shared_ptr<const PickPinEntity>& entity,
                    const std::shared_ptr<i_graph::IOpenGL>& gl)
            : EntityGraphics<PickPinEntity>(
                  entity, gl, i_graph::ShaderId::kGeneralCurve, false) {}

 protected:
    /// @brief 同期 (GLリソースなしのため何もしない)
    void DoSynchronize() override {}

    /// @brief 描画 (GLリソースなしのため何もしない)
    void DrawImpl(igesio::graphics::gl::Uint,
                  const std::pair<float, float>&) const override {}
};

/// @brief スコープ終了時に指定型の登録を解除するガード
/// @tparam T 解除対象のエンティティ型
template <typename T>
class PickRegistryGuard {
 public:
    /// @brief デストラクタ (登録を解除する)
    ~PickRegistryGuard() { PickRegistry::Unregister<T>(); }
};

/// @brief 静的初期化での自動登録 (テスト本体でIsRegisteredを確認する)
/// @note 公開APIと同じ翻訳単位に置く規約の実例. add_executable直下のTUのため
///       リンカ除去は起きない
const i_graph::PickAutoRegistrar<AutoPickPinEntity> kAutoPickPinRegistrar{
        {[](const AutoPickPinEntity&, const Matrix4d&,
            const i_graph::Ray&, const i_graph::RayIntersectionParams&)
                 -> std::vector<i_graph::RayHit> {
             return {};  // 登録自体の検証用
         },
         nullptr}};

/// @brief z=0平面上の単位直角三角形メッシュを作成する
/// @return 頂点 (0,0,0), (1,0,0), (0,1,0) の1枚の三角形
i_num::TriangleMeshd MakeTriangleMesh() {
    i_num::TriangleMeshd mesh;
    mesh.positions.resize(3, 3);
    mesh.positions.col(0) = Vector3d(0.0, 0.0, 0.0);
    mesh.positions.col(1) = Vector3d(1.0, 0.0, 0.0);
    mesh.positions.col(2) = Vector3d(0.0, 1.0, 0.0);
    mesh.indices = {0, 1, 2};
    return mesh;
}

/// @brief スモーク用の単純な円弧 (中心原点・半径1・z=0平面の1/4円)
std::shared_ptr<i_ent::CircularArc> MakeArc() {
    return i_ent::MakeCircularArc(
        Vector2d(0.0, 0.0), Vector2d(1.0, 0.0), Vector2d(0.0, 1.0), 0.0);
}

/// @brief -z方向の単位レイを作成する
/// @param x レイ始点のx座標
/// @param y レイ始点のy座標
/// @param z レイ始点のz座標
i_graph::Ray MakeRayDown(const double x, const double y, const double z) {
    return i_graph::Ray{Vector3d(x, y, z), Vector3d(0.0, 0.0, -1.0)};
}

/// @brief 平行移動の同次変換行列を作成する
Matrix4d MakeTranslation(const double x, const double y, const double z) {
    Matrix4d m = Matrix4d::Identity();
    m(0, 3) = x;
    m(1, 3) = y;
    m(2, 3) = z;
    return m;
}

}  // namespace



/**
 * 正常系: 登録・ディスパッチ
 */

// 登録したレイ交差関数が既定実装から呼ばれ、world_transformが渡される
TEST(PickRegistryTest, RegisterAndIntersectDispatch) {
    PickRegistryGuard<PickPinEntity> guard;
    PickRegistry::TypedPickFunctions<PickPinEntity> functions;
    functions.intersect =
            [](const PickPinEntity&, const Matrix4d& world_transform,
               const i_graph::Ray&, const i_graph::RayIntersectionParams&)
                    -> std::vector<i_graph::RayHit> {
                // 平行移動成分を交点として返す (変換が渡ることの観測用)
                return {{Vector3d(world_transform(0, 3), world_transform(1, 3),
                                  world_transform(2, 3)), 7.0}};
            };
    PickRegistry::Register<PickPinEntity>(std::move(functions));
    EXPECT_TRUE(PickRegistry::IsRegistered<PickPinEntity>());

    auto gl = std::make_shared<MockOpenGL>();
    auto pin = std::make_shared<PickPinEntity>();
    PickPinGraphics graphics(pin, gl);
    graphics.SetWorldTransform(MakeTranslation(1.0, 2.0, 3.0));

    EXPECT_TRUE(graphics.CanIntersect());
    const auto hits = graphics.Intersect(MakeRayDown(0.0, 0.0, 5.0), {});
    ASSERT_EQ(hits.size(), 1u);
    EXPECT_NEAR(hits[0].distance, 7.0, kTol);
    EXPECT_NEAR(hits[0].position.x(), 1.0, kTol);
    EXPECT_NEAR(hits[0].position.y(), 2.0, kTol);
    EXPECT_NEAR(hits[0].position.z(), 3.0, kTol);
}

// 範囲選択関数のみの登録ではGetSelectionSamplesだけが有効になり、
// レイ交差は能力ディスパッチへフォールバックする (非曲線/曲面のためfalse)
TEST(PickRegistryTest, SelectionOnlyRegistration) {
    PickRegistryGuard<PickPinEntity> guard;
    PickRegistry::TypedPickFunctions<PickPinEntity> functions;
    functions.selection_samples =
            [](const PickPinEntity&, const Matrix4d& world_transform,
               const i_graph::SelectionSampleParams&) {
                i_graph::SelectionSamples samples;
                samples.points.push_back(
                        Vector3d(world_transform(0, 3), 0.0, 0.0));
                return samples;
            };
    PickRegistry::Register<PickPinEntity>(std::move(functions));

    auto gl = std::make_shared<MockOpenGL>();
    auto pin = std::make_shared<PickPinEntity>();
    PickPinGraphics graphics(pin, gl);
    graphics.SetWorldTransform(MakeTranslation(4.0, 0.0, 0.0));

    EXPECT_FALSE(graphics.CanIntersect());
    EXPECT_TRUE(graphics.Intersect(MakeRayDown(0.0, 0.0, 5.0), {}).empty());

    const auto samples = graphics.GetSelectionSamples({});
    ASSERT_EQ(samples.points.size(), 1u);
    EXPECT_NEAR(samples.points[0].x(), 4.0, kTol);
}

// 未登録の非IGES型 (ICurve/ISurfaceのいずれでもない) はピック不可
TEST(PickRegistryTest, UnregisteredTypeHasNoPick) {
    auto gl = std::make_shared<MockOpenGL>();
    auto pin = std::make_shared<PickPinEntity>();
    PickPinGraphics graphics(pin, gl);

    EXPECT_FALSE(graphics.CanIntersect());
    EXPECT_TRUE(graphics.Intersect(MakeRayDown(0.0, 0.0, 5.0), {}).empty());
    const auto samples = graphics.GetSelectionSamples({});
    EXPECT_TRUE(samples.points.empty());
    EXPECT_TRUE(samples.polylines.empty());
    EXPECT_TRUE(samples.segment_vertices.empty());
    EXPECT_TRUE(samples.segments.empty());
}

// 組み込み対応済みの具象型を登録すると既定のピックを差し替えられる
// (レジストリ最優先のディスパッチ順の確認)
TEST(PickRegistryTest, RegisteredIntersectOverridesBuiltin) {
    auto gl = std::make_shared<MockOpenGL>();
    auto arc = MakeArc();
    auto graphics = i_graph::CreateEntityGraphics(arc, gl);
    ASSERT_NE(graphics, nullptr);

    // 円弧上の45°点を-z方向に貫くレイ (解析パスでヒットする)
    const double c = std::sqrt(0.5);
    const auto ray = MakeRayDown(c, c, 5.0);

    // 組み込み (解析パス) では距離はレイ始点から円弧までの約5
    const auto builtin_hits = graphics->Intersect(ray, {});
    ASSERT_FALSE(builtin_hits.empty());
    EXPECT_NEAR(builtin_hits[0].distance, 5.0, 1e-3);

    {
        PickRegistryGuard<i_ent::CircularArc> guard;
        PickRegistry::TypedPickFunctions<i_ent::CircularArc> functions;
        functions.intersect =
                [](const i_ent::CircularArc&, const Matrix4d&,
                   const i_graph::Ray&,
                   const i_graph::RayIntersectionParams&)
                        -> std::vector<i_graph::RayHit> {
                    return {{Vector3d::Zero(), 42.0}};  // 差し替えの観測用
                };
        PickRegistry::Register<i_ent::CircularArc>(std::move(functions));

        const auto hits = graphics->Intersect(ray, {});
        ASSERT_EQ(hits.size(), 1u);
        EXPECT_NEAR(hits[0].distance, 42.0, kTol);
    }

    // 解除後は組み込み (解析パス) へ戻る
    const auto restored_hits = graphics->Intersect(ray, {});
    ASSERT_FALSE(restored_hits.empty());
    EXPECT_NEAR(restored_hits[0].distance, 5.0, 1e-3);
}

// 静的初期化子からの自動登録 (PickAutoRegistrar) が有効になっている
TEST(PickRegistryTest, AutoRegistrarRegistersAtStaticInitialization) {
    EXPECT_TRUE(PickRegistry::IsRegistered<AutoPickPinEntity>());
}



/**
 * 異常系: 重複・空関数
 */

// 重複登録はRegisterでは例外、TryRegisterではfalse (変更なし)
TEST(PickRegistryTest, DuplicateRegistration) {
    PickRegistryGuard<PickPinEntity> guard;
    PickRegistry::TypedPickFunctions<PickPinEntity> functions;
    functions.intersect =
            [](const PickPinEntity&, const Matrix4d&, const i_graph::Ray&,
               const i_graph::RayIntersectionParams&)
                    -> std::vector<i_graph::RayHit> { return {}; };
    PickRegistry::Register<PickPinEntity>(functions);

    EXPECT_THROW(PickRegistry::Register<PickPinEntity>(functions),
                 std::invalid_argument);
    EXPECT_FALSE(PickRegistry::TryRegister<PickPinEntity>(functions));
    EXPECT_TRUE(PickRegistry::IsRegistered<PickPinEntity>());
}

// 両方の関数が空の登録はRegister/TryRegisterとも例外
TEST(PickRegistryTest, EmptyFunctionsThrow) {
    const PickRegistry::TypedPickFunctions<PickPinEntity> empty;
    EXPECT_THROW(PickRegistry::Register<PickPinEntity>(empty),
                 std::invalid_argument);
    EXPECT_THROW(PickRegistry::TryRegister<PickPinEntity>(empty),
                 std::invalid_argument);
    EXPECT_FALSE(PickRegistry::IsRegistered<PickPinEntity>());
}



/**
 * MeshEntityの組み込みseed
 */

// MeshEntityのピック関数は初期化時にseedされている
TEST(MeshEntityPickTest, SeededByDefault) {
    EXPECT_TRUE(PickRegistry::IsRegistered<i_ent::MeshEntity>());
}

// メッシュの三角形をレイ交差でヒットできる (ワールド変換込み)
TEST(MeshEntityPickTest, IntersectHitsTriangle) {
    auto gl = std::make_shared<MockOpenGL>();
    auto entity = std::make_shared<i_ent::MeshEntity>(MakeTriangleMesh());
    i_graph::TriangleMeshGraphics graphics(entity, gl);

    EXPECT_TRUE(graphics.CanIntersect());

    // 恒等変換: 三角形内部 (0.25, 0.25) を貫く
    const auto hits = graphics.Intersect(MakeRayDown(0.25, 0.25, 5.0), {});
    ASSERT_EQ(hits.size(), 1u);
    EXPECT_NEAR(hits[0].distance, 5.0, kTol);
    EXPECT_NEAR(hits[0].position.z(), 0.0, kTol);

    // 三角形外 (0.9, 0.9) はヒットしない
    EXPECT_TRUE(graphics.Intersect(MakeRayDown(0.9, 0.9, 5.0), {}).empty());

    // 平行移動後はワールド座標で判定される
    graphics.SetWorldTransform(MakeTranslation(10.0, 0.0, 0.0));
    EXPECT_TRUE(graphics.Intersect(MakeRayDown(0.25, 0.25, 5.0), {}).empty());
    const auto moved_hits =
            graphics.Intersect(MakeRayDown(10.25, 0.25, 5.0), {});
    ASSERT_EQ(moved_hits.size(), 1u);
    EXPECT_NEAR(moved_hits[0].distance, 5.0, kTol);
    EXPECT_NEAR(moved_hits[0].position.x(), 10.25, kTol);
}

// 範囲選択サンプルは全頂点 (共有プール) と全エッジ (インデックス線分) を返す
TEST(MeshEntityPickTest, SelectionSamplesContainVerticesAndEdges) {
    auto gl = std::make_shared<MockOpenGL>();
    auto entity = std::make_shared<i_ent::MeshEntity>(MakeTriangleMesh());
    i_graph::TriangleMeshGraphics graphics(entity, gl);
    graphics.SetWorldTransform(MakeTranslation(10.0, 0.0, 0.0));

    const auto samples = graphics.GetSelectionSamples({});
    ASSERT_EQ(samples.segment_vertices.size(), 3u);  // 三角形の3頂点
    ASSERT_EQ(samples.segments.size(), 3u);          // 三角形の3エッジ
    for (const auto& segment : samples.segments) {
        EXPECT_LT(segment[0], samples.segment_vertices.size());
        EXPECT_LT(segment[1], samples.segment_vertices.size());
    }
    // 旧来のチャンネルは使用しない (頂点重複の射影を避けるため)
    EXPECT_TRUE(samples.points.empty());
    EXPECT_TRUE(samples.polylines.empty());
    // ワールド変換が適用されている (頂点0 = (0,0,0) + 平行移動)
    EXPECT_NEAR(samples.segment_vertices[0].x(), 10.0, kTol);
    EXPECT_NEAR(samples.segment_vertices[0].y(), 0.0, kTol);
}

// レンダラのPickEntitiesでMeshEntityがヒットする (エンドツーエンド)
TEST(MeshEntityPickTest, RendererPickEntitiesHitsMesh) {
    auto gl = std::make_shared<MockOpenGL>();
    i_graph::EntityRenderer renderer(gl);

    auto root = i_mod::MakeAssembly();
    auto entity = std::make_shared<i_ent::MeshEntity>(MakeTriangleMesh());
    root->AddEntity(entity);
    i_mod::Scene scene(root);
    renderer.SetScene(&scene);

    // スクリーン座標はヒット許容量のピクセル換算にのみ使われるため画面中央を渡す
    const auto hits =
            renderer.PickEntities(MakeRayDown(0.25, 0.25, 5.0), 640.0, 360.0);
    ASSERT_FALSE(hits.empty());
    EXPECT_EQ(hits.front().id, entity->GetID());
}
