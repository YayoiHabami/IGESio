/**
 * @file numerics/optimization_impl.h
 * @brief 最適化・求根関数の実装 (内部実装用)
 *
 * 以下を実装済み (いずれもテンプレート版)
 * - MinimizeScalarT: scipy.optimize.minimize_scalar(method="bounded")相当
 * - FindRootScalarT: scipy.optimize.brentq相当
 * @author Yayoi Habami
 * @date 2026-04-09
 * @copyright 2026 Yayoi Habami
 */
#ifndef SRC_NUMERICS_OPTIMIZATION_IMPL_H_
#define SRC_NUMERICS_OPTIMIZATION_IMPL_H_

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

#include <boost/math/tools/minima.hpp>
#include <boost/math/tools/roots.hpp>

#include "igesio/numerics/optimization.h"

namespace igesio::numerics {

// ---------------------------------------------------------------------------
// minimize_scalar (bounded) のC++実装
// ---------------------------------------------------------------------------

/// @brief scipy.optimize.minimize_scalar(method="bounded") に相当する関数。
///
/// boost::math::tools::brent_find_minimaを用いて [t_lower, t_upper] 内で
/// 目的関数の最小値を探索する。
///
/// @tparam F double(double) と互換なcallable型
/// @param func  最小化する目的関数
/// @param t_lower 探索区間の下限
/// @param t_upper 探索区間の上限
/// @param tol     許容絶対誤差 (xatolに相当)
/// @return MinimizeScalarResult構造体 (最適tおよび目的関数値)
/// @throws std::invalid_argument t_lower >= t_upper の場合
template <typename F>
MinimizeScalarResult MinimizeScalarT(F func, double t_lower, double t_upper,
                                     double tol) {
    if (t_lower >= t_upper) {
        throw std::invalid_argument(
            "t_lower must be strictly less than t_upper");
    }

    // 許容誤差をビット精度に変換する。
    // brent_find_minimaのbits引数は有効ビット数を取るため、
    // tolを区間幅で正規化してlog2変換する。安全のため、ceilで切り上げる。
    const int kMaxBits = std::numeric_limits<double>::digits;  // 53
    int bits = static_cast<int>(std::ceil(-std::log2(tol / (t_upper - t_lower))));
    if (bits <= 0 || bits > kMaxBits) bits = kMaxBits;

    std::uintmax_t max_iter = 500;
    auto [t_opt, f_opt] = boost::math::tools::brent_find_minima(
        func, t_lower, t_upper, bits, max_iter);

    return MinimizeScalarResult{t_opt, f_opt};
}

// ---------------------------------------------------------------------------
// brentq の C++ 実装
// ---------------------------------------------------------------------------

/// @brief scipy.optimize.brentqに相当する関数。
///
/// boost::math::tools::toms748_solveを用いて [t_lower, t_upper] 内で
/// 目的関数のゼロ点を探索する。TOMS 748アルゴリズムはbrentqと同等以上の
/// 収束性を持つ。
///
/// @tparam F double(double) と互換なcallable型
/// @param func    零点を探索する関数
/// @param t_lower 探索区間の下限 (f(t_lower)とf(t_upper)は異符号であること)
/// @param t_upper 探索区間の上限
/// @param x_tol   許容絶対誤差
/// @param maxiter 最大反復回数
/// @return 求根結果tの値 (区間中点)
/// @throws std::invalid_argument 区間端で符号が同じ場合
/// @throws std::runtime_error 最大反復回数内に収束しなかった場合
template <typename F>
double FindRootScalarT(F func, double t_lower, double t_upper,
                       double x_tol, std::uintmax_t maxiter = 200) {
    double f_lower = func(t_lower);
    double f_upper = func(t_upper);

    if (f_lower * f_upper > 0.0) {
        throw std::invalid_argument(
            "f(t_lower) and f(t_upper) must have opposite signs");
    }

    // 絶対誤差x_tolを基準に収束判定するファンクタを構築する。
    auto tol_func = [x_tol](double a, double b) -> bool {
        return std::abs(b - a) <= x_tol;
    };

    std::uintmax_t iters = maxiter;
    auto [t_lo, t_hi] = boost::math::tools::toms748_solve(
        func, t_lower, t_upper,
        f_lower, f_upper, tol_func, iters);

    if (iters >= maxiter) {
        throw std::runtime_error(
            "FindRootScalar: failed to converge within maxiter iterations");
    }

    // 区間の中点をゼロ点の推定値として返す。
    return (t_lo + t_hi) / 2.0;
}

}  // namespace igesio::numerics

#endif  // SRC_NUMERICS_OPTIMIZATION_IMPL_H_
