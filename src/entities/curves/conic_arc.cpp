/**
 * @file entities/curves/conic_arc.cpp
 * @brief Conic Arc (Type 104): 円錐曲線エンティティの定義
 * @author Yayoi Habami
 * @date 2025-08-08
 * @copyright 2025 Yayoi Habami
 * @note このファイルは、円錐曲線 (楕円、放物線、双曲線) を表すエンティティの定義を含む。
 */
#include "igesio/entities/curves/conic_arc.h"

#include <string>
#include <utility>
#include <vector>

#include "igesio/common/tolerance.h"

namespace {

using ConicArc = igesio::entities::ConicArc;
namespace i_ent = igesio::entities;

/// @brief パラメータ範囲を計算するヘルパー
/// @param[in] start_param 始点に対応するパラメータ
/// @param[in] end_param 終点に対応するパラメータ
/// @param[in] is_ellipse 楕円かどうか
/// @return パラメータ範囲 {t_start, t_end}
static std::array<double, 2> calculate_parameter_range(
        double start_param, double end_param, bool is_ellipse) {
    if (is_ellipse) {
        // 楕円の場合、角度は反時計回りに増加する
        // 始点の角度を [0, 2*PI) の範囲に正規化
        if (start_param < 0) {
            start_param += 2.0 * igesio::kPi;
        }
        // 終点の角度を、始点から反時計回りに進んだ角度として計算
        if (end_param <= start_param) {
            end_param += 2.0 * igesio::kPi;
        }
        return {start_param, end_param};
    } else {
        // 放物線、双曲線の場合
        if (start_param > end_param) {
            std::swap(start_param, end_param);
        }
        return {start_param, end_param};
    }
}

std::optional<i_ent::ConicType>
CalculateConicType(std::array<double, 6> coeffs) {
    auto [A, B, C, D, E, F] = coeffs;

    // 規格書の式だと正しく判定できないことがあるため、q2の式のみを使用
    auto q2 = A * C - (B * B) / 4.0;

    if (igesio::IsApproxZero(q2)) {
        // parabola
        return i_ent::ConicType::kParabola;
    } else if (igesio::IsApproxGreaterThan(q2, 0.0)) {
        // ellipse
        return i_ent::ConicType::kEllipse;
    } else if (igesio::IsApproxLessThan(q2, 0.0)) {
        // hyperbola
        return i_ent::ConicType::kHyperbola;
    }
    return std::nullopt;  // 不正な円錐曲線
}

}  // namespace




std::string
i_ent::ToString(const ConicType type) {
    switch (type) {
        case ConicType::kEllipse:
            return "Ellipse";
        case ConicType::kHyperbola:
            return "Hyperbola";
        case ConicType::kParabola:
            return "Parabola";
        default:
            return "Unknown";
    }
}




/**
 * コンストラクタ
 */

ConicArc::ConicArc(const RawEntityDE& de_record,
                   const IGESParameterVector& parameters,
                   const pointer2ID& de2id,
                   const ObjectID& iges_id)
        : EntityBase(de_record, parameters, de2id, iges_id) {
    InitializePD(de2id);
}

ConicArc::ConicArc(const std::array<double, 6>& coeffs,
                   const Vector2d& start_point, const Vector2d& terminate_point,
                   const double z_t)
        : EntityBase(
                RawEntityDE::ByDefault(EntityType::kConicArc,
                        static_cast<int>((::CalculateConicType(coeffs))
                                         .value_or(ConicType::kEllipse))),
                IGESParameterVector{
                    coeffs[0], coeffs[1], coeffs[2],
                    coeffs[3], coeffs[4], coeffs[5], z_t,
                    start_point[0], start_point[1],
                    terminate_point[0], terminate_point[1]}) {
    InitializePD({});

    auto calculated_type = ::CalculateConicType(coeffs);
    if (!calculated_type.has_value() || GetConicType() != *calculated_type) {
        throw igesio::DataFormatError(
            "The provided coefficients do not match the conic type.");
    }
}

