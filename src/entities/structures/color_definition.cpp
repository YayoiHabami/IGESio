/**
 * @file entities/structures/color_definition.cpp
 * @brief ColorDefinition (Type 314): 色定義エンティティの定義
 * @author Yayoi Habami
 * @date 2025-08-02
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/structures/color_definition.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace i_ent = igesio::entities;
using ColorDef = i_ent::ColorDefinition;

}  // namespace



/**
 * コンストラクタ
 */

ColorDef::ColorDefinition(const RawEntityDE& de_record,
                          const IGESParameterVector& parameters,
                          const pointer2ID& de2id,
                          const ObjectID& iges_id)
        : EntityBase(de_record, parameters, de2id, iges_id) {
    InitializePD(de2id);

    // 仕様上 Type 314 の Color Number (DEフィールド13) は 1〜8 が必須だが、
    // 一部CAD (Fusion 360 等) は 0/未指定で出力する。読み込みを通すため、
    // 0/未指定の場合は定義RGBに最も近い標準色へ正規化する。検証 (IsValid) は
    // 1〜8 を要求し続けるため (出力は厳格)、ここで仕様準拠の値に寄せる。
    if (de_color_.GetValue() == 0) {
        de_color_.SetColor(GetClosestStandardColor(rgb_));
    }
}

ColorDef::ColorDefinition(const std::array<double, 3>& rgb,
                          const std::string& color_name)
        : ColorDefinition(RawEntityDE::ByDefault(EntityType::kColorDefinition, 0),
                          {rgb[0], rgb[1], rgb[2], color_name}) {
    // 自身のDEのColor Numberを設定
    de_color_.SetColor(GetClosestStandardColor(rgb_));
    // 値の検証
    auto errors = ValidatePD();
    if (!errors.is_valid) {
        throw igesio::EntityValueError("Invalid parameters for ColorDefinition: " +
                                        errors.Message());
    }
}



/**
 * EntityBase Implementation
 */

igesio::IGESParameterVector ColorDef::GetMainPDParameters() const {
    IGESParameterVector params{rgb_[0], rgb_[1], rgb_[2]};
    if (!color_name_.empty()) {
        params.push_back(color_name_);
    }
    return params;
}

size_t ColorDef::SetMainPDParameters(const pointer2ID& de2id) {
    auto& pd = pd_parameters_;
    if (pd.size() < 3) {
        throw igesio::EntityParameterError("ColorDefinition requires 3 or 4 parameters.");
    }

    rgb_[0] = pd.access_as<double>(0);
    rgb_[1] = pd.access_as<double>(1);
    rgb_[2] = pd.access_as<double>(2);

    if (pd.size() >= 4 && pd.is_type<std::string>(3)) {
        color_name_ = pd.access_as<std::string>(3);
        return 4;
    }
    color_name_.clear();
    return 3;
}

igesio::ValidationResult ColorDef::ValidatePD() const {
    std::vector<ValidationError> errors;
    if (rgb_[0] < 0.0 || rgb_[0] > 100.0) {
        errors.emplace_back("Red component is out of range [0.0, 100.0].");
    }
    if (rgb_[1] < 0.0 || rgb_[1] > 100.0) {
        errors.emplace_back("Green component is out of range [0.0, 100.0].");
    }
    if (rgb_[2] < 0.0 || rgb_[2] > 100.0) {
        errors.emplace_back("Blue component is out of range [0.0, 100.0].");
    }
    return MakeValidationResult(std::move(errors));
}



/**
 * ColorDefinition Implementation
 */

i_ent::ColorNumber ColorDef::GetClosestStandardColor(
        const std::array<double, 3>& rgb) const {
    double min_distance = std::numeric_limits<double>::max();
    ColorNumber closest_color = ColorNumber::kNoColor;

    // 標準色 (kBlack=1 〜 kWhite=8) の全てを探索する
    for (int i = 1; i <= 8; ++i) {
        const auto& color_vector = kColorVectors[static_cast<size_t>(i)];
        double distance = std::sqrt(std::pow(rgb[0] - color_vector[0], 2) +
                                    std::pow(rgb[1] - color_vector[1], 2) +
                                    std::pow(rgb[2] - color_vector[2], 2));
        if (distance < min_distance) {
            min_distance = distance;
            closest_color = static_cast<ColorNumber>(i);
        }
    }
    return closest_color;
}

