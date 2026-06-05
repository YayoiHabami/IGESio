/**
 * @file tests/entities/surfaces/test_trimmed_surface.cpp
 * @brief TrimmedSurface (Type 144) および IRestrictedSurface のテスト
 * @author Yayoi Habami
 * @date 2026-06-03
 * @copyright 2026 Yayoi Habami
 * @note IRestrictedSurfaceは抽象クラスのため、具象TrimmedSurfaceを介して
 *       テストする。対象は以下:
 *       - IRestrictedSurface: IsInDomain / TryGetDefinedDerivatives /
 *         GetParameterRange / GetDefinedBoundingBox / IsUClosed / IsVClosed /
 *         GetOuterDomainPolygon / GetInnerDomainPolygons / IsOuterBoundaryOfD
 *       - TrimmedSurface フック: GetBaseSurface / GetOuterUVBoundary /
 *         GetInnerBoundaryCount / GetInnerUVBoundaryAt
 *       - キャッシュ無効化 (PrepareGeometryCache / Set/Add/Remove系)
 *       - エラー系・グレースフル劣化 (未解決参照)
 *
 * TODO: ValidatePD()は本変更で不変のため未カバー。
 * TODO: 境界曲線上 (u==辺) のちょうどの点の内外判定は近似依存のため未カバー
 *       (±εの内/外で検証)。
 * TODO: 内側境界が外側境界の外に出る不正配置は未カバー。
 */
#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/common/iges_parameter_vector.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/entities/entity_type.h"
#include "igesio/entities/curves/circular_arc.h"
#include "igesio/entities/curves/linear_path.h"
#include "igesio/entities/curves/curve_on_a_parametric_surface.h"
#include "igesio/entities/surfaces/trimmed_surface.h"
#include "./surfaces_for_testing.h"

namespace {

namespace i_ent = igesio::entities;
namespace i_test = igesio::tests;
using igesio::Vector2d;
using igesio::Vector3d;
using i_ent::TrimmedSurface;
using i_ent::CurveOnAParametricSurface;

/// @brief 数値比較の許容誤差
constexpr double kTol = 1e-9;
/// @brief 境界精度テストで辺から内/外へずらすオフセット
constexpr double kBoundaryEps = 0.01;

/// @brief 基底NURBS平面 D=[0,1]² を作成する
std::shared_ptr<i_ent::ISurface> MakePlane() {
    return i_test::CreateRationalBSplineSurfaces()[0].surface;
}

/// @brief V方向に閉じた全回転面 (0〜2π) を作成する
/// @note IsVClosed()==true、IsUClosed()==false (母線が開曲線)
std::shared_ptr<i_ent::ISurface> MakeClosedBaseSurface() {
    return i_test::CreateSurfaceOfRevolutions()[0].surface;
}

/// @brief z=0平面上の軸平行矩形ループ (kPlanarLoop) を作成する
std::shared_ptr<i_ent::ICurve> MakeUvRectLoop(
        const double umin, const double vmin,
        const double umax, const double vmax) {
    return std::make_shared<i_ent::LinearPath>(
        std::vector<Vector2d>{
            {umin, vmin}, {umax, vmin}, {umax, vmax}, {umin, vmax}},
        true);
}

/// @brief z=0平面上の全円ループを作成する
std::shared_ptr<i_ent::ICurve> MakeUvCircle(
        const Vector2d& center, const double radius) {
    return i_ent::MakeCircle(center, radius);
}

/// @brief UV曲線を境界 (Type142) に変換する (C(t)は自動生成)
std::shared_ptr<CurveOnAParametricSurface> MakeBoundary142(
        const std::shared_ptr<i_ent::ISurface>& surface,
        const std::shared_ptr<i_ent::ICurve>& uv_loop) {
    auto [c142, _] = i_ent::MakeCurveOnAParametricSurface(surface, uv_loop);
    return c142;
}

}  // namespace



/**
 * フック (GetBaseSurface / GetOuterUVBoundary / GetInnerBoundaryCount /
 *        GetInnerUVBoundaryAt) と外側境界フラグ
 */

TEST(TrimmedSurfaceHooks, UntrimmedExposesNoBoundaries) {
    auto plane = MakePlane();
    auto ts = std::make_shared<TrimmedSurface>(plane);  // N1=0

    EXPECT_TRUE(ts->IsOuterBoundaryOfD());
    EXPECT_EQ(ts->GetOuterUVBoundary(), nullptr);
    EXPECT_EQ(ts->GetInnerBoundaryCount(), 0u);
    EXPECT_EQ(ts->GetBaseSurface().get(), plane.get());
}

