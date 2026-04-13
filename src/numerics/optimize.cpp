/**
 * @file numerics/optimization.cpp
 * @brief 最適化・求根関数の実装
 * @author Yayoi Habami
 * @date 2026-04-10
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/numerics/optimization.h"

#include "./optimization_impl.h"

namespace igesio::numerics {

MinimizeScalarResult MinimizeScalar(
        const std::function<double(double)>& func,
        double t_lower, double t_upper, double tol) {
    return MinimizeScalarT(func, t_lower, t_upper, tol);
}

double FindRootScalar(
        const std::function<double(double)>& func,
        double t_lower, double t_upper,
        double x_tol, std::uintmax_t maxiter) {
    return FindRootScalarT(func, t_lower, t_upper, x_tol, maxiter);
}

}  // namespace igesio::numerics
