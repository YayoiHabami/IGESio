/**
 * @file numerics/tolerance.h
 * @brief 許容誤差に関する定数と、近似比較のための関数を定義する
 * @author Yayoi Habami
 * @date 2025-07-05
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/numerics/tolerance.h"

namespace {

namespace i_num = igesio::numerics;
constexpr double kInf = std::numeric_limits<double>::infinity();

}  // namespace



bool i_num::IsApproxZero(const double value, const double tolerance) {
    return std::abs(value) <= tolerance;
}

bool i_num::IsApproxOne(const double value, const double tolerance) {
    return std::abs(value - 1.0) <= tolerance;
}

bool i_num::IsApproxEqual(const double a, const double b, const double tolerance) {
    return std::abs(a - b) <= tolerance;
}

bool i_num::IsApproxLessThan(const double a, const double b, const double tolerance) {
    return (b - a) > tolerance;  // b - a が許容誤差より大きい場合はtrue
}

bool i_num::IsApproxLEQ(const double a, const double b, const double tolerance) {
    if (a == kInf && b == kInf)  return true;
    if (a == -kInf && b == -kInf)  return true;

    return (a < b) || IsApproxEqual(a, b, tolerance);
}

bool i_num::IsApproxGreaterThan(const double a, const double b, const double tolerance) {
    return (a - b) > tolerance;  // a - b が許容誤差より大きい場合はtrue
}

bool i_num::IsApproxGEQ(const double a, const double b, const double tolerance) {
    if (a == kInf && b == kInf)  return true;
    if (a == -kInf && b == -kInf)  return true;

    return (a > b) || IsApproxEqual(a, b, tolerance);
}
