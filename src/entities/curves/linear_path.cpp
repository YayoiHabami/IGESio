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
using i_ent::CopiousDataType;
using igesio::Vector3d;

/// @brief 各頂点における累積弧長を計算する
/// @param base CopiousDataBaseの参照
/// @return 頂点ごとの累積弧長 (サイズ n, 先頭は 0.0)
std::vector<double>
ComputeVertexLengths(const i_ent::CopiousDataBase& base) {
    const size_t n = base.GetCount();
    std::vector<double> lengths(n, 0.0);
    for (size_t i = 1; i < n; ++i) {
        lengths[i] = lengths[i - 1]
            + (base.Coordinate(i) - base.Coordinate(i - 1)).norm();
    }
    return lengths;
}

/// @brief 弧長パラメータ t に対応する頂点インデックスを返す
/// @param vertex_lengths ComputeVertexLengths の結果
/// @param t パラメータ値
/// @param eps 許容誤差
/// @return 一致する頂点インデックス. 見つからない場合は std::nullopt
std::optional<size_t>
FindVertexIndex(const std::vector<double>& vertex_lengths,
                const double t, const double eps = 1e-9) {
    for (size_t i = 0; i < vertex_lengths.size(); ++i) {
        if (std::abs(vertex_lengths[i] - t) < eps) return i;
    }
    return std::nullopt;
}

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
                          Matrix3Xd::Zero(3, coordinates.size())) {
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
        coordinates_.block<3, 1>(0, i) = coordinates[i];
    }
}



/**
 * 直線部・角点サポート
 */

std::vector<std::array<double, 2>> LinearPath::GetLinearSegments() const {
    const size_t n = GetCount();
    if (n < 2) return {};

    const bool is_loop = (GetDataType() == CopiousDataType::kPlanarLoop);
    const auto vertex_lengths = ComputeVertexLengths(*this);

    std::vector<std::array<double, 2>> segments;
    segments.reserve(is_loop ? n : n - 1);

    for (size_t i = 0; i + 1 < n; ++i) {
        segments.push_back({vertex_lengths[i], vertex_lengths[i + 1]});
    }
    if (is_loop) {
        // 末端→先頭の閉鎖辺
        segments.push_back({vertex_lengths[n - 1], CopiousDataBase::Length()});
    }
    return segments;
}

std::vector<double> LinearPath::GetCornerParams() const {
    const size_t n = GetCount();
    if (n < 2) return {};

    const bool is_loop = (GetDataType() == CopiousDataType::kPlanarLoop);
    const auto vertex_lengths = ComputeVertexLengths(*this);

    std::vector<double> corners;
    if (is_loop) {
        // 頂点0 (t=0) も閉鎖辺との接続点なので角点
        corners.push_back(0.0);
        for (size_t i = 1; i < n; ++i) {
            corners.push_back(vertex_lengths[i]);
        }
    } else {
        // 内部頂点のみ (始点・終点を除く)
        for (size_t i = 1; i + 1 < n; ++i) {
            corners.push_back(vertex_lengths[i]);
        }
    }
    return corners;
}

std::optional<Vector3d> LinearPath::LeftTangentAt(const double t) const {
    if (!IsCorner(t)) return ICurve::LeftTangentAt(t);

    const size_t n = GetCount();
    if (n < 2) return std::nullopt;

    const bool is_loop = (GetDataType() == CopiousDataType::kPlanarLoop);
    const auto vertex_lengths = ComputeVertexLengths(*this);

    const auto idx_opt = FindVertexIndex(vertex_lengths, t);
    if (!idx_opt.has_value()) return ICurve::LeftTangentAt(t);
    const size_t idx = *idx_opt;

    // 入射辺: 前の頂点 → 現在の頂点
    Vector3d prev;
    if (idx == 0) {
        if (!is_loop) return std::nullopt;  // 非loop始点は左接線なし
        prev = Coordinate(n - 1);
    } else {
        prev = Coordinate(idx - 1);
    }
    const Vector3d curr = Coordinate(idx);
    const double seg_len = (curr - prev).norm();
    if (seg_len < 1e-12) return std::nullopt;
    return (curr - prev) / seg_len;
}

std::optional<Vector3d> LinearPath::RightTangentAt(const double t) const {
    if (!IsCorner(t)) return ICurve::RightTangentAt(t);

    const size_t n = GetCount();
    if (n < 2) return std::nullopt;

    const bool is_loop = (GetDataType() == CopiousDataType::kPlanarLoop);
    const auto vertex_lengths = ComputeVertexLengths(*this);

    const auto idx_opt = FindVertexIndex(vertex_lengths, t);
    if (!idx_opt.has_value()) return ICurve::RightTangentAt(t);
    const size_t idx = *idx_opt;

    // 出射辺: 現在の頂点 → 次の頂点
    const Vector3d curr = Coordinate(idx);
    Vector3d next;
    if (idx == n - 1) {
        if (!is_loop) return std::nullopt;  // 非loop終点は右接線なし
        next = Coordinate(0);
    } else {
        next = Coordinate(idx + 1);
    }
    const double seg_len = (next - curr).norm();
    if (seg_len < 1e-12) return std::nullopt;
    return (next - curr) / seg_len;
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
    return {0.0, CopiousDataBase::Length()};
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
    if ((*idx) < GetCount() - 1) {
        // セグメントが始点から終点までの間にある場合
        result[1] = (Coordinate(*idx + 1) - Coordinate(*idx)).normalized();
    } else if ((*idx) == GetCount() - 1) {
        // セグメントが終点と始点を結ぶ線分（kPlanarLoopの場合）にある場合
        result[1] = (Coordinate(0) - Coordinate(*idx)).normalized();
    } else {
        // エラー
        return std::nullopt;
    }

    // 二階導関数以上は常にゼロベクトル
    return result;
}

double LinearPath::Length(const double start, const double end) const {
    // パラメータの妥当性確認
    if (start >= end) {
        throw std::invalid_argument(
            "Invalid parameters: start must be less than end.");
    }
    auto [t_start, t_end] = GetParameterRange();
    if (start < t_start || end > t_end) {
        throw std::invalid_argument("Parameters start=" + std::to_string(start)
            + ", end=" + std::to_string(end) + " out of range: [" +
            std::to_string(t_start) + ", " + std::to_string(t_end) + "].");
    } else if (numerics::IsApproxEqual(start, t_start) &&
               numerics::IsApproxEqual(end, t_end)) {
        // 完全一致
        return CopiousDataBase::Length();
    }

    // 完全一致ではない場合は、各セグメントの長さを足し合わせて計算
    auto start_index = GetSegmentIndexAt(start);
    auto end_index = GetSegmentIndexAt(end);
    if (!start_index.has_value() || !end_index.has_value()) {
        throw std::invalid_argument("Parameters start=" + std::to_string(start)
            + ", end=" + std::to_string(end) + " out of index range: [" +
            std::to_string(t_start) + ", " + std::to_string(t_end) + "].");
    }
    // 対応するセグメントが存在していれば、セグメントの長さはend-startに相当
    return end - start;
}
