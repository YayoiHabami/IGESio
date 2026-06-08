/**
 * @file graphics/surfaces/surface_mesh.h
 * @brief 汎用曲面 (掃引系含む) の描画用三角形メッシュを生成する
 * @author Yayoi Habami
 * @date 2026-06-08
 * @copyright 2026 Yayoi Habami
 * @note GLに依存しない純粋な生成処理として切り出し、単体テスト可能にしている.
 *       ISurfaceGraphicsはここで生成したデータをGPUへ転送するだけ.
 */
#ifndef IGESIO_GRAPHICS_SURFACES_SURFACE_MESH_H_
#define IGESIO_GRAPHICS_SURFACES_SURFACE_MESH_H_

#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/entities/interfaces/i_surface.h"
#include "igesio/graphics/core/gl_types.h"



namespace igesio::graphics {

/// @brief 汎用曲面の三角形メッシュ
/// @note 頂点は位置・法線・テクスチャ座標を1列にまとめて保持する
struct GeneralSurfaceMesh {
    /// @brief 頂点データ (8行N列; 各列 {x, y, z, nx, ny, nz, u, v})
    MatrixXf vertices;
    /// @brief 三角形インデックス (3要素で1三角形; verticesの列番号)
    std::vector<gl::Uint> indices;
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
