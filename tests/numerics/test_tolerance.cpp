/**
 * @file numerics/test_tolerance.cpp
 * @brief numerics/core/tolerance.hのテスト
 * @author Yayoi Habami
 * @date 2025-08-11
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "igesio/numerics/core/tolerance.h"

namespace {

namespace i_num = igesio::numerics;

}  // namespace



TEST(ToleranceTest, IsApproxZero) {
    EXPECT_TRUE(i_num::IsApproxZero(0.0));
    EXPECT_TRUE(i_num::IsApproxZero(1e-14));
    EXPECT_FALSE(i_num::IsApproxZero(1.0));
    EXPECT_TRUE(i_num::IsApproxZero(-1e-14));
    EXPECT_FALSE(i_num::IsApproxZero(-1.0));
    EXPECT_TRUE(i_num::IsApproxZero(1e-6, 1e-5));
    EXPECT_FALSE(i_num::IsApproxZero(1e-4, 1e-5));
}

TEST(ToleranceTest, IsApproxOne) {
    EXPECT_TRUE(i_num::IsApproxOne(1.0));
    EXPECT_TRUE(i_num::IsApproxOne(1.0 + 1e-14));
    EXPECT_FALSE(i_num::IsApproxOne(0.0));
    EXPECT_TRUE(i_num::IsApproxOne(1.0 - 1e-14));
    EXPECT_FALSE(i_num::IsApproxOne(2.0));
    EXPECT_TRUE(i_num::IsApproxOne(1.0 + 1e-6, 1e-5));
    EXPECT_FALSE(i_num::IsApproxOne(1.0 + 1e-4, 1e-5));
}

TEST(ToleranceTest, IsApproxEqual) {
    EXPECT_TRUE(i_num::IsApproxEqual(1.0, 1.0));
    EXPECT_TRUE(i_num::IsApproxEqual(1.0, 1.0 + 1e-14));
    EXPECT_FALSE(i_num::IsApproxEqual(1.0, 0.0));
    EXPECT_TRUE(i_num::IsApproxEqual(1.0, 1.0 - 1e-14));
    EXPECT_FALSE(i_num::IsApproxEqual(1.0, 2.0));
    EXPECT_TRUE(i_num::IsApproxEqual(1.0, 1.0 + 1e-6, 1e-5));
    EXPECT_FALSE(i_num::IsApproxEqual(1.0, 1.0 + 1e-4, 1e-5));
}

TEST(ToleranceTest, IsApproxEqualMatrix) {
    igesio::Matrix3d a;
    a << 1.0, 2.0, 3.0,
         4.0, 5.0, 6.0,
         7.0, 8.0, 9.0;
    igesio::Matrix3d b = a;
    EXPECT_TRUE(i_num::IsApproxEqual(a, b));
    b(0, 0) += 1e-14;
    EXPECT_TRUE(i_num::IsApproxEqual(a, b));
    b(0, 0) += 1e-4;
    EXPECT_FALSE(i_num::IsApproxEqual(a, b));

    igesio::Matrix<double, 2, 3> c;
    c << 1.0, 2.0, 3.0,
         4.0, 5.0, 6.0;
    igesio::Matrix<double, 2, 3> d = c;
    EXPECT_TRUE(i_num::IsApproxEqual(c, d));
    d(0, 0) += 1e-14;
    EXPECT_TRUE(i_num::IsApproxEqual(c, d));
    d(0, 0) += 1e-4;
    EXPECT_FALSE(i_num::IsApproxEqual(c, d));
}

TEST(ToleranceTest, TryClampToRange_WithinRange) {
    // 範囲内の値はそのまま返る
    auto r = i_num::TryClampToRange(0.5, 0.0, 1.0);
    ASSERT_TRUE(r.has_value());
    EXPECT_DOUBLE_EQ(*r, 0.5);
}

TEST(ToleranceTest, TryClampToRange_AtBoundary) {
    // 下端・上端ちょうどの値はその端点が返る
    auto lo = i_num::TryClampToRange(0.0, 0.0, 1.0);
    auto hi = i_num::TryClampToRange(1.0, 0.0, 1.0);
    ASSERT_TRUE(lo.has_value());
    ASSERT_TRUE(hi.has_value());
    EXPECT_DOUBLE_EQ(*lo, 0.0);
    EXPECT_DOUBLE_EQ(*hi, 1.0);
}

TEST(ToleranceTest, TryClampToRange_OvershootWithinToleranceIsClamped) {
    // 許容誤差内で僅かに域外へ出た境界値は端点へ丸められる
    // (報告事例: 代表v値が上端を1 ULP超過するケースに相当)
    const double hi = 1.0;
    const double over = std::nextafter(hi, 2.0);  // hi + 1 ULP
    auto ro = i_num::TryClampToRange(over, 0.0, hi);
    ASSERT_TRUE(ro.has_value());
    EXPECT_DOUBLE_EQ(*ro, hi);

    const double lo = 0.0;
    const double under = std::nextafter(lo, -1.0);  // lo - 1 ULP
    auto ru = i_num::TryClampToRange(under, lo, 1.0);
    ASSERT_TRUE(ru.has_value());
    EXPECT_DOUBLE_EQ(*ru, lo);
}

TEST(ToleranceTest, TryClampToRange_OutOfRangeReturnsNullopt) {
    // 許容誤差を明確に超える域外値はstd::nulloptを返す
    EXPECT_FALSE(i_num::TryClampToRange(1.0 + 1e-9, 0.0, 1.0).has_value());
    EXPECT_FALSE(i_num::TryClampToRange(-1e-9, 0.0, 1.0).has_value());
    EXPECT_FALSE(i_num::TryClampToRange(2.0, 0.0, 1.0).has_value());
}

TEST(ToleranceTest, TryClampToRange_InfiniteBoundsPassThrough) {
    // 無限端の範囲は任意の有限値を素通しする
    constexpr double kInf = std::numeric_limits<double>::infinity();
    auto r = i_num::TryClampToRange(1e10, -kInf, kInf);
    ASSERT_TRUE(r.has_value());
    EXPECT_DOUBLE_EQ(*r, 1e10);
}

TEST(ToleranceTest, TryClampToRange_CustomTolerance) {
    // 許容誤差を指定した場合の境界判定
    EXPECT_TRUE(i_num::TryClampToRange(1.0 + 1e-6, 0.0, 1.0, 1e-5).has_value());
    EXPECT_FALSE(i_num::TryClampToRange(1.0 + 1e-4, 0.0, 1.0, 1e-5).has_value());
}
