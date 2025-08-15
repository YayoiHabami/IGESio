/**
 * @file entities/structures/unsupported_entity.cpp
 * @brief UnsupportedEntityクラスの定義
 * @author Yayoi Habami
 * @date 2025-08-14
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/structures/unsupported_entity.h"

namespace {

using UnsupportedEntity = igesio::entities::UnsupportedEntity;

}  // namespace



/**
 * コンストラクタ
 */

UnsupportedEntity::UnsupportedEntity()
    : UnsupportedEntity(EntityType::kNull, {}, {}) {}

UnsupportedEntity::UnsupportedEntity(const EntityType entity_type,
                                     const IGESParameterVector parameters,
                                     const pointer2ID& de2id)
    : UnsupportedEntity(RawEntityDE::ByDefault(entity_type),
                        parameters, de2id) {}

UnsupportedEntity::UnsupportedEntity(const RawEntityDE& de_record,
                                     const IGESParameterVector& parameters,
                                     const pointer2ID& de2id,
                                     const uint64_t iges_id)
        : EntityBase(de_record, parameters, de2id, iges_id) {
    InitializePD(de2id);
}



/**
 * EntityBase implementation
 */

igesio::IGESParameterVector UnsupportedEntity::GetMainPDParameters() const {
    return pd_parameters_;
}

size_t UnsupportedEntity::SetMainPDParameters(const pointer2ID& de2id) {
    // メインのパラメータと追加ポインタを区別せず、全てのパラメータを
    // メインパラメータとして扱う
    return pd_parameters_.size();
}
