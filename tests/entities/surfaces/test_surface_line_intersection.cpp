/**
 * @file entities/surfaces/test_surface_line_intersection.cpp
 * @brief surface_line_intersection.h のテスト
 * @author Yayoi Habami
 * @date 2026-05-27
 * @copyright 2026 Yayoi Habami
 *
 * ### 対象関数
 * - igesio::entities::IntersectSurfaceWithLine
 *
 * TODO: TrimmedSurface の穴領域テスト（CurveOnAParametricSurfaceの構築が複雑なため未実装）
 * TODO: 無限パラメータ範囲の直接的な検証（フォールバック値がBBサイズ依存のため複雑）
 */
#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/numerics/core/tolerance.h"
#include "igesio/entities/curves/line.h"
#include "igesio/entities/surfaces/rational_b_spline_surface.h"
#include "igesio/entities/surfaces/surface_of_revolution.h"
#include "igesio/entities/transformations/transformation_matrix.h"
#include "igesio/entities/surfaces/algorithms/surface_line_intersection.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using i_ent::SurfaceLineIntersection;
using i_ent::SurfaceLineIntersectionParams;
using i_ent::IntersectSurfaceWithLine;
using igesio::Vector3d;
using igesio::Matrix3d;
using LineType = i_num::BoundingBox::DirectionType;

/// @brief 位置比較の許容誤差 (Newton収束許容誤差の1000倍)
constexpr double kPosTol = 1e-6;

/// @brief 交点位置が期待値と一致するか検証するヘルパー
void ExpectPositionNear(const Vector3d& actual, const Vector3d& expected,
                        const double tol = kPosTol) {
    EXPECT_NEAR(actual.x(), expected.x(), tol);
    EXPECT_NEAR(actual.y(), expected.y(), tol);
    EXPECT_NEAR(actual.z(), expected.z(), tol);
}

/// @brief y=5 の平面 (RationalBSplineSurface, u/v ∈ [0,1], x/z ∈ [-5,5])
/// @note 変換なし（定義空間 = ワールド空間）
std::shared_ptr<i_ent::RationalBSplineSurface> MakeYFivePlane() {
    igesio::IGESParameterVector param{
        1, 1,
        1, 1,
        false, false, true, false, false,
        0., 0., 1., 1.,
        0., 0., 1., 1.,
        1., 1., 1., 1.,
        -5., 5.,  5.,
        -5., 5., -5.,
         5., 5.,  5.,
         5., 5., -5.,
        0., 1., 0., 1.
    };
    return std::make_shared<i_ent::RationalBSplineSurface>(param);
}

/// @brief y=0 の平面 (RationalBSplineSurface, u/v ∈ [0,1], x/z ∈ [-5,5])
/// @note 変換行列で移動・回転するための基底として使用する
std::shared_ptr<i_ent::RationalBSplineSurface> MakeYZeroPlane() {
    igesio::IGESParameterVector param{
        1, 1,
        1, 1,
        false, false, true, false, false,
        0., 0., 1., 1.,
        0., 0., 1., 1.,
        1., 1., 1., 1.,
        -5., 0.,  5.,
        -5., 0., -5.,
         5., 0.,  5.,
         5., 0., -5.,
        0., 1., 0., 1.
    };
    return std::make_shared<i_ent::RationalBSplineSurface>(param);
}

/// @brief z軸を回転軸とする半径2・高さ4の円柱面 (SurfaceOfRevolution)
/// @note S(u,v) = (2*cos(v), 2*sin(v), 4*u), u∈[0,1], v∈[0,2π]
std::shared_ptr<i_ent::SurfaceOfRevolution> MakeCylinder() {
    auto axis      = std::make_shared<i_ent::Line>(
        Vector3d{0., 0., 0.}, Vector3d{0., 0., 1.});
    auto generatrix = std::make_shared<i_ent::Line>(
        Vector3d{2., 0., 0.}, Vector3d{2., 0., 4.});
    return std::make_shared<i_ent::SurfaceOfRevolution>(
        axis, generatrix, 0.0, 2.0 * igesio::kPi);
}

