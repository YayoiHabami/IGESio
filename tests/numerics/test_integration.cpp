/**
 * @file numerics/test_integration.cpp
 * @brief numerics/integration.hのテスト
 * @author Yayoi Habami
 * @date 2025-10-20
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <cmath>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "igesio/numerics/integration.h"

namespace {

namespace i_num = igesio::numerics;

/// @brief 誤差の検証
/// @param computed 計算結果
/// @param expected 真値
/// @param tolerance 許容誤差
/// @return 許容誤差内であれば空文字列、そうでなければエラーメッセージ
std::string CheckError(const double computed, const double expected,
                       const i_num::Tolerance& tolerance) {
    // 絶対誤差と相対誤差の両方を評価
    auto abs_tol = tolerance.abs_tol;
    auto rel_tol = tolerance.rel_tol;

    double abs_error = std::fabs(computed - expected);
    double rel_error = (expected != 0.0) ? abs_error / std::fabs(expected) : abs_error;

    // 許容範囲内かどうかを判定
    std::string msg;
    if (abs_error > abs_tol && rel_error > rel_tol) {
        msg += "Absolute error " + std::to_string(abs_error)
             + " exceeds tolerance " + std::to_string(abs_tol) + ". "
             + "Relative error " + std::to_string(rel_error)
             + " exceeds tolerance " + std::to_string(rel_tol) + ". ";
    }

    if (msg.empty()) return "";

    // 2つの値の情報も併せて返す
    msg += "(Computed: " + std::to_string(computed)
        +  ", Expected: " + std::to_string(expected) + ")";
    return msg;
}

/// @brief テスト用関数: f(x) = x^2
double f_x_1(double x) { return x * x; }
/// @brief f_x_1の不定積分: F(x) = (1/3)x^3
double int_f_x_1(double x) { return (1.0 / 3.0) * x * x * x; }

/// @brief テスト用関数: f(x) = 5x^6 - 3x^4 + 2x - 7
double f_x_2(double x) {
    return 5.0 * std::pow(x, 6) - 3.0 * std::pow(x, 4) + 2.0 * x - 7.0;
}
/// @brief f_x_2の不定積分: F(x) = (5/7)x^7 - (3/5)x^5 + x^2 - 7x
double int_f_x_2(double x) {
    return (5.0 / 7.0) * std::pow(x, 7) - (3.0 / 5.0) * std::pow(x, 5)
           + x * x - 7.0 * x;
}

/// @brief テスト用関数: f(x) = x sin(3x)
double f_x_3(double x) { return x * std::sin(3.0 * x); }
/// @brief f_x_3の不定積分: F(x) = 1/9 (sin(3x) - 3x cos(3x))
double int_f_x_3(double x) {
    return (1.0 / 9.0) * (std::sin(3.0 * x) - 3.0 * x * std::cos(3.0 * x));
}

/// @brief テスト用関数: f(x, y) = x^2 + y^3
double f_xy_1(double x, double y) { return x * x + y * y * y; }
/// @brief f_xy_1の不定積分
double int_f_xy_1(std::array<double, 4> range) {
    double xmM3 = std::pow(range[0], 3) - std::pow(range[1], 3);
    double ymM4 = std::pow(range[2], 4) - std::pow(range[3], 4);

    return 1.0 / 3.0 * xmM3 * (range[2] - range[3])
         + 1.0 / 4.0 * (range[0] - range[1]) * ymM4;
}

/// @brief テスト用関数: f(x, y) = x^4 + x^2 y^2 + 4x y^3 + 6y^4
double f_xy_2(double x, double y) {
    return std::pow(x, 4) + x * x * y * y + 4.0 * x * y * y * y + 6.0 * std::pow(y, 4);
}
/// @brief f_xy_2の不定積分
double int_f_xy_2(std::array<double, 4> range) {
    double xmM5 = std::pow(range[0], 5) - std::pow(range[1], 5);
    double ymM5 = std::pow(range[2], 5) - std::pow(range[3], 5);
    double ymM4 = std::pow(range[2], 4) - std::pow(range[3], 4);
    double xmM3 = std::pow(range[0], 3) - std::pow(range[1], 3);
    double ymM3 = std::pow(range[2], 3) - std::pow(range[3], 3);
    double xmM2 = std::pow(range[0], 2) - std::pow(range[1], 2);

    return 1.0 / 5.0 * xmM5 * (range[2] - range[3])
         + 1.0 / 9.0 * xmM3 * ymM3
         + 1.0 / 2.0 * xmM2 * ymM4
         + 6.0 / 5.0 * (range[0] - range[1]) * ymM5;
}

/// @brief テスト用関数: f(x, y) = e^(x+y)
double f_xy_3(double x, double y) { return std::exp(x + y); }
/// @brief f_xy_3の不定積分
double int_f_xy_3(std::array<double, 4> range) {
    return (std::exp(range[1]) - std::exp(range[0]))
         * (std::exp(range[3]) - std::exp(range[2]));
}

}  // namespace



/**
 * GaussLegendreIntegrateのテスト
 */

