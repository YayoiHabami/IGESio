/**
 * @file entities/test_circular_arc.cpp
 * @brief CircularArcエンティティのテスト
 * @author Yayoi Habami
 * @date 2025-08-16
 * @copyright 2025 Yayoi Habami
 * @note ファクトリ関数 (MakeCircularArc / MakeCircle /
 *       MakeCircularArcThroughPoints) のテストを含む
 * @note 時計回り (CW) の弧の生成・評価と、IGES出力時の展開 (ExpandForExport:
 *       鏡映CCW弧 + Type 124) のテストを含む
 */
#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/numerics/core/tolerance.h"
#include "igesio/entities/curves/circular_arc.h"
#include "igesio/entities/structures/color_definition.h"
#include "igesio/entities/transformations/transformation_matrix.h"

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

    // 始点角度が終点角度より大きい場合は例外ではなく時計回りの弧になる
    Vector2d center_cw(0.0, 0.0);
    double radius_cw = 1.0;
    double start_angle_cw = kPi / 2;
    double end_angle_cw = 0.0;
    double z_t_cw = 0.0;

    ASSERT_NO_THROW({
        CircularArc arc(center_cw, radius_cw, start_angle_cw, end_angle_cw, z_t_cw);
    });
    CircularArc arc_cw(center_cw, radius_cw, start_angle_cw, end_angle_cw, z_t_cw);
    EXPECT_TRUE(arc_cw.IsClockwise());
    EXPECT_TRUE(i_num::IsApproxEqual(
            arc_cw.TryGetDefinedPointAt(arc_cw.GetParameterRange()[0]).value(),
            Vector3d(0.0, 1.0, 0.0), kTol));
    EXPECT_TRUE(i_num::IsApproxEqual(
            arc_cw.TryGetDefinedPointAt(arc_cw.GetParameterRange()[1]).value(),
            Vector3d(1.0, 0.0, 0.0), kTol));
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

