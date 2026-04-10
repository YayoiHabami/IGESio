/**
 * @file entities/interfaces/i_curve.cpp
 * @brief 曲線クラスのインターフェース定義
 * @author Yayoi Habami
 * @date 2025-08-02
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/interfaces/i_curve.h"

#include <limits>

#include "igesio/numerics/integration.h"
#include "igesio/numerics/tolerance.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;

using igesio::Vector2d;
using igesio::Vector3d;
using i_ent::ICurve;
using i_ent::ICurve2D;
using i_ent::ICurve3D;
using i_ent::CurveDerivatives;

/// @brief 左側接線/右側接線を計算する際の微小パラメータ値h
constexpr double kTangentH = 1e-7;

}  // namespace



/**
 * CurveDerivatives
 */

CurveDerivatives::CurveDerivatives(const unsigned int n) {
    derivatives.resize(n + 1, Vector3d::Zero());
}

const Vector3d& CurveDerivatives::operator[](const unsigned int n) const {
    if (n < derivatives.size()) {
        return derivatives[n];
    } else {
        throw std::out_of_range(
            "Requested derivative order " + std::to_string(n) +
            " is out of range. Contains up to order " +
            std::to_string(derivatives.size() - 1) + ".");
    }
}

Vector3d& CurveDerivatives::operator[](const unsigned int n) {
    if (n < derivatives.size()) {
        return derivatives[n];
    } else {
        throw std::out_of_range(
            "Requested derivative order " + std::to_string(n) +
            " is out of range. Contains up to order " +
            std::to_string(derivatives.size() - 1) + ".");
    }
}

void CurveDerivatives::Resize(const unsigned int n) {
    derivatives.resize(n, Vector3d::Zero());
}



/**
 * 基本的な情報の取得 (ICurve)
 */

bool ICurve::HasFiniteStart() const {
    return std::isfinite(GetParameterRange()[0]);
}

bool ICurve::HasFiniteEnd() const {
    return std::isfinite(GetParameterRange()[1]);
}

bool ICurve::IsFinite() const {
    return HasFiniteStart() && HasFiniteEnd();
}



/**
 * 曲線の幾何学的情報 (ベクトル) を取得する (ICurve)
 */

double ICurve::GetCurvature(const double t) const {
    auto deriv = TryGetDerivatives(t, 2);
    if (!deriv.has_value()) return -1;

    // κ = |C' × C''| / |C'|^3
    return ((*deriv)[1].cross((*deriv)[2]).norm())
         / std::pow((*deriv)[1].norm(), 3);
}



/**
 * 直線部・角点サポート (ICurve)
 */

std::vector<std::array<double, 2>> ICurve::GetLinearSegments() const {
    return {};
}

bool ICurve::IsInLinearSegment(const double t, const double eps) const {
    for (const auto& seg : GetLinearSegments()) {
        if (t >= seg[0] - eps && t <= seg[1] + eps) return true;
    }
    return false;
}

std::vector<double> ICurve::GetCornerParams() const {
    return {};
}

bool ICurve::IsCorner(const double t, const double eps) const {
    for (const auto tc : GetCornerParams()) {
        if (std::abs(t - tc) < eps) return true;
    }
    return false;
}

std::optional<Vector3d> ICurve::LeftTangentAt(const double t) const {
    // (t-h)における導関数の方向で近似
    const double t_eval = std::max(t - kTangentH, GetParameterRange()[0]);
    auto deriv = TryGetDerivatives(t_eval, 1);
    if (!deriv.has_value()) return std::nullopt;

    // 導関数がゼロベクトルに近い場合は接線ベクトルを定義できないとみなす
    const double norm = (*deriv)[1].norm();
    if (norm < 1e-12) return std::nullopt;
    return (*deriv)[1] / norm;
}

std::optional<Vector3d> ICurve::RightTangentAt(const double t) const {
    // (t+h)における導関数の方向で近似
    const double t_eval = std::min(t + kTangentH, GetParameterRange()[1]);
    auto deriv = TryGetDerivatives(t_eval, 1);
    if (!deriv.has_value()) return std::nullopt;

    // 導関数がゼロベクトルに近い場合は接線ベクトルを定義できないとみなす
    const double norm = (*deriv)[1].norm();
    if (norm < 1e-12) return std::nullopt;
    return (*deriv)[1] / norm;
}

std::optional<double> ICurve::CornerExteriorAngle(
        const double t, const Vector3d& reference_normal) const {
    const double n_norm = reference_normal.norm();
    if (n_norm < 1e-12) return std::nullopt;
    const Vector3d n_hat = reference_normal / n_norm;

    auto tm = LeftTangentAt(t);
    if (!tm.has_value()) return std::nullopt;
    auto tp = RightTangentAt(t);
    if (!tp.has_value()) return std::nullopt;

    // (T⁻ × T⁺) · n̂ が符号付き sin 成分に対応
    const double cross_n = (*tm).cross(*tp).dot(n_hat);
    const double dot_val = tm->dot(*tp);
    return std::atan2(cross_n, dot_val);
}

