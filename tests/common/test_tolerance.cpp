/**
 * @file common/test_tolerance.cpp
 * @brief common/tolerance.hのテスト
 * @author Yayoi Habami
 * @date 2025-08-11
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include "igesio/common/tolerance.h"



TEST(ToleranceTest, IsApproxZero) {
    EXPECT_TRUE(igesio::IsApproxZero(0.0));
    EXPECT_TRUE(igesio::IsApproxZero(1e-14));
    EXPECT_FALSE(igesio::IsApproxZero(1.0));
    EXPECT_TRUE(igesio::IsApproxZero(-1e-14));
    EXPECT_FALSE(igesio::IsApproxZero(-1.0));
    EXPECT_TRUE(igesio::IsApproxZero(1e-6, 1e-5));
    EXPECT_FALSE(igesio::IsApproxZero(1e-4, 1e-5));
}

TEST(ToleranceTest, IsApproxOne) {
    EXPECT_TRUE(igesio::IsApproxOne(1.0));
    EXPECT_TRUE(igesio::IsApproxOne(1.0 + 1e-14));
    EXPECT_FALSE(igesio::IsApproxOne(0.0));
    EXPECT_TRUE(igesio::IsApproxOne(1.0 - 1e-14));
    EXPECT_FALSE(igesio::IsApproxOne(2.0));
    EXPECT_TRUE(igesio::IsApproxOne(1.0 + 1e-6, 1e-5));
    EXPECT_FALSE(igesio::IsApproxOne(1.0 + 1e-4, 1e-5));
}

TEST(ToleranceTest, IsApproxEqual) {
    EXPECT_TRUE(igesio::IsApproxEqual(1.0, 1.0));
    EXPECT_TRUE(igesio::IsApproxEqual(1.0, 1.0 + 1e-14));
    EXPECT_FALSE(igesio::IsApproxEqual(1.0, 0.0));
    EXPECT_TRUE(igesio::IsApproxEqual(1.0, 1.0 - 1e-14));
    EXPECT_FALSE(igesio::IsApproxEqual(1.0, 2.0));
    EXPECT_TRUE(igesio::IsApproxEqual(1.0, 1.0 + 1e-6, 1e-5));
    EXPECT_FALSE(igesio::IsApproxEqual(1.0, 1.0 + 1e-4, 1e-5));
}

TEST(ToleranceTest, IsApproxEqualMatrix) {
    igesio::Matrix3d a;
    a << 1.0, 2.0, 3.0,
         4.0, 5.0, 6.0,
         7.0, 8.0, 9.0;
    igesio::Matrix3d b = a;
    EXPECT_TRUE(igesio::IsApproxEqual(a, b));
    b(0, 0) += 1e-14;
    EXPECT_TRUE(igesio::IsApproxEqual(a, b));
    b(0, 0) += 1e-4;
    EXPECT_FALSE(igesio::IsApproxEqual(a, b));

    igesio::Matrix<double, 2, 3> c;
    c << 1.0, 2.0, 3.0,
         4.0, 5.0, 6.0;
    igesio::Matrix<double, 2, 3> d = c;
    EXPECT_TRUE(igesio::IsApproxEqual(c, d));
    d(0, 0) += 1e-14;
    EXPECT_TRUE(igesio::IsApproxEqual(c, d));
    d(0, 0) += 1e-4;
    EXPECT_FALSE(igesio::IsApproxEqual(c, d));
}
