/**
 * @file entities/de/de_level.cpp
 * @brief 4th Directory Entryフィールド (Level) を表すクラス
 * @author Yayoi Habami
 * @date 2025-06-08
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/de/de_level.h"

#include <memory>

namespace {

namespace i_ent = igesio::entities;
using DELevel = i_ent::DELevel;

}  // namespace



std::shared_ptr<const i_ent::IDefinitionLevelsProperty>
DELevel::GetPointer() const {
    return GetPointer<IDefinitionLevelsProperty>();
}

DELevel::DELevel(const int value)
        : DELevel::DEFieldWrapper(), level_number_(value) {
    SetLevelNumber(value);
}

void DELevel::SetLevelNumber(const int value) {
    if (value < 0) {
        throw std::invalid_argument("Level number must be non-negative: "
                                    + std::to_string(value));
    }
    level_number_ = value;
    if (level_number_ == 0) {
        SetAsDefault();  // デフォルト値として扱う
    } else {
        SetAsPositive();  // 正の値として扱う
    }
}

int DELevel::GetValue() const {
    if (GetValueType() == DEFieldValueType::kPositive) {
        return level_number_;
    } else if (GetValueType() == DEFieldValueType::kDefault) {
        // 設定順序によってはkDefaultでlevel_number_が0以外のケースが
        // あるため、ここで強制的に0を返す
        return 0;
    }
    return DEFieldWrapper::GetValue();
}