std::optional<double> ICurve::TryGetSignedCurvature(
        const double t, const Vector3d& reference_normal) const {
    const double n_norm = reference_normal.norm();
    if (n_norm < 1e-12) return std::nullopt;
    const Vector3d n_hat = reference_normal / n_norm;

    if (IsCorner(t)) {
        // 角点では、外角αに基づいて符号付き曲率を定義する
        auto alpha = CornerExteriorAngle(t, n_hat);
        if (!alpha.has_value()) return std::nullopt;
        if (*alpha > 0) return  std::numeric_limits<double>::infinity();
        if (*alpha < 0) return -std::numeric_limits<double>::infinity();
        return 0.0;
    }

    // 直線部では曲率は0
    if (IsInLinearSegment(t)) return 0.0;

    // それ以外の点では、通常の曲率に符号をつける
    auto deriv = TryGetDerivatives(t, 2);
    if (!deriv.has_value()) return std::nullopt;

    const auto& c1 = (*deriv)[1];
    const auto& c2 = (*deriv)[2];
    const double speed3 = std::pow(c1.norm(), 3);
    if (speed3 < 1e-14) return std::nullopt;

    return c1.cross(c2).dot(n_hat) / speed3;
}

double ICurve::Length() const {
    return Length(GetParameterRange()[0], GetParameterRange()[1]);
}

double ICurve::Length(const double start, const double end) const {
    if (std::abs(start) == std::numeric_limits<double>::infinity() ||
        std::abs(end) == std::numeric_limits<double>::infinity()) {
        // 一端が無限大の場合、長さは無限大
        return std::numeric_limits<double>::infinity();
    } else if (!numerics::IsApproxLessThan(start, end)) {
        throw std::invalid_argument(
            "Invalid parameter range for Length(): start must be less than end. Got "
            "start = " + std::to_string(start) + ", end = " + std::to_string(end) + ".");
    } else if (!(GetParameterRange()[0] <= start) || !(end <= GetParameterRange()[1])) {
        throw std::invalid_argument(
            "Parameter range for Length() is out of curve's parameter range. Got "
            "start = " + std::to_string(start) + ", end = " + std::to_string(end) + ", "
            "curve parameter range = [" + std::to_string(GetParameterRange()[0]) +
            ", " + std::to_string(GetParameterRange()[1]) + "].");
    }

    auto integrand = [this](double t) -> double {
        auto deriv = TryGetDerivatives(t, 1);
        // 導関数が取得できない場合は0を返す
        if (!deriv.has_value()) return 0.0;

        const auto& c1 = (*deriv)[1];
        return c1.norm();
    };

    try {
        return i_num::Integrate(integrand, {start, end});
    } catch (const std::invalid_argument& e) {
        // パラメータの問題で積分に失敗した場合は0を返す
        return 0.0;
    }
}



/**
 * 曲線の幾何学的情報 (ベクトル) を取得する (ICurve)
 */

std::optional<Vector3d> ICurve::TryGetDefinedStartPoint() const {
    if (!HasFiniteStart()) return std::nullopt;

    return TryGetDefinedPointAt(GetParameterRange()[0]);
}

std::optional<Vector3d> ICurve::TryGetDefinedEndPoint() const {
    if (!HasFiniteEnd()) return std::nullopt;

    return TryGetDefinedPointAt(GetParameterRange()[1]);
}

std::optional<Vector3d> ICurve::TryGetDefinedPointAt(const double t) const {
    auto deriv = TryGetDerivatives(t, 0);
    if (!deriv.has_value()) return std::nullopt;

    // 0階導関数が曲線上の点
    return (*deriv)[0];
}

std::optional<Vector3d> ICurve::TryGetDefinedTangentAt(const double t) const {
    auto deriv = TryGetDerivatives(t, 1);
    if (!deriv.has_value()) return std::nullopt;

    // T = C' / |C'|
    return (*deriv)[1].normalized();
}

std::optional<Vector3d> ICurve::TryGetDefinedNormalAt(const double t) const {
    // 曲率が0（または計算できない場合の-1）ではない場合にのみ計算する
    if (GetCurvature(t) <= 0) return std::nullopt;

    auto deriv = TryGetDerivatives(t, 2);
    if (!deriv.has_value()) return std::nullopt;

    // N = (C''|C'|^2 + C'(C'・C'')) / |C'|^3
    auto c1_norm = (*deriv)[1].norm();
    auto normal = (*deriv)[2] / c1_norm
         - (*deriv)[1] * ((*deriv)[1].dot((*deriv)[2])) / std::pow(c1_norm, 3);
    return normal.normalized();
}

