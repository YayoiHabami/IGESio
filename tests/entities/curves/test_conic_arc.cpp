/**
 * @file entities/curves/test_conic_arc.cpp
 * @brief ConicArc (Type 104) のファクトリ関数のテスト
 * @author Yayoi Habami
 * @date 2026-06-05
 * @copyright 2026 Yayoi Habami
 * @note 対象: MakeConicArc / MakeEllipticArc / MakeEllipse /
 *       MakeParabolicArc / MakeHyperbolicArc
 * @note TODO: ConicArcエンティティ本体 (コンストラクタ・ICurve実装) の
 *       テストは未整備
 * @note TODO: MakeConicArcのエラーケース (曲線種別の判別不能) は、現実装の
 *       CalculateConicTypeが全実数係数に対して値を返すため到達不能であり未作成
 */
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <memory>
#include <utility>

#include "igesio/common/errors.h"
#include "igesio/numerics/core/tolerance.h"
#include "igesio/entities/curves/conic_arc.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using Vector2d = igesio::Vector2d;
using Vector3d = igesio::Vector3d;
using ConicAxis = igesio::entities::ConicAxis;
using ConicType = igesio::entities::ConicType;
constexpr double kPi = igesio::kPi;
/// @brief 浮動小数点比較の許容誤差
constexpr double kTol = 1e-9;

/// @brief 点の2次曲線への代入残差を計算する
/// @param coeffs 2次方程式の係数 {A, B, C, D, E, F}
/// @param p 評価する点 (x, y, z_t); z成分は使用しない
/// @return 残差 Ax^2 + Bxy + Cy^2 + Dx + Ey + F (点が曲線上にあれば0)
double ConicResidual(const std::array<double, 6>& coeffs, const Vector3d& p) {
    const auto [A, B, C, D, E, F] = coeffs;
    return A * p.x() * p.x() + B * p.x() * p.y() + C * p.y() * p.y()
            + D * p.x() + E * p.y() + F;
}

}  // namespace



/**
 * ファクトリ関数: MakeConicArc
 */

// 代表値: 係数・始終点・z_tが格納される
TEST(ConicArcFactoryTest, MakeConicArc_StoresCoefficientsAndPoints) {
    // 楕円 x^2/4 + y^2 = 1 (rx=2, ry=1): 係数 (ry^2, 0, rx^2, 0, 0, -rx^2*ry^2)
    const std::array<double, 6> coeffs{1.0, 0.0, 4.0, 0.0, 0.0, -4.0};
    const auto arc = i_ent::MakeConicArc(
            coeffs, Vector2d(2.0, 0.0), Vector2d(0.0, 1.0), 0.5);

    ASSERT_NE(arc, nullptr);
    EXPECT_EQ(arc->GetConicType(), ConicType::kEllipse);
    for (size_t i = 0; i < coeffs.size(); ++i) {
        EXPECT_DOUBLE_EQ(arc->ConicCoefficients()[i], coeffs[i]);
    }
    EXPECT_TRUE(i_num::IsApproxEqual(
            arc->ConicStartPoint(), Vector3d(2.0, 0.0, 0.5)));
    EXPECT_TRUE(i_num::IsApproxEqual(
            arc->ConicTerminatePoint(), Vector3d(0.0, 1.0, 0.5)));
    EXPECT_TRUE(arc->IsValid());
}

// 代表値: Form番号 (曲線種別) が係数から自動決定される
TEST(ConicArcFactoryTest, MakeConicArc_AutoDetectsFormNumber) {
    // 双曲線 x^2/4 - y^2 = 1: q2 = AC < 0 → kHyperbola
    const auto hyperbola = i_ent::MakeConicArc(
            {1.0, 0.0, -4.0, 0.0, 0.0, -4.0}, Vector2d(2.0, 0.0),
            Vector2d(2.0 / std::cos(1.0), std::tan(1.0)));
    EXPECT_EQ(hyperbola->GetConicType(), ConicType::kHyperbola);

    // 放物線 y^2 = 2x: q2 = 0 → kParabola
    const auto parabola = i_ent::MakeConicArc(
            {0.0, 0.0, 1.0, -2.0, 0.0, 0.0},
            Vector2d(0.5, -1.0), Vector2d(2.0, 2.0));
    EXPECT_EQ(parabola->GetConicType(), ConicType::kParabola);
}



