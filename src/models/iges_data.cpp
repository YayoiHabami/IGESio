/**
 * @file models/iges_data.cpp
 * @brief 一つのIGESファイルに対応するデータ構造を管理するクラス
 * @author Yayoi Habami
 * @date 2025-08-02
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/models/iges_data.h"

#include <memory>
#include <unordered_set>

#include "igesio/entities/factory.h"

namespace {

namespace i_models = igesio::models;
using IgesData = i_models::IgesData;

}  // namespace



i_models::GlobalParam i_models::GetDefaultGlobalParam() {
    GlobalParam param = {};
    param.units_flag = UnitFlag::kMillimeter;
    param.max_line_weight = 1.0;
    param.min_resolution = 0.001;
    return param;
}



void IgesData::SetPointerIfUnset(std::shared_ptr<entities::EntityBase> entity) {
    // nullptrチェック
    if (!entity) return;

    // entityが参照するエンティティのうち、ポインタが未設定のもののIDを取得
    for (auto& id : entity->GetUnresolvedReferences()) {
        auto it = entities_.find(id);
        if (it != entities_.end()) {
            // その未設定のエンティティを自身が持っている場合は、
            // entityにそのエンティティのポインタを設定する
            entity->SetUnresolvedReference(it->second);
        }
    }

    // entities_に登録されている各要素について、entityを参照していて
    // entityへのポインタが未設定の場合は設定
    for (const auto& [id, ent] : entities_) {
        for (auto& ptr : ent->GetUnresolvedReferences()) {
            if (ptr == entity->GetID()) {
                // entityへの参照ポインタを設定
                ent->SetUnresolvedReference(entity);
            }
        }
    }
}

bool IgesData::AreAllReferencesSet() const {
    auto unresolved = GetUnresolvedReferences();
    return unresolved.empty();
}

std::unordered_set<igesio::ObjectID>
IgesData::GetUnresolvedReferences() const {
    std::unordered_set<ObjectID> unresolved_ids;

    for (const auto& [id, entity] : entities_) {
        auto unset = entity->GetUnresolvedReferences();
        unresolved_ids.insert(unset.begin(), unset.end());

        // entityは保持するが、IgesDataに登録されていないポインタがないか確認
        for (const auto& ptr_id : entity->GetReferencedEntityIDs()) {
            if (entities_.find(ptr_id) == entities_.end()) {
                // IgesDataに登録されていないポインタ
                unresolved_ids.insert(ptr_id);
            }
        }
    }
    return unresolved_ids;
}

std::shared_ptr<igesio::entities::EntityBase>
IgesData::GetEntity(const ObjectID& id) const {
    auto it = entities_.find(id);
    if (it != entities_.end()) {
        return it->second;
    }
    return nullptr;  // 存在しない場合はnullptrを返す
}

bool IgesData::IsReady() const {
    // すべての参照が設定されているか確認
    if (!AreAllReferencesSet()) {
        return false;
    }

    // すべてのエンティティが有効であることを確認
    for (const auto& [id, entity] : entities_) {
        if (!entity->IsValid()) {
            return false;  // 無効なエンティティがある場合は`false`
        }
    }
    return true;  // すべての条件を満たしている場合は`true`
}

igesio::ValidationResult IgesData::Validate() const {
    ValidationResult result;

    // 参照が未設定のエンティティがないか確認
    for (const auto& id : GetUnresolvedReferences()) {
        result.AddError(ValidationError(
            "Reference to the entity with ID " + ToString(id) +
            " is unresolved. Please add a pointer to this entity "
            "from `AddEntity` function."));
    }

    // すべてのエンティティの検証を実行
    for (const auto& [id, entity] : entities_) {
        auto validation = entity->Validate();
        if (!validation.is_valid) {
            result.Merge(validation);
        }
    }
    return result;
}
