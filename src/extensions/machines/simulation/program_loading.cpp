/**
 * @file extensions/machines/simulation/program_loading.cpp
 * @brief プロジェクト定義の`[[program]]`の読込 (`ClProgram`への変換)
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/machines/simulation/program_loading.h"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "igesio/extensions/machines/toolpath/nc_block.h"

namespace igesio::extensions::machines {

namespace {

/// @brief NCプログラムを読み込む
/// @param spec プログラムの定義
/// @param index `[[program]]`のインデックス
/// @param options 読込の設定
/// @param[in,out] state モーダル状態
/// @return CLプログラム
/// @throw igesio::FileOpenError ファイルを開けない場合
/// @throw igesio::DataFormatError 内容に不備がある場合
ClProgram LoadGcode(const ProgramSpec& spec, const int index,
                    const ProgramLoadOptions& options, NcState& state) {
    NcLexOptions lex;
    lex.start_line = spec.start_line;
    lex.end_line = spec.end_line;
    lex.block_skip = spec.block_skip;
    NcInterpretOptions interpret;
    interpret.dialect = options.dialect;
    interpret.subprogram_loader = options.subprogram_loader;
    interpret.program_index = index;
    interpret.keep_comments = options.keep_comments;
    interpret.keep_unknown_words = options.keep_unknown_words;
    return InterpretNcFile(spec.file.resolved, lex, interpret, &state);
}

/// @brief CL/APTプログラムを読み込み、先頭に状態レコードを補う
/// @param spec プログラムの定義
/// @param index `[[program]]`のインデックス
/// @param units プロジェクトの単位
/// @param[in,out] state モーダル状態 (工具番号とワークオフセットidを読込後に更新する)
/// @return CLプログラム (先頭に`ClLoadTool`/`ClSelectWorkOffset`)
/// @throw igesio::FileOpenError ファイルを開けない場合
/// @throw igesio::DataFormatError 内容に不備がある場合
ClProgram LoadCl(const ProgramSpec& spec, const int index,
                 const UnitScales& units, NcState& state) {
    ClReadOptions read;
    read.start_line = spec.start_line;
    read.end_line = spec.end_line;
    read.unit_scale = UnitScale(spec, units);
    read.program_index = index;
    ClProgram program =
            ReadClFile(spec.file.resolved, ToClFileFormat(spec.type), read);

    const bool with_sources = program.HasSources();
    const std::vector<ClRecord> head = {ClLoadTool{state.tool_number},
                                        ClSelectWorkOffset{state.work_offset_id}};
    program.records.insert(program.records.begin(), head.begin(), head.end());
    if (with_sources) {
        program.sources.insert(program.sources.begin(), head.size(),
                               SourceLocation{index, 0});
    }
    ClState cl;
    for (const ClRecord& record : program.records) cl.Apply(record);
    state.tool_number = cl.tool;
    state.work_offset_id = cl.work_offset;
    return program;
}

/// @brief 動作レコードを除く範囲を適用する
/// @param program 対象のプログラム
/// @param begin 残す範囲の先頭 (このインデックスより前の動作レコードを除く)
/// @param end 残す範囲の末尾の次 (このインデックス以降の動作レコードを除く)
void RemoveMotionOutside(ClProgram& program, const std::size_t begin,
                         const std::size_t end) {
    const bool with_sources = program.HasSources();
    std::vector<ClRecord> records;
    std::vector<SourceLocation> sources;
    for (std::size_t i = 0; i < program.records.size(); ++i) {
        if (IsMotion(program.records[i]) && (i < begin || i >= end)) continue;
        records.push_back(program.records[i]);
        if (with_sources) sources.push_back(program.sources[i]);
    }
    program.records = std::move(records);
    program.sources = std::move(sources);
}

/// @brief 指定したインデックス以降で最初に条件を満たす`ClLoadTool`を探す
/// @param program プログラム
/// @param from 探し始めるインデックス
/// @param number 工具番号
/// @param equal `true`なら`number`と一致するもの、`false`なら異なるもの
/// @return レコードのインデックス. 無ければ`std::nullopt`
std::optional<std::size_t> FindLoadTool(
        const ClProgram& program, const std::size_t from, const int number,
        const bool equal) {
    for (std::size_t i = from; i < program.records.size(); ++i) {
        const auto* load = std::get_if<ClLoadTool>(&program.records[i]);
        if (load == nullptr) continue;
        if ((load->number == number) == equal) return i;
    }
    return std::nullopt;
}

/// @brief 警告を追加する
/// @param[out] warnings 追記先 (`nullptr`なら何もしない)
/// @param context 発生個所
/// @param message 内容
void PushWarning(std::vector<Diagnostic>* warnings, const std::string& context,
                 const std::string& message) {
    if (warnings == nullptr) return;

    warnings->push_back(Diagnostic{Severity::kWarning, context, message, 0});
}

}  // namespace



ClFileFormat ToClFileFormat(const ProgramType type) {
    switch (type) {
        case ProgramType::kCl:
            return ClFileFormat::kTriplet;
        case ProgramType::kApt:
            return ClFileFormat::kApt;
        case ProgramType::kGcode:
            break;
    }
    throw std::invalid_argument("ToClFileFormat: gcode is not a CL file format");
}

NcDialect DialectForProject(const ProjectDefinition& project, const NcDialect& base) {
    NcDialect dialect = base;
    if (!project.controller.has_value()) return dialect;

    for (const std::string& code : project.controller->disabled_codes) {
        dialect.disabled_codes.insert(NormalizeCodeName(code));
    }
    return dialect;
}

std::vector<LoadedProgram> LoadPrograms(const MachiningSetup& setup,
                                        const ProgramLoadOptions& options,
                                        NcState* state,
                                        std::vector<Diagnostic>* warnings) {
    NcState local;
    NcState& current = state != nullptr ? *state : local;
    if (state == nullptr) {
        current.tool_number = setup.InitialTool();
        current.work_offset_id = setup.InitialWorkOffset();
    }

    const ProjectDefinition& project = setup.Project();
    std::vector<LoadedProgram> loaded;
    for (std::size_t i = 0; i < project.programs.size(); ++i) {
        const ProgramSpec& spec = project.programs[i];
        if (!spec.enabled) continue;
        if (spec.tool.has_value()) current.tool_number = *spec.tool;
        if (spec.work_offset.has_value()) current.work_offset_id = *spec.work_offset;

        const int index = static_cast<int>(i);
        LoadedProgram entry;
        entry.program_index = index;
        entry.program = spec.type == ProgramType::kGcode
                ? LoadGcode(spec, index, options, current)
                : LoadCl(spec, index, project.units, current);
        if (entry.program.name.empty()) entry.program.name = DisplayName(spec);
        if (warnings != nullptr) {
            for (Diagnostic warning : entry.program.warnings) {
                if (warning.context.empty()) warning.context = DisplayName(spec);
                warnings->push_back(std::move(warning));
            }
        }
        loaded.push_back(std::move(entry));
    }
    return loaded;
}

ClProgram ConcatenatePrograms(const std::vector<LoadedProgram>& programs) {
    ClProgram result;
    if (programs.size() == 1) result.name = programs.front().program.name;
    for (std::size_t p = 0; p < programs.size(); ++p) {
        const ClProgram& program = programs[p].program;
        const bool last = (p + 1 == programs.size());
        const bool with_sources = program.HasSources();
        const SourceLocation fallback{programs[p].program_index, 0};
        result.records.push_back(ClMarker{ClMarker::Kind::kOperation, program.name});
        result.sources.push_back(fallback);
        for (std::size_t i = 0; i < program.records.size(); ++i) {
            if (!last && std::holds_alternative<ClEnd>(program.records[i])) continue;
            result.records.push_back(program.records[i]);
            result.sources.push_back(with_sources ? program.sources[i] : fallback);
        }
        result.warnings.insert(result.warnings.end(), program.warnings.begin(),
                               program.warnings.end());
    }
    return result;
}

void TrimToToolRange(ClProgram& program, const RunSettings& run,
                     std::vector<Diagnostic>* warnings) {
    if (!run.start_tool.has_value() && !run.stop_tool.has_value()) return;

    std::size_t begin = 0;
    if (run.start_tool.has_value()) {
        const std::optional<std::size_t> found =
                FindLoadTool(program, 0, *run.start_tool, true);
        if (!found.has_value()) {
            PushWarning(warnings, "[run]",
                        "start_tool #" + std::to_string(*run.start_tool)
                        + " is never selected; no motion remains");
            RemoveMotionOutside(program, 0, 0);
            return;
        }
        begin = *found;
    }

    std::size_t end = program.records.size();
    if (run.stop_tool.has_value()) {
        const std::optional<std::size_t> selected =
                FindLoadTool(program, begin, *run.stop_tool, true);
        if (!selected.has_value()) {
            PushWarning(warnings, "[run]",
                        "stop_tool #" + std::to_string(*run.stop_tool)
                        + " is never selected; the program runs to the end");
        } else {
            end = FindLoadTool(program, *selected + 1, *run.stop_tool, false)
                          .value_or(program.records.size());
        }
    }
    RemoveMotionOutside(program, begin, end);
}

}  // namespace igesio::extensions::machines
