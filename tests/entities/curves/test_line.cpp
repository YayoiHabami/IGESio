/**
 * @file entities/curves/test_line.cpp
 * @brief Line (Type 110) のファクトリ関数のテスト
 * @author Yayoi Habami
 * @date 2026-06-05
 * @copyright 2026 Yayoi Habami
 * @note 対象: MakeLine / MakeRay / MakeUnboundedLine
 * @note TODO: Lineエンティティ本体 (コンストラクタ・ICurve実装) のテストは未整備
 */
#include <gtest/gtest.h>

#include <cmath>

#include "igesio/common/errors.h"
#include "igesio/numerics/core/tolerance.h"
#include "igesio/entities/curves/line.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using igesio::Vector3d;
using i_ent::LineType;
/// @brief 浮動小数点比較の許容誤差
constexpr double kTol = 1e-9;

}  // namespace



/**
 * ファクトリ関数: MakeLine
 */

// 代表値: 2点が格納され、デフォルトは線分 (パラメータ範囲 [0, 1])
TEST(LineFactoryTest, MakeLine_StoresEndpointsAsSegment) {
    const Vector3d p1(1.0, 2.0, 3.0), p2(4.0, 6.0, 3.0);
    const auto line = i_ent::MakeLine(p1, p2);

    ASSERT_NE(line, nullptr);
    EXPECT_EQ(line->GetLineType(), LineType::kSegment);
    const auto [start, end] = line->GetDefinedAnchorPoints();
    EXPECT_TRUE(i_num::IsApproxEqual(start, p1, kTol));
    EXPECT_TRUE(i_num::IsApproxEqual(end, p2, kTol));
    const auto range = line->GetParameterRange();
    EXPECT_NEAR(range[0], 0.0, kTol);
    EXPECT_NEAR(range[1], 1.0, kTol);
    EXPECT_TRUE(line->IsValid());
}

// 代表値: line_typeの明示指定が反映される (kRay / kLine)
TEST(LineFactoryTest, MakeLine_HonorsExplicitLineType) {
    const Vector3d p1(0.0, 0.0, 0.0), p2(1.0, 0.0, 0.0);

    const auto ray = i_ent::MakeLine(p1, p2, LineType::kRay);
    EXPECT_EQ(ray->GetLineType(), LineType::kRay);
    const auto ray_range = ray->GetParameterRange();
    EXPECT_NEAR(ray_range[0], 0.0, kTol);
    EXPECT_TRUE(std::isinf(ray_range[1]));

    const auto unbounded = i_ent::MakeLine(p1, p2, LineType::kLine);
    EXPECT_EQ(unbounded->GetLineType(), LineType::kLine);
    const auto line_range = unbounded->GetParameterRange();
    EXPECT_TRUE(std::isinf(line_range[0]) && line_range[0] < 0.0);
    EXPECT_TRUE(std::isinf(line_range[1]) && line_range[1] > 0.0);
}

// エラー+境界精度: 同一2点は棄却される (コンストラクタの厳密比較を透過)。
// 1成分でもビット表現が異なれば受理される
TEST(LineFactoryTest, MakeLine_ThrowsEntityValueErrorWhenPointsCoincide) {
    EXPECT_THROW(i_ent::MakeLine(
            Vector3d(1.0, 1.0, 1.0), Vector3d(1.0, 1.0, 1.0)),
            igesio::EntityValueError);
    EXPECT_NO_THROW(i_ent::MakeLine(
            Vector3d(1.0, 1.0, 1.0), Vector3d(1.0, 1.0, 1.0 + 1e-12)));
}



/**
 * ファクトリ関数: MakeRay / MakeUnboundedLine
 */

// 代表値: P1=origin、P2=origin+directionで半直線が生成される
TEST(LineFactoryTest, MakeRay_StoresOriginAndDirectionPoint) {
    const Vector3d origin(1.0, -2.0, 0.5), direction(0.0, 3.0, 4.0);
    const auto ray = i_ent::MakeRay(origin, direction);

    ASSERT_NE(ray, nullptr);
    EXPECT_EQ(ray->GetLineType(), LineType::kRay);
    const auto [p1, p2] = ray->GetDefinedAnchorPoints();
    EXPECT_TRUE(i_num::IsApproxEqual(p1, origin, kTol));
    EXPECT_TRUE(i_num::IsApproxEqual(p2, Vector3d(1.0, 1.0, 4.5), kTol));
}

// エラー+境界精度: 方向ノルムがkGeometryTolerance (1e-9) の内側→throw、
// すぐ外→受理
TEST(LineFactoryTest, MakeRay_ThrowsEntityValueErrorWhenDirectionNearZero) {
    EXPECT_THROW(i_ent::MakeRay(Vector3d::Zero(), Vector3d::Zero()),
                 igesio::EntityValueError);
    EXPECT_THROW(i_ent::MakeRay(Vector3d::Zero(), Vector3d(1e-10, 0.0, 0.0)),
                 igesio::EntityValueError);
    EXPECT_NO_THROW(i_ent::MakeRay(Vector3d::Zero(), Vector3d(1e-8, 0.0, 0.0)));
}

// 代表値: P1=point、P2=point+directionで直線が生成される
TEST(LineFactoryTest, MakeUnboundedLine_StoresPointAndDirectionPoint) {
    const Vector3d point(0.0, 1.0, 0.0), direction(2.0, 0.0, -1.0);
    const auto line = i_ent::MakeUnboundedLine(point, direction);

    ASSERT_NE(line, nullptr);
    EXPECT_EQ(line->GetLineType(), LineType::kLine);
    const auto [p1, p2] = line->GetDefinedAnchorPoints();
    EXPECT_TRUE(i_num::IsApproxEqual(p1, point, kTol));
    EXPECT_TRUE(i_num::IsApproxEqual(p2, Vector3d(2.0, 1.0, -1.0), kTol));
}

// エラー+境界精度: 方向ノルムの境界 (MakeRayと同一の検証)
TEST(LineFactoryTest,
     MakeUnboundedLine_ThrowsEntityValueErrorWhenDirectionNearZero) {
    EXPECT_THROW(i_ent::MakeUnboundedLine(
            Vector3d(1.0, 1.0, 1.0), Vector3d(0.0, 1e-10, 0.0)),
            igesio::EntityValueError);
    EXPECT_NO_THROW(i_ent::MakeUnboundedLine(
            Vector3d(1.0, 1.0, 1.0), Vector3d(0.0, 1e-8, 0.0)));
}
