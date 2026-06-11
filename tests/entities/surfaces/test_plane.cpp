/**
 * @file tests/entities/surfaces/test_plane.cpp
 * @brief Plane (Type 108, Form 0) / BoundedPlane (Form 1/-1) のテスト
 * @author Yayoi Habami
 * @date 2026-06-09
 * @copyright 2026 Yayoi Habami
 *
 * テスト対象:
 *   [自由関数] MakePlaneFrame
 *   [Plane (Form 0)] MakePlane / 係数・フレーム・表示シンボル / ISurface実装
 *     (無限範囲・アフィン偏導関数・無限OBB) / Transform / PD読込 / ValidatePD
 *   [BoundedPlane (Form 1/-1)] MakeBoundedPlane / IRestrictedSurfaceフック /
 *     内外判定 (円・矩形・傾き平面) / 偏導関数の域ゲート / 有界BBox /
 *     キャッシュ無効化 / グレースフル劣化 / ValidatePD
 *   [EntityFactory] form番号による Plane/BoundedPlaneの振り分け
 *
 * TODO:
 *   - 表示シンボルと追加ポインタ(402/212/312)同居時のインデックス曖昧性解消は
 *     test_entity_base / test_igesio の責務
 *   - GetMainPDParameters()経由の書き出し整合 (protected) はtest_igesioの責務
 *   - 円境界の「辺ちょうど」の内外は近似依存のため未検証 (中心 vs 明確な遠点、
 *     および厳密多角形である矩形境界の±オフセットでのみ検証)
 *   - 汎用ISurface適合 (評価系の網羅) はtest_i_surface.cppの責務
 */
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/common/iges_parameter_vector.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/entities/entity_type.h"
#include "igesio/entities/factory.h"
#include "igesio/entities/de/raw_entity_de.h"
#include "igesio/entities/curves/circular_arc.h"
#include "igesio/entities/curves/linear_path.h"
#include "igesio/entities/transformations/transformation_matrix.h"
#include "igesio/entities/surfaces/plane.h"

namespace {

namespace i_ent = igesio::entities;
using igesio::Vector2d;
using igesio::Vector3d;
using i_ent::Plane;
using i_ent::BoundedPlane;
using i_ent::EntityType;
using i_ent::RawEntityDE;
using igesio::IGESParameterVector;

/// @brief 数値比較の許容誤差
constexpr double kTol = 1e-9;
/// @brief IsApproxZeroの許容差を確実に超えるオフセット
constexpr double kClearOffset = 1e-3;
/// @brief 矩形境界で辺から内/外へずらすオフセット (厳密多角形)
constexpr double kBoundaryEps = 0.01;



/// @brief Vector3dの成分一致を検証する
void ExpectVectorNear(const Vector3d& actual, const Vector3d& expected,
                      const double tol = kTol) {
    EXPECT_NEAR(actual.x(), expected.x(), tol);
    EXPECT_NEAR(actual.y(), expected.y(), tol);
    EXPECT_NEAR(actual.z(), expected.z(), tol);
}

/// @brief 曲面上の点S(u, v)を評価する (TryGetDefinedDerivativesの0階)
Vector3d EvalSurface(const i_ent::ISurface& surface,
                     const double u, const double v) {
    auto deriv = surface.TryGetDefinedDerivatives(u, v, 0);
    EXPECT_TRUE(deriv.has_value());
    if (!deriv.has_value()) return Vector3d::Zero();
    return deriv.value()(0, 0);
}

/// @brief z=0平面上の円 (中心(cx,cy)・半径r) を作成する
std::shared_ptr<i_ent::ICurve> MakeZ0Circle(
        const double cx, const double cy, const double r) {
    return i_ent::MakeCircle(Vector2d(cx, cy), r);
}

/// @brief z=0平面上の軸平行矩形ループ (閉) を作成する
std::shared_ptr<i_ent::ICurve> MakeZ0RectLoop(
        const double umin, const double vmin,
        const double umax, const double vmax) {
    return i_ent::MakeLinearPath(
        std::vector<Vector2d>{
            {umin, vmin}, {umax, vmin}, {umax, vmax}, {umin, vmax}},
        true);
}

/// @brief from-IGES構築用のデフォルトDE (Plane, 指定form)
RawEntityDE PlaneDE(const int form) {
    return RawEntityDE::ByDefault(EntityType::kPlane, form);
}

}  // namespace



