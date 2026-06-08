/**
 * @file tests/entities/surfaces/test_tabulated_cylinder.cpp
 * @brief TabulatedCylinder (Type 122) の構築・編集APIのテスト
 * @author Yayoi Habami
 * @date 2026-06-05
 * @copyright 2026 Yayoi Habami
 *
 * テスト対象:
 *   [ファクトリ関数]
 *     MakeTabulatedCylinder(directrix, location_vector)
 *     MakeExtrudedSurface(directrix, extrusion)
 *   [要素の取得]
 *     TryGetDefinedDirection(), TryGetDirection()
 *   [コンストラクタ]
 *     TabulatedCylinder(directrix, ...) のnullptr検出 (回帰)
 *
 * TODO:
 *   - 評価系 (TryGetDefinedDerivatives等) の網羅はtest_i_surface.cppの責務
 *     (本ファイルでは掃引の幾何対応のみ検証)
 *   - GetMainPDParameters()経由の書き出しはprotectedのため対象外
 *     (test_igesio側の責務)
 *   - 始点一致・ゼロベクトル判定の許容差近傍の境界精度は未検証
 *     (IsApproxEqual/IsApproxZeroの内部許容差に依存するため、
 *      許容差を確実に超える代表値での両側検証のみ実施)
 */
#include <gtest/gtest.h>

#include <initializer_list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/common/iges_parameter_vector.h"
#include "igesio/entities/curves/line.h"
#include "igesio/entities/curves/linear_path.h"
#include "igesio/entities/surfaces/tabulated_cylinder.h"
#include "igesio/entities/transformations/transformation_matrix.h"

namespace {

namespace i_ent = igesio::entities;
using igesio::Vector3d;
using i_ent::TabulatedCylinder;
using i_ent::SubordinateEntitySwitch;
/// @brief 浮動小数点比較の許容誤差
constexpr double kTol = 1e-9;
/// @brief IsApproxEqual/IsApproxZeroの許容差を確実に超えるオフセット
constexpr double kClearOffset = 1e-3;



/**
 * 検証ヘルパー・フィクスチャ
 */

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

/// @brief 準線: X軸に沿う線分 (1,2,3)→(11,2,3)
std::shared_ptr<i_ent::Line> MakeDirectrix() {
    return i_ent::MakeLine(Vector3d(1.0, 2.0, 3.0), Vector3d(11.0, 2.0, 3.0));
}

/// @brief 始点を持たない準線 (Form 2の無限直線)
std::shared_ptr<i_ent::Line> MakeUnboundedDirectrix() {
    return i_ent::MakeUnboundedLine(Vector3d(0.0, 0.0, 0.0),
                                    Vector3d(1.0, 0.0, 0.0));
}

}  // namespace



/**
 * ファクトリ関数: 正常系
 */

TEST(TabulatedCylinderFactoryTest, MakeTabulatedCylinder_StoredValues) {
    const auto directrix = MakeDirectrix();
    const Vector3d location(1.0, 5.0, 6.0);
    const auto surface = i_ent::MakeTabulatedCylinder(directrix, location);

    EXPECT_EQ(surface->GetDirectrix()->GetID(), directrix->GetID());
    ExpectVectorNear(surface->GetLocationVector(), location);
    EXPECT_EQ(directrix->GetSubordinateEntitySwitch(),
              SubordinateEntitySwitch::kPhysicallyDependent);
}

TEST(TabulatedCylinderFactoryTest,
     MakeExtrudedSurface_LocationIsStartPlusExtrusion) {
    const auto directrix = MakeDirectrix();
    const Vector3d extrusion(0.3, -0.4, 1.2);
    const auto surface = i_ent::MakeExtrudedSurface(directrix, extrusion);

    // 母線の終点 = C(0) + extrusion = (1,2,3) + (0.3,-0.4,1.2)
    ExpectVectorNear(surface->GetLocationVector(), Vector3d(1.3, 1.6, 4.2));
    // 非正規化の押し出しベクトルがそのまま方向ベクトルとなる
    ExpectVectorNear(surface->GetDirection(), extrusion);
}

