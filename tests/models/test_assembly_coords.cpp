/**
 * @file tests/models/test_assembly_coords.cpp
 * @brief models/assembly.hの座標系・ビュー・空間検索のテスト (P5)
 * @author Yayoi Habami
 * @date 2026-05-29
 * @copyright 2026 Yayoi Habami
 *
 * テスト対象:
 *   - CoordFrame: Definition / EntityLocal / World / RelativeTo / GetKind /
 *                 GetRelativeBase
 *   - Assembly::ResolvePlacement
 *   - Assembly::GetCurveView / GetSurfaceView
 *   - Assembly::GetWorldBoundingBox
 *
 * 設計方針:
 *   - 配置合成 (G_kの積) の検証が核心であり、エンティティのM_entity値には依存しない。
 *     そのため曲線・ResolvePlacement・WorldBBは閉形式で予測できる合成Line/Pointを用いる。
 *   - GetSurfaceViewのみ実ISurfaceが必要なため、single_rounded_cube.igesのTrimmedSurface
 *     (144) を遅延ロードして1つ借用する (AddEntityは所属排他を実行時強制しないため流用可)。
 *   - ツリーはAddChildAssemblyがweak_from_thisを使うため、必ずshared_ptrで組む。
 *
 * TODO: CollectWorldVertices内の `zero_axes>=2` (AABB構築後の防御分岐) は実2Dメンバ単独では
 *       誘発困難なため、nullopt経路は「全メンバ退化 (pts空)」で代表検証する。
 * TODO: スケールを含む変換 (§11末尾) は当面対象外 (124相当の回転+並進のみ)。
 */
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "igesio/reader.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/numerics/geometric/bounding_box.h"
#include "igesio/numerics/core/tolerance.h"
#include "igesio/entities/entity_base.h"
#include "igesio/entities/curves/line.h"
#include "igesio/entities/curves/point.h"
#include "igesio/entities/interfaces/i_curve.h"
#include "igesio/entities/interfaces/i_surface.h"
#include "igesio/entities/views/curve_view.h"
#include "igesio/entities/views/surface_view.h"
#include "igesio/models/assembly.h"
#include "igesio/models/iges_data.h"

