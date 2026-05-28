/**
 * @file models/assembly.cpp
 * @brief エンティティの集合を表すアセンブリクラス
 * @author Yayoi Habami
 * @date 2026-05-28
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/models/assembly.h"

#include <memory>
#include <unordered_set>
#include <vector>

namespace {

namespace i_models = igesio::models;
using Assembly = i_models::Assembly;

}  // namespace



/**
 * 内部ヘルパ (ルート探索・逆引きインデックス)
 */

Assembly* Assembly::RootRaw() {
    Assembly* node = this;
    while (auto parent = node->parent_.lock()) {
        node = parent.get();
    }
    return node;
}

const Assembly* Assembly::RootRaw() const {
    const Assembly* node = this;
    while (auto parent = node->parent_.lock()) {
        node = parent.get();
    }
    return node;
}

void Assembly::RegisterInIndex(const ObjectID& id, Assembly* owner) {
    RootRaw()->entity_index_[id] = owner;
}

void Assembly::ReindexInto(Assembly* root) {
    for (const auto& [id, entity] : entities_) {
        root->entity_index_[id] = this;
    }
    for (const auto& child : children_) {
        child->ReindexInto(root);
    }
}



/**
 * エンティティの管理 (IgesDataから移設. 対象はこのノードのentities_のみ)
 */

void Assembly::SetPointerIfUnset(std::shared_ptr<entities::EntityBase> entity) {
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

bool Assembly::AreAllReferencesSet() const {
    auto unresolved = GetUnresolvedReferences();
    return unresolved.empty();
}

std::unordered_set<igesio::ObjectID>
Assembly::GetUnresolvedReferences() const {
    std::unordered_set<ObjectID> unresolved_ids;

    for (const auto& [id, entity] : entities_) {
        auto unset = entity->GetUnresolvedReferences();
        unresolved_ids.insert(unset.begin(), unset.end());

        // entityは保持するが、このノードに登録されていないポインタがないか確認
        for (const auto& ptr_id : entity->GetReferencedEntityIDs()) {
            if (entities_.find(ptr_id) == entities_.end()) {
                // このノードに登録されていないポインタ
                unresolved_ids.insert(ptr_id);
            }
        }
    }
    return unresolved_ids;
}

std::shared_ptr<igesio::entities::EntityBase>
Assembly::GetEntity(const ObjectID& id) const {
    auto it = entities_.find(id);
    if (it != entities_.end()) {
        return it->second;
    }
    return nullptr;  // 存在しない場合はnullptrを返す
}

bool Assembly::IsReady() const {
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

igesio::ValidationResult Assembly::Validate() const {
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



/**
 * ツリー・変換
 */

void Assembly::AddChildAssembly(const std::shared_ptr<Assembly>& child) {
    if (!child) {
        throw std::invalid_argument("Child assembly pointer is null");
    }

    // 親を自身に設定し、子として登録する
    child->parent_ = weak_from_this();
    children_.push_back(child);

    // 子とその子孫のエンティティをルートの逆引きインデックスへ登録する
    child->ReindexInto(RootRaw());
}

std::shared_ptr<Assembly> Assembly::Root() {
    auto self = shared_from_this();
    while (auto parent = self->parent_.lock()) {
        self = parent;
    }
    return self;
}



/**
 * 子要素のクエリ
 */

Assembly* Assembly::FindOwner(const ObjectID& id) const {
    const Assembly* root = RootRaw();
    auto it = root->entity_index_.find(id);
    if (it != root->entity_index_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<igesio::ObjectID>
Assembly::GetEntityIDs(const bool recursive) const {
    std::vector<ObjectID> ids;
    ids.reserve(entities_.size());
    for (const auto& [id, entity] : entities_) {
        ids.push_back(id);
    }
    if (recursive) {
        for (const auto& child : children_) {
            auto child_ids = child->GetEntityIDs(true);
            ids.insert(ids.end(), child_ids.begin(), child_ids.end());
        }
    }
    return ids;
}

std::vector<std::shared_ptr<igesio::entities::EntityBase>>
Assembly::FindEntities(
        const std::function<bool(const entities::EntityBase&)>& predicate,
        const bool recursive) const {
    std::vector<std::shared_ptr<entities::EntityBase>> result;
    for (const auto& [id, entity] : entities_) {
        if (predicate(*entity)) {
            result.push_back(entity);
        }
    }
    if (recursive) {
        for (const auto& child : children_) {
            auto sub = child->FindEntities(predicate, true);
            result.insert(result.end(), sub.begin(), sub.end());
        }
    }
    return result;
}

std::vector<std::shared_ptr<igesio::entities::EntityBase>>
Assembly::FindEntitiesByType(const entities::EntityType type,
                             const bool recursive) const {
    return FindEntities([type](const entities::EntityBase& entity) {
        return entity.GetType() == type;
    }, recursive);
}

std::vector<std::shared_ptr<igesio::entities::EntityBase>>
Assembly::FindEntitiesByUseFlag(const entities::EntityUseFlag flag,
                                const bool recursive) const {
    return FindEntities([flag](const entities::EntityBase& entity) {
        return entity.GetEntityUseFlag() == flag;
    }, recursive);
}