ConicArc::ConicArc(const std::pair<double, double>& radius,
                   const double start_angle, const double end_angle,
                   const double z_t)
        : EntityBase(
                RawEntityDE::ByDefault(EntityType::kConicArc,
                        static_cast<int>(i_ent::ConicType::kEllipse)),
                IGESParameterVector{}) {
    auto rx = radius.first, ry = radius.second;
    if (IsApproxZero(rx) || IsApproxZero(ry)) {
        throw igesio::DataFormatError("Invalid radius for ConicArc");
    }
    auto rx2 = rx * rx, ry2 = ry * ry;

    pd_parameters_ = IGESParameterVector{
            ry2, 0.0, rx2, 0.0,                // A, B, C, D
            0.0, - rx2 * ry2, z_t,             // E, F, z_t
            0.0 + rx * std::cos(start_angle),  // 始点 x
            0.0 + ry * std::sin(start_angle),  // 始点 y
            0.0 + rx * std::cos(end_angle),    // 終点 x
            0.0 + ry * std::sin(end_angle)     // 終点 y
    };

    InitializePD({});
}

i_ent::ConicType ConicArc::GetConicType() const {
    return static_cast<i_ent::ConicType>(form_number_);
}



/**
 * EntityBase implementation
 */

igesio::IGESParameterVector ConicArc::GetMainPDParameters() const {
    IGESParameterVector params = {
            coeffs_[0],       coeffs_[1],       coeffs_[2],
            coeffs_[3],       coeffs_[4],       coeffs_[5],
            start_point_[2],  start_point_[0],  start_point_[1],
            terminate_point_[0], terminate_point_[1]};

    // 元のフォーマット情報を適用
    for (size_t i = 0; i < pd_parameters_.size(); ++i) {
        try {
            params.set_format(i, pd_parameters_.get_format(i));
        } catch (const std::invalid_argument&) {
            // フォーマットが不正な場合は無視
        }
    }
    return params;
}

size_t ConicArc::SetMainPDParameters(const pointer2ID& de2id) {
    auto& pd = pd_parameters_;
    if (pd.size() < 11) {
        throw DataFormatError("ConicArc requires at least 11 parameters");
    }

    for (size_t i = 0; i < 6; ++i) {
        coeffs_[i] = pd.access_as<double>(i);
    }
    const auto z_t = pd.access_as<double>(6);
    start_point_ = {pd.access_as<double>(7), pd.access_as<double>(8), z_t};
    terminate_point_ = {pd.access_as<double>(9), pd.access_as<double>(10), z_t};

    return 11;
}

igesio::ValidationResult ConicArc::ValidatePD() const {
    std::vector<ValidationError> errors;

    // 1. 始点と終点が2次曲線上にあるか検証
    if (!IsOnConic(start_point_.x(), start_point_.y())) {
        errors.emplace_back(
            "Start point does not lie on the conic curve.");
    }
    if (!IsOnConic(terminate_point_.x(), terminate_point_.y())) {
        errors.emplace_back(
            "Terminate point does not lie on the conic curve.");
    }

    // 2. DEのForm Numberと係数から計算した曲線種別が一致するか検証
    auto calculated_type = CalculateConicType();
    if (!calculated_type.has_value() || GetConicType() != *calculated_type) {
        std::string type_val_str = "undefined";
        if (calculated_type.has_value()) {
            type_val_str = std::to_string(static_cast<int>(*calculated_type));
        }

        errors.emplace_back(
            "Form number in Directory Entry does not match the conic type "
            "derived from coefficients. Expected: " + ToString(*calculated_type) +
            " (" + type_val_str + "), Actual: " + ToString(GetConicType()) +
            " (" + std::to_string(form_number_) + ").");
    }

    // 3. 縮退形式でないか検証 (Q1=0)
    auto [A, B, C, D, E, F] = coeffs_;
    auto q1 = A * (C * F - E * E / 4.0) -
              B / 2.0 * (B / 2.0 * F - E / 2.0 * D / 2.0) +
              D / 2.0 * (B / 2.0 * E / 2.0 - C * D / 2.0);
    if (IsApproxZero(q1, kGeometryTolerance)) {
        errors.emplace_back(
            "Degenerate conic (e.g., point or line) is not allowed for "
            "ConicArc entity.");
    }

    return MakeValidationResult(std::move(errors));
}



