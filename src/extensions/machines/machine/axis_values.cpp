/**
 * @file extensions/machines/machine/axis_values.cpp
 * @brief 機械の軸の値の表現 (軸変位量ベクトル・NC指令値)
 * @author Yayoi Habami
 * @date 2026-09-10
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/machines/machine/axis_values.h"

#include <cstddef>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace igesio::extensions::machines {

JointVector::JointVector(const std::size_t axis_count, const double value)
    : values_(axis_count, value) {}

JointVector::JointVector(std::vector<double> values)
    : values_(std::move(values)) {}



NcValues::NcValues(const std::initializer_list<NcEntry> entries) {
    for (const NcEntry& entry : entries) Set(entry.register_name, entry.value);
}

std::optional<std::size_t> NcValues::IndexOf(const std::string_view register_name) const {
    // 軸数は高々十数本なので線形探索で十分
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].register_name == register_name) return i;
    }
    return std::nullopt;
}

std::optional<double> NcValues::Get(const std::string_view register_name) const {
    const std::optional<std::size_t> index = IndexOf(register_name);
    if (!index.has_value()) return std::nullopt;
    return entries_[*index].value;
}

double NcValues::GetOr(const std::string_view register_name, const double fallback) const {
    return Get(register_name).value_or(fallback);
}

double NcValues::At(const std::string_view register_name) const {
    const std::optional<std::size_t> index = IndexOf(register_name);
    if (!index.has_value()) {
        throw std::out_of_range("NcValues: unknown register: "
                                + std::string(register_name));
    }
    return entries_[*index].value;
}

bool NcValues::Contains(const std::string_view register_name) const {
    return IndexOf(register_name).has_value();
}

void NcValues::Set(std::string register_name, const double value) {
    const std::optional<std::size_t> index = IndexOf(register_name);
    if (index.has_value()) {
        entries_[*index].value = value;
        return;
    }
    entries_.push_back(NcEntry{std::move(register_name), value});
}

void NcValues::Merge(const NcValues& overlay) {
    for (const NcEntry& entry : overlay.entries_) Set(entry.register_name, entry.value);
}

bool operator==(const NcValues& lhs, const NcValues& rhs) {
    // 軸名は重複しないため、要素数が等しく片側が他方に含まれれば集合として一致する
    if (lhs.Size() != rhs.Size()) return false;
    for (const NcEntry& entry : lhs.Entries()) {
        const std::optional<double> other = rhs.Get(entry.register_name);
        if (!other.has_value() || *other != entry.value) return false;
    }
    return true;
}

}  // namespace igesio::extensions::machines
