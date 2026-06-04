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
#include "numerics/core/matrix.h"
#include "numerics/core/tolerance.h"
#include "numerics/core/constexpr_math.h"
#include "numerics/core/combinatorics.h"

// 数値解析 (数値積分、最適化・求根)
#include "numerics/analysis/integration.h"
#include "numerics/analysis/optimization.h"

// 幾何要素 (バウンディングボックス、多角形)
#include "numerics/geometric/bounding_box.h"
#include "numerics/geometric/polygon.h"

#endif  // IGESIO_NUMERICS_H_
