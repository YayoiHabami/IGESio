/**
 * @file graphics/surfaces/surface_mesh.h
 * @brief 汎用曲面 (掃引系含む) の描画用三角形メッシュを生成する
 * @author Yayoi Habami
 * @date 2026-06-08
 * @copyright 2026 Yayoi Habami
 * @note GLに依存しない純粋な生成処理として切り出し、単体テスト可能にしている.
 *       ISurfaceGraphicsはここで生成したメッシュをBuildInterleavedVerticesで
 *       整形してGPUへ転送するだけ.
 */
#ifndef IGESIO_GRAPHICS_SURFACES_SURFACE_MESH_H_
#define IGESIO_GRAPHICS_SURFACES_SURFACE_MESH_H_

#include "igesio/numerics/meshes/triangle_mesh.h"
#include "igesio/entities/interfaces/i_surface.h"



namespace igesio::graphics {

/// @brief 汎用曲面の三角形メッシュ (グリッド情報付き)
struct GeneralSurfaceMesh {
    /// @brief 三角形メッシュ (positions/normals/uvsの全チャンネルを持つ.
    ///        uvsはパラメータ範囲を [0,1] に正規化したテクスチャ座標)
    numerics::TriangleMeshf mesh;
    /// @brief uサンプル行数 (折れ目の二重化を含む実際の行数)
    int u_row_count = 0;
    /// @brief v方向分割数
    int v_div = 0;
};

/// @brief 汎用曲面 (掃引系含む) の三角形メッシュを生成する
/// @param surface 対象の曲面
/// @param u_div u方向の一様分割数
/// @param v_div v方向の一様分割数
/// @return 生成したメッシュデータ
/// @note u方向の折れ目 (ISurface::GetUCreaseParameters) の位置にはサンプル線を
///       挿入し、その行を法線片側評価で二重化して角を立てる (ハードエッジ化).
///       点は有効だが法線が定義できない退化点 (apex等) は隣接頂点の法線を借用し、
///       頂点を捨てない (穴の発生を防ぐ).
GeneralSurfaceMesh BuildGeneralSurfaceMesh(
        const entities::ISurface& surface, int u_div, int v_div);

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_SURFACES_SURFACE_MESH_H_
