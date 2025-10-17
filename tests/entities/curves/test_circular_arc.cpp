/**
 * @file entities/test_circular_arc.cpp
 * @brief CircularArcエンティティのテスト
 * @author Yayoi Habami
 * @date 2025-08-16
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/common/tolerance.h"
#include "igesio/entities/curves/circular_arc.h"

namespace {

namespace i_ent = igesio::entities;
using Vector2d = igesio::Vector2d;
using Vector3d = igesio::Vector3d;
using CircularArc = igesio::entities::CircularArc;
constexpr double kPi = igesio::kPi;

}  // namespace



/**
 * コンストラクタのテスト
 */

// RawEntityDEとIGESParameterVectorからのコンストラクタ
TEST(CircularArcTest, ConstructorFromDEAndParameters) {
    auto de = i_ent::RawEntityDE::ByDefault(i_ent::EntityType::kCircularArc);
    igesio::IGESParameterVector parameters{
        0.0,  // z_t
        0.0, 0.0,  // center (x_c, y_c)
        1.0, 1.0,  // start_point (x_s, y_s)
        -1.0, -1.0,  // terminate_point (x_t, y_t)
    };

    ASSERT_NO_THROW({
        CircularArc arc_test(de, parameters);
    });

    CircularArc arc(de, parameters);

    EXPECT_TRUE(igesio::IsApproxEqual(arc.Center(), Vector3d(0.0, 0.0, 0.0)));
    EXPECT_DOUBLE_EQ(arc.Radius(), sqrt(2.0));
    EXPECT_DOUBLE_EQ(arc.StartAngle(), kPi / 4);
    EXPECT_DOUBLE_EQ(arc.EndAngle(), 5 * kPi / 4);

    auto result = arc.Validate();
    EXPECT_TRUE(result.is_valid) << result.Message();
}

// 中心点と始点・終点から円弧を生成するコンストラクタ
TEST(CircularArcTest, ConstructorFromCenterStartTerminate) {
    Vector2d center(0.0, 0.0);
    Vector2d start_point(1.0, 0.0);
    Vector2d terminate_point(0.0, 1.0);
    double z_t = 0.0;

    ASSERT_NO_THROW({
        CircularArc arc_test(center, start_point, terminate_point, z_t);
    });

    CircularArc arc(center, start_point, terminate_point, z_t);

    EXPECT_TRUE(igesio::IsApproxEqual(arc.Center(), Vector3d(0.0, 0.0, 0.0)));
    EXPECT_DOUBLE_EQ(arc.Radius(), 1.0);
    EXPECT_DOUBLE_EQ(arc.StartAngle(), 0.0);
    EXPECT_DOUBLE_EQ(arc.EndAngle(), kPi / 2);

    EXPECT_TRUE(arc.IsValid());


    // 例外テスト: 始点と終点が中心から等距離でない場合
    Vector2d center_invalid(0.0, 0.0);
    Vector2d start_point_invalid(1.0, 0.0);
    Vector2d terminate_point_invalid(0.0, 2.0);  // 距離が異なる
    double z_t_invalid = 0.0;

    ASSERT_THROW({
        CircularArc arc(center_invalid, start_point_invalid,
                        terminate_point_invalid, z_t_invalid);
    }, igesio::DataFormatError);

    // 例外テスト: 半径が0に近い場合
    Vector2d center_zero_radius(0.0, 0.0);
    Vector2d start_point_zero_radius(0.0, 0.0 + igesio::kGeometryTolerance / 2);
    Vector2d terminate_point_zero_radius(0.0, 0.0 + igesio::kGeometryTolerance / 2);
    double z_t_zero_radius = 0.0;

    ASSERT_THROW({
        CircularArc arc(center_zero_radius,
                        start_point_zero_radius,
                        terminate_point_zero_radius,
                        z_t_zero_radius);
    }, igesio::DataFormatError);
}

// 中心点と半径、始点角度と終点角度から円弧を生成するコンストラクタ
TEST(CircularArcTest, ConstructorFromCenterRadiusStartEndAngle) {
    Vector2d center(0.0, 0.0);
    double radius = 1.0;
    double start_angle = 0.0;
    double end_angle = kPi / 2;
    double z_t = 0.0;

    ASSERT_NO_THROW({
        CircularArc arc_test(center, radius, start_angle, end_angle, z_t);
    });

    CircularArc arc(center, radius, start_angle, end_angle, z_t);

    EXPECT_TRUE(igesio::IsApproxEqual(arc.Center(), Vector3d(0.0, 0.0, 0.0)));
    EXPECT_DOUBLE_EQ(arc.Radius(), 1.0);
    EXPECT_DOUBLE_EQ(arc.StartAngle(), 0.0);
    EXPECT_DOUBLE_EQ(arc.EndAngle(), kPi / 2);

    EXPECT_TRUE(arc.IsValid());


    // 例外テスト: 半径が0に近い場合
    Vector2d center_zero_radius(0.0, 0.0);
    double radius_zero = igesio::kGeometryTolerance / 2;
    double start_angle_zero = 0.0;
    double end_angle_zero = kPi / 2;
    double z_t_zero = 0.0;

    ASSERT_THROW({
        CircularArc arc(center_zero_radius, radius_zero,
                        start_angle_zero, end_angle_zero, z_t_zero);
    }, igesio::DataFormatError);

    // 例外テスト: 始点角度が終点角度より大きい場合
    Vector2d center_invalid_angle(0.0, 0.0);
    double radius_invalid = 1.0;
    double start_angle_invalid = kPi / 2;
    double end_angle_invalid = 0.0;
    double z_t_invalid_angle = 0.0;

    ASSERT_THROW({
        CircularArc arc(center_invalid_angle, radius_invalid,
                        start_angle_invalid, end_angle_invalid,
                        z_t_invalid_angle);
    }, igesio::DataFormatError);
}