namespace {

namespace fs = std::filesystem;
namespace i_ent = igesio::entities;
namespace i_mdl = igesio::models;
namespace i_num = igesio::numerics;
using igesio::ObjectID;
using igesio::Matrix4d;
using igesio::Vector3d;
using i_mdl::Assembly;
using i_mdl::CoordFrame;
using i_ent::Line;
using i_ent::Point;

/// @brief 浮動小数点比較の許容誤差
constexpr double kTol = 1e-9;
/// @brief π/2 (回転角に使用)
constexpr double kPiHalf = 1.57079632679489661923;
/// @brief 単位行列 (IsApproxEqualへ式テンプレートではなく具体型で渡すための定数)
const igesio::Matrix4d kIdentity4 = igesio::Matrix4d::Identity();

/// @brief テスト用IGESファイル (一辺が丸められた立方体; 124/142/144を含む)
const std::string kCubePath =
        fs::path(__FILE__).parent_path().parent_path()
            .append("test_data").append("single_rounded_cube.iges").string();

/// @brief 並進のみの同次変換行列を生成する
igesio::Matrix4d MakeTranslation(const Vector3d& t) {
    igesio::Matrix4d m = igesio::Matrix4d::Identity();
    m.topRightCorner<3, 1>() = t;
    return m;
}

/// @brief 回転 R(angle, axis) と並進 t からなる同次変換行列を生成する
/// @param angle 回転角度 [rad] (0なら回転は単位行列)
/// @param axis 回転軸 (正規化前で可。ゼロベクトル不可)
/// @param t 並進ベクトル
igesio::Matrix4d MakeTransform(const double angle, const Vector3d& axis,
                               const Vector3d& t) {
    igesio::Matrix4d m = igesio::Matrix4d::Identity();
    if (angle != 0.0) {
        m.topLeftCorner<3, 3>() =
            igesio::AngleAxisd(angle, axis.normalized()).toRotationMatrix();
    }
    m.topRightCorner<3, 1>() = t;
    return m;
}

/// @brief 始点・終点を持つ線分エンティティ (M_entityは単位) を生成する
std::shared_ptr<Line> MakeLine(const Vector3d& start, const Vector3d& end) {
    return std::make_shared<Line>(start, end, i_ent::LineType::kSegment);
}

/// @brief root→a→b の3段ツリーと、bへ追加した線分を束ねる
struct Chain {
    std::shared_ptr<Assembly> root;
    std::shared_ptr<Assembly> a;
    std::shared_ptr<Assembly> b;
    std::shared_ptr<Line> line;
    ObjectID line_id;
};

/// @brief 既知の大域変換を持つ root→a→b の3段ツリーを構築し、bへ線分を追加する
/// @param g_root rootの大域変換
/// @param g_a aの大域変換
/// @param g_b bの大域変換
Chain BuildChain(const Matrix4d& g_root, const Matrix4d& g_a,
                 const Matrix4d& g_b) {
    Chain c;
    c.root = std::make_shared<Assembly>();
    c.a = std::make_shared<Assembly>();
    c.b = std::make_shared<Assembly>();
    c.root->SetGlobalTransform(g_root);
    c.a->SetGlobalTransform(g_a);
    c.b->SetGlobalTransform(g_b);
    c.root->AddChildAssembly(c.a);
    c.a->AddChildAssembly(c.b);
    c.line = MakeLine(Vector3d(0.0, 0.0, 0.0), Vector3d(1.0, 0.0, 0.0));
    c.line_id = c.b->AddEntity(c.line);
    return c;
}

/// @brief バウンディングボックスが軸平行AABB [lo, hi] と一致することを確認する
void ExpectBoxEquals(const i_num::BoundingBox& bb, const Vector3d& lo,
                     const Vector3d& hi) {
    const Vector3d expected_sizes = hi - lo;
    EXPECT_TRUE(i_num::IsApproxEqual(bb.GetControl(), lo));
    const auto s = bb.GetSizes();
    EXPECT_NEAR(s[0], expected_sizes.x(), kTol);
    EXPECT_NEAR(s[1], expected_sizes.y(), kTol);
    EXPECT_NEAR(s[2], expected_sizes.z(), kTol);
}

}  // namespace



/// @brief 実ISurface供給のためにcubeを共有するフィクスチャ
class AssemblyCoordsTest : public ::testing::Test {
 protected:
    /// @brief 一度だけロードするIGESデータ
    inline static std::unique_ptr<i_mdl::IgesData> data_;

    static void SetUpTestSuite() {
        if (!data_) {
            data_ = std::make_unique<i_mdl::IgesData>(igesio::ReadIges(kCubePath));
        }
    }

    /// @brief ロード済みエンティティから最初のTrimmedSurface(144)を返す
    static std::shared_ptr<i_ent::EntityBase> LoadedSurface() {
        auto surfaces = data_->Root().FindEntitiesByType(
                i_ent::EntityType::kTrimmedSurface);
        return surfaces.empty() ? nullptr : surfaces.front();
    }

    /// @brief パラメータ中央で点が得られる、トリムなしのISurfaceを返す
    /// @note TrimmedSurfaceはトリム領域外で点を返さない (IsInDomainで棄却) ため、
    ///       配置合成の検算には下位の素のサーフェス (128等) を用いる。
    static std::shared_ptr<i_ent::EntityBase> WorkingSurface() {
        for (const auto& [id, e] : data_->Root().GetEntities()) {
            auto surf = std::dynamic_pointer_cast<const i_ent::ISurface>(e);
            if (!surf) continue;
            const auto r = surf->GetParameterRange();
            const double u = 0.5 * (r[0] + r[1]);
            const double v = 0.5 * (r[2] + r[3]);
            if (surf->TryGetPointAt(u, v).has_value()) return e;
        }
        return nullptr;
    }

