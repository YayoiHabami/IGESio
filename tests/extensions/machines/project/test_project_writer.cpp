/**
 * @file tests/extensions/machines/project/test_project_writer.cpp
 * @brief プロジェクト定義の書き出し (project/project_io) のテスト
 * @author Yayoi Habami
 * @date 2026-09-12
 * @copyright 2026 Yayoi Habami
 * @note 対象: WriteProject / WriteProjectToString
 *       - 正常系 (往復): `sample.toml`を読込→書き出し→再読込して全フィールドと
 *         `retained`が一致し警告0件であること、C++で組み立てた定義 (ライブラリ参照
 *         工具・幾何形式ワークオフセット・入れ子モデル・`machine_pair`・inch/rad宣言)
 *         の往復
 *       - 正常系 (出力形式): 常に書くセクション、単位の差し替え出力、`file`/`library`の
 *         復元と相対化、軸値の宣言単位、既定値の省略、`retained`の末尾配置、
 *         日時リテラル
 *       - 異常系: 機械に無い軸名・`raw`空のライブラリ参照・ライブラリ参照の
 *         プログラムの`invalid_argument`、出力先がディレクトリの`FileOpenError`
 *       TODO: 退化ケース (工具・モデル等が全て空の定義) は`Omission_Defaults`の
 *             最小構成で兼ねる
 */
#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/rotation.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/machine/machine_definition.h"
#include "igesio/extensions/machines/machine/machine_io.h"
#include "igesio/extensions/machines/project/project_definition.h"
#include "igesio/extensions/machines/project/project_io.h"
#include "igesio/extensions/machines/tools/tool_assembly.h"
#include "igesio/extensions/machines/tools/tool_profile.h"
#include "../machine/machines_for_testing.h"
#include "./projects_for_testing.h"