// 中心点と半径から円（閉じた円弧）を生成するコンストラクタ
TEST(CircularArcTest, ConstructorFromCenterRadius) {
    Vector2d center(0.0, 0.0);
    double radius = 1.0;
    double z_t = 0.0;

    ASSERT_NO_THROW({
        CircularArc arc_test(center, radius, z_t);
    });

    CircularArc arc(center, radius, z_t);

    EXPECT_TRUE(igesio::IsApproxEqual(arc.Center(), Vector3d(0.0, 0.0, 0.0)));
    EXPECT_DOUBLE_EQ(arc.Radius(), 1.0);
    EXPECT_DOUBLE_EQ(arc.StartAngle(), 0.0);
    EXPECT_DOUBLE_EQ(arc.EndAngle(), 2.0 * kPi);

    EXPECT_TRUE(arc.IsValid());
}



/**
 * ICurve実装のテスト
 */

// パラメータ範囲のテスト
TEST(CircularArcTest, GetParameterRange) {
    Vector2d center(0.0, 0.0);
    double radius = 1.0;
    double start_angle = kPi / 4;
    double end_angle = 5 * kPi / 4;
    double z_t = 0.0;

    CircularArc arc(center, radius, start_angle, end_angle, z_t);

    auto range = arc.GetParameterRange();

    EXPECT_DOUBLE_EQ(range[0], kPi / 4);
    EXPECT_DOUBLE_EQ(range[1], 5 * kPi / 4);
}

// 閉じているかのテスト
TEST(CircularArcTest, IsClosed) {
    // 閉じている円弧のテスト
    Vector2d center(0.0, 0.0);
    double radius = 1.0;
    double z_t = 0.0;

    CircularArc closed_arc(center, radius, z_t);
    EXPECT_TRUE(closed_arc.IsClosed());

    // 閉じていない円弧のテスト
    Vector2d center2(0.0, 0.0);
    double radius2 = 1.0;
    double start_angle = 0.0;
    double end_angle = kPi / 2;
    double z_t2 = 0.0;

    CircularArc open_arc(center2, radius2, start_angle, end_angle, z_t2);
    EXPECT_FALSE(open_arc.IsClosed());
}

// PointAt, TangentAt, NormalAtのテスト
TEST(CircularArcTest, PointTangentNormalAt) {
    Vector2d center(0.0, 0.0);
    double radius = 1.0;
    double start_angle = 0.0;
    double end_angle = kPi / 2;
    double z_t = 0.0;

    CircularArc arc(center, radius, start_angle, end_angle, z_t);
    auto range = arc.GetParameterRange();
    double start = range[0];
    double end = range[1];

    // 複数のパラメータ値でテスト
    std::vector<double> params = {start, (start + end) / 2.0, end};

    for (const auto& t : params) {
        auto point = arc.TryGetDefinedPointAt(t);
        auto tangent = arc.TryGetDefinedTangentAt(t);
        auto normal = arc.TryGetDefinedNormalAt(t);

        ASSERT_TRUE(point.has_value());
        ASSERT_TRUE(tangent.has_value());
        ASSERT_TRUE(normal.has_value());

        // PointAtは中心からの距離が半径であるか
        EXPECT_NEAR((point.value() - Vector3d(center[0], center[1], z_t)).norm(),
                    radius, igesio::kGeometryTolerance);

        // TangentAtとNormalAtは直交するか
        EXPECT_NEAR(tangent.value().dot(normal.value()),
                    0.0, igesio::kGeometryTolerance);
    }

    // パラメータ範囲外のテスト
    auto point_out = arc.TryGetDefinedPointAt(start - 0.1);
    auto tangent_out = arc.TryGetDefinedTangentAt(start - 0.1);
    auto normal_out = arc.TryGetDefinedNormalAt(start - 0.1);

    EXPECT_FALSE(point_out.has_value());
    EXPECT_FALSE(tangent_out.has_value());
    EXPECT_FALSE(normal_out.has_value());
}
