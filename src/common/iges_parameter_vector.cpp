/**
 * @file common/iges_parameter_vector.cpp
 * @brief IGESのパラメータを混合して保持するためのクラス
 * @author Yayoi Habami
 * @date 2025-05-31
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/common/iges_parameter_vector.h"

#include <iomanip>
#include <ostream>
#include <string>

namespace {

using IPVec = igesio::IGESParameterVector;

}  // namespace



IPVec::IGESParameterVector(const std::initializer_list<VecParamType>& init)
        : data_(init), formats_(init.size(), DefaultValueFormat<int>()) {
    // formats_の各要素をinitの型に合わせて変更
    for (size_t i = 0; i < init.size(); ++i) {
        auto type = static_cast<CppParameterType>(init.begin()[i].index());
        formats_[i] = DefaultValueFormat(type);
    }
}

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

size_t IPVec::capacity() const noexcept {
    return data_.capacity();
}



/**
 * 要素へのアクセス
 */

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

IPVec IPVec::copy(size_t idx_start, size_t count) const {
    if (idx_start >= data_.size() || idx_start + count > data_.size()) {
        throw std::out_of_range(
                "Index out of range in IGESParameterVector copy."
                " idx_start: " + std::to_string(idx_start) +
                ", count: " + std::to_string(count) + ")");
    }
    IGESParameterVector copy_vector;
    auto d_idx_start = static_cast<std::vector<VecParamType>::difference_type>(idx_start);
    auto d_count = static_cast<std::vector<VecParamType>::difference_type>(count);
    copy_vector.data_.assign(
            data_.begin() + d_idx_start, data_.begin() + d_idx_start + d_count);
    copy_vector.formats_.assign(
            formats_.begin() + d_idx_start, formats_.begin() + d_idx_start + d_count);
    return copy_vector;
}



/**
 * 要素の検証など
 */

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

void IPVec::set_format(size_t index, const ValueFormat& format) {
    if (index >= formats_.size()) {
        throw std::out_of_range("Index out of range in IGESParameterVector.");
    }
    if (!IsCompatibleParameterType(get_type(index), format.type)) {
        throw std::invalid_argument("Format type does not match the element type.");
    }
    formats_[index] = format;
}



std::ostream& igesio::operator<<(std::ostream& os, const IGESParameterVector& vec) {
    os << "[";
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) {
            os << ", ";
        }
        if (vec.is_type<bool>(i)) {
            os << vec.get<bool>(i);
        } else if (vec.is_type<int>(i)) {
            os << vec.get<int>(i);
        } else if (vec.is_type<double>(i)) {
            // doubleは常に小数部分を出力 (整数の場合は".0"を付加)
            double value = vec.get<double>(i);
            if (value == static_cast<int>(value)) {
                os << std::fixed << std::setprecision(1) << value;
            } else {
                os << value;
            }
        } else if (vec.is_type<ObjectID>(i)) {
            os << ToString(vec.get<ObjectID>(i));
        } else if (vec.is_type<std::string>(i)) {
            os << vec.get<std::string>(i);
        } else {
            os << " ";
        }
    }
    os << "]";
    return os;
}

igesio::ObjectID igesio::GetObjectIDFromParameters(
        const IGESParameterVector& parameters,
        const size_t index, const pointer2ID& de2id,
        const bool allow_unset_id) {
    if (index >= parameters.size()) {
        throw std::out_of_range("Parameter index " + std::to_string(index)
            + " out of range (size: " + std::to_string(parameters.size()) + ").");
    }

    ObjectID result_id;
    if (parameters.is_type<int>(index)) {
        // int型の場合 (負値の場合もあるため、絶対値を取る)
        auto int_id = std::abs(parameters.get<int>(index));
        if (int_id == kInvalidIntID) {
            // 未設定ID
            result_id = IDGenerator::UnsetID();
        } else {
            // 有効なint型ID
            if (de2id.empty()) {
                // de2idが空の場合はエラー
                throw std::invalid_argument(
                    "Cannot resolve ObjectID from int ID "
                    + std::to_string(int_id) + " because de2id mapping is empty.");
            }
            if (de2id.find(int_id) == de2id.end()) {
                // de2idに存在しない場合はエラー
                throw std::invalid_argument(
                    "Cannot resolve ObjectID from int ID "
                    + std::to_string(int_id) + " because it is not found in de2id mapping.");
            }
            result_id = de2id.at(int_id);
        }
    } else if (parameters.is_type<ObjectID>(index)) {
        // ObjectID型の場合
        result_id = parameters.get<ObjectID>(index);
    } else {
        // それ以外の型の場合はエラー
        throw std::invalid_argument(
            "Parameter at index " + std::to_string(index)
            + " is neither int nor ObjectID type.");
    }

    // 未設定IDを許容しない場合のチェック
    if (!allow_unset_id && result_id == IDGenerator::UnsetID()) {
        throw std::invalid_argument(
            "Parameter at index " + std::to_string(index)
            + " resolves to UnsetID, which is not allowed.");
    }

    return result_id;
}
