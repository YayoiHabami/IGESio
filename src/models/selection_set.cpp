/**
 * @file models/selection_set.cpp
 * @brief GUI非依存の選択状態を管理するクラスの実装
 * @author Yayoi Habami
 * @date 2026-05-29
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/models/selection_set.h"



namespace igesio::models {

void SelectionSet::Select(const ObjectID& id) {
    selected_.insert(id);
    active_ = id;
    ++version_;
}

void SelectionSet::Deselect(const ObjectID& id) {
    auto it = selected_.find(id);
    if (it == selected_.end()) return;
    selected_.erase(it);
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
    selected_.clear();
    selected_.insert(id);
    active_ = id;
    ++version_;
}

void SelectionSet::Clear() {
    if (selected_.empty() && !active_.has_value()) return;
    selected_.clear();
    active_ = std::nullopt;
    ++version_;
}

bool SelectionSet::Contains(const ObjectID& id) const {
    return selected_.find(id) != selected_.end();
}

}  // namespace igesio::models
