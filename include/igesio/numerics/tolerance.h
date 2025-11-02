/**
 * @file numerics/tolerance.h
 * @brief 許容誤差に関する定数と、近似比較のための関数を定義する
 * @author Yayoi Habami
 * @date 2025-07-05
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_NUMERICS_TOLERANCE_H_
#define IGESIO_NUMERICS_TOLERANCE_H_

#include <limits>

#include "igesio/numerics/matrix.h"



namespace igesio::numerics {

/// @brief ジオメトリの計算に使用する許容誤差
constexpr double kGeometryTolerance = 1e-9;
/// @brief 角度の計算に使用する許容誤差
constexpr double kAngleTolerance = 1e-12;
/// @brief 近似比較に使用する許容誤差
constexpr double kParameterTolerance
    = std::numeric_limits<double>::epsilon() * 100;

/// @brief 計算の許容誤差
/// @note 絶対許容誤差と相対許容誤差の両方を指定する
/// @note 数値積分など、反復計算の収束判定に使用することを想定している
struct Tolerance {
    /// @brief 絶対許容誤差
    /// @note 基本的にはこちらが優先される. 例えば更新前と後との値の差 diff
    ///       (絶対値)がabs_tol以下であれば収束とみなす.
    double abs_tol = 1e-4;
    /// @brief 相対許容誤差
    /// @note 更新前や後の値が非常に大きい場合（例えば1e14など）には、
    ///       double型の精度限界によりabs_tol以下の差が出ないことがある.
    ///       そのような場合にこちらの相対許容誤差を使用する.
    /// @note 例えば更新前後での値の差 diff (絶対値)が、更新前の値の
    ///       絶対値 * rel_tol以下であれば収束とみなす.
    double rel_tol = 1e-10;

    /// @brief デフォルトコンストラクタ
    Tolerance() : Tolerance(1e-4, 1e-10) {}

    /// @brief コンストラクタ
    /// @param abs_tol_ 絶対許容誤差
    /// @param rel_tol_ 相対許容誤差 (デフォルトは1e-10)
    explicit Tolerance(const double abs_tol_, const double rel_tol_ = 1e-10)
        : abs_tol(abs_tol_), rel_tol(rel_tol_) {}
};

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

/// @brief a < b かどうかを判定する
/// @param a 判定する値1
/// @param b 判定する値2
/// @param tolerance 許容誤差 (デフォルトはkParameterTolerance)
bool IsApproxLessThan(const double, const double, const double = kParameterTolerance);

/// @brief a <= b かどうかを判定する
/// @param a 判定する値1
/// @param b 判定する値2
/// @param tolerance 許容誤差 (デフォルトはkParameterTolerance)
/// @note a=b=±∞の場合もtrue
bool IsApproxLEQ(const double, const double, const double = kParameterTolerance);

/// @brief a > b かどうかを判定する
/// @param a 判定する値1
/// @param b 判定する値2
/// @param tolerance 許容誤差 (デフォルトはkParameterTolerance)
bool IsApproxGreaterThan(const double, const double, const double = kParameterTolerance);

/// @brief a >= b かどうかを判定する
/// @param a 判定する値1
/// @param b 判定する値2
/// @param tolerance 許容誤差 (デフォルトはkParameterTolerance)
/// @note a=b=±∞の場合もtrue
bool IsApproxGEQ(const double, const double, const double = kParameterTolerance);

}  // namespace igesio

#endif  // IGESIO_NUMERICS_TOLERANCE_H_
