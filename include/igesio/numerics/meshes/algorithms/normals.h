/**
 * @file numerics/meshes/algorithms/normals.h
 * @brief 三角形メッシュ (TriangleMeshT) の頂点法線・面法線を計算するアルゴリズム
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 */
#ifndef IGESIO_NUMERICS_MESHES_ALGORITHMS_NORMALS_H_
#define IGESIO_NUMERICS_MESHES_ALGORITHMS_NORMALS_H_

#include <cstddef>

#include "igesio/numerics/core/matrix.h"
#include "igesio/numerics/meshes/triangle_mesh.h"



namespace igesio::numerics {

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

/// @brief 各三角形の単位面法線を計算する
/// @param mesh 対象のメッシュ
/// @return 三角形毎の単位面法線 (3×三角形数; 各列が1三角形).
///         退化三角形 (外積がゼロ) はゼロベクトル
template <typename Scalar>
Eigen::Matrix<Scalar, 3, Eigen::Dynamic> ComputeFaceNormals(
        const TriangleMeshT<Scalar>& mesh) {
    using Vec3 = Eigen::Matrix<Scalar, 3, 1>;
    const auto triangle_count = mesh.TriangleCount();
    Eigen::Matrix<Scalar, 3, Eigen::Dynamic> normals;
    normals.setZero(3, static_cast<Eigen::Index>(triangle_count));

    for (std::size_t t = 0; t < triangle_count; ++t) {
        const Vec3 p0 = mesh.positions.col(mesh.indices[3 * t]);
        const Vec3 edge1 = Vec3(mesh.positions.col(mesh.indices[3 * t + 1])) - p0;
        const Vec3 edge2 = Vec3(mesh.positions.col(mesh.indices[3 * t + 2])) - p0;
        const Vec3 cross = edge1.cross(edge2);
        const Scalar norm = cross.norm();
        if (norm > Scalar(0)) {
            normals.col(static_cast<Eigen::Index>(t)) = cross / norm;
        }
    }
    return normals;
}

}  // namespace igesio::numerics

#endif  // IGESIO_NUMERICS_MESHES_ALGORITHMS_NORMALS_H_