TEST(TrimmedSurfaceHooks, ExplicitOuterExposesOuterBaseCurve) {
    auto plane = MakePlane();
    auto loop = MakeUvRectLoop(0.2, 0.2, 0.8, 0.8);
    auto [outer, _] = i_ent::MakeCurveOnAParametricSurface(plane, loop);
    auto ts = std::make_shared<TrimmedSurface>(plane, outer);  // N1=1

    EXPECT_FALSE(ts->IsOuterBoundaryOfD());
    // 外側境界のUV曲線は、外側Type142のベース曲線 (= 渡したloop) と一致する
    EXPECT_EQ(ts->GetOuterUVBoundary().get(), loop.get());
}

TEST(TrimmedSurfaceHooks, InnerBoundariesExposed) {
    auto plane = MakePlane();
    auto inner_loop = MakeUvRectLoop(0.4, 0.4, 0.6, 0.6);
    auto [inner, _] = i_ent::MakeCurveOnAParametricSurface(plane, inner_loop);

    auto ts = std::make_shared<TrimmedSurface>(plane);
    ts->AddInnerBoundary(inner);

    EXPECT_EQ(ts->GetInnerBoundaryCount(), 1u);
    EXPECT_EQ(ts->GetInnerUVBoundaryAt(0).get(), inner_loop.get());
}



/**
 * IsInDomain (矩形境界で厳密に)
 */

TEST(TrimmedSurfaceDomain, UntrimmedTrueEverywhere) {
    auto plane = MakePlane();
    auto ts = std::make_shared<TrimmedSurface>(plane);

    EXPECT_TRUE(ts->IsInDomain(0.5, 0.5));
    EXPECT_TRUE(ts->IsInDomain(0.0, 0.0));
    EXPECT_TRUE(ts->IsInDomain(1.0, 1.0));
}

TEST(TrimmedSurfaceDomain, OuterRectInsideTrueOutsideFalse) {
    auto plane = MakePlane();
    auto outer = MakeBoundary142(plane, MakeUvRectLoop(0.2, 0.2, 0.8, 0.8));
    auto ts = std::make_shared<TrimmedSurface>(plane, outer);

    EXPECT_TRUE(ts->IsInDomain(0.5, 0.5));   // 矩形内部
    EXPECT_FALSE(ts->IsInDomain(0.1, 0.5));  // D内だが矩形外
    EXPECT_FALSE(ts->IsInDomain(0.5, 0.9));  // D内だが矩形外
}

TEST(TrimmedSurfaceDomain, OuterRectBoundaryPrecision) {
    auto plane = MakePlane();
    auto outer = MakeBoundary142(plane, MakeUvRectLoop(0.2, 0.2, 0.8, 0.8));
    auto ts = std::make_shared<TrimmedSurface>(plane, outer);

    // u=0.2 (矩形の左辺) の内/外
    EXPECT_FALSE(ts->IsInDomain(0.2 - kBoundaryEps, 0.5));
    EXPECT_TRUE(ts->IsInDomain(0.2 + kBoundaryEps, 0.5));
}

TEST(TrimmedSurfaceDomain, InnerHoleExcluded) {
    auto plane = MakePlane();
    auto outer = MakeBoundary142(plane, MakeUvRectLoop(0.2, 0.2, 0.8, 0.8));
    auto hole = MakeBoundary142(plane, MakeUvRectLoop(0.4, 0.4, 0.6, 0.6));

    auto ts = std::make_shared<TrimmedSurface>(plane, outer);
    ts->AddInnerBoundary(hole);

    EXPECT_FALSE(ts->IsInDomain(0.5, 0.5));  // 穴の内部
    EXPECT_TRUE(ts->IsInDomain(0.3, 0.5));   // 外側内かつ穴の外
    EXPECT_FALSE(ts->IsInDomain(0.1, 0.5));  // 外側境界の外
}

