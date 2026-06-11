/**
 * @file numerics/mesh/algorithms.h
 * @brief 三角形メッシュ (TriangleMeshT) に対する基本アルゴリズムの集約ヘッダ
 * @author Yayoi Habami
 * @date 2026-06-10
 * @copyright 2026 Yayoi Habami
 * @note 関数指向で提供する. 検査 (inspection)・法線 (normals)・エッジ (edges)・
 *       変換 (conversion)・交差判定 (mesh_line_intersection) の各サブヘッダを
 *       束ねる. メッシュの溶接・簡略化等の追加アルゴリズムも将来は本階層
 *       (algorithms/) へ置く.
 */
#ifndef IGESIO_NUMERICS_MESH_ALGORITHMS_H_
#define IGESIO_NUMERICS_MESH_ALGORITHMS_H_

#include "igesio/numerics/mesh/algorithms/inspection.h"
#include "igesio/numerics/mesh/algorithms/normals.h"
#include "igesio/numerics/mesh/algorithms/edges.h"
#include "igesio/numerics/mesh/algorithms/conversion.h"
#include "igesio/numerics/mesh/algorithms/mesh_line_intersection.h"

#endif  // IGESIO_NUMERICS_MESH_ALGORITHMS_H_
