/**
 * @file entities/curves/test_curve_line_intersection.cpp
 * @brief curve_line_intersection.h のテスト
 * @author Yayoi Habami
 * @date 2026-05-27
 * @copyright 2026 Yayoi Habami
 *
 * ### 対象関数
 * - igesio::entities::IntersectCurveWithLine
 * - igesio::entities::IntersectPointWithLine
 *
 * TODO: NURBS曲線・CompositeCurve（複数区間）での検証は未実装
 * TODO: 無限パラメータ範囲を持つ曲線（kRay/kLineのLine）の直接検証は
 *       クランプ値がBBサイズ依存のため未実装
 */
#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <memory>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/entities/curves/line.h"
#include "igesio/entities/curves/circular_arc.h"
#include "igesio/entities/transformations/transformation_matrix.h"
#include "igesio/entities/curves/algorithms/curve_line_intersection.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using i_ent::CurveLineIntersection;
using i_ent::CurveLineIntersectionParams;
using i_ent::IntersectCurveWithLine;
using i_ent::IntersectPointWithLine;
using igesio::Vector2d;
using igesio::Vector3d;
using LineType = i_num::BoundingBox::DirectionType;

/// @brief 位置比較の許容誤差
constexpr double kPosTol = 1e-6;

/// @brief 交点位置が期待値と一致するか検証するヘルパー
void ExpectPositionNear(const Vector3d& actual, const Vector3d& expected,
                        const double tol = kPosTol) {
    EXPECT_NEAR(actual.x(), expected.x(), tol);
    EXPECT_NEAR(actual.y(), expected.y(), tol);
    EXPECT_NEAR(actual.z(), expected.z(), tol);
}

/// @brief z軸方向の線分 (0,0,-5)→(0,0,5) (定義空間=ワールド空間)
/// @note C(t) = (0, 0, -5 + 10t), t∈[0,1]. z=0 で t=0.5
std::shared_ptr<i_ent::Line> MakeZSegment() {
    return i_ent::MakeLine(
        Vector3d{0., 0., -5.}, Vector3d{0., 0., 5.},
        i_ent::LineType::kSegment);
}

/// @brief 原点中心・半径2・z=0平面上の単位円 (CircularArc, 閉曲線)
/// @note C(θ) = (2cosθ, 2sinθ, 0), θ∈[0, 2π]
std::shared_ptr<i_ent::CircularArc> MakeCircleR2() {
    return i_ent::MakeCircle(Vector2d{0., 0.}, 2.0, 0.0);
}

/// @brief テスト用の探索パラメータ (hit_toleranceを指定)
CurveLineIntersectionParams MakeParams(const double hit_tol,
                                       const int n = 20) {
    CurveLineIntersectionParams p;
    p.curve_samples = n;
    p.hit_tolerance = hit_tol;
    return p;
}

}  // namespace



/**
 * 直線エンティティとの近接判定（厳密交差・近接）
 */

/// @brief x軸レイがz軸線分を貫通する場合、gap≈0で交差点を返す
TEST(IntersectCurveWithLineTest, LineHit_RayCrossing) {
    const auto seg = MakeZSegment();
    // y=0,z=0 を通る x方向のレイ → 原点で線分と交差
    const auto hits = IntersectCurveWithLine(
        *seg, Vector3d{10., 0., 0.}, Vector3d{-10., 0., 0.},
        LineType::kRay, MakeParams(1.0));

    ASSERT_EQ(hits.size(), 1u);
    ExpectPositionNear(hits[0].position, Vector3d{0., 0., 0.});
    EXPECT_NEAR(hits[0].t_curve, 0.5, kPosTol);
    EXPECT_NEAR(hits[0].gap, 0.0, kPosTol);
}