namespace {

namespace fs = std::filesystem;
namespace mc = igesio::extensions::machines;
using igesio::Matrix4d;
using igesio::Vector3d;
using mc::ToRadians;
using machines_test::kFixturePath;
using projects_test::DefaultOptions;
using projects_test::kMachinesDir;
using projects_test::kProjectsDir;
using projects_test::MinimalProject;
using projects_test::ReadProjectText;
using projects_test::Replace;
using projects_test::RoundTrip;

/// @brief 往復比較の許容誤差 (単位換算の往復とdouble文字列化の丸めを含む)
constexpr double kTol = 1e-9;

/// @brief 文字列に部分文字列が含まれるか
bool Contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

/// @brief 任意の実数が両方とも無いか、両方あって一致することを検証する
void ExpectSameOptional(const std::optional<double>& expected,
                        const std::optional<double>& actual) {
    ASSERT_EQ(expected.has_value(), actual.has_value());
    if (expected.has_value()) EXPECT_NEAR(*expected, *actual, kTol);
}

/// @brief ファイル参照の一致を検証する
/// @note 期待側の`raw`が空 (C++で組み立てた参照) なら、書き出しで相対化された
///       `raw`は比較せず解決済みパスのみ比べる
void ExpectSameReference(const mc::FileReference& expected,
                         const mc::FileReference& actual) {
    if (!expected.raw.empty()) EXPECT_EQ(expected.raw, actual.raw);
    EXPECT_EQ(expected.from_library, actual.from_library);
    EXPECT_EQ(expected.resolved.lexically_normal(), actual.resolved.lexically_normal());
}

/// @brief 工具エントリの一致を検証する
void ExpectSameTool(const mc::ToolEntry& expected, const mc::ToolEntry& actual) {
    EXPECT_EQ(expected.number, actual.number);
    EXPECT_EQ(expected.name, actual.name);
    ExpectSameOptional(expected.gauge_length, actual.gauge_length);
    EXPECT_EQ(expected.control_point, actual.control_point);
    ASSERT_EQ(expected.source.index(), actual.source.index());
    if (const auto* simple = std::get_if<mc::SimpleToolSpec>(&expected.source);
        simple != nullptr) {
        const auto& other = std::get<mc::SimpleToolSpec>(actual.source);
        EXPECT_EQ(simple->cutter, other.cutter);
        EXPECT_EQ(simple->command_point, other.command_point);
        EXPECT_NEAR(simple->diameter, other.diameter, kTol);
        EXPECT_NEAR(simple->corner_radius, other.corner_radius, kTol);
        EXPECT_NEAR(simple->cutting_length, other.cutting_length, kTol);
        EXPECT_NEAR(simple->tool_length, other.tool_length, kTol);
        EXPECT_NEAR(simple->overhang, other.overhang, kTol);
        EXPECT_NEAR(simple->holder_diameter, other.holder_diameter, kTol);
        EXPECT_NEAR(simple->holder_length, other.holder_length, kTol);
    } else {
        const auto& ref = std::get<mc::LibraryToolRef>(expected.source);
        const auto& other = std::get<mc::LibraryToolRef>(actual.source);
        EXPECT_EQ(ref.source, other.source);
        EXPECT_EQ(ref.assembly, other.assembly);
    }
}

/// @brief ワークオフセットの一致を検証する
void ExpectSameWorkOffset(const mc::WorkOffsetSpec& expected,
                          const mc::WorkOffsetSpec& actual) {
    EXPECT_EQ(expected.id, actual.id);
    EXPECT_EQ(expected.description, actual.description);
    EXPECT_EQ(expected.from, actual.from);
    EXPECT_EQ(expected.attach, actual.attach);
    ASSERT_EQ(expected.placement.index(), actual.placement.index());
    if (const auto* values = std::get_if<mc::NcValues>(&expected.placement);
        values != nullptr) {
        const auto& other = std::get<mc::NcValues>(actual.placement);
        ASSERT_EQ(values->Size(), other.Size());
        for (const mc::NcEntry& entry : values->Entries()) {
            EXPECT_NEAR(entry.value, other.At(entry.register_name), kTol);
        }
    } else {
        const auto& placement = std::get<mc::GeometricPlacement>(expected.placement);
        const auto& other = std::get<mc::GeometricPlacement>(actual.placement);
        EXPECT_TRUE(placement.origin.isApprox(other.origin, kTol));
        EXPECT_TRUE(placement.rotation.isApprox(other.rotation, kTol));
    }
}

/// @brief 形状の一致を検証する
void ExpectSameGeometry(const mc::GeometrySpec& expected, const mc::GeometrySpec& actual) {
    EXPECT_EQ(expected.name, actual.name);
    ASSERT_EQ(expected.source.index(), actual.source.index());
    if (const auto* path = std::get_if<fs::path>(&expected.source); path != nullptr) {
        EXPECT_EQ(path->lexically_normal(),
                  std::get<fs::path>(actual.source).lexically_normal());
        EXPECT_EQ(expected.raw_path, actual.raw_path);
    } else {
        const auto& primitive = std::get<mc::PrimitiveSpec>(expected.source);
        const auto& other = std::get<mc::PrimitiveSpec>(actual.source);
        EXPECT_EQ(primitive.kind, other.kind);
        EXPECT_TRUE(primitive.size.isApprox(other.size, kTol));
        EXPECT_NEAR(primitive.radius, other.radius, kTol);
        EXPECT_NEAR(primitive.height, other.height, kTol);
    }
    EXPECT_NEAR(expected.file_unit_scale, actual.file_unit_scale, kTol);
    ASSERT_EQ(expected.color.has_value(), actual.color.has_value());
    if (expected.color.has_value()) {
        EXPECT_NEAR(expected.color->r, actual.color->r, 1.0 / 255.0);
    }
    EXPECT_NEAR(static_cast<double>(expected.opacity),
                static_cast<double>(actual.opacity), 1e-6);
}

/// @brief モデルの一致を検証する
void ExpectSameModel(const mc::ModelSpec& expected, const mc::ModelSpec& actual) {
    EXPECT_EQ(expected.name, actual.name);
    EXPECT_EQ(expected.role, actual.role);
    EXPECT_EQ(expected.attach, actual.attach);
    ExpectSameGeometry(expected.geometry, actual.geometry);
    EXPECT_TRUE(expected.placement.origin.isApprox(actual.placement.origin, kTol));
    EXPECT_TRUE(expected.placement.rotation.isApprox(actual.placement.rotation, kTol));
    EXPECT_EQ(expected.collision, actual.collision);
    EXPECT_EQ(expected.visible, actual.visible);
}

/// @brief プログラムの一致を検証する
void ExpectSameProgram(const mc::ProgramSpec& expected, const mc::ProgramSpec& actual) {
    ExpectSameReference(expected.file, actual.file);
    EXPECT_EQ(expected.name, actual.name);
    EXPECT_EQ(expected.enabled, actual.enabled);
    EXPECT_EQ(expected.type, actual.type);
    EXPECT_EQ(expected.encoding, actual.encoding);
    EXPECT_EQ(expected.newline, actual.newline);
    EXPECT_EQ(expected.unit, actual.unit);
    EXPECT_EQ(expected.start_line, actual.start_line);
    EXPECT_EQ(expected.end_line, actual.end_line);
    EXPECT_EQ(expected.block_skip, actual.block_skip);
    EXPECT_EQ(expected.tool, actual.tool);
    EXPECT_EQ(expected.work_offset, actual.work_offset);
}

/// @brief 干渉設定の一致を検証する
void ExpectSameCollision(const std::optional<mc::ProjectCollisionSettings>& expected,
                         const std::optional<mc::ProjectCollisionSettings>& actual) {
    ASSERT_EQ(expected.has_value(), actual.has_value());
    if (!expected.has_value()) return;
    EXPECT_EQ(expected->enabled, actual->enabled);
    EXPECT_NEAR(expected->default_clearance, actual->default_clearance, kTol);
    ASSERT_EQ(expected->tool_pairs.size(), actual->tool_pairs.size());
    for (std::size_t i = 0; i < expected->tool_pairs.size(); ++i) {
        EXPECT_EQ(expected->tool_pairs[i].part, actual->tool_pairs[i].part);
        EXPECT_EQ(expected->tool_pairs[i].target, actual->tool_pairs[i].target);
        EXPECT_EQ(expected->tool_pairs[i].enabled, actual->tool_pairs[i].enabled);
        ExpectSameOptional(expected->tool_pairs[i].clearance, actual->tool_pairs[i].clearance);
    }
    ASSERT_EQ(expected->machine_pairs.size(), actual->machine_pairs.size());
    for (std::size_t i = 0; i < expected->machine_pairs.size(); ++i) {
        EXPECT_EQ(expected->machine_pairs[i].targets, actual->machine_pairs[i].targets);
        EXPECT_EQ(expected->machine_pairs[i].subtree, actual->machine_pairs[i].subtree);
        ExpectSameOptional(expected->machine_pairs[i].clearance,
                           actual->machine_pairs[i].clearance);
        EXPECT_EQ(expected->machine_pairs[i].enabled, actual->machine_pairs[i].enabled);
    }
}

/// @brief プロジェクト定義の全フィールドの一致を検証する (警告・行番号・source_*は除く)
void ExpectSameProject(const mc::ProjectDefinition& expected,
                       const mc::ProjectDefinition& actual) {
    EXPECT_EQ(expected.format_version, actual.format_version);
    EXPECT_EQ(expected.name, actual.name);
    EXPECT_EQ(expected.description, actual.description);
    EXPECT_EQ(expected.author, actual.author);
    EXPECT_EQ(expected.modified, actual.modified);
    EXPECT_EQ(expected.units.length_unit, actual.units.length_unit);
    EXPECT_EQ(expected.units.angle_unit, actual.units.angle_unit);
    ExpectSameReference(expected.machine_ref, actual.machine_ref);
    EXPECT_EQ(expected.machine.name, actual.machine.name);
    ASSERT_EQ(expected.controller.has_value(), actual.controller.has_value());
    if (expected.controller.has_value()) {
        ExpectSameReference(expected.controller->file, actual.controller->file);
        EXPECT_EQ(expected.controller->disabled_codes, actual.controller->disabled_codes);
    }
    ASSERT_EQ(expected.tool_libraries.size(), actual.tool_libraries.size());
    for (std::size_t i = 0; i < expected.tool_libraries.size(); ++i) {
        EXPECT_EQ(expected.tool_libraries[i].alias, actual.tool_libraries[i].alias);
        ExpectSameReference(expected.tool_libraries[i].file, actual.tool_libraries[i].file);
    }
    ASSERT_EQ(expected.tools.size(), actual.tools.size());
    for (std::size_t i = 0; i < expected.tools.size(); ++i) {
        ExpectSameTool(expected.tools[i], actual.tools[i]);
    }
    ASSERT_EQ(expected.tool_offsets.size(), actual.tool_offsets.size());
    for (std::size_t i = 0; i < expected.tool_offsets.size(); ++i) {
        EXPECT_EQ(expected.tool_offsets[i].number, actual.tool_offsets[i].number);
        EXPECT_EQ(expected.tool_offsets[i].tool, actual.tool_offsets[i].tool);
        ExpectSameOptional(expected.tool_offsets[i].length, actual.tool_offsets[i].length);
        EXPECT_NEAR(expected.tool_offsets[i].length_wear, actual.tool_offsets[i].length_wear, kTol);
        ExpectSameOptional(expected.tool_offsets[i].radius, actual.tool_offsets[i].radius);
        EXPECT_NEAR(expected.tool_offsets[i].radius_wear, actual.tool_offsets[i].radius_wear, kTol);
    }
    ASSERT_EQ(expected.work_offsets.size(), actual.work_offsets.size());
    for (std::size_t i = 0; i < expected.work_offsets.size(); ++i) {
        ExpectSameWorkOffset(expected.work_offsets[i], actual.work_offsets[i]);
    }
    ASSERT_EQ(expected.models.size(), actual.models.size());
    for (std::size_t i = 0; i < expected.models.size(); ++i) {
        ExpectSameModel(expected.models[i], actual.models[i]);
    }
    ASSERT_EQ(expected.programs.size(), actual.programs.size());
    for (std::size_t i = 0; i < expected.programs.size(); ++i) {
        ExpectSameProgram(expected.programs[i], actual.programs[i]);
    }
    EXPECT_EQ(expected.initial_tool, actual.initial_tool);
    EXPECT_EQ(expected.initial_work_offset, actual.initial_work_offset);
    ASSERT_EQ(expected.initial_axes.Size(), actual.initial_axes.Size());
    for (const mc::NcEntry& entry : expected.initial_axes.Entries()) {
        EXPECT_NEAR(entry.value, actual.initial_axes.At(entry.register_name), kTol);
    }
    ExpectSameCollision(expected.collision, actual.collision);
    EXPECT_EQ(expected.run.start_tool, actual.run.start_tool);
    EXPECT_EQ(expected.run.stop_tool, actual.run.stop_tool);
    EXPECT_EQ(expected.run.overtravel, actual.run.overtravel);
    EXPECT_EQ(expected.run.collision, actual.run.collision);
    ASSERT_EQ(expected.run.output.dir.has_value(), actual.run.output.dir.has_value());
    if (expected.run.output.dir.has_value()) {
        ExpectSameReference(*expected.run.output.dir, *actual.run.output.dir);
    }
    EXPECT_EQ(expected.run.output.log, actual.run.output.log);
    EXPECT_EQ(expected.run.output.report, actual.run.output.report);
    EXPECT_EQ(expected.run.output.cut_stock, actual.run.output.cut_stock);
    ASSERT_EQ(expected.retained.size(), actual.retained.size());
    for (std::size_t i = 0; i < expected.retained.size(); ++i) {
        EXPECT_EQ(expected.retained[i].first, actual.retained[i].first);
        EXPECT_EQ(expected.retained[i].second, actual.retained[i].second);
    }
}

/// @brief 実例機を参照する`[machine]`のライブラリ参照
mc::FileReference MachineReference() {
    mc::FileReference reference;
    reference.raw = "t-ZYX-b-AC-w.toml";
    reference.resolved = kFixturePath;
    reference.from_library = true;
    return reference;
}

/// @brief C++で組み立てた定義 (ライブラリ参照工具2本・幾何形式ワークオフセット・
///        入れ子モデル・`machine_pair`・inch/deg宣言・保持断片)
/// @note 単位換算の往復を検証するため、内部値はmm・radで与える
mc::ProjectDefinition BuiltInCpp() {
    mc::ProjectDefinition project;
    project.format_version = mc::kProjectFormatVersion;
    project.name = "built";
    project.description = "assembled in C++";
    project.author = "test";
    project.modified = "2026-09-12";
    project.units = mc::MakeUnitScales(mc::LengthUnit::kInch, mc::AngleUnit::kDegree);
    project.machine_ref = MachineReference();
    project.machine = mc::ReadMachineDefinition(kFixturePath);
    project.source_dir = kProjectsDir;

    mc::ToolLibrarySpec std_lib;
    std_lib.alias = "std";
    std_lib.file.raw = "tools/tools.json";
    std_lib.file.resolved = kMachinesDir / "tools" / "tools.json";
    std_lib.file.from_library = true;
    mc::ToolLibrarySpec local;
    local.alias = "local";
    local.file.raw = "";   // 空のrawは解決済みパスから相対化される
    local.file.resolved = kMachinesDir / "tools" / "dummy.json";
    project.tool_libraries = {std_lib, local};

    mc::ToolEntry library_tool;
    library_tool.number = 3;
    library_tool.name = "Lib 3";
    library_tool.source = mc::LibraryToolRef{"local", 7};
    library_tool.gauge_length = 254.0;
    library_tool.control_point = mc::ControlPoint::kGauge;
    mc::ToolEntry simple_tool;
    simple_tool.number = 5;
    mc::SimpleToolSpec simple;
    simple.cutter = mc::SimpleToolSpec::Cutter::kRadius;
    simple.diameter = 25.4;
    simple.corner_radius = 2.54;
    simple.cutting_length = 50.8;
    simple.tool_length = 127.0;
    simple.overhang = 101.6;
    simple.holder_diameter = 50.8;
    simple.holder_length = 76.2;
    simple_tool.source = simple;
    project.tools = {library_tool, simple_tool};

    mc::ToolOffsetEntry offset;
    offset.number = 3;
    offset.tool = 3;
    offset.radius = 12.7;
    offset.radius_wear = -0.254;
    project.tool_offsets = {offset};

    mc::WorkOffsetSpec g54;
    g54.id = "G54";
    g54.placement = mc::NcValues{{"X", 25.4}, {"C", ToRadians(90.0)}};
    mc::WorkOffsetSpec g55;
    g55.id = "G55";
    g55.description = "on the vise";
    g55.from = mc::WorkOffsetFrom::kMachine;
    g55.attach = "vise";
    g55.placement = mc::GeometricPlacement{
            Vector3d(0.0, 0.0, 50.8),
            mc::RotationAboutAxis(Vector3d::UnitZ(), ToRadians(30.0))};
    project.work_offsets = {g54, g55};

    mc::ModelSpec vise;
    vise.name = "vise";
    vise.role = mc::ModelRole::kFixture;
    vise.geometry.name = "vise";
    vise.geometry.source = kProjectsDir / "models" / "cube.stl";
    vise.geometry.raw_path = "models/cube.stl";
    vise.geometry.file_unit_scale = 1.0;   // inch宣言下でmm → unit = "mm"が書かれる
    vise.placement = mc::GeometricPlacement{
            Vector3d(25.4, 0.0, 0.0),
            mc::RotationAboutAxis(Vector3d::UnitX(), ToRadians(15.0))};
    vise.geometry.color = igesio::Color::FromRGB255(128, 128, 128);
    mc::ModelSpec stock;
    stock.name = "stock";
    stock.role = mc::ModelRole::kStock;
    stock.attach = "vise";
    stock.geometry.name = "stock";
    mc::PrimitiveSpec box;
    box.kind = mc::PrimitiveSpec::Kind::kBox;
    box.size = Vector3d(254.0, 127.0, 50.8);
    stock.geometry.source = box;
    stock.placement.origin = Vector3d(0.0, 0.0, 25.4);
    stock.geometry.opacity = 0.5f;
    stock.collision = false;   // 役割の既定 (true) と異なる → 書かれる
    mc::ModelSpec design;
    design.name = "design";
    design.role = mc::ModelRole::kDesign;
    design.attach = "stock";
    design.geometry.name = "design";
    design.geometry.source = kProjectsDir / "models" / "cube.obj";
    design.geometry.raw_path = "models/cube.obj";
    design.geometry.file_unit_scale = 25.4;
    design.collision = true;   // 役割の既定 (false) と異なる → 書かれる (警告)
    design.visible = false;
    project.models = {vise, stock, design};

    mc::ProgramSpec program;
    program.file.raw = "programs/placeholder.cl";
    program.file.resolved = kProjectsDir / "programs" / "placeholder.cl";
    program.type = mc::ProgramType::kCl;
    program.unit = mc::LengthUnit::kMillimeter;
    program.newline = mc::NewlineStyle::kCrlf;
    program.block_skip = {2, 4};
    program.tool = 5;
    program.work_offset = "G55";
    project.programs = {program};

    project.initial_tool = 3;
    project.initial_work_offset = "G55";
    project.initial_axes = mc::NcValues{{"Z", 254.0}, {"A", ToRadians(-90.0)}};

    mc::ProjectCollisionSettings collision;
    collision.enabled = false;
    collision.default_clearance = 2.54;
    mc::ToolPairSpec pair;
    pair.part = mc::ToolPart::kCutter;
    pair.target = mc::ModelRole::kStock;
    pair.enabled = true;   // 既定 (false) と異なる → 書かれる
    collision.tool_pairs = {pair};
    mc::MachinePairOverride machine_pair;
    machine_pair.targets = {"Z", "A"};
    machine_pair.subtree = {true, true};
    machine_pair.clearance = 12.7;
    machine_pair.enabled = false;
    collision.machine_pairs = {machine_pair};
    project.collision = collision;

    project.run.start_tool = 3;
    project.run.overtravel = mc::OvertravelPolicy::kWarning;
    project.run.output.dir = mc::FileReference{"out", kProjectsDir / "out", false};
    project.run.output.report = "report.json";

    // トップレベルの値はテーブルより前に書かれるので、出現順もその順にしておく
    project.retained = {{"note", mc::OpaqueToml("note = \"kept as is\"\n")},
                        {"pnf", mc::OpaqueToml("[pnf]\nfile = \"pnf/x.pnf\"\ntool = 3\n")}};
    return project;
}

}  // namespace



