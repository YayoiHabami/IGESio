/**
 * @file entities/curves/copious_data.cpp
 * @brief Copious Data (Type 106, forms 1-3) クラス
 * @author Yayoi Habami
 * @date 2025-09-12
 * @copyright 2025 Yayoi Habami
 * @note Copious Data (Forms 1-3) のエンティティクラス.
 */
#include "igesio/entities/curves/copious_data.h"

#include <memory>
#include <vector>

#include "igesio/numerics/core/tolerance.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using i_ent::CopiousData;
using Vector3d = igesio::Vector3d;

/// @brief 2次元の点列を3xN行列に変換する (z行は共通のz_t)
igesio::Matrix3Xd ToCoordinateMatrix(
        const std::vector<igesio::Vector2d>& points, const double z_t) {
    igesio::Matrix3Xd mat(3, points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        mat.block<2, 1>(0, i) = points[i];
        mat(2, i) = z_t;
    }
    return mat;
}

/// @brief 3次元の点列を3xN行列に変換する
igesio::Matrix3Xd ToCoordinateMatrix(const std::vector<Vector3d>& points) {
    igesio::Matrix3Xd mat(3, points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        mat.col(i) = points[i];
    }
    return mat;
}

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
CopiousData::TryGetDefinedDerivatives(const double t, const unsigned int n) const {
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


/**
 * ファクトリ関数
 */

std::shared_ptr<CopiousData> i_ent::MakeCopiousData(
        const std::vector<Vector2d>& points, const double z_t) {
    return std::make_shared<CopiousData>(
            CopiousDataType::kPlanarPoints, ToCoordinateMatrix(points, z_t));
}

std::shared_ptr<CopiousData> i_ent::MakeCopiousData(
        const std::vector<Vector3d>& points) {
    return std::make_shared<CopiousData>(
            CopiousDataType::kPoints3D, ToCoordinateMatrix(points));
}

std::shared_ptr<CopiousData> i_ent::MakeCopiousData(
        const std::vector<Vector3d>& points,
        const std::vector<Vector3d>& vectors) {
    // 点とベクトルの数の不一致はCopiousDataBaseコンストラクタが検証する
    return std::make_shared<CopiousData>(
            CopiousDataType::kSextuples,
            ToCoordinateMatrix(points), ToCoordinateMatrix(vectors));
}