    /// @brief ロード済みエンティティから、物理従属かつ幾何を持つメンバを返す
    static std::shared_ptr<i_ent::EntityBase> LoadedDependentGeometry() {
        auto found = data_->Root().FindEntities(
                [](const i_ent::EntityBase& e) {
                    return e.GetSubordinateEntitySwitch()
                            == i_ent::SubordinateEntitySwitch::kPhysicallyDependent;
                });
        for (const auto& e : found) {
            if (std::dynamic_pointer_cast<const i_ent::IGeometry>(e)) return e;
        }
        return nullptr;
    }
};



/**
 * CoordFrame (値オブジェクト)
 */

// 各静的生成関数がKindを正しく設定し、RelativeToは基準IDを保持する
TEST_F(AssemblyCoordsTest, CoordFrame_FactoriesSetKind) {
    EXPECT_EQ(CoordFrame::Definition().GetKind(),
              CoordFrame::Kind::kDefinition);
    EXPECT_EQ(CoordFrame::EntityLocal().GetKind(),
              CoordFrame::Kind::kEntityLocal);
    EXPECT_EQ(CoordFrame::World().GetKind(), CoordFrame::Kind::kWorld);

    const ObjectID base = igesio::IDGenerator::Generate(
            igesio::ObjectType::kAssembly);
    const auto rel = CoordFrame::RelativeTo(base);
    EXPECT_EQ(rel.GetKind(), CoordFrame::Kind::kRelative);
    EXPECT_EQ(rel.GetRelativeBase(), base);
    igesio::IDGenerator::Release(base.ToInt());
}



/**
 * ResolvePlacement
 */

// kDefinitionは配置概念がないためnullopt
TEST_F(AssemblyCoordsTest, ResolvePlacement_DefinitionReturnsNullopt) {
    auto c = BuildChain(MakeTranslation({1, 2, 3}),
                        MakeTranslation({4, 0, 0}),
                        MakeTranslation({0, 5, 0}));
    EXPECT_FALSE(
        c.root->ResolvePlacement(c.line_id, CoordFrame::Definition()).has_value());
}

// kEntityLocalは単位行列 (M_entityのみ・Assembly非依存)
TEST_F(AssemblyCoordsTest, ResolvePlacement_EntityLocalReturnsIdentity) {
    auto c = BuildChain(MakeTranslation({1, 2, 3}),
                        MakeTranslation({4, 0, 0}),
                        MakeTranslation({0, 5, 0}));
    const auto pl = c.root->ResolvePlacement(c.line_id, CoordFrame::EntityLocal());
    ASSERT_TRUE(pl.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(*pl, kIdentity4));
}

// kWorldはルートから所有ノードまでの大域変換の積 G_root·G_a·G_b
TEST_F(AssemblyCoordsTest, ResolvePlacement_WorldComposesRootToOwner) {
    const Matrix4d g_root = MakeTranslation({1, 2, 3});
    const Matrix4d g_a = MakeTransform(kPiHalf, {0, 0, 1}, {4, 0, 0});
    const Matrix4d g_b = MakeTranslation({0, 5, 0});
    auto c = BuildChain(g_root, g_a, g_b);

    const auto pl = c.root->ResolvePlacement(c.line_id, CoordFrame::World());
    ASSERT_TRUE(pl.has_value());
    const Matrix4d expected = g_root * g_a * g_b;
    EXPECT_TRUE(i_num::IsApproxEqual(*pl, expected));
}

// RelativeTo(直近の親a): 基準aのG_aは含めず、owner(b)のG_bのみ
TEST_F(AssemblyCoordsTest,
       ResolvePlacement_RelativeToImmediateParentExcludesBaseG) {
    const Matrix4d g_root = MakeTranslation({1, 2, 3});
    const Matrix4d g_a = MakeTransform(kPiHalf, {0, 0, 1}, {4, 0, 0});
    const Matrix4d g_b = MakeTranslation({0, 5, 0});
    auto c = BuildChain(g_root, g_a, g_b);

    const auto pl =
        c.root->ResolvePlacement(c.line_id, CoordFrame::RelativeTo(c.a->GetID()));
    ASSERT_TRUE(pl.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(*pl, g_b));
}