/**
 * ---- 往復 ----
 */

TEST(ProjectWriterTest, RoundTrip_SampleFile) {
    const auto original = mc::ReadProject(kProjectsDir / "sample.toml", DefaultOptions());
    const auto restored = RoundTrip(original, kProjectsDir);
    for (const auto& warning : restored.warnings) ADD_FAILURE() << mc::FormatDiagnostic(warning);
    ExpectSameProject(original, restored);
}

TEST(ProjectWriterTest, RoundTrip_LibraryRefFile) {
    const auto original = mc::ReadProject(kProjectsDir / "library_ref.toml", DefaultOptions());
    const auto restored = RoundTrip(original, kProjectsDir);
    EXPECT_TRUE(restored.warnings.empty());
    ExpectSameProject(original, restored);
}

TEST(ProjectWriterTest, RoundTrip_BuiltInCpp) {
    const auto original = BuiltInCpp();
    const auto restored = RoundTrip(original, kProjectsDir);
    // 再読込の警告は、rawが空だった参照の相対化 (`..`) とdesignの`collision = true`
    ASSERT_EQ(restored.warnings.size(), 2u);
    EXPECT_TRUE(Contains(restored.warnings[0].message, "above base directory"));
    EXPECT_TRUE(Contains(restored.warnings[1].message, "design"));
    ExpectSameProject(original, restored);
    // rawが空だった参照は相対化されたパスがrawになる
    EXPECT_EQ(restored.tool_libraries[1].file.raw, "../tools/dummy.json");
    EXPECT_EQ(restored.tool_libraries[1].file.resolved.lexically_normal(),
              (kMachinesDir / "tools" / "dummy.json").lexically_normal());
}