/**
 * ファクトリ関数: MakeEllipticArc / MakeEllipse
 */

// 代表値: 半径・始終角の点・z_tが格納される
TEST(ConicArcFactoryTest, MakeEllipticArc_StoresRadiiAndAngles) {
    const auto arc = i_ent::MakeEllipticArc(
            2.0, 1.0, kPi / 6.0, 2.0 * kPi / 3.0, 0.5);

    ASSERT_NE(arc, nullptr);
    EXPECT_EQ(arc->GetConicType(), ConicType::kEllipse);
    const auto [rx, ry] = arc->EllipseRadii();
    EXPECT_NEAR(rx, 2.0, kTol);
    EXPECT_NEAR(ry, 1.0, kTol);
    EXPECT_TRUE(i_num::IsApproxEqual(
            arc->ConicStartPoint(),
            Vector3d(2.0 * std::cos(kPi / 6.0), std::sin(kPi / 6.0), 0.5),
            kTol));
    EXPECT_TRUE(i_num::IsApproxEqual(
            arc->ConicTerminatePoint(),
            Vector3d(2.0 * std::cos(2.0 * kPi / 3.0),
                     std::sin(2.0 * kPi / 3.0), 0.5),
            kTol));
    EXPECT_NEAR(arc->EllipseStartAngle(), kPi / 6.0, kTol);
    EXPECT_NEAR(arc->EllipseEndAngle(), 2.0 * kPi / 3.0, kTol);
    EXPECT_FALSE(arc->IsClosed());
    EXPECT_TRUE(arc->IsValid());
}

// 代表値: 負角度の指定でも始角の点から反時計回りの弧になる
TEST(ConicArcFactoryTest, MakeEllipticArc_NegativeAnglesYieldCcwArc) {
    const auto arc = i_ent::MakeEllipticArc(2.0, 1.0, -kPi / 2.0, kPi / 2.0);

    // 始点(0,-ry)から終点(0,ry)まで反時計回り (+X側を通る半周)
    EXPECT_TRUE(i_num::IsApproxEqual(
            arc->ConicStartPoint(), Vector3d(0.0, -1.0, 0.0), kTol));
    EXPECT_TRUE(i_num::IsApproxEqual(
            arc->ConicTerminatePoint(), Vector3d(0.0, 1.0, 0.0), kTol));
    const auto range = arc->GetParameterRange();
    EXPECT_NEAR(range[1] - range[0], kPi, kTol);
}

// 境界/縮退: 同角度 → 閉じた楕円 (範囲幅2π)
TEST(ConicArcFactoryTest, MakeEllipticArc_EqualAnglesYieldClosedEllipse) {
    const auto arc = i_ent::MakeEllipticArc(2.0, 1.0, kPi / 4.0, kPi / 4.0);
    EXPECT_TRUE(arc->IsClosed());
    const auto range = arc->GetParameterRange();
    EXPECT_NEAR(range[1] - range[0], 2.0 * kPi, kTol);
}

// エラー+境界精度: 半径の検証はIsApproxZero (kParameterTolerance≈2.22e-14)
TEST(ConicArcFactoryTest,
     MakeEllipticArc_ThrowsEntityValueErrorWhenRadiusNearZero) {
    // 許容誤差の内側 → throw
    EXPECT_THROW(i_ent::MakeEllipticArc(1e-15, 1.0, 0.0, kPi),
                 igesio::EntityValueError);
    EXPECT_THROW(i_ent::MakeEllipticArc(1.0, 1e-15, 0.0, kPi),
                 igesio::EntityValueError);
    // 許容誤差のすぐ外 → 受理
    EXPECT_NO_THROW(i_ent::MakeEllipticArc(1e-12, 1.0, 0.0, kPi));
}

