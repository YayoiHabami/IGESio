/**
 * @file numerics/test_tolerance.cpp
 * @brief numerics/tolerance.hのテスト
 * @author Yayoi Habami
 * @date 2025-08-11
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include "igesio/numerics/tolerance.h"

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