/**
 * MakePlaneFrame (自由関数)
 */

TEST(PlaneFrameTest, Frame_AxisAlignedZ_Orthonormal) {
    const auto f = i_ent::MakePlaneFrame(0.0, 0.0, 1.0, 0.0);

    ExpectVectorNear(f.normal, Vector3d(0.0, 0.0, 1.0));
    ExpectVectorNear(f.origin, Vector3d(0.0, 0.0, 0.0));
    EXPECT_NEAR(f.e_u.norm(), 1.0, kTol);
    EXPECT_NEAR(f.e_v.norm(), 1.0, kTol);
    EXPECT_NEAR(f.e_u.dot(f.e_v), 0.0, kTol);
    EXPECT_NEAR(f.e_u.dot(f.normal), 0.0, kTol);
    EXPECT_NEAR(f.e_v.dot(f.normal), 0.0, kTol);
    // 右手系: e_u × e_v = n̂
    ExpectVectorNear(f.e_u.cross(f.e_v), f.normal);
}

TEST(PlaneFrameTest, Frame_Tilted_LiesInPlaneAndOrthonormal) {
    const auto f = i_ent::MakePlaneFrame(1.0, 1.0, 1.0, 3.0);

    // n·origin = D
    EXPECT_NEAR(Vector3d(1.0, 1.0, 1.0).dot(f.origin), 3.0, kTol);
    EXPECT_NEAR(f.normal.norm(), 1.0, kTol);
    EXPECT_NEAR(f.e_u.dot(f.e_v), 0.0, kTol);
    EXPECT_NEAR(f.e_u.dot(f.normal), 0.0, kTol);
    EXPECT_NEAR(f.e_v.dot(f.normal), 0.0, kTol);
    ExpectVectorNear(f.e_u.cross(f.e_v), f.normal);
}

TEST(PlaneFrameTest, Frame_ThrowsEntityValueErrorWhenNormalZero) {
    EXPECT_THROW(i_ent::MakePlaneFrame(0.0, 0.0, 0.0, 5.0),
                 igesio::EntityValueError);
    // 境界の内側: 微小でも非ゼロの法線は許容される
    EXPECT_NO_THROW(i_ent::MakePlaneFrame(kClearOffset, 0.0, 0.0, 5.0));
}



/**
 * Plane (Form 0): 構築・アクセサ
 */

TEST(PlaneConstruction, MakePlane_StoresCoefficientsAndForm0) {
    const auto p = i_ent::MakePlane(0.0, 0.0, 1.0, 2.0);

    const auto& c = p->GetCoefficients();
    EXPECT_NEAR(c[0], 0.0, kTol);
    EXPECT_NEAR(c[1], 0.0, kTol);
    EXPECT_NEAR(c[2], 1.0, kTol);
    EXPECT_NEAR(c[3], 2.0, kTol);
    EXPECT_EQ(p->GetType(), EntityType::kPlane);
    EXPECT_EQ(p->GetFormNumber(), 0);
}

TEST(PlaneConstruction, MakePlaneFromNormalAndPoint_PassesThroughPoint) {
    const Vector3d normal(0.0, 0.0, 2.0);  // 非単位でも可
    const Vector3d point(1.0, 2.0, 3.0);
    const auto p = i_ent::MakePlane(normal, point);

    // D = n·point。点が面上にあること (n·S = D)
    const auto& c = p->GetCoefficients();
    EXPECT_NEAR(c[3], normal.dot(point), kTol);
    EXPECT_NEAR(Vector3d(c[0], c[1], c[2]).dot(point), c[3], kTol);
}

