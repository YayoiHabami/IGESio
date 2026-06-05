/**
 * @file entities/test_circular_arc.cpp
 * @brief CircularArcエンティティのテスト
 * @author Yayoi Habami
 * @date 2025-08-16
 * @copyright 2025 Yayoi Habami
 * @note ファクトリ関数 (MakeCircularArc / MakeCircle /
 *       MakeCircularArcThroughPoints) のテストを含む
 */
#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/numerics/core/tolerance.h"
#include "igesio/entities/curves/circular_arc.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using Vector2d = igesio::Vector2d;
using Vector3d = igesio::Vector3d;
using CircularArc = igesio::entities::CircularArc;
constexpr double kPi = igesio::kPi;
/// @brief 浮動小数点比較の許容誤差
constexpr double kTol = 1e-9;

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

    EXPECT_TRUE(i_num::IsApproxEqual(arc.Center(), Vector3d(0.0, 0.0, 0.0)));
    EXPECT_DOUBLE_EQ(arc.Radius(), sqrt(2.0));
    EXPECT_DOUBLE_EQ(arc.StartAngle(), kPi / 4);
    EXPECT_DOUBLE_EQ(arc.EndAngle(), 5 * kPi / 4);

    auto result = arc.Validate();
    EXPECT_TRUE(result.is_valid) << result.Message();
}

// H: 始終点が中心から等距離でないPDデータ (ファイル読込相当) → 等距離でなくても円弧は
// 描画可能なためValidatePDは警告 (kWarning) を出すがis_valid=true (描画ブロックしない)。
// NOTE: プログラム的コンストラクタは非等距離でthrowするためPDコンストラクタで構築する。
TEST(CircularArcTest, NotEquidistant_IsValidWithWarning) {
    auto de = i_ent::RawEntityDE::ByDefault(i_ent::EntityType::kCircularArc);
    igesio::IGESParameterVector parameters{
        0.0,        // z_t
        0.0, 0.0,   // center
        1.0, 0.0,   // start (中心からの距離 r1 = 1)
        0.0, 2.0,   // terminate (r2 = 2) → 等距離でない
    };
    const CircularArc arc(de, parameters);  // PDコンストラクタはthrowしない
    const auto result = arc.Validate();
    EXPECT_TRUE(result.is_valid);  // 描画はブロックしない (本対処の要点)
    bool has_warning = false;
    for (const auto& e : result.errors) {
        if (e.severity == igesio::ValidationSeverity::kWarning) has_warning = true;
    }
    EXPECT_TRUE(has_warning);
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

    EXPECT_TRUE(i_num::IsApproxEqual(arc.Center(), Vector3d(0.0, 0.0, 0.0)));
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
    }, igesio::EntityValueError);

    // 例外テスト: 半径が0に近い場合
    Vector2d center_zero_radius(0.0, 0.0);
    Vector2d start_point_zero_radius(0.0, 0.0 + i_num::kGeometryTolerance / 2);
    Vector2d terminate_point_zero_radius(0.0, 0.0 + i_num::kGeometryTolerance / 2);
    double z_t_zero_radius = 0.0;

    ASSERT_THROW({
        CircularArc arc(center_zero_radius,
                        start_point_zero_radius,
                        terminate_point_zero_radius,
                        z_t_zero_radius);
    }, igesio::EntityValueError);
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

    EXPECT_TRUE(i_num::IsApproxEqual(arc.Center(), Vector3d(0.0, 0.0, 0.0)));
    EXPECT_DOUBLE_EQ(arc.Radius(), 1.0);
    EXPECT_DOUBLE_EQ(arc.StartAngle(), 0.0);
    EXPECT_DOUBLE_EQ(arc.EndAngle(), kPi / 2);

    EXPECT_TRUE(arc.IsValid());


    // 例外テスト: 半径が0に近い場合
    Vector2d center_zero_radius(0.0, 0.0);
    double radius_zero = i_num::kGeometryTolerance / 2;
    double start_angle_zero = 0.0;
    double end_angle_zero = kPi / 2;
    double z_t_zero = 0.0;

    ASSERT_THROW({
        CircularArc arc(center_zero_radius, radius_zero,
                        start_angle_zero, end_angle_zero, z_t_zero);
    }, igesio::EntityValueError);

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
    }, igesio::EntityValueError);
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

    EXPECT_TRUE(i_num::IsApproxEqual(arc.Center(), Vector3d(0.0, 0.0, 0.0)));
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
                    radius, i_num::kGeometryTolerance);

        // TangentAtとNormalAtは直交するか
        EXPECT_NEAR(tangent.value().dot(normal.value()),
                    0.0, i_num::kGeometryTolerance);
    }

    // パラメータ範囲外のテスト
    auto point_out = arc.TryGetDefinedPointAt(start - 0.1);
    auto tangent_out = arc.TryGetDefinedTangentAt(start - 0.1);
    auto normal_out = arc.TryGetDefinedNormalAt(start - 0.1);

    EXPECT_FALSE(point_out.has_value());
    EXPECT_FALSE(tangent_out.has_value());
    EXPECT_FALSE(normal_out.has_value());
}