// RelativeTo(root): rootのG_rootは含めず、G_a·G_b
TEST_F(AssemblyCoordsTest, ResolvePlacement_RelativeToRootExcludesRootG) {
    const Matrix4d g_root = MakeTranslation({1, 2, 3});
    const Matrix4d g_a = MakeTransform(kPiHalf, {0, 0, 1}, {4, 0, 0});
    const Matrix4d g_b = MakeTranslation({0, 5, 0});
    auto c = BuildChain(g_root, g_a, g_b);

    const auto pl = c.root->ResolvePlacement(
            c.line_id, CoordFrame::RelativeTo(c.root->GetID()));
    ASSERT_TRUE(pl.has_value());
    const Matrix4d expected = g_a * g_b;
    EXPECT_TRUE(i_num::IsApproxEqual(*pl, expected));
}

// RelativeTo(自身の所有ノードb): 単位行列 (G_bも含めず即終了)
TEST_F(AssemblyCoordsTest, ResolvePlacement_RelativeToSelfOwnerReturnsIdentity) {
    auto c = BuildChain(MakeTranslation({1, 2, 3}),
                        MakeTranslation({4, 0, 0}),
                        MakeTranslation({0, 5, 0}));
    const auto pl =
        c.root->ResolvePlacement(c.line_id, CoordFrame::RelativeTo(c.b->GetID()));
    ASSERT_TRUE(pl.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(*pl, kIdentity4));
}

// 呼び出すノード (root / 所有ノードb) によらず結果は同一 (逆引きインデックス経由)
TEST_F(AssemblyCoordsTest, ResolvePlacement_SameResultRegardlessOfCallingNode) {
    const Matrix4d g_root = MakeTranslation({1, 2, 3});
    const Matrix4d g_a = MakeTransform(kPiHalf, {0, 0, 1}, {4, 0, 0});
    const Matrix4d g_b = MakeTranslation({0, 5, 0});
    auto c = BuildChain(g_root, g_a, g_b);

    const auto from_root =
        c.root->ResolvePlacement(c.line_id, CoordFrame::World());
    const auto from_owner =
        c.b->ResolvePlacement(c.line_id, CoordFrame::World());
    ASSERT_TRUE(from_root.has_value());
    ASSERT_TRUE(from_owner.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(*from_root, *from_owner));
}

// 未登録IDはnullopt
TEST_F(AssemblyCoordsTest, ResolvePlacement_UnknownIDReturnsNullopt) {
    auto c = BuildChain(Matrix4d::Identity(), Matrix4d::Identity(),
                        Matrix4d::Identity());
    auto orphan = MakeLine({0, 0, 0}, {1, 1, 1});  // ツリー外
    EXPECT_FALSE(
        c.root->ResolvePlacement(orphan->GetID(), CoordFrame::World()).has_value());
}

// RelativeToの基準が所有ノードの祖先でない場合は例外
TEST_F(AssemblyCoordsTest, ResolvePlacement_ThrowsWhenRelativeBaseNotAncestor) {
    auto c = BuildChain(Matrix4d::Identity(), Matrix4d::Identity(),
                        Matrix4d::Identity());
    // ツリーに属さない別Assemblyを基準に指定する
    auto outsider = std::make_shared<Assembly>();
    EXPECT_THROW(
        c.root->ResolvePlacement(c.line_id,
                                 CoordFrame::RelativeTo(outsider->GetID())),
        std::invalid_argument);
}



/**
 * GetCurveView
 */

