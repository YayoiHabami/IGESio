/**
 * @file entities/curves/test_copious_data.cpp
 * @brief CopiousData (Type 106, Forms 1-3) のファクトリ関数のテスト
 * @author Yayoi Habami
 * @date 2026-06-05
 * @copyright 2026 Yayoi Habami
 * @note 対象: MakeCopiousData (2D点列 / 3D点列 / 点列+ベクトル)
 * @note TODO: CopiousDataエンティティ本体 (コンストラクタ・ICurve実装) の
 *       テストは未整備
 */
#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/numerics/core/tolerance.h"
#include "igesio/entities/curves/copious_data.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using igesio::Vector2d;
using igesio::Vector3d;
using i_ent::CopiousDataType;
/// @brief 浮動小数点比較の許容誤差
constexpr double kTol = 1e-9;

}  // namespace



/**
 * ファクトリ関数: MakeCopiousData
 */

// 代表値: 2D点列 → kPlanarPoints (Form 1)、全点のz=z_t
TEST(CopiousDataFactoryTest, MakeCopiousData2D_CreatesPlanarPointsWithZt) {
    const std::vector<Vector2d> points{{0.0, 0.0}, {1.0, 2.0}, {-1.0, 3.0}};
    const auto data = i_ent::MakeCopiousData(points, 2.5);

    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->GetDataType(), CopiousDataType::kPlanarPoints);
    ASSERT_EQ(data->GetCount(), points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        EXPECT_TRUE(i_num::IsApproxEqual(
                data->Coordinate(i),
                Vector3d(points[i].x(), points[i].y(), 2.5), kTol));
    }
    // 離散な点列であるため、閉曲線にはならず長さも持たない
    EXPECT_FALSE(data->IsClosed());
    EXPECT_NEAR(data->Length(), 0.0, kTol);
    EXPECT_TRUE(data->IsValid());
}

// 代表値: 3D点列 → kPoints3D (Form 2)
TEST(CopiousDataFactoryTest, MakeCopiousData3D_CreatesPoints3D) {
    const std::vector<Vector3d> points{
            {0.0, 0.0, 1.0}, {1.0, 2.0, -1.0}, {3.0, -2.0, 0.5}};
    const auto data = i_ent::MakeCopiousData(points);

    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->GetDataType(), CopiousDataType::kPoints3D);
    ASSERT_EQ(data->GetCount(), points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        EXPECT_TRUE(i_num::IsApproxEqual(data->Coordinate(i), points[i], kTol));
    }
    EXPECT_TRUE(data->IsValid());
}

// 代表値: 点列+ベクトル → kSextuples (Form 3)、両方が格納される
TEST(CopiousDataFactoryTest, MakeCopiousDataWithVectors_CreatesSextuples) {
    const std::vector<Vector3d> points{{0.0, 0.0, 0.0}, {1.0, 1.0, 1.0}};
    const std::vector<Vector3d> vectors{{0.0, 0.0, 1.0}, {1.0, 0.0, 0.0}};
    const auto data = i_ent::MakeCopiousData(points, vectors);

    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->GetDataType(), CopiousDataType::kSextuples);
    ASSERT_EQ(data->GetCount(), points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        EXPECT_TRUE(i_num::IsApproxEqual(data->Coordinate(i), points[i], kTol));
        EXPECT_TRUE(i_num::IsApproxEqual(data->Addition(i), vectors[i], kTol));
    }
    EXPECT_TRUE(data->IsValid());
}

// エラー+境界精度: 2点未満→throw、2点ちょうど→受理 (2D/3D両方)
TEST(CopiousDataFactoryTest,
     MakeCopiousData_ThrowsEntityValueErrorWhenFewerThanTwoPoints) {
    EXPECT_THROW(i_ent::MakeCopiousData(std::vector<Vector2d>{}),
                 igesio::EntityValueError);
    EXPECT_THROW(i_ent::MakeCopiousData(std::vector<Vector2d>{{0.0, 0.0}}),
                 igesio::EntityValueError);
    EXPECT_THROW(i_ent::MakeCopiousData(
            std::vector<Vector3d>{{0.0, 0.0, 0.0}}),
            igesio::EntityValueError);
    // 境界: 2点ちょうどは受理される
    EXPECT_NO_THROW(i_ent::MakeCopiousData(
            std::vector<Vector2d>{{0.0, 0.0}, {1.0, 0.0}}));
    EXPECT_NO_THROW(i_ent::MakeCopiousData(
            std::vector<Vector3d>{{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}}));
}

// エラー: 点とベクトルの数の不一致→throw、同数→受理
TEST(CopiousDataFactoryTest,
     MakeCopiousDataWithVectors_ThrowsEntityValueErrorWhenSizeMismatch) {
    const std::vector<Vector3d> points{
            {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {2.0, 0.0, 0.0}};
    EXPECT_THROW(i_ent::MakeCopiousData(points, std::vector<Vector3d>{
            {0.0, 0.0, 1.0}, {0.0, 0.0, 1.0}}),
            igesio::EntityValueError);
    EXPECT_NO_THROW(i_ent::MakeCopiousData(points, std::vector<Vector3d>{
            {0.0, 0.0, 1.0}, {0.0, 0.0, 1.0}, {0.0, 0.0, 1.0}}));
}