/**
 * ファクトリ関数: MakeCircularArc (中心・始終点版/角度版)
 *
 * NOTE: ラップ先コンストラクタが持つ検証の境界精度は上記コンストラクタ
 *       テストの責務とし、ここでは例外型の透過のみ確認する
 */

// 代表値: 非原点中心+z_t≠0で各アクセサに値が格納される
TEST(CircularArcFactoryTest, MakeCircularArc_StoresCenterStartTerminate) {
    const auto arc = i_ent::MakeCircularArc(
            Vector2d(1.0, 2.0), Vector2d(3.0, 2.0), Vector2d(1.0, 4.0), 0.5);

    ASSERT_NE(arc, nullptr);
    EXPECT_TRUE(i_num::IsApproxEqual(arc->Center(), Vector3d(1.0, 2.0, 0.5)));
    EXPECT_NEAR(arc->Radius(), 2.0, kTol);
    EXPECT_NEAR(arc->StartAngle(), 0.0, kTol);
    EXPECT_NEAR(arc->EndAngle(), kPi / 2.0, kTol);
    EXPECT_TRUE(arc->IsValid());
}

// エラー透過: 始終点が中心から等距離でない → EntityValueError
TEST(CircularArcFactoryTest,
     MakeCircularArc_ThrowsEntityValueErrorWhenNotEquidistant) {
    EXPECT_THROW(i_ent::MakeCircularArc(
            Vector2d(0.0, 0.0), Vector2d(1.0, 0.0), Vector2d(0.0, 2.0)),
            igesio::EntityValueError);
}

// 代表値: 半径と始終角が格納され、始角の点の座標が一致する
TEST(CircularArcFactoryTest, MakeCircularArcFromAngles_StoresRadiusAndAngles) {
    const auto arc = i_ent::MakeCircularArc(
            Vector2d(-1.0, 1.0), 2.0, kPi / 6.0, kPi / 2.0, 1.5);

    ASSERT_NE(arc, nullptr);
    EXPECT_NEAR(arc->Radius(), 2.0, kTol);
    EXPECT_NEAR(arc->StartAngle(), kPi / 6.0, kTol);
    EXPECT_NEAR(arc->EndAngle(), kPi / 2.0, kTol);

    const auto start = arc->TryGetDefinedPointAt(kPi / 6.0);
    ASSERT_TRUE(start.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(
            *start, Vector3d(-1.0 + std::sqrt(3.0), 2.0, 1.5), kTol));
}

