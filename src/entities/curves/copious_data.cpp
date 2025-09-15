/**
 * @file entities/curves/copious_data.cpp
 * @brief Copious Data (Type 106, forms 1-3) クラス
 * @author Yayoi Habami
 * @date 2025-09-12
 * @copyright 2025 Yayoi Habami
 * @note Copious Data (Forms 1-3) のエンティティクラス.
 */
#include "igesio/entities/curves/copious_data.h"

#include "igesio/common/tolerance.h"

namespace {

namespace i_ent = igesio::entities;
using i_ent::CopiousData;
using Vector3d = igesio::Vector3d;

}  // namespace



igesio::ValidationResult CopiousData::ValidatePD() const {
    auto result = CopiousDataBase::ValidatePD();

    // CopiousDataTypeが1-3のいずれかであることを確認
    auto type = GetDataType();
    if (type != CopiousDataType::kPlanarPoints &&
        type != CopiousDataType::kPoints3D &&
        type != CopiousDataType::kSextuples) {
        result.AddError(ValidationError(
            "Invalid CopiousDataType for CopiousData: " +
            std::to_string(static_cast<int>(type))));
    }
    return result;
}



/**
 * ICurve implementation
 */

bool CopiousData::IsClosed() const { return false; }

std::array<double, 2> CopiousData::GetParameterRange() const {
    return {0.0, TotalLength()};
}

std::optional<Vector3d>
CopiousData::TryGetDefinedPointAt(const double t) const {
    if (t < 0 || t > TotalLength()) return std::nullopt;

    auto [index, dist] = GetNearestVertexAt(t);
    if (IsApproxZero(dist, kGeometryTolerance)) {
        return Coordinates().col(index);
    }
    return std::nullopt;
}

std::optional<Vector3d>
CopiousData::TryGetDefinedTangentAt(const double) const {
    return std::nullopt;
}

std::optional<Vector3d>
CopiousData::TryGetDefinedNormalAt(const double) const {
    return std::nullopt;
}

std::optional<Vector3d>
CopiousData::TryGetPointAt(const double) const {
    return TransformPoint(TryGetDefinedEndPoint());
}

std::optional<Vector3d>
CopiousData::TryGetTangentAt(const double t) const {
    return TransformPoint(TryGetDefinedTangentAt(t));
}

std::optional<Vector3d>
CopiousData::TryGetNormalAt(const double t) const {
    return TransformPoint(TryGetDefinedNormalAt(t));
}