TEST(TrimmedSurfaceDomain, MultipleHolesExcluded) {
    auto plane = MakePlane();
    auto outer = MakeBoundary142(plane, MakeUvRectLoop(0.1, 0.1, 0.9, 0.9));
    auto hole1 = MakeBoundary142(plane, MakeUvRectLoop(0.2, 0.2, 0.35, 0.35));
    auto hole2 = MakeBoundary142(plane, MakeUvRectLoop(0.6, 0.6, 0.75, 0.75));

    auto ts = std::make_shared<TrimmedSurface>(plane, outer);
    ts->AddInnerBoundary(hole1);
    ts->AddInnerBoundary(hole2);

    EXPECT_EQ(ts->GetInnerBoundaryCount(), 2u);
    EXPECT_FALSE(ts->IsInDomain(0.275, 0.275));  // 穴1の内部
    EXPECT_FALSE(ts->IsInDomain(0.675, 0.675));  // 穴2の内部
    EXPECT_TRUE(ts->IsInDomain(0.5, 0.5));        // どちらの穴でもない
}

TEST(TrimmedSurfaceDomain, OuterCircleInsideOutside) {
    auto plane = MakePlane();
    auto outer = MakeBoundary142(plane, MakeUvCircle(Vector2d{0.5, 0.5}, 0.3));
    auto ts = std::make_shared<TrimmedSurface>(plane, outer);

    EXPECT_TRUE(ts->IsInDomain(0.5, 0.5));   // 円の中心 (内部)
    EXPECT_FALSE(ts->IsInDomain(0.5, 0.95));  // 円外 (距離0.45 > 0.3)
}



/**
 * 基底曲面への委譲
 */

TEST(TrimmedSurfaceDelegation, ParameterRangeMatchesBase) {
    auto plane = MakePlane();
    auto ts = std::make_shared<TrimmedSurface>(plane);

    auto r = ts->GetParameterRange();
    auto r0 = plane->GetParameterRange();
    for (int i = 0; i < 4; ++i) EXPECT_NEAR(r[i], r0[i], kTol);
}

TEST(TrimmedSurfaceDelegation, BoundingBoxMatchesBase) {
    auto plane = MakePlane();
    auto ts = std::make_shared<TrimmedSurface>(plane);

    auto bb = ts->GetDefinedBoundingBox();
    auto bb0 = plane->GetDefinedBoundingBox();
    EXPECT_NEAR((bb.GetControl() - bb0.GetControl()).norm(), 0.0, kTol);
    auto s = bb.GetSizes();
    auto s0 = bb0.GetSizes();
    for (int i = 0; i < 3; ++i) EXPECT_NEAR(s[i], s0[i], kTol);
}

TEST(TrimmedSurfaceDelegation, DerivativesNulloptOutsideDomain) {
    auto plane = MakePlane();
    auto outer = MakeBoundary142(plane, MakeUvRectLoop(0.2, 0.2, 0.8, 0.8));
    auto ts = std::make_shared<TrimmedSurface>(plane, outer);

    EXPECT_FALSE(ts->TryGetDefinedDerivatives(0.1, 0.5, 0).has_value());
}

TEST(TrimmedSurfaceDelegation, DerivativesPresentInsideDomain) {
    auto plane = MakePlane();
    auto outer = MakeBoundary142(plane, MakeUvRectLoop(0.2, 0.2, 0.8, 0.8));
    auto ts = std::make_shared<TrimmedSurface>(plane, outer);

    EXPECT_TRUE(ts->TryGetDefinedDerivatives(0.5, 0.5, 1).has_value());
}



/**
 * 閉性 (N1=0は委譲、N1=1は保守的にfalse)
 */

TEST(TrimmedSurfaceClosedness, UntrimmedDelegatesToBase) {
    auto base = MakeClosedBaseSurface();
    ASSERT_TRUE(base->IsVClosed());  // 全回転面はV方向に閉
    auto ts = std::make_shared<TrimmedSurface>(base);  // N1=0

    EXPECT_EQ(ts->IsUClosed(), base->IsUClosed());
    EXPECT_EQ(ts->IsVClosed(), base->IsVClosed());
}

TEST(TrimmedSurfaceClosedness, ExplicitOuterReturnsFalse) {
    auto base = MakeClosedBaseSurface();
    ASSERT_TRUE(base->IsVClosed());

    auto r = base->GetParameterRange();
    auto loop = MakeUvRectLoop(
        r[0] + 0.3 * (r[1] - r[0]), r[2] + 0.3 * (r[3] - r[2]),
        r[0] + 0.6 * (r[1] - r[0]), r[2] + 0.6 * (r[3] - r[2]));
    auto outer = MakeBoundary142(base, loop);
    auto ts = std::make_shared<TrimmedSurface>(base, outer);  // N1=1

    EXPECT_FALSE(ts->IsUClosed());
    EXPECT_FALSE(ts->IsVClosed());  // 基底はV閉だが明示外側のため保守的にfalse
}



