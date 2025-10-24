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

/// @brief 楕円のパラメータ範囲を計算するヘルパー (正規化する)
/// @param[in] start_param 始点に対応するパラメータ
/// @param[in] end_param 終点に対応するパラメータ
/// @return パラメータ範囲 {t_start, t_end}
static std::array<double, 2>
RegularizeParameterRange(double start_param, double end_param) {
    // 楕円の場合、角度は反時計回りに増加する
    // 始点の角度を [0, 2*PI] の範囲に正規化
    if (start_param < 0) {
        start_param += 2.0 * igesio::kPi;
    }
    // 終点の角度を、始点から反時計回りに進んだ角度として計算
    if (end_param <= start_param) {
        end_param += 2.0 * igesio::kPi;
    }
    return {start_param, end_param};
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

    auto xs = start_point_.x(), ys = start_point_.y();
    auto xe = terminate_point_.x(), ye = terminate_point_.y();

    switch (GetConicType()) {
        case ConicType::kEllipse: {
            auto [rx, ry] = EllipseRadii();
            auto t1 = std::atan2(ys / ry, xs / rx);
            auto t2 = std::atan2(ye / ry, xe / rx);

            // パラメータ範囲を調整 (ただし閉曲線の場合は1周分に設定)
            auto range = RegularizeParameterRange(t1, t2);
            if (IsClosed()) range[1] = range[0] + 2.0 * kPi;
            return range;
        }
        case ConicType::kParabola: {
            if (!IsApproxZero(A) && !IsApproxZero(E)) {  // Y = k * X^2
                return (xs < xe) ? std::array{xs, xe} : std::array{-xs, -xe};
            }
            if (!IsApproxZero(C) && !IsApproxZero(D)) {  // X = k * Y^2
                return (ys < ye) ? std::array{ys, ye} : std::array{-ys, -ye};
            }
            break;  // 不正な放物線
        }
        case ConicType::kHyperbola: {
            if ((F * A < 0.0) && (F * C > 0.0)) {  // X-axis is transverse axis
                auto t_s = std::atan(ys * std::sqrt(C / F));
                auto t_e = std::atan(ye * std::sqrt(C / F));
                return (t_s < t_e) ? std::array{t_s, t_e} : std::array{-t_s, -t_e};
            } else if ((F * A > 0.0) && (F * C < 0.0)) {  // Y-axis is transverse axis
                auto t_s = std::atan(xs * std::sqrt(A / F));
                auto t_e = std::atan(xe * std::sqrt(A / F));
                return (t_s < t_e) ? std::array{t_s, t_e} : std::array{-t_s, -t_e};
            }
            break;  // 不正な双曲線
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

std::optional<i_ent::CurveDerivatives>
ConicArc::TryGetDerivatives(const double t, const unsigned int n) const {
    // 計算式の詳細は docs/entities/curves/104_conic_arc_ja.md を参照
    switch (GetConicType()) {
        case ConicType::kEllipse: {
            return TryGetEllipseDerivatives(t, n);
        }
        case ConicType::kParabola: {
            return TryGetParabolaDerivatives(t, n);
        }
        case ConicType::kHyperbola: {
            return TryGetHyperbolaDerivatives(t, n);
        }
        default:
            return std::nullopt;
    }
    return std::nullopt;
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

std::optional<i_ent::CurveDerivatives>
ConicArc::TryGetEllipseDerivatives(const double t, const unsigned int n) const {
    const auto range = GetParameterRange();
    if (t < range[0] - kGeometryTolerance || t > range[1] + kGeometryTolerance) {
        return std::nullopt;
    }

    CurveDerivatives result(n);

    auto z_t = start_point_.z();

    auto [rx, ry] = EllipseRadii();
    // n階導関数を一般式で計算（位相は k * PI/2 で増える）
    for (unsigned int k = 0; k <= n; ++k) {
        double phase = t + static_cast<double>(k) * (kPi / 2.0);
        result[k] = Vector3d{rx * std::cos(phase), ry * std::sin(phase), 0.0};

        // z成分の設定 (0階のみz_t)
        if (k == 0) result[k].z() = z_t;
    }

    return result;
}

std::optional<i_ent::CurveDerivatives>
ConicArc::TryGetParabolaDerivatives(const double t, const unsigned int n) const {
    const auto range = GetParameterRange();
    if (t < range[0] - kGeometryTolerance || t > range[1] + kGeometryTolerance) {
        return std::nullopt;
    }

    CurveDerivatives result(n);

    auto [A, B, C, D, E, F] = coeffs_;
    auto xs = start_point_.x(), ys = start_point_.y();
    auto xe = terminate_point_.x(), ye = terminate_point_.y();
    auto z_t = start_point_.z();

    if (!IsApproxZero(A) && !IsApproxZero(E)) {   // Y = k * X^2
        double x_coef = (xs < xe) ? 1.0 : -1.0;
        for (unsigned int k = 0; k < n; ++k) {
            if (k == 0) {
                result[k] = Vector3d{x_coef*t, -(A/E)*t*t, z_t};
            } else if (k == 1) {
                result[k] = Vector3d{x_coef, -2.0*(A/E)*t, 0.0};
            } else if (k == 2) {
                result[k] = Vector3d{   0.0, -2.0*(A/E), 0.0};
            }
            // 3以上についてはゼロであり、Resizeで初期化されているため省略
        }
    } else if (!IsApproxZero(C) && !IsApproxZero(D)) {   // X = k * Y^2
        double y_coef = (ys < ye) ? 1.0 : -1.0;
        for (unsigned int k = 0; k < n; ++k) {
            if (k == 0) {
                result[k] = Vector3d{-(C/D)*t*t, y_coef*t, z_t};
            } else if (k == 1) {
                result[k] = Vector3d{-2.0*(C/D)*t, y_coef, 0.0};
            } else if (k == 2) {
                result[k] = Vector3d{-2.0*(C/D),      0.0, 0.0};
            }
            // 3以上についてはゼロであり、Resizeで初期化されているため省略
        }
    }

    return result;
}

std::optional<i_ent::CurveDerivatives>
ConicArc::TryGetHyperbolaDerivatives(const double t, const unsigned int n) const {
    const auto range = GetParameterRange();
    if (t < range[0] - kGeometryTolerance || t > range[1] + kGeometryTolerance) {
        return std::nullopt;
    }

    CurveDerivatives result(n);

    auto [A, B, C, D, E, F] = coeffs_;
    auto xs = start_point_.x(), ys = start_point_.y();
    auto xe = terminate_point_.x(), ye = terminate_point_.y();
    auto z_t = start_point_.z();

    double sec_t = 1.0 / std::cos(t), tan_t = std::tan(t);
    double sec_t2 = sec_t * sec_t;
    double sec3_sec1tan2 = sec_t * (sec_t * sec_t + tan_t * tan_t);

    auto error = igesio::NotImplementedError(
            "Derivatives of hyperbola higher than 2 are not implemented.");

    if (F * A < 0.0) {   // X-axis is transverse axis
        double a = std::sqrt(-F / A), b = std::sqrt(F / C);
        double sgn = (ys < ye) ? 1.0 : -1.0;

        for (unsigned int k = 0; k < n; ++k) {
            if (k == 0) {
                result[k] = Vector3d{a / std::cos(t), sgn * b * std::tan(t), z_t};
            } else if (k == 1) {
                result[k] = Vector3d{a * sec_t * tan_t, sgn * b * sec_t2, 0.0};
            } else if (k == 2) {
                result[k] = Vector3d{a * sec3_sec1tan2, sgn * 2 * b * sec_t2 * tan_t, 0.0};
            } else {
                throw error;
            }
        }
    } else {   // Y-axis is transverse axis
        double a = std::sqrt(F / A), b = std::sqrt(-F / C);
        double sgn = (xs < xe) ? 1.0 : -1.0;

        for (unsigned int k = 0; k < n; ++k) {
            if (k == 0) {
                result[k] = Vector3d{sgn * a * std::tan(t), b / std::cos(t), z_t};
            } else if (k == 1) {
                result[k] = Vector3d{sgn * a * sec_t2, b * sec_t * tan_t, 0.0};
            } else if (k == 2) {
                result[k] = Vector3d{sgn * 2 * a * sec_t2 * tan_t, b * sec3_sec1tan2, 0.0};
            } else {
                throw error;
            }
        }
    }

    return result;
}
