/**
 * @file entities/curves/test_point.cpp
 * @brief Point (Type 116) のファクトリ関数のテスト
 * @author Yayoi Habami
 * @date 2026-06-05
 * @copyright 2026 Yayoi Habami
 * @note 対象: MakePoint
 * @note TODO: Pointエンティティ本体 (PDコンストラクタ・サブフィギュア参照) の
 *       テストは未整備
 * @note エラーケースのテストはない (MakePointは検証を持たず、
 *       Point::ValidatePDもサブフィギュア参照のみを検査するため)
 */
#include <gtest/gtest.h>

#include "igesio/numerics/core/tolerance.h"
#include "igesio/entities/curves/point.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using igesio::Vector3d;
/// @brief 浮動小数点比較の許容誤差
constexpr double kTol = 1e-9;

}  // namespace



/**
 * ファクトリ関数: MakePoint
 */

// 代表値: 位置が格納され、バウンディングボックスはサイズ0
TEST(PointFactoryTest, MakePoint_StoresPosition) {
    const Vector3d position(1.0, -2.0, 3.5);
    const auto point = i_ent::MakePoint(position);

    ASSERT_NE(point, nullptr);
    EXPECT_TRUE(i_num::IsApproxEqual(
            point->GetDefinedPosition(), position, kTol));
    EXPECT_TRUE(point->IsValid());

    // バウンディングボックスは基点=position、各方向のサイズ0
    // (directions_は常に単位ベクトルのため、大きさはGetSizes()で検証する)
    const auto bbox = point->GetDefinedBoundingBox();
    EXPECT_TRUE(i_num::IsApproxEqual(bbox.GetControl(), position, kTol));
    for (const auto size : bbox.GetSizes()) {
        EXPECT_NEAR(size, 0.0, kTol);
    }
}
