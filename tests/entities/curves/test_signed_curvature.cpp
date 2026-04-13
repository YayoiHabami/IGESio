/**
 * @file entities/curves/test_signed_curvature.cpp
 * @brief ICurve::CornerExteriorAngle と ICurve::TryGetSignedCurvature のテスト.
 *        デフォルト実装 (ICurve) レベルでの挙動を検証する
 * @author Yayoi Habami
 * @date 2026-04-10
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <vector>

#include "igesio/entities/curves/circular_arc.h"
#include "igesio/entities/curves/line.h"
#include "igesio/entities/curves/linear_path.h"
#include "igesio/entities/curves/parametric_spline_curve.h"
#include "igesio/numerics/matrix.h"

namespace {

namespace i_ent = igesio::entities;
using i_ent::CircularArc;
using i_ent::Line;
using i_ent::LinearPath;
using i_ent::ParametricSplineCurve;
using igesio::Vector2d;
using igesio::Vector3d;

/// @brief テストの許容誤差
constexpr double kTol = 1e-9;

/// @brief 角度の許容誤差 (数値近似による誤差を含む)
constexpr double kAngleTol = 1e-5;

/// @brief 非ループ LinearPath: (0,0)→(1,0)→(1,1)
/// @details 内部頂点 t=1 での外角 = +π/2 (法線(0,0,1)基準では左折り)
std::shared_ptr<LinearPath> MakeLeftTurnLP() {
    return std::make_shared<LinearPath>(
        std::vector<Vector2d>{{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}}, false);
}

/// @brief 非ループ LinearPath: (0,0)→(1,0)→(1,-1)
/// @details 内部頂点 t=1 での外角 = -π/2 (法線(0,0,1)基準では右折り)
std::shared_ptr<LinearPath> MakeRightTurnLP() {
    return std::make_shared<LinearPath>(
        std::vector<Vector2d>{{0.0, 0.0}, {1.0, 0.0}, {1.0, -1.0}}, false);
}

/// @brief 非ループ LinearPath: (0,0)→(1,0)→(2,0)
/// @details 内部頂点 t=1 での外角 = 0 (直線通過)
std::shared_ptr<LinearPath> MakeStraightLP() {
    return std::make_shared<LinearPath>(
        std::vector<Vector2d>{{0.0, 0.0}, {1.0, 0.0}, {2.0, 0.0}}, false);
}

/// @brief 非ループ LinearPath: (0,0)→(1,0)→(0,0)
/// @details 内部頂点 t=1 での外角 = π (atan2(0,-1); 完全折り返し).
///          TryGetSignedCurvature は +∞ を返す
std::shared_ptr<LinearPath> MakeUTurnLP() {
    return std::make_shared<LinearPath>(
        std::vector<Vector2d>{{0.0, 0.0}, {1.0, 0.0}, {0.0, 0.0}}, false);
}

/// @brief 半径 R=1.5 の全周円 (CCW 方向)
/// @details TryGetDerivatives は CircularArc が提供し、
///          LeftTangentAt / RightTangentAt は ICurve のデフォルト実装を使用する
std::shared_ptr<CircularArc> MakeCircle() {
    return std::make_shared<CircularArc>(
        Vector2d{-0.75, 0.0}, 1.5);
}

/// @brief CTYPE=3 (kCubic), H=0, NDIM=3, N=2 の区分定数スプライン
/// @details C'=0 のため速度がゼロ. 非角点・非直線部での TryGetSignedCurvature は nullopt
std::shared_ptr<ParametricSplineCurve> MakeDegree0Spline() {
    const auto param = igesio::IGESParameterVector{
        3, 0, 3, 2,
        0.0, 1.0, 2.0,
        0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        1.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        1.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0
    };
    return std::make_shared<ParametricSplineCurve>(param);
}

}  // namespace



/// @brief `ICurve::CornerExteriorAngle` のテスト
class CornerExteriorAngleTest : public ::testing::Test {};

/// @brief 左折り角点: T⁻=(1,0,0), T⁺=(0,1,0) → 外角 +π/2
TEST(CornerExteriorAngleTest, LeftTurn_PositivePiHalf) {
    const auto lp = MakeLeftTurnLP();
    const Vector3d n(0.0, 0.0, 1.0);
    const auto angle = lp->CornerExteriorAngle(1.0, n);
    ASSERT_TRUE(angle.has_value());
    EXPECT_NEAR(*angle, igesio::kPi / 2.0, kTol);
}

/// @brief 右折り角点: T⁻=(1,0,0), T⁺=(0,-1,0) → 外角 -π/2
TEST(CornerExteriorAngleTest, RightTurn_NegativePiHalf) {
    const auto lp = MakeRightTurnLP();
    const Vector3d n(0.0, 0.0, 1.0);
    const auto angle = lp->CornerExteriorAngle(1.0, n);
    ASSERT_TRUE(angle.has_value());
    EXPECT_NEAR(*angle, -igesio::kPi / 2.0, kTol);
}

/// @brief 直線通過の角点: T⁻=T⁺=(1,0,0) → 外角 0
TEST(CornerExteriorAngleTest, StraightThrough_Zero) {
    const auto lp = MakeStraightLP();
    const Vector3d n(0.0, 0.0, 1.0);
    const auto angle = lp->CornerExteriorAngle(1.0, n);
    ASSERT_TRUE(angle.has_value());
    EXPECT_NEAR(*angle, 0.0, kTol);
}

/// @brief 完全折り返し: T⁻=(1,0,0), T⁺=(-1,0,0) → atan2(0,-1) = π
/// @details cross = T⁻×T⁺ = (0,0,0), dot = -1 のため atan2(0,-1) = π > 0.
///          TryGetSignedCurvature は +∞ を返すことを合わせて確認する
TEST(CornerExteriorAngleTest, UTurn_Pi) {
    const auto lp = MakeUTurnLP();
    const Vector3d n(0.0, 0.0, 1.0);
    const auto angle = lp->CornerExteriorAngle(1.0, n);
    ASSERT_TRUE(angle.has_value());
    EXPECT_NEAR(*angle, igesio::kPi, kTol);
    // TryGetSignedCurvature: 外角 π > 0 → +∞
    const auto kappa = lp->TryGetSignedCurvature(1.0, n);
    ASSERT_TRUE(kappa.has_value());
    EXPECT_EQ(*kappa, std::numeric_limits<double>::infinity());
}

/// @brief 法線反転: 左折りで法線が (0,0,-1) → 外角 -π/2
TEST(CornerExteriorAngleTest, NormalFlipped_SignFlipped) {
    const auto lp = MakeLeftTurnLP();
    const Vector3d n(0.0, 0.0, -1.0);
    const auto angle = lp->CornerExteriorAngle(1.0, n);
    ASSERT_TRUE(angle.has_value());
    EXPECT_NEAR(*angle, -igesio::kPi / 2.0, kTol);
}

/// @brief 法線がゼロベクトル → nullopt
TEST(CornerExteriorAngleTest, ZeroNormal_Nullopt) {
    const auto lp = MakeLeftTurnLP();
    const Vector3d zero(0.0, 0.0, 0.0);
    EXPECT_FALSE(lp->CornerExteriorAngle(1.0, zero).has_value());
}

/// @brief 非単位法線: n=(0,0,2) は内部で正規化されるため n=(0,0,1) と同じ結果
TEST(CornerExteriorAngleTest, NonUnitNormal_SameResult) {
    const auto lp = MakeLeftTurnLP();
    const Vector3d n_unit(0.0, 0.0, 1.0);
    const Vector3d n_scaled(0.0, 0.0, 2.0);
    const auto a_unit   = lp->CornerExteriorAngle(1.0, n_unit);
    const auto a_scaled = lp->CornerExteriorAngle(1.0, n_scaled);
    ASSERT_TRUE(a_unit.has_value());
    ASSERT_TRUE(a_scaled.has_value());
    EXPECT_NEAR(*a_unit, *a_scaled, kTol);
}



/// @brief ICurve デフォルト実装の `LeftTangentAt` / `RightTangentAt` のテスト
/// @details CircularArc はこれらをオーバーライドしないため、ICurve の数値近似が用いられる
class DefaultTangentAtTest : public ::testing::Test {};

/// @brief 滑らかな曲線の内部点では Left ≈ Right (h=1e-7 近似による差異は微小)
TEST(DefaultTangentAtTest, SmoothCurve_Interior_LeftApproxRight) {
    const auto circle = MakeCircle();
    const double t_mid = igesio::kPi;
    const auto left  = circle->LeftTangentAt(t_mid);
    const auto right = circle->RightTangentAt(t_mid);
    ASSERT_TRUE(left.has_value());
    ASSERT_TRUE(right.has_value());
    // 両者の方向差が微小であることを内積で確認
    EXPECT_NEAR(left->dot(*right), 1.0, kAngleTol);
}

/// @brief 始点では LeftTangentAt が nullopt でなく、始点での接線を返す
/// @details デフォルト実装は t_eval = max(t-h, t_start) = t_start にクランプする.
///          これは非ループ LinearPath の始点で nullopt を返す解析実装とは異なる
TEST(DefaultTangentAtTest, StartParam_LeftIsNotNullopt) {
    const auto circle = MakeCircle();
    const double t_start = circle->GetParameterRange()[0];
    const auto left  = circle->LeftTangentAt(t_start);
    const auto right = circle->RightTangentAt(t_start);
    ASSERT_TRUE(left.has_value());
    ASSERT_TRUE(right.has_value());
    // クランプ後は同じ点で評価されるため、方向もほぼ一致する
    EXPECT_NEAR(left->dot(*right), 1.0, kAngleTol);
}

/// @brief 終点でも RightTangentAt が nullopt でなく、終点での接線を返す
TEST(DefaultTangentAtTest, EndParam_RightIsNotNullopt) {
    const auto circle = MakeCircle();
    const double t_end = circle->GetParameterRange()[1];
    const auto left  = circle->LeftTangentAt(t_end);
    const auto right = circle->RightTangentAt(t_end);
    ASSERT_TRUE(left.has_value());
    ASSERT_TRUE(right.has_value());
    EXPECT_NEAR(left->dot(*right), 1.0, kAngleTol);
}



/// @brief `ICurve::TryGetSignedCurvature` の一般点分岐のテスト
/// @details 角点・直線部以外の点では c1×c2·n̂ / |c1|³ で曲率を計算する
class SignedCurvatureGeneralTest : public ::testing::Test {};

/// @brief 半径 R の円では κ_s = +1/R (CCW、法線(0,0,1))
/// @details c1=(−R sinθ, R cosθ, 0), c2=(−R cosθ, −R sinθ, 0)
///          → c1×c2 = (0,0,R²), |c1|³=R³, κ_s = R²/R³ = 1/R
TEST(SignedCurvatureGeneralTest, Circle_Positive) {
    const auto circle = MakeCircle();
    constexpr double kR = 1.5;
    const Vector3d n(0.0, 0.0, 1.0);
    // 内部点 θ=π/4 で検証
    const auto kappa = circle->TryGetSignedCurvature(igesio::kPi / 4.0, n);
    ASSERT_TRUE(kappa.has_value());
    EXPECT_NEAR(*kappa, 1.0 / kR, kTol);
}

/// @brief 法線を反転すると κ_s の符号も反転する
TEST(SignedCurvatureGeneralTest, Circle_Negative) {
    const auto circle = MakeCircle();
    constexpr double kR = 1.5;
    const Vector3d n(0.0, 0.0, -1.0);
    const auto kappa = circle->TryGetSignedCurvature(igesio::kPi / 4.0, n);
    ASSERT_TRUE(kappa.has_value());
    EXPECT_NEAR(*kappa, -1.0 / kR, kTol);
}

/// @brief Line エンティティの内部点: C''=0 → κ_s = 0
/// @details Line は GetLinearSegments をオーバーライドしないため非直線部扱いとなり、
///          一般分岐 (c1×c2)/|c1|³ を経由して 0 が返る
TEST(SignedCurvatureGeneralTest, LineSegment_ZeroCurvature) {
    const auto line = std::make_shared<Line>(
        Vector3d{0.0, -1.0, 0.0}, Vector3d{1.0, 1.0, 0.0},
        i_ent::LineType::kSegment);
    const Vector3d n(0.0, 0.0, 1.0);
    const auto kappa = line->TryGetSignedCurvature(0.5, n);
    ASSERT_TRUE(kappa.has_value());
    EXPECT_NEAR(*kappa, 0.0, kTol);
}

/// @brief C'=0 の縮退点 (速度ゼロ) では nullopt
/// @details 区分定数スプライン (H=0) の非角点内部: IsCorner=false, IsInLinearSegment=false,
///          かつ |C'|³ < 1e-14 のため nullopt が返る
TEST(SignedCurvatureGeneralTest, ZeroSpeed_Nullopt) {
    const auto spline = MakeDegree0Spline();
    const Vector3d n(0.0, 0.0, 1.0);
    // t=0.5: 非角点 (角点は t=1.0 のみ)、非直線部 (CTYPE=3≠kLinear)、C'=0
    EXPECT_FALSE(spline->TryGetSignedCurvature(0.5, n).has_value());
}
