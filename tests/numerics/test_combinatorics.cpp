/**
 * @file numerics/test_combinatorics.cpp
 * @brief numerics/combinatorics.hのテスト
 * @author Yayoi Habami
 * @date 2025-10-23
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>

#include "igesio/numerics/combinatorics.h"

namespace {

namespace i_num = igesio::numerics;

}  // namespace

template <typename T>
class BinomialCoefficientTest : public ::testing::Test {};

using TestTypes = ::testing::Types<int, unsigned int, double>;
TYPED_TEST_SUITE(BinomialCoefficientTest, TestTypes);

TYPED_TEST(BinomialCoefficientTest, BaseCases) {
    using T = TypeParam;

    // 0C0 = 1
    EXPECT_EQ(i_num::BinomialCoefficient<T>(0, 0), 1);

    // nC0 = 1
    EXPECT_EQ(i_num::BinomialCoefficient<T>(5, 0), 1);
    EXPECT_EQ(i_num::BinomialCoefficient<T>(10, 0), 1);
    EXPECT_EQ(i_num::BinomialCoefficient<T>(100, 0), 1);

    // nCn = 1
    EXPECT_EQ(i_num::BinomialCoefficient<T>(5, 5), 1);
    EXPECT_EQ(i_num::BinomialCoefficient<T>(10, 10), 1);
    EXPECT_EQ(i_num::BinomialCoefficient<T>(100, 100), 1);
}

TYPED_TEST(BinomialCoefficientTest, GeneralValues) {
    using T = TypeParam;

    // 4C2 = 6
    EXPECT_EQ(i_num::BinomialCoefficient<T>(4, 2), 6);

    // 5C2 = 5C3 = 10
    EXPECT_EQ(i_num::BinomialCoefficient<T>(5, 2), 10);
    EXPECT_EQ(i_num::BinomialCoefficient<T>(5, 3), 10);

    // 6C3 = 20
    EXPECT_EQ(i_num::BinomialCoefficient<T>(6, 3), 20);

    // 10C4 = 10C6 = 210
    EXPECT_EQ(i_num::BinomialCoefficient<T>(10, 4), 210);
    EXPECT_EQ(i_num::BinomialCoefficient<T>(10, 6), 210);

    // 20C10 = 184756
    EXPECT_EQ(i_num::BinomialCoefficient<T>(20, 10), 184756);
}

// nCr, r > nの場合は0を返す
TYPED_TEST(BinomialCoefficientTest, RGreaterThanN) {
    using T = TypeParam;

    EXPECT_EQ(i_num::BinomialCoefficient<T>(5, 6), 0);
    EXPECT_EQ(i_num::BinomialCoefficient<T>(0, 1), 0);
    EXPECT_EQ(i_num::BinomialCoefficient<T>(10, 15), 0);
}

// nCr, n < 0 または r < 0の場合は0を返す
TYPED_TEST(BinomialCoefficientTest, NegativeNOrR) {
    using T = TypeParam;
    // Tが符号付き型の場合のみテストを実行
    if constexpr (std::is_signed_v<T>) {
        EXPECT_EQ(i_num::BinomialCoefficient<T>(-5, 2), 0);
        EXPECT_EQ(i_num::BinomialCoefficient<T>(5, -2), 0);
        EXPECT_EQ(i_num::BinomialCoefficient<T>(-3, -1), 0);
        EXPECT_EQ(i_num::BinomialCoefficient<T>(-1, 0), 0);
        EXPECT_EQ(i_num::BinomialCoefficient<T>(0, -1), 0);
        EXPECT_EQ(i_num::BinomialCoefficient<T>(-10, -5), 0);
    }
}

// unsigned int型の限界に近い値のテスト
TYPED_TEST(BinomialCoefficientTest, LargeValues) {
    using T = TypeParam;
    if constexpr (std::is_same_v<T, unsigned int> ||
                  std::is_same_v<T, double>) {
        // C(30, 15) = 155,117,520
        EXPECT_EQ(i_num::BinomialCoefficient<T>(30, 15), 155117520);

        // C(32, 16) = 601,080,390
        EXPECT_EQ(i_num::BinomialCoefficient<T>(32, 16), 601080390);

        // C(35, 16) = 4,059,928,950
        // (32-bit unsigned int の最大値 4,294,967,295 未満)
        EXPECT_EQ(i_num::BinomialCoefficient<T>(35, 16), 4059928950);

        // C(34, 1) = 34
        EXPECT_EQ(i_num::BinomialCoefficient<T>(34, 1), 34);
    } else if constexpr (std::is_same_v<T, int>) {
        // C(37, 12) = 1,852,482,996
        EXPECT_EQ(i_num::BinomialCoefficient<T>(37, 12), 1852482996);

        // 32-bit signed int の限界に近い値のテスト -> std::invalid_argument例外
        // C(34, 16) = 2,203,961,430 > 2,147,483,647
        EXPECT_THROW(i_num::BinomialCoefficient<T>(34, 16), std::invalid_argument);

        // C(35, 16) > 4,059,928,950
        EXPECT_THROW(i_num::BinomialCoefficient<T>(35, 16), std::invalid_argument);
    }
}

// 浮動小数点型でのテスト
// nやrに小数部分がある場合、小数部分は無視される
TYPED_TEST(BinomialCoefficientTest, FloatingPointValues) {
    using T = TypeParam;
    if constexpr (std::is_floating_point_v<T>) {
        // 5.7C2.3 = 5C2 = 10
        EXPECT_EQ(i_num::BinomialCoefficient<T>(5.7, 2.3), 10);

        // 6.9C3.1 = 6C3 = 20
        EXPECT_EQ(i_num::BinomialCoefficient<T>(6.9, 3.1), 20);

        // 10.5C4.9 = 10C4 = 210
        EXPECT_EQ(i_num::BinomialCoefficient<T>(10.5, 4.9), 210);

        // 20.2C10.8 = 20C10 = 184756
        EXPECT_EQ(i_num::BinomialCoefficient<T>(20.2, 10.8), 184756);
    }
}
