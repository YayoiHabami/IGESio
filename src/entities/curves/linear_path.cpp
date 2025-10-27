/**
 * @file entities/curves/linear_path.h
 * @brief Linear Path (Type 106, forms 11-13) クラス
 * @author Yayoi Habami
 * @date 2025-09-12
 * @copyright 2025 Yayoi Habami
 * @note Linear Path (Forms 11-13) のエンティティクラス.
 */
#include "igesio/entities/curves/linear_path.h"

#include <vector>

#include "igesio/numerics/tolerance.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using i_ent::LinearPath;
using igesio::Vector3d;

}  // namespace



igesio::ValidationResult LinearPath::ValidatePD() const {
    auto result = CopiousDataBase::ValidatePD();

    // CopiousDataTypeが11-13, 63のいずれかであることを確認
    auto type = GetDataType();
    if (type != CopiousDataType::kPlanarPolyline &&
        type != CopiousDataType::kPolyline3D &&
        type != CopiousDataType::kPolylineAndVectors &&
        type != CopiousDataType::kPlanarLoop) {
        result.AddError(ValidationError(
            "Invalid CopiousDataType for CopiousData: " +
            std::to_string(static_cast<int>(type))));
    }
    return result;
}

LinearPath::LinearPath(const std::vector<Vector2d>& coordinates, const bool is_closed)
        : CopiousDataBase(is_closed ? CopiousDataType::kPlanarLoop
                                    : CopiousDataType::kPlanarPolyline,
                          Matrix3Xd(3, coordinates.size())) {
    // 座標値を3xNの行列に変換して格納
    for (size_t i = 0; i < coordinates.size(); ++i) {
        coordinates_.block<2, 1>(0, i) = coordinates[i];
        coordinates_(2, i) = 0.0;  // z座標は0に設定
    }
}

LinearPath::LinearPath(const std::vector<Vector3d>& coordinates)
        : CopiousDataBase(CopiousDataType::kPolyline3D,
                          Matrix3Xd(3, coordinates.size())) {
    // 座標値を3xNの行列に変換して格納
    for (size_t i = 0; i < coordinates.size(); ++i) {
        coordinates_.col(i) = coordinates[i];
    }
}



/**
 * ICurve implementation
 */

bool LinearPath::IsClosed() const {
    // kPlanarLoopの場合は常に閉じている
    if (GetDataType() == CopiousDataType::kPlanarLoop) return true;

    // 点が2点未満の場合は閉じていない
    if (GetCount() < 2) return false;

    // 最初の点と最後の点が一致するか確認
    const auto& first_point = Coordinate(0);
    const auto& last_point = Coordinate(GetCount() - 1);
    if (i_num::IsApproxEqual(first_point, last_point, i_num::kGeometryTolerance)) {
        return true;
    }
    return false;
}

std::array<double, 2> LinearPath::GetParameterRange() const {
    return {0.0, Length()};
}

std::optional<i_ent::CurveDerivatives>
LinearPath::TryGetDerivatives(const double t, const unsigned int n) const {
    auto idx = GetSegmentIndexAt(t);
    if (!idx.has_value())  return std::nullopt;

    CurveDerivatives result(n);

    // 0階導関数: 点の座標値
    auto point_opt = GetCoordinateAtLength(t);
    if (point_opt.has_value()) {
        result[0] = point_opt.value();
    } else {
        // エラー
        return std::nullopt;
    }
    if (n < 1) return result;

    // 1階導関数
    if ((*idx) < GetCount() - 2) {
        // セグメントが始点から終点までの間にある場合
        result[1] = (Coordinate(*idx + 1) - Coordinate(*idx)).normalized();
    } else if ((*idx) == GetCount() - 2) {
        // セグメントが終点と始点を結ぶ線分（kPlanarLoopの場合）にある場合
        result[1] = (Coordinate(0) - Coordinate(*idx)).normalized();
    } else {
        // エラー
        return std::nullopt;
    }

    // 二階導関数以上は常にゼロベクトル
    return result;
}