TEST(PlaneConstruction, SymbolDefaultsZeroForDirectConstruction) {
    const auto p = i_ent::MakePlane(0.0, 0.0, 1.0, 0.0);
    ExpectVectorNear(p->GetSymbolLocation(), Vector3d::Zero());
    EXPECT_NEAR(p->GetSymbolSize(), 0.0, kTol);
}



/**
 * Plane (Form 0): ISurface 幾何
 */

TEST(PlaneGeometry, ParameterRange_Infinite) {
    const auto p = i_ent::MakePlane(0.0, 0.0, 1.0, 0.0);
    const auto r = p->GetParameterRange();
    EXPECT_FALSE(std::isfinite(r[0]));
    EXPECT_FALSE(std::isfinite(r[1]));
    EXPECT_FALSE(std::isfinite(r[2]));
    EXPECT_FALSE(std::isfinite(r[3]));
    EXPECT_FALSE(p->IsFinite());
    EXPECT_FALSE(p->HasFiniteUStart());
}

TEST(PlaneGeometry, Closedness_FalseBoth) {
    const auto p = i_ent::MakePlane(0.0, 0.0, 1.0, 0.0);
    EXPECT_FALSE(p->IsUClosed());
    EXPECT_FALSE(p->IsVClosed());
}

TEST(PlaneGeometry, Derivatives_AffineForm) {
    const auto p = i_ent::MakePlane(0.0, 0.0, 1.0, 0.0);
    const auto f = p->GetFrame();

    auto d = p->TryGetDefinedDerivatives(2.0, 3.0, 2);
    ASSERT_TRUE(d.has_value());
    // S(u,v) = origin + u·e_u + v·e_v
    ExpectVectorNear((*d)(0, 0), f.origin + 2.0 * f.e_u + 3.0 * f.e_v);
    // Su = e_u, Sv = e_v
    ExpectVectorNear((*d)(1, 0), f.e_u);
    ExpectVectorNear((*d)(0, 1), f.e_v);
    // 2階はすべてゼロ
    ExpectVectorNear((*d)(2, 0), Vector3d::Zero());
    ExpectVectorNear((*d)(1, 1), Vector3d::Zero());
    ExpectVectorNear((*d)(0, 2), Vector3d::Zero());
}

TEST(PlaneGeometry, Derivatives_PointLiesInPlane) {
    const auto p = i_ent::MakePlane(1.0, 1.0, 1.0, 3.0);
    const Vector3d n(1.0, 1.0, 1.0);
    for (const auto& uv : std::vector<std::array<double, 2>>{
            {0.0, 0.0}, {3.0, -2.0}, {-5.0, 7.0}, {100.0, -100.0}}) {
        const auto s = EvalSurface(*p, uv[0], uv[1]);
        EXPECT_NEAR(n.dot(s), 3.0, 1e-6);
    }
}

TEST(PlaneGeometry, Evaluation_LargeParameters) {
    const auto p = i_ent::MakePlane(0.0, 0.0, 1.0, 0.0);
    EXPECT_TRUE(p->TryGetDefinedDerivatives(1e6, -1e6, 1).has_value());
}

TEST(PlaneGeometry, IsInDomain_TrueEverywhere) {
    const auto p = i_ent::MakePlane(0.0, 0.0, 1.0, 0.0);
    EXPECT_TRUE(p->IsInDomain(0.0, 0.0));
    EXPECT_TRUE(p->IsInDomain(1e9, -1e9));
}

TEST(PlaneGeometry, Normal_MatchesPlaneNormal) {
    const auto p = i_ent::MakePlane(0.0, 0.0, 1.0, 0.0);
    const auto normal = p->TryGetDefinedNormalAt(2.0, 3.0);
    ASSERT_TRUE(normal.has_value());
    ExpectVectorNear(*normal, Vector3d(0.0, 0.0, 1.0));
}

