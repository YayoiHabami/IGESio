/**
 * @file numerics/mesh/algorithms.h
 * @brief 三角形メッシュ (TriangleMeshT) に対する基本アルゴリズム
 * @author Yayoi Habami
 * @date 2026-06-10
 * @copyright 2026 Yayoi Habami
 * @note 関数指向で提供する. メッシュの溶接・簡略化等の追加アルゴリズムも
 *       将来は本階層へ置く.
 */
#ifndef IGESIO_NUMERICS_MESH_ALGORITHMS_H_
#define IGESIO_NUMERICS_MESH_ALGORITHMS_H_

#include <cstdint>
#include <string>

#include "igesio/common/validation_result.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/numerics/geometric/bounding_box.h"
#include "igesio/numerics/mesh/triangle_mesh.h"



namespace igesio::numerics {

/// @brief メッシュの構造的な整合性を検証する
/// @param mesh 検証対象のメッシュ
/// @return 検証結果. 以下を検査する:
///         - インデックス数が3の倍数であること
///         - 全インデックスが頂点数未満であること
///         - 法線・UVチャンネルが存在する場合、列数が頂点数と一致すること
///         - 面グループの範囲が三角形数に収まること
template <typename Scalar>
ValidationResult Validate(const TriangleMeshT<Scalar>& mesh) {
    ValidationResult result;
    const auto vertex_count = mesh.VertexCount();

    if (mesh.indices.size() % 3 != 0) {
        result.AddError(ValidationError(
            "Triangle index count must be a multiple of 3 (got " +
            std::to_string(mesh.indices.size()) + ")"));
    }
    for (const auto index : mesh.indices) {
        if (index >= vertex_count) {
            result.AddError(ValidationError(
                "Triangle index " + std::to_string(index) +
                " is out of range (vertex count: " +
                std::to_string(vertex_count) + ")"));
            break;  // 大量の不正インデックスを逐一報告しない
        }
    }
    if (mesh.HasNormals() &&
        mesh.normals.cols() != mesh.positions.cols()) {
        result.AddError(ValidationError(
            "Normal channel size (" + std::to_string(mesh.normals.cols()) +
            ") does not match vertex count (" +
            std::to_string(mesh.positions.cols()) + ")"));
    }
    if (mesh.HasUVs() && mesh.uvs.cols() != mesh.positions.cols()) {
        result.AddError(ValidationError(
            "UV channel size (" + std::to_string(mesh.uvs.cols()) +
            ") does not match vertex count (" +
            std::to_string(mesh.positions.cols()) + ")"));
    }
    const auto triangle_count = mesh.TriangleCount();
    for (const auto& group : mesh.groups) {
        const auto end = static_cast<std::size_t>(group.first_triangle) +
                         group.triangle_count;
        if (end > triangle_count) {
            result.AddError(ValidationError(
                "Mesh group '" + group.name + "' range [" +
                std::to_string(group.first_triangle) + ", " +
                std::to_string(end) + ") exceeds triangle count (" +
                std::to_string(triangle_count) + ")"));
        }
    }
    return result;
}

/// @brief メッシュの軸平行バウンディングボックスを計算する
/// @param mesh 対象のメッシュ
/// @return 全頂点を包含する軸平行バウンディングボックス.
///         頂点が無い場合は空のBoundingBox
template <typename Scalar>
BoundingBox ComputeBoundingBox(const TriangleMeshT<Scalar>& mesh) {
    if (mesh.positions.cols() == 0) return BoundingBox();
    const Vector3d lo =
            mesh.positions.rowwise().minCoeff().template cast<double>();
    const Vector3d hi =
            mesh.positions.rowwise().maxCoeff().template cast<double>();
    return BoundingBox(lo, hi);
}

/// @brief 頂点法線を面積重み平均で再計算する
/// @param[in,out] mesh 対象のメッシュ (normalsチャンネルを上書きする)
/// @note 各三角形の外積 (大きさ=面積の2倍) を頂点へ加算して正規化する.
///       面積重みは頂点まわりの面の大きさに応じた自然な平均を与える.
///       退化三角形 (外積がゼロ) は寄与しない. どの面にも属さない頂点の
///       法線はゼロベクトルとなる
template <typename Scalar>
void RecomputeNormals(TriangleMeshT<Scalar>& mesh) {
    using Vec3 = Eigen::Matrix<Scalar, 3, 1>;
    mesh.normals.setZero(3, mesh.positions.cols());

    for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const auto i0 = mesh.indices[t];
        const auto i1 = mesh.indices[t + 1];
        const auto i2 = mesh.indices[t + 2];
        const Vec3 p0 = mesh.positions.col(i0);
        const Vec3 edge1 = Vec3(mesh.positions.col(i1)) - p0;
        const Vec3 edge2 = Vec3(mesh.positions.col(i2)) - p0;
        const Vec3 cross = edge1.cross(edge2);
        mesh.normals.col(i0) += cross;
        mesh.normals.col(i1) += cross;
        mesh.normals.col(i2) += cross;
    }

    for (Eigen::Index c = 0; c < mesh.normals.cols(); ++c) {
        const Scalar norm = mesh.normals.col(c).norm();
        if (norm > Scalar(0)) mesh.normals.col(c) /= norm;
    }
}

/// @brief メッシュのスカラー型を変換する
/// @tparam To 変換先のスカラー型
/// @param mesh 変換元のメッシュ
/// @return 変換後のメッシュ (インデックス・グループはそのままコピー)
template <typename To, typename From>
TriangleMeshT<To> CastScalar(const TriangleMeshT<From>& mesh) {
    TriangleMeshT<To> out;
    out.positions = mesh.positions.template cast<To>();
    out.normals = mesh.normals.template cast<To>();
    out.uvs = mesh.uvs.template cast<To>();
    out.indices = mesh.indices;
    out.groups = mesh.groups;
    return out;
}

}  // namespace igesio::numerics

#endif  // IGESIO_NUMERICS_MESH_ALGORITHMS_H_