/**
 * ICurve implementation
 */

std::array<double, 2> ConicArc::GetParameterRange() const {
    auto [A, B, C, D, E, F] = coeffs_;

    auto x1 = start_point_.x(), y1 = start_point_.y();
    auto x2 = terminate_point_.x(), y2 = terminate_point_.y();

    switch (GetConicType()) {
        case ConicType::kEllipse: {
            auto a = std::sqrt(-F / A), b = std::sqrt(-F / C);
            auto t1 = std::atan2(y1 / b, x1 / a);
            auto t2 = std::atan2(y2 / b, x2 / a);
            auto range = calculate_parameter_range(t1, t2, true);
            if (IsClosed()) {
                range[1] = range[0] + 2.0 * kPi;
            }
            return range;
        }
        case ConicType::kParabola: {
            if (!IsApproxZero(A) && !IsApproxZero(E)) {  // Y = k * X^2
                return calculate_parameter_range(x1, x2, false);
            }
            if (!IsApproxZero(C) && !IsApproxZero(D)) {  // X = k * Y^2
                return calculate_parameter_range(y1, y2, false);
            }
            break;  // 不正な放物線
        }
        case ConicType::kHyperbola: {
            if (F * A < 0.0) {  // X-axis is transverse axis
                auto a = std::sqrt(-F / A), b = std::sqrt(F / C);
                // t = atanh(y/b) or asinh(y/b)
                auto t1 = std::asinh(y1 / b);
                auto t2 = std::asinh(y2 / b);
                return calculate_parameter_range(t1, t2, false);
            } else {  // Y-axis is transverse axis
                auto a = std::sqrt(F / A), b = std::sqrt(-F / C);
                // t = atanh(x/a) or asinh(x/a)
                auto t1 = std::asinh(x1 / a);
                auto t2 = std::asinh(x2 / a);
                return calculate_parameter_range(t1, t2, false);
            }
        }
        default:
            break;
    }
    // 円錐曲線として不正な場合
    return {0.0, 0.0};
}

bool ConicArc::IsClosed() const {
    // 楕円で始点と終点が一致する場合のみ閉曲線とみなす
    if (GetConicType() != ConicType::kEllipse) {
        return false;
    }
    return IsApproxEqual(start_point_, terminate_point_, kGeometryTolerance);
}

std::optional<igesio::Vector3d>
ConicArc::TryGetDefinedPointAt(const double t) const {
    const auto range = GetParameterRange();
    if (t < range[0] - kGeometryTolerance || t > range[1] + kGeometryTolerance) {
        return std::nullopt;
    }

    auto [A, B, C, D, E, F] = coeffs_;
    auto z_t = start_point_.z();

    Vector3d point;

    switch (GetConicType()) {
        case ConicType::kEllipse: {
            auto a = std::sqrt(-F / A), b = std::sqrt(-F / C);
            point = {a * std::cos(t), b * std::sin(t), z_t};
            break;
        }
        case ConicType::kParabola: {
            if (!IsApproxZero(A) && !IsApproxZero(E)) {   // Y = k * X^2
                point = {t, -(A / E) * t * t, z_t};
            } else if (!IsApproxZero(C) && !IsApproxZero(D)) {   // X = k * Y^2
                point = {-(C / D) * t * t, t, z_t};
            } else {
                return std::nullopt;   // Invalid parabola
            }
            break;
        }
        case ConicType::kHyperbola: {
            if (F * A < 0.0) {   // X-axis is transverse axis
                auto a = std::sqrt(-F / A), b = std::sqrt(F / C);
                point = {a * std::sinh(t), b * std::cosh(t), z_t};
            } else {   // Y-axis is transverse axis
                auto a = std::sqrt(F / A), b = std::sqrt(-F / C);
                point = {a * std::sinh(t), b * std::cosh(t), z_t};
            }
            // Hyperbola has two branches. Need to check if the sign is correct.
            // The parameterization with sinh/cosh only covers one branch (x>0 or y>0).
            // We check the sign of the start point to determine the branch.
            if (start_point_.x() < 0) point.x() = -point.x();
            if (start_point_.y() < 0) point.y() = -point.y();
            break;
        }
        default:
            return std::nullopt;
    }
    return point;
}

