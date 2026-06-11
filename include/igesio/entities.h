/**
 * @file entities.h
 * @brief entitiesディレクトリで定義されるファイルをまとめるヘッダーファイル
 * @author Yayoi Habami
 * @date 2025-06-08
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_H_
#define IGESIO_ENTITIES_H_

// エンティティの型
#include "igesio/entities/entity_type.h"

// 中間生成物
#include "igesio/entities/de.h"
#include "igesio/entities/pd.h"

// インターフェース・抽象クラス
#include "igesio/entities/interfaces.h"
#include "igesio/entities/entity_base.h"
#include "igesio/entities/non_iges_entity_base.h"

// 非IGESエンティティ (計算・描画専用)
#include "igesio/entities/meshes/mesh_entity.h"

// 具象エンティティ (カテゴリ別集約)
#include "igesio/entities/curves.h"
#include "igesio/entities/surfaces.h"
#include "igesio/entities/structures.h"
#include "igesio/entities/transformations/transformation_matrix.h"  // Type 124

// ビュー (Assembly文脈で変換適用済みの曲線・曲面を参照する)
#include "igesio/entities/views/curve_view.h"
#include "igesio/entities/views/surface_view.h"

// ファクトリ (RawEntityPD→型付きエンティティ変換)
#include "igesio/entities/factory.h"

namespace igesio {

// 頻用型をigesio直下に昇格させる別名
// (個別ヘッダのみをincludeした場合は従来どおりigesio::entities::で参照する)
// (ICurve/ISurface系はcurves.h/surfaces.h側で昇格済み)
using entities::EntityBase;
using entities::EntityType;

}  // namespace igesio

#endif  // IGESIO_ENTITIES_H_