// 代表値/縮退: 閉じた楕円が生成される
TEST(ConicArcFactoryTest, MakeEllipse_CreatesClosedEllipse) {
    const auto ellipse = i_ent::MakeEllipse(3.0, 1.5, -1.0);

    ASSERT_NE(ellipse, nullptr);
    EXPECT_TRUE(ellipse->IsClosed());
    const auto [rx, ry] = ellipse->EllipseRadii();
    EXPECT_NEAR(rx, 3.0, kTol);
    EXPECT_NEAR(ry, 1.5, kTol);
    // 始終点は一致し、(rx, 0, z_t) に置かれる
    EXPECT_TRUE(i_num::IsApproxEqual(
            ellipse->ConicStartPoint(), Vector3d(3.0, 0.0, -1.0), kTol));
    EXPECT_TRUE(i_num::IsApproxEqual(
            ellipse->ConicStartPoint(), ellipse->ConicTerminatePoint(), kTol));
    const auto range = ellipse->GetParameterRange();
    EXPECT_NEAR(range[1] - range[0], 2.0 * kPi, kTol);
}

// エラー透過: 半径0 → EntityValueError
TEST(ConicArcFactoryTest,
     MakeEllipse_ThrowsEntityValueErrorWhenRadiusNearZero) {
    EXPECT_THROW(i_ent::MakeEllipse(0.0, 1.0), igesio::EntityValueError);
    EXPECT_THROW(i_ent::MakeEllipse(1.0, 0.0), igesio::EntityValueError);
}



/**
 * ファクトリ関数: MakeParabolicArc
 */

// 代表値 (kX): 標準形の係数・端点が格納され、tが曲線パラメータとして復元される
TEST(ConicArcFactoryTest, MakeParabolicArc_AxisXStoresCanonicalCoefficients) {
    // y^2 = 2x (p=0.5)、tはy座標
    const auto arc = i_ent::MakeParabolicArc(0.5, -1.0, 2.0);

    ASSERT_NE(arc, nullptr);
    EXPECT_EQ(arc->GetConicType(), ConicType::kParabola);
    const std::array<double, 6> expected{0.0, 0.0, 1.0, -2.0, 0.0, 0.0};
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_DOUBLE_EQ(arc->ConicCoefficients()[i], expected[i]);
    }
    EXPECT_TRUE(i_num::IsApproxEqual(
            arc->ConicStartPoint(), Vector3d(0.5, -1.0, 0.0), kTol));
    EXPECT_TRUE(i_num::IsApproxEqual(
            arc->ConicTerminatePoint(), Vector3d(2.0, 2.0, 0.0), kTol));
    EXPECT_NEAR(ConicResidual(arc->ConicCoefficients(),
                              arc->ConicStartPoint()), 0.0, kTol);

    // ファクトリのtが曲線パラメータとして復元される
    const auto range = arc->GetParameterRange();
    EXPECT_NEAR(range[0], -1.0, kTol);
    EXPECT_NEAR(range[1], 2.0, kTol);
    EXPECT_TRUE(arc->IsValid());
}

// 代表値 (kY): 標準形の係数・端点が格納され、tが曲線パラメータとして復元される
TEST(ConicArcFactoryTest, MakeParabolicArc_AxisYStoresCanonicalCoefficients) {
    // x^2 = 4y (p=1)、tはx座標
    const auto arc = i_ent::MakeParabolicArc(1.0, 0.0, 2.0, ConicAxis::kY);

    EXPECT_EQ(arc->GetConicType(), ConicType::kParabola);
    const std::array<double, 6> expected{1.0, 0.0, 0.0, 0.0, -4.0, 0.0};
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_DOUBLE_EQ(arc->ConicCoefficients()[i], expected[i]);
    }
    EXPECT_TRUE(i_num::IsApproxEqual(
            arc->ConicStartPoint(), Vector3d(0.0, 0.0, 0.0), kTol));
    EXPECT_TRUE(i_num::IsApproxEqual(
            arc->ConicTerminatePoint(), Vector3d(2.0, 1.0, 0.0), kTol));
    const auto range = arc->GetParameterRange();
    EXPECT_NEAR(range[0], 0.0, kTol);
    EXPECT_NEAR(range[1], 2.0, kTol);
}