/// @brief デフォルトの探索パラメータ (テスト用・サンプル数を増やしてある)
SurfaceLineIntersectionParams MakeParams(const int n = 20) {
    SurfaceLineIntersectionParams p;
    p.u_samples = n;
    p.v_samples = n;
    return p;
}

}  // namespace



/**
 * 基本的な交差テスト (変換なし)
 */

/// @brief y=5 の平面に法線方向のレイが正確に交差する
TEST(IntersectSurfaceWithLineTest, PlaneHit_RayNormal) {
    const auto plane = MakeYFivePlane();
    // p0=(0,0,0), p1=(0,1,0): +y方向のレイ
    const auto hits = IntersectSurfaceWithLine(
        *plane, Vector3d{0., 0., 0.}, Vector3d{0., 1., 0.},
        LineType::kRay, MakeParams());

    ASSERT_EQ(hits.size(), 1u);
    ExpectPositionNear(hits[0].position, Vector3d{0., 5., 0.});
    EXPECT_NEAR(hits[0].t, 5.0, kPosTol);
}

/// @brief 平面に平行なレイは交差しない
TEST(IntersectSurfaceWithLineTest, PlaneNoHit_Parallel) {
    const auto plane = MakeYFivePlane();
    // y=0 を通る x方向のレイは y=5 の平面と平行
    const auto hits = IntersectSurfaceWithLine(
        *plane, Vector3d{0., 0., 0.}, Vector3d{1., 0., 0.},
        LineType::kRay, MakeParams());

    EXPECT_TRUE(hits.empty());
}

/// @brief kSegment で平面に届かない場合は交差しない
TEST(IntersectSurfaceWithLineTest, PlaneNoHit_SegmentTooShort) {
    const auto plane = MakeYFivePlane();
    // p0=(0,0,0), p1=(0,1,0) のkSegment → t∈[0,1] で y=1 まで: 届かない
    const auto hits = IntersectSurfaceWithLine(
        *plane, Vector3d{0., 0., 0.}, Vector3d{0., 1., 0.},
        LineType::kSegment, MakeParams());

    EXPECT_TRUE(hits.empty());
}

/// @brief kLine では t<0 側も交差として検出される
TEST(IntersectSurfaceWithLineTest, PlaneHit_LineNegativeT) {
    const auto plane = MakeYFivePlane();
    // p0=(0,10,0), p1=(0,9,0): -y方向のkLine
    // t=5 で y=10+5*(-1)=5 → 交差
    const auto hits = IntersectSurfaceWithLine(
        *plane, Vector3d{0., 10., 0.}, Vector3d{0., 9., 0.},
        LineType::kLine, MakeParams());

    ASSERT_EQ(hits.size(), 1u);
    ExpectPositionNear(hits[0].position, Vector3d{0., 5., 0.});
}

/// @brief p0 == p1 (方向ベクトルがゼロ) の場合は空リストを返す
TEST(IntersectSurfaceWithLineTest, EmptyResult_ZeroDirection) {
    const auto plane = MakeYFivePlane();
    const auto hits = IntersectSurfaceWithLine(
        *plane, Vector3d{0., 0., 0.}, Vector3d{0., 0., 0.},
        LineType::kRay, MakeParams());

    EXPECT_TRUE(hits.empty());
}

/// @brief p0 に NaN が含まれる場合は例外を投げる
TEST(IntersectSurfaceWithLineTest, Throws_InvalidArg_NaN) {
    const auto plane = MakeYFivePlane();
    EXPECT_THROW(
        IntersectSurfaceWithLine(
            *plane,
            Vector3d{std::numeric_limits<double>::quiet_NaN(), 0., 0.},
            Vector3d{0., 1., 0.},
            LineType::kRay),
        std::invalid_argument);
}



/**
 * 円柱面の交差テスト
 */

