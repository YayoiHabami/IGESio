/**
 * @file numerics/mesh/algorithms/inspection.h
 * @brief 三角形メッシュ (TriangleMeshT) を検査して要約値を返すアルゴリズム
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 * @note メッシュを変更せず, 検証結果やバウンディングボックスを返す関数を置く.
 */
#ifndef IGESIO_NUMERICS_MESH_ALGORITHMS_INSPECTION_H_
#define IGESIO_NUMERICS_MESH_ALGORITHMS_INSPECTION_H_

#include <cstddef>
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

}  // namespace igesio::numerics

#endif  // IGESIO_NUMERICS_MESH_ALGORITHMS_INSPECTION_H_