// 代表値: 負の焦点距離では-X方向に開く放物線になる
TEST(ConicArcFactoryTest,
     MakeParabolicArc_NegativeFocalDistanceOpensNegativeDirection) {
    // y^2 = -2x (p=-0.5)
    const auto arc = i_ent::MakeParabolicArc(-0.5, 1.0, 2.0);

    EXPECT_EQ(arc->GetConicType(), ConicType::kParabola);
    EXPECT_LT(arc->ConicStartPoint().x(), 0.0);
    EXPECT_LT(arc->ConicTerminatePoint().x(), 0.0);
    EXPECT_NEAR(ConicResidual(arc->ConicCoefficients(),
                              arc->ConicStartPoint()), 0.0, kTol);
    EXPECT_NEAR(ConicResidual(arc->ConicCoefficients(),
                              arc->ConicTerminatePoint()), 0.0, kTol);
}

// 境界: t_start > t_endの指定も受理され、入力の順序のまま格納される
TEST(ConicArcFactoryTest, MakeParabolicArc_AcceptsReversedParameterOrder) {
    const auto arc = i_ent::MakeParabolicArc(0.5, 2.0, -1.0);
    EXPECT_TRUE(i_num::IsApproxEqual(
            arc->ConicStartPoint(), Vector3d(2.0, 2.0, 0.0), kTol));
    EXPECT_TRUE(i_num::IsApproxEqual(
            arc->ConicTerminatePoint(), Vector3d(0.5, -1.0, 0.0), kTol));
}

// エラー+境界精度: 焦点距離がkGeometryTolerance (1e-9) の内側→throw、すぐ外→受理
TEST(ConicArcFactoryTest,
     MakeParabolicArc_ThrowsEntityValueErrorWhenFocalDistanceNearZero) {
    EXPECT_THROW(i_ent::MakeParabolicArc(1e-10, 0.0, 1.0),
                 igesio::EntityValueError);
    EXPECT_NO_THROW(i_ent::MakeParabolicArc(1e-8, 0.0, 1e-3));
}

// エラー+境界精度: t_start≈t_end (差がkGeometryToleranceの内側) → throw
TEST(ConicArcFactoryTest,
     MakeParabolicArc_ThrowsEntityValueErrorWhenParametersEqual) {
    EXPECT_THROW(i_ent::MakeParabolicArc(0.5, 1.0, 1.0),
                 igesio::EntityValueError);
    EXPECT_THROW(i_ent::MakeParabolicArc(0.5, 1.0, 1.0 + 1e-10),
                 igesio::EntityValueError);
    // 差が許容誤差のすぐ外 → 受理
    EXPECT_NO_THROW(i_ent::MakeParabolicArc(0.5, 1.0, 1.0 + 1e-8));
}



/**
 * ファクトリ関数: MakeHyperbolicArc
 */

// 代表値 (kX): 標準形の係数・端点が格納され、tが曲線パラメータとして復元される
TEST(ConicArcFactoryTest, MakeHyperbolicArc_AxisXStoresCanonicalCoefficients) {
    // x^2/4 - y^2 = 1 (a=2, b=1) の+X枝
    const auto arc = i_ent::MakeHyperbolicArc(2.0, 1.0, -0.5, 1.0);

    ASSERT_NE(arc, nullptr);
    EXPECT_EQ(arc->GetConicType(), ConicType::kHyperbola);
    const std::array<double, 6> expected{1.0, 0.0, -4.0, 0.0, 0.0, -4.0};
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_DOUBLE_EQ(arc->ConicCoefficients()[i], expected[i]);
    }
    // 始終点 C(t) = (a*sec(t), b*tan(t))
    EXPECT_TRUE(i_num::IsApproxEqual(
            arc->ConicStartPoint(),
            Vector3d(2.0 / std::cos(-0.5), std::tan(-0.5), 0.0), kTol));
    EXPECT_TRUE(i_num::IsApproxEqual(
            arc->ConicTerminatePoint(),
            Vector3d(2.0 / std::cos(1.0), std::tan(1.0), 0.0), kTol));
    EXPECT_NEAR(ConicResidual(arc->ConicCoefficients(),
                              arc->ConicStartPoint()), 0.0, kTol);

    // ファクトリのtが曲線パラメータとして復元される
    const auto range = arc->GetParameterRange();
    EXPECT_NEAR(range[0], -0.5, kTol);
    EXPECT_NEAR(range[1], 1.0, kTol);
    EXPECT_FALSE(arc->IsClosed());
    EXPECT_TRUE(arc->IsValid());
}