void ColorDef::SetRGB(const std::array<double, 3>& rgb) {
    // 検証はValidatePDと同じ範囲規則 ([0.0, 100.0]) を用いる
    constexpr std::array<const char*, 3> kComponentNames = {"Red", "Green", "Blue"};
    for (size_t i = 0; i < 3; ++i) {
        if (rgb[i] < 0.0 || rgb[i] > 100.0) {
            throw igesio::EntityValueError(
                    std::string(kComponentNames[i]) +
                    " component is out of range [0.0, 100.0].");
        }
    }
    rgb_ = rgb;
    // DEのColor Numberも最も近い標準色へ更新する (構築時の不変条件を維持)
    de_color_.SetColor(GetClosestStandardColor(rgb_));
}

std::array<int, 3> ColorDef::GetRGB255() const {
    std::array<int, 3> rgb255{};
    for (size_t i = 0; i < 3; ++i) {
        // 範囲外のRGB値 (不正なファイルの読み込み時等) は[0, 255]に飽和させる
        const auto scaled = std::lround(rgb_[i] * 255.0 / 100.0);
        rgb255[i] = static_cast<int>(std::clamp(scaled, 0L, 255L));
    }
    return rgb255;
}

std::string ColorDef::GetHexCode() const {
    const auto rgb255 = GetRGB255();
    std::ostringstream oss;
    oss << '#' << std::hex << std::uppercase << std::setfill('0')
        << std::setw(2) << rgb255[0]
        << std::setw(2) << rgb255[1]
        << std::setw(2) << rgb255[2];
    return oss.str();
}



/**
 * ファクトリ関数
 */

std::shared_ptr<ColorDef>
i_ent::MakeColorDefinition(const std::array<double, 3>& rgb,
                           const std::string& color_name) {
    return std::make_shared<ColorDefinition>(rgb, color_name);
}

std::shared_ptr<ColorDef>
i_ent::MakeColorDefinitionFromRGB255(const int r, const int g, const int b,
                                     const std::string& color_name) {
    for (const int component : {r, g, b}) {
        if (component < 0 || component > 255) {
            throw std::invalid_argument(
                    "MakeColorDefinitionFromRGB255: components must be in"
                    " range [0, 255], but got (" + std::to_string(r) + ", " +
                    std::to_string(g) + ", " + std::to_string(b) + ").");
        }
    }
    return std::make_shared<ColorDefinition>(
            std::array<double, 3>{r * 100.0 / 255.0,
                                  g * 100.0 / 255.0,
                                  b * 100.0 / 255.0}, color_name);
}

std::shared_ptr<ColorDef>
i_ent::MakeColorDefinitionFromHex(const std::string& hex_code,
                                  const std::string& color_name) {
    // 先頭の'#'は省略可能。6桁の16進数字のみを受け付ける
    const std::string digits = (!hex_code.empty() && hex_code.front() == '#')
            ? hex_code.substr(1) : hex_code;
    const bool all_hex_digits = std::all_of(
            digits.begin(), digits.end(),
            [](const unsigned char c) { return std::isxdigit(c) != 0; });
    if (digits.size() != 6 || !all_hex_digits) {
        throw std::invalid_argument(
                "MakeColorDefinitionFromHex: hex_code must be in the form"
                " \"#RRGGBB\" or \"RRGGBB\", but got \"" + hex_code + "\".");
    }
    const int r = std::stoi(digits.substr(0, 2), nullptr, 16);
    const int g = std::stoi(digits.substr(2, 2), nullptr, 16);
    const int b = std::stoi(digits.substr(4, 2), nullptr, 16);
    return MakeColorDefinitionFromRGB255(r, g, b, color_name);
}
