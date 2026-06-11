/**
 * @file graphics/core/mesh_staging.h
 * @brief 三角形メッシュ (TriangleMeshT) のGPU転送用ステージング整形
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 * @note GLに依存しない純粋な整形処理 (単体テスト可能)。SoAレイアウトの
 *       メッシュを、汎用曲面シェーダー (ShaderId::kGeneralSurface) の
 *       頂点レイアウトへinterleaveする。転送系の描画クラス
 *       (ISurfaceGraphics / RestrictedSurfaceGraphics / TriangleMeshGraphics)
 *       が共用する。
 */
#ifndef IGESIO_GRAPHICS_CORE_MESH_STAGING_H_
#define IGESIO_GRAPHICS_CORE_MESH_STAGING_H_

#include <cstddef>
#include <vector>

#include "igesio/numerics/mesh/triangle_mesh.h"



namespace igesio::graphics {

/// @brief メッシュをinterleaved頂点列 (頂点あたりfloat 8個) へ整形する
/// @tparam Scalar メッシュのスカラー型 (float / double)
/// @param mesh 整形対象のメッシュ
/// @return 各頂点 {x, y, z, nx, ny, nz, u, v} を連結したfloat列
/// @note 汎用曲面シェーダーの頂点属性レイアウト (位置3+法線3+UV2) に対応する。
///       法線・UVチャンネルが無い場合はゼロ埋めする (照明が必要なメッシュは
///       呼び出し側でnumerics::RecomputeNormals等により法線を補うこと)。
///       GPU境界のためここで単精度へ変換する (CPU正準値はdoubleのまま)
template <typename Scalar>
std::vector<float> BuildInterleavedVertices(
        const numerics::TriangleMeshT<Scalar>& mesh) {
    const auto vertex_count = mesh.VertexCount();
    const bool has_normals = mesh.HasNormals();
    const bool has_uvs = mesh.HasUVs();

    std::vector<float> interleaved(vertex_count * 8, 0.0f);
    for (std::size_t c = 0; c < vertex_count; ++c) {
        const auto col = static_cast<Eigen::Index>(c);
        float* v = interleaved.data() + c * 8;
        v[0] = static_cast<float>(mesh.positions(0, col));
        v[1] = static_cast<float>(mesh.positions(1, col));
        v[2] = static_cast<float>(mesh.positions(2, col));
        if (has_normals) {
            v[3] = static_cast<float>(mesh.normals(0, col));
            v[4] = static_cast<float>(mesh.normals(1, col));
            v[5] = static_cast<float>(mesh.normals(2, col));
        }
        if (has_uvs) {
            v[6] = static_cast<float>(mesh.uvs(0, col));
            v[7] = static_cast<float>(mesh.uvs(1, col));
        }
    }
    return interleaved;
}

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CORE_MESH_STAGING_H_