/**
 * 領域多角形アクセサ (テッセレーション用)
 */

TEST(TrimmedSurfaceDomainPolygons, UntrimmedOuterNullopt) {
    auto plane = MakePlane();
    auto ts = std::make_shared<TrimmedSurface>(plane);

    EXPECT_FALSE(ts->GetOuterDomainPolygon().has_value());
    EXPECT_TRUE(ts->GetInnerDomainPolygons().empty());
}

TEST(TrimmedSurfaceDomainPolygons, ExplicitOuterHasValue) {
    auto plane = MakePlane();
    auto outer = MakeBoundary142(plane, MakeUvRectLoop(0.2, 0.2, 0.8, 0.8));
    auto ts = std::make_shared<TrimmedSurface>(plane, outer);

    EXPECT_TRUE(ts->GetOuterDomainPolygon().has_value());
}

TEST(TrimmedSurfaceDomainPolygons, InnerCountMatches) {
    auto plane = MakePlane();
    auto hole1 = MakeBoundary142(plane, MakeUvRectLoop(0.2, 0.2, 0.35, 0.35));
    auto hole2 = MakeBoundary142(plane, MakeUvRectLoop(0.6, 0.6, 0.75, 0.75));

    auto ts = std::make_shared<TrimmedSurface>(plane);
    ts->AddInnerBoundary(hole1);
    ts->AddInnerBoundary(hole2);

    EXPECT_EQ(ts->GetInnerDomainPolygons().size(), 2u);
}



/**
 * キャッシュの構築・無効化
 */

TEST(TrimmedSurfaceCache, PrepareThenConsistent) {
    auto plane = MakePlane();
    auto outer = MakeBoundary142(plane, MakeUvRectLoop(0.2, 0.2, 0.8, 0.8));
    auto hole = MakeBoundary142(plane, MakeUvRectLoop(0.4, 0.4, 0.6, 0.6));
    auto ts = std::make_shared<TrimmedSurface>(plane, outer);
    ts->AddInnerBoundary(hole);

    ASSERT_NO_THROW(ts->PrepareGeometryCache());

    EXPECT_TRUE(ts->IsInDomain(0.3, 0.5));   // 外側内・穴外
    EXPECT_FALSE(ts->IsInDomain(0.5, 0.5));  // 穴内
}

TEST(TrimmedSurfaceCache, AddInnerInvalidates) {
    auto plane = MakePlane();
    auto outer = MakeBoundary142(plane, MakeUvRectLoop(0.2, 0.2, 0.8, 0.8));
    auto ts = std::make_shared<TrimmedSurface>(plane, outer);

    EXPECT_TRUE(ts->IsInDomain(0.5, 0.5));  // 穴追加前は内部

    auto hole = MakeBoundary142(plane, MakeUvRectLoop(0.4, 0.4, 0.6, 0.6));
    ts->AddInnerBoundary(hole);

    EXPECT_FALSE(ts->IsInDomain(0.5, 0.5));  // 穴追加でキャッシュ無効化→穴内
}

TEST(TrimmedSurfaceCache, RemoveInnerInvalidates) {
    auto plane = MakePlane();
    auto outer = MakeBoundary142(plane, MakeUvRectLoop(0.2, 0.2, 0.8, 0.8));
    auto hole = MakeBoundary142(plane, MakeUvRectLoop(0.4, 0.4, 0.6, 0.6));
    auto ts = std::make_shared<TrimmedSurface>(plane, outer);
    ts->AddInnerBoundary(hole);

    EXPECT_FALSE(ts->IsInDomain(0.5, 0.5));  // 穴内

    ts->RemoveInnerBoundaryAt(0);

    EXPECT_EQ(ts->GetInnerBoundaryCount(), 0u);
    EXPECT_TRUE(ts->IsInDomain(0.5, 0.5));  // 穴除去でキャッシュ無効化→内部
}

TEST(TrimmedSurfaceCache, SetOuterNullRevertsToUntrimmed) {
    auto plane = MakePlane();
    auto outer = MakeBoundary142(plane, MakeUvRectLoop(0.2, 0.2, 0.8, 0.8));
    auto ts = std::make_shared<TrimmedSurface>(plane, outer);

    EXPECT_FALSE(ts->IsInDomain(0.1, 0.5));  // 外側境界の外

    ts->SetOuterBoundary(nullptr);  // N1=0へ

    EXPECT_TRUE(ts->IsOuterBoundaryOfD());
    EXPECT_TRUE(ts->IsInDomain(0.1, 0.5));  // 制限なしへ復帰
}



