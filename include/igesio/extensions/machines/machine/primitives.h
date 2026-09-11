/**
 * @file extensions/machines/machine/primitives.h
 * @brief 機械定義用のプリミティブ形状の三角形メッシュ生成
 * @author Yayoi Habami
 * @date 2026-09-11
 * @copyright 2026 Yayoi Habami
 * @note 頂点法線は面法線とし、平面部 (直方体の各面・円柱の上下面) は頂点を共有しない
 *       (角が丸く陰影づかないようにする). 巻き方向は外向きに右手系で反時計回り.
 *       円柱の側面は半径方向の法線を持ち、上下のリング部で頂点を共有する
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_MACHINE_PRIMITIVES_H_
#define IGESIO_EXTENSIONS_MACHINES_MACHINE_PRIMITIVES_H_

#include "igesio/numerics/core/matrix.h"
#include "igesio/numerics/meshes/triangle_mesh.h"
#include "igesio/extensions/machines/machine/machine_definition.h"

namespace igesio::extensions::machines {

/// @brief 中心が原点で各辺が座標軸に平行な直方体のメッシュを作る
/// @param size 各軸方向の辺長 [mm]
/// @return 面ごとに4頂点 (24頂点・12三角形). 法線は面法線
/// @throw std::invalid_argument `size`の成分が正でない場合
numerics::TriangleMeshd MakeBoxMesh(const igesio::Vector3d& size);

/// @brief z軸平行で高さ方向の中心が原点にある、上下面つきの円柱のメッシュを作る
/// @param radius 半径 [mm]
/// @param height 高さ [mm] (z ∈ [-height / 2, height / 2])
/// @param segments 円周の分割数 (≥ 3)
/// @return 側面は2 × segments頂点 (法線は半径方向),
///         上下面は各segments+1頂点(法線は±z). 4 × segmentsの三角形 (外向き)
/// @throw std::invalid_argument `radius`・`height`が正でない場合,
///        または`segments < 3`の場合
numerics::TriangleMeshd MakeCylinderMesh(double radius, double height,
                                         int segments = 48);

/// @brief プリミティブ形状の指定からメッシュを作る
/// @param spec プリミティブ形状 (`kBox`→`MakeBoxMesh(size)`,
///        `kCylinder`→`MakeCylinderMesh(radius, height)`)
/// @throw std::invalid_argument 上記関数の例外
numerics::TriangleMeshd MakePrimitiveMesh(const PrimitiveSpec& spec);

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_MACHINE_PRIMITIVES_H_