/**
 * ---- 出力形式 ----
 */

TEST(ProjectWriterTest, Sections_AlwaysWritten) {
    auto project = ReadProjectText(MinimalProject());
    const std::string text = mc::WriteProjectToString(project, kProjectsDir);
    EXPECT_TRUE(Contains(text, "# machining-project 1.0"));
    EXPECT_TRUE(Contains(text, "[format]\nname = \"machining-project\"\nversion = [1, 0]"));
    EXPECT_TRUE(Contains(text, "[project]\nname = \"minimal\""));
    EXPECT_TRUE(Contains(text, "[units]\nlength = \"mm\"\nangle = \"deg\""));
    EXPECT_TRUE(Contains(text, "[machine]\nlibrary = \"t-ZYX-b-AC-w.toml\""));
    EXPECT_TRUE(Contains(text, "values = {X = 0.0, Y = 180.0, Z = -250.5}"))
            << text;

    // 単位を差し替えると数値が換算される (180 mm → 7.0866... inch)
    project.units = mc::MakeUnitScales(mc::LengthUnit::kInch, mc::AngleUnit::kRadian);
    const std::string inch = mc::WriteProjectToString(project, kProjectsDir);
    EXPECT_TRUE(Contains(inch, "length = \"inch\"\nangle = \"rad\""));
    EXPECT_TRUE(Contains(inch, "Y = 7.0866141732283"));   // 180 / 25.4 (最短往復桁数)
    EXPECT_TRUE(Contains(inch, "diameter = 0.39370078740157"));
    const auto restored = mc::ReadProjectFromString(inch, kProjectsDir, DefaultOptions());
    EXPECT_NEAR(std::get<mc::SimpleToolSpec>(restored.tools[0].source).diameter, 10.0, kTol);
}

