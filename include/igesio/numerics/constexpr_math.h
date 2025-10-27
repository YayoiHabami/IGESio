/**
 * @file numerics/constexpr_math.h
 * @brief constexpr対応の数学関数群
 * @author Yayoi Habami
 * @date 2025-10-20
 * @copyright 2025 Yayoi Habami
 * @note 現在使用しているC++17標準では、cmathの多くの関数がconstexprに対応していないため、
 *       ここにconstexpr対応の数学関数を実装する。
 */
#ifndef IGESIO_NUMERICS_CONSTEXPR_MATH_H_
#define IGESIO_NUMERICS_CONSTEXPR_MATH_H_

namespace igesio::numerics {

/// @brief abs関数のconstexpr版
constexpr double abs_c(const double x) {
    return (x < 0.0) ? -x : x;
}

/// @brief sqrtの許容誤差 (絶対・相対誤差の閾値)
constexpr double kSqrtTolerance = 1e-10;
/// @brief sqrt関数のconstexpr版（ニュートン法による近似）
/// @param x 非負の実数 (負の値を与えると未定義動作)
/// @param guess 初期推定値（デフォルトは-1.0; 自動設定される）
constexpr double sqrt_c(const double x, const double guess = -1.0) {
    if (x < 0.0)   return -1.0;
    if (x == 0.0)  return 0.0;

    const double initial_guess = (guess >= 0.0) ? guess : ((x < 1.0) ? 1.0 : x / 2.0);
    const double next_guess = (initial_guess + x / initial_guess) / 2.0;

    // 変化が十分に小さいか、二乗した値がxに十分近ければ終了
    if (abs_c(next_guess - initial_guess)  < abs_c(initial_guess) * kSqrtTolerance ||
        abs_c(next_guess * next_guess - x) < kSqrtTolerance) {
        return next_guess;
    }

    return sqrt_c(x, next_guess);
}

}  // namespace igesio::numerics

#endif  // IGESIO_NUMERICS_CONSTEXPR_MATH_H_
