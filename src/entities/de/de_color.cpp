/**
 * @file entities/de/de_color.cpp
 * @brief 13th Directory Entryフィールド (Color) を表すクラス
 * @author Yayoi Habami
 * @date 2025-06-08
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/de/de_color.h"

#include <memory>

namespace {

namespace i_ent = igesio::entities;
using DEColor = i_ent::DEColor;

}  // namespace



std::shared_ptr<const i_ent::IColorDefinition> DEColor::GetPointer() const {
    return GetPointer<IColorDefinition>();
}

DEColor::DEColor(const int value) : DEColor::DEFieldWrapper() {
    if ((value < 0) || (value > 8)) {
        throw std::invalid_argument("Invalid Color Number value: "
                + std::to_string(value) + ". Valid values are 0 to 8.");
    }
    SetColor(static_cast<ColorNumber>(value));
}

DEColor::DEColor(const ColorNumber value) : DEColor::DEFieldWrapper() {
    SetColor(value);
}

std::array<double, 3> DEColor::GetRGB() const {
    if (GetValueType() == DEFieldValueType::kDefault) {
        // デフォルト値の場合は黒を返す
        return {0.0, 0.0, 0.0};
    }

    if (GetValueType() == DEFieldValueType::kPositive) {
        // 規定色が指定されている場合はそのRGB値を返す
        switch (color_) {
            case ColorNumber::kNoColor:
                // 規定色が指定されていない場合は後の処理で対応
                break;
            case ColorNumber::kBlack:
                return {0.0, 0.0, 0.0};
            case ColorNumber::kRed:
                return {100.0, 0.0, 0.0};
            case ColorNumber::kGreen:
                return {0.0, 100.0, 0.0};
            case ColorNumber::kBlue:
                return {0.0, 0.0, 100.0};
            case ColorNumber::kYellow:
                return {100.0, 100.0, 0.0};
            case ColorNumber::kMagenta:
                return {100.0, 0.0, 100.0};
            case ColorNumber::kCyan:
                return {0.0, 100.0, 100.0};
            case ColorNumber::kWhite:
                return {100.0, 100.0, 100.0};
        }
    }

    // 規定色が指定されていない場合
    if (GetValueType() == DEFieldValueType::kPointer) {
        // ポインタが設定されている場合は、Color Definition Entityから取得する
        auto ptr = GetPointer<IColorDefinition>();
        if (ptr) return ptr->GetRGB();
    }

    // デフォルト値または無効な状態の場合は黒を返す
    return {0.0, 0.0, 0.0};
}

void DEColor::SetColor(const ColorNumber value) {
    color_ = value;
    if (color_ == ColorNumber::kNoColor) {
        SetAsDefault();  // デフォルト値として扱う
    } else {
        SetAsPositive();  // 正の値として扱う
    }
}

int DEColor::GetValue() const {
    if (GetValueType() == DEFieldValueType::kPositive) {
        return static_cast<int>(color_);
    } else if (GetValueType() == DEFieldValueType::kDefault) {
        // 設定順序によってはkDefaultでcolor_がkNoColor以外のケースが
        // あるため、ここで強制的に0を返す
        return 0;
    }
    return DEFieldWrapper::GetValue();
}