TEST(ProjectWriterTest, Paths_FileAndLibrary) {
    auto project = BuiltInCpp();
    const std::string same = mc::WriteProjectToString(project, kProjectsDir);
    EXPECT_TRUE(Contains(same, "[machine]\nlibrary = \"t-ZYX-b-AC-w.toml\""));
    EXPECT_TRUE(Contains(same, "library = \"tools/tools.json\""));
    EXPECT_TRUE(Contains(same, "file = \"../tools/dummy.json\""));
    EXPECT_TRUE(Contains(same, "file = \"models/cube.stl\""));
    EXPECT_TRUE(Contains(same, "file = \"programs/placeholder.cl\""));
    EXPECT_TRUE(Contains(same, "dir = \"out\""));

    // 他ディレクトリへ書くと`file`は解決済みパスから相対化され、`library`は変わらない
    const std::string other = mc::WriteProjectToString(project, kMachinesDir);
    EXPECT_TRUE(Contains(other, "library = \"t-ZYX-b-AC-w.toml\""));
    EXPECT_TRUE(Contains(other, "file = \"tools/dummy.json\""));
    EXPECT_TRUE(Contains(other, "file = \"projects/models/cube.stl\""));
    EXPECT_TRUE(Contains(other, "file = \"projects/programs/placeholder.cl\""));
    EXPECT_TRUE(Contains(other, "dir = \"projects/out\""));
}

