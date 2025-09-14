/**
 * @file entities/de/de_line_font_pattern.cpp
 * @brief 4th Directory Entryフィールド (Line Font Pattern) を表すクラス
 * @author Yayoi Habami
 * @date 2025-06-08
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/de/de_line_font_pattern.h"

#include <memory>

namespace {

namespace i_ent = igesio::entities;
using DELineFontPattern = i_ent::DELineFontPattern;

}  // namespace



std::shared_ptr<const i_ent::ILineFontDefinition>
DELineFontPattern::GetPointer() const {
    return GetPointer<i_ent::ILineFontDefinition>();
}

DELineFontPattern::DELineFontPattern(const int value)
        : DELineFontPattern::DEFieldWrapper() {
    if ((value < 0) || (value > 5)) {
        throw std::invalid_argument("Invalid Line Font Pattern value: "
                + std::to_string(value) + ". Valid values are 1 to 5.");
    }
    SetPattern(static_cast<LineFontPattern>(value));
}

DELineFontPattern::DELineFontPattern(const LineFontPattern value)
        : DELineFontPattern::DEFieldWrapper() {
    SetPattern(value);
}

void DELineFontPattern::SetPattern(const LineFontPattern value) {
    pattern_ = value;
    if (pattern_ == LineFontPattern::kNoPattern) {
        SetAsDefault();  // デフォルト値として扱う
    } else {
        SetAsPositive();  // 正の値として扱う
    }
}

int DELineFontPattern::GetValue(const id2pointer& id2de) const {
    if (GetValueType() == DEFieldValueType::kPositive) {
        return static_cast<int>(pattern_);
    } else if (GetValueType() == DEFieldValueType::kDefault) {
        // 設定順序によってはkDefaultでpattern_がkNoPattern以外のケースが
        // あるため、ここで強制的に0を返す
        return 0;
    }
    // 0またはポインタ
    // field 4ではポインタは負の値で表現されるため、符号反転する
    return -DEFieldWrapper::GetValue(id2de);
}