// 向き: start_angle > end_angleは厳密比較で時計回りの弧、同値は反時計回りの閉円
TEST(CircularArcFactoryTest,
     MakeCircularArcFromAngles_StartExceedingEndYieldsClockwise) {
    const auto cw = i_ent::MakeCircularArc(
            Vector2d(0.0, 0.0), 1.0, kPi / 2.0, 0.0);
    EXPECT_TRUE(cw->IsClockwise());
    EXPECT_NEAR(cw->StartAngle(), kPi / 2.0, kTol);
    EXPECT_NEAR(cw->EndAngle(), 0.0, kTol);
    EXPECT_NEAR(cw->SweepAngle(), kPi / 2.0, kTol);

    const auto closed = i_ent::MakeCircularArc(
            Vector2d(0.0, 0.0), 1.0, kPi / 4.0, kPi / 4.0);
    EXPECT_FALSE(closed->IsClockwise());
    EXPECT_TRUE(closed->IsClosed());
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

// 代表値: 時計回りの3点は入力順のまま時計回りの弧になり、通過点が弧上に乗る
TEST(CircularArcFactoryTest, Clockwise_ThroughPointsKeepsInputOrder) {
    const auto arc = i_ent::MakeCircularArcThroughPoints(
            Vector2d(1.0, 0.0), Vector2d(0.0, -1.0), Vector2d(-1.0, 0.0));

    // 始点(1,0)・終点(-1,0)の時計回りの弧 (θs = 0、Δ = π、終点角 −π)
    EXPECT_TRUE(arc->IsClockwise());
    EXPECT_NEAR(arc->StartAngle(), 0.0, kTol);
    EXPECT_NEAR(arc->EndAngle(), -kPi, kTol);
    EXPECT_NEAR(arc->SweepAngle(), kPi, kTol);

    const auto range = arc->GetParameterRange();
    EXPECT_TRUE(i_num::IsApproxEqual(
            arc->TryGetDefinedPointAt(range[0]).value(),
            Vector3d(1.0, 0.0, 0.0), kTol));
    EXPECT_TRUE(i_num::IsApproxEqual(
            arc->TryGetDefinedPointAt(range[1]).value(),
            Vector3d(-1.0, 0.0, 0.0), kTol));
    // 通過点(0,-1)が弧上 (パラメータ t0 + π/2) に乗る
    const auto mid = arc->TryGetDefinedPointAt(range[0] + kPi / 2.0);
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



/**
 * 時計回りの弧: 生成と評価
 */

// 点指定 + is_clockwise: フラグが保持され、角度・範囲の契約が向きに応じて変わる
TEST(CircularArcTest, Clockwise_ConstructorFromPointsStoresFlag) {
    // 始点(1,0) → 終点(0,1) を時計回りに進む弧 (掃引角 3π/2)
    const auto arc = i_ent::MakeCircularArc(
            Vector2d(0.0, 0.0), Vector2d(1.0, 0.0), Vector2d(0.0, 1.0), 0.0, true);

    EXPECT_TRUE(arc->IsClockwise());
    EXPECT_NEAR(arc->StartAngle(), 0.0, kTol);
    EXPECT_NEAR(arc->SweepAngle(), 3.0 * kPi / 2.0, kTol);
    EXPECT_GT(arc->StartAngle(), arc->EndAngle());
    EXPECT_NEAR(arc->EndAngle(), -3.0 * kPi / 2.0, kTol);
    const auto range = arc->GetParameterRange();
    EXPECT_LT(range[0], range[1]);
    EXPECT_NEAR(range[1] - range[0], arc->SweepAngle(), kTol);

    // 同じ点でis_clockwise = falseなら反時計回りの掃引角 π/2
    const auto ccw = i_ent::MakeCircularArc(
            Vector2d(0.0, 0.0), Vector2d(1.0, 0.0), Vector2d(0.0, 1.0));
    EXPECT_FALSE(ccw->IsClockwise());
    EXPECT_NEAR(ccw->SweepAngle(), kPi / 2.0, kTol);
}

// 角度指定: start > end は例外にならず時計回り、始終点は各角度のcos/sin
TEST(CircularArcTest, Clockwise_AngleConstructorStartGreaterThanEnd) {
    std::shared_ptr<CircularArc> arc;
    ASSERT_NO_THROW(arc = i_ent::MakeCircularArc(
            Vector2d(1.0, -1.0), 2.0, kPi / 2.0, 0.0, 0.5));
    EXPECT_TRUE(arc->IsClockwise());
    const auto range = arc->GetParameterRange();
    EXPECT_TRUE(i_num::IsApproxEqual(
            arc->TryGetDefinedPointAt(range[0]).value(),
            Vector3d(1.0, 1.0, 0.5), kTol));
    EXPECT_TRUE(i_num::IsApproxEqual(
            arc->TryGetDefinedPointAt(range[1]).value(),
            Vector3d(3.0, -1.0, 0.5), kTol));
    EXPECT_NEAR(arc->SweepAngle(), kPi / 2.0, kTol);
}

// 閉じた円のCW: 点集合は同じで、パラメータの進行方向のみ逆
TEST(CircularArcTest, Clockwise_ClosedCircle) {
    const auto circle = i_ent::MakeCircle(Vector2d(0.0, 0.0), 1.0, 0.0, true);

    EXPECT_TRUE(circle->IsClockwise());
    EXPECT_TRUE(circle->IsClosed());
    EXPECT_NEAR(circle->SweepAngle(), 2.0 * kPi, kTol);
    EXPECT_NEAR(circle->StartAngle(), 0.0, kTol);
    EXPECT_NEAR(circle->EndAngle(), -2.0 * kPi, kTol);

    // t0 + π/2 で角度 −π/2 の点 (0, −1) を通る
    const auto range = circle->GetParameterRange();
    EXPECT_TRUE(i_num::IsApproxEqual(
            circle->TryGetDefinedPointAt(range[0] + kPi / 2.0).value(),
            Vector3d(0.0, -1.0, 0.0), kTol));
    // バウンディングボックスはCCWの閉円と同じ
    const auto ccw = i_ent::MakeCircle(Vector2d(0.0, 0.0), 1.0);
    const auto bb_cw = circle->GetDefinedBoundingBox();
    const auto bb_ccw = ccw->GetDefinedBoundingBox();
    EXPECT_TRUE(i_num::IsApproxEqual(bb_cw.GetControl(), bb_ccw.GetControl(), kTol));
    for (size_t i = 0; i < 3; ++i) {
        EXPECT_NEAR(bb_cw.GetSizes()[i], bb_ccw.GetSizes()[i], kTol);
    }
}

// 導関数: C'(t0)が時計回り方向、C''が中心向き (σ² = 1)、C(t1)が終点
TEST(CircularArcTest, Clockwise_DerivativesFollowOrientation) {
    // 始点(1,0) → 終点(0,−1) を時計回りに進む弧 (掃引角 π/2)
    const auto arc = i_ent::MakeCircularArc(
            Vector2d(0.0, 0.0), Vector2d(1.0, 0.0), Vector2d(0.0, -1.0), 0.0, true);
    const auto range = arc->GetParameterRange();

    const auto d0 = arc->TryGetDefinedDerivatives(range[0], 2);
    ASSERT_TRUE(d0.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(d0.value()[0], Vector3d(1.0, 0.0, 0.0), kTol));
    // 接線は (0, −1): 始点ベクトル (1, 0) との外積 (z成分) が負 = 時計回り
    EXPECT_TRUE(i_num::IsApproxEqual(d0.value()[1], Vector3d(0.0, -1.0, 0.0), kTol));
    const double cross = d0.value()[0].x() * d0.value()[1].y()
                       - d0.value()[0].y() * d0.value()[1].x();
    EXPECT_LT(cross, 0.0);
    // 2階導関数は中心向き (−始点ベクトル)
    EXPECT_TRUE(i_num::IsApproxEqual(d0.value()[2], Vector3d(-1.0, 0.0, 0.0), kTol));

    // 終点
    const auto d1 = arc->TryGetDefinedDerivatives(range[1], 1);
    ASSERT_TRUE(d1.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(d1.value()[0], Vector3d(0.0, -1.0, 0.0), kTol));
    EXPECT_TRUE(i_num::IsApproxEqual(d1.value()[1], Vector3d(-1.0, 0.0, 0.0), kTol));

    // 中間点 (t0 + π/4): 角度 −π/4
    const auto mid = arc->TryGetDefinedPointAt(range[0] + kPi / 4.0);
    ASSERT_TRUE(mid.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(
            *mid, Vector3d(std::sqrt(0.5), -std::sqrt(0.5), 0.0), kTol));
}

// バウンディングボックス: CW弧 (θs, Δ) は CCW弧 (θs−Δ, Δ) と一致する
TEST(CircularArcTest, Clockwise_BoundingBoxMatchesReversedCcw) {
    const Vector2d center(0.5, -0.5);
    const double radius = 2.0;
    struct Case {
        double start;  // θs
        double sweep;  // Δ
    };
    // 主要角 (0) を跨ぐ例: θs = π/2 → −π/2、跨がない例: θs = π/3 → π/6
    const std::vector<Case> cases{{kPi / 2.0, kPi}, {kPi / 3.0, kPi / 6.0},
                                  {3.0 * kPi / 2.0, kPi}};
    for (const auto& c : cases) {
        const auto cw = i_ent::MakeCircularArc(
                center, radius, c.start, c.start - c.sweep);
        ASSERT_TRUE(cw->IsClockwise());
        const auto ccw = i_ent::MakeCircularArc(
                center, radius, c.start - c.sweep, c.start);
        ASSERT_FALSE(ccw->IsClockwise());

        const auto bb_cw = cw->GetDefinedBoundingBox();
        const auto bb_ccw = ccw->GetDefinedBoundingBox();
        EXPECT_TRUE(i_num::IsApproxEqual(
                bb_cw.GetControl(), bb_ccw.GetControl(), kTol))
                << "start = " << c.start << ", sweep = " << c.sweep;
        for (size_t i = 0; i < 3; ++i) {
            EXPECT_NEAR(bb_cw.GetSizes()[i], bb_ccw.GetSizes()[i], kTol)
                    << "start = " << c.start << ", sweep = " << c.sweep;
        }
    }

    // 跨ぐ例 (θs = π/2、Δ = π) の具体値: x ∈ [xc, xc+r]、y ∈ [yc−r, yc+r]
    const auto cw = i_ent::MakeCircularArc(center, radius, kPi / 2.0, -kPi / 2.0);
    const auto bb = cw->GetDefinedBoundingBox();
    EXPECT_TRUE(i_num::IsApproxEqual(
            bb.GetControl(), Vector3d(0.5, -2.5, 0.0), kTol));
    EXPECT_NEAR(bb.GetSizes()[0], 2.0, kTol);
    EXPECT_NEAR(bb.GetSizes()[1], 4.0, kTol);
}

// 回帰: 反時計回りの弧では SweepAngle / EndAngle が従来の GetParameterRange と一致
TEST(CircularArcTest, Ccw_RegressionUnchanged) {
    const auto arc = i_ent::MakeCircularArc(
            Vector2d(1.0, 1.0), Vector2d(2.0, 1.0), Vector2d(0.0, 1.0));
    EXPECT_FALSE(arc->IsClockwise());
    const auto range = arc->GetParameterRange();
    EXPECT_NEAR(arc->SweepAngle(), range[1] - range[0], kTol);
    EXPECT_NEAR(arc->EndAngle(), range[1], kTol);
    EXPECT_NEAR(arc->StartAngle(), range[0], kTol);
    EXPECT_NEAR(arc->SweepAngle(), kPi, kTol);
}



/**
 * 時計回りの弧: 出力時展開 (ExpandForExport)
 */

namespace {

/// @brief 展開テスト用の時計回り弧 (中心 (1,2)、z_t = 0.5、始点(2,2) → 終点(1,3))
std::shared_ptr<CircularArc> MakeClockwiseSample() {
    return i_ent::MakeCircularArc(
            Vector2d(1.0, 2.0), Vector2d(2.0, 2.0), Vector2d(1.0, 3.0), 0.5, true);
}

}  // namespace

// 反時計回りの弧は展開不要
TEST(CircularArcTest, Expand_CcwReturnsEmpty) {
    const auto arc = i_ent::MakeCircularArc(
            Vector2d(1.0, 2.0), Vector2d(2.0, 2.0), Vector2d(1.0, 3.0), 0.5);
    const auto expansion = arc->ExpandForExport();
    EXPECT_EQ(expansion.replacement, nullptr);
    EXPECT_TRUE(expansion.auxiliaries.empty());
}

// CW弧は鏡映CCW弧 (置換) と π回転のType 124 (補助) に展開される
TEST(CircularArcTest, Expand_CwProducesMirroredArcAndFlip) {
    const auto arc = MakeClockwiseSample();
    const auto expansion = arc->ExpandForExport();

    // 置換弧: PDは {zt, xc, yc, xs, 2yc−ys, xt, 2yc−yt}、反時計回り
    ASSERT_NE(expansion.replacement, nullptr);
    const auto mirrored =
            std::dynamic_pointer_cast<CircularArc>(expansion.replacement);
    ASSERT_NE(mirrored, nullptr);
    EXPECT_FALSE(mirrored->IsClockwise());
    auto params = mirrored->GetParameters();  // access_asは非const
    ASSERT_EQ(params.size(), 7u);
    const std::vector<double> expected{0.5, 1.0, 2.0, 2.0, 2.0, 1.0, 1.0};
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_NEAR(params.access_as<double>(i), expected[i], kTol) << "i = " << i;
    }

    // 補助: Type 124 ×1、R = diag(1,−1,−1)、T = (0, 2yc, 2zt)、Form 0、
    // 従属スイッチは00 (DE第7欄からの参照は従属関係を作らない)
    ASSERT_EQ(expansion.auxiliaries.size(), 1u);
    const auto flip = std::dynamic_pointer_cast<i_ent::TransformationMatrix>(
            expansion.auxiliaries[0]);
    ASSERT_NE(flip, nullptr);
    igesio::Matrix3d expected_rotation = igesio::Matrix3d::Identity();
    expected_rotation(1, 1) = -1.0;
    expected_rotation(2, 2) = -1.0;
    EXPECT_TRUE(i_num::IsApproxEqual(flip->GetRotation(), expected_rotation, kTol));
    EXPECT_TRUE(i_num::IsApproxEqual(
            flip->GetTranslation(), Vector3d(0.0, 4.0, 1.0), kTol));
    EXPECT_EQ(flip->GetFormNumber(), 0);
    EXPECT_EQ(flip->GetSubordinateEntitySwitch(),
              i_ent::SubordinateEntitySwitch::kIndependent);
    EXPECT_EQ(flip->GetRefTransformation(), nullptr);

    // 置換弧のDE7が補助を指す
    EXPECT_EQ(mirrored->GetTransformationMatrix().GetID(), flip->GetID());
    ASSERT_NE(mirrored->GetTransformationMatrix().GetPointer(), nullptr);
    EXPECT_EQ(mirrored->GetTransformationMatrix().GetPointer()->GetID(),
              flip->GetID());

    // 元の弧は変更されない
    EXPECT_TRUE(arc->IsClockwise());
    EXPECT_EQ(arc->GetTransformationMatrix().GetValueType(),
              i_ent::DEFieldValueType::kDefault);
}

// 点対応: 置換弧を補助行列で変換した点 (モデル空間) が元のCW弧の点と向きを含めて一致
TEST(CircularArcTest, Expand_PointCorrespondence) {
    const auto arc = MakeClockwiseSample();
    const auto expansion = arc->ExpandForExport();
    const auto mirrored =
            std::dynamic_pointer_cast<CircularArc>(expansion.replacement);
    ASSERT_NE(mirrored, nullptr);

    const auto range = arc->GetParameterRange();
    const auto range_m = mirrored->GetParameterRange();
    const double sweep = range[1] - range[0];
    EXPECT_NEAR(range_m[1] - range_m[0], sweep, kTol);

    for (const double u : {0.0, sweep / 3.0, sweep}) {
        // 置換弧のTryGetDerivatives (モデル空間) は自身のDE7 (補助行列) を適用する
        const auto original = arc->TryGetDefinedDerivatives(range[0] + u, 1);
        const auto expanded = mirrored->TryGetDerivatives(range_m[0] + u, 1);
        ASSERT_TRUE(original.has_value());
        ASSERT_TRUE(expanded.has_value());
        EXPECT_TRUE(i_num::IsApproxEqual(
                expanded.value()[0], original.value()[0], kTol)) << "u = " << u;
        EXPECT_TRUE(i_num::IsApproxEqual(
                expanded.value()[1], original.value()[1], kTol)) << "u = " << u;
    }
}

// DEフィールド (色・レベル・ラベル・線種・従属スイッチ・線幅) が置換弧へ複製される
TEST(CircularArcTest, Expand_CopiesDeFields) {
    const auto arc = MakeClockwiseSample();
    const auto color = i_ent::MakeColorDefinitionFromRGB255(10, 20, 30);
    ASSERT_TRUE(arc->OverwriteColor(color));
    ASSERT_TRUE(arc->OverwriteLevel(5));
    ASSERT_TRUE(arc->SetEntityLabel("ARC"));
    ASSERT_TRUE(arc->OverwriteLineFontPattern(i_ent::LineFontPattern::kDashed));
    arc->SetSubordinateEntitySwitch(
            i_ent::SubordinateEntitySwitch::kPhysicallyDependent);
    ASSERT_TRUE(arc->SetLineWeightNumber(3));

    const auto mirrored = arc->ExpandForExport().replacement;
    ASSERT_NE(mirrored, nullptr);
    EXPECT_EQ(mirrored->GetColor().GetID(), color->GetID());
    ASSERT_NE(mirrored->GetColor().GetPointer(), nullptr);
    EXPECT_EQ(mirrored->GetLevel().GetLevelNumber(), 5);
    EXPECT_EQ(mirrored->GetEntityLabel(), "ARC");
    EXPECT_EQ(mirrored->GetLineFontPattern().GetPattern(),
              i_ent::LineFontPattern::kDashed);
    EXPECT_EQ(mirrored->GetSubordinateEntitySwitch(),
              i_ent::SubordinateEntitySwitch::kPhysicallyDependent);
    EXPECT_EQ(mirrored->GetLineWeightNumber(), 3);
    EXPECT_EQ(mirrored->GetFormNumber(), arc->GetFormNumber());
}

// 元の弧がM0を参照していれば 置換弧 → flip → M0 の連鎖になり、モデル空間の点が一致
TEST(CircularArcTest, Expand_ChainsExistingMatrix) {
    const auto arc = MakeClockwiseSample();
    const auto m0 = i_ent::MakeRotation(kPi / 3.0, Vector3d(0.0, 0.0, 1.0),
                                        Vector3d(1.0, 1.0, 1.0));
    ASSERT_TRUE(arc->OverwriteTransformationMatrix(m0));

    const auto expansion = arc->ExpandForExport();
    const auto mirrored =
            std::dynamic_pointer_cast<CircularArc>(expansion.replacement);
    ASSERT_NE(mirrored, nullptr);
    ASSERT_EQ(expansion.auxiliaries.size(), 1u);
    const auto flip = std::dynamic_pointer_cast<i_ent::TransformationMatrix>(
            expansion.auxiliaries[0]);
    ASSERT_NE(flip, nullptr);

    ASSERT_NE(flip->GetRefTransformation(), nullptr);
    EXPECT_EQ(flip->GetRefTransformation()->GetID(), m0->GetID());
    EXPECT_EQ(mirrored->GetTransformationMatrix().GetID(), flip->GetID());
    // 元の弧のDE7はM0のまま
    EXPECT_EQ(arc->GetTransformationMatrix().GetID(), m0->GetID());

    const auto range = arc->GetParameterRange();
    const auto range_m = mirrored->GetParameterRange();
    const double sweep = range[1] - range[0];
    for (const double u : {0.0, sweep / 2.0, sweep}) {
        const auto original = arc->TryGetPointAt(range[0] + u);
        const auto expanded = mirrored->TryGetPointAt(range_m[0] + u);
        ASSERT_TRUE(original.has_value());
        ASSERT_TRUE(expanded.has_value());
        EXPECT_TRUE(i_num::IsApproxEqual(*expanded, *original, kTol)) << "u = " << u;
    }
}

// DE7がID保持のみ (参照先が失効) のCW弧は展開できず ReferenceError
TEST(CircularArcTest, Expand_UnresolvedMatrixThrows) {
    const auto arc = MakeClockwiseSample();
    {
        const auto m0 = i_ent::MakeTranslation(Vector3d(1.0, 0.0, 0.0));
        ASSERT_TRUE(arc->OverwriteTransformationMatrix(m0));
    }
    // DEフィールドは弱参照のため、M0の破棄後はIDのみが残る
    ASSERT_EQ(arc->GetTransformationMatrix().GetValueType(),
              i_ent::DEFieldValueType::kPointer);
    ASSERT_EQ(arc->GetTransformationMatrix().GetPointer(), nullptr);
    EXPECT_THROW(arc->ExpandForExport(), igesio::ReferenceError);
}
