/**
 * @file numerics/core/tolerance.h
 * @brief 許容誤差に関する定数と、近似比較のための関数を定義する
 * @author Yayoi Habami
 * @date 2025-07-05
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_NUMERICS_CORE_TOLERANCE_H_
#define IGESIO_NUMERICS_CORE_TOLERANCE_H_

#include <limits>
#include <optional>

#include "igesio/numerics/core/matrix.h"



namespace igesio::numerics {

/// @brief ジオメトリの計算に使用する許容誤差
constexpr double kGeometryTolerance = 1e-9;
/// @brief 角度の計算に使用する許容誤差
constexpr double kAngleTolerance = 1e-12;
/// @brief 近似比較に使用する許容誤差
constexpr double kParameterTolerance
    = std::numeric_limits<double>::epsilon() * 100;
/// @brief 単精度(float)を経由した幾何データの近似比較に使用する許容誤差
/// @note GPUへ渡す変換行列(Matrix4f)などは単精度に丸められるため、回転行列の
///       R·Rᵀ−I が約1e-7の残差を持つ. これはdouble機械イプシロン尺度の
///       kParameterTolerance(約2.2e-14)では検証できないため、float由来の値を
///       検証・比較する箇所ではこちらを使用すること.
constexpr double kFloatGeometryTolerance = 1e-5;

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

/// @brief ベクトルがほぼunitベクトルであるかどうかを判定する
/// @param vec 判定するベクトル
/// @param index 非ゼロのインデックス
/// @param tolerance 許容誤差 (デフォルトはkParameterTolerance)
/// @note vec[index]がほぼ1で、他の要素がほぼ0であればtrue
template<typename T, int N>
bool IsApproxUnitVector(const Vector<T, N>& vec, size_t index,
                        const double tolerance = kParameterTolerance) {
    for (size_t i = 0; i < static_cast<size_t>(N); ++i) {
        if (i == index) {
            if (!IsApproxOne(vec[i], tolerance)) {
                return false;  // 非ゼロの要素が1でない場合はfalse
            }
        } else {
            if (!IsApproxZero(vec[i], tolerance)) {
                return false;  // 他の要素が0でない場合はfalse
            }
        }
    }
    return true;  // 全ての要素が条件を満たす場合はtrue
}

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

/// @brief 行列がほぼ単位行列であるかどうかを判定する
/// @param mat 判定する行列
/// @param tolerance 許容誤差 (デフォルトはkParameterTolerance)
template<typename T, int N, int M>
bool IsApproxIdentity(const Matrix<T, N, M>& mat,
                      const double tolerance = kParameterTolerance) {
    if (mat.rows() != mat.cols()) {
        return false;  // 単位行列は正方行列である必要がある
    }
    for (size_t i = 0; i < mat.rows(); ++i) {
        for (size_t j = 0; j < mat.cols(); ++j) {
            double expected = (i == j) ? 1.0 : 0.0;  // 対角要素は1、非対角要素は0
            if (!IsApproxEqual(mat(i, j), expected, tolerance)) {
                return false;  // 1つでも許容誤差を超える差があればfalse
            }
        }
    }
    return true;  // 全ての要素が許容誤差以下であればtrue
}

/// @brief 行列の要素が定数値に近いかどうかを判定する
/// @param mat 判定する行列
/// @param value 近いとみなす定数値
/// @param tolerance 許容誤差 (デフォルトはkParameterTolerance)
template<typename T, int N, int M>
bool IsApproxConstant(const Matrix<T, N, M>& mat, const double value,
                        const double tolerance = kParameterTolerance) {
    for (size_t i = 0; i < mat.rows(); ++i) {
        for (size_t j = 0; j < mat.cols(); ++j) {
            if (!IsApproxEqual(mat(i, j), value, tolerance)) {
                return false;  // 1つでも許容誤差を超える差があればfalse
            }
        }
    }
    return true;  // 全ての要素が許容誤差以下であればtrue
}

/// @brief 3x3行列がほぼ回転行列(直交かつ行列式が+1)であるかを判定する
/// @param rot 判定する行列
/// @param tolerance 許容誤差 (デフォルトはkFloatGeometryTolerance)
/// @return R·Rᵀがほぼ単位行列で、かつdetがほぼ+1であればtrue
/// @note 単精度を経由した回転行列を想定し、既定でfloat尺度の許容誤差を用いる.
bool IsRotation(const Matrix3d&, double tolerance = kFloatGeometryTolerance);

/// @brief 与えられた3x3行列に最も近い回転行列を返す
/// @param rot 入力行列 (ほぼ回転行列であることを想定)
/// @return 正規直交かつ行列式が+1の回転行列
/// @note 列ベクトルをGram-Schmidt法で正規直交化し、第3列を第1×第2列とすることで
///       右手系(det=+1)を保証する. ほぼ回転行列である入力の微小な非直交性を
///       除去する用途. 入力が回転から大きく外れる場合の結果は保証しない.
Matrix3d NearestRotation(const Matrix3d&);

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

/// @brief 値tが許容誤差を考慮して範囲[lo, hi]内にあるかを判定し、
///        範囲内であれば[lo, hi]へクランプした値を返す
/// @param t 判定する値
/// @param lo 範囲の下限
/// @param hi 範囲の上限
/// @param tolerance 許容誤差 (デフォルトはkParameterTolerance)
/// @return 範囲内ならstd::clamp(t, lo, hi)、範囲外ならstd::nullopt
/// @note 厳密比較では境界値が浮動小数点丸めで僅かに域外へ出た際に
///       評価不能となるため、許容誤差つき比較で判定し域内へ丸める
std::optional<double> TryClampToRange(
    double t, double lo, double hi, double tolerance = kParameterTolerance);

}  // namespace igesio

#endif  // IGESIO_NUMERICS_CORE_TOLERANCE_H_
