/**
 * @file numerics/optimization.h
 * @brief 最適化・求根関数 (公開API)
 *
 * Boostを公開せず、std::functionを引数に取るラッパー関数を提供する。
 * テンプレート版は内部実装用の `optimization_impl.h` を参照すること。
 * - MinimizeScalar: scipy.optimize.minimize_scalar(method="bounded")相当
 * - FindRootScalar: scipy.optimize.brentq相当
 * @author Yayoi Habami
 * @date 2026-04-10
 * @copyright 2026 Yayoi Habami
 */
#ifndef IGESIO_NUMERICS_OPTIMIZATION_H_
#define IGESIO_NUMERICS_OPTIMIZATION_H_

#include <cstdint>
#include <functional>

namespace igesio::numerics {

/// @brief 有界区間内で1次元目的関数を最小化した結果を保持する構造体。
struct MinimizeScalarResult {
    /// @brief 最小値を与えるtの値
    double t_opt;
    /// @brief 最小値f(t_opt)
    double f_opt;
};

/// @brief scipy.optimize.minimize_scalar(method="bounded") に相当する関数。
///
/// [t_lower, t_upper] 内で目的関数の最小値を探索する。
///
/// @param func  最小化する目的関数
/// @param t_lower 探索区間の下限
/// @param t_upper 探索区間の上限
/// @param tol     許容絶対誤差 (xatolに相当)
/// @return MinimizeScalarResult構造体 (最適tおよび目的関数値)
/// @throws std::invalid_argument t_lower >= t_upper の場合
MinimizeScalarResult MinimizeScalar(
    const std::function<double(double)>& func,
    double t_lower, double t_upper, double tol);

/// @brief scipy.optimize.brentqに相当する関数。
///
/// [t_lower, t_upper] 内で目的関数のゼロ点を探索する。
///
/// @param func    零点を探索する関数
/// @param t_lower 探索区間の下限 (f(t_lower)とf(t_upper)は異符号であること)
/// @param t_upper 探索区間の上限
/// @param x_tol   許容絶対誤差
/// @param maxiter 最大反復回数
/// @return 求根結果tの値 (区間中点)
/// @throws std::invalid_argument 区間端で符号が同じ場合
/// @throws std::runtime_error 最大反復回数内に収束しなかった場合
double FindRootScalar(
    const std::function<double(double)>& func,
    double t_lower, double t_upper,
    double x_tol, std::uintmax_t maxiter = 200);

}  // namespace igesio::numerics

#endif  // IGESIO_NUMERICS_OPTIMIZATION_H_