// 代表値 (kY): 標準形の係数・端点が格納され、tが曲線パラメータとして復元される
TEST(ConicArcFactoryTest, MakeHyperbolicArc_AxisYStoresCanonicalCoefficients) {
    // y^2/4 - x^2 = 1 (a=1, b=2) の+Y枝
    const auto arc = i_ent::MakeHyperbolicArc(
            1.0, 2.0, 0.2, 1.2, ConicAxis::kY);

    EXPECT_EQ(arc->GetConicType(), ConicType::kHyperbola);
    const std::array<double, 6> expected{-4.0, 0.0, 1.0, 0.0, 0.0, -4.0};
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_DOUBLE_EQ(arc->ConicCoefficients()[i], expected[i]);
    }
    // 始終点 C(t) = (a*tan(t), b*sec(t))
    EXPECT_TRUE(i_num::IsApproxEqual(
            arc->ConicStartPoint(),
            Vector3d(std::tan(0.2), 2.0 / std::cos(0.2), 0.0), kTol));
    const auto range = arc->GetParameterRange();
    EXPECT_NEAR(range[0], 0.2, kTol);
    EXPECT_NEAR(range[1], 1.2, kTol);
}

// エラー+境界精度: 半軸長がkGeometryTolerance (1e-9) 以下は正とみなされない
TEST(ConicArcFactoryTest,
     MakeHyperbolicArc_ThrowsEntityValueErrorWhenSemiAxisNotPositive) {
    EXPECT_THROW(i_ent::MakeHyperbolicArc(1e-10, 1.0, 0.1, 0.5),
                 igesio::EntityValueError);
    EXPECT_THROW(i_ent::MakeHyperbolicArc(1.0, 1e-10, 0.1, 0.5),
                 igesio::EntityValueError);
    EXPECT_THROW(i_ent::MakeHyperbolicArc(1.0, -1.0, 0.1, 0.5),
                 igesio::EntityValueError);
    // 許容誤差のすぐ外 → 受理
    EXPECT_NO_THROW(i_ent::MakeHyperbolicArc(1e-8, 1.0, 0.1, 0.5));
}

// エラー+境界精度: |t|がπ/2に達する場合は棄却され、すぐ内側は受理される
TEST(ConicArcFactoryTest,
     MakeHyperbolicArc_ThrowsEntityValueErrorWhenParameterReachesHalfPi) {
    EXPECT_THROW(i_ent::MakeHyperbolicArc(2.0, 1.0, 0.0, kPi / 2.0),
                 igesio::EntityValueError);
    EXPECT_THROW(i_ent::MakeHyperbolicArc(2.0, 1.0, -kPi / 2.0, 0.0),
                 igesio::EntityValueError);
    EXPECT_NO_THROW(i_ent::MakeHyperbolicArc(2.0, 1.0, 0.0,
                                             kPi / 2.0 - 1e-6));
}

// エラー+境界精度: t_start≈t_end (差がkGeometryToleranceの内側) → throw
TEST(ConicArcFactoryTest,
     MakeHyperbolicArc_ThrowsEntityValueErrorWhenParametersEqual) {
    EXPECT_THROW(i_ent::MakeHyperbolicArc(2.0, 1.0, 0.5, 0.5),
                 igesio::EntityValueError);
    EXPECT_THROW(i_ent::MakeHyperbolicArc(2.0, 1.0, 0.5, 0.5 + 1e-10),
                 igesio::EntityValueError);
    EXPECT_NO_THROW(i_ent::MakeHyperbolicArc(2.0, 1.0, 0.5, 0.5 + 1e-8));
}
