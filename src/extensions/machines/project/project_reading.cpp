/**
 * @file extensions/machines/project/project_reading.cpp
 * @brief TOMLからプロジェクト定義を構築する関数の実装
 * @author Yayoi Habami
 * @date 2026-09-12
 * @copyright 2026 Yayoi Habami
 * @note 読込順: `[format]` → `[project]` → `[units]` → `[machine]` (参照先の
 *       機械定義も読み込む) → `[controller]` → `[[tool_library]]` →
 *       `[[tool]]` → `[[tool_offset]]` → `[[work_offset]]` → `[[model]]` →
 *       取り付け先の名前の重複検査 → `[initial]` → `[[program]]` →
 *       `[collision]` → `[run]` → 読み込まなかったトップレベルのキーを
 *       `retained`に格納する.
 * @note 検証項目は仕様の検証規則のうち、ファイル内の整合と機械定義とを
 *       突き合わせて判定できるもの. 未知のキーおよびセクションは検出しない.
 */
#include "extensions/machines/project/project_reading.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/extensions/machines/core/formatting.h"
#include "igesio/extensions/machines/core/tolerances.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/machine/machine_io.h"
#include "igesio/extensions/machines/tools/tool_profile.h"

namespace igesio::extensions::machines::detail {

namespace {

/// @brief 読み込むトップレベルのキー
/// @note これ以外のキーは`retained`に格納する
constexpr std::array<const char*, 14> kReadSections = {
        "format", "project", "units", "machine", "controller", "tool_library",
        "tool", "tool_offset", "work_offset", "model", "program", "initial",
        "collision", "run"};

/// @brief 読込全体で共有する内容
struct ReadContext {
    /// @brief `file`キーの相対パス解決の基準ディレクトリ
    std::filesystem::path base_dir;
    /// @brief ライブラリ検索ディレクトリ
    std::vector<std::filesystem::path> library_dirs;
    /// @brief 組み立て中のプロジェクト定義 (単位、機械定義、警告の参照先)
    ProjectDefinition* project = nullptr;
};

/// @brief 機械定義の軸 (軸名→軸定義と種別)
struct MachineAxis {
    /// @brief 軸定義
    const AxisSpec* spec = nullptr;
    /// @brief 回転軸か
    bool is_rotary = false;
};

/// @brief `[[section]][i]`形式の文脈文字列を作る
/// @param section セクション名 (`"[[tool]]"`等)
/// @param index 要素の添字
std::string Indexed(const std::string& section, const std::size_t index) {
    return section + "[" + std::to_string(index) + "]";
}

/// @brief 警告を`ctx.project->warnings`に追加する
/// @param ctx 読込の文脈
/// @param context 読込箇所
/// @param message 警告の文言
/// @param line 行番号 (該当行が無ければ0)
void WarnAt(const ReadContext& ctx, const std::string& context,
            const std::string& message, const int line = 0) {
    Warn(ctx.project->warnings, context, message, line);
}

/**
 * ---- 参照の解決 ----
 */

/// @brief パスの規約を検査し、可搬でないパスを参照ごとに警告する
/// @param raw ファイルに記載されたパス
/// @param context 読込箇所
/// @param line パスの行番号
/// @param ctx 読込の文脈
void CheckReferencePath(const std::string& raw, const std::string& context,
                        const int line, const ReadContext& ctx) {
    PathIssues issues;
    CheckFilePath(raw, context, issues);
    if (issues.absolute > 0) {
        WarnAt(ctx, context, "non-portable path (absolute): " + raw, line);
    }
    if (issues.escape > 0) {
        WarnAt(ctx, context, "non-portable path (above base directory): " + raw,
               line);
    }
}

/// @brief `library`キーのパスをライブラリ検索ディレクトリから解決する
/// @param raw ファイルに記載されたパス
/// @param context 読込箇所
/// @param line パスの行番号
/// @param ctx 読込の文脈
/// @return 見つかったファイルの正規化済みパス
/// @throw igesio::DataFormatError 検索ディレクトリが無い,
///        または見つからない場合
/// @note `raw`が絶対パスの場合は検索せず、正規化して返す
std::filesystem::path ResolveLibraryPath(
        const std::string& raw, const std::string& context, const int line,
        const ReadContext& ctx) {
    // 絶対パスの場合は検索しない
    if (IsAbsolutePathString(raw)) {
        return std::filesystem::path(raw).lexically_normal();
    }

    if (ctx.library_dirs.empty()) {
        Fail(context, "no library directories to search: " + raw, line);
    }
    for (const std::filesystem::path& dir : ctx.library_dirs) {
        const std::filesystem::path candidate = (dir / raw).lexically_normal();
        if (std::filesystem::exists(candidate)) return candidate;
    }
    Fail(context, "not found in library directories: " + raw, line);
}

/// @brief `file`/`library`キーを持つテーブルからファイル参照を読む
/// @param table `file`/`library`を持ち得るテーブル
/// @param context 読込箇所
/// @param ctx 読込の文脈
/// @param allow_library `library`キーを許すか (`[[program]]`は`file`のみ)
/// @param check_exists 解決したファイルの存在を要求するか
/// @throw igesio::DataFormatError `file`/`library`の択一に反する、パスが空,
///        またはファイルが存在しない場合 (ライブラリの探索失敗は
///        `ResolveLibraryPath`から伝播)
FileReference ReadFileReference(
        const TomlValue& table, const std::string& context,
        const ReadContext& ctx, const bool allow_library,
        const bool check_exists) {
    const std::optional<std::string> file =
            OptionalString(table, "file", context);
    const std::optional<std::string> library = allow_library
            ? OptionalString(table, "library", context)
            : std::nullopt;
    if (file.has_value() == library.has_value()) {
        Fail(context, allow_library ? "specify exactly one of file and library"
                                    : "file is missing", LineOf(table));
    }
    FileReference reference;
    reference.from_library = library.has_value();
    reference.raw = reference.from_library ? *library : *file;
    const TomlValue& value =
            *Find(table, reference.from_library ? "library" : "file");
    if (reference.raw.empty()) {
        Fail(context, "invalid path: " + FormatValue(value), LineOf(value));
    }
    CheckReferencePath(reference.raw, context, LineOf(value), ctx);
    if (reference.from_library) {
        reference.resolved = ResolveLibraryPath(reference.raw, context,
                                                LineOf(value), ctx);
    } else if (IsAbsolutePathString(reference.raw)) {
        reference.resolved =
                std::filesystem::path(reference.raw).lexically_normal();
    } else {
        reference.resolved = (ctx.base_dir / reference.raw).lexically_normal();
    }
    if (check_exists && !std::filesystem::exists(reference.resolved)) {
        Fail(context, "file does not exist: " + reference.raw, LineOf(value));
    }
    return reference;
}

/**
 * ---- 機械定義との突き合わせ ----
 */

/// @brief 機械定義の軸の表を作る
/// @param machine 機械定義
std::unordered_map<std::string, MachineAxis> CollectMachineAxes(
        const MachineDefinition& machine) {
    std::unordered_map<std::string, MachineAxis> axes;
    for (const ComponentSpec& component : machine.components) {
        if (!component.axis.has_value()) continue;
        axes[component.axis->register_name] = MachineAxis{
                &*component.axis, component.type == ComponentType::kRotary};
    }
    return axes;
}

/// @brief 軸名→NC指令値のテーブル (`values`/`[initial.axes]`) を読む
/// @param table 軸名をキーとするテーブル
/// @param context 読込箇所
/// @param ctx 読込の文脈
/// @param check_limits `limits`内であることを要求するか
/// @throw igesio::DataFormatError テーブルでない、機械に無い軸名、実数でない値,
///        または`limits`外の場合
/// @note 軸の種別 (回転/直進) に応じて内部単位に換算する
NcValues ReadAxisTable(const TomlValue& table, const std::string& context,
                       const ReadContext& ctx, const bool check_limits) {
    EnsureTable(table, context + ": not a table of axis values");
    const auto axes = CollectMachineAxes(ctx.project->machine);
    NcValues values;
    for (const auto& [name, raw] : table.as_table()) {
        const auto it = axes.find(name);
        if (it == axes.end()) {
            Fail(context, name + " is not an axis of the machine", LineOf(raw));
        }
        const double scale = it->second.is_rotary ? ctx.project->units.angle
                                                  : ctx.project->units.length;
        const double value = AsReal(raw, context + "." + name) * scale;
        const auto& limits = it->second.spec->limits;
        if (check_limits && limits.has_value() &&
            (value < (*limits)[0] - kLimitTolerance ||
             value > (*limits)[1] + kLimitTolerance)) {
            Fail(context, name + " is outside limits: " + FormatValue(raw),
                 LineOf(raw));
        }
        values.Set(name, value);
    }
    return values;
}

/// @brief コンポーネント名か
/// @param machine 機械定義
/// @param name 検査する名前
bool IsComponentName(
        const MachineDefinition& machine, const std::string& name) {
    return std::any_of(
            machine.components.begin(), machine.components.end(),
            [&name](const ComponentSpec& c) { return c.name == name; });
}

/// @brief 工具番号が存在することを検査する
/// @param number 工具番号
/// @param context 読込箇所
/// @param line 工具番号の行番号
/// @param ctx 読込の文脈
/// @param allow_none `kNoTool` (0) を許すか
/// @throw igesio::DataFormatError 未定義の工具番号の場合
void CheckToolNumber(
        const int number, const std::string& context, const int line,
        const ReadContext& ctx, const bool allow_none) {
    if (allow_none && number == kNoTool) return;

    if (FindTool(*ctx.project, number) == nullptr) {
        Fail(context, "tool #" + std::to_string(number) + " is not defined",
             line);
    }
}

/// @brief ワークオフセットidが存在することを検査する
/// @param id ワークオフセットid
/// @param context 読込箇所
/// @param line idの行番号
/// @param ctx 読込の文脈
/// @throw igesio::DataFormatError 未定義のidの場合
/// @note `[[work_offset]]`が無い場合は暗黙の`G54`のみ有効
void CheckWorkOffsetId(const std::string& id, const std::string& context,
                       const int line, const ReadContext& ctx) {
    const bool exists = ctx.project->work_offsets.empty()
            ? id == kImplicitWorkOffsetId
            : FindWorkOffset(*ctx.project, id) != nullptr;
    if (!exists) {
        Fail(context, "work offset \"" + id + "\" is not defined", line);
    }
}

/**
 * ---- メタ情報/機械/制御装置 ----
 */

/// @brief `[project]`を読む
/// @param root TOMLのルートテーブル
/// @param[out] project 読んだ内容の格納先
/// @throw igesio::DataFormatError `[project]`が無い、テーブルでない,
///        または`name`が無い場合
void ReadProjectMeta(const TomlValue& root, ProjectDefinition& project) {
    const TomlValue* table = Find(root, "project");
    if (table == nullptr) Fail("", "[project].name is missing");
    EnsureTable(*table, "[project] is not a table");
    project.name = RequireString(*table, "name", "[project]");
    project.description =
            OptionalString(*table, "description", "[project]").value_or("");
    project.author = OptionalString(*table, "author", "[project]").value_or("");
    project.modified =
            ReadDateTimeText(*table, "modified", "[project]").value_or("");
}

/// @brief `[machine]`を読み、参照先の機械定義を読み込む
/// @param root TOMLのルートテーブル
/// @param ctx 読込の文脈
/// @throw igesio::DataFormatError `[machine]`が無い、参照先が存在しない,
///        または機械定義の読込に失敗した場合
/// @note 機械定義の読込エラーの文言には先頭に`"[machine]: "`を付ける.
///       機械定義の警告は読込箇所を`"machine"`として`project.warnings`に
///       転記する
void ReadMachineSection(const TomlValue& root, const ReadContext& ctx) {
    const TomlValue* table = Find(root, "machine");
    if (table == nullptr) Fail("", "[machine] is missing");
    EnsureTable(*table, "[machine] is not a table");
    ProjectDefinition& project = *ctx.project;
    project.machine_ref =
            ReadFileReference(*table, "[machine]", ctx, true, true);
    try {
        project.machine = ReadMachineDefinition(project.machine_ref.resolved);
    } catch (const igesio::DataFormatError& e) {
        throw igesio::DataFormatError("[machine]: " + std::string(e.what()));
    }
    for (const Diagnostic& diagnostic : project.machine.warnings) {
        const std::string prefix =
                diagnostic.context.empty() ? "" : diagnostic.context + ": ";
        project.warnings.push_back(Diagnostic{
                diagnostic.severity, "machine", prefix + diagnostic.message,
                diagnostic.line});
    }
}

/// @brief `[controller]`を読む
/// @param root TOMLのルートテーブル
/// @param ctx 読込の文脈
/// @return `[controller]`が無ければ`std::nullopt`
/// @throw igesio::DataFormatError `[controller]`がテーブルでない場合
///        (ファイル参照の不備は`ReadFileReference`から伝播)
std::optional<ControllerSpec> ReadController(const TomlValue& root,
                                             const ReadContext& ctx) {
    const TomlValue* table = Find(root, "controller");
    if (table == nullptr) return std::nullopt;

    EnsureTable(*table, "[controller] is not a table");
    ControllerSpec controller;
    controller.file =
            ReadFileReference(*table, "[controller]", ctx, true, true);
    controller.disabled_codes =
            OptionalStringArray(*table, "disabled_codes", "[controller]");
    return controller;
}

/**
 * ---- 工具 ----
 */

/// @brief `[[tool_library]]`を読む
/// @param root TOMLのルートテーブル
/// @param ctx 読込の文脈
/// @throw igesio::DataFormatError ライブラリが2つ以上あるのに`alias`が無い,
///        または`alias`が重複する場合 (ファイル参照の不備は
///        `ReadFileReference`から伝播)
std::vector<ToolLibrarySpec> ReadToolLibraries(const TomlValue& root,
                                               const ReadContext& ctx) {
    const std::vector<const TomlValue*> tables =
            TableArray(root, "tool_library", "[[tool_library]]");
    std::vector<ToolLibrarySpec> libraries;
    std::set<std::string> aliases;
    for (std::size_t i = 0; i < tables.size(); ++i) {
        const std::string context = Indexed("[[tool_library]]", i);
        ToolLibrarySpec library;
        library.line = LineOf(*tables[i]);
        library.alias =
                OptionalString(*tables[i], "alias", context).value_or("");
        if (library.alias.empty() && tables.size() > 1) {
            Fail(context, "alias is required when more than one tool library "
                          "is defined", library.line);
        }
        if (!library.alias.empty() && !aliases.insert(library.alias).second) {
            Fail(context, "duplicate alias: " + library.alias, library.line);
        }
        library.file = ReadFileReference(*tables[i], context, ctx, true, true);
        libraries.push_back(std::move(library));
    }
    return libraries;
}

/// @brief `[tool.simple]`を読む
/// @param table `[tool.simple]`のテーブル
/// @param context 読込箇所
/// @param ctx 読込の文脈
/// @throw igesio::DataFormatError テーブルでない、必須キーが無い,
///        未知の`cutter`/`command_point`、`radius`以外の`cutter`に
///        `corner_radius`がある、または正でない値の場合
/// @note 検証はキーの型と正値のみ. 幾何の検証は`MakeSimpleToolProfile`で行う
SimpleToolSpec ReadSimpleTool(
        const TomlValue& table, const std::string& context,
        const ReadContext& ctx) {
    EnsureTable(table, context + ": not a table");
    const double length_scale = ctx.project->units.length;
    SimpleToolSpec spec;
    const std::string cutter = RequireString(table, "cutter", context);
    const auto parsed_cutter = ParseSimpleCutter(cutter);
    if (!parsed_cutter.has_value()) {
        Fail(context, "unknown cutter: " + cutter,
             LineOf(*Find(table, "cutter")));
    }
    spec.cutter = *parsed_cutter;
    const auto positive = [&](const char* key) {
        const TomlValue* value = Find(table, key);
        if (value == nullptr) {
            Fail(context, std::string(key) + " is missing", LineOf(table));
        }
        return AsPositive(*value, context + "." + key) * length_scale;
    };
    spec.diameter = positive("diameter");
    spec.cutting_length = positive("cutting_length");
    spec.tool_length = positive("tool_length");
    spec.overhang = positive("overhang");
    if (spec.cutter == SimpleToolSpec::Cutter::kRadius) {
        spec.corner_radius = positive("corner_radius");
    } else if (Find(table, "corner_radius") != nullptr) {
        Fail(context, "corner_radius is valid only for cutter = \"radius\"",
             LineOf(*Find(table, "corner_radius")));
    }
    if (const auto text = OptionalString(table, "command_point", context);
        text.has_value()) {
        const auto point = ParseSimpleCommandPoint(*text);
        if (!point.has_value()) {
            Fail(context, "unknown command_point: " + *text,
                 LineOf(*Find(table, "command_point")));
        }
        spec.command_point = *point;
    }
    const TomlValue* holder = Find(table, "holder");
    if (holder == nullptr) Fail(context, "holder is missing", LineOf(table));
    const std::string holder_context = context + ".holder";
    EnsureTable(*holder, holder_context + ": not a table");
    const auto holder_positive = [&](const char* key) {
        const TomlValue* value = Find(*holder, key);
        if (value == nullptr) {
            Fail(holder_context, std::string(key) + " is missing",
                 LineOf(*holder));
        }
        return AsPositive(*value, holder_context + "." + key) * length_scale;
    };
    spec.holder_diameter = holder_positive("diameter");
    spec.holder_length = holder_positive("length");
    return spec;
}

/// @brief 簡易アセンブリの幾何を`MakeSimpleToolProfile`で検証し、警告を転記する
/// @param spec 簡易アセンブリの定義 (内部単位)
/// @param context 読込箇所
/// @param line `[tool.simple]`の行番号
/// @param ctx 読込の文脈
/// @throw igesio::DataFormatError 幾何が成立しない場合
void ValidateSimpleTool(const SimpleToolSpec& spec, const std::string& context,
                        const int line, const ReadContext& ctx) {
    std::vector<Diagnostic> warnings;
    try {
        MakeSimpleToolProfile(spec, &warnings);
    } catch (const std::invalid_argument& e) {
        Fail(context, e.what(), line);
    }
    for (const Diagnostic& warning : warnings) {
        WarnAt(ctx, context, warning.message, line);
    }
}

/// @brief `[[tool]]`の`source` (ライブラリ別名) を読んで検証する
/// @param table `[[tool]]`の要素のテーブル
/// @param context 読込箇所
/// @param ctx 読込の文脈
/// @return 記載された別名. 未記載なら空 (ライブラリが1つの場合のみ許す)
/// @throw igesio::DataFormatError 未記載でライブラリが1つでない,
///        または未知の別名の場合
std::string ReadToolSource(const TomlValue& table, const std::string& context,
                           const ReadContext& ctx) {
    const std::string source =
            OptionalString(table, "source", context).value_or("");
    const auto& libraries = ctx.project->tool_libraries;
    if (source.empty()) {
        if (libraries.size() != 1) {
            Fail(context,
                 libraries.empty()
                         ? "assembly requires a [[tool_library]]"
                         : "source is required when more than one tool "
                           "library is defined",
                 LineOf(table));
        }
        return source;
    }
    if (FindToolLibrary(*ctx.project, source) == nullptr) {
        Fail(context, "unknown tool library alias: " + source,
             LineOf(*Find(table, "source")));
    }
    return source;
}

/// @brief `[[tool]]`の1要素を読む
/// @param table 要素のテーブル
/// @param index `[[tool]]`内の添字 (`number`を読む前の読込箇所に用いる)
/// @param ctx 読込の文脈
/// @throw igesio::DataFormatError `number`が正でない、重複する、`assembly`と
///        `[tool.simple]`の択一に反する、`gauge_length`が正でない,
///        または未知の`control_point`の場合
///        (`ReadSimpleTool`/`ValidateSimpleTool`/`ReadToolSource`からも伝播)
ToolEntry ReadTool(const TomlValue& table, const std::size_t index,
                   const ReadContext& ctx) {
    ToolEntry entry;
    entry.line = LineOf(table);
    entry.number = RequireInteger(table, "number", Indexed("[[tool]]", index));
    const std::string context =
            "[[tool]](#" + std::to_string(entry.number) + ")";
    if (entry.number <= 0) {
        Fail(context, "number must be a positive integer", entry.line);
    }
    if (FindTool(*ctx.project, entry.number) != nullptr) {
        Fail(context, "duplicate tool number", entry.line);
    }
    entry.name = OptionalString(table, "name", context).value_or("");
    const TomlValue* assembly = Find(table, "assembly");
    const TomlValue* simple = Find(table, "simple");
    if ((assembly != nullptr) == (simple != nullptr)) {
        Fail(context, "specify exactly one of assembly and [tool.simple]",
             entry.line);
    }
    if (simple != nullptr) {
        const SimpleToolSpec spec =
                ReadSimpleTool(*simple, context + ".simple", ctx);
        ValidateSimpleTool(spec, context + ".simple", LineOf(*simple), ctx);
        entry.source = spec;
    } else {
        LibraryToolRef ref;
        ref.source = ReadToolSource(table, context, ctx);
        ref.assembly = AsInteger(*assembly, context + ".assembly");
        entry.source = ref;
    }
    if (const TomlValue* gauge = Find(table, "gauge_length");
        gauge != nullptr) {
        entry.gauge_length =
                AsPositive(*gauge, context + ".gauge_length", true) *
                ctx.project->units.length;
    }
    if (const auto text = OptionalString(table, "control_point", context);
        text.has_value()) {
        const auto point = ParseControlPoint(*text);
        if (!point.has_value()) {
            Fail(context, "unknown control_point: " + *text,
                 LineOf(*Find(table, "control_point")));
        }
        entry.control_point = *point;
    }
    return entry;
}

/// @brief `[[tool]]`を読む
/// @param root TOMLのルートテーブル
/// @param ctx 読込の文脈
/// @note 要素は読み次第`project.tools`に追加し、番号の重複検査に用いる
void ReadTools(const TomlValue& root, const ReadContext& ctx) {
    const std::vector<const TomlValue*> tables =
            TableArray(root, "tool", "[[tool]]");
    for (std::size_t i = 0; i < tables.size(); ++i) {
        ctx.project->tools.push_back(ReadTool(*tables[i], i, ctx));
    }
}

/// @brief `[[tool_offset]]`を読む
/// @param root TOMLのルートテーブル
/// @param ctx 読込の文脈
/// @throw igesio::DataFormatError `number`が正でない、重複する,
///        または`tool`が未定義の工具番号の場合
std::vector<ToolOffsetEntry> ReadToolOffsets(const TomlValue& root,
                                             const ReadContext& ctx) {
    const std::vector<const TomlValue*> tables =
            TableArray(root, "tool_offset", "[[tool_offset]]");
    const double length_scale = ctx.project->units.length;
    std::vector<ToolOffsetEntry> offsets;
    std::set<int> numbers;
    for (std::size_t i = 0; i < tables.size(); ++i) {
        const TomlValue& table = *tables[i];
        ToolOffsetEntry entry;
        entry.line = LineOf(table);
        entry.number =
                RequireInteger(table, "number", Indexed("[[tool_offset]]", i));
        const std::string context =
                "[[tool_offset]](#" + std::to_string(entry.number) + ")";
        if (entry.number <= 0) {
            Fail(context, "number must be a positive integer", entry.line);
        }
        if (!numbers.insert(entry.number).second) {
            Fail(context, "duplicate offset number", entry.line);
        }
        entry.tool = OptionalInteger(table, "tool", context);
        if (entry.tool.has_value()) {
            CheckToolNumber(*entry.tool, context, LineOf(*Find(table, "tool")),
                            ctx, false);
        }
        if (const TomlValue* value = Find(table, "length"); value != nullptr) {
            entry.length = AsReal(*value, context + ".length") * length_scale;
        }
        entry.length_wear =
                ReadRealOr(table, "length_wear", 0.0, context) * length_scale;
        if (const TomlValue* value = Find(table, "radius"); value != nullptr) {
            entry.radius = AsReal(*value, context + ".radius") * length_scale;
        }
        entry.radius_wear =
                ReadRealOr(table, "radius_wear", 0.0, context) * length_scale;
        offsets.push_back(entry);
    }
    return offsets;
}

/**
 * ---- ワークオフセット/モデル ----
 */

/// @brief `[[work_offset]]`の1要素を読む
/// @param table 要素のテーブル
/// @param index `[[work_offset]]`内の添字 (`id`を読む前の読込箇所に用いる)
/// @param ctx 読込の文脈
/// @throw igesio::DataFormatError `id`が重複する、未知の`from`、`attach`が空,
///        または`values`と`origin`/回転の択一に反する場合
///        (`values`の不備は`ReadAxisTable`から伝播)
WorkOffsetSpec ReadWorkOffset(const TomlValue& table, const std::size_t index,
                              const ReadContext& ctx) {
    WorkOffsetSpec spec;
    spec.line = LineOf(table);
    spec.id = RequireString(table, "id", Indexed("[[work_offset]]", index));
    const std::string context = "[[work_offset]](" + spec.id + ")";
    if (FindWorkOffset(*ctx.project, spec.id) != nullptr) {
        Fail(context, "duplicate id", spec.line);
    }
    spec.description =
            OptionalString(table, "description", context).value_or("");
    if (const auto text = OptionalString(table, "from", context);
        text.has_value()) {
        const auto from = ParseWorkOffsetFrom(*text);
        if (!from.has_value()) {
            Fail(context, "unknown from: " + *text,
                 LineOf(*Find(table, "from")));
        }
        spec.from = *from;
    }
    spec.attach =
            OptionalString(table, "attach", context).value_or(spec.attach);
    if (spec.attach.empty()) Fail(context, "attach is empty", spec.line);

    const TomlValue* values = Find(table, "values");
    const bool geometric =
            Find(table, "origin") != nullptr ||
            !PresentKeys(table, {kRotationKeys[0], kRotationKeys[1],
                                 kRotationKeys[2]}).empty();
    if ((values != nullptr) == geometric) {
        Fail(context, "specify exactly one of values and origin/rotation",
             spec.line);
    }
    if (values != nullptr) {
        spec.placement =
                ReadAxisTable(*values, context + ".values", ctx, false);
    } else {
        GeometricPlacement placement;
        placement.origin =
                ReadVec3Or(table, "origin", igesio::Vector3d::Zero(),
                           context) * ctx.project->units.length;
        placement.rotation =
                ReadRotation(table, context, ctx.project->units.angle);
        spec.placement = placement;
    }
    return spec;
}

/// @brief `[[work_offset]]`を読む
/// @param root TOMLのルートテーブル
/// @param ctx 読込の文脈
/// @note 要素は読み次第`project.work_offsets`に追加し、`id`の重複検査に用いる
void ReadWorkOffsets(const TomlValue& root, const ReadContext& ctx) {
    const std::vector<const TomlValue*> tables =
            TableArray(root, "work_offset", "[[work_offset]]");
    for (std::size_t i = 0; i < tables.size(); ++i) {
        ctx.project->work_offsets.push_back(ReadWorkOffset(*tables[i], i, ctx));
    }
}

/// @brief `[[model]]`の`collision`を役割ごとのデフォルト値と規則に従って決める
/// @param table 要素のテーブル
/// @param role モデルの役割
/// @param context 読込箇所
/// @param ctx 読込の文脈
/// @return `collision`が無ければ役割のデフォルト値
/// @throw igesio::DataFormatError `role = "display"`で`collision = true`の場合
/// @note `role = "design"`で`collision = true`の場合は警告する
bool ReadModelCollision(const TomlValue& table, const ModelRole role,
                        const std::string& context, const ReadContext& ctx) {
    const TomlValue* value = Find(table, "collision");
    if (value == nullptr) return DefaultCollisionFor(role);

    const bool collision = OptionalBool(table, "collision", true, context);
    if (collision && role == ModelRole::kDisplay) {
        Fail(context, "collision = true is not allowed for role = \"display\"",
             LineOf(*value));
    }
    if (collision && role == ModelRole::kDesign) {
        WarnAt(ctx, context, "collision = true for role = \"design\"",
               LineOf(*value));
    }
    return collision;
}

/// @brief `[[model]]`の1要素を読む
/// @param table 要素のテーブル
/// @param index `[[model]]`内の添字 (`name`を読む前の読込箇所に用いる)
/// @param ctx 読込の文脈
/// @param[out] issues 可搬でないパスの件数の加算先
/// @throw igesio::DataFormatError 未知の`role`、または`attach`が空の場合
///        (形状と`collision`の不備は`ReadGeometry`/`ReadModelCollision`
///        から伝播)
ModelSpec ReadModel(const TomlValue& table, const std::size_t index,
                    const ReadContext& ctx, PathIssues& issues) {
    ModelSpec model;
    model.line = LineOf(table);
    model.name = RequireString(table, "name", Indexed("[[model]]", index));
    const std::string context = "[[model]](" + model.name + ")";
    const std::string role = RequireString(table, "role", context);
    const auto parsed_role = ParseModelRole(role);
    if (!parsed_role.has_value()) {
        Fail(context, "unknown role: " + role, LineOf(*Find(table, "role")));
    }
    model.role = *parsed_role;
    model.attach =
            OptionalString(table, "attach", context).value_or(model.attach);
    if (model.attach.empty()) Fail(context, "attach is empty", model.line);

    // 形状のキーは`[[component.geometry]]`と同じ.
    // `name`も同じテーブルから読まれる
    const GeometryContext geometry_ctx{ctx.base_dir, ctx.project->units};
    std::optional<GeometryEntry> entry = ReadGeometry(
            table, context, geometry_ctx, ctx.project->warnings, issues, true);
    model.geometry = std::move(entry->geometry);
    model.placement = entry->placement;
    model.visible = entry->visible;
    model.collision = ReadModelCollision(table, model.role, context, ctx);
    return model;
}

/// @brief `[[model]]`を読む
/// @param root TOMLのルートテーブル
/// @param ctx 読込の文脈
/// @note 可搬でないパスの警告は要素ごとではなく、件数をまとめて1つ出す
std::vector<ModelSpec> ReadModels(
        const TomlValue& root, const ReadContext& ctx) {
    const std::vector<const TomlValue*> tables =
            TableArray(root, "model", "[[model]]");
    std::vector<ModelSpec> models;
    PathIssues issues;
    for (std::size_t i = 0; i < tables.size(); ++i) {
        models.push_back(ReadModel(*tables[i], i, ctx, issues));
    }
    const int total = issues.absolute + issues.escape;
    if (total > 0) {
        WarnAt(ctx, "[[model]]",
               std::to_string(total) +
               " non-portable model path(s) (absolute: " +
               std::to_string(issues.absolute) + ", above base directory: " +
               std::to_string(issues.escape) + ")");
    }
    return models;
}

/// @brief 取り付け先の名前の重複を検査する
/// @param ctx 読込の文脈
/// @throw igesio::DataFormatError モデル名またはワークオフセットidが他の
///        取り付け先の名前と重複する場合
/// @note 取り付け先の名前 (予約名/コンポーネント名/モデル名/ワークオフセットid)
///       は全体で一意でなければならない
void CheckAttachNamespace(const ReadContext& ctx) {
    const ProjectDefinition& project = *ctx.project;
    std::set<std::string> names = {std::string(kWorkMountAttach),
                                   std::string(kToolMountAttach)};
    for (const ComponentSpec& component : project.machine.components) {
        names.insert(component.name);
    }
    const auto check = [&names](const std::string& name,
                                const std::string& context, const int line) {
        if (!names.insert(name).second) {
            Fail(context,
                 "name is already used in the attach namespace: " + name, line);
        }
    };
    for (const ModelSpec& model : project.models) {
        check(model.name, "[[model]](" + model.name + ")", model.line);
    }
    for (const WorkOffsetSpec& offset : project.work_offsets) {
        check(offset.id, "[[work_offset]](" + offset.id + ")", offset.line);
    }
}

/**
 * ---- 初期状態/プログラム ----
 */

/// @brief `[initial]`を読む
/// @param root TOMLのルートテーブル
/// @param ctx 読込の文脈
/// @throw igesio::DataFormatError テーブルでない、未定義の工具番号,
///        または未定義のワークオフセットidの場合
///        (`axes`の不備は`ReadAxisTable`から伝播)
void ReadInitial(const TomlValue& root, const ReadContext& ctx) {
    const TomlValue* table = Find(root, "initial");
    if (table == nullptr) return;

    EnsureTable(*table, "[initial] is not a table");
    ProjectDefinition& project = *ctx.project;
    if (const auto tool = OptionalInteger(*table, "tool", "[initial]");
        tool.has_value()) {
        CheckToolNumber(*tool, "[initial]", LineOf(*Find(*table, "tool")),
                        ctx, true);
        project.initial_tool = *tool;
    }
    if (const auto id = OptionalString(*table, "work_offset", "[initial]");
        id.has_value()) {
        CheckWorkOffsetId(*id, "[initial]",
                          LineOf(*Find(*table, "work_offset")), ctx);
        project.initial_work_offset = *id;
    }
    if (const TomlValue* axes = Find(*table, "axes"); axes != nullptr) {
        project.initial_axes =
                ReadAxisTable(*axes, "[initial.axes]", ctx, true);
    }
}

/// @brief `[[program]]`の`block_skip`を読む
/// @param table 要素のテーブル
/// @param context 読込箇所
/// @return 昇順に並べたスイッチ番号. `block_skip`が無ければ空
/// @throw igesio::DataFormatError 整数の配列でない、1〜9の範囲外,
///        または重複する場合
std::vector<int> ReadBlockSkip(
        const TomlValue& table, const std::string& context) {
    std::vector<int> switches;
    const TomlValue* value = Find(table, "block_skip");
    if (value == nullptr) return switches;

    if (!value->is_array()) {
        Fail(context, "block_skip is not an array of integers", LineOf(*value));
    }
    for (const TomlValue& element : value->as_array()) {
        const int number = AsInteger(element, context + ".block_skip");
        if (number < 1 || number > 9) {
            Fail(context,
                 "block_skip switch must be 1..9: " + std::to_string(number),
                 LineOf(*value));
        }
        if (std::find(switches.begin(), switches.end(), number) !=
            switches.end()) {
            Fail(context,
                 "duplicate block_skip switch: " + std::to_string(number),
                 LineOf(*value));
        }
        switches.push_back(number);
    }
    std::sort(switches.begin(), switches.end());
    return switches;
}

/// @brief `[[program]]`の種別、文字コード、改行、単位を読む
/// @param table 要素のテーブル
/// @param context 読込箇所
/// @param[out] program 読んだ内容の格納先
/// @throw igesio::DataFormatError 未知の`type`/`encoding`/`newline`/`unit`,
///        または`gcode`に`unit`が指定された場合
void ReadProgramFormat(const TomlValue& table, const std::string& context,
                       ProgramSpec& program) {
    if (const auto text = OptionalString(table, "type", context);
        text.has_value()) {
        const auto type = ParseProgramType(*text);
        if (!type.has_value()) {
            Fail(context, "unknown type: " + *text,
                 LineOf(*Find(table, "type")));
        }
        program.type = *type;
    }
    if (const auto text = OptionalString(table, "encoding", context);
        text.has_value()) {
        const auto encoding = ParseTextEncoding(*text);
        if (!encoding.has_value()) {
            Fail(context, "unknown encoding: " + *text,
                 LineOf(*Find(table, "encoding")));
        }
        program.encoding = *encoding;
    }
    if (const auto text = OptionalString(table, "newline", context);
        text.has_value()) {
        const auto newline = ParseNewlineStyle(*text);
        if (!newline.has_value()) {
            Fail(context, "unknown newline: " + *text,
                 LineOf(*Find(table, "newline")));
        }
        program.newline = *newline;
    }
    if (const auto text = OptionalString(table, "unit", context);
        text.has_value()) {
        if (program.type == ProgramType::kGcode) {
            Fail(context, "unit cannot be specified for gcode (use G20/G21)",
                 LineOf(*Find(table, "unit")));
        }
        const auto unit = ParseLengthUnit(*text);
        if (!unit.has_value()) {
            Fail(context, "unknown unit: " + *text,
                 LineOf(*Find(table, "unit")));
        }
        program.unit = *unit;
    }
}

/// @brief `[[program]]`の1要素を読む
/// @param table 要素のテーブル
/// @param index `[[program]]`内の添字
/// @param ctx 読込の文脈
/// @throw igesio::DataFormatError `start_line`が1未満、`end_line`が`start_line`
///        未満、未定義の工具番号、または未定義のワークオフセットidの場合
///        (`file`/形式/`block_skip`の不備は`ReadFileReference`/
///        `ReadProgramFormat`/`ReadBlockSkip`から伝播)
ProgramSpec ReadProgram(const TomlValue& table, const std::size_t index,
                        const ReadContext& ctx) {
    const std::string context = Indexed("[[program]]", index);
    ProgramSpec program;
    program.line = LineOf(table);
    program.file = ReadFileReference(table, context, ctx, false, true);
    program.name = OptionalString(table, "name", context).value_or("");
    program.enabled = OptionalBool(table, "enabled", true, context);
    ReadProgramFormat(table, context, program);
    program.start_line =
            OptionalInteger(table, "start_line", context).value_or(1);
    if (program.start_line < 1) {
        Fail(context, "start_line must be >= 1",
             LineOf(*Find(table, "start_line")));
    }
    program.end_line = OptionalInteger(table, "end_line", context);
    if (program.end_line.has_value() &&
        *program.end_line < program.start_line) {
        Fail(context, "end_line must be >= start_line",
             LineOf(*Find(table, "end_line")));
    }
    program.block_skip = ReadBlockSkip(table, context);
    program.tool = OptionalInteger(table, "tool", context);
    if (program.tool.has_value()) {
        CheckToolNumber(*program.tool, context, LineOf(*Find(table, "tool")),
                        ctx, true);
    }
    program.work_offset = OptionalString(table, "work_offset", context);
    if (program.work_offset.has_value()) {
        CheckWorkOffsetId(*program.work_offset, context,
                          LineOf(*Find(table, "work_offset")), ctx);
    }
    return program;
}

/// @brief `[[program]]`を読む
/// @param root TOMLのルートテーブル
/// @param ctx 読込の文脈
std::vector<ProgramSpec> ReadPrograms(
        const TomlValue& root, const ReadContext& ctx) {
    const std::vector<const TomlValue*> tables =
            TableArray(root, "program", "[[program]]");
    std::vector<ProgramSpec> programs;
    for (std::size_t i = 0; i < tables.size(); ++i) {
        programs.push_back(ReadProgram(*tables[i], i, ctx));
    }
    return programs;
}

/**
 * ---- 干渉チェック/実行制御 ----
 */

/// @brief `[[collision.tool_pair]]`を読む
/// @param collision `[collision]`のテーブル
/// @param ctx 読込の文脈
/// @throw igesio::DataFormatError 未知の`part`、`target`が`stock`/`fixture`
///        以外、ペアが重複する、または`clearance`が正でない場合
std::vector<ToolPairSpec> ReadToolPairs(const TomlValue& collision,
                                        const ReadContext& ctx) {
    const std::vector<const TomlValue*> tables =
            TableArray(collision, "tool_pair", "[[collision.tool_pair]]");
    std::vector<ToolPairSpec> pairs;
    std::set<std::pair<ToolPart, ModelRole>> seen;
    for (std::size_t i = 0; i < tables.size(); ++i) {
        const TomlValue& table = *tables[i];
        const std::string context = Indexed("[[collision.tool_pair]]", i);
        ToolPairSpec pair;
        const std::string part = RequireString(table, "part", context);
        const auto parsed_part = ParseToolPart(part);
        if (!parsed_part.has_value()) {
            Fail(context, "unknown part: " + part,
                 LineOf(*Find(table, "part")));
        }
        pair.part = *parsed_part;
        const std::string target = RequireString(table, "target", context);
        const auto parsed_target = ParseModelRole(target);
        if (!parsed_target.has_value() ||
            (*parsed_target != ModelRole::kStock &&
             *parsed_target != ModelRole::kFixture)) {
            Fail(context, "target must be \"stock\" or \"fixture\": " + target,
                 LineOf(*Find(table, "target")));
        }
        pair.target = *parsed_target;
        if (!seen.insert({pair.part, pair.target}).second) {
            Fail(context, "duplicate tool pair: " + part + " x " + target,
                 LineOf(table));
        }
        pair.enabled = OptionalBool(
                table, "enabled",
                DefaultToolPairEnabled(pair.part, pair.target), context);
        if (const TomlValue* value = Find(table, "clearance");
            value != nullptr) {
            pair.clearance = AsPositive(*value, context + ".clearance", true) *
                             ctx.project->units.length;
        }
        pairs.push_back(pair);
    }
    return pairs;
}

/// @brief `[[collision.machine_pair]]`を読む
/// @param collision `[collision]`のテーブル
/// @param ctx 読込の文脈
/// @throw igesio::DataFormatError `targets`が文字列2つでない、存在しない名前,
///        `subtree`が真偽値2つでない、または`clearance`が正でない場合
/// @note 検証は名前の存在のみ. ペアの規則は`MachiningSetup`で検証する
std::vector<MachinePairOverride> ReadMachinePairs(const TomlValue& collision,
                                                  const ReadContext& ctx) {
    const std::vector<const TomlValue*> tables =
            TableArray(collision, "machine_pair", "[[collision.machine_pair]]");
    std::vector<MachinePairOverride> pairs;
    for (std::size_t i = 0; i < tables.size(); ++i) {
        const TomlValue& table = *tables[i];
        const std::string context = Indexed("[[collision.machine_pair]]", i);
        MachinePairOverride pair;
        const TomlValue* targets = Find(table, "targets");
        const bool well_formed =
                targets != nullptr && targets->is_array() &&
                targets->as_array().size() == 2 &&
                targets->as_array()[0].is_string() &&
                targets->as_array()[1].is_string();
        if (!well_formed) {
            Fail(context, "targets is not two strings", LineOf(table));
        }
        pair.targets = {targets->as_array()[0].as_string(),
                        targets->as_array()[1].as_string()};
        for (const std::string& target : pair.targets) {
            if (!IsComponentName(ctx.project->machine, target) &&
                !IsReservedCollisionTarget(target)) {
                Fail(context, "target does not exist: " + target,
                     LineOf(*targets));
            }
        }
        if (const TomlValue* subtree = Find(table, "subtree");
            subtree != nullptr) {
            const bool pair_of_bools =
                    subtree->is_array() && subtree->as_array().size() == 2 &&
                    subtree->as_array()[0].is_boolean() &&
                    subtree->as_array()[1].is_boolean();
            if (!pair_of_bools) {
                Fail(context, "subtree is not two booleans", LineOf(*subtree));
            }
            pair.subtree = {subtree->as_array()[0].as_boolean(),
                            subtree->as_array()[1].as_boolean()};
        }
        if (const TomlValue* value = Find(table, "clearance");
            value != nullptr) {
            pair.clearance = AsPositive(*value, context + ".clearance", true) *
                             ctx.project->units.length;
        }
        if (Find(table, "enabled") != nullptr) {
            pair.enabled = OptionalBool(table, "enabled", true, context);
        }
        pairs.push_back(pair);
    }
    return pairs;
}

/// @brief `[collision]`を読む
/// @param root TOMLのルートテーブル
/// @param ctx 読込の文脈
/// @return `[collision]`が無ければ`std::nullopt`
/// @throw igesio::DataFormatError テーブルでない、または`default_clearance`が
///        正でない場合 (ペアの不備は`ReadToolPairs`/`ReadMachinePairs`から伝播)
std::optional<ProjectCollisionSettings> ReadCollision(const TomlValue& root,
                                                      const ReadContext& ctx) {
    const TomlValue* table = Find(root, "collision");
    if (table == nullptr) return std::nullopt;

    EnsureTable(*table, "[collision] is not a table");
    ProjectCollisionSettings settings;
    settings.enabled = OptionalBool(*table, "enabled", true, "[collision]");
    if (const TomlValue* value = Find(*table, "default_clearance");
        value != nullptr) {
        settings.default_clearance =
                AsPositive(*value, "[collision].default_clearance", true) *
                ctx.project->units.length;
    }
    settings.tool_pairs = ReadToolPairs(*table, ctx);
    settings.machine_pairs = ReadMachinePairs(*table, ctx);
    return settings;
}

/// @brief `[run.output]`を読む
/// @param run `[run]`のテーブル
/// @param ctx 読込の文脈
/// @return `[run.output]`が無ければ全メンバ未設定
/// @throw igesio::DataFormatError テーブルでない、または`dir`が空の場合
/// @note `cut_stock`の拡張子が`.stl`/`.obj`以外の場合は警告する
RunOutput ReadRunOutput(const TomlValue& run, const ReadContext& ctx) {
    RunOutput output;
    const TomlValue* table = Find(run, "output");
    if (table == nullptr) return output;

    const std::string context = "[run.output]";
    EnsureTable(*table, context + " is not a table");
    if (const auto dir = OptionalString(*table, "dir", context);
        dir.has_value()) {
        // 出力先は存在しなくてよいので、存在を検査する`ReadFileReference`は
        // 使わず、`file`と同じ規則でパスを組み立てる
        const TomlValue& value = *Find(*table, "dir");
        if (dir->empty()) {
            Fail(context, "invalid dir: " + FormatValue(value), LineOf(value));
        }
        FileReference reference;
        reference.raw = *dir;
        CheckReferencePath(reference.raw, context, LineOf(value), ctx);
        reference.resolved = IsAbsolutePathString(reference.raw)
                ? std::filesystem::path(reference.raw).lexically_normal()
                : (ctx.base_dir / reference.raw).lexically_normal();
        output.dir = reference;
    }
    output.log = OptionalString(*table, "log", context);
    output.report = OptionalString(*table, "report", context);
    output.cut_stock = OptionalString(*table, "cut_stock", context);
    if (output.cut_stock.has_value()) {
        const GeometryFileFormat format =
                ClassifyGeometryFile(std::filesystem::path(*output.cut_stock));
        if (format != GeometryFileFormat::kStl &&
            format != GeometryFileFormat::kObj) {
            WarnAt(ctx, context,
                   "cut_stock extension is neither .stl nor .obj: " +
                   *output.cut_stock, LineOf(*Find(*table, "cut_stock")));
        }
    }
    return output;
}

/// @brief `[run]`を読む
/// @param root TOMLのルートテーブル
/// @param ctx 読込の文脈
/// @return `[run]`が無ければデフォルト値
/// @throw igesio::DataFormatError テーブルでない、未定義の工具番号,
///        または未知の`overtravel`/`collision`の場合
RunSettings ReadRun(const TomlValue& root, const ReadContext& ctx) {
    RunSettings run;
    const TomlValue* table = Find(root, "run");
    if (table == nullptr) return run;

    EnsureTable(*table, "[run] is not a table");
    for (const auto& [key, target] : {std::pair{"start_tool", &run.start_tool},
                                      std::pair{"stop_tool", &run.stop_tool}}) {
        *target = OptionalInteger(*table, key, "[run]");
        if (target->has_value()) {
            CheckToolNumber(**target, "[run]." + std::string(key),
                            LineOf(*Find(*table, key)), ctx, false);
        }
    }
    if (const auto text = OptionalString(*table, "overtravel", "[run]");
        text.has_value()) {
        const auto policy = ParseOvertravelPolicy(*text);
        if (!policy.has_value()) {
            Fail("[run]", "unknown overtravel: " + *text,
                 LineOf(*Find(*table, "overtravel")));
        }
        run.overtravel = *policy;
    }
    if (const auto text = OptionalString(*table, "collision", "[run]");
        text.has_value()) {
        const auto policy = ParseCollisionPolicy(*text);
        if (!policy.has_value()) {
            Fail("[run]", "unknown collision: " + *text,
                 LineOf(*Find(*table, "collision")));
        }
        run.collision = *policy;
    }
    run.output = ReadRunOutput(*table, ctx);
    return run;
}

/// @brief 読み込まなかったトップレベルのキーを出現順に集める
/// @param root TOMLのルートテーブル
std::vector<std::pair<std::string, OpaqueToml>>
CollectRetained(const TomlValue& root) {
    std::vector<std::pair<std::string, OpaqueToml>> retained;
    for (const auto& [key, value] : root.as_table()) {
        const bool consumed =
                std::find(kReadSections.begin(), kReadSections.end(), key) !=
                kReadSections.end();
        if (!consumed) retained.emplace_back(key, ToOpaque(key, value));
    }
    return retained;
}

}  // namespace


ProjectDefinition ReadProjectDocument(const TomlValue& root,
                                      const std::filesystem::path& base_dir,
                                      const std::string& source_name,
                                      const ReadProjectOptions& options) {
    ProjectDefinition project;
    project.source_dir = base_dir;
    project.source_name = source_name;
    const ReadContext ctx{base_dir, options.library_dirs, &project};

    project.format_version = ReadFormat(
            root, kProjectFormatName, kProjectFormatVersion, project.warnings);
    ReadProjectMeta(root, project);
    project.units = ReadUnits(root);
    ReadMachineSection(root, ctx);
    project.controller = ReadController(root, ctx);
    project.tool_libraries = ReadToolLibraries(root, ctx);
    ReadTools(root, ctx);
    project.tool_offsets = ReadToolOffsets(root, ctx);
    ReadWorkOffsets(root, ctx);
    project.models = ReadModels(root, ctx);
    CheckAttachNamespace(ctx);
    ReadInitial(root, ctx);
    project.programs = ReadPrograms(root, ctx);
    project.collision = ReadCollision(root, ctx);
    project.run = ReadRun(root, ctx);
    project.retained = CollectRetained(root);
    return project;
}

}  // namespace igesio::extensions::machines::detail