/// @brief y方向にずれたレイは線分と交差しないが、許容内なら近接点を返す
TEST(IntersectCurveWithLineTest, LineHit_SkewWithinTolerance) {
    const auto seg = MakeZSegment();
    // y=2,z=0 を通る x方向のレイ. 線分との最近接距離は2
    const auto hits = IntersectCurveWithLine(
        *seg, Vector3d{10., 2., 0.}, Vector3d{-10., 2., 0.},
        LineType::kRay, MakeParams(3.0));

    ASSERT_EQ(hits.size(), 1u);
    ExpectPositionNear(hits[0].position, Vector3d{0., 0., 0.});
    EXPECT_NEAR(hits[0].gap, 2.0, kPosTol);
}

/// @brief 最近接距離が許容値を超える場合は空リスト
TEST(IntersectCurveWithLineTest, LineNoHit_GapExceedsTolerance) {
    const auto seg = MakeZSegment();
    // 最近接距離2 > hit_tolerance=1
    const auto hits = IntersectCurveWithLine(
        *seg, Vector3d{10., 2., 0.}, Vector3d{-10., 2., 0.},
        LineType::kRay, MakeParams(1.0));

    EXPECT_TRUE(hits.empty());
}



/**
 * 円弧エンティティとの交差判定
 */

/// @brief x軸レイが円を貫通し、2交点をt_line昇順で返す
TEST(IntersectCurveWithLineTest, Circle_TwoHits) {
    const auto circle = MakeCircleR2();
    // x軸方向のレイ: 円と (2,0,0), (-2,0,0) で交差
    const auto hits = IntersectCurveWithLine(
        *circle, Vector3d{10., 0., 0.}, Vector3d{-10., 0., 0.},
        LineType::kRay, MakeParams(0.5));

    ASSERT_EQ(hits.size(), 2u);
    EXPECT_LT(hits[0].t_line, hits[1].t_line);
    ExpectPositionNear(hits[0].position, Vector3d{2., 0., 0.});
    ExpectPositionNear(hits[1].position, Vector3d{-2., 0., 0.});
    EXPECT_NEAR(hits[0].gap, 0.0, kPosTol);
    EXPECT_NEAR(hits[1].gap, 0.0, kPosTol);
}

/// @brief 円の外側を通るレイは、許容内なら最近接の1点を返す
TEST(IntersectCurveWithLineTest, Circle_NearMissWithinTolerance) {
    const auto circle = MakeCircleR2();
    // y=3,z=0 を通る x方向のレイ. 円(半径2)との最近接は (0,2,0), gap=1
    const auto hits = IntersectCurveWithLine(
        *circle, Vector3d{10., 3., 0.}, Vector3d{-10., 3., 0.},
        LineType::kRay, MakeParams(1.5));

    ASSERT_EQ(hits.size(), 1u);
    ExpectPositionNear(hits[0].position, Vector3d{0., 2., 0.});
    EXPECT_NEAR(hits[0].gap, 1.0, kPosTol);
}



/**
 * 線種制約のテスト
 */

/// @brief 垂線の足が線分外（s>1）にある交差はkSegmentでは返されない
TEST(IntersectCurveWithLineTest, Circle_SegmentFeetOutside_Empty) {
    const auto circle = MakeCircleR2();
    // 線分 (10,0,0)→(5,0,0): 円の交点(±2,0,0)への射影sは1を超える
    const auto hits = IntersectCurveWithLine(
        *circle, Vector3d{10., 0., 0.}, Vector3d{5., 0., 0.},
        LineType::kSegment, MakeParams(0.5));

    EXPECT_TRUE(hits.empty());
}

/// @brief 円が背後（s<0）にあるkRayは交差を返さない
TEST(IntersectCurveWithLineTest, Circle_RayPointingAway_Empty) {
    const auto circle = MakeCircleR2();
    // p0=(10,0,0) から +x方向のレイ: 円は背後 (s<0)
    const auto hits = IntersectCurveWithLine(
        *circle, Vector3d{10., 0., 0.}, Vector3d{20., 0., 0.},
        LineType::kRay, MakeParams(0.5));

    EXPECT_TRUE(hits.empty());
}

