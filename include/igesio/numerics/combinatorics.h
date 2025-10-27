/**
 * @file numerics/combinatorics.h
 * @brief 組み合わせ数学に関するユーティリティ関数群
 * @author Yayoi Habami
 * @date 2025-10-23
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_NUMERICS_COMBINATORICS_H_
#define IGESIO_NUMERICS_COMBINATORICS_H_

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "igesio/numerics/tolerance.h"



namespace igesio::numerics {

/// @brief 二項係数 (nCr: n choose r) を計算する
/// @tparam T 数値型
/// @param n 非負整数 n
/// @param r 非負整数 r (r <= n)
/// @return 二項係数 nCr
/// @note nやrに小数部分がある場合、小数部分は無視される. 例えば、
///       n = 5.2, r = 2.8の場合、5C2が計算される
/// @throw std::invalid_argument オーバフローが生じる場合
template<typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
T BinomialCoefficient(const T n, const T r) {
    if (r > n) return 0;
    if (n < 0 || r < 0) return 0;

    // 小数点以下がある場合は切り捨て
    T n_eff = n, r_eff = r;
    if constexpr (std::is_floating_point_v<T>) {
        n_eff = std::trunc(n);
        r_eff = std::trunc(r);
    }
    if (numerics::IsApproxZero(r_eff) || numerics::IsApproxEqual(r_eff, n_eff)) return 1;

    // 計算の効率化のため、rをn-rと比較して小さい方を使用
    if (r_eff > n_eff - r_eff) {
        r_eff = n_eff - r_eff;
    }

    // nCr = Π(i=0 to r-1) (n - i) / (i + 1)
    if constexpr (std::is_integral_v<T>) {
        // 整数型の場合はパスカルの三角形を用いて計算
        // 先に1~(r_eff-1)行目まで計算し、最後にr_eff行目を計算する
        std::vector<uint64_t> p;
        p.push_back(1);  // 0行目

        // 1行目から (r_eff - 1) 行目までを計算
        for (T i = 1; i < n_eff; ++i) {
            std::vector<uint64_t> new_row;
            new_row.push_back(1);  // 各行の左端の要素は1

            for (size_t j = 0; j < p.size() - 1; ++j) {
                if (p[j] > std::numeric_limits<uint64_t>::max() - p[j + 1]) {
                    // uint64_tの範囲を超える場合は例外を投げる
                    throw std::invalid_argument(
                        "BinomialCoefficient: Overflow occurred during calculation, "
                        "exceeding uint64_t limits.");
                }
                new_row.push_back(p[j] + p[j + 1]);
            }
            new_row.push_back(1);  // 各行の右端の要素は1

            p = new_row;
        }

        size_t k = static_cast<size_t>(r_eff);
        // p[k-1], p[k], p[k-1]+p[k] がTの範囲内であることを確認
        if (p[k - 1] > static_cast<uint64_t>(std::numeric_limits<T>::max()) ||
            p[k] > static_cast<uint64_t>(std::numeric_limits<T>::max()) ||
            p[k - 1]  > static_cast<uint64_t>(std::numeric_limits<T>::max()) - p[k]) {
            throw std::invalid_argument(
                "BinomialCoefficient: Overflow occurred during calculation, "
                "exceeding the limits of the return type T.");
        }
        return p[k - 1] + p[k];
    } else {
        long double result = 1;
        for (long double i = 0; i < r_eff; ++i) {
            if (result > std::numeric_limits<long double>::max() / (n_eff - i) * (i + 1)) {
                throw std::invalid_argument(
                    "BinomialCoefficient: Overflow occurred during calculation, "
                    "exceeding long double limits.");
            }
            result *= (n_eff - i) / (i + 1);
        }

        if (result > static_cast<long double>(std::numeric_limits<T>::max())) {
            throw std::invalid_argument(
                "BinomialCoefficient: Overflow occurred during calculation, "
                "exceeding the limits of the return type T.");
        }
        return std::round(result);
    }
}

}  // namespace igesio::numerics

#endif  // IGESIO_NUMERICS_COMBINATORICS_H_