// 基本的な関数での検証: f_x_2
// n_points 1～5まで
TEST(IntegrationTest, GaussLegendreIntegrateBasic) {
    std::array<double, 2> range = {-1.0, 1.0};
    double exact_integral = int_f_x_1(range[1]) - int_f_x_1(range[0]);

    // 分割数: ある程度大きめに1000分割
    size_t n_intervals = 1000;

    for (size_t n_points = 1; n_points <= i_num::kGaussLegendreIntegrateMaxPoints; ++n_points) {
        double numerical_integral = i_num::GaussLegendreIntegrate(
            f_x_1, range, n_intervals, n_points);
        EXPECT_NEAR(numerical_integral, exact_integral, 1e-4)
            << "n_points: " << n_points;
    }
}



/**
 * Integrateのテスト (1次元)
 */

// Gauss-Legendre, points=5 での検証
TEST(IntegrationTest, Integrate1DGaussLegendreAutoIntervals) {
    std::vector<std::pair<double, double>> test_ranges = {
        {-100.0, -50.0},  // 負方向の大きな範囲
        {-3.0, -1.0},     // 負方向の小さな範囲
        {-1.0, 0.0},      // 負から正への範囲
        {-1.0, 1.0},      // 負から正への範囲
        {0.0, 1.0},       // 正方向の小さな範囲
        {1.0, 3.0},       // 正方向の中程度の範囲
        {50.0, 100.0}     // 正方向の大きな範囲
    };
    auto tol = i_num::Tolerance(1e-6);
    auto options = i_num::IntegrationOptions::GaussLegendre();

    for (const auto& [x_min, x_max] : test_ranges) {
        std::string description = "Integrate with Gauss-Legendre, n_points=5, "
            "x from " + std::to_string(x_min) + " to " + std::to_string(x_max);
        std::array<double, 2> range = {x_min, x_max};

        // f_x_1での検証
        double exact_integral1 = int_f_x_1(x_max) - int_f_x_1(x_min);
        double numerical_integral1 = i_num::Integrate(f_x_1, range, tol, options);
        auto error_msg1 = CheckError(numerical_integral1, exact_integral1, tol);
        EXPECT_TRUE(error_msg1.empty()) << description << ": " << error_msg1;

        // f_x_2での検証
        double exact_integral2 = int_f_x_2(x_max) - int_f_x_2(x_min);
        double numerical_integral2 = i_num::Integrate(f_x_2, range, tol, options);
        auto error_msg2 = CheckError(numerical_integral2, exact_integral2, tol);
        EXPECT_TRUE(error_msg2.empty()) << description << ": " << error_msg2;

        // f_x_3での検証
        double exact_integral3 = int_f_x_3(x_max) - int_f_x_3(x_min);
        double numerical_integral3 = i_num::Integrate(f_x_3, range, tol, options);
        auto error_msg3 = CheckError(numerical_integral3, exact_integral3, tol);
        EXPECT_TRUE(error_msg3.empty()) << description << ": " << error_msg3;
    }
}

/**
 * Integrateのテスト (2次元)
 */

// Gauss-Legendre, points=5 での検証
TEST(IntegrationTest, Integrate2DGaussLegendreAutoIntervals) {
    std::vector<std::array<double, 4>> test_ranges = {
        {-100, -50, -100, -50},    // 負方向の大きな範囲
        {-2.0, -1.0, -2.0, -1.0},  // 負方向の小さな範囲
        {-1.0, 0.0, -1.0, 0.0},    // 負から正への範囲
        {-1.0, 1.0, -1.0, 1.0},    // 負から正への範囲
        {0.0, 1.0, 0.0, 1.0},      // 正方向の小さな範囲
        {1.0, 2.0, 1.0, 2.0},      // 正方向の中程度の範囲
        {50, 100, 50, 100}         // 正方向の大きな範囲
    };
    auto tol = i_num::Tolerance();
    auto options = i_num::IntegrationOptions::GaussLegendre();

    for (const auto& range : test_ranges) {
        std::string description = "Integrate with Gauss-Legendre, n_points=5, (x, y) "
            "from (" + std::to_string(range[0]) + ", " + std::to_string(range[2]) +
            ") to (" + std::to_string(range[1]) + ", " + std::to_string(range[3]) + ")";

        // f_xy_1での検証
        double exact_integral1 = int_f_xy_1(range);
        double numerical_integral1 = i_num::Integrate(f_xy_1, range, tol, options);
        auto error_msg1 = CheckError(numerical_integral1, exact_integral1, tol);
        EXPECT_TRUE(error_msg1.empty()) << description << ": " << error_msg1;

        // f_xy_2での検証
        double exact_integral2 = int_f_xy_2(range);
        double numerical_integral2 = i_num::Integrate(f_xy_2, range, tol, options);
        auto error_msg2 = CheckError(numerical_integral2, exact_integral2, tol);
        EXPECT_TRUE(error_msg2.empty()) << description << ": " << error_msg2;

        // f_xy_3での検証
        double exact_integral3 = int_f_xy_3(range);
        double numerical_integral3 = i_num::Integrate(f_xy_3, range, tol, options);
        auto error_msg3 = CheckError(numerical_integral3, exact_integral3, tol);
        EXPECT_TRUE(error_msg3.empty()) << description << ": " << error_msg3;
    }
}
