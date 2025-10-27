/**
 * @file entities/curves/copious_data.cpp
 * @brief Copious Data (Type 106, forms 1-3) クラス
 * @author Yayoi Habami
 * @date 2025-09-12
 * @copyright 2025 Yayoi Habami
 * @note Copious Data (Forms 1-3) のエンティティクラス.
 */
#include "igesio/entities/curves/copious_data.h"

#include "igesio/numerics/tolerance.h"

namespace {

namespace i_num = igesio::numerics;
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
    // パラメータ範囲は常に[0, 全長]
    // ここでの全長は曲線としての長さではなく、頂点列の長さ
    return {0.0, CopiousDataBase::Length()};
}

std::optional<i_ent::CurveDerivatives>
CopiousData::TryGetDerivatives(const double t, const unsigned int n) const {
    if (t < GetParameterRange()[0] || t > GetParameterRange()[1]) {
        return std::nullopt;
    }

    CurveDerivatives result(n);

    auto [index, dist] = GetNearestVertexAt(t);
    if (i_num::IsApproxZero(dist, i_num::kGeometryTolerance)) {
        result[0] = Coordinates().col(index);
    }
    return result;
}