// ビューのサンプル点がResolvePlacementおよび手計算と一致する (配線整合)
TEST_F(AssemblyCoordsTest, GetCurveView_PointMatchesResolvedPlacement) {
    const Matrix4d g_root = MakeTranslation({1, 2, 3});
    const Matrix4d g_a = MakeTransform(kPiHalf, {0, 0, 1}, {4, 0, 0});
    const Matrix4d g_b = MakeTranslation({0, 5, 0});
    auto c = BuildChain(g_root, g_a, g_b);

    const auto pl = c.root->ResolvePlacement(c.line_id, CoordFrame::World());
    ASSERT_TRUE(pl.has_value());
    auto view = c.root->GetCurveView(c.line_id, CoordFrame::World());
    ASSERT_NE(view, nullptr);

    const double t = 0.5;
    const auto via_view = view->TryGetPointAt(t);
    const auto via_entity = c.line->TryGetPointAt(t, *pl);
    const auto p_def = c.line->TryGetPointAt(t);  // M_entity単位なので定義空間点
    ASSERT_TRUE(via_view.has_value());
    ASSERT_TRUE(via_entity.has_value());
    ASSERT_TRUE(p_def.has_value());

    const Vector3d via_manual = i_num::ApplyTransform(*pl, *p_def, true);
    EXPECT_TRUE(i_num::IsApproxEqual(*via_view, *via_entity));
    EXPECT_TRUE(i_num::IsApproxEqual(*via_view, via_manual));
}

// 接線は回転のみ適用され並進は乗らない (点・ベクトルの区別)
TEST_F(AssemblyCoordsTest,
       GetCurveView_DerivativeAppliesRotationToVectorNotTranslation) {
    auto root = std::make_shared<Assembly>();
    const Matrix4d p = MakeTransform(kPiHalf, {0, 0, 1}, {10, 20, 30});
    root->SetGlobalTransform(p);
    auto line = MakeLine({0, 0, 0}, {1, 0, 0});
    const auto id = root->AddEntity(line);

    auto view = root->GetCurveView(id, CoordFrame::World());
    ASSERT_NE(view, nullptr);

    const double t = 0.5;
    const auto tang_view = view->TryGetTangentAt(t);
    const auto tang_def = line->TryGetTangentAt(t);  // 定義空間の単位接線
    ASSERT_TRUE(tang_view.has_value());
    ASSERT_TRUE(tang_def.has_value());

    // 並進を含まないApplyTransform(is_point=false)と一致 = 並進が漏れていない
    const Vector3d expected = i_num::ApplyTransform(p, *tang_def, false);
    EXPECT_TRUE(i_num::IsApproxEqual(*tang_view, expected));
}

// 未登録IDはnullptr
TEST_F(AssemblyCoordsTest, GetCurveView_UnknownIDReturnsNull) {
    auto root = std::make_shared<Assembly>();
    auto orphan = MakeLine({0, 0, 0}, {1, 1, 1});
    EXPECT_EQ(root->GetCurveView(orphan->GetID(), CoordFrame::World()), nullptr);
}

// 曲面IDをGetCurveViewへ渡すとnullptr (型不一致)
TEST_F(AssemblyCoordsTest, GetCurveView_SurfaceIDReturnsNull) {
    auto surface = LoadedSurface();
    ASSERT_NE(surface, nullptr);
    auto root = std::make_shared<Assembly>();
    const auto id = root->AddEntity(surface);
    EXPECT_EQ(root->GetCurveView(id, CoordFrame::World()), nullptr);
}

// kDefinitionでのビュー生成は例外 (ビューはM_entity適用が前提)
TEST_F(AssemblyCoordsTest, GetCurveView_ThrowsWhenDefinitionFrame) {
    auto root = std::make_shared<Assembly>();
    auto line = MakeLine({0, 0, 0}, {1, 1, 1});
    const auto id = root->AddEntity(line);
    EXPECT_THROW(root->GetCurveView(id, CoordFrame::Definition()),
                 std::invalid_argument);
}

// RelativeToの基準が祖先でない場合は例外
TEST_F(AssemblyCoordsTest, GetCurveView_ThrowsWhenRelativeBaseNotAncestor) {
    auto c = BuildChain(Matrix4d::Identity(), Matrix4d::Identity(),
                        Matrix4d::Identity());
    auto outsider = std::make_shared<Assembly>();
    EXPECT_THROW(
        c.root->GetCurveView(c.line_id,
                             CoordFrame::RelativeTo(outsider->GetID())),
        std::invalid_argument);
}



