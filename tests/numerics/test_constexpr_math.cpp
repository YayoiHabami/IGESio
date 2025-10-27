/**
 * @file numerics/test_constexpr_math.cpp
 * @brief numerics/constexpr_math.hのテスト
 * @author Yayoi Habami
 * @date 2025-10-20
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>

#include "igesio/numerics/constexpr_math.h"

namespace {

namespace i_num = igesio::numerics;

}  // namespace



/**
 * abs_cのテスト
 */

// constexprでのチェック

TEST(ConstexprMathTest, AbsCConstexpr) {
    constexpr double val1 = -5.0;
    constexpr double val2 = 3.2;
    constexpr double val3 = 0.0;

    constexpr double abs1 = i_num::abs_c(val1);
    constexpr double abs2 = i_num::abs_c(val2);
    constexpr double abs3 = i_num::abs_c(val3);

    EXPECT_DOUBLE_EQ(abs1, 5.0);
    EXPECT_DOUBLE_EQ(abs2, 3.2);
    EXPECT_DOUBLE_EQ(abs3, 0.0);
}

// 実行時でのチェック
TEST(ConstexprMathTest, AbsCRuntime) {
    double val1 = -7.5;
    double val2 = 4.3;
    double val3 = 0.0;

    double abs1 = i_num::abs_c(val1);
    double abs2 = i_num::abs_c(val2);
    double abs3 = i_num::abs_c(val3);

    EXPECT_DOUBLE_EQ(abs1, 7.5);
    EXPECT_DOUBLE_EQ(abs2, 4.3);
    EXPECT_DOUBLE_EQ(abs3, 0.0);
}

// 適当な値でのチェック
TEST(ConstexprMathTest, AbsCRandomValues) {
    srand(0);

    // ランダムな負の値での検証
    for (int i = 0; i < 100; ++i) {
        double val = -static_cast<double>(rand()) / RAND_MAX * 100.0;
        double abs_val = i_num::abs_c(val);
        EXPECT_TRUE(abs_val >= 0.0) << "abs_c(" << val << ") = " << abs_val;
        EXPECT_DOUBLE_EQ(abs_val, -val) << "abs_c(" << val << ") = " << abs_val;
    }

    // ランダムな正の値での検証
    for (int i = 0; i < 100; ++i) {
        double val = static_cast<double>(rand()) / RAND_MAX * 100.0;
        double abs_val = i_num::abs_c(val);
        EXPECT_TRUE(abs_val >= 0.0) << "abs_c(" << val << ") = " << abs_val;
        EXPECT_DOUBLE_EQ(abs_val, val) << "abs_c(" << val << ") = " << abs_val;
    }
}



/**
 * sqrt_cのテスト
 */

namespace {

/// @brief 精度の評価
/// @param root sqrt_cで計算された平方根
/// @param expected 期待される元の数値
bool IsApproxSqrt(const double root, const double expected) {
    // 以下の条件を満たす場合に、誤差が許容範囲内とみなす
    // 1. 絶対誤差が許容範囲内
    // 2. 絶対誤差が許容範囲外だが、相対誤差が許容範囲内
    double abs_diff = std::abs(root * root - expected);
    if (abs_diff < i_num::kSqrtTolerance) return true;
    if (expected != 0.0) {
        double rel_diff = abs_diff / std::abs(expected);
        if (rel_diff < i_num::kSqrtTolerance) return true;
    }
    return false;
}

}  // namespace

// constexprでのチェック
TEST(ConstexprMathTest, SqrtCConstexpr) {
    constexpr double val1 = 16.0;
    constexpr double val2 = 2.25;
    constexpr double val3 = 0.0;
    constexpr double val4 = 0.1369 * 0.1369;

    constexpr double sqrt1 = i_num::sqrt_c(val1);
    constexpr double sqrt2 = i_num::sqrt_c(val2);
    constexpr double sqrt3 = i_num::sqrt_c(val3);
    constexpr double sqrt4 = i_num::sqrt_c(val4);

    EXPECT_TRUE(IsApproxSqrt(sqrt1, val1)) << "sqrt1: " << sqrt1;
    EXPECT_TRUE(IsApproxSqrt(sqrt2, val2)) << "sqrt2: " << sqrt2;
    EXPECT_TRUE(IsApproxSqrt(sqrt3, val3)) << "sqrt3: " << sqrt3;
    EXPECT_TRUE(IsApproxSqrt(sqrt4, val4)) << "sqrt4: " << sqrt4;
}

// 実行時でのチェック
TEST(ConstexprMathTest, SqrtCRuntime) {
    double val1 = 25.0;
    double val2 = 3.24;
    double val3 = 0.0;
    double val4 = 0.1369 * 0.1369;

    double sqrt1 = i_num::sqrt_c(val1);
    double sqrt2 = i_num::sqrt_c(val2);
    double sqrt3 = i_num::sqrt_c(val3);
    double sqrt4 = i_num::sqrt_c(val4);

    EXPECT_TRUE(IsApproxSqrt(sqrt1, val1)) << "sqrt1: " << sqrt1;
    EXPECT_TRUE(IsApproxSqrt(sqrt2, val2)) << "sqrt2: " << sqrt2;
    EXPECT_TRUE(IsApproxSqrt(sqrt3, val3)) << "sqrt3: " << sqrt3;
    EXPECT_TRUE(IsApproxSqrt(sqrt4, val4)) << "sqrt4: " << sqrt4;
}

// 適当な値でのチェック
TEST(ConstexprMathTest, SqrtCRandomValues) {
    srand(0);

    // [0, 1)での検証
    for (int i = 0; i < 100; ++i) {
        double val = static_cast<double>(rand()) / RAND_MAX;
        double sqrt_val = i_num::sqrt_c(val);
        double diff = std::abs(sqrt_val * sqrt_val - val);
        EXPECT_TRUE(IsApproxSqrt(sqrt_val, val))
            << "sqrt_c: " << sqrt_val << ", actual: "
            << std::sqrt(val) << "(diff: " << diff << ")";
    }

    // 1以上の数値での検証
    for (int i = 0; i < 100; ++i) {
        double val = static_cast<double>(rand());
        double sqrt_val = i_num::sqrt_c(val);
        double diff = std::abs(sqrt_val * sqrt_val - val);
        EXPECT_TRUE(IsApproxSqrt(sqrt_val, val))
            << "sqrt_c: " << sqrt_val << ", actual: "
            << std::sqrt(val) << "(diff: " << diff << ")";
    }
}