/// @brief kLineでは線分範囲外（s>1）の交点も検出される
TEST(IntersectCurveWithLineTest, Circle_LineDetectsBothSides) {
    const auto circle = MakeCircleR2();
    // kLineなら(10,0,0)→(5,0,0)方向の無限直線で両交点を検出
    const auto hits = IntersectCurveWithLine(
        *circle, Vector3d{10., 0., 0.}, Vector3d{5., 0., 0.},
        LineType::kLine, MakeParams(0.5));

    ASSERT_EQ(hits.size(), 2u);
}



/**
 * 座標空間の検証（変換行列あり）
 */

/// @brief z=0の円に並進T=(0,0,5)を適用すると z=5 でレイが交差する
TEST(IntersectCurveWithLineTest, WithTransform_HitsInWorldSpace) {
    auto circle = MakeCircleR2();
    auto trans = i_ent::MakeTranslation(Vector3d{0., 0., 5.});
    circle->OverwriteTransformationMatrix(trans);

    // z=5 を通る x方向レイ → ワールド空間 z=5 の円と交差
    const auto hits = IntersectCurveWithLine(
        *circle, Vector3d{10., 0., 5.}, Vector3d{-10., 0., 5.},
        LineType::kRay, MakeParams(0.5));

    ASSERT_EQ(hits.size(), 2u);
    ExpectPositionNear(hits[0].position, Vector3d{2., 0., 5.});
    ExpectPositionNear(hits[1].position, Vector3d{-2., 0., 5.});
}

/// @brief 並進後はz=0平面のレイは円に届かず空リスト
TEST(IntersectCurveWithLineTest, WithTransform_MissesAtDefinedSpace) {
    auto circle = MakeCircleR2();
    auto trans = i_ent::MakeTranslation(Vector3d{0., 0., 5.});
    circle->OverwriteTransformationMatrix(trans);

    // z=0 のレイ: ワールド円は z=5 にあるため最近接距離5で空
    const auto hits = IntersectCurveWithLine(
        *circle, Vector3d{10., 0., 0.}, Vector3d{-10., 0., 0.},
        LineType::kRay, MakeParams(1.0));

    EXPECT_TRUE(hits.empty());
}



/**
 * 重複除去・広域棄却・エラー
 */

/// @brief サンプル数を増やしても同一交点は重複除去され2点に収まる
TEST(IntersectCurveWithLineTest, Dedup_SameSolution) {
    const auto circle = MakeCircleR2();
    const auto hits = IntersectCurveWithLine(
        *circle, Vector3d{10., 0., 0.}, Vector3d{-10., 0., 0.},
        LineType::kRay, MakeParams(0.5, 60));

    EXPECT_EQ(hits.size(), 2u);
}

/// @brief 遠方を通るレイは広域棄却で空リストになる
TEST(IntersectCurveWithLineTest, BroadPhase_FarLineRejected) {
    const auto circle = MakeCircleR2();
    // x=100 を通る直線: 円(最大x=2)から遠く離れている
    const auto hits = IntersectCurveWithLine(
        *circle, Vector3d{100., 100., 0.}, Vector3d{100., -100., 0.},
        LineType::kLine, MakeParams(1.0));

    EXPECT_TRUE(hits.empty());
}

/// @brief 方向ベクトルがゼロ (p0==p1) の場合は空リストを返す
TEST(IntersectCurveWithLineTest, EmptyResult_ZeroDirection) {
    const auto circle = MakeCircleR2();
    const auto hits = IntersectCurveWithLine(
        *circle, Vector3d{0., 0., 0.}, Vector3d{0., 0., 0.},
        LineType::kRay, MakeParams(1.0));

    EXPECT_TRUE(hits.empty());
}

/// @brief p0にNaNが含まれる場合はstd::invalid_argumentを投げる
TEST(IntersectCurveWithLineTest, Throws_InvalidArg_NaN) {
    const auto circle = MakeCircleR2();
    EXPECT_THROW(
        IntersectCurveWithLine(
            *circle,
            Vector3d{std::numeric_limits<double>::quiet_NaN(), 0., 0.},
            Vector3d{0., 1., 0.},
            LineType::kRay),
        std::invalid_argument);
}