TEST(PlaneTransform, PointAt_RotationApplied) {
    // z=0平面 (normal ẑ) をx軸周りに90°回転 → 法線は -ŷ
    const auto p = i_ent::MakePlane(0.0, 0.0, 1.0, 0.0);
    const auto rotation =
        i_ent::MakeRotation(igesio::kPi / 2.0, Vector3d(1.0, 0.0, 0.0));
    ASSERT_TRUE(p->OverwriteTransformationMatrix(rotation));

    // 定義空間 S(1,0) = origin + e_u = (1,0,0); R_x(90°)·(1,0,0) = (1,0,0)
    const auto model = p->TryGetPointAt(1.0, 0.0);
    ASSERT_TRUE(model.has_value());
    ExpectVectorNear(*model, Vector3d(1.0, 0.0, 0.0));
    // 定義空間 S(0,1) = e_v = (0,1,0); R_x(90°)·(0,1,0) = (0,0,1)
    const auto model2 = p->TryGetPointAt(0.0, 1.0);
    ASSERT_TRUE(model2.has_value());
    ExpectVectorNear(*model2, Vector3d(0.0, 0.0, 1.0));
}



/**
 * Plane (Form 0): バウンディングボックス (無限OBB)
 */

TEST(PlaneBoundingBox, Defined_Is2DAndInfinite) {
    const auto p = i_ent::MakePlane(0.0, 0.0, 1.0, 0.0);
    const auto bb = p->GetDefinedBoundingBox();
    EXPECT_FALSE(bb.IsFinite());
    EXPECT_EQ(bb.Dimension(), 2u);
}

TEST(PlaneBoundingBox, Defined_NormalAxisSizeZero) {
    // z=0平面: 面内2方向 (e_u,e_v) が無限、法線方向 (ẑ) のサイズが0
    const auto p = i_ent::MakePlane(0.0, 0.0, 1.0, 0.0);
    const auto bb = p->GetDefinedBoundingBox();

    ExpectVectorNear(bb.GetControl(), Vector3d::Zero());
    const auto sizes = bb.GetSizes();
    int zero_count = 0, inf_count = 0;
    for (int i = 0; i < 3; ++i) {
        if (sizes[i] == 0.0) ++zero_count;
        else if (!std::isfinite(sizes[i])) ++inf_count;
    }
    EXPECT_EQ(zero_count, 1);
    EXPECT_EQ(inf_count, 2);
}



/**
 * Plane (Form 0): PD読込 (from-IGESコンストラクタ経由)
 */

TEST(PlanePD, ReadsDisplaySymbol_NineParams) {
    const Plane p(PlaneDE(0),
                  IGESParameterVector{1.0, 0.0, 0.0, 0.0, 0, 2.0, 3.0, 4.0, 5.0});
    ExpectVectorNear(p.GetSymbolLocation(), Vector3d(2.0, 3.0, 4.0));
    EXPECT_NEAR(p.GetSymbolSize(), 5.0, kTol);
}

TEST(PlanePD, NoDisplaySymbol_FiveParams_DefaultsZero) {
    const Plane p(PlaneDE(0), IGESParameterVector{1.0, 0.0, 0.0, 0.0, 0});
    ExpectVectorNear(p.GetSymbolLocation(), Vector3d::Zero());
    EXPECT_NEAR(p.GetSymbolSize(), 0.0, kTol);
}



/**
 * Plane (Form 0): 検証・エラー
 */

TEST(PlaneValidation, ValidatePD_ValidForm0) {
    const auto p = i_ent::MakePlane(0.0, 0.0, 1.0, 0.0);
    EXPECT_TRUE(p->ValidatePD().is_valid);
}

TEST(PlaneValidation, ValidatePD_InvalidWhenNormalZero) {
    const Plane p(PlaneDE(0), IGESParameterVector{0.0, 0.0, 0.0, 5.0, 0});
    EXPECT_FALSE(p.ValidatePD().is_valid);
}

TEST(PlaneValidation, ValidatePD_InvalidWhenFormNonZero) {
    // form 1のDEでPlaneを直接構築 (本来は BoundedPlaneが担うform)
    const Plane p(PlaneDE(1), IGESParameterVector{0.0, 0.0, 1.0, 0.0, 0});
    EXPECT_FALSE(p.ValidatePD().is_valid);
}

