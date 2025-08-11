/**
 * @file common/tolerance.h
 * @brief 許容誤差に関する定数と、近似比較のための関数を定義する
 * @author Yayoi Habami
 * @date 2025-07-05
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_COMMON_TOLERANCE_H_
#define IGESIO_COMMON_TOLERANCE_H_

#include <limits>

#include "igesio/common/matrix.h"



namespace igesio {

/// @brief ジオメトリの計算に使用する許容誤差
constexpr double kGeometryTolerance = 1e-9;
/// @brief 角度の計算に使用する許容誤差
constexpr double kAngleTolerance = 1e-12;
/// @brief 近似比較に使用する許容誤差
constexpr double kParameterTolerance
    = std::numeric_limits<double>::epsilon() * 100;

/// @brief 値がほぼゼロかどうかを判定する
/// @param value 判定する値
/// @param tolerance 許容誤差 (デフォルトはkParameterTolerance)
/// @return 値が許容誤差以下であればtrue
bool IsApproxZero(const double, const double = kParameterTolerance);

/// @brief 値がほぼ1であるかどうかを判定する
/// @param value 判定する値
/// @param tolerance 許容誤差 (デフォルトはkParameterTolerance)
bool IsApproxOne(const double, const double = kParameterTolerance);

/// @brief 2つの値がほぼ等しいかどうかを判定する
/// @param a 判定する値1
/// @param b 判定する値2
/// @param tolerance 許容誤差 (デフォルトはkParameterTolerance)
bool IsApproxEqual(const double, const double, const double = kParameterTolerance);

/// @brief 2つの行列がほぼ等しいかどうかを判定する
/// @param a 判定する行列1
/// @param b 判定する行列2
/// @param tolerance 許容誤差 (デフォルトはkParameterTolerance)
/// @note 各要素の差がそれぞれ許容誤差以下であるかを判定する
template<typename T, int N, int M>
bool IsApproxEqual(const Matrix<T, N, M>& a, const Matrix<T, N, M>& b,
                   const double tolerance = kParameterTolerance) {
    if (a.rows() != b.rows() || a.cols() != b.cols()) {
        return false;  // 次元が異なる場合はfalse
    }
    for (size_t i = 0; i < a.rows(); ++i) {
        for (size_t j = 0; j < a.cols(); ++j) {
            if (!IsApproxEqual(a(i, j), b(i, j), tolerance)) {
                return false;  // 1つでも許容誤差を超える差があればfalse
            }
        }
    }
    return true;  // 全ての要素が許容誤差以下であればtrue
}

}  // namespace igesio

#endif  // IGESIO_COMMON_TOLERANCE_H_
