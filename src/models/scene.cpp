/**
 * @file models/scene.cpp
 * @brief Scene クラスの実装
 * @author Yayoi Habami
 * @date 2026-05-30
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/models/scene.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>



namespace igesio::models {

Scene::Scene(std::shared_ptr<Assembly> root)
    : root_(std::move(root)),
      active_selection_(IDGenerator::Generate(ObjectType::kAssembly)) {
    // 既定の選択セットを1つ用意し、アクティブにする
    selections_.emplace(active_selection_, SelectionSet{});
}

Assembly& Scene::Root() { return *root_; }

const Assembly& Scene::Root() const { return *root_; }

std::shared_ptr<Assembly> Scene::RootPtr() const { return root_; }

SelectionSet& Scene::ActiveSelection() {
    return selections_.at(active_selection_);
}

const SelectionSet& Scene::ActiveSelection() const {
    return selections_.at(active_selection_);
}

ObjectID Scene::ActiveSelectionId() const { return active_selection_; }

ObjectID Scene::CreateSelectionSet() {
    const ObjectID handle = IDGenerator::Generate(ObjectType::kAssembly);
    selections_.emplace(handle, SelectionSet{});
    return handle;
}

bool Scene::ActivateSelectionSet(const ObjectID& id) {
    if (selections_.find(id) == selections_.end()) return false;
    active_selection_ = id;
    return true;
}

std::vector<ObjectID> Scene::SelectionSetIds() const {
    std::vector<ObjectID> ids;
    ids.reserve(selections_.size());
    for (const auto& [handle, set] : selections_) ids.push_back(handle);
    return ids;
}

bool Scene::TrySelectWithLock(SelectionSet& set, const ObjectID& id) {
    // v1: エンティティ(body)単位の選択のみ. bodiesフィルタが無効なら拒否
    if (!pick_filter_.bodies) return false;
    // ロックされたサブツリーの要素は対話選択しない
    if (root_ && !root_->IsEffectivelySelectable(id)) return false;
    set.Select(id);
    return true;
}

PickFilter& Scene::Filter() { return pick_filter_; }

const PickFilter& Scene::Filter() const { return pick_filter_; }

std::optional<ObjectID> Scene::ActiveContext() const { return active_context_; }

}  // namespace igesio::models