TEST(PlaneErrors, MakePlane_ThrowsEntityValueErrorWhenNormalZero) {
    EXPECT_THROW(i_ent::MakePlane(0.0, 0.0, 0.0, 5.0), igesio::EntityValueError);
    EXPECT_NO_THROW(i_ent::MakePlane(0.0, 0.0, kClearOffset, 5.0));
}

TEST(PlaneErrors, MakePlaneFromNormal_ThrowsEntityValueErrorWhenNormalZero) {
    EXPECT_THROW(i_ent::MakePlane(Vector3d::Zero(), Vector3d(1.0, 2.0, 3.0)),
                 igesio::EntityValueError);
}



/**
 * BoundedPlane (Form 1/-1): 構築・フック
 */

TEST(BoundedPlaneConstruction, MakeBoundedPlane_Form1_StoresBoundaryAndFlags) {
    const auto circle = MakeZ0Circle(0.0, 0.0, 2.0);
    const auto bp = i_ent::MakeBoundedPlane(0.0, 0.0, 1.0, 0.0, circle);

    EXPECT_EQ(bp->GetType(), EntityType::kPlane);
    EXPECT_EQ(bp->GetFormNumber(), 1);
    EXPECT_FALSE(bp->IsNegative());
    EXPECT_FALSE(bp->IsOuterBoundaryOfD());
    EXPECT_EQ(bp->GetBoundaryCurve()->GetID(), circle->GetID());
    auto circle_base = std::dynamic_pointer_cast<i_ent::EntityBase>(circle);
    ASSERT_NE(circle_base, nullptr);
    EXPECT_EQ(circle_base->GetSubordinateEntitySwitch(),
              i_ent::SubordinateEntitySwitch::kPhysicallyDependent);
}

TEST(BoundedPlaneConstruction, MakeBoundedPlane_Negative_FormMinus1) {
    const auto bp = i_ent::MakeBoundedPlane(
            0.0, 0.0, 1.0, 0.0, MakeZ0Circle(0.0, 0.0, 2.0), /*negative=*/true);
    EXPECT_EQ(bp->GetFormNumber(), -1);
    EXPECT_TRUE(bp->IsNegative());
}

TEST(BoundedPlaneConstruction, GetBaseSurface_PlaneWithSameCoefficients) {
    const auto bp = i_ent::MakeBoundedPlane(
            0.0, 0.0, 1.0, 0.0, MakeZ0Circle(0.0, 0.0, 2.0));
    const auto base = bp->GetBaseSurface();
    ASSERT_NE(base, nullptr);
    const auto base_plane = std::dynamic_pointer_cast<const Plane>(base);
    ASSERT_NE(base_plane, nullptr);
    EXPECT_NEAR(base_plane->GetCoefficients()[2], 1.0, kTol);
    // 基底は無限平面
    EXPECT_FALSE(base->IsFinite());
}

TEST(BoundedPlaneHooks, OuterNonNull_InnerZero_InnerAtThrows) {
    const auto bp = i_ent::MakeBoundedPlane(
            0.0, 0.0, 1.0, 0.0, MakeZ0Circle(0.0, 0.0, 2.0));
    EXPECT_NE(bp->GetOuterUVBoundary(), nullptr);
    EXPECT_EQ(bp->GetInnerBoundaryCount(), 0u);
    EXPECT_THROW(bp->GetInnerUVBoundaryAt(0), std::out_of_range);
}

TEST(BoundedPlaneHooks, Children_ContainBoundary) {
    const auto circle = MakeZ0Circle(0.0, 0.0, 2.0);
    const auto bp = i_ent::MakeBoundedPlane(0.0, 0.0, 1.0, 0.0, circle);

    const auto ids = bp->GetChildIDs();
    ASSERT_EQ(ids.size(), 1u);
    EXPECT_EQ(ids[0], circle->GetID());
    ASSERT_NE(bp->GetChildEntity(circle->GetID()), nullptr);
    EXPECT_EQ(bp->GetChildEntity(circle->GetID())->GetID(), circle->GetID());
}