std::optional<Vector3d> ICurve::TryGetDefinedBinormalAt(const double t) const {
    auto tangent = TryGetDefinedTangentAt(t);
    if (!tangent.has_value()) return std::nullopt;

    auto normal = TryGetDefinedNormalAt(t);
    if (!normal.has_value()) return std::nullopt;

    // B = T × N
    return (*tangent).cross(*normal);
}

std::optional<Vector3d> ICurve::TryGetStartPoint() const {
    if (!HasFiniteStart()) return std::nullopt;

    return TryGetPointAt(GetParameterRange()[0]);
}

std::optional<Vector3d> ICurve::TryGetEndPoint() const {
    if (!HasFiniteEnd()) return std::nullopt;

    return TryGetPointAt(GetParameterRange()[1]);
}

std::optional<Vector3d> ICurve::TryGetPointAt(const double t) const  {
    return Transform(TryGetDefinedPointAt(t), true);
}

std::optional<Vector3d> ICurve::TryGetTangentAt(const double t) const {
    return Transform(TryGetDefinedTangentAt(t), false);
}

std::optional<Vector3d> ICurve::TryGetNormalAt(const double t) const {
    return Transform(TryGetDefinedNormalAt(t), false);
}

std::optional<Vector3d> ICurve::TryGetBinormalAt(const double t) const {
    return Transform(TryGetDefinedBinormalAt(t), false);
}

Vector3d ICurve::GetStartPoint() const {
    auto point = TryGetStartPoint();
    if (!point) {
        throw std::out_of_range("Curve has no finite start point.");
    }
    return *point;
}

Vector3d ICurve::GetEndPoint() const {
    auto point = TryGetEndPoint();
    if (!point) {
        throw std::out_of_range("Curve has no finite end point.");
    }
    return *point;
}

Vector3d ICurve::GetPointAt(const double t) const {
    auto point = TryGetPointAt(t);
    if (!point) {
        throw std::out_of_range("Parameter t = " + std::to_string(t) +
            " is out of range for the curve.");
    }
    return *point;
}

Vector3d ICurve::GetTangentAt(const double t) const {
    auto tangent = TryGetTangentAt(t);
    if (!tangent) {
        throw std::out_of_range("Parameter t = " + std::to_string(t) +
            " is out of range for the curve.");
    }
    return *tangent;
}

Vector3d ICurve::GetNormalAt(const double t) const {
    auto normal = TryGetNormalAt(t);
    if (!normal) {
        throw std::out_of_range("Parameter t = " + std::to_string(t) +
            " is out of range for the curve.");
    }
    return *normal;
}

Vector3d ICurve::GetBinormalAt(const double t) const {
    auto binormal = TryGetBinormalAt(t);
    if (!binormal) {
        throw std::out_of_range("Parameter t = " + std::to_string(t) +
            " is out of range for the curve.");
    }
    return *binormal;
}



/**
 * 描画に関する情報 (ICurve)
 */



/**
 * 曲線の幾何学的情報 (ベクトル) を取得する (ICurve2D)
 */

std::optional<Vector2d> ICurve2D::TryGetDefinedStartPoint2D() const {
    auto pt = TryGetDefinedStartPoint();
    if (pt)  return Vector2d(pt->x(), pt->y());
    return std::nullopt;
}

std::optional<Vector2d> ICurve2D::TryGetDefinedEndPoint2D() const {
    auto pt = TryGetDefinedEndPoint();
    if (pt)  return Vector2d(pt->x(), pt->y());
    return std::nullopt;
}

std::optional<Vector2d> ICurve2D::TryGetDefinedPointAt2D(const double t) const {
    auto pt = TryGetDefinedPointAt(t);
    if (pt)  return Vector2d(pt->x(), pt->y());
    return std::nullopt;
}

std::optional<Vector2d> ICurve2D::TryGetDefinedTangentAt2D(const double t) const {
    auto tangent = TryGetDefinedTangentAt(t);
    if (tangent)  return Vector2d(tangent->x(), tangent->y());
    return std::nullopt;
}

std::optional<Vector2d> ICurve2D::TryGetDefinedNormalAt2D(const double t) const {
    auto normal = TryGetDefinedNormalAt(t);
    if (normal)  return Vector2d(normal->x(), normal->y());
    return std::nullopt;
}

Vector2d ICurve2D::GetStartPoint2D() const {
    auto point = TryGetDefinedStartPoint2D();
    if (!point) throw std::out_of_range("Curve has no finite start point.");
    return *point;
}

Vector2d ICurve2D::GetEndPoint2D() const {
    auto point = TryGetDefinedEndPoint2D();
    if (!point) throw std::out_of_range("Curve has no finite end point.");
    return *point;
}
