/**
 * @file graphics/core/type_conversion_utils.h
 * @brief 型変換ユーティリティ
 * @author Yayoi Habami
 * @date 2025-09-26
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_GRAPHICS_CORE_TYPE_CONVERSION_UTILS_H_
#define IGESIO_GRAPHICS_CORE_TYPE_CONVERSION_UTILS_H_

#include <algorithm>
#include <vector>

namespace igesio::graphics {

/// @brief std::vector<T> -> std::vector<float> の変換
template <typename T>
std::vector<float> ConvertToFloatVector(
        const std::vector<T>& vec) {
    std::vector<float> result(vec.size());
    std::transform(vec.begin(), vec.end(), result.begin(),
                   [](T val) { return static_cast<float>(val); });
    return result;
}

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CORE_TYPE_CONVERSION_UTILS_H_
