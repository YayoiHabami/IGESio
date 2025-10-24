/**
 * @file numerics/integration.cpp
 * @brief 数値積分に関するユーティリティ関数群
 * @author Yayoi Habami
 * @date 2025-10-20
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/numerics/integration.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>

#include "igesio/numerics/constexpr_math.h"

namespace {

namespace i_num = igesio::numerics;

}  // namespace


i_num::IntegrationOptions
i_num::IntegrationOptions::GaussLegendre(
        const size_t n_points, const size_t max_recursion_depth) {
    return IntegrationOptions{
        IntegrateMethod::kGaussLegendre, n_points, max_recursion_depth
    };
}

void i_num::IntegrationOptions::Check() const {
    if (method == i_num::IntegrateMethod::kGaussLegendre) {
        if (n_points == 0 ||
            n_points > i_num::kGaussLegendreIntegrateMaxPoints) {
            throw std::invalid_argument(
                "Integrate: n_points must be in the range [1, " +
                std::to_string(i_num::kGaussLegendreIntegrateMaxPoints) + "], "
                " but got " + std::to_string(n_points) + ".");
        }
        return;
    }
    throw std::invalid_argument(
        "Integrate: Unsupported integration method.");
}


/**
 * Gauss-Legendre求積法による数値積分
 */

namespace {

/// @brief 1点ガウス-ルジャンドル求積法の分点x_0
constexpr std::array<double, 1> kGaussLegendre1Points = {
    0.0
};
/// @brief 1点ガウス-ルジャンドル求積法の重みw_0
constexpr std::array<double, 1> kGaussLegendre1Weights = {
    2.0
};
/// @brief 2点ガウス-ルジャンドル求積法の分点x_i
constexpr std::array<double, 2> kGaussLegendre2Points = {
    -1.0 / i_num::sqrt_c(3.0), 1.0 / i_num::sqrt_c(3.0)
};
/// @brief 2点ガウス-ルジャンドル求積法の重みw_i
constexpr std::array<double, 2> kGaussLegendre2Weights = {
    1.0, 1.0
};
/// @brief 3点ガウス-ルジャンドル求積法の分点x_i
constexpr std::array<double, 3> kGaussLegendre3Points = {
    -i_num::sqrt_c(3.0 / 5.0), 0.0, i_num::sqrt_c(3.0 / 5.0)
};
/// @brief 3点ガウス-ルジャンドル求積法の重みw_i
constexpr std::array<double, 3> kGaussLegendre3Weights = {
    5.0 / 9.0, 8.0 / 9.0, 5.0 / 9.0
};
/// @brief 4点ガウス-ルジャンドル求積法の分点x_i
constexpr std::array<double, 4> kGaussLegendre4Points = {
    -1.0 / i_num::sqrt_c(3.0/7.0 + 2.0/7.0 * i_num::sqrt_c(6.0/5.0)),
    -1.0 / i_num::sqrt_c(3.0/7.0 - 2.0/7.0 * i_num::sqrt_c(6.0/5.0)),
     1.0 / i_num::sqrt_c(3.0/7.0 - 2.0/7.0 * i_num::sqrt_c(6.0/5.0)),
     1.0 / i_num::sqrt_c(3.0/7.0 + 2.0/7.0 * i_num::sqrt_c(6.0/5.0))
};
/// @brief 4点ガウス-ルジャンドル求積法の重みw_i
constexpr std::array<double, 4> kGaussLegendre4Weights = {
    (18.0 - i_num::sqrt_c(30.0)) / 36.0,
    (18.0 + i_num::sqrt_c(30.0)) / 36.0,
    (18.0 + i_num::sqrt_c(30.0)) / 36.0,
    (18.0 - i_num::sqrt_c(30.0)) / 36.0
};
/// @brief 5点ガウス-ルジャンドル求積法の分点x_i
constexpr std::array<double, 5> kGaussLegendre5Points = {
    -1.0 / 3.0 * i_num::sqrt_c(5.0 + 2.0 * i_num::sqrt_c(10.0 / 7.0)),
    -1.0 / 3.0 * i_num::sqrt_c(5.0 - 2.0 * i_num::sqrt_c(10.0 / 7.0)),
    0.0,
    1.0 / 3.0 * i_num::sqrt_c(5.0 - 2.0 * i_num::sqrt_c(10.0 / 7.0)),
    1.0 / 3.0 * i_num::sqrt_c(5.0 + 2.0 * i_num::sqrt_c(10.0 / 7.0))
};
/// @brief 5点ガウス-ルジャンドル求積法の重みw_i
constexpr std::array<double, 5> kGaussLegendre5Weights = {
    (322.0 - 13.0 * i_num::sqrt_c(70.0)) / 900.0,
    (322.0 + 13.0 * i_num::sqrt_c(70.0)) / 900.0,
    128.0 / 225.0,
    (322.0 + 13.0 * i_num::sqrt_c(70.0)) / 900.0,
    (322.0 - 13.0 * i_num::sqrt_c(70.0)) / 900.0
};

/// @brief ガウス=ルジャンドル求積法による数値積分
/// @param f 被積分関数 f(x) (x ∈ [x_min, x_max])
/// @param range 積分区間の範囲 [x_min, x_max]
/// @param n_points ガウス点の数（デフォルトは5点）
/// @return 積分値 ∫[x_min, x_max] f(x) dx
/// @throw std::invalid_argument n_pointsがサポート範囲外の場合
double GaussLegendreIntegrateImpl(
        const std::function<double(double)>& f,
        const std::array<double, 2>& range,
        const size_t n_points = 5) {
    if (n_points > i_num::kGaussLegendreIntegrateMaxPoints) {
        throw std::invalid_argument(
            "GaussLegendreIntegrate currently supports up to " +
            std::to_string(i_num::kGaussLegendreIntegrateMaxPoints) + " points.");
    }

    // 積分区間 [x_min, x_max] を t ∈ [-1, 1] に変換
    // x = at + b,   a = (x_max - x_min) / 2, b = (x_max + x_min) / 2
    double half_width = (range[1] - range[0]) / 2.0;
    double center = (range[1] + range[0]) / 2.0;

    // 積分値の計算
    //   ∫[x_min, x_max] f(x) dx
    // = ∫[-1, 1] f(at + b) * a dt
    // ≈ a * Σ[i=0 to n_points-1] w_i * f(a * x_i + b)
    double integral = 0.0;
    for (size_t i = 0; i < n_points; ++i) {
        switch (n_points) {
            case 1:
                // 1点ガウス-ルジャンドル求積法
                integral += kGaussLegendre1Weights[i] *
                            f(half_width * kGaussLegendre1Points[i] + center);
                break;
            case 2:
                // 2点ガウス-ルジャンドル求積法
                integral += kGaussLegendre2Weights[i] *
                            f(half_width * kGaussLegendre2Points[i] + center);
                break;
            case 3:
                // 3点ガウス-ルジャンドル求積法
                integral += kGaussLegendre3Weights[i] *
                            f(half_width * kGaussLegendre3Points[i] + center);
                break;
            case 4:
                // 4点ガウス-ルジャンドル求積法
                integral += kGaussLegendre4Weights[i] *
                            f(half_width * kGaussLegendre4Points[i] + center);
                break;
            case 5:
                // 5点ガウス-ルジャンドル求積法
                integral += kGaussLegendre5Weights[i] *
                            f(half_width * kGaussLegendre5Points[i] + center);
                break;
            default:
                integral += 0.0;  // 到達しないはず
        }
    }
    return half_width * integral;
}

}  // namespace