/**
 * BoundedPlane: 内外判定
 */

TEST(BoundedPlaneDomain, CircleInsideOutside) {
    const auto bp = i_ent::MakeBoundedPlane(
            0.0, 0.0, 1.0, 0.0, MakeZ0Circle(0.0, 0.0, 2.0));
    EXPECT_TRUE(bp->IsInDomain(0.0, 0.0));    // 中心
    EXPECT_TRUE(bp->IsInDomain(1.0, 0.0));    // 内部
    EXPECT_FALSE(bp->IsInDomain(5.0, 0.0));   // 遠点 (半径2の外)
}

TEST(BoundedPlaneDomain, RectBoundaryPrecision) {
    const auto bp = i_ent::MakeBoundedPlane(
            0.0, 0.0, 1.0, 0.0, MakeZ0RectLoop(0.2, 0.2, 0.8, 0.8));
    EXPECT_TRUE(bp->IsInDomain(0.5, 0.5));                    // 矩形内
    EXPECT_FALSE(bp->IsInDomain(0.2 - kBoundaryEps, 0.5));    // 左辺の外
    EXPECT_TRUE(bp->IsInDomain(0.2 + kBoundaryEps, 0.5));     // 左辺の内
}

// 傾いた平面: z=0円をx軸周り45°回転し、法線∝(0,-1,1)の平面に乗せる。
// モデル空間フレームへの内積射影が正しく (u,v) を与えること。
TEST(BoundedPlaneDomain, TiltedPlaneCircle_InsideOutside) {
    auto circle = MakeZ0Circle(0.0, 0.0, 2.0);
    auto rot = i_ent::MakeRotation(igesio::kPi / 4.0, Vector3d(1.0, 0.0, 0.0));
    auto circle_base = std::dynamic_pointer_cast<i_ent::EntityBase>(circle);
    ASSERT_NE(circle_base, nullptr);
    ASSERT_TRUE(circle_base->OverwriteTransformationMatrix(rot));

    const auto bp = i_ent::MakeBoundedPlane(0.0, -1.0, 1.0, 0.0, circle);
    EXPECT_TRUE(bp->IsInDomain(0.0, 0.0));    // 円の中心は (u,v)=(0,0)
    EXPECT_TRUE(bp->IsInDomain(1.0, 0.0));    // 内部
    EXPECT_FALSE(bp->IsInDomain(0.0, 5.0));   // 遠点
}

TEST(BoundedPlaneDomain, FormMinus1_SameAsForm1) {
    const auto bp1 = i_ent::MakeBoundedPlane(
            0.0, 0.0, 1.0, 0.0, MakeZ0Circle(0.0, 0.0, 2.0), false);
    const auto bpm = i_ent::MakeBoundedPlane(
            0.0, 0.0, 1.0, 0.0, MakeZ0Circle(0.0, 0.0, 2.0), true);
    EXPECT_EQ(bp1->IsInDomain(0.0, 0.0), bpm->IsInDomain(0.0, 0.0));
    EXPECT_EQ(bp1->IsInDomain(5.0, 0.0), bpm->IsInDomain(5.0, 0.0));
}

TEST(BoundedPlaneDerivatives, NulloptOutside_PresentInside) {
    const auto bp = i_ent::MakeBoundedPlane(
            0.0, 0.0, 1.0, 0.0, MakeZ0Circle(0.0, 0.0, 2.0));
    EXPECT_FALSE(bp->TryGetDefinedDerivatives(5.0, 0.0, 0).has_value());

    auto d = bp->TryGetDefinedDerivatives(0.0, 0.0, 0);
    ASSERT_TRUE(d.has_value());
    // 内側は基底平面 S(0,0) = origin = (0,0,0)
    ExpectVectorNear((*d)(0, 0), Vector3d::Zero());
}

