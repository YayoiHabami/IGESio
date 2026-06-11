/**
 * @file numerics/mesh/algorithms/conversion.h
 * @brief 三角形メッシュ (TriangleMeshT) のスカラー型を変換するアルゴリズム
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 */
#ifndef IGESIO_NUMERICS_MESH_ALGORITHMS_CONVERSION_H_
#define IGESIO_NUMERICS_MESH_ALGORITHMS_CONVERSION_H_

#include "igesio/numerics/mesh/triangle_mesh.h"



namespace igesio::numerics {

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

#endif  // IGESIO_NUMERICS_MESH_ALGORITHMS_CONVERSION_H_
