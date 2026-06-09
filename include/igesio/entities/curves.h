/**
 * @file entities/curves.h
 * @brief 全ての曲線エンティティをまとめるヘッダーファイル
 * @author Yayoi Habami
 * @date 2026-06-04
 * @copyright 2026 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_CURVES_H_
#define IGESIO_ENTITIES_CURVES_H_

#include "igesio/entities/curves/circular_arc.h"         // Type 100
#include "igesio/entities/curves/composite_curve.h"      // Type 102
#include "igesio/entities/curves/conic_arc.h"            // Type 104
#include "igesio/entities/curves/copious_data.h"         // Type 106
#include "igesio/entities/curves/linear_path.h"          // Type 106 (Form 11-13)
#include "igesio/entities/curves/line.h"                 // Type 110
#include "igesio/entities/curves/parametric_spline_curve.h"  // Type 112
#include "igesio/entities/curves/point.h"                    // Type 116
#include "igesio/entities/curves/rational_b_spline_curve.h"  // Type 126
#include "igesio/entities/curves/curve_on_a_parametric_surface.h"  // Type 142

namespace igesio {

// 頻用型をigesio直下に昇格させる別名
// (個別ヘッダのみをincludeした場合は従来どおりigesio::entities::で参照する)
using entities::ICurve;
using entities::ICurve2D;
using entities::ICurve3D;

}  // namespace igesio

#endif  // IGESIO_ENTITIES_CURVES_H_
