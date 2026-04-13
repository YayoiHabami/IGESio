/**
 * @file numerics/test_optimization.cpp
 * @brief numerics/optimization.hのテスト
 * @author Yayoi Habami
 * @date 2026-04-10
 * @copyright 2026 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <cmath>
#include <functional>
#include <stdexcept>

#include "igesio/numerics/optimization.h"

namespace {

namespace i_num = igesio::numerics;

/// @brief 円周率
/// @note matrix.hをインクルードするのはコストが高いので以下を利用
const double kPi = std::acos(-1.0);

/// @brief MinimizeScalarのテストケース構造体
struct MinimizeTestCase {
    std::function<double(double)> f;
    double lower;
    double upper;
    double true_min;
    const char* name;  // 失敗時にどの関数か特定しやすくするため
};

}  // namespace



/**
 * MinimizeScalarのテスト
 */

// 放物線 f(x)=(x-2)^2 の最小値: x=2, f=0
TEST(MinimizeScalarTest, Parabola) {
    auto f = [](double x) { return (x - 2.0) * (x - 2.0); };
    const double tol = 1e-6;
    auto result = i_num::MinimizeScalar(f, 0.0, 4.0, tol);
    EXPECT_NEAR(result.t_opt, 2.0, tol);
    EXPECT_NEAR(result.f_opt, 0.0, 1e-10);
}

// コサイン f(x)=cos(x) の [2,5] での最小値: x=π, f=-1
TEST(MinimizeScalarTest, Cosine) {
    auto f = [](double x) { return std::cos(x); };
    const double tol = 1e-6;
    auto result = i_num::MinimizeScalar(f, 2.0, 5.0, tol);
    EXPECT_NEAR(result.t_opt, kPi, tol);
    EXPECT_NEAR(result.f_opt, -1.0, 1e-10);
}

// 区間中央でない最小値: f(x)=(x+1)^2 の最小値は x=-1
TEST(MinimizeScalarTest, OffCenterMinimum) {
    auto f = [](double x) { return (x + 1.0) * (x + 1.0); };
    const double tol = 1e-6;
    auto result = i_num::MinimizeScalar(f, -3.0, 3.0, tol);
    EXPECT_NEAR(result.t_opt, -1.0, tol);
    EXPECT_NEAR(result.f_opt, 0.0, 1e-10);
}

// tolの指定が精度に反映されること: 厳しいtolほど真値に近い、あるいは同等の結果を返す
TEST(MinimizeScalarTest, TolerancePrecision) {
    const double tol_loose = 1e-2, tol_tight = 1e-7;
    std::vector<MinimizeTestCase> test_cases = {
        {[](double x) { return (x - 1.5) * (x - 1.5); }, 0.0, 3.0, 1.5, "Quadratic"},
        {[](double x) { return std::cosh(x - 2.0); }, -1.0, 5.0, 2.0, "Cosh"},
        {[](double x) { return std::exp(x) + std::exp(-x); }, -2.0, 2.0, 0.0, "ExpSum"}
    };

    for (const auto& tc : test_cases) {
        SCOPED_TRACE(tc.name);  // 失敗した際に対象のケースを表示

        auto result_loose = i_num::MinimizeScalar(
                tc.f, tc.lower, tc.upper, tol_loose);
        auto result_tight = i_num::MinimizeScalar(
                tc.f, tc.lower, tc.upper, tol_tight);

        double err_loose = std::abs(result_loose.t_opt - tc.true_min);
        double err_tight = std::abs(result_tight.t_opt - tc.true_min);

        // 厳しいtolの結果が、緩いtolの結果と同等か、より真値に近いこと
        // (既に真値に到達している場合は 0 == 0 となるため LE を使用)
        EXPECT_LE(err_tight, err_loose);

        // 各tolの範囲内に収まること
        EXPECT_LE(err_loose, tol_loose);
        EXPECT_LE(err_tight, tol_tight);
    }
}

// t_lower == t_upper → std::invalid_argument
TEST(MinimizeScalarTest, InvalidBoundsEqual) {
    auto f = [](double x) { return x * x; };
    EXPECT_THROW(
        i_num::MinimizeScalar(f, 1.0, 1.0, 1e-6),
        std::invalid_argument);
}

// t_lower > t_upper → std::invalid_argument
TEST(MinimizeScalarTest, InvalidBoundsReversed) {
    auto f = [](double x) { return x * x; };
    EXPECT_THROW(
        i_num::MinimizeScalar(f, 2.0, 1.0, 1e-6),
        std::invalid_argument);
}

// tol=0 での堅牢性: log2(0)=-∞ となりstatic_cast<int>が未定義動作になりうる。
// kMaxBitsへのクランプで正常動作すること、かつ高精度な結果が得られることを確認する。
TEST(MinimizeScalarTest, TolZeroRobustness) {
    auto f = [](double x) { return (x - 2.0) * (x - 2.0); };
    EXPECT_NO_THROW({
        auto result = i_num::MinimizeScalar(f, 0.0, 4.0, 0.0);
        EXPECT_NEAR(result.t_opt, 2.0, 1e-10);
    });
}

