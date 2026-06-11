/**
 * @file numerics.h
 * @brief numericsディレクトリで定義されるファイルをまとめるヘッダーファイル
 * @author Yayoi Habami
 * @date 2026-06-04
 * @copyright 2026 Yayoi Habami
 */
#ifndef IGESIO_NUMERICS_H_
#define IGESIO_NUMERICS_H_

// 基本要素 (行列・ベクトル、許容誤差、コンパイル時計算)
#include "igesio/numerics/core/matrix.h"
#include "igesio/numerics/core/tolerance.h"
#include "igesio/numerics/core/constexpr_math.h"
#include "igesio/numerics/core/combinatorics.h"

// 数値解析 (数値積分、最適化・求根)
#include "igesio/numerics/analysis/integration.h"
#include "igesio/numerics/analysis/optimization.h"

// 幾何要素 (バウンディングボックス、多角形)
#include "igesio/numerics/geometric/bounding_box.h"
#include "igesio/numerics/geometric/polygon.h"

// メッシュ (三角形メッシュと基本アルゴリズム)
#include "igesio/numerics/mesh/triangle_mesh.h"
#include "igesio/numerics/mesh/algorithms.h"

namespace igesio {

// 頻用型をigesio直下に昇格させる別名
// (個別ヘッダのみをincludeした場合は従来どおりigesio::numerics::で参照する)
using numerics::BoundingBox;

}  // namespace igesio

#endif  // IGESIO_NUMERICS_H_