// 境界/縮退: 同角度 → 閉じた円 (範囲幅2π)
TEST(CircularArcFactoryTest,
     MakeCircularArcFromAngles_EqualAnglesYieldsClosedCircle) {
    const auto arc = i_ent::MakeCircularArc(
            Vector2d(0.0, 0.0), 1.0, kPi / 4.0, kPi / 4.0);
    EXPECT_TRUE(arc->IsClosed());
    const auto range = arc->GetParameterRange();
    EXPECT_NEAR(range[1] - range[0], 2.0 * kPi, kTol);
}

// エラー+境界精度: start_angle > end_angleは厳密比較で棄却され、同値は受理
TEST(CircularArcFactoryTest,
     MakeCircularArcFromAngles_ThrowsEntityValueErrorWhenStartExceedsEnd) {
    EXPECT_THROW(i_ent::MakeCircularArc(
            Vector2d(0.0, 0.0), 1.0, kPi / 4.0 + 1e-12, kPi / 4.0),
            igesio::EntityValueError);
    EXPECT_NO_THROW(i_ent::MakeCircularArc(
            Vector2d(0.0, 0.0), 1.0, kPi / 4.0, kPi / 4.0));
}

// エラー+境界精度: 半径がkGeometryTolerance (1e-9) の内側→throw、すぐ外→受理
TEST(CircularArcFactoryTest,
     MakeCircularArcFromAngles_ThrowsEntityValueErrorWhenRadiusNearZero) {
    EXPECT_THROW(i_ent::MakeCircularArc(
            Vector2d(0.0, 0.0), 1e-10, 0.0, kPi / 2.0),
            igesio::EntityValueError);
    EXPECT_NO_THROW(i_ent::MakeCircularArc(
            Vector2d(0.0, 0.0), 1e-8, 0.0, kPi / 2.0));
}



/**
 * ファクトリ関数: MakeCircle
 */

// 代表値/縮退: 閉じた円が生成される
TEST(CircularArcFactoryTest, MakeCircle_CreatesClosedCircle) {
    const auto circle = i_ent::MakeCircle(Vector2d(2.0, -1.0), 1.5, 0.25);

    ASSERT_NE(circle, nullptr);
    EXPECT_TRUE(circle->IsClosed());
    EXPECT_TRUE(i_num::IsApproxEqual(
            circle->Center(), Vector3d(2.0, -1.0, 0.25)));
    EXPECT_NEAR(circle->Radius(), 1.5, kTol);
    const auto range = circle->GetParameterRange();
    EXPECT_NEAR(range[1] - range[0], 2.0 * kPi, kTol);
}

// エラー透過: 半径0 → EntityValueError
TEST(CircularArcFactoryTest,
     MakeCircle_ThrowsEntityValueErrorWhenRadiusNearZero) {
    EXPECT_THROW(i_ent::MakeCircle(Vector2d(0.0, 0.0), 0.0),
                 igesio::EntityValueError);
}



/**
 * ファクトリ関数: MakeCircularArcThroughPoints
 */

// 代表値: 反時計回りの3点はそのままの順序で弧になる
TEST(CircularArcFactoryTest,
     MakeCircularArcThroughPoints_CcwPointsPreserveOrder) {
    const auto arc = i_ent::MakeCircularArcThroughPoints(
            Vector2d(1.0, 0.0), Vector2d(0.0, 1.0), Vector2d(-1.0, 0.0));

    ASSERT_NE(arc, nullptr);
    EXPECT_TRUE(i_num::IsApproxEqual(arc->Center(), Vector3d(0.0, 0.0, 0.0)));
    EXPECT_NEAR(arc->Radius(), 1.0, kTol);
    EXPECT_NEAR(arc->StartAngle(), 0.0, kTol);
    EXPECT_NEAR(arc->EndAngle(), kPi, kTol);

    // 通過点(0,1)が弧上 (角π/2) に乗る
    const auto mid = arc->TryGetDefinedPointAt(kPi / 2.0);
    ASSERT_TRUE(mid.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(*mid, Vector3d(0.0, 1.0, 0.0), kTol));
}

