/**
 * @file entities/structures/color_definition.cpp
 * @brief ColorDefinition (Type 314): 色定義エンティティの定義
 * @author Yayoi Habami
 * @date 2025-08-02
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/structures/color_definition.h"

#include <limits>
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
                          const uint64_t iges_id)
        : EntityBase(de_record, parameters, de2id, iges_id) {
    InitializePD(de2id);
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
        throw igesio::DataFormatError("Invalid parameters for ColorDefinition: " +
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
        throw igesio::DataFormatError("ColorDefinition requires 3 or 4 parameters.");
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

    for (int i = 1; i < 8; ++i) {
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