TEST(ProjectWriterTest, AxisTables_DeclaredUnits) {
    const std::string text = mc::WriteProjectToString(BuiltInCpp(), kProjectsDir);
    // inch・deg宣言: 直進軸は長さ、回転軸は角度の単位で書かれる
    EXPECT_TRUE(Contains(text, "values = {X = 1.0, C = 90.0}")) << text;
    EXPECT_TRUE(Contains(text, "[initial.axes]\nZ = 10.0\nA = -90.0")) << text;
    EXPECT_TRUE(Contains(text, "gauge_length = 10.0"));
    EXPECT_TRUE(Contains(text, "corner_radius = 0.1"));
    EXPECT_TRUE(Contains(text, "origin = [0.0, 0.0, 2.0]"));
    EXPECT_TRUE(Contains(text, "clearance = 0.5"));
}

TEST(ProjectWriterTest, Omission_Defaults) {
    const auto project = ReadProjectText(MinimalProject() + R"(
[[program]]
file = "programs/placeholder.nc"

[[tool_offset]]
number = 1
tool = 1

[collision]

[[collision.tool_pair]]
part = "cutter"
target = "stock"
)");
    const std::string text = mc::WriteProjectToString(project, kProjectsDir);
    for (const char* absent : {"enabled", "control_point", "from =", "attach = \"work_mount\"",
                               "start_line", "type = \"gcode\"", "encoding", "newline",
                               "unit =", "length_wear", "radius_wear", "[run]", "[controller]",
                               "collision = true", "description", "command_point",
                               "default_clearance", "visible"}) {
        EXPECT_FALSE(Contains(text, absent)) << absent << "\n" << text;
    }
    EXPECT_TRUE(Contains(text, "[collision]"));
    EXPECT_TRUE(Contains(text, "[[collision.tool_pair]]\npart = \"cutter\"\ntarget = \"stock\""));
    EXPECT_TRUE(Contains(text, "[[work_offset]]\nid = \"G55\"\nattach = \"stock\""));
    EXPECT_TRUE(Contains(text, "[initial]\ntool = 1\nwork_offset = \"G54\""));

    // 全て既定の定義では[initial]・[collision]も書かれない
    mc::ProjectDefinition bare;
    bare.name = "bare";
    bare.machine_ref = MachineReference();
    bare.machine = mc::ReadMachineDefinition(kFixturePath);
    const std::string minimal = mc::WriteProjectToString(bare, kProjectsDir);
    EXPECT_FALSE(Contains(minimal, "[initial]"));
    EXPECT_FALSE(Contains(minimal, "[collision]"));
    EXPECT_FALSE(Contains(minimal, "[["));
    const auto restored = mc::ReadProjectFromString(minimal, kProjectsDir, DefaultOptions());
    EXPECT_TRUE(restored.work_offsets.empty());
    EXPECT_TRUE(restored.warnings.empty());
}