/// @brief 円柱を貫通するレイが2交点を返し、t昇順に並ぶ
TEST(IntersectSurfaceWithLineTest, Cylinder_TwoHits) {
    const auto cylinder = MakeCylinder();
    // p0=(-10,0,2), p1=(0,0,2): +x方向のレイ、高さ z=2
    const auto hits = IntersectSurfaceWithLine(
        *cylinder,
        Vector3d{-10., 0., 2.}, Vector3d{0., 0., 2.},
        LineType::kRay, MakeParams());

    ASSERT_EQ(hits.size(), 2u);
    // t=0.8 → (-2, 0, 2), t=1.2 → (2, 0, 2)
    EXPECT_LT(hits[0].t, hits[1].t);
    ExpectPositionNear(hits[0].position, Vector3d{-2., 0., 2.});
    ExpectPositionNear(hits[1].position, Vector3d{ 2., 0., 2.});
}

/// @brief BBとの事前判定で外れる場合は空リスト
TEST(IntersectSurfaceWithLineTest, Cylinder_NoHit_BBoxReject) {
    const auto cylinder = MakeCylinder();
    // y方向にずれたレイ (円柱のBBの外を通る)
    const auto hits = IntersectSurfaceWithLine(
        *cylinder,
        Vector3d{-10., 10., 2.}, Vector3d{0., 10., 2.},
        LineType::kRay, MakeParams());

    EXPECT_TRUE(hits.empty());
}



/**
 * 重複除去テスト
 */

/// @brief 複数の初期点が同一の収束解を返した場合、1点に重複除去される
TEST(IntersectSurfaceWithLineTest, Dedup_SameSolution) {
    const auto plane = MakeYFivePlane();
    // サンプル数を増やして同一解が多数生成されても重複除去後は1点
    SurfaceLineIntersectionParams p = MakeParams(50);
    const auto hits = IntersectSurfaceWithLine(
        *plane, Vector3d{0., 0., 0.}, Vector3d{0., 1., 0.},
        LineType::kRay, p);

    EXPECT_EQ(hits.size(), 1u);
}



/**
 * Phase 0-1 検証: 非単位変換行列を持つ曲面との交差
 *
 * TryGetDerivativesは定義空間を返すが、IntersectSurfaceWithLineは
 * ワールド空間のレイと交差判定する。変換行列が非単位の場合でも
 * 正確な交点座標が得られることを確認する。
 */

/// @brief y=0 平面に並進変換 T=(0,5,0) を適用すると y=5 で交差する
TEST(IntersectSurfaceWithLineTest, WithTransform_Translation) {
    auto plane = MakeYZeroPlane();

    // 並進 T=(0,5,0): 定義空間の y=0 がワールド空間の y=5 になる
    auto trans = i_ent::MakeTranslation(Vector3d{0., 5., 0.});
    plane->OverwriteTransformationMatrix(trans);

    // +y方向のレイ: ワールド空間 y=5 で交差するはず
    const auto hits = IntersectSurfaceWithLine(
        *plane, Vector3d{0., 0., 0.}, Vector3d{0., 1., 0.},
        LineType::kRay, MakeParams());

    ASSERT_EQ(hits.size(), 1u);
    ExpectPositionNear(hits[0].position, Vector3d{0., 5., 0.});
    EXPECT_NEAR(hits[0].t, 5.0, kPosTol);
}

/// @brief y=0 平面に z軸回りの90°回転を適用すると x=0 の平面になり交差位置が変わる
TEST(IntersectSurfaceWithLineTest, WithTransform_Rotation) {
    auto plane = MakeYZeroPlane();

    // z軸回りに90°回転: (x,0,z) → (0,x,z)  → ワールド空間の x=0 平面
    Matrix3d rz90;
    rz90 <<  0., -1., 0.,
             1.,  0., 0.,
             0.,  0., 1.;
    auto trans = i_ent::MakeTransformationMatrix(rz90);
    plane->OverwriteTransformationMatrix(trans);

    // +x → -x方向のレイ: ワールド空間 x=0 で交差するはず
    const auto hits = IntersectSurfaceWithLine(
        *plane, Vector3d{1., 0., 0.}, Vector3d{0., 0., 0.},
        LineType::kRay, MakeParams());

    ASSERT_EQ(hits.size(), 1u);
    ExpectPositionNear(hits[0].position, Vector3d{0., 0., 0.});
    EXPECT_NEAR(hits[0].t, 1.0, kPosTol);
}
