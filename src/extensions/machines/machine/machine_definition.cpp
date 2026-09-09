/**
 * @file extensions/machines/machine/machine_definition.cpp
 * @brief 機械定義 (TOML) のデータモデル
 * @author Yayoi Habami
 * @date 2026-09-08
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/machines/machine/machine_definition.h"

#include <optional>
#include <string_view>

namespace igesio::extensions::machines {

std::optional<ComponentType> ParseComponentType(const std::string_view text) {
    if (text == "base") return ComponentType::kBase;
    if (text == "linear") return ComponentType::kLinear;
    if (text == "rotary") return ComponentType::kRotary;
    if (text == "spindle") return ComponentType::kSpindle;
    if (text == "tool_mount") return ComponentType::kToolMount;
    if (text == "work_mount") return ComponentType::kWorkMount;
    if (text == "fixed") return ComponentType::kFixed;
    return std::nullopt;
}

std::string_view ComponentTypeName(const ComponentType type) {
    switch (type) {
        case ComponentType::kBase: return "base";
        case ComponentType::kLinear: return "linear";
        case ComponentType::kRotary: return "rotary";
        case ComponentType::kSpindle: return "spindle";
        case ComponentType::kToolMount: return "tool_mount";
        case ComponentType::kWorkMount: return "work_mount";
        case ComponentType::kFixed: return "fixed";
    }
    return "fixed";
}

std::optional<BranchPolicy> ParseBranchPolicy(const std::string_view text) {
    if (text == "positive") return BranchPolicy::kPositive;
    if (text == "negative") return BranchPolicy::kNegative;
    if (text == "continuous") return BranchPolicy::kContinuous;
    return std::nullopt;
}

std::string_view BranchPolicyName(const BranchPolicy policy) {
    switch (policy) {
        case BranchPolicy::kPositive: return "positive";
        case BranchPolicy::kNegative: return "negative";
        case BranchPolicy::kContinuous: return "continuous";
    }
    return "positive";
}

std::optional<CollisionMode> ParseCollisionMode(const std::string_view text) {
    if (text == "pairs") return CollisionMode::kPairs;
    if (text == "all_except_adjacent") return CollisionMode::kAllExceptAdjacent;
    return std::nullopt;
}

std::string_view CollisionModeName(const CollisionMode mode) {
    return mode == CollisionMode::kAllExceptAdjacent ? "all_except_adjacent"
                                                     : "pairs";
}

}  // namespace igesio::extensions::machines
