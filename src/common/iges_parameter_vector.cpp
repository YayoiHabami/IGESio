/**
 * @file common/iges_parameter_vector.cpp
 * @brief IGESのパラメータを混合して保持するためのクラス
 * @author Yayoi Habami
 * @date 2025-05-31
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/common/iges_parameter_vector.h"

#include <string>

namespace {

using IPVec = igesio::IGESParameterVector;

}  // namespace



void IPVec::resize(const size_t new_size) {
    resize(new_size, 0, DefaultValueFormat<int>());
}

void IPVec::resize(const size_t new_size,
                   const igesio::VecParamType& default_value) {
    // default_valueの型に応じたValueFormatを取得
    auto type = static_cast<igesio::CppParameterType>(default_value.index());
    resize(new_size, default_value, igesio::DefaultValueFormat(type));
}

void IPVec::resize(const size_t new_size,
                   const igesio::VecParamType& default_value,
                   const igesio::ValueFormat& format) {
    data_.resize(new_size, default_value);
    formats_.resize(new_size, format);
}

void IPVec::reserve(const size_t capacity) {
    data_.reserve(capacity);
    formats_.reserve(capacity);
}

std::string
IPVec::get_as_string(size_t index, const igesio::SerializationConfig& config) const {
    if (index >= data_.size()) {
        throw std::out_of_range("Index out of range in IGESParameterVector.");
    }

    return std::visit([&](const auto& value) -> std::string {
        return ToIgesValue(value, formats_[index], config);
    }, data_[index]);
}

void IPVec::clear() noexcept {
    data_.clear();
    formats_.clear();
}

igesio::CppParameterType IPVec::get_type(size_t index) const {
    if (index >= formats_.size()) {
        throw std::out_of_range("Index out of range in IGESParameterVector.");
    }
    return static_cast<igesio::CppParameterType>(formats_[index].type);
}

igesio::ValueFormat IPVec::get_format(size_t index) const {
    if (index >= formats_.size()) {
        throw std::out_of_range("Index out of range in IGESParameterVector.");
    }
    return formats_[index];
}