/**
 * 点と直線の近接判定 (IntersectPointWithLine)
 */

/// @brief 点がレイ上にある場合、gap≈0で点とt_lineを返す
TEST(IntersectPointWithLineTest, PointOnRay_GapZero) {
    // 点(0,0,0)、x軸方向のレイ (10,0,0)→(9,0,0); 単位方向なのでt_line=距離
    const auto hit = IntersectPointWithLine(
        Vector3d{0., 0., 0.}, Vector3d{10., 0., 0.}, Vector3d{9., 0., 0.},
        LineType::kRay, 1.0);

    ASSERT_TRUE(hit.has_value());
    ExpectPositionNear(hit->position, Vector3d{0., 0., 0.});
    EXPECT_NEAR(hit->gap, 0.0, kPosTol);
    EXPECT_NEAR(hit->t_line, 10.0, kPosTol);
}

/// @brief レイからずれた点も許容内なら近接として返す (gapが正しい)
TEST(IntersectPointWithLineTest, PointSkew_WithinTolerance) {
    // 点(0,2,0)、x軸 (z=0,y=0) のレイ. 最近接距離は2
    const auto hit = IntersectPointWithLine(
        Vector3d{0., 2., 0.}, Vector3d{10., 0., 0.}, Vector3d{9., 0., 0.},
        LineType::kRay, 3.0);

    ASSERT_TRUE(hit.has_value());
    EXPECT_NEAR(hit->gap, 2.0, kPosTol);
}

/// @brief gapが許容値を超える場合はnulloptを返す
TEST(IntersectPointWithLineTest, NoHit_GapExceedsTolerance) {
    const auto hit = IntersectPointWithLine(
        Vector3d{0., 2., 0.}, Vector3d{10., 0., 0.}, Vector3d{9., 0., 0.},
        LineType::kRay, 1.0);

    EXPECT_FALSE(hit.has_value());
}

/// @brief kRayで点が始点より後方 (s<0) の場合はnulloptを返す
TEST(IntersectPointWithLineTest, NoHit_BehindRayOrigin) {
    // レイは(0,0,0)から+x方向. 点(-5,0,0)はs<0 (後方)
    const auto hit = IntersectPointWithLine(
        Vector3d{-5., 0., 0.}, Vector3d{0., 0., 0.}, Vector3d{1., 0., 0.},
        LineType::kRay, 1.0);

    EXPECT_FALSE(hit.has_value());
}

/// @brief kSegmentで射影が線分外 (s>1) の場合はnulloptを返す
TEST(IntersectPointWithLineTest, NoHit_OutsideSegment) {
    // 線分(0,0,0)→(1,0,0). 点(5,0,0)はs=5>1
    const auto hit = IntersectPointWithLine(
        Vector3d{5., 0., 0.}, Vector3d{0., 0., 0.}, Vector3d{1., 0., 0.},
        LineType::kSegment, 1.0);

    EXPECT_FALSE(hit.has_value());
}

/// @brief 方向ベクトルがゼロ (p0==p1) の場合はnulloptを返す
TEST(IntersectPointWithLineTest, NoHit_ZeroDirection) {
    const auto hit = IntersectPointWithLine(
        Vector3d{0., 0., 0.}, Vector3d{1., 1., 1.}, Vector3d{1., 1., 1.},
        LineType::kRay, 1.0);

    EXPECT_FALSE(hit.has_value());
}

/// @brief pointにNaNが含まれる場合はstd::invalid_argumentを投げる
TEST(IntersectPointWithLineTest, Throws_InvalidArg_NaN) {
    EXPECT_THROW(
        IntersectPointWithLine(
            Vector3d{std::numeric_limits<double>::quiet_NaN(), 0., 0.},
            Vector3d{0., 0., 0.}, Vector3d{1., 0., 0.},
            LineType::kRay, 1.0),
        std::invalid_argument);
}