TEST(ProjectWriterTest, Retained_WrittenBack) {
    const auto original = mc::ReadProject(kProjectsDir / "sample.toml", DefaultOptions());
    const std::string text = mc::WriteProjectToString(original, kProjectsDir);
    const std::size_t run = text.find("[run]");
    const std::size_t pnf = text.find("[pnf]");
    const std::size_t cutting = text.find("[cutting]");
    const std::size_t compare = text.find("[cutting.compare]");
    const std::size_t settings = text.find("[settings.cspace_cam]");
    ASSERT_NE(run, std::string::npos);
    ASSERT_NE(pnf, std::string::npos);
    ASSERT_NE(settings, std::string::npos);
    EXPECT_LT(run, pnf);
    EXPECT_LT(pnf, cutting);
    EXPECT_LT(cutting, compare);
    EXPECT_LT(compare, settings);
    EXPECT_TRUE(Contains(text, "\"desc\": \"C-Space設定\""));
    const auto restored = mc::ReadProjectFromString(text, kProjectsDir, DefaultOptions());
    ASSERT_EQ(restored.retained.size(), original.retained.size());
    for (std::size_t i = 0; i < original.retained.size(); ++i) {
        EXPECT_EQ(restored.retained[i].second, original.retained[i].second);
    }

    // トップレベルの値は読むセクションより前に置かれる (テーブルに吸われない)
    const auto scalar = ReadProjectText("note = \"kept\"\n" + MinimalProject());
    const std::string scalar_text = mc::WriteProjectToString(scalar, kProjectsDir);
    EXPECT_LT(scalar_text.find("note = \"kept\""), scalar_text.find("[format]"));
    const auto scalar_restored =
            mc::ReadProjectFromString(scalar_text, kProjectsDir, DefaultOptions());
    ASSERT_EQ(scalar_restored.retained.size(), 1u);
    EXPECT_EQ(scalar_restored.retained[0].first, "note");
}

