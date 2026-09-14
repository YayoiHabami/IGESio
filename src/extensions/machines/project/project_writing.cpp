/**
 * @file extensions/machines/project/project_writing.cpp
 * @brief プロジェクト定義をTOML形式の文字列に変換する関数の実装
 * @author Yayoi Habami
 * @date 2026-09-12
 * @copyright 2026 Yayoi Habami
 * @note `toml::ordered_value`で出力内容を組み立て、`[[tool]]`/`[tool.simple]`
 *       等の配置はtoml11のシリアライザに任せる. 値の生成、幾何要素,
 *       `[format]`/`[units]`の出力は機械定義と共有する`toml_writing.h`にある.
 */
#include "extensions/machines/project/project_writing.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <toml.hpp>

#include "igesio/extensions/machines/core/formatting.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/machine/axis_values.h"
#include "igesio/extensions/machines/tools/tool_assembly.h"
#include "igesio/extensions/machines/tools/tool_profile.h"
#include "extensions/machines/machine/toml_writing.h"

namespace igesio::extensions::machines::detail {

namespace {

/// @brief 出力の先頭に置く見出しコメント
constexpr const char* kHeaderComment =
        "# machining-project 1.0 "
        "(written by the IGESio machines extension)\n\n";

/// @brief 軸名→回転軸かの表
/// @note NC指令値の単位換算に用いる
using AxisKinds = std::unordered_map<std::string, bool>;

/// @brief 機械定義から軸名→回転軸かの表を作る
/// @param machine 機械定義
AxisKinds CollectAxisKinds(const MachineDefinition& machine) {
    AxisKinds kinds;
    for (const ComponentSpec& component : machine.components) {
        if (component.axis.has_value()) {
            kinds[component.axis->register_name] =
                    component.type == ComponentType::kRotary;
        }
    }
    return kinds;
}

/**
 * ---- 参照/NC指令値 ----
 */

/// @brief ファイル参照を`file =`/`library =`として書き込む
/// @param[out] table 書き込み先のテーブル
/// @param reference ファイル参照
/// @param context 出力箇所 (例外の文言に用いる)
/// @param ctx 出力の文脈
/// @throw std::invalid_argument ライブラリ参照で`raw`が空、または`file`参照で
///        `raw`/`resolved`とも空の場合
void PutFileReference(TomlValue& table, const FileReference& reference,
                      const std::string& context, const WriteContext& ctx) {
    if (reference.from_library) {
        if (reference.raw.empty()) {
            throw std::invalid_argument(
                    context + ": library reference has no path");
        }
        table["library"] = reference.raw;
        return;
    }
    if (reference.raw.empty() && reference.resolved.empty()) {
        throw std::invalid_argument(context + ": file reference has no path");
    }
    table["file"] = PathText(reference.raw, reference.resolved, ctx);
}

/// @brief 軸名→NC指令値のテーブル (`values`/`[initial.axes]`) を宣言単位で書く
/// @param values 軸名→NC指令値 (内部単位)
/// @param kinds 軸名→回転軸かの表
/// @param context 出力箇所 (例外の文言に用いる)
/// @param ctx 出力の文脈
/// @param inline_form インライン表 (`values = { ... }`) で書くか
/// @throw std::invalid_argument 機械に無い軸名を含む場合
TomlValue MakeAxisTable(const NcValues& values, const AxisKinds& kinds,
                        const std::string& context, const WriteContext& ctx,
                        const bool inline_form) {
    TomlValue table = inline_form ? InlineTable() : Table();
    for (const NcEntry& entry : values.Entries()) {
        const auto it = kinds.find(entry.register_name);
        if (it == kinds.end()) {
            throw std::invalid_argument(context + ": " + entry.register_name
                                        + " is not an axis of the machine");
        }
        const double scale = it->second ? ctx.angle_scale : ctx.length_scale;
        table[entry.register_name] = Real(entry.value / scale);
    }
    return table;
}

/**
 * ---- 工具 ----
 */

/// @brief `[tool.simple]`を書く
/// @param spec 簡易アセンブリの定義 (内部単位)
/// @param ctx 出力の文脈
TomlValue MakeSimpleTool(const SimpleToolSpec& spec, const WriteContext& ctx) {
    const double scale = ctx.length_scale;
    TomlValue table = Table();
    table["cutter"] = std::string(SimpleCutterName(spec.cutter));
    table["diameter"] = Real(spec.diameter / scale);
    if (spec.cutter == SimpleToolSpec::Cutter::kRadius) {
        table["corner_radius"] = Real(spec.corner_radius / scale);
    }
    table["cutting_length"] = Real(spec.cutting_length / scale);
    table["tool_length"] = Real(spec.tool_length / scale);
    if (spec.command_point != SimpleToolSpec::CommandPoint::kTip) {
        table["command_point"] =
                std::string(SimpleCommandPointName(spec.command_point));
    }
    table["overhang"] = Real(spec.overhang / scale);
    TomlValue holder = Table();
    holder["diameter"] = Real(spec.holder_diameter / scale);
    holder["length"] = Real(spec.holder_length / scale);
    table["holder"] = holder;
    return table;
}

/// @brief `[[tool]]`の1要素を書く
/// @param entry 出力する工具 (内部単位)
/// @param ctx 出力の文脈
TomlValue MakeTool(const ToolEntry& entry, const WriteContext& ctx) {
    TomlValue table = Table();
    table["number"] = entry.number;
    if (!entry.name.empty()) table["name"] = entry.name;
    if (const auto* ref = std::get_if<LibraryToolRef>(&entry.source);
        ref != nullptr) {
        if (!ref->source.empty()) table["source"] = ref->source;
        table["assembly"] = ref->assembly;
    }
    if (entry.gauge_length.has_value()) {
        table["gauge_length"] = Real(*entry.gauge_length / ctx.length_scale);
    }
    if (entry.control_point != ControlPoint::kTip) {
        table["control_point"] =
                std::string(ControlPointName(entry.control_point));
    }
    if (const auto* simple = std::get_if<SimpleToolSpec>(&entry.source);
        simple != nullptr) {
        table["simple"] = MakeSimpleTool(*simple, ctx);
    }
    return table;
}

/// @brief `[[tool_offset]]`の1要素を書く
/// @param entry 出力する工具オフセット (内部単位)
/// @param ctx 出力の文脈
/// @note 摩耗量が0のキーは省略する
TomlValue MakeToolOffset(
        const ToolOffsetEntry& entry, const WriteContext& ctx) {
    const double scale = ctx.length_scale;
    TomlValue table = Table();
    table["number"] = entry.number;
    if (entry.tool.has_value()) table["tool"] = *entry.tool;
    if (entry.length.has_value()) table["length"] = Real(*entry.length / scale);
    if (entry.length_wear != 0.0) {
        table["length_wear"] = Real(entry.length_wear / scale);
    }
    if (entry.radius.has_value()) table["radius"] = Real(*entry.radius / scale);
    if (entry.radius_wear != 0.0) {
        table["radius_wear"] = Real(entry.radius_wear / scale);
    }
    return table;
}

/// @brief `[[tool_library]]`の1要素を書く
/// @param library 出力するライブラリ参照
/// @param index `[[tool_library]]`内の添字 (例外の文言に用いる)
/// @param ctx 出力の文脈
/// @throw std::invalid_argument ライブラリ参照で`raw`が空、または`file`参照で
///        `raw`/`resolved`とも空の場合 (`PutFileReference`から伝播)
TomlValue MakeToolLibrary(
        const ToolLibrarySpec& library, const std::size_t index,
        const WriteContext& ctx) {
    TomlValue table = Table();
    if (!library.alias.empty()) table["alias"] = library.alias;
    PutFileReference(table, library.file,
                     "[[tool_library]][" + std::to_string(index) + "]", ctx);
    return table;
}

/**
 * ---- ワークオフセット/モデル/プログラム ----
 */

/// @brief `[[work_offset]]`の1要素を書く
/// @param spec 出力するワークオフセット (内部単位)
/// @param kinds 軸名→回転軸かの表
/// @param ctx 出力の文脈
/// @throw std::invalid_argument `values`に機械に無い軸名を含む場合
///        (`MakeAxisTable`から伝播)
TomlValue MakeWorkOffset(const WorkOffsetSpec& spec, const AxisKinds& kinds,
                         const WriteContext& ctx) {
    TomlValue table = Table();
    table["id"] = spec.id;
    if (!spec.description.empty()) table["description"] = spec.description;
    if (spec.from != WorkOffsetFrom::kToolMount) {
        table["from"] = std::string(WorkOffsetFromName(spec.from));
    }
    if (spec.attach != kWorkMountAttach) table["attach"] = spec.attach;
    if (const auto* values = std::get_if<NcValues>(&spec.placement);
        values != nullptr) {
        table["values"] = MakeAxisTable(
                *values, kinds, "[[work_offset]](" + spec.id + ").values",
                ctx, true);
    } else {
        const GeometricPlacement& placement =
                std::get<GeometricPlacement>(spec.placement);
        // 幾何形式には`origin`か回転のどちらかが必要.
        // 両方がデフォルト値の場合は`origin`を明示する
        if (placement.origin.isZero() && placement.rotation.isIdentity()) {
            table["origin"] = Vec3(placement.origin);
        }
        PutOrigin(table, placement.origin, ctx.length_scale);
        PutRotation(table, placement.rotation);
    }
    return table;
}

/// @brief `[[model]]`の1要素を書く
/// @param model 出力するモデル (内部単位)
/// @param ctx 出力の文脈
/// @throw std::invalid_argument STL/OBJの`file_unit_scale`がmm/inchのいずれでも
///        ない場合 (`MakeGeometry`から伝播)
/// @note 形状のキーは`MakeGeometry`で作り、`name`/`role`/`attach`を先頭に置く
TomlValue MakeModel(const ModelSpec& model, const WriteContext& ctx) {
    TomlValue table = Table();
    table["name"] = model.name;
    table["role"] = std::string(ModelRoleName(model.role));
    if (model.attach != kWorkMountAttach) table["attach"] = model.attach;
    const GeometryEntry entry{model.geometry, model.placement,
                              model.collision, model.visible};
    const TomlValue geometry = MakeGeometry(
            entry, "[[model]](" + model.name + ")", ctx,
            DefaultCollisionFor(model.role));
    for (const auto& [key, value] : geometry.as_table()) {
        if (key != "name") table[key] = value;
    }
    return table;
}

/// @brief `[[program]]`の1要素を書く
/// @param program 出力するプログラム
/// @param index `[[program]]`内の添字 (例外の文言に用いる)
/// @param ctx 出力の文脈
/// @throw std::invalid_argument ライブラリ参照のプログラム、または`file`参照で
///        `raw`/`resolved`とも空の場合
TomlValue MakeProgram(const ProgramSpec& program, const std::size_t index,
                      const WriteContext& ctx) {
    const std::string context = "[[program]][" + std::to_string(index) + "]";
    if (program.file.from_library) {
        throw std::invalid_argument(
                context + ": program cannot be a library reference");
    }
    TomlValue table = Table();
    PutFileReference(table, program.file, context, ctx);
    if (!program.name.empty()) table["name"] = program.name;
    if (!program.enabled) table["enabled"] = false;
    if (program.type != ProgramType::kGcode) {
        table["type"] = std::string(ProgramTypeName(program.type));
    }
    if (program.encoding != TextEncoding::kUtf8) {
        table["encoding"] = std::string(TextEncodingName(program.encoding));
    }
    if (program.newline.has_value()) {
        table["newline"] = std::string(NewlineStyleName(*program.newline));
    }
    if (program.unit.has_value() && program.type != ProgramType::kGcode) {
        table["unit"] = std::string(LengthUnitName(*program.unit));
    }
    if (program.start_line != 1) table["start_line"] = program.start_line;
    if (program.end_line.has_value()) table["end_line"] = *program.end_line;
    if (!program.block_skip.empty()) {
        TomlArray switches;
        for (const int number : program.block_skip) {
            switches.push_back(TomlValue(number));
        }
        table["block_skip"] = OnelineArray(std::move(switches));
    }
    if (program.tool.has_value()) table["tool"] = *program.tool;
    if (program.work_offset.has_value()) {
        table["work_offset"] = *program.work_offset;
    }
    return table;
}

/**
 * ---- セクション ----
 */

/// @brief `[project]`を書く
/// @param project 出力するプロジェクト定義
/// @note 空の任意キーは省略する
TomlValue MakeProjectMeta(const ProjectDefinition& project) {
    TomlValue table = Table();
    table["name"] = project.name;
    if (!project.description.empty()) {
        table["description"] = project.description;
    }
    if (!project.author.empty()) table["author"] = project.author;
    if (!project.modified.empty()) {
        PutDateTime(table, "modified", project.modified);
    }
    return table;
}

/// @brief `[controller]`を書く
/// @param controller 出力する制御装置の設定
/// @param ctx 出力の文脈
/// @throw std::invalid_argument ライブラリ参照で`raw`が空、または`file`参照で
///        `raw`/`resolved`とも空の場合 (`PutFileReference`から伝播)
TomlValue MakeController(
        const ControllerSpec& controller, const WriteContext& ctx) {
    TomlValue table = Table();
    PutFileReference(table, controller.file, "[controller]", ctx);
    if (!controller.disabled_codes.empty()) {
        TomlArray codes;
        for (const std::string& code : controller.disabled_codes) {
            codes.push_back(TomlValue(code));
        }
        table["disabled_codes"] = OnelineArray(std::move(codes));
    }
    return table;
}

/// @brief `[initial]`を書く
/// @param project 出力するプロジェクト定義 (内部単位)
/// @param kinds 軸名→回転軸かの表
/// @param ctx 出力の文脈
/// @return 書く内容が無ければ`std::nullopt`
/// @throw std::invalid_argument `[initial.axes]`に機械に無い軸名を含む場合
///        (`MakeAxisTable`から伝播)
std::optional<TomlValue> MakeInitial(
        const ProjectDefinition& project, const AxisKinds& kinds,
        const WriteContext& ctx) {
    if (project.initial_tool == kNoTool &&
        !project.initial_work_offset.has_value() &&
        project.initial_axes.Empty()) {
        return std::nullopt;
    }

    TomlValue table = Table();
    if (project.initial_tool != kNoTool) table["tool"] = project.initial_tool;
    if (project.initial_work_offset.has_value()) {
        table["work_offset"] = *project.initial_work_offset;
    }
    if (!project.initial_axes.Empty()) {
        table["axes"] = MakeAxisTable(
                project.initial_axes, kinds, "[initial.axes]", ctx, false);
    }
    return table;
}

/// @brief `[collision]`を書く
/// @param settings 出力する干渉チェックの設定 (内部単位)
/// @param ctx 出力の文脈
/// @note デフォルト値のキーは省略する
TomlValue MakeCollision(const ProjectCollisionSettings& settings,
                        const WriteContext& ctx) {
    TomlValue table = Table();
    if (!settings.enabled) table["enabled"] = false;
    if (settings.default_clearance != 0.0) {
        table["default_clearance"] =
                Real(settings.default_clearance / ctx.length_scale);
    }
    if (!settings.tool_pairs.empty()) {
        TomlValue pairs = TableArrayValue();
        for (const ToolPairSpec& pair : settings.tool_pairs) {
            TomlValue entry = Table();
            entry["part"] = std::string(ToolPartName(pair.part));
            entry["target"] = std::string(ModelRoleName(pair.target));
            if (pair.enabled !=
                DefaultToolPairEnabled(pair.part, pair.target)) {
                entry["enabled"] = pair.enabled;
            }
            if (pair.clearance.has_value()) {
                entry["clearance"] = Real(*pair.clearance / ctx.length_scale);
            }
            pairs.push_back(entry);
        }
        table["tool_pair"] = pairs;
    }
    if (!settings.machine_pairs.empty()) {
        TomlValue pairs = TableArrayValue();
        for (const MachinePairOverride& pair : settings.machine_pairs) {
            TomlValue entry = Table();
            entry["targets"] = NamePair(pair.targets);
            if (pair.subtree[0] || pair.subtree[1]) {
                entry["subtree"] = BoolPair(pair.subtree);
            }
            if (pair.clearance.has_value()) {
                entry["clearance"] = Real(*pair.clearance / ctx.length_scale);
            }
            if (pair.enabled.has_value()) entry["enabled"] = *pair.enabled;
            pairs.push_back(entry);
        }
        table["machine_pair"] = pairs;
    }
    return table;
}

/// @brief `[run]`を書く
/// @param run 出力する実行制御の設定
/// @param ctx 出力の文脈
/// @return 全てデフォルト値なら`std::nullopt`
std::optional<TomlValue> MakeRun(
        const RunSettings& run, const WriteContext& ctx) {
    const RunOutput& output = run.output;
    const bool has_output = output.dir.has_value() || output.log.has_value() ||
                            output.report.has_value() ||
                            output.cut_stock.has_value();
    if (!run.start_tool.has_value() && !run.stop_tool.has_value() &&
        run.overtravel == OvertravelPolicy::kError &&
        run.collision == CollisionPolicy::kWarning && !has_output) {
        return std::nullopt;
    }

    TomlValue table = Table();
    if (run.start_tool.has_value()) table["start_tool"] = *run.start_tool;
    if (run.stop_tool.has_value()) table["stop_tool"] = *run.stop_tool;
    if (run.overtravel != OvertravelPolicy::kError) {
        table["overtravel"] = std::string(OvertravelPolicyName(run.overtravel));
    }
    if (run.collision != CollisionPolicy::kWarning) {
        table["collision"] = std::string(CollisionPolicyName(run.collision));
    }
    if (has_output) {
        TomlValue out = Table();
        if (output.dir.has_value()) {
            out["dir"] = PathText(output.dir->raw, output.dir->resolved, ctx);
        }
        if (output.log.has_value()) out["log"] = *output.log;
        if (output.report.has_value()) out["report"] = *output.report;
        if (output.cut_stock.has_value()) out["cut_stock"] = *output.cut_stock;
        table["output"] = out;
    }
    return table;
}

/// @brief テーブル配列のセクションを書き込む
/// @param[out] root 書き込み先のルートテーブル
/// @param key セクション名
/// @param elements 出力する要素の並び
/// @param make 要素とその添字からテーブルを作る関数
/// @note `elements`が空ならセクションを書かない
template <typename T, typename MakeElement>
void PutTableArray(TomlValue& root, const std::string& key,
                   const std::vector<T>& elements, const MakeElement& make) {
    if (elements.empty()) return;

    TomlValue array = TableArrayValue();
    for (std::size_t i = 0; i < elements.size(); ++i) {
        array.push_back(make(elements[i], i));
    }
    root[key] = array;
}

}  // namespace


std::string FormatProject(const ProjectDefinition& project,
                          const std::filesystem::path& base_dir) {
    const WriteContext ctx =
            MakeContext(project.source_dir, project.units, base_dir);
    const AxisKinds kinds = CollectAxisKinds(project.machine);
    TomlValue root = Table();
    root["format"] = MakeFormat(kProjectFormatName, kProjectFormatVersion);
    root["project"] = MakeProjectMeta(project);
    root["units"] = MakeUnits(project.units);
    TomlValue machine = Table();
    PutFileReference(machine, project.machine_ref, "[machine]", ctx);
    root["machine"] = machine;
    if (project.controller.has_value()) {
        root["controller"] = MakeController(*project.controller, ctx);
    }
    PutTableArray(root, "tool_library", project.tool_libraries,
                  [&ctx](const ToolLibrarySpec& library,
                         const std::size_t index) {
                      return MakeToolLibrary(library, index, ctx);
                  });
    PutTableArray(root, "tool", project.tools,
                  [&ctx](const ToolEntry& entry, std::size_t) {
                      return MakeTool(entry, ctx);
                  });
    PutTableArray(root, "tool_offset", project.tool_offsets,
                  [&ctx](const ToolOffsetEntry& entry, std::size_t) {
                      return MakeToolOffset(entry, ctx);
                  });
    PutTableArray(root, "work_offset", project.work_offsets,
                  [&ctx, &kinds](const WorkOffsetSpec& spec, std::size_t) {
                      return MakeWorkOffset(spec, kinds, ctx);
                  });
    PutTableArray(root, "model", project.models,
                  [&ctx](const ModelSpec& model, std::size_t) {
                      return MakeModel(model, ctx);
                  });
    PutTableArray(root, "program", project.programs,
                  [&ctx](const ProgramSpec& program, const std::size_t index) {
                      return MakeProgram(program, index, ctx);
                  });
    if (const auto initial = MakeInitial(project, kinds, ctx);
        initial.has_value()) {
        root["initial"] = *initial;
    }
    if (project.collision.has_value()) {
        root["collision"] = MakeCollision(*project.collision, ctx);
    }
    if (const auto run = MakeRun(project.run, ctx); run.has_value()) {
        root["run"] = *run;
    }
    for (const auto& [key, opaque] : project.retained) {
        root[key] = FromOpaque(key, opaque);
    }
    return kHeaderComment + toml::format(root);
}

}  // namespace igesio::extensions::machines::detail
