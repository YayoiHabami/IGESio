/**
 * @file entities/curves/test_parametric_spline_curve.cpp
 * @brief ParametricSplineCurve の GetLinearSegments / GetCornerParams のテスト
 * @author Yayoi Habami
 * @date 2026-04-10
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "igesio/entities/curves/parametric_spline_curve.h"

namespace {

namespace i_ent = igesio::entities;
using i_ent::ParametricSplineCurve;

/// @brief テストの許容誤差
constexpr double kTol = 1e-9;

/// @brief CTYPE=1 (kLinear), H=1, NDIM=3, N=2 の折れ線スプライン
/// @details ブレークポイント [0, 1, 2]、幾何形状 (0,0,0)→(1,0,0)→(1,1,0).
///          GetLinearSegments は [{0,1},{1,2}]、GetCornerParams は [] を返すべき
std::shared_ptr<ParametricSplineCurve> MakeLinearSpline() {
    const auto param = igesio::IGESParameterVector{
        1, 1, 3, 2,          // CTYPE=1 (kLinear), H=1, NDIM=3, N=2
        0.0, 1.0, 2.0,       // T(1), T(2), T(3)
        // セグメント1: C(s)=(s, 0, 0)  (s = u - T(1) = u)
        0.0, 1.0, 0.0, 0.0,  // Ax, Bx, Cx, Dx
        0.0, 0.0, 0.0, 0.0,  // Ay, By, Cy, Dy
        0.0, 0.0, 0.0, 0.0,  // Az, Bz, Cz, Dz
        // セグメント2: C(s)=(1, s, 0)  (s = u - T(2) = u - 1)
        1.0, 0.0, 0.0, 0.0,  // Ax, Bx, Cx, Dx
        0.0, 1.0, 0.0, 0.0,  // Ay, By, Cy, Dy
        0.0, 0.0, 0.0, 0.0,  // Az, Bz, Cz, Dz
        // 末端点 u=T(3)=2, s=1: C=(1,1,0), C'=(0,1,0), C''=C'''=(0,0,0)
        1.0, 0.0, 0.0, 0.0,  // TPX0, TPX1, TPX2, TPX3
        1.0, 1.0, 0.0, 0.0,  // TPY0, TPY1, TPY2, TPY3
        0.0, 0.0, 0.0, 0.0   // TPZ0, TPZ1, TPZ2, TPZ3
    };
    return std::make_shared<ParametricSplineCurve>(param);
}

/// @brief CTYPE=3 (kCubic), H=3, NDIM=3, N=2 の3次スプライン
/// @details 幾何形状は MakeLinearSpline と同じだが CTYPE が kLinear でない.
///          GetLinearSegments は [] を返すべき
std::shared_ptr<ParametricSplineCurve> MakeCubicSpline() {
    const auto param = igesio::IGESParameterVector{
        3, 3, 3, 2,          // CTYPE=3 (kCubic), H=3, NDIM=3, N=2
        0.0, 1.0, 2.0,       // T(1), T(2), T(3)
        // セグメント1
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        // セグメント2
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        // 末端点
        1.0, 0.0, 0.0, 0.0,
        1.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0
    };
    return std::make_shared<ParametricSplineCurve>(param);
}

/// @brief CTYPE=3 (kCubic), H=0, NDIM=3, N=2 の区分定数スプライン
/// @details ブレークポイント [0, 1, 2]、B=C=D=0 のため C'=0 が常に成立.
///          GetCornerParams は内部ブレークポイント [1.0] を返すべき
std::shared_ptr<ParametricSplineCurve> MakeDegree0Spline() {
    const auto param = igesio::IGESParameterVector{
        3, 0, 3, 2,          // CTYPE=3 (kCubic), H=0, NDIM=3, N=2
        0.0, 1.0, 2.0,       // T(1), T(2), T(3)
        // セグメント1: 定数 (0, 0, 0)
        0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        // セグメント2: 定数 (1, 0, 0)
        1.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        // 末端点 u=T(3)=2, s=1: C=(1,0,0), C'=C''=C'''=(0,0,0)
        1.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0
    };
    return std::make_shared<ParametricSplineCurve>(param);
}

}  // namespace



/// @brief `GetLinearSegments` / `IsInLinearSegment` のテスト
class ParametricSplineCurveLinearSegmentTest : public ::testing::Test {};

/// @brief kLinear 型 (CTYPE=1) は全ブレークポイント区間を返す
TEST(ParametricSplineCurveLinearSegmentTest, LinearType_TwoSegments) {
    auto spline = MakeLinearSpline();
    const auto segs = spline->GetLinearSegments();
    ASSERT_EQ(segs.size(), 2u);
    EXPECT_NEAR(segs[0][0], 0.0, kTol);
    EXPECT_NEAR(segs[0][1], 1.0, kTol);
    EXPECT_NEAR(segs[1][0], 1.0, kTol);
    EXPECT_NEAR(segs[1][1], 2.0, kTol);
}

/// @brief kLinear 以外 (CTYPE=3) は空リストを返す
TEST(ParametricSplineCurveLinearSegmentTest, CubicType_EmptySegments) {
    auto spline = MakeCubicSpline();
    EXPECT_TRUE(spline->GetLinearSegments().empty());
}

/// @brief セグメント内部の点は IsInLinearSegment が true
TEST(ParametricSplineCurveLinearSegmentTest, LinearType_IsInLinearSegment_Interior) {
    auto spline = MakeLinearSpline();
    EXPECT_TRUE(spline->IsInLinearSegment(0.5));
    EXPECT_TRUE(spline->IsInLinearSegment(1.5));
}

/// @brief 内部ブレークポイント (セグメント境界) は両セグメントの端点として true
TEST(ParametricSplineCurveLinearSegmentTest, LinearType_IsInLinearSegment_AtBreakpoint) {
    auto spline = MakeLinearSpline();
    // t=1.0 は Seg1 の終点かつ Seg2 の始点
    EXPECT_TRUE(spline->IsInLinearSegment(1.0));
}

/// @brief パラメータ範囲外は false
TEST(ParametricSplineCurveLinearSegmentTest, LinearType_IsInLinearSegment_OutOfRange) {
    auto spline = MakeLinearSpline();
    EXPECT_FALSE(spline->IsInLinearSegment(2.5));
    EXPECT_FALSE(spline->IsInLinearSegment(-0.1));
}



/// @brief `GetCornerParams` / `IsCorner` のテスト
class ParametricSplineCurveCornerTest : public ::testing::Test {};

/// @brief H=0 (degree=0) では内部ブレークポイントが角点
TEST(ParametricSplineCurveCornerTest, Degree0_OneInternalBreakpoint) {
    auto spline = MakeDegree0Spline();
    const auto corners = spline->GetCornerParams();
    ASSERT_EQ(corners.size(), 1u);
    EXPECT_NEAR(corners[0], 1.0, kTol);
}

/// @brief H=1 では角点なし
TEST(ParametricSplineCurveCornerTest, Degree1_Empty) {
    auto spline = MakeLinearSpline();
    EXPECT_TRUE(spline->GetCornerParams().empty());
}

/// @brief H=3 では角点なし
TEST(ParametricSplineCurveCornerTest, Degree3_Empty) {
    auto spline = MakeCubicSpline();
    EXPECT_TRUE(spline->GetCornerParams().empty());
}

/// @brief H=0 において始点・終点は角点でなく、内部ブレークポイントのみが角点
TEST(ParametricSplineCurveCornerTest, Degree0_EndpointKnots_NotCorners) {
    auto spline = MakeDegree0Spline();
    EXPECT_FALSE(spline->IsCorner(0.0));
    EXPECT_FALSE(spline->IsCorner(2.0));
    EXPECT_TRUE(spline->IsCorner(1.0));
}
