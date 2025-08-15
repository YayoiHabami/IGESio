/**
 * @file entities/interfaces/i_curve.cpp
 * @brief 曲線クラスのインターフェース定義
 * @author Yayoi Habami
 * @date 2025-08-02
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/interfaces/i_curve.h"

namespace {

namespace i_ent = igesio::entities;

using Vector2d = igesio::Vector2d;
using Vector3d = igesio::Vector3d;
using ICurve = i_ent::ICurve;
using ICurve2D = i_ent::ICurve2D;
using ICurve3D = i_ent::ICurve3D;

}  // namespace



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

std::optional<Vector3d> ICurve::TryGetDefinedStartPoint() const {
    if (!HasFiniteStart()) return std::nullopt;

    return TryGetDefinedPointAt(GetParameterRange()[0]);
}

std::optional<Vector3d> ICurve::TryGetDefinedEndPoint() const {
    if (!HasFiniteEnd()) return std::nullopt;

    return TryGetDefinedPointAt(GetParameterRange()[1]);
}

std::optional<Vector3d> ICurve::TryGetStartPoint() const {
    if (!HasFiniteStart()) return std::nullopt;

    return TryGetPointAt(GetParameterRange()[0]);
}

std::optional<Vector3d> ICurve::TryGetEndPoint() const {
    if (!HasFiniteEnd()) return std::nullopt;

    return TryGetPointAt(GetParameterRange()[1]);
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