/**
 * GetSurfaceView
 */

// 曲面に対しビューを生成し、型・SourceIDが元へ委譲される
TEST_F(AssemblyCoordsTest, GetSurfaceView_ReturnsViewForSurface) {
    auto surface = LoadedSurface();
    ASSERT_NE(surface, nullptr);
    auto root = std::make_shared<Assembly>();
    const auto id = root->AddEntity(surface);

    auto view = root->GetSurfaceView(id, CoordFrame::World());
    ASSERT_NE(view, nullptr);
    EXPECT_EQ(view->GetType(), i_ent::EntityType::kTrimmedSurface);
    EXPECT_EQ(view->GetSourceID(), surface->GetID());
}

// 曲面ビューのサンプル点がResolvePlacementと一致する (配線整合)
// NOTE: TrimmedSurfaceはトリム領域外で点を返さずデータ依存になるため、配置合成の検算には
//       中央で点が得られる素のISurface (WorkingSurface) を用いる。型ディスパッチの検証は
//       GetSurfaceView_ReturnsViewForSurface (144) が担う。
TEST_F(AssemblyCoordsTest, GetSurfaceView_PointMatchesResolvedPlacement) {
    auto surface = WorkingSurface();
    ASSERT_NE(surface, nullptr) << "中央で点が得られるISurfaceが見つからない";
    auto surf = std::dynamic_pointer_cast<const i_ent::ISurface>(surface);
    ASSERT_NE(surf, nullptr);

    auto root = std::make_shared<Assembly>();
    root->SetGlobalTransform(MakeTransform(kPiHalf, {0, 0, 1}, {1, 2, 3}));
    const auto id = root->AddEntity(surface);

    const auto pl = root->ResolvePlacement(id, CoordFrame::World());
    ASSERT_TRUE(pl.has_value());
    auto view = root->GetSurfaceView(id, CoordFrame::World());
    ASSERT_NE(view, nullptr);

    // パラメータ中央 (WorkingSurfaceが点を返すことを保証済み) で配置適用を検算する
    const auto range = surf->GetParameterRange();
    const double u = 0.5 * (range[0] + range[1]);
    const double v = 0.5 * (range[2] + range[3]);
    const auto local = surf->TryGetPointAt(u, v);  // M_entity適用済み
    ASSERT_TRUE(local.has_value());
    const auto via_view = view->TryGetPointAt(u, v);
    ASSERT_TRUE(via_view.has_value());
    const Vector3d expected = i_num::ApplyTransform(*pl, *local, true);
    EXPECT_TRUE(i_num::IsApproxEqual(*via_view, expected));
}

// 曲線IDをGetSurfaceViewへ渡すとnullptr (型不一致)
TEST_F(AssemblyCoordsTest, GetSurfaceView_CurveIDReturnsNull) {
    auto root = std::make_shared<Assembly>();
    auto line = MakeLine({0, 0, 0}, {1, 1, 1});
    const auto id = root->AddEntity(line);
    EXPECT_EQ(root->GetSurfaceView(id, CoordFrame::World()), nullptr);
}

// 未登録IDはnullptr
TEST_F(AssemblyCoordsTest, GetSurfaceView_UnknownIDReturnsNull) {
    auto root = std::make_shared<Assembly>();
    const ObjectID unknown = igesio::IDGenerator::Generate(
            igesio::ObjectType::kAssembly);
    EXPECT_EQ(root->GetSurfaceView(unknown, CoordFrame::World()), nullptr);
    igesio::IDGenerator::Release(unknown.ToInt());
}

// kDefinitionでのビュー生成は例外
TEST_F(AssemblyCoordsTest, GetSurfaceView_ThrowsWhenDefinitionFrame) {
    auto surface = LoadedSurface();
    ASSERT_NE(surface, nullptr);
    auto root = std::make_shared<Assembly>();
    const auto id = root->AddEntity(surface);
    EXPECT_THROW(root->GetSurfaceView(id, CoordFrame::Definition()),
                 std::invalid_argument);
}



