/**
 * @file extensions/machines/project/project_definition.cpp
 * @brief プロジェクト定義 (TOML) のデータモデル
 * @author Yayoi Habami
 * @date 2026-09-12
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/machines/project/project_definition.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace igesio::extensions::machines {

bool IsReservedAttachName(const std::string_view name) {
    return name == kWorkMountAttach || name == kToolMountAttach;
}



/**
 * ---- 列挙型 ----
 */

std::optional<WorkOffsetFrom> ParseWorkOffsetFrom(const std::string_view text) {
    if (text == kToolMountAttach) return WorkOffsetFrom::kToolMount;
    if (text == "machine") return WorkOffsetFrom::kMachine;
    return std::nullopt;
}

std::string_view WorkOffsetFromName(const WorkOffsetFrom from) {
    return from == WorkOffsetFrom::kMachine ? "machine" : kToolMountAttach;
}

std::optional<ModelRole> ParseModelRole(const std::string_view text) {
    if (text == "stock") return ModelRole::kStock;
    if (text == "fixture") return ModelRole::kFixture;
    if (text == "design") return ModelRole::kDesign;
    if (text == "display") return ModelRole::kDisplay;
    return std::nullopt;
}

std::string_view ModelRoleName(const ModelRole role) {
    switch (role) {
        case ModelRole::kStock: return "stock";
        case ModelRole::kFixture: return "fixture";
        case ModelRole::kDesign: return "design";
        case ModelRole::kDisplay: return "display";
    }
    return "stock";
}

bool DefaultCollisionFor(const ModelRole role) {
    return role == ModelRole::kStock || role == ModelRole::kFixture;
}

std::optional<ProgramType> ParseProgramType(const std::string_view text) {
    if (text == "gcode") return ProgramType::kGcode;
    if (text == "cl") return ProgramType::kCl;
    if (text == "apt") return ProgramType::kApt;
    return std::nullopt;
}

std::string_view ProgramTypeName(const ProgramType type) {
    switch (type) {
        case ProgramType::kGcode: return "gcode";
        case ProgramType::kCl: return "cl";
        case ProgramType::kApt: return "apt";
    }
    return "gcode";
}

bool DefaultToolPairEnabled(const ToolPart part, const ModelRole target) {
    return !(part == ToolPart::kCutter && target == ModelRole::kStock);
}

std::optional<OvertravelPolicy>
ParseOvertravelPolicy(const std::string_view text) {
    if (text == "error") return OvertravelPolicy::kError;
    if (text == "warning") return OvertravelPolicy::kWarning;
    if (text == "ignore") return OvertravelPolicy::kIgnore;
    return std::nullopt;
}

std::string_view OvertravelPolicyName(const OvertravelPolicy policy) {
    switch (policy) {
        case OvertravelPolicy::kError: return "error";
        case OvertravelPolicy::kWarning: return "warning";
        case OvertravelPolicy::kIgnore: return "ignore";
    }
    return "error";
}

std::optional<CollisionPolicy>
ParseCollisionPolicy(const std::string_view text) {
    if (text == "warning") return CollisionPolicy::kWarning;
    if (text == "error") return CollisionPolicy::kError;
    return std::nullopt;
}

std::string_view CollisionPolicyName(const CollisionPolicy policy) {
    return policy == CollisionPolicy::kError ? "error" : "warning";
}



/**
 * ---- 派生値 ----
 */

double UnitScale(const ProgramSpec& program, const UnitScales& units) {
    if (program.type == ProgramType::kGcode) return 1.0;
    return LengthScale(program.unit.value_or(units.length_unit));
}

std::string DisplayName(const ProgramSpec& program) {
    if (!program.name.empty()) return program.name;
    return program.file.resolved.filename().string();
}

const ToolEntry* FindTool(const ProjectDefinition& project, const int number) {
    for (const ToolEntry& tool : project.tools) {
        if (tool.number == number) return &tool;
    }
    return nullptr;
}

const ToolLibrarySpec* FindToolLibrary(const ProjectDefinition& project,
                                       const std::string_view alias) {
    for (const ToolLibrarySpec& library : project.tool_libraries) {
        if (library.alias == alias) return &library;
    }
    return nullptr;
}

const WorkOffsetSpec* FindWorkOffset(const ProjectDefinition& project,
                                     const std::string_view id) {
    for (const WorkOffsetSpec& offset : project.work_offsets) {
        if (offset.id == id) return &offset;
    }
    return nullptr;
}

const ModelSpec* FindModel(const ProjectDefinition& project,
                           const std::string_view name) {
    for (const ModelSpec& model : project.models) {
        if (model.name == name) return &model;
    }
    return nullptr;
}

std::filesystem::path OutputDir(const ProjectDefinition& project) {
    if (project.run.output.dir.has_value()) {
        return project.run.output.dir->resolved;
    }
    return project.source_dir / "output";
}

}  // namespace igesio::extensions::machines
