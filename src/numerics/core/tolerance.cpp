/**
 * @file numerics/core/tolerance.cpp
 * @brief 許容誤差に関する定数と、近似比較のための関数を定義する
 * @author Yayoi Habami
 * @date 2025-07-05
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/numerics/core/tolerance.h"

#include <algorithm>
#include <optional>

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

bool i_num::IsRotation(const igesio::Matrix3d& rot, const double tolerance) {
    const igesio::Matrix3d should_be_identity = rot * rot.transpose();
    return IsApproxIdentity(should_be_identity, tolerance) &&
           IsApproxEqual(rot.determinant(), 1.0, tolerance);
}

igesio::Matrix3d i_num::NearestRotation(const igesio::Matrix3d& rot) {
    // 列ベクトルをGram-Schmidt法で正規直交化する
    const igesio::Vector3d c0 = rot.col(0);
    const double n0 = c0.norm();
    // 退化(ゼロ列)時は補正不能なため入力をそのまま返す
    // (回転として不正な入力はIsRotationで事前に弾く想定)
    if (n0 == 0.0)  return rot;
    const igesio::Vector3d e0 = c0 / n0;

    igesio::Vector3d e1 = rot.col(1) - rot.col(1).dot(e0) * e0;
    const double n1 = e1.norm();
    if (n1 == 0.0)  return rot;
    e1 /= n1;

    // 第3列は外積で定め、右手系(det=+1)を保証する
    const igesio::Vector3d e2 = e0.cross(e1);

    igesio::Matrix3d result;
    result.col(0) = e0;
    result.col(1) = e1;
    result.col(2) = e2;
    return result;
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

std::optional<double> i_num::TryClampToRange(
        const double t, const double lo, const double hi, const double tolerance) {
    if (IsApproxLessThan(t, lo, tolerance) ||
        IsApproxGreaterThan(t, hi, tolerance)) {
        return std::nullopt;
    }
    return std::clamp(t, lo, hi);
}