double i_num::GaussLegendreIntegrate(
        const std::function<double(double)>& f,
        const std::array<double, 2>& range,
        const size_t n_intervals,
        const size_t n_points) {
    if (n_intervals == 0) return 0.0;

    double total_integral = 0.0;

    // 各分割区間でガウス-ルジャンドル求積法を適用
    // 区間幅: dx = (x_max - x_min) / n_intervals
    // 区間　: [x_min + i*dx, x_min + (i+1)*dx]
    // (i = 0, 1, ..., n_intervals-1)
    // 総和　: Σ[i=0 to n_intervals-1] ∫[x_min + i*dx, x_min + (i+1)*dx] f(x) dx
    double dx = (range[1] - range[0]) / static_cast<double>(n_intervals);
    for (size_t i = 0; i < n_intervals; ++i) {
        double t_min = range[0] + i * dx;
        double t_max = range[0] + (i + 1) * dx;
        total_integral += GaussLegendreIntegrateImpl(f, {t_min, t_max}, n_points);
    }

    return total_integral;
}



/**
 * 統合
 */

namespace {

/// @brief 数値積分 (1次元)
/// @param f 被積分関数 f(x) (x ∈ [x_min, x_max])
/// @param range 現在の階層での積分区間の範囲 [x_min, x_max]
/// @param options 積分オプション
/// @return 積分値 ∫[x_min, x_max] f(x) dx
/// @throw std::invalid_argument サポートされていない積分手法が指定された場合等
double IntegrateBy(const std::function<double(double)>& f,
                   const std::array<double, 2>& range,
                   const i_num::IntegrationOptions& options) {
    switch (options.method) {
        case i_num::IntegrateMethod::kGaussLegendre:
            return GaussLegendreIntegrateImpl(f, range, options.n_points);
        default:
            throw std::invalid_argument(
                "IntegrationByOptions: Unsupported integration method.");
    }
}

/// @brief 数値積分 (2次元)
/// @param f 被積分関数 f(x, y) (x ∈ [x_min, x_max], y ∈ [y_min, y_max])
/// @param range 積分区間の範囲 [x_min, x_max, y_min, y_max]
/// @param options 積分オプション
/// @return 積分値 ∫[x_min, x_max] ∫[y_min, y_max] f(x, y) dy dx
/// @throw std::invalid_argument サポートされていない積分手法が指定された場合等
double IntegrateBy(const std::function<double(double, double)>& f,
                   const std::array<double, 4>& range,
                   const i_num::IntegrationOptions& options) {
    // g(y) = ∫[x_min, x_max] f(x, y) dx
    auto g = [&](double y) {
        auto f_at_y = [&](double x) { return f(x, y); };
        return IntegrateBy(f_at_y, {range[0], range[1]}, options);
    };

    // ∫[y_min, y_max] g(y) dy
    return IntegrateBy(g, {range[2], range[3]}, options);
}

/// @brief 再帰的に数値積分を行う補助関数 (1次元)
/// @param f 被積分関数 f(x) (x ∈ [x_min, x_max])
/// @param range 現在の階層での積分区間の範囲 [x_min, x_max]
/// @param integral_whole 現在の区間 (range) 全体での積分値
/// @param tolerance 全区間での許容誤差
/// @param depth 現在の再帰深度
/// @param options 積分オプション
/// @return 積分値 ∫[x_min, x_max] f(x) dx
/// @throw std::invalid_argument サポートされていない積分手法が指定された場合等
/// @note toleranceは全区間での許容誤差であり、各再帰深度 (各深度での区間幅) に応じて分割する.
///       再帰n階層目では区間幅は全体の1/(2^n)となるため、許容誤差は全体のtolerance/(2^n)とする.
double RecursiveIntegrateImpl(
        const std::function<double(double)>& f,
        const std::array<double, 2>& range,
        const double integral_whole,
        const i_num::Tolerance& tolerance, const size_t depth,
        const i_num::IntegrationOptions& options) {
    // 区間を半分に分割し、[x_min, x_mid], [x_mid, x_max] での積分値を計算
    double x_mid = (range[0] + range[1]) / 2.0;
    if (x_mid == range[0] || x_mid == range[1]) {
        // これ以上分割できない場合、現在の積分値を返す (浮動小数精度の限界)
        return integral_whole;
    }
    std::array<double, 2> sub_integrals = {
        IntegrateBy(f, {range[0], x_mid}, options),
        IntegrateBy(f, {x_mid, range[1]}, options)
    };
    double integral = sub_integrals[0] + sub_integrals[1];

    // 誤差の評価; 許容誤差は tolerance / (2 ^ depth)、ただし相対誤差も考慮
    double error = std::fabs(integral - integral_whole);
    double allowed_error = (tolerance.abs_tol / static_cast<double>(1 << depth))
                         + (tolerance.rel_tol * std::fabs(integral_whole));

    // 誤差が許容範囲内、または最大再帰深度に達した場合、積分値を返す
    if (error < allowed_error || depth >= options.max_recursion_depth) {
        return integral;
    }

    // 再帰的に数値積分を行う
    // f[x_min, x_mid] と f[x_mid, x_max] に分割
    return RecursiveIntegrateImpl(f, {range[0], x_mid}, sub_integrals[0],
                                  tolerance, depth + 1, options)
         + RecursiveIntegrateImpl(f, {x_mid, range[1]}, sub_integrals[1],
                                  tolerance, depth + 1, options);
}

/// @brief 再帰的に数値積分を行う補助関数 (2次元)
/// @param f 被積分関数 f(x, y) (x ∈ [x_min, x_max], y ∈ [y_min, y_max])
/// @param range 現在の階層での積分区間の範囲 [x_min, x_max, y_min, y_max]
/// @param integral_whole 現在の矩形領域全体での積分値
/// @param tolerance 全区間での許容誤差
/// @param depth 現在の再帰深度
/// @param options 積分オプション
/// @return 積分値 ∫[x_min, x_max] ∫[y_min, y_max] f(x, y) dy dx
/// @throw std::invalid_argument サポートされていない積分手法が指定された場合等
double RecursiveIntegrateImpl(
        const std::function<double(double, double)>& f,
        const std::array<double, 4>& range,
        const double integral_whole,
        const i_num::Tolerance& tolerance, const size_t depth,
        const i_num::IntegrationOptions& options) {
    // 領域を4つの小さな矩形に分割する
    double x_mid = (range[0] + range[1]) / 2.0;
    double y_mid = (range[2] + range[3]) / 2.0;
    if (x_mid == range[0] || x_mid == range[1] ||
        y_mid == range[2] || y_mid == range[3]) {
        // これ以上分割できない場合、現在の積分値を返す (浮動小数精度の限界)
        return integral_whole;
    }
    std::array<std::array<double, 4>, 4> sub_ranges = {{
        {range[0], x_mid, range[2], y_mid},  // 左下
        {x_mid, range[1], range[2], y_mid},  // 右下
        {range[0], x_mid, y_mid, range[3]},  // 左上
        {x_mid, range[1], y_mid, range[3]}   // 右上
    }};

    // 各小矩形での積分値を計算
    std::array<double, 4> sub_integrals = {
        IntegrateBy(f, sub_ranges[0], options),
        IntegrateBy(f, sub_ranges[1], options),
        IntegrateBy(f, sub_ranges[2], options),
        IntegrateBy(f, sub_ranges[3], options)
    };
    double integral = sub_integrals[0] + sub_integrals[1]
                    + sub_integrals[2] + sub_integrals[3];

    // 誤差の評価; 許容誤差は tolerance / (4 ^ depth)、ただし相対誤差も考慮
    double error = std::fabs(integral - integral_whole);
    double allowed_error = (tolerance.abs_tol / static_cast<double>(1 << (2 * depth)))
                         + (tolerance.rel_tol * std::fabs(integral_whole));

    // 誤差が許容範囲内、または最大再帰深度に達した場合、積分値を返す
    if (error < allowed_error || depth >= options.max_recursion_depth) {
        return integral;
    }

    // 再帰的に数値積分を行う
    return RecursiveIntegrateImpl(f, sub_ranges[0], sub_integrals[0],
                                  tolerance, depth + 1, options)
         + RecursiveIntegrateImpl(f, sub_ranges[1], sub_integrals[1],
                                  tolerance, depth + 1, options)
         + RecursiveIntegrateImpl(f, sub_ranges[2], sub_integrals[2],
                                  tolerance, depth + 1, options)
         + RecursiveIntegrateImpl(f, sub_ranges[3], sub_integrals[3],
                                  tolerance, depth + 1, options);
}

}  // namespace

