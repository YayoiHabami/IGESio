/**
 * @file entities/de/de_color.cpp
 * @brief 13th Directory Entryフィールド (Color) を表すクラス
 * @author Yayoi Habami
 * @date 2025-06-08
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/de/de_color.h"

#include <limits>
#include <memory>

#include "igesio/common/errors.h"

namespace {

namespace i_ent = igesio::entities;
using DEColor = i_ent::DEColor;

}  // namespace



i_ent::ColorNumber i_ent::ClosestColorNumber(const Color& color) {
    double min_distance = std::numeric_limits<double>::max();
    ColorNumber closest = ColorNumber::kBlack;

    // 標準色 (kBlack=1 〜 kWhite=8) の全てを探索する.
    // 厳密な小なり比較のため、同距離の場合は先に見つかった (番号の小さい) 色を採用する
    for (int i = static_cast<int>(ColorNumber::kBlack);
         i <= static_cast<int>(ColorNumber::kWhite); ++i) {
        const auto number = static_cast<ColorNumber>(i);
        const double distance = color.SquaredDistanceRGB(ToColor(number));
        if (distance < min_distance) {
            min_distance = distance;
            closest = number;
        }
    }
    return closest;
}



std::shared_ptr<const i_ent::IColorDefinition> DEColor::GetPointer() const {
    return GetPointer<IColorDefinition>();
}

DEColor::DEColor(const int value) : DEColor::DEFieldWrapper() {
    if ((value < 0) || (value > 8)) {
        throw igesio::DataFormatError("Invalid Color Number value: "
                + std::to_string(value) + ". Valid values are 0 to 8.");
    }
    SetColor(static_cast<ColorNumber>(value));
}

DEColor::DEColor(const ColorNumber value) : DEColor::DEFieldWrapper() {
    SetColor(value);
}

igesio::Color DEColor::GetRGB() const {
    if (GetValueType() == DEFieldValueType::kPositive) {
        // 規定色が指定されている場合はその色 (kNoColorは黒)
        return ToColor(color_);
    }

    if (GetValueType() == DEFieldValueType::kPointer) {
        // ポインタが設定されている場合は、Color Definition Entityから取得する
        auto ptr = GetPointer<IColorDefinition>();
        if (ptr) return ptr->GetRGB();
    }

    // デフォルト値または参照未解決の場合は黒を返す
    return Color{};
}

void DEColor::SetColor(const ColorNumber value) {
    color_ = value;
    if (color_ == ColorNumber::kNoColor) {
        SetAsDefault();  // デフォルト値として扱う
    } else {
        SetAsPositive();  // 正の値として扱う
    }
}

int DEColor::GetValue(const id2pointer& id2de) const {
    if (GetValueType() == DEFieldValueType::kPositive) {
        return static_cast<int>(color_);
    } else if (GetValueType() == DEFieldValueType::kDefault) {
        // 設定順序によってはkDefaultでcolor_がkNoColor以外のケースが
        // あるため、ここで強制的に0を返す
        return 0;
    }
    // 0またはポインタ
    // field 13ではポインタは負の値で表現されるため、符号反転する
    return -DEFieldWrapper::GetValue(id2de);
}