/**
 * GetWorldBoundingBox
 */

// メンバなしのAssemblyはnullopt
TEST_F(AssemblyCoordsTest, GetWorldBoundingBox_EmptyAssemblyReturnsNullopt) {
    Assembly empty;
    EXPECT_FALSE(empty.GetWorldBoundingBox().has_value());
}

// 3D線分1本のみ: その世界AABBと一致する
TEST_F(AssemblyCoordsTest,
       GetWorldBoundingBox_SingleSpatialMemberMatchesMemberBox) {
    auto root = std::make_shared<Assembly>();
    root->AddEntity(MakeLine({0, 0, 0}, {1, 2, 3}));

    const auto bb = root->GetWorldBoundingBox();
    ASSERT_TRUE(bb.has_value());
    ExpectBoxEquals(*bb, Vector3d(0, 0, 0), Vector3d(1, 2, 3));
}

// 子Assemblyの大域変換が世界AABBへ反映される
TEST_F(AssemblyCoordsTest,
       GetWorldBoundingBox_NestedChildAppliesGlobalTransform) {
    auto root = std::make_shared<Assembly>();
    auto child = std::make_shared<Assembly>();
    child->SetGlobalTransform(MakeTranslation({10, 0, 0}));
    child->AddEntity(MakeLine({0, 0, 0}, {1, 1, 1}));
    root->AddChildAssembly(child);

    const auto bb = root->GetWorldBoundingBox();
    ASSERT_TRUE(bb.has_value());
    ExpectBoxEquals(*bb, Vector3d(10, 0, 0), Vector3d(11, 1, 1));
}

// 平面状 (1軸退化) のメンバでも扁平なboxを返す (nulloptにしない)
TEST_F(AssemblyCoordsTest, GetWorldBoundingBox_PlanarMemberReturnsFlatBox) {
    auto root = std::make_shared<Assembly>();
    root->AddEntity(MakeLine({0, 0, 0}, {1, 1, 0}));  // z一定の対角線 (2次元)

    const auto bb = root->GetWorldBoundingBox();
    ASSERT_TRUE(bb.has_value());
    ExpectBoxEquals(*bb, Vector3d(0, 0, 0), Vector3d(1, 1, 0));
}

// 0D点・1D軸平行線のみ (全て退化) はnullopt
TEST_F(AssemblyCoordsTest,
       GetWorldBoundingBox_AllDegenerateMembersReturnNullopt) {
    auto root = std::make_shared<Assembly>();
    root->AddEntity(std::make_shared<Point>(Vector3d(2, 2, 2)));  // 0次元
    root->AddEntity(MakeLine({0, 0, 0}, {1, 0, 0}));  // x軸平行 = 1次元

    EXPECT_FALSE(root->GetWorldBoundingBox().has_value());
}

// 物理従属メンバは世界BBに寄与しない
TEST_F(AssemblyCoordsTest,
       GetWorldBoundingBox_ExcludesPhysicallyDependentMembers) {
    auto dependent = LoadedDependentGeometry();
    ASSERT_NE(dependent, nullptr)
        << "物理従属かつ幾何を持つメンバが見つからない";

    // 物理従属メンバ単独 → 除外されてnullopt
    auto only_dep = std::make_shared<Assembly>();
    only_dep->AddEntity(dependent);
    EXPECT_FALSE(only_dep->GetWorldBoundingBox().has_value());

    // 独立な3D線 + 物理従属メンバ → 線のBBのみが反映される
    auto mixed = std::make_shared<Assembly>();
    mixed->AddEntity(MakeLine({0, 0, 0}, {1, 2, 3}));
    mixed->AddEntity(dependent);
    const auto bb = mixed->GetWorldBoundingBox();
    ASSERT_TRUE(bb.has_value());
    ExpectBoxEquals(*bb, Vector3d(0, 0, 0), Vector3d(1, 2, 3));
}
