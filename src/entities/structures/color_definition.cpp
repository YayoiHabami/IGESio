/**
 * @file entities/structures/color_definition.cpp
 * @brief ColorDefinition (Type 314): 色定義エンティティの定義
 * @author Yayoi Habami
 * @date 2025-08-02
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/structures/color_definition.h"

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace i_ent = igesio::entities;
using ColorDef = i_ent::ColorDefinition;

/// @brief RGBの各成分が[0.0, 1.0]の範囲に収まっているかを検証する
/// @param color 検証する色 (αは検証しない)
/// @throw igesio::EntityValueError RGB成分が[0.0, 1.0]の範囲外の場合
void ValidateUnitRangeRGB(const igesio::Color& color) {
    constexpr std::array<const char*, 3> kComponentNames = {"Red", "Green", "Blue"};
    for (std::size_t i = 0; i < 3; ++i) {
        if (color[i] < 0.0 || color[i] > 1.0) {
            throw igesio::EntityValueError(
                    std::string(kComponentNames[i]) +
                    " component is out of range [0.0, 1.0].");
        }
    }
}

/// @brief 定義色と色名からType 314のPDパラメータを組み立てる
/// @param color 定義色 (単位スケール)
/// @param color_name 色名 (空文字列も4番目のパラメータとして渡す)
/// @return {R, G, B, name} (RGBはIGESスケール 0.0〜100.0)
igesio::IGESParameterVector MakeColorPD(const igesio::Color& color,
                                        const std::string& color_name) {
    const std::array<double, 3> iges_rgb = color.ToIgesRGB();
    return {iges_rgb[0], iges_rgb[1], iges_rgb[2], color_name};
}

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
        de_color_.SetColor(ClosestColorNumber(GetRGB()));
    }
}

ColorDef::ColorDefinition(const Color& color, const std::string& color_name)
        : ColorDefinition(RawEntityDE::ByDefault(EntityType::kColorDefinition, 0),
                          MakeColorPD(color, color_name)) {
    // 値の検証 (読み込み経路と異なり、プログラムからの指定時は
    //          RGBの各成分が[0.0, 1.0]の範囲内であることを要求する)
    ValidateUnitRangeRGB(color);
    // 自身のDEのColor Numberを設定
    de_color_.SetColor(ClosestColorNumber(color));
}



/**
 * EntityBase Implementation
 */

igesio::IGESParameterVector ColorDef::GetMainPDParameters() const {
    IGESParameterVector params{iges_rgb_[0], iges_rgb_[1], iges_rgb_[2]};
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

    iges_rgb_[0] = pd.access_as<double>(0);
    iges_rgb_[1] = pd.access_as<double>(1);
    iges_rgb_[2] = pd.access_as<double>(2);

    if (pd.size() >= 4 && pd.is_type<std::string>(3)) {
        color_name_ = pd.access_as<std::string>(3);
        return 4;
    }
    color_name_.clear();
    return 3;
}

igesio::ValidationResult ColorDef::ValidatePD() const {
    std::vector<ValidationError> errors;
    if (iges_rgb_[0] < 0.0 || iges_rgb_[0] > 100.0) {
        errors.emplace_back("Red component is out of range [0.0, 100.0].");
    }
    if (iges_rgb_[1] < 0.0 || iges_rgb_[1] > 100.0) {
        errors.emplace_back("Green component is out of range [0.0, 100.0].");
    }
    if (iges_rgb_[2] < 0.0 || iges_rgb_[2] > 100.0) {
        errors.emplace_back("Blue component is out of range [0.0, 100.0].");
    }
    return MakeValidationResult(std::move(errors));
}



/**
 * ColorDefinition Implementation
 */

void ColorDef::SetRGB(const Color& color) {
    ValidateUnitRangeRGB(color);
    iges_rgb_ = color.ToIgesRGB();
    // DEのColor Numberも最も近い標準色へ更新する (構築時の不変条件を維持)
    de_color_.SetColor(ClosestColorNumber(color));
}



/**
 * ファクトリ関数
 */

std::shared_ptr<ColorDef>
i_ent::MakeColorDefinition(const Color& color, const std::string& color_name) {
    return std::make_shared<ColorDefinition>(color, color_name);
}

std::shared_ptr<ColorDef>
i_ent::MakeColorDefinitionFromRGB255(const int r, const int g, const int b,
                                     const std::string& color_name) {
    // Color::FromRGB255は範囲検証を行わないため、ここで検査する
    for (const int component : {r, g, b}) {
        if (component < 0 || component > 255) {
            throw std::invalid_argument(
                    "MakeColorDefinitionFromRGB255: components must be in"
                    " range [0, 255], but got (" + std::to_string(r) + ", " +
                    std::to_string(g) + ", " + std::to_string(b) + ").");
        }
    }
    return MakeColorDefinition(Color::FromRGB255(r, g, b), color_name);
}

std::shared_ptr<ColorDef>
i_ent::MakeColorDefinitionFromHex(const std::string& hex_code,
                                  const std::string& color_name) {
    // 受理形式の判定と例外はColor::FromHexに委ねる (8桁のα成分は捨てられる)
    return MakeColorDefinition(Color::FromHex(hex_code), color_name);
}