TEST(TabulatedCylinderFactoryTest, MakeExtrudedSurface_SweepGeometry) {
    const auto surface = i_ent::MakeExtrudedSurface(
            MakeDirectrix(), Vector3d(0.0, 3.0, 4.0));

    // S(u,1) - S(u,0) = extrusion (母線が準線に沿って平行移動する)
    for (const double u : {0.0, 0.5, 1.0}) {
        const auto bottom = EvalSurface(*surface, u, 0.0);
        const auto top = EvalSurface(*surface, u, 1.0);
        ExpectVectorNear(top - bottom, Vector3d(0.0, 3.0, 4.0));
    }
}



/**
 * ファクトリ関数: 異常系
 */

TEST(TabulatedCylinderFactoryTest,
     MakeTabulatedCylinder_ThrowsInvalidArgumentWhenDirectrixIsNull) {
    EXPECT_THROW(
        i_ent::MakeTabulatedCylinder(nullptr, Vector3d(0.0, 0.0, 1.0)),
        std::invalid_argument);
}

TEST(TabulatedCylinderFactoryTest,
     MakeTabulatedCylinder_ThrowsEntityValueErrorWhenLocationEqualsStart) {
    // 境界の外側: 準線の始点 (1,2,3) と完全に一致する終点
    EXPECT_THROW(
        i_ent::MakeTabulatedCylinder(MakeDirectrix(), Vector3d(1.0, 2.0, 3.0)),
        igesio::EntityValueError);
    // 境界の内側: 始点から明確に離れた終点は許容される
    EXPECT_NO_THROW(i_ent::MakeTabulatedCylinder(
        MakeDirectrix(), Vector3d(1.0 + kClearOffset, 2.0, 3.0)));
}

TEST(TabulatedCylinderFactoryTest,
     MakeExtrudedSurface_ThrowsInvalidArgumentWhenDirectrixIsNull) {
    EXPECT_THROW(
        i_ent::MakeExtrudedSurface(nullptr, Vector3d(0.0, 0.0, 1.0)),
        std::invalid_argument);
}

TEST(TabulatedCylinderFactoryTest,
     MakeExtrudedSurface_ThrowsEntityValueErrorWhenExtrusionIsZero) {
    // 境界の外側: ゼロベクトル
    EXPECT_THROW(
        i_ent::MakeExtrudedSurface(MakeDirectrix(), Vector3d(0.0, 0.0, 0.0)),
        igesio::EntityValueError);
    // 境界の内側: 微小でも非ゼロの押し出しは許容される
    EXPECT_NO_THROW(i_ent::MakeExtrudedSurface(
        MakeDirectrix(), Vector3d(0.0, 0.0, kClearOffset)));
}

TEST(TabulatedCylinderFactoryTest,
     MakeExtrudedSurface_ThrowsInvalidArgumentWhenNoStartPoint) {
    // 押し出しの基点C(0)が定まらないため構築できない
    EXPECT_THROW(
        i_ent::MakeExtrudedSurface(MakeUnboundedDirectrix(),
                                   Vector3d(0.0, 0.0, 1.0)),
        std::invalid_argument);
}



/**
 * 方向ベクトルのTry系アクセサ
 */

TEST(TabulatedCylinderDirectionTest, TryGetDefinedDirection_MatchesGetDirection) {
    const auto surface = i_ent::MakeExtrudedSurface(
            MakeDirectrix(), Vector3d(0.0, 3.0, 4.0));

    const auto direction = surface->TryGetDefinedDirection();
    ASSERT_TRUE(direction.has_value());
    ExpectVectorNear(*direction, surface->GetDirection());
}

// 準線の参照が未解決の場合: nulloptを返す (GetDirectionはthrow)
TEST(TabulatedCylinderDirectionTest,
     TryGetDefinedDirection_NulloptWhenDirectrixUnresolved) {
    const auto directrix = MakeDirectrix();
    // ポインタを解決せずIDのみ指定して構築
    const TabulatedCylinder surface(
            igesio::IGESParameterVector{directrix->GetID(), 1.0, 5.0, 6.0});

    EXPECT_EQ(surface.TryGetDefinedDirection(), std::nullopt);
    EXPECT_THROW(surface.GetDirection(), igesio::ReferenceError);
}

