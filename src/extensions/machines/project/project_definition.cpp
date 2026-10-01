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
#include <utility>
#include <variant>

#include "igesio/extensions/machines/machine/machine_io.h"
#include "igesio/utils/path_encoding.h"

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
    return utils::PathToUtf8(program.file.resolved.filename());
}



/**
 * ---- 組み立て ----
 */

namespace {

/// @brief 機械定義を持つプロジェクト定義を作る
/// @param source 機械定義の指定
/// @param machine `source`から作った機械定義
/// @param name プロジェクト名
/// @return 機械定義の警告は読込と同じく`context = "machine"`で`warnings`に転記する
ProjectDefinition MakeProjectWithMachine(
        std::variant<FileReference, VirtualMachineSpec> source,
        MachineDefinition machine, const std::string_view name) {
    ProjectDefinition project;
    project.format_version = kProjectFormatVersion;
    project.name = std::string(name);
    project.source_name = std::string(name);
    for (const Diagnostic& diagnostic : machine.warnings) {
        const std::string prefix =
                diagnostic.context.empty() ? "" : diagnostic.context + ": ";
        project.warnings.push_back(Diagnostic{
                diagnostic.severity, "machine", prefix + diagnostic.message,
                diagnostic.line});
    }
    project.machine_source = std::move(source);
    project.machine = std::move(machine);
    return project;
}

}  // namespace



ProjectDefinition MakeProjectDefinition(FileReference machine_file,
                                        const std::string_view name) {
    MachineDefinition machine = ReadMachineDefinition(machine_file.resolved);
    return MakeProjectWithMachine(std::move(machine_file), std::move(machine),
                                  name);
}

ProjectDefinition MakeProjectDefinition(const VirtualMachineSpec& machine,
                                        const std::string_view name) {
    return MakeProjectWithMachine(machine, MakeVirtualMachineDefinition(machine),
                                  name);
}



/**
 * ---- 検索 ----
 */

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
