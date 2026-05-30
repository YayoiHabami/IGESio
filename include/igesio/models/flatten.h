/**
 * @file models/flatten.h
 * @brief Assemblyの配置を永続化(フラット化)するための関数
 * @author Yayoi Habami
 * @date 2026-05-28
 * @copyright 2026 Yayoi Habami
 * @note Assembly・ビューはIGESファイルへ出力しない(非永続)ため、配置を永続化する場合は
 *       大域変換をエンティティのDE変換(124)へ畳み込み、フラットなエンティティ集合へ変換する.
 */
#ifndef IGESIO_MODELS_FLATTEN_H_
#define IGESIO_MODELS_FLATTEN_H_

#include <memory>

#include "igesio/numerics/matrix.h"
#include "igesio/entities/entity_base.h"
#include "igesio/entities/transformations/transformation_matrix.h"
#include "igesio/models/iges_data.h"



namespace igesio::models {

/// @brief Materializeの結果
struct MaterializeResult {
    /// @brief 配置を畳み込んだ複製エンティティ(新しいID)
    std::shared_ptr<entities::EntityBase> entity;
    /// @brief 新たに生成した変換行列(124). placementが単位行列の場合はnullptr
    /// @note entityがこれを参照する. 出力時はentityと共にフラットマップへ加える必要がある
    std::shared_ptr<entities::TransformationMatrix> transform;
};

/// @brief エンティティを複製し、配置をDE変換(124)へ畳み込む
/// @param entity 複製元のエンティティ
/// @param placement 畳み込む配置行列(M_entityに前から掛ける累積変換)
/// @return 複製エンティティと、生成した124(placementが単位行列の場合はnullptr)
/// @note 複製は`entities::CloneEntity`による浅い複製(参照は元と共有). placementが単位行列で
///       ない場合、新124 = placement·M_entity を生成して複製の変換を差し替える(§12.3:
///       既存124があれば合成、なければ新規). スケールなし(124相当)を前提とする.
MaterializeResult Materialize(const entities::EntityBase& entity,
                              const Matrix4d& placement);

/// @brief Assembly階層の配置を畳み込み、フラットなIgesDataを生成する
/// @param src フラット化対象のIgesData
/// @return 各独立エンティティに所有Assemblyのワールド配置を畳み込んだ、子Assemblyを持たない
///         フラットなIgesData. description/global_sectionは複製する
/// @note 独立(非物理従属)エンティティのみMaterializeし、物理従属の子は親の畳み込んだ124を
///       介して追従する. 被参照の子・色定義等は複製元を共有してフラットマップへ集約する.
///       スケールなし(124相当)を前提とする.
IgesData Flatten(const IgesData& src);

}  // namespace igesio::models

#endif  // IGESIO_MODELS_FLATTEN_H_