/**
 * エラー系
 */

TEST(TrimmedSurfaceErrors, GetInnerUVBoundaryAtThrowsOutOfRange) {
    auto plane = MakePlane();
    auto ts = std::make_shared<TrimmedSurface>(plane);  // 内側境界なし

    EXPECT_THROW(ts->GetInnerUVBoundaryAt(0), std::out_of_range);
}

TEST(TrimmedSurfaceErrors, ConstructorThrowsWhenSurfaceNull) {
    std::shared_ptr<i_ent::ISurface> null_surface;
    EXPECT_THROW((void)std::make_shared<TrimmedSurface>(null_surface),
                 std::invalid_argument);
}

TEST(TrimmedSurfaceErrors, AddInnerBoundaryThrowsWhenNull) {
    auto plane = MakePlane();
    auto ts = std::make_shared<TrimmedSurface>(plane);
    std::shared_ptr<CurveOnAParametricSurface> null_boundary;

    EXPECT_THROW(ts->AddInnerBoundary(null_boundary), std::invalid_argument);
}

// 境界: パラメータ数が最小値4のすぐ外 (3個) はEntityParameterError
TEST(TrimmedSurfaceErrors, ConstructorThrowsEntityParameterErrorWhenTooFewParams) {
    auto plane = MakePlane();

    EXPECT_THROW((void)std::make_shared<TrimmedSurface>(
            i_ent::RawEntityDE::ByDefault(i_ent::EntityType::kTrimmedSurface),
            igesio::IGESParameterVector{plane->GetID(), 0, 0}),
        igesio::EntityParameterError);
}

// 内側境界数N2に対してポインタ数が不足する場合もEntityParameterError
TEST(TrimmedSurfaceErrors,
     ConstructorThrowsEntityParameterErrorWhenInnerCountExceedsParams) {
    auto plane = MakePlane();

    // N2=2と宣言するが内側境界ポインタが0個 (必要数4+2=6 > 4)
    EXPECT_THROW((void)std::make_shared<TrimmedSurface>(
            i_ent::RawEntityDE::ByDefault(i_ent::EntityType::kTrimmedSurface),
            igesio::IGESParameterVector{plane->GetID(), 0, 2, 0}),
        igesio::EntityParameterError);
}

// 参照未解決のままGetSurfaceを呼ぶとReferenceErrorを投げる
TEST(TrimmedSurfaceErrors, GetSurfaceThrowsReferenceErrorWhenUnresolved) {
    auto plane = MakePlane();

    // from-IGES構築: 曲面のIDのみ保持し、参照解決は行わない
    auto ts = std::make_shared<TrimmedSurface>(
        i_ent::RawEntityDE::ByDefault(i_ent::EntityType::kTrimmedSurface),
        igesio::IGESParameterVector{plane->GetID(), 1, 0, 0});

    EXPECT_THROW(ts->GetSurface(), igesio::ReferenceError);
}



/**
 * グレースフル劣化 (C層): 外側境界の参照が未解決のまま
 */

TEST(TrimmedSurfaceGraceful, UnresolvedOuterTreatedAsNoRestriction) {
    auto plane = MakePlane();
    auto outer = MakeBoundary142(plane, MakeUvRectLoop(0.2, 0.2, 0.8, 0.8));

    // from-IGES構築: N1=1 で外側境界を参照するが、SetUnresolvedPDReferences を
    // 呼ばないため参照は未解決のまま
    auto ts = std::make_shared<TrimmedSurface>(
        i_ent::RawEntityDE::ByDefault(i_ent::EntityType::kTrimmedSurface),
        igesio::IGESParameterVector{plane->GetID(), 1, 0, outer->GetID()});

    EXPECT_FALSE(ts->IsOuterBoundaryOfD());        // N1=1
    EXPECT_EQ(ts->GetOuterUVBoundary(), nullptr);  // 未解決 → nullptr
    // 制限を構築できないため、ドメイン判定は常にtrue (制限なし扱い)
    EXPECT_TRUE(ts->IsInDomain(0.5, 0.5));
    EXPECT_TRUE(ts->IsInDomain(0.1, 0.5));
}
