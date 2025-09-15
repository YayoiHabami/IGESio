/**
 * @file entities/curves/linear_path.h
 * @brief Linear Path (Type 106, forms 11-13) クラス
 * @author Yayoi Habami
 * @date 2025-09-12
 * @copyright 2025 Yayoi Habami
 * @note Linear Path (Forms 11-13) のエンティティクラス.
 */
#include "igesio/entities/curves/linear_path.h"

#include "igesio/common/tolerance.h"

namespace {

namespace i_ent = igesio::entities;
using i_ent::LinearPath;
using igesio::Vector3d;

}  // namespace



igesio::ValidationResult LinearPath::ValidatePD() const {
    auto result = CopiousDataBase::ValidatePD();

    // CopiousDataTypeが1-3のいずれかであることを確認
    auto type = GetDataType();
    if (type != CopiousDataType::kPlanarPolyline &&
        type != CopiousDataType::kPolyline3D &&
        type != CopiousDataType::kPolylineAndVectors) {
        result.AddError(ValidationError(
            "Invalid CopiousDataType for CopiousData: " +
            std::to_string(static_cast<int>(type))));
    }
    return result;
}



/**
 * ICurve implementation
 */

bool LinearPath::IsClosed() const { return false; }

std::array<double, 2> LinearPath::GetParameterRange() const {
    return {0.0, TotalLength()};
}

std::optional<Vector3d>
LinearPath::TryGetDefinedPointAt(const double t) const {
    return GetCoordinateAtLength(t);
}

std::optional<Vector3d>
LinearPath::TryGetDefinedTangentAt(const double) const {
    return std::nullopt;
}

std::optional<Vector3d>
LinearPath::TryGetDefinedNormalAt(const double) const {
    return std::nullopt;
}

std::optional<Vector3d>
LinearPath::TryGetPointAt(const double) const {
    return TransformPoint(TryGetDefinedEndPoint());
}

std::optional<Vector3d>
LinearPath::TryGetTangentAt(const double t) const {
    return TransformPoint(TryGetDefinedTangentAt(t));
}

std::optional<Vector3d>
LinearPath::TryGetNormalAt(const double t) const {
    return TransformPoint(TryGetDefinedNormalAt(t));
}