TEST(ProjectWriterTest, DateTime_Literal) {
    auto project = ReadProjectText(MinimalProject());
    project.modified = "2026-09-02T10:00:00+09:00";
    EXPECT_TRUE(Contains(mc::WriteProjectToString(project, kProjectsDir),
                         "modified = 2026-09-02T10:00:00+09:00\n"));
    project.modified = "2026-09-02";
    EXPECT_TRUE(Contains(mc::WriteProjectToString(project, kProjectsDir),
                         "modified = 2026-09-02\n"));
    project.modified = "yesterday";
    EXPECT_TRUE(Contains(mc::WriteProjectToString(project, kProjectsDir),
                         "modified = \"yesterday\"\n"));
    project.modified.clear();
    EXPECT_FALSE(Contains(mc::WriteProjectToString(project, kProjectsDir), "modified"));
}



/**
 * ---- 異常系 ----
 */

TEST(ProjectWriterTest, Throws_InvalidArgumentOnInexpressibleValues) {
    {
        auto project = ReadProjectText(MinimalProject());
        project.work_offsets[0].placement = mc::NcValues{{"Q", 1.0}};
        EXPECT_THROW(mc::WriteProjectToString(project, kProjectsDir), std::invalid_argument);
    }
    {
        auto project = ReadProjectText(MinimalProject());
        project.initial_axes = mc::NcValues{{"Q", 1.0}};
        EXPECT_THROW(mc::WriteProjectToString(project, kProjectsDir), std::invalid_argument);
    }
    {
        auto project = ReadProjectText(MinimalProject());
        project.machine_ref.raw.clear();   // from_library かつ raw が空
        EXPECT_THROW(mc::WriteProjectToString(project, kProjectsDir), std::invalid_argument);
    }
    {
        auto project = BuiltInCpp();
        project.programs[0].file.from_library = true;
        EXPECT_THROW(mc::WriteProjectToString(project, kProjectsDir), std::invalid_argument);
    }
    {
        auto project = BuiltInCpp();
        project.retained[0].second = mc::OpaqueToml("[pnf\nbroken");
        EXPECT_THROW(mc::WriteProjectToString(project, kProjectsDir), std::invalid_argument);
        project.retained[0].second = mc::OpaqueToml("[other]\nfile = \"x\"\n");
        EXPECT_THROW(mc::WriteProjectToString(project, kProjectsDir), std::invalid_argument);
    }
    {
        auto project = BuiltInCpp();
        project.models[0].geometry.file_unit_scale = 2.0;   // mm・inchのどちらでもない
        EXPECT_THROW(mc::WriteProjectToString(project, kProjectsDir), std::invalid_argument);
    }
}

TEST(ProjectWriterTest, WriteProject_WritesFileAndThrowsFileOpenErrorOnDirectory) {
    const fs::path dir = fs::temp_directory_path() / "igesio_project_writer_test";
    fs::create_directories(dir);
    const auto original = mc::ReadProject(kProjectsDir / "sample.toml", DefaultOptions());
    const fs::path path = dir / "sample_out.toml";
    mc::WriteProject(original, path);
    ASSERT_TRUE(fs::is_regular_file(path));
    // 別ディレクトリへ書いたので`file`は相対化されている (プログラムは存在確認に通る)
    const auto restored = mc::ReadProject(path, DefaultOptions());
    EXPECT_EQ(restored.programs[0].file.resolved.lexically_normal(),
              original.programs[0].file.resolved.lexically_normal());
    EXPECT_EQ(std::get<fs::path>(restored.models[0].geometry.source).lexically_normal(),
              std::get<fs::path>(original.models[0].geometry.source).lexically_normal());
    EXPECT_THROW(mc::WriteProject(original, dir), igesio::FileOpenError);
    fs::remove_all(dir);
}