double i_num::Integrate(
        const std::function<double(double)>& f,
        const std::array<double, 2>& range, const Tolerance& tolerance,
        const IntegrationOptions& options) {
    // 積分手法の確認
    options.Check();

    if (range[0] == range[1]) return 0.0;

    // 積分区間が逆の場合
    if (range[0] > range[1]) {
        return -Integrate(f, {range[1], range[0]}, tolerance, options);
    }

    // 再帰的に数値積分を行う
    auto initial_integral = IntegrateBy(f, range, options);
    return RecursiveIntegrateImpl(f, range, initial_integral, tolerance, 0, options);
}

double i_num::Integrate(
        const std::function<double(double, double)>& f,
        const std::array<double, 4>& range, const Tolerance& tolerance,
        const IntegrationOptions& options) {
    // 積分手法の確認
    options.Check();

    if (range[0] == range[1] || range[2] == range[3]) return 0.0;

    // 積分区間が逆の場合
    if (range[0] > range[1] && range[2] > range[3]) {
        return Integrate(f, {range[1], range[0], range[3], range[2]},
                         tolerance, options);
    } else if (range[0] > range[1]) {
        return -Integrate(f, {range[1], range[0], range[2], range[3]},
                          tolerance, options);
    } else if (range[2] > range[3]) {
        return -Integrate(f, {range[0], range[1], range[3], range[2]},
                          tolerance, options);
    }

    // 再帰的に数値積分を行う
    auto initial_integral = IntegrateBy(f, range, options);
    return RecursiveIntegrateImpl(f, range, initial_integral, tolerance, 0, options);
}
