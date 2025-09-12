/**
 * @file common/tolerance.h
 * @brief 許容誤差に関する定数と、近似比較のための関数を定義する
 * @author Yayoi Habami
 * @date 2025-07-05
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/common/tolerance.h"



bool igesio::IsApproxZero(const double value, const double tolerance) {
    return std::abs(value) <= tolerance;
}

bool igesio::IsApproxOne(const double value, const double tolerance) {
    return std::abs(value - 1.0) <= tolerance;
}

bool igesio::IsApproxEqual(const double a, const double b, const double tolerance) {
    return std::abs(a - b) <= tolerance;
}

bool igesio::IsApproxLessThan(const double a, const double b, const double tolerance) {
    return (b - a) > tolerance;  // b - a が許容誤差より大きい場合はtrue
}

bool igesio::IsApproxGreaterThan(const double a, const double b, const double tolerance) {
    return (a - b) > tolerance;  // a - b が許容誤差より大きい場合はtrue
}