TEST(BoundedPlaneBoundingBox, Defined_FiniteAndContrastsBase) {
    const auto bp = i_ent::MakeBoundedPlane(
            0.0, 0.0, 1.0, 0.0, MakeZ0Circle(0.0, 0.0, 2.0));
    const auto bb = bp->GetDefinedBoundingBox();
    EXPECT_TRUE(bb.IsFinite());
    EXPECT_EQ(bb.Dimension(), 2u);
    // 基底 (無限平面) は無限
    EXPECT_FALSE(bp->GetBaseSurface()->GetDefinedBoundingBox().IsFinite());
}



/**
 * BoundedPlane: キャッシュ無効化・事前構築
 */

TEST(BoundedPlaneCache, SetBoundaryInvalidatesDomain) {
    const auto bp = i_ent::MakeBoundedPlane(
            0.0, 0.0, 1.0, 0.0, MakeZ0Circle(0.0, 0.0, 1.0));  // 半径1
    EXPECT_FALSE(bp->IsInDomain(2.0, 0.0));  // 半径1の外

    bp->SetBoundaryCurve(MakeZ0Circle(0.0, 0.0, 3.0));  // 半径3へ
    EXPECT_TRUE(bp->IsInDomain(2.0, 0.0));   // キャッシュ無効化→内部
}

TEST(BoundedPlaneCache, PrepareThenConsistent) {
    const auto bp = i_ent::MakeBoundedPlane(
            0.0, 0.0, 1.0, 0.0, MakeZ0Circle(0.0, 0.0, 2.0));
    ASSERT_NO_THROW(bp->PrepareGeometryCache());
    EXPECT_TRUE(bp->IsInDomain(0.0, 0.0));
    EXPECT_FALSE(bp->IsInDomain(5.0, 0.0));
}

// 境界の参照が未解決のまま: 制限を構築できないため内外判定は常にtrue (制限なし扱い)
TEST(BoundedPlaneGraceful, UnresolvedBoundaryNoRestriction) {
    const auto circle = MakeZ0Circle(0.0, 0.0, 2.0);
    const BoundedPlane bp(PlaneDE(1),
        IGESParameterVector{0.0, 0.0, 1.0, 0.0, circle->GetID(),
                            0.0, 0.0, 0.0, 0.0});

    EXPECT_FALSE(bp.IsOuterBoundaryOfD());        // form ±1
    EXPECT_EQ(bp.GetOuterUVBoundary(), nullptr);  // 未解決 → nullptr
    EXPECT_TRUE(bp.IsInDomain(0.0, 0.0));
    EXPECT_TRUE(bp.IsInDomain(100.0, 100.0));
}



/**
 * BoundedPlane: 検証・エラー
 */

TEST(BoundedPlaneValidation, ValidatePD_ValidClosed) {
    const auto bp = i_ent::MakeBoundedPlane(
            0.0, 0.0, 1.0, 0.0, MakeZ0Circle(0.0, 0.0, 2.0));
    EXPECT_TRUE(bp->ValidatePD().is_valid);
}

TEST(BoundedPlaneValidation, ValidatePD_WarningButValidForOpenBoundary) {
    // 開いた折れ線 (IsClosed()==false) はkWarningだがis_validはtrue
    auto open = i_ent::MakeLinearPath(
            std::vector<Vector2d>{{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}}, false);
    const auto bp = i_ent::MakeBoundedPlane(0.0, 0.0, 1.0, 0.0, open);
    const auto vr = bp->ValidatePD();
    EXPECT_TRUE(vr.is_valid);
    EXPECT_FALSE(vr.errors.empty());  // 警告が記録されている
}

TEST(BoundedPlaneValidation, ValidatePD_InvalidWhenBoundaryUnset) {
    const auto circle = MakeZ0Circle(0.0, 0.0, 2.0);
    const BoundedPlane bp(PlaneDE(1),
        IGESParameterVector{0.0, 0.0, 1.0, 0.0, circle->GetID(),
                            0.0, 0.0, 0.0, 0.0});
    EXPECT_FALSE(bp.ValidatePD().is_valid);  // 境界未解決 → kError
}

