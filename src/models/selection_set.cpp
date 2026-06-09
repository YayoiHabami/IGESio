/**
 * @file models/selection_set.cpp
 * @brief GUI非依存の選択状態を管理するクラスの実装
 * @author Yayoi Habami
 * @date 2026-05-29
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/models/selection_set.h"

#include <algorithm>



namespace igesio::models {

void SelectionSet::Select(const ObjectID& id) {
    // 選択済みの場合は順序位置を維持し、activeのみ更新する
    if (lookup_.insert(id).second) items_.push_back(id);
    active_ = id;
    ++version_;
}

void SelectionSet::Deselect(const ObjectID& id) {
    auto it = lookup_.find(id);
    if (it == lookup_.end()) return;
    lookup_.erase(it);
    items_.erase(std::find(items_.begin(), items_.end(), id));
    if (active_.has_value() && *active_ == id) active_ = std::nullopt;
    ++version_;
}

void SelectionSet::Toggle(const ObjectID& id) {
    if (Contains(id)) {
        Deselect(id);
    } else {
        Select(id);
    }
}

void SelectionSet::Replace(const ObjectID& id) {
    items_.clear();
    lookup_.clear();
    items_.push_back(id);
    lookup_.insert(id);
    active_ = id;
    ++version_;
}

void SelectionSet::Clear() {
    if (items_.empty() && !active_.has_value()) return;
    items_.clear();
    lookup_.clear();
    active_ = std::nullopt;
    ++version_;
}

bool SelectionSet::Contains(const ObjectID& id) const {
    return lookup_.find(id) != lookup_.end();
}

}  // namespace igesio::models