// 準線が始点を持たない場合: nulloptを返す (GetDirectionはthrow)
TEST(TabulatedCylinderDirectionTest,
     TryGetDefinedDirection_NulloptWhenNoStartPoint) {
    const auto surface = i_ent::MakeTabulatedCylinder(
            MakeUnboundedDirectrix(), Vector3d(0.0, 0.0, 1.0));

    EXPECT_EQ(surface->TryGetDefinedDirection(), std::nullopt);
    EXPECT_THROW(surface->GetDirection(), igesio::ReferenceError);
}

TEST(TabulatedCylinderDirectionTest, TryGetDirection_NoTransform_SameAsDefined) {
    const auto surface = i_ent::MakeExtrudedSurface(
            MakeDirectrix(), Vector3d(0.0, 3.0, 4.0));

    const auto direction = surface->TryGetDirection();
    ASSERT_TRUE(direction.has_value());
    ExpectVectorNear(*direction, Vector3d(0.0, 3.0, 4.0));
}

TEST(TabulatedCylinderDirectionTest, TryGetDirection_RotationApplied) {
    const auto surface = i_ent::MakeExtrudedSurface(
            MakeDirectrix(), Vector3d(0.0, 0.0, 5.0));
    const auto rotation =
        i_ent::MakeRotation(igesio::kPi / 2.0, Vector3d(1.0, 0.0, 0.0));
    ASSERT_TRUE(surface->OverwriteTransformationMatrix(rotation));

    // R_x(90°) * (0,0,5) = (0,-5,0); 定義空間側は不変
    const auto direction = surface->TryGetDirection();
    ASSERT_TRUE(direction.has_value());
    ExpectVectorNear(*direction, Vector3d(0.0, -5.0, 0.0));
    ExpectVectorNear(*surface->TryGetDefinedDirection(),
                     Vector3d(0.0, 0.0, 5.0));
}



/**
 * コンストラクタ: nullptr検出の回帰テスト
 * (委譲コンストラクタ引数内での逆参照によりUBだった経路)
 */

TEST(TabulatedCylinderConstructorTest,
     Constructor_ThrowsInvalidArgumentWhenDirectrixIsNull_LocationOverload) {
    EXPECT_THROW(TabulatedCylinder(nullptr, Vector3d(0.0, 0.0, 1.0)),
                 std::invalid_argument);
}

TEST(TabulatedCylinderConstructorTest,
     Constructor_ThrowsInvalidArgumentWhenDirectrixIsNull_DirectionOverload) {
    EXPECT_THROW(TabulatedCylinder(nullptr, Vector3d(0.0, 0.0, 1.0), 2.0),
                 std::invalid_argument);
}



/**
 * GetUCreaseParameters() のテスト
 *
 * 準線の角点 t を u = (t - t_start)/(t_end - t_start) で u∈[0,1] へ写像する.
 */

// 準線が角を持つ折れ線の場合、角点を単位区間へ写像して返す
TEST(TabulatedCylinderCreaseTest,
     GetUCreaseParameters_MapsDirectrixCornersToUnitInterval) {
    // 準線: 折れ線 (0,0,0)-(0,2,0)-(1,2,0) (長さ2 + 1 = 3, 角は累積長2.0)
    auto directrix = i_ent::MakeLinearPath(std::vector<Vector3d>{
        Vector3d{0., 0., 0.}, Vector3d{0., 2., 0.}, Vector3d{1., 2., 0.}});
    auto surface = i_ent::MakeExtrudedSurface(directrix, Vector3d{0., 0., 5.});

    const auto creases = surface->GetUCreaseParameters();
    ASSERT_EQ(creases.size(), 1u);
    EXPECT_NEAR(creases[0], 2.0 / 3.0, kTol);  // 角(累積長2.0) / 全長3.0
}

// 滑らかな準線 (直線) の場合は空 (角なし)
TEST(TabulatedCylinderCreaseTest,
     GetUCreaseParameters_EmptyWhenSmoothDirectrix) {
    auto surface = i_ent::MakeExtrudedSurface(
        MakeDirectrix(), Vector3d{0., 3., 4.});
    EXPECT_TRUE(surface->GetUCreaseParameters().empty());
}