// 単調増加関数: 最小値が左端境界 x=0 にある。
// brent_find_minimaは区間内部の極小値を前提とするため、境界点そのものは返せない。
// 返された点での関数値が区間中点より小さければ十分とする。
TEST(MinimizeScalarTest, MonotonicFunctionBoundaryMin) {
    auto f = [](double x) { return x; };
    auto result = i_num::MinimizeScalar(f, 0.0, 1.0, 1e-6);

    // 返された点は区間内に収まること
    EXPECT_GE(result.t_opt, 0.0);
    EXPECT_LE(result.t_opt, 1.0);
    // 区間中点 (f=0.5) よりも小さい関数値を返すこと
    EXPECT_LT(result.f_opt, 0.5);
}

// 複数の局所最小値: f(x) = sin(x) + 0.05*x^2 は複数の谷を持つ。
// 指定した区間 [0, 5] で何らかの極小値を返すことを確認する。
TEST(MinimizeScalarTest, MultipleLocalMinima) {
    auto f = [](double x) { return std::sin(x) + 0.05 * x * x; };
    const double tol = 1e-6;
    auto result = i_num::MinimizeScalar(f, 0.0, 5.0, tol);

    // 返された点が区間内に収まること
    EXPECT_GE(result.t_opt, 0.0);
    EXPECT_LE(result.t_opt, 5.0);
    // 返された点が真に極小であること (近傍より小さい)
    const double delta = 1e-4;
    EXPECT_LE(result.f_opt, f(result.t_opt - delta));
    EXPECT_LE(result.f_opt, f(result.t_opt + delta));
}



/**
 * FindRootScalarのテスト
 */

// 線形関数 f(x) = x - 1.5 の根: x=1.5
TEST(FindRootScalarTest, LinearFunction) {
    auto f = [](double x) { return x - 1.5; };
    const double x_tol = 1e-9;
    double root = i_num::FindRootScalar(f, 0.0, 3.0, x_tol);
    EXPECT_NEAR(root, 1.5, x_tol);
}

// f(x) = x^2 - 2 の根: x=√2
TEST(FindRootScalarTest, SquareRoot2) {
    auto f = [](double x) { return x * x - 2.0; };
    const double x_tol = 1e-9;
    double root = i_num::FindRootScalar(f, 1.0, 2.0, x_tol);
    EXPECT_NEAR(root, std::sqrt(2.0), x_tol);
}

// f(x) = sin(x) の [2,4] での根: x=π
TEST(FindRootScalarTest, Trigonometric) {
    auto f = [](double x) { return std::sin(x); };
    const double x_tol = 1e-9;
    double root = i_num::FindRootScalar(f, 2.0, 4.0, x_tol);
    EXPECT_NEAR(root, kPi, x_tol);
}

// 結果がx_tol内に収まること (複数のtolで確認)
TEST(FindRootScalarTest, TolerancePrecision) {
    auto f = [](double x) { return x * x - 2.0; };
    const double true_root = std::sqrt(2.0);

    for (double x_tol : {1e-3, 1e-6, 1e-9}) {
        double root = i_num::FindRootScalar(f, 1.0, 2.0, x_tol);
        EXPECT_NEAR(root, true_root, x_tol) << "x_tol: " << x_tol;
    }
}

// f(t_lower) * f(t_upper) > 0 → std::invalid_argument
TEST(FindRootScalarTest, SameSigns) {
    // f(1)=0.5 > 0, f(2)=3.5 > 0 → 同符号
    auto f = [](double x) { return x * x - 0.5; };
    EXPECT_THROW(
        i_num::FindRootScalar(f, 1.0, 2.0, 1e-6),
        std::invalid_argument);
}

// maxiter=1: 収束不可能なため std::runtime_error
// 合わせて iters >= maxiter の判定も検証する (オフバイワン候補)
TEST(FindRootScalarTest, MaxiterExceeded) {
    auto f = [](double x) { return x * x - 2.0; };
    EXPECT_THROW(
        i_num::FindRootScalar(f, 1.0, 2.0, 1e-9, 1),
        std::runtime_error);
}

// f(t_lower)=0 のとき: 0*f_upper=0 → 例外なし、t_lower付近の根を返す
TEST(FindRootScalarTest, BoundaryZeroLower) {
    // f(1)=0, f(3)=2
    auto f = [](double x) { return x - 1.0; };
    EXPECT_NO_THROW({
        double root = i_num::FindRootScalar(f, 1.0, 3.0, 1e-9);
        EXPECT_NEAR(root, 1.0, 1e-9);
    });
}

// f(t_upper)=0 のとき: f_lower*0=0 → 例外なし、t_upper付近の根を返す
TEST(FindRootScalarTest, BoundaryZeroUpper) {
    // f(1)=-2, f(3)=0
    auto f = [](double x) { return x - 3.0; };
    EXPECT_NO_THROW({
        double root = i_num::FindRootScalar(f, 1.0, 3.0, 1e-9);
        EXPECT_NEAR(root, 3.0, 1e-9);
    });
}

// x_tol=0 の動作確認: 浮動小数点の打ち切りにより収束するかmaxiter超過するかの
// いずれかであること。std::invalid_argument は想定外。
TEST(FindRootScalarTest, TolZeroConvergence) {
    auto f = [](double x) { return x - 1.0; };
    try {
        double root = i_num::FindRootScalar(f, 0.0, 2.0, 0.0);
        // 収束した場合は機械イプシロン相当の精度を期待
        EXPECT_NEAR(root, 1.0, 1e-14);
    } catch (const std::runtime_error&) {
        // maxiter超過は許容動作
    }
}