// 代表値: 時計回りの3点は始終点が入れ替わり、通過点が弧上に保たれる
TEST(CircularArcFactoryTest,
     MakeCircularArcThroughPoints_CwPointsSwapEndpoints) {
    const auto arc = i_ent::MakeCircularArcThroughPoints(
            Vector2d(1.0, 0.0), Vector2d(0.0, -1.0), Vector2d(-1.0, 0.0));

    // 始点(-1,0)・終点(1,0)の反時計回りの弧 [π, 2π] になる
    EXPECT_NEAR(arc->StartAngle(), kPi, kTol);
    EXPECT_NEAR(arc->EndAngle(), 2.0 * kPi, kTol);

    // 通過点(0,-1)が弧上 (角3π/2) に乗る
    const auto mid = arc->TryGetDefinedPointAt(3.0 * kPi / 2.0);
    ASSERT_TRUE(mid.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(*mid, Vector3d(0.0, -1.0, 0.0), kTol));
}

// 代表値: 非対称な3点から外心が正しく計算され、z_tが伝播する
TEST(CircularArcFactoryTest,
     MakeCircularArcThroughPoints_ComputesCircumcenter) {
    const auto arc = i_ent::MakeCircularArcThroughPoints(
            Vector2d(2.0, 0.0), Vector2d(1.0, 1.0), Vector2d(0.0, 0.0), 2.0);

    EXPECT_TRUE(i_num::IsApproxEqual(arc->Center(), Vector3d(1.0, 0.0, 2.0)));
    EXPECT_NEAR(arc->Radius(), 1.0, kTol);
    EXPECT_TRUE(arc->IsValid());
}

// エラー+境界精度: 共線判定 (|u×v| ≤ kGeometryTolerance·|u||v|)。
// 中点の浮きsに対し正規化外積≈s/L となる構成で境界を確認する。
// 大半径構成 (半径~5e7以上) はラップ先の絶対許容誤差による等距離検証と
// 干渉するため、半径~5e4に収まる小スケール点列を使用する
TEST(CircularArcFactoryTest,
     MakeCircularArcThroughPoints_ThrowsEntityValueErrorWhenCollinear) {
    constexpr double kL = 1e-3;  // 半弦長
    // 厳密に共線 → throw
    EXPECT_THROW(i_ent::MakeCircularArcThroughPoints(
            Vector2d(0.0, 0.0), Vector2d(kL, 0.0), Vector2d(2.0 * kL, 0.0)),
            igesio::EntityValueError);
    // s/L = 1e-10 < 1e-9: 許容誤差の内側 → 共線とみなされ棄却
    EXPECT_THROW(i_ent::MakeCircularArcThroughPoints(
            Vector2d(0.0, 0.0), Vector2d(kL, 1e-13), Vector2d(2.0 * kL, 0.0)),
            igesio::EntityValueError);
    // s/L = 1e-8 > 1e-9: 許容誤差のすぐ外 → 受理され大半径の弧になる
    std::shared_ptr<CircularArc> arc;
    EXPECT_NO_THROW(arc = i_ent::MakeCircularArcThroughPoints(
            Vector2d(0.0, 0.0), Vector2d(kL, 1e-11), Vector2d(2.0 * kL, 0.0)));
    EXPECT_NEAR(arc->Radius(), 5.0e4, 1.0);
}

// エラー: 点の一致は共線判定で棄却される
TEST(CircularArcFactoryTest,
     MakeCircularArcThroughPoints_ThrowsEntityValueErrorWhenPointsCoincide) {
    EXPECT_THROW(i_ent::MakeCircularArcThroughPoints(
            Vector2d(1.0, 1.0), Vector2d(1.0, 1.0), Vector2d(2.0, 3.0)),
            igesio::EntityValueError);
    EXPECT_THROW(i_ent::MakeCircularArcThroughPoints(
            Vector2d(1.0, 1.0), Vector2d(2.0, 3.0), Vector2d(1.0, 1.0)),
            igesio::EntityValueError);
}