std::optional<igesio::Vector3d>
ConicArc::TryGetDefinedTangentAt(const double t) const {
    const auto range = GetParameterRange();
    if (t < range[0] - kGeometryTolerance || t > range[1] + kGeometryTolerance) {
        return std::nullopt;
    }

    auto [A, B, C, D, E, F] = coeffs_;

    Vector3d tangent{0.0, 0.0, 0.0};

    switch (GetConicType()) {
        case ConicType::kEllipse: {
            auto [a, b] = EllipseRadii();
            tangent = {-a * std::sin(t), b * std::cos(t), 0.0};
            break;
        }
        case ConicType::kParabola: {
            if (!IsApproxZero(A) && !IsApproxZero(E)) {  // Y = k * X^2
                tangent = {1.0, -2.0 * (A / E) * t, 0.0};
            } else if (!IsApproxZero(C) && !IsApproxZero(D)) {  // X = k * Y^2
                tangent = {-2.0 * (C / D) * t, 1.0, 0.0};
            } else {
                return std::nullopt;  // Invalid parabola
            }
            break;
        }
        case ConicType::kHyperbola: {
            if (F * A < 0.0) {  // X-axis is transverse axis
                auto a = std::sqrt(-F / A), b = std::sqrt(F / C);
                tangent = {a * std::sinh(t), b * std::cosh(t), 0.0};
            } else {  // Y-axis is transverse axis
                auto a = std::sqrt(F / A), b = std::sqrt(-F / C);
                tangent = {a * std::cosh(t), b * std::sinh(t), 0.0};
            }
            // Adjust sign for the correct branch
            if (start_point_.x() < 0) tangent.x() = -tangent.x();
            if (start_point_.y() < 0) tangent.y() = -tangent.y();
            break;
        }
        default:
            return std::nullopt;
    }

    if (IsApproxZero(tangent.norm(), kGeometryTolerance)) {
        return std::nullopt;  // Degenerate tangent
    }
    return tangent.normalized();
}

std::optional<igesio::Vector3d>
ConicArc::TryGetDefinedNormalAt(const double t) const {
    auto tangent = TryGetDefinedTangentAt(t);
    if (!tangent) {
        return std::nullopt;
    }
    // 2D平面内の法線ベクトル (接線を+90度回転)
    return Vector3d{-tangent->y(), tangent->x(), 0.0}.normalized();
}

std::optional<igesio::Vector3d>
ConicArc::TryGetPointAt(const double t) const {
    return TransformPoint(TryGetDefinedPointAt(t));
}

std::optional<igesio::Vector3d>
ConicArc::TryGetTangentAt(const double t) const {
    return TransformVector(TryGetDefinedTangentAt(t));
}

std::optional<igesio::Vector3d>
ConicArc::TryGetNormalAt(const double t) const {
    return TransformVector(TryGetDefinedNormalAt(t));
}



/**
 * 描画用
 */
igesio::Vector3d ConicArc::EllipseCenter() const {
    // IGES 5.3の実装では、楕円中心は常に原点
    return {0.0, 0.0, start_point_.z()};
}

std::pair<double, double> ConicArc::EllipseRadii() const {
    return {std::sqrt(-coeffs_[5] / coeffs_[0]),
            std::sqrt(-coeffs_[5] / coeffs_[2])};
}

double ConicArc::EllipseStartAngle() const {
    return GetParameterRange()[0];
}

double ConicArc::EllipseEndAngle() const {
    return GetParameterRange()[1];
}



/**
 * private members
 */

std::optional<i_ent::ConicType> ConicArc::CalculateConicType() const {
    return ::CalculateConicType(coeffs_);
}

bool ConicArc::IsOnConic(const double x, const double y) const {
    auto [A, B, C, D, E, F] = coeffs_;

    const double value =
            A * x * x + B * x * y + C * y * y + D * x + E * y + F;
    return IsApproxZero(value, kGeometryTolerance);
}
