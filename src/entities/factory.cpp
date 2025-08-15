/**
 * @file entities/factory.cpp
 * @brief 各エンティティクラスのファクトリクラス
 * @author Yayoi Habami
 * @date 2025-08-02
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/factory.h"

#include <memory>

#include "igesio/entities/structures/unsupported_entity.h"

#include "igesio/entities/structures/null_entity.h"     // type 000
#include "igesio/entities/curves/circular_arc.h"        // type 100



namespace {

using IVec = igesio::IGESParameterVector;
using ET = igesio::entities::EntityType;
using DE = igesio::entities::RawEntityDE;
using p2I = igesio::entities::pointer2ID;
namespace i_ent = igesio::entities;

}  // namespace



void i_ent::EntityFactory::Initialize() {
    if (initialized_) return;

    // エンティティタイプと作成関数のマッピングを設定
    // 0 - Null Entity
    creators_[ET::kNull] = [](const DE& de, const IVec& p,
                              const p2I& d2i, const uint64_t iid) {
        return std::make_shared<i_ent::NullEntity>(de, p, d2i, iid);
    };
    // 100 - Circular Arc
    creators_[ET::kCircularArc] = [](const DE& de, const IVec& p,
                                     const p2I& d2i, const uint64_t iid) {
        return std::make_shared<i_ent::CircularArc>(de, p, d2i, iid);
    };

    initialized_ = true;
}



std::shared_ptr<i_ent::EntityBase> i_ent::EntityFactory::CreateEntity(
        const RawEntityDE& de, const IVec& parameters,
        const pointer2ID& de2id, const uint64_t iges_id) {
    Initialize();  // 初期化

    auto it = creators_.find(de.entity_type);
    if (it != creators_.end()) {
        return it->second(de, parameters, de2id, iges_id);
    }

    // 対応するエンティティが存在しない場合はUnsupportedEntityを返す
    return std::make_shared<i_ent::UnsupportedEntity>(de, parameters, de2id);
}

std::shared_ptr<i_ent::EntityBase> i_ent::EntityFactory::CreateEntity(
        const RawEntityDE& de, const RawEntityPD& pd,
        const pointer2ID& de2id, const uint64_t iges_id) {
    return CreateEntity(de, ToIGESParameterVector(pd), de2id, iges_id);
}
