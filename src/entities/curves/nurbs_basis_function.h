/**
 * @file entities/curves/nurbs_basis_function.h
 * @brief NURBS基底関数の定義
 * @author Yayoi Habami
 * @date 2025-09-25
 * @copyright 2025 Yayoi Habami
 * @note Rational B-Spline Curve/Surfaceの両方で同じ基底関数を使用するため、
 *      共通のヘッダーファイルとして定義する. 公開APIには含めない.
 */
#ifndef IGESIO_ENTITIES_CURVES_NURBS_BASIS_FUNCTION_H_
#define IGESIO_ENTITIES_CURVES_NURBS_BASIS_FUNCTION_H_

#include <optional>
#include <utility>
#include <vector>

#include "igesio/numerics/tolerance.h"
#include "igesio/numerics/matrix.h"



namespace igesio::entities {

/// @brief 基底関数とその導関数の計算結果を格納する構造体
/// @note 1つの媒介変数に対する基底関数の計算結果を格納する.
///       曲線の場合は1つ、曲面の場合は2つのBasisFunctionResultが必要になる.
struct BasisFunctionResult {
    /// @brief パラメータtが含まれるノットスパン
    /// @note [T(j), T(j+1)] なる j
    int knot_span;
    /// @note 基底関数の値 b_{j-m,m}(t), ..., b_{j,m}(t)
    std::vector<double> values;
    /// @note 基底関数の導関数の値 b_{j-m,m}^(i)(t), ..., b_{j,m}^(i)(t)
    /// @note derivatives[i]は(i+1)次導関数の値
    std::vector<std::vector<double>> derivatives;

    /// @brief n階導関数の値を取得する
    /// @param n 導関数の階数 (0なら基底関数)
    /// @return n階導関数 b_{j-m,m}^(n)(t), ..., b_{j,m}^(n)(t)
    const std::vector<double>& GetDerivatives(const int n) const {
        if (n < 0 || n > static_cast<int>(derivatives.size())) {
            throw std::out_of_range(
                "Requested derivative order " + std::to_string(n) +
                " is out of range.");
        }

        if (n == 0) {
            return values;
        }
        return derivatives[n - 1];
    }
};

/// @brief Bスプライン基底関数とその導関数を計算する
/// @param t パラメータ値
/// @param num_derivatives 計算する導関数の次数
///        (0なら基底関数のみ、1なら1次導関数まで)
/// @return BasisFunctionResult構造体、計算に失敗した場合はstd::nullopt
inline std::optional<BasisFunctionResult>
TryComputeBasisFunctions(const double t, const int num_derivatives,
                         const unsigned int degree,
                         const std::vector<double>& knots,
                         const std::array<double, 2>& parameter_range) {
    // パラメータtが定義域内にあるか確認
    if (numerics::IsApproxLessThan(t, parameter_range[0]) ||
        numerics::IsApproxGreaterThan(t, parameter_range[1])) {
        return std::nullopt;
    }
    // 比較誤差を考慮し、tを定義域内に丸める
    const double clamped_t = std::clamp(t, parameter_range[0], parameter_range[1]);

    const int m = static_cast<int>(degree);
    const int k = static_cast<int>(knots.size()) - m - 2;

    // パラメータtを含むノットスパン [T(j), T(j + 1)] を探す
    int j;
    if (clamped_t >= knots[k + 1]) {
        // パラメータが定義域の終点にある場合の特別処理
        j = k;
    } else {
        // std::upper_boundを使用して効率的にスパンを探索
        auto it = std::upper_bound(knots.begin(), knots.end(), clamped_t);
        j = std::distance(knots.begin(), it) - 1;
    }

    // 基底関数とその導関数を計算 ("The NURBS Book", Algorithm A2.3)
    BasisFunctionResult result;
    result.knot_span = j;
    result.derivatives.resize(num_derivatives + 1);
    for (auto& vec : result.derivatives) vec.resize(m + 1, 0.0);

    MatrixXd ndu(m + 1, m + 1);
    ndu(0, 0) = 1.0;

    std::vector<double> left(m + 1), right(m + 1);
    for (int p = 1; p <= m; ++p) {
        left[p] = clamped_t - knots[j + 1 - p];
        right[p] = knots[j + p] - clamped_t;
        double saved = 0.0;
        for (int r = 0; r < p; ++r) {
            ndu(p, r) = right[r + 1] + left[p - r];
            double temp = ndu(r, p - 1) / ndu(p, r);
            ndu(r, p) = saved + right[r + 1] * temp;
            saved = left[p - r] * temp;
        }
        ndu(p, p) = saved;
    }

    result.values.resize(m + 1);
    for (int i = 0; i <= m; ++i) result.values[i] = ndu(i, m);

    // 導関数の計算
    if (num_derivatives > 0) {
        MatrixXd a = MatrixXd::Zero(m + 1, m + 1);
        for (int r = 0; r <= m; ++r) {
            int s1 = 0, s2 = 1;
            a(0, 0) = 1.0;
            for (int k = 1; k <= num_derivatives; ++k) {
                double d = 0.0;
                int rk = r - k, pk = m - k;
                if (r >= k) {
                    a(s2, 0) = a(s1, 0) / ndu(pk + 1, rk);
                    d = a(s2, 0) * ndu(rk, pk);
                }
                int j1 = (rk >= -1) ? 1 : -rk;
                int j2 = (r - 1 <= pk) ? k - 1 : m - r;
                for (int i = j1; i <= j2; ++i) {
                    a(s2, i) = (a(s1, i) - a(s1, i - 1)) / ndu(pk + 1, rk + i);
                    d += a(s2, i) * ndu(rk + i, pk);
                }
                if (r <= pk) {
                    a(s2, k) = -a(s1, k - 1) / ndu(pk + 1, r);
                    d += a(s2, k) * ndu(r, pk);
                }
                result.derivatives[k-1][r] = d;
                std::swap(s1, s2);
            }
        }
        int r = m;
        for (int k = 1; k <= num_derivatives; ++k) {
            for (int i = 0; i <= m; ++i) {
                result.derivatives[k-1][i] *= r;
            }
            r *= (m - k);
        }
    }

    result.derivatives.erase(result.derivatives.begin() + num_derivatives,
                             result.derivatives.end());
    return result;
}

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_CURVES_NURBS_BASIS_FUNCTION_H_
