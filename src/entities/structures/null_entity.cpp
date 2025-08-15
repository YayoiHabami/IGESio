/**
 * @file entities/structures/null_entity.cpp
 * @brief NullEntity (Type 0) クラスの定義
 * @author Yayoi Habami
 * @date 2025-08-02
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/structures/null_entity.h"

namespace {

using NullEntity = igesio::entities::NullEntity;

}  // namespace


/**
 * コンストラクタ
 */

NullEntity::NullEntity() : EntityBase(EntityType::kNull, {}) {
    InitializePD({});
}

NullEntity::NullEntity(const RawEntityDE& de_record,
                       const IGESParameterVector& parameters,
                       const pointer2ID& de2id,
                       const uint64_t iges_id)
        : EntityBase(de_record, parameters, de2id, iges_id) {
    InitializePD(de2id);
}


/**
 * EntityBase implementation
 */

igesio::IGESParameterVector NullEntity::GetMainPDParameters() const {
    return {};
}

size_t NullEntity::SetMainPDParameters(const pointer2ID& de2id) {
    // Nullエンティティはパラメータを持たないため、常に0を返す
    return 0;
}