TEST(BoundedPlaneErrors, MakeBoundedPlane_ThrowsInvalidArgumentWhenBoundaryNull) {
    EXPECT_THROW(i_ent::MakeBoundedPlane(0.0, 0.0, 1.0, 0.0, nullptr),
                 std::invalid_argument);
}

TEST(BoundedPlaneErrors, MakeBoundedPlane_ThrowsEntityValueErrorWhenNormalZero) {
    EXPECT_THROW(
        i_ent::MakeBoundedPlane(0.0, 0.0, 0.0, 5.0, MakeZ0Circle(0.0, 0.0, 2.0)),
        igesio::EntityValueError);
    EXPECT_NO_THROW(i_ent::MakeBoundedPlane(
        0.0, 0.0, kClearOffset, 5.0, MakeZ0Circle(0.0, 0.0, 2.0)));
}

TEST(BoundedPlaneErrors, SetBoundaryCurve_ThrowsInvalidArgumentWhenNull) {
    const auto bp = i_ent::MakeBoundedPlane(
            0.0, 0.0, 1.0, 0.0, MakeZ0Circle(0.0, 0.0, 2.0));
    EXPECT_THROW(bp->SetBoundaryCurve(nullptr), std::invalid_argument);
}

TEST(BoundedPlaneErrors, Constructor_ThrowsEntityParameterErrorWhenTooFewParams) {
    // 最小値5のすぐ外 (4個)
    EXPECT_THROW((void)std::make_shared<BoundedPlane>(
            PlaneDE(1), IGESParameterVector{0.0, 0.0, 1.0, 0.0}),
        igesio::EntityParameterError);
}

TEST(BoundedPlaneErrors, GetBoundaryCurve_ThrowsReferenceErrorWhenUnresolved) {
    const auto circle = MakeZ0Circle(0.0, 0.0, 2.0);
    const BoundedPlane bp(PlaneDE(1),
        IGESParameterVector{0.0, 0.0, 1.0, 0.0, circle->GetID(),
                            0.0, 0.0, 0.0, 0.0});
    EXPECT_THROW(bp.GetBoundaryCurve(), igesio::ReferenceError);
}



/**
 * EntityFactory: form番号による振り分け
 */

TEST(PlaneFactoryDispatch, Form0_CreatesPlane) {
    auto e = i_ent::EntityFactory::CreateEntity(
        PlaneDE(0),
        IGESParameterVector{0.0, 0.0, 1.0, 0.0, 0, 0.0, 0.0, 0.0, 0.0}, {});
    EXPECT_NE(std::dynamic_pointer_cast<Plane>(e), nullptr);
    EXPECT_EQ(std::dynamic_pointer_cast<BoundedPlane>(e), nullptr);
}

TEST(PlaneFactoryDispatch, Form1_CreatesBoundedPlane) {
    const auto circle = MakeZ0Circle(0.0, 0.0, 2.0);
    auto e = i_ent::EntityFactory::CreateEntity(
        PlaneDE(1),
        IGESParameterVector{0.0, 0.0, 1.0, 0.0, circle->GetID(),
                            0.0, 0.0, 0.0, 0.0}, {});
    EXPECT_NE(std::dynamic_pointer_cast<BoundedPlane>(e), nullptr);
    EXPECT_EQ(std::dynamic_pointer_cast<Plane>(e), nullptr);
}

TEST(PlaneFactoryDispatch, FormMinus1_CreatesBoundedPlane) {
    const auto circle = MakeZ0Circle(0.0, 0.0, 2.0);
    auto e = i_ent::EntityFactory::CreateEntity(
        PlaneDE(-1),
        IGESParameterVector{0.0, 0.0, 1.0, 0.0, circle->GetID(),
                            0.0, 0.0, 0.0, 0.0}, {});
    auto bp = std::dynamic_pointer_cast<BoundedPlane>(e);
    ASSERT_NE(bp, nullptr);
    EXPECT_EQ(bp->GetFormNumber(), -1);
}
