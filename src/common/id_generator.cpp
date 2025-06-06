/**
 * @file common/id_generator.cpp
 * @brief IDを生成するためのクラスを定義する
 * @author Yayoi Habami
 * @date 2025-06-02
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/common/id_generator.h"

#include <utility>



std::size_t igesio::PairHash::operator()(
        const std::pair<uint64_t, unsigned int>& p) const {
    auto h1 = std::hash<uint64_t>{}(p.first);
    auto h2 = std::hash<unsigned int>{}(p.second);
    return h1 ^ (h2 << 1);
}

uint64_t igesio::IDGenerator::Reserve(
        const uint64_t iges_id,
        const unsigned int pd_pointer) {
    std::lock_guard<std::mutex> lock(reserved_ids_mutex_);
    auto key = std::make_pair(iges_id, pd_pointer);
    auto it = reserved_ids_.find(key);
    if (it != reserved_ids_.end()) {
        // すでに予約されている場合は、そのIDを返す
        return it->second;
    }

    // 予約されていない場合は、新しいIDを生成
    uint64_t reserved_id = ++counter;
    reserved_ids_[key] = reserved_id;
    return reserved_id;
}

uint64_t igesio::IDGenerator::GetReservedID(
        const uint64_t iges_id,
        const unsigned int pd_pointer) {
    std::lock_guard<std::mutex> lock(reserved_ids_mutex_);
    auto key = std::make_pair(iges_id, pd_pointer);
    auto it = reserved_ids_.find(key);
    if (it == reserved_ids_.end()) {
        throw std::out_of_range(
            "ID not reserved for the given IGES ID (" + std::to_string(iges_id) +
            ") and PD pointer (" + std::to_string(pd_pointer) + ")");
    }
    return it->second;
}
