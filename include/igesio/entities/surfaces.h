/**
 * @file entities/surfaces.h
 * @brief 全ての曲面エンティティをまとめるヘッダーファイル
 * @author Yayoi Habami
 * @date 2026-06-04
 * @copyright 2026 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_SURFACES_H_
#define IGESIO_ENTITIES_SURFACES_H_

#include "igesio/entities/surfaces/plane.h"                  // Type 108
#include "igesio/entities/surfaces/ruled_surface.h"          // Type 118
#include "igesio/entities/surfaces/surface_of_revolution.h"  // Type 120
#include "igesio/entities/surfaces/tabulated_cylinder.h"     // Type 122
#include "igesio/entities/surfaces/rational_b_spline_surface.h"  // Type 128
#include "igesio/entities/surfaces/trimmed_surface.h"        // Type 144

namespace igesio {

// 頻用型をigesio直下に昇格させる別名
// (個別ヘッダのみをincludeした場合は従来どおりigesio::entities::で参照する)
using entities::ISurface;
using entities::IRestrictedSurface;

}  // namespace igesio

#endif  // IGESIO_ENTITIES_SURFACES_H_
