/**
 * @file entities/de/de_test_core.h
 * @brief RawEntityDEのテストに必要な情報を定義する
 * @author Yayoi Habami
 * @date 2025-08-13
 * @copyright 2025 Yayoi Habami
 */
#ifndef TESTS_ENTITIES_DE_DE_TEST_CORE_H_
#define TESTS_ENTITIES_DE_DE_TEST_CORE_H_

#include "igesio/common/errors.h"

#include "igesio/entities/de/raw_entity_de.h"

#include <string>



namespace igesio::tests {

/// @brief DEセクションの各レコードに入る数値の種類
enum class DEValueType {
    /// @brief Not Applicable (<n.a.>)
    kNA,
    /// @brief Integer (#)
    kInt,
    /// @brief Pointer (=>)
    kPtr,
    /// @brief Integer or Pointer (#,=>; pointer is negated)
    kIPtr,
    /// @brief Zero or Pointer (0,=>)
    kZPtr,
    /// @brief 1
    kOne,
    /// @brief 0
    kZero,
    /// @brief 1以上
    /// @note Type 304のパラメータ4と、Type 314のパラメータ13では、
    ///       未指定 (0) を許容せず、かつポインター (負値) も許容しない
    //        ため、これを使用する
    kPositive
};

/// @brief DEValueTypeで受け入れられるデフォルト値を返す
/// @param type DEValueType
/// @return int値
/// @throw iio::ImplementationError 無効なDEValueTypeが指定された場合
int DefaultValue(const DEValueType type) {
    switch (type) {
        case DEValueType::kNA:
        case DEValueType::kInt:
        case DEValueType::kIPtr:
        case DEValueType::kZPtr:
        case DEValueType::kZero:
            return 0;
        case DEValueType::kPtr:
            return 1;  // `=>`の場合は1以上の正値で参照する
        case DEValueType::kOne:
            return 1;
        case DEValueType::kPositive:
            return 1;  // 1以上の値を期待するが、ここでは1を返す
        default:
            throw igesio::ImplementationError("Unknown DEValueType");
    }
}

/// @brief ステータス番号からデフォルト値を取得する
/// @param char1 ステータス番号 (1桁目)
/// @param char2 ステータス番号 (2桁目)
/// @return ステータス番号のデフォルト値
int DefaultStatusNumber(const char char1, const char char2) {
    if (char1 == '*' && char2 == '*') {
        return 0;  // N.A.と同等
    } else if (char1 == '?' && char2 == '?') {
        return -1;  // 任意値
    } else if (char1 == '0' && (char2 >= '0' && char2 <= '6')) {
        return static_cast<int>(char2 - '0');
    }
    throw igesio::ImplementationError(
            "Invalid status number characters: '" + std::string(1, char1) +
            std::string(1, char2) + "'");
}

/// @brief EntityStatusのデフォルト値を取得する
/// @param status ステータス番号
/// @return EntityStatus
igesio::entities::EntityStatus
DefaultEntityStatus(const std::string& status) {
    if (status.size() != 8) {
        throw igesio::ImplementationError(
            "Invalid status string size; expected 8 digits, got '" + status + "'");
    }

    const auto& s = status;
    igesio::entities::EntityStatus es;
    es.blank_status = DefaultStatusNumber(s[0], s[1]) == 0 ? true : false;
    es.subordinate_entity_switch = static_cast<igesio::entities::SubordinateEntitySwitch>(
        DefaultStatusNumber(s[2], s[3]));
    es.entity_use_flag = static_cast<igesio::entities::EntityUseFlag>(
        DefaultStatusNumber(s[4], s[5]));
    es.hierarchy = static_cast<igesio::entities::HierarchyType>(
        DefaultStatusNumber(s[6], s[7]));

    return es;
}

}  // namespace igesio::tests

#endif  // TESTS_ENTITIES_DE_DE_TEST_CORE_H_
