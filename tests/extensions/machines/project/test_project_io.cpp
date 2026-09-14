/**
 * @file tests/extensions/machines/project/test_project_io.cpp
 * @brief プロジェクト定義の読込 (project/project_io) のテスト
 * @author Yayoi Habami
 * @date 2026-09-12
 * @copyright 2026 Yayoi Habami
 * @note 対象: ReadProject / ReadProjectFromString
 *       (書き出し WriteProject / WriteProjectToString は`test_project_writer.cpp`)
 *       - 正常系 (実例): `sample.toml`・`library_ref.toml`が警告0件で読め、
 *         参照・工具・ワークオフセット・モデル・プログラム・保持セクションが入ること
 *       - 正常系 (要素): `modified`の表記保持、`library`の検索、機械定義の警告転記、
 *         簡易工具の単位換算、ライブラリ参照の保持、省略値の`nullopt`保持、
 *         ワークオフセットの2形式と単位換算、役割別の`collision`既定、形状の再利用、
 *         `[initial]`・`[[program]]`・`[collision]`・`[run]`の各項目、`retained`
 *       - 正常系 (境界値): `[initial.axes]`の`limits`端と許容誤差、`block_skip`の1と9
 *       - 異常系: 仕様§5.1の各項目を代表1件ずつ、例外型と識別語で検証
 *       - 警告: minor版、`overhang < cutting_length`、designの`collision`、
 *         可搬でないパス、`cut_stock`の拡張子
 *       TODO: 退化ケース (セクションが全て省略された最小構成) は
 *             `WorkOffsets_NoImplicitEntry`の空定義で兼ねる
 */
#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/formatting.h"
#include "igesio/extensions/machines/core/rotation.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/machine/machine_definition.h"
#include "igesio/extensions/machines/project/project_definition.h"
#include "igesio/extensions/machines/project/project_io.h"
#include "igesio/extensions/machines/tools/tool_assembly.h"
#include "igesio/extensions/machines/tools/tool_profile.h"
#include "../machine/machines_for_testing.h"
#include "./projects_for_testing.h"

namespace {

namespace fs = std::filesystem;
namespace mc = igesio::extensions::machines;
using igesio::Vector3d;
using mc::ToRadians;
using machines_test::MinimalXyzAc;
using projects_test::DefaultOptions;
using projects_test::kMachinesDir;
using projects_test::kProjectsDir;
using projects_test::MinimalProject;
using projects_test::ReadProjectText;
using projects_test::ReadProjectWithMachine;
using projects_test::Replace;

/// @brief 数値比較の許容誤差
constexpr double kTol = 1e-12;

/// @brief `DataFormatError`が投げられ、メッセージに指定語を含むことを検証する
void ExpectDataFormatError(const std::string& toml, const std::string& keyword) {
    try {
        ReadProjectText(toml);
        FAIL() << "DataFormatError was not thrown (expected: " << keyword << ")";
    } catch (const igesio::DataFormatError& e) {
        EXPECT_NE(std::string(e.what()).find(keyword), std::string::npos)
                << "message: " << e.what();
    }
}

/// @brief 警告がちょうど1件で、指定語を含むことを検証する
void ExpectSingleWarning(const mc::ProjectDefinition& project,
                         const std::string& keyword) {
    ASSERT_EQ(project.warnings.size(), 1u)
            << (project.warnings.empty() ? "" : project.warnings[0].message);
    EXPECT_NE(project.warnings[0].message.find(keyword), std::string::npos)
            << "message: " << project.warnings[0].message;
}

/// @brief 最小構成のプロジェクトにセクションを追記する
std::string WithSection(const std::string& section) {
    return MinimalProject() + "\n" + section;
}

/// @brief チェーン外の直進軸 (扉. レジスタ`U`) を持つ機械定義
/// @note `MinimalXyzAc`に`base`直下の`Door`を加えたもの. `U`はどちらのチェーンにも
///       属さないので、`values`・`[initial.axes]`のチェーン所属の検査に用いる
std::string MachineWithDoor() {
    return MinimalXyzAc() + R"(
[[component]]
name = "Door"
type = "linear"
parent = "base"

[component.axis]
register = "U"
direction = [0, 1, 0]
limits = [0, 500]
)";
}

/// @brief `[machine]`を持たない最小構成 (`ReadProjectWithMachine`用)
std::string MinimalWithoutMachine() {
    return Replace(MinimalProject(), "[machine]\nlibrary = \"t-ZYX-b-AC-w.toml\"\n", "");
}

/// @brief 長さ単位をinchにした最小構成
/// @note `[initial.axes]`のZ (100) はinchでは`limits`を超えるので10 (254 mm) にする
std::string WithInchUnits(const std::string& toml) {
    return Replace(Replace(toml, "[machine]", "[units]\nlength = \"inch\"\n\n[machine]"),
                   "Z = 100.0", "Z = 10.0");
}



/**
 * ---- [format]・[project] ----
 */

TEST(ProjectIoTest, Format_ThrowsDataFormatErrorWhenNameOrMajorMismatch) {
    ExpectDataFormatError(Replace(MinimalProject(), "machining-project", "cspace-project"),
                          "machining-project");
    ExpectDataFormatError(Replace(MinimalProject(), "version = [1, 0]", "version = [2, 0]"),
                          "unsupported format version");
}

TEST(ProjectIoTest, Format_WarnsWhenMinorIsNewer) {
    const auto project =
            ReadProjectText(Replace(MinimalProject(), "version = [1, 0]", "version = [1, 1]"));
    ExpectSingleWarning(project, "newer minor");
    EXPECT_EQ(project.format_version[1], 1);
}

TEST(ProjectIoTest, Project_ModifiedIsKept) {
    const auto literal = ReadProjectText(Replace(
            MinimalProject(), "name = \"minimal\"",
            "name = \"minimal\"\nmodified = 2026-09-02T10:00:00+09:00"));
    EXPECT_EQ(literal.modified, "2026-09-02T10:00:00+09:00");
    const auto text = ReadProjectText(Replace(
            MinimalProject(), "name = \"minimal\"",
            "name = \"minimal\"\nmodified = \"yesterday\""));
    EXPECT_EQ(text.modified, "yesterday");
    EXPECT_TRUE(ReadProjectText(MinimalProject()).modified.empty());
    EXPECT_EQ(ReadProjectText(MinimalProject()).name, "minimal");
    ExpectDataFormatError(Replace(MinimalProject(), "name = \"minimal\"", ""),
                          "[project]: name is missing");
}



/**
 * ---- [machine]・[controller] ----
 */

TEST(ProjectIoTest, Machine_FileAndLibraryXor) {
    ExpectDataFormatError(Replace(MinimalProject(), "library = \"t-ZYX-b-AC-w.toml\"",
                                  "library = \"t-ZYX-b-AC-w.toml\"\nfile = \"x.toml\""),
                          "exactly one of file and library");
    ExpectDataFormatError(Replace(MinimalProject(), "library = \"t-ZYX-b-AC-w.toml\"", ""),
                          "exactly one of file and library");
    const auto project = ReadProjectText(MinimalProject());
    EXPECT_TRUE(project.machine_ref.from_library);
    EXPECT_EQ(project.machine_ref.raw, "t-ZYX-b-AC-w.toml");
    EXPECT_EQ(project.machine_ref.resolved,
              (kMachinesDir / "t-ZYX-b-AC-w.toml").lexically_normal());
    EXPECT_EQ(project.machine.name, "tool-ZYX-base-AC-work");
    EXPECT_TRUE(project.warnings.empty());
}

TEST(ProjectIoTest, Machine_ThrowsDataFormatErrorWhenLibraryDirsAreEmpty) {
    EXPECT_THROW(ReadProjectText(MinimalProject(), mc::ReadProjectOptions{}),
                 igesio::DataFormatError);
    try {
        ReadProjectText(MinimalProject(), mc::ReadProjectOptions{});
    } catch (const igesio::DataFormatError& e) {
        EXPECT_NE(std::string(e.what()).find("no library directories"), std::string::npos);
    }
    ExpectDataFormatError(Replace(MinimalProject(), "t-ZYX-b-AC-w.toml", "missing.toml"),
                          "not found in library directories");
}

TEST(ProjectIoTest, Machine_FileIsResolvedFromBaseDir) {
    const auto project = ReadProjectWithMachine(MinimalXyzAc(), MinimalWithoutMachine(),
                                                "igesio_project_io_file");
    EXPECT_FALSE(project.machine_ref.from_library);
    EXPECT_EQ(project.machine_ref.raw, "machine.toml");
    EXPECT_EQ(project.machine.name, "minimal-xyz-ac");
    EXPECT_TRUE(project.warnings.empty());
}

TEST(ProjectIoTest, Machine_WarningsAreForwarded) {
    const auto project = ReadProjectWithMachine(
            Replace(MinimalXyzAc(), "version = [2, 0]", "version = [2, 1]"),
            MinimalWithoutMachine(), "igesio_project_io_warn");
    ASSERT_EQ(project.warnings.size(), 1u);
    EXPECT_EQ(project.warnings[0].context, "machine");
    EXPECT_NE(project.warnings[0].message.find("newer minor"), std::string::npos);
}

TEST(ProjectIoTest, Machine_ThrowsDataFormatErrorWithPrefixWhenDefinitionIsInvalid) {
    try {
        ReadProjectWithMachine(Replace(MinimalXyzAc(), "type = \"work_mount\"",
                                       "type = \"fixed\""),
                               MinimalWithoutMachine(), "igesio_project_io_bad");
        FAIL() << "DataFormatError was not thrown";
    } catch (const igesio::DataFormatError& e) {
        EXPECT_NE(std::string(e.what()).find("[machine]: "), std::string::npos) << e.what();
    }
}

TEST(ProjectIoTest, Controller_ReferenceAndDisabledCodes) {
    const auto project = ReadProjectText(WithSection(
            "[controller]\nlibrary = \"controllers/generic-tcp.toml\"\n"
            "disabled_codes = [\"G68.2\", \"G43.4\"]\n"));
    ASSERT_TRUE(project.controller.has_value());
    EXPECT_TRUE(project.controller->file.from_library);
    EXPECT_EQ(project.controller->disabled_codes,
              (std::vector<std::string>{"G68.2", "G43.4"}));
    EXPECT_FALSE(ReadProjectText(MinimalProject()).controller.has_value());
    ExpectDataFormatError(WithSection("[controller]\nlibrary = \"controllers/none.toml\"\n"),
                          "not found in library directories");
}



/**
 * ---- [[tool_library]]・[[tool]]・[[tool_offset]] ----
 */

TEST(ProjectIoTest, Tools_SimpleIsValidatedNotResolved) {
    const auto project = ReadProjectText(MinimalProject());
    ASSERT_EQ(project.tools.size(), 1u);
    const mc::ToolEntry& tool = project.tools[0];
    EXPECT_EQ(tool.number, 1);
    EXPECT_TRUE(tool.name.empty());
    EXPECT_FALSE(tool.gauge_length.has_value());
    EXPECT_EQ(tool.control_point, mc::ControlPoint::kTip);
    ASSERT_TRUE(std::holds_alternative<mc::SimpleToolSpec>(tool.source));
    const auto& simple = std::get<mc::SimpleToolSpec>(tool.source);
    EXPECT_EQ(simple.cutter, mc::SimpleToolSpec::Cutter::kBall);
    EXPECT_NEAR(simple.diameter, 10.0, kTol);
    EXPECT_NEAR(simple.holder_length, 50.0, kTol);
    EXPECT_TRUE(project.warnings.empty());

    // inch宣言なら寸法はmmへ換算される
    const auto inch = ReadProjectText(WithInchUnits(MinimalProject()));
    EXPECT_NEAR(std::get<mc::SimpleToolSpec>(inch.tools[0].source).diameter, 254.0, kTol);
}

TEST(ProjectIoTest, Tools_ThrowsDataFormatErrorWhenSimpleGeometryIsInvalid) {
    ExpectDataFormatError(Replace(MinimalProject(), "cutting_length = 20.0",
                                  "cutting_length = 4.0"),
                          "[[tool]](#1).simple: ");
    ExpectDataFormatError(Replace(MinimalProject(), "cutter = \"ball\"",
                                  "cutter = \"square\"\ncommand_point = \"center\""),
                          "center");
    ExpectDataFormatError(Replace(MinimalProject(), "cutter = \"ball\"",
                                  "cutter = \"ball\"\ncorner_radius = 1.0"),
                          "corner_radius is valid only");
    ExpectDataFormatError(Replace(MinimalProject(), "cutter = \"ball\"",
                                  "cutter = \"radius\""),
                          "corner_radius is missing");
    ExpectDataFormatError(Replace(MinimalProject(), "cutter = \"ball\"",
                                  "cutter = \"drill\""),
                          "unknown cutter");
    ExpectDataFormatError(Replace(MinimalProject(), "diameter = 10.0", "diameter = 0"),
                          "not positive");
    ExpectDataFormatError(Replace(MinimalProject(),
                                  "[tool.simple.holder]\ndiameter = 40.0\nlength = 50.0\n", ""),
                          "holder is missing");
}

TEST(ProjectIoTest, Tools_OverhangWarning) {
    const auto project =
            ReadProjectText(Replace(MinimalProject(), "overhang = 40.0", "overhang = 10.0"));
    ExpectSingleWarning(project, "holder covers the cutting edge");
    EXPECT_EQ(project.warnings[0].context, "[[tool]](#1).simple");
}

TEST(ProjectIoTest, Tools_ThrowsDataFormatErrorWhenNumberIsInvalid) {
    ExpectDataFormatError(Replace(MinimalProject(), "number = 1", "number = 0"),
                          "positive integer");
    ExpectDataFormatError(Replace(MinimalProject(), "number = 1", "number = 1.5"),
                          "not an integer");
    ExpectDataFormatError(WithSection("[[tool]]\nnumber = 1\nassembly = 3\n"),
                          "duplicate tool number");
    ExpectDataFormatError(Replace(MinimalProject(), "number = 1", "number = 1\nassembly = 3"),
                          "exactly one of assembly and [tool.simple]");
}

TEST(ProjectIoTest, Tools_LibraryRefIsKept) {
    const auto project = mc::ReadProject(kProjectsDir / "library_ref.toml", DefaultOptions());
    EXPECT_TRUE(project.warnings.empty());
    ASSERT_EQ(project.tool_libraries.size(), 2u);
    EXPECT_EQ(project.tool_libraries[1].alias, "special");
    ASSERT_EQ(project.tools.size(), 3u);
    ASSERT_TRUE(std::holds_alternative<mc::LibraryToolRef>(project.tools[0].source));
    const auto& ref = std::get<mc::LibraryToolRef>(project.tools[0].source);
    EXPECT_EQ(ref.source, "std");
    EXPECT_EQ(ref.assembly, 2);
    EXPECT_EQ(project.tools[1].name, "Special 7");
    EXPECT_NEAR(project.tools[1].gauge_length.value_or(0.0), 150.0, kTol);
    EXPECT_EQ(project.tools[1].control_point, mc::ControlPoint::kGauge);
    EXPECT_TRUE(std::holds_alternative<mc::SimpleToolSpec>(project.tools[2].source));
    EXPECT_NE(mc::FindToolLibrary(project, "std"), nullptr);
    EXPECT_EQ(mc::FindToolLibrary(project, "none"), nullptr);
    EXPECT_NE(mc::FindTool(project, 3), nullptr);
    EXPECT_EQ(mc::FindTool(project, 4), nullptr);
}

TEST(ProjectIoTest, Tools_LibraryAliasRules) {
    const std::string two_libraries =
            "[[tool_library]]\nalias = \"a\"\nlibrary = \"tools/tools.json\"\n"
            "[[tool_library]]\nalias = \"b\"\nlibrary = \"tools/dummy.json\"\n";
    ExpectDataFormatError(WithSection(Replace(two_libraries, "alias = \"b\"\n", "")),
                          "alias is required");
    ExpectDataFormatError(WithSection(Replace(two_libraries, "alias = \"b\"", "alias = \"a\"")),
                          "duplicate alias");
    ExpectDataFormatError(WithSection(two_libraries + "[[tool]]\nnumber = 2\nassembly = 1\n"),
                          "source is required");
    ExpectDataFormatError(WithSection(two_libraries
                                      + "[[tool]]\nnumber = 2\nsource = \"c\"\nassembly = 1\n"),
                          "unknown tool library alias");
    ExpectDataFormatError(WithSection("[[tool]]\nnumber = 2\nassembly = 1\n"),
                          "requires a [[tool_library]]");
    // ライブラリ1つなら別名も`source`も省略できる
    const auto single = ReadProjectText(WithSection(
            "[[tool_library]]\nlibrary = \"tools/tools.json\"\n"
            "[[tool]]\nnumber = 2\nassembly = 1\n"));
    EXPECT_TRUE(single.tool_libraries[0].alias.empty());
    EXPECT_TRUE(std::get<mc::LibraryToolRef>(single.tools[1].source).source.empty());
}

TEST(ProjectIoTest, ToolOffsets_RawValuesKept) {
    const auto project = ReadProjectText(WithSection(
            "[[tool_offset]]\nnumber = 1\ntool = 1\n"
            "[[tool_offset]]\nnumber = 2\nlength = 120.5\nlength_wear = -0.02\n"
            "radius = 5\nradius_wear = 0.01\n"));
    ASSERT_EQ(project.tool_offsets.size(), 2u);
    EXPECT_EQ(project.tool_offsets[0].tool.value_or(0), 1);
    EXPECT_FALSE(project.tool_offsets[0].length.has_value());
    EXPECT_FALSE(project.tool_offsets[0].radius.has_value());
    EXPECT_NEAR(project.tool_offsets[0].length_wear, 0.0, kTol);
    EXPECT_FALSE(project.tool_offsets[1].tool.has_value());
    EXPECT_NEAR(project.tool_offsets[1].length.value_or(0.0), 120.5, kTol);
    EXPECT_NEAR(project.tool_offsets[1].length_wear, -0.02, kTol);
    EXPECT_NEAR(project.tool_offsets[1].radius.value_or(0.0), 5.0, kTol);
    EXPECT_NEAR(project.tool_offsets[1].radius_wear, 0.01, kTol);
}

TEST(ProjectIoTest, ToolOffsets_ThrowsDataFormatErrorWhenToolOrNumberIsInvalid) {
    ExpectDataFormatError(WithSection("[[tool_offset]]\nnumber = 1\ntool = 0\n"),
                          "tool #0 is not defined");
    ExpectDataFormatError(WithSection("[[tool_offset]]\nnumber = 1\ntool = 9\n"),
                          "tool #9 is not defined");
    ExpectDataFormatError(WithSection("[[tool_offset]]\nnumber = 1\n[[tool_offset]]\nnumber = 1\n"),
                          "duplicate offset number");
    ExpectDataFormatError(WithSection("[[tool_offset]]\nnumber = -1\n"), "positive integer");
}



/**
 * ---- [[work_offset]] ----
 */

TEST(ProjectIoTest, WorkOffsets_VariantAndKeys) {
    const auto project = ReadProjectText(MinimalProject());
    ASSERT_EQ(project.work_offsets.size(), 2u);
    const auto* values = std::get_if<mc::NcValues>(&project.work_offsets[0].placement);
    ASSERT_NE(values, nullptr);
    EXPECT_NEAR(values->At("Y"), 180.0, kTol);
    EXPECT_NEAR(values->At("Z"), -250.5, kTol);
    EXPECT_FALSE(values->Contains("A"));
    EXPECT_EQ(project.work_offsets[0].from, mc::WorkOffsetFrom::kToolMount);
    EXPECT_EQ(project.work_offsets[0].attach, "work_mount");
    const auto* geometric =
            std::get_if<mc::GeometricPlacement>(&project.work_offsets[1].placement);
    ASSERT_NE(geometric, nullptr);
    EXPECT_TRUE(geometric->origin.isApprox(Vector3d(0.0, 0.0, 20.0), kTol));
    EXPECT_TRUE(geometric->rotation.isIdentity(kTol));
    EXPECT_EQ(project.work_offsets[1].attach, "stock");
    EXPECT_NE(mc::FindWorkOffset(project, "G55"), nullptr);
    EXPECT_EQ(mc::FindWorkOffset(project, "G56"), nullptr);

    // 単位換算: 回転軸は角度、直進軸は長さの宣言単位に従う
    const auto scaled = ReadProjectText(Replace(
            WithInchUnits(MinimalProject()),
            "values = { X = 0.0, Y = 180.0, Z = -250.5 }",
            "values = { X = 1.0, C = 90.0 }\nfrom = \"machine\""));
    const auto& nc = std::get<mc::NcValues>(scaled.work_offsets[0].placement);
    EXPECT_NEAR(nc.At("X"), 25.4, kTol);
    EXPECT_NEAR(nc.At("C"), ToRadians(90.0), kTol);
    EXPECT_EQ(scaled.work_offsets[0].from, mc::WorkOffsetFrom::kMachine);
}

TEST(ProjectIoTest, WorkOffsets_OffChainAxisIsAcceptedOnRead) {
    // チェーン外の軸 (扉のU) は機械の軸なので読込では受理する (拒否はセットアップ)
    const auto project = ReadProjectWithMachine(
            MachineWithDoor(),
            Replace(MinimalWithoutMachine(), "values = { X = 0.0, Y = 180.0, Z = -250.5 }",
                    "values = { U = 10.0 }"),
            "igesio_project_io_door");
    EXPECT_NEAR(std::get<mc::NcValues>(project.work_offsets[0].placement).At("U"),
                10.0, kTol);
}

TEST(ProjectIoTest, WorkOffsets_ThrowsDataFormatErrorWhenFormIsInvalid) {
    ExpectDataFormatError(Replace(MinimalProject(), "values = { X = 0.0, Y = 180.0, Z = -250.5 }",
                                  "values = { X = 0.0 }\norigin = [0, 0, 0]"),
                          "exactly one of values and origin/rotation");
    ExpectDataFormatError(Replace(MinimalProject(), "values = { X = 0.0, Y = 180.0, Z = -250.5 }",
                                  ""),
                          "exactly one of values and origin/rotation");
    ExpectDataFormatError(Replace(MinimalProject(), "values = { X = 0.0, Y = 180.0, Z = -250.5 }",
                                  "values = { Q = 1.0 }"),
                          "Q is not an axis of the machine");
    ExpectDataFormatError(Replace(MinimalProject(), "id = \"G55\"",
                                  "id = \"G55\"\nfrom = \"spindle\""),
                          "unknown from");
    ExpectDataFormatError(Replace(MinimalProject(), "id = \"G55\"", "id = \"G54\""),
                          "duplicate id");
    ExpectDataFormatError(Replace(MinimalProject(), "origin = [0.0, 0.0, 20.0]",
                                  "origin = [0.0, 0.0, 20.0]\nrotation_euler_ijk = [0, 0, 0]\n"
                                  "rotation_axis_angle = { axis = [0, 0, 1], angle = 0 }"),
                          "multiple rotation forms");
}

TEST(ProjectIoTest, WorkOffsets_NoImplicitEntry) {
    const std::string none = Replace(
            Replace(MinimalProject(), "[[work_offset]]\nid = \"G54\"\n"
                    "values = { X = 0.0, Y = 180.0, Z = -250.5 }\n", ""),
            "[[work_offset]]\nid = \"G55\"\nattach = \"stock\"\norigin = [0.0, 0.0, 20.0]\n", "");
    const auto project = ReadProjectText(none);
    EXPECT_TRUE(project.work_offsets.empty());
    EXPECT_EQ(project.initial_work_offset.value_or(""), "G54");
    ExpectDataFormatError(Replace(none, "work_offset = \"G54\"", "work_offset = \"G55\""),
                          "work offset \"G55\" is not defined");
}



/**
 * ---- [[model]]・名前空間 ----
 */

TEST(ProjectIoTest, Models_RoleAndCollision) {
    const auto project = ReadProjectText(WithSection(
            "[[model]]\nname = \"vise\"\nrole = \"fixture\"\n"
            "primitive = \"box\"\nsize = [1, 1, 1]\n"
            "[[model]]\nname = \"cad\"\nrole = \"design\"\n"
            "primitive = \"box\"\nsize = [1, 1, 1]\n"
            "[[model]]\nname = \"deco\"\nrole = \"display\"\n"
            "primitive = \"box\"\nsize = [1, 1, 1]\n"
            "[[model]]\nname = \"off\"\nrole = \"stock\"\n"
            "primitive = \"box\"\nsize = [1, 1, 1]\ncollision = false\n"));
    ASSERT_EQ(project.models.size(), 5u);
    EXPECT_TRUE(project.models[0].collision);    // stock
    EXPECT_TRUE(project.models[1].collision);    // fixture
    EXPECT_FALSE(project.models[2].collision);   // design
    EXPECT_FALSE(project.models[3].collision);   // display
    EXPECT_FALSE(project.models[4].collision);   // 明示
    EXPECT_EQ(project.models[1].role, mc::ModelRole::kFixture);
    EXPECT_EQ(project.models[0].attach, "work_mount");
    EXPECT_EQ(project.models[0].geometry.name, "stock");
    EXPECT_TRUE(project.warnings.empty());
    EXPECT_NE(mc::FindModel(project, "deco"), nullptr);
    EXPECT_EQ(mc::FindModel(project, "none"), nullptr);

    const auto design = ReadProjectText(WithSection(
            "[[model]]\nname = \"cad\"\nrole = \"design\"\nprimitive = \"box\"\nsize = [1, 1, 1]\n"
            "collision = true\n"));
    ExpectSingleWarning(design, "collision = true for role = \"design\"");
    EXPECT_TRUE(design.models[1].collision);
}

TEST(ProjectIoTest, Models_ThrowsDataFormatErrorWhenRoleOrCollisionIsInvalid) {
    ExpectDataFormatError(WithSection("[[model]]\nname = \"deco\"\nrole = \"display\"\n"
                                      "primitive = \"box\"\nsize = [1, 1, 1]\ncollision = true\n"),
                          "not allowed for role = \"display\"");
    ExpectDataFormatError(Replace(MinimalProject(), "role = \"stock\"\n", ""), "role is missing");
    ExpectDataFormatError(Replace(MinimalProject(), "role = \"stock\"", "role = \"part\""),
                          "unknown role");
    ExpectDataFormatError(Replace(MinimalProject(), "name = \"stock\"\nrole", "role"),
                          "name is missing");
    ExpectDataFormatError(Replace(MinimalProject(), "primitive = \"box\"",
                                  "primitive = \"box\"\nfile = \"models/cube.stl\""),
                          "exactly one of file and primitive");
}

TEST(ProjectIoTest, Models_GeometryReuse) {
    const auto project = ReadProjectText(WithSection(
            "[[model]]\nname = \"vise\"\nrole = \"fixture\"\nfile = \"models/cube.stl\"\n"
            "unit = \"inch\"\norigin = [10, 20, 30]\n"
            "rotation_axis_angle = { axis = [0, 0, 1], angle = 90 }\n"
            "color = \"#808080\"\nopacity = 0.5\nvisible = false\n"));
    const mc::ModelSpec& vise = project.models[1];
    EXPECT_NEAR(vise.geometry.file_unit_scale, 25.4, kTol);
    EXPECT_EQ(std::get<fs::path>(vise.geometry.source),
              (kProjectsDir / "models" / "cube.stl").lexically_normal());
    EXPECT_EQ(vise.geometry.raw_path, "models/cube.stl");
    // 剛体変換は取り付け先座標系相対のまま (書かれた値)
    EXPECT_TRUE(vise.placement.origin.isApprox(Vector3d(10.0, 20.0, 30.0), 1e-9));
    EXPECT_TRUE(vise.placement.rotation.isApprox(
            mc::RotationAboutAxis(Vector3d::UnitZ(), ToRadians(90.0)), 1e-9));
    EXPECT_FALSE(vise.visible);
    EXPECT_TRUE(vise.collision);
    EXPECT_NEAR(static_cast<double>(vise.geometry.opacity), 0.5, 1e-6);
    ASSERT_TRUE(vise.geometry.color.has_value());
    EXPECT_NEAR(vise.geometry.color->r, 128.0 / 255.0, 1e-6);
    EXPECT_TRUE(project.warnings.empty());
}

TEST(ProjectIoTest, Models_AbsolutePathsAreCountedInOneWarning) {
    const auto project = ReadProjectText(WithSection(
            "[[model]]\nname = \"a\"\nrole = \"display\"\nfile = \"C:/models/a.stl\"\n"
            "[[model]]\nname = \"b\"\nrole = \"display\"\nfile = \"/models/b.obj\"\n"));
    ExpectSingleWarning(project, "2 non-portable model path(s)");
    EXPECT_EQ(project.warnings[0].context, "[[model]]");
}

TEST(ProjectIoTest, Models_StepIsKeptWithWarning) {
    const auto project = ReadProjectText(WithSection(
            "[[model]]\nname = \"cad\"\nrole = \"design\"\nfile = \"models/part.stp\"\n"));
    ExpectSingleWarning(project, "unsupported");
    ASSERT_EQ(project.models.size(), 2u);
    EXPECT_EQ(mc::ClassifyGeometryFile(std::get<fs::path>(project.models[1].geometry.source)),
              mc::GeometryFileFormat::kStep);
}

TEST(ProjectIoTest, Namespace_ThrowsDataFormatErrorWhenNamesCollide) {
    ExpectDataFormatError(Replace(MinimalProject(), "name = \"stock\"", "name = \"X\""),
                          "already used in the attach namespace: X");
    ExpectDataFormatError(Replace(MinimalProject(), "id = \"G55\"", "id = \"stock\""),
                          "already used in the attach namespace: stock");
    ExpectDataFormatError(Replace(MinimalProject(), "name = \"stock\"", "name = \"work_mount\""),
                          "already used in the attach namespace: work_mount");
    ExpectDataFormatError(WithSection("[[model]]\nname = \"stock\"\nrole = \"stock\"\n"
                                      "primitive = \"box\"\nsize = [1, 1, 1]\n"),
                          "already used in the attach namespace: stock");
}



/**
 * ---- [initial] ----
 */

TEST(ProjectIoTest, Initial_AxesAndDefaults) {
    const auto project = ReadProjectText(MinimalProject());
    EXPECT_EQ(project.initial_tool, 1);
    EXPECT_EQ(project.initial_work_offset.value_or(""), "G54");
    EXPECT_NEAR(project.initial_axes.At("Z"), 100.0, kTol);

    const auto defaults = ReadProjectText(Replace(
            MinimalProject(),
            "[initial]\ntool = 1\nwork_offset = \"G54\"\n\n[initial.axes]\nZ = 100.0\n", ""));
    EXPECT_EQ(defaults.initial_tool, mc::kNoTool);
    EXPECT_FALSE(defaults.initial_work_offset.has_value());
    EXPECT_TRUE(defaults.initial_axes.Empty());

    // 工具0 (工具なし) は許容、角度はradへ換算
    const auto zero = ReadProjectText(Replace(
            Replace(MinimalProject(), "tool = 1\nwork_offset", "tool = 0\nwork_offset"),
            "Z = 100.0", "Z = 100.0\nA = 45.0"));
    EXPECT_EQ(zero.initial_tool, mc::kNoTool);
    EXPECT_NEAR(zero.initial_axes.At("A"), ToRadians(45.0), kTol);
}

TEST(ProjectIoTest, Initial_OffChainAxisIsAccepted) {
    const auto project = ReadProjectWithMachine(
            MachineWithDoor(),
            Replace(MinimalWithoutMachine(), "Z = 100.0", "Z = 100.0\nU = 250.0"),
            "igesio_project_io_door_initial");
    EXPECT_NEAR(project.initial_axes.At("U"), 250.0, kTol);
}

TEST(ProjectIoTest, Initial_LimitBoundaryAndTolerance) {
    // Zの上限300: 端は受理、許容誤差 (1e-9) の内側も受理、外側は拒否
    EXPECT_NO_THROW(ReadProjectText(Replace(MinimalProject(), "Z = 100.0", "Z = 300.0")));
    EXPECT_NO_THROW(ReadProjectText(Replace(MinimalProject(), "Z = 100.0", "Z = 300.0000000001")));
    ExpectDataFormatError(Replace(MinimalProject(), "Z = 100.0", "Z = 300.001"),
                          "Z is outside limits");
}

TEST(ProjectIoTest, Initial_ThrowsDataFormatErrorWhenToolOrOffsetIsUnknown) {
    ExpectDataFormatError(Replace(MinimalProject(), "tool = 1\nwork_offset",
                                  "tool = 7\nwork_offset"),
                          "tool #7 is not defined");
    ExpectDataFormatError(Replace(MinimalProject(), "work_offset = \"G54\"",
                                  "work_offset = \"G59\""),
                          "work offset \"G59\" is not defined");
    ExpectDataFormatError(Replace(MinimalProject(), "Z = 100.0", "Q = 1.0"),
                          "Q is not an axis of the machine");
}



/**
 * ---- [[program]] ----
 */

TEST(ProjectIoTest, Programs_ReadsFieldsAndDefaults) {
    const auto project = ReadProjectText(WithSection(
            "[[program]]\nfile = \"programs/placeholder.nc\"\n"
            "[[program]]\nfile = \"programs/placeholder.cl\"\nname = \"finish\"\ntype = \"cl\"\n"
            "encoding = \"shift_jis\"\nnewline = \"crlf\"\nenabled = false\nstart_line = 5\n"
            "end_line = 9\nblock_skip = [9, 1, 3]\ntool = 0\nwork_offset = \"G55\"\n"));
    ASSERT_EQ(project.programs.size(), 2u);
    const mc::ProgramSpec& nc = project.programs[0];
    EXPECT_TRUE(nc.name.empty());
    EXPECT_EQ(mc::DisplayName(nc), "placeholder.nc");
    EXPECT_TRUE(fs::exists(nc.file.resolved));
    EXPECT_TRUE(nc.enabled);
    EXPECT_EQ(nc.type, mc::ProgramType::kGcode);
    EXPECT_EQ(nc.encoding, mc::TextEncoding::kUtf8);
    EXPECT_FALSE(nc.newline.has_value());
    EXPECT_FALSE(nc.unit.has_value());
    EXPECT_EQ(nc.start_line, 1);
    EXPECT_FALSE(nc.end_line.has_value());
    EXPECT_TRUE(nc.block_skip.empty());
    EXPECT_FALSE(nc.tool.has_value());
    EXPECT_FALSE(nc.work_offset.has_value());
    EXPECT_NEAR(mc::UnitScale(nc, project.units), 1.0, kTol);

    const mc::ProgramSpec& cl = project.programs[1];
    EXPECT_EQ(mc::DisplayName(cl), "finish");
    EXPECT_FALSE(cl.enabled);
    EXPECT_EQ(cl.type, mc::ProgramType::kCl);
    EXPECT_EQ(cl.encoding, mc::TextEncoding::kShiftJis);
    EXPECT_EQ(cl.newline.value_or(mc::NewlineStyle::kLf), mc::NewlineStyle::kCrlf);
    EXPECT_EQ(cl.start_line, 5);
    EXPECT_EQ(cl.end_line.value_or(0), 9);
    EXPECT_EQ(cl.block_skip, (std::vector<int>{1, 3, 9}));
    EXPECT_EQ(cl.tool.value_or(-1), mc::kNoTool);
    EXPECT_EQ(cl.work_offset.value_or(""), "G55");
    // unit省略のcl: 係数は宣言単位 (mm) の1
    EXPECT_NEAR(mc::UnitScale(cl, project.units), 1.0, kTol);
}

TEST(ProjectIoTest, Programs_UnitFollowsDeclaredLengthUnit) {
    const auto project = ReadProjectText(WithInchUnits(
            WithSection("[[program]]\nfile = \"programs/placeholder.cl\"\ntype = \"cl\"\n"
                        "[[program]]\nfile = \"programs/placeholder.cl\"\ntype = \"apt\"\n"
                        "unit = \"mm\"\n")));
    EXPECT_FALSE(project.programs[0].unit.has_value());
    EXPECT_NEAR(mc::UnitScale(project.programs[0], project.units), 25.4, kTol);
    EXPECT_EQ(project.programs[1].unit.value_or(mc::LengthUnit::kInch),
              mc::LengthUnit::kMillimeter);
    EXPECT_NEAR(mc::UnitScale(project.programs[1], project.units), 1.0, kTol);
}

TEST(ProjectIoTest, Programs_ThrowsDataFormatErrorOnEachRule) {
    const std::string program = "[[program]]\nfile = \"programs/placeholder.nc\"\n";
    ExpectDataFormatError(WithSection("[[program]]\nfile = \"programs/missing.nc\"\n"),
                          "file does not exist");
    ExpectDataFormatError(WithSection("[[program]]\nlibrary = \"programs/placeholder.nc\"\n"),
                          "file is missing");
    ExpectDataFormatError(WithSection(program + "unit = \"mm\"\n"),
                          "unit cannot be specified for gcode");
    ExpectDataFormatError(WithSection(program + "start_line = 0\n"), "start_line must be >= 1");
    ExpectDataFormatError(WithSection(program + "start_line = 5\nend_line = 4\n"),
                          "end_line must be >= start_line");
    ExpectDataFormatError(WithSection(program + "block_skip = [10]\n"),
                          "block_skip switch must be 1..9");
    ExpectDataFormatError(WithSection(program + "block_skip = [0]\n"),
                          "block_skip switch must be 1..9");
    ExpectDataFormatError(WithSection(program + "block_skip = [2, 2]\n"), "duplicate block_skip");
    ExpectDataFormatError(WithSection(program + "tool = 5\n"), "tool #5 is not defined");
    ExpectDataFormatError(WithSection(program + "work_offset = \"G59\"\n"),
                          "work offset \"G59\" is not defined");
    ExpectDataFormatError(WithSection(program + "type = \"heidenhain\"\n"), "unknown type");
    ExpectDataFormatError(WithSection(program + "encoding = \"euc-jp\"\n"), "unknown encoding");
    ExpectDataFormatError(WithSection(program + "newline = \"cr\"\n"), "unknown newline");
}



/**
 * ---- [collision]・[run] ----
 */

TEST(ProjectIoTest, Collision_ParsedAndScaled) {
    const auto project = ReadProjectText(WithInchUnits(
            WithSection("[collision]\ndefault_clearance = 1.0\n"
                        "[[collision.tool_pair]]\npart = \"cutter\"\ntarget = \"stock\"\n"
                        "[[collision.tool_pair]]\npart = \"holder\"\ntarget = \"stock\"\n"
                        "clearance = 2.0\n"
                        "[[collision.tool_pair]]\npart = \"cutter\"\ntarget = \"fixture\"\n"
                        "enabled = false\n"
                        "[[collision.machine_pair]]\ntargets = [\"tool\", \"base\"]\n"
                        "enabled = false\n"
                        "[[collision.machine_pair]]\ntargets = [\"Z\", \"A\"]\n"
                        "subtree = [true, true]\nclearance = 0.5\n")));
    ASSERT_TRUE(project.collision.has_value());
    const mc::ProjectCollisionSettings& collision = *project.collision;
    EXPECT_TRUE(collision.enabled);
    EXPECT_NEAR(collision.default_clearance, 25.4, kTol);
    ASSERT_EQ(collision.tool_pairs.size(), 3u);
    EXPECT_FALSE(collision.tool_pairs[0].enabled);   // cutter x stock の既定はfalse
    EXPECT_FALSE(collision.tool_pairs[0].clearance.has_value());
    EXPECT_TRUE(collision.tool_pairs[1].enabled);
    EXPECT_NEAR(collision.tool_pairs[1].clearance.value_or(0.0), 50.8, kTol);
    EXPECT_FALSE(collision.tool_pairs[2].enabled);
    EXPECT_EQ(collision.tool_pairs[2].target, mc::ModelRole::kFixture);
    ASSERT_EQ(collision.machine_pairs.size(), 2u);
    EXPECT_EQ(collision.machine_pairs[0].enabled.value_or(true), false);
    EXPECT_FALSE(collision.machine_pairs[0].clearance.has_value());
    EXPECT_EQ(collision.machine_pairs[1].subtree, (std::array<bool, 2>{true, true}));
    EXPECT_NEAR(collision.machine_pairs[1].clearance.value_or(0.0), 12.7, kTol);
    EXPECT_FALSE(collision.machine_pairs[1].enabled.has_value());
    EXPECT_FALSE(ReadProjectText(MinimalProject()).collision.has_value());
}

TEST(ProjectIoTest, Collision_ThrowsDataFormatErrorWhenPairsAreInvalid) {
    const std::string pair = "[collision]\n[[collision.tool_pair]]\n";
    ExpectDataFormatError(WithSection(pair + "part = \"cutter\"\ntarget = \"stock\"\n"
                                      "[[collision.tool_pair]]\npart = \"cutter\"\n"
                                      "target = \"stock\"\n"),
                          "duplicate tool pair");
    ExpectDataFormatError(WithSection(pair + "part = \"cutter\"\ntarget = \"design\"\n"),
                          "target must be");
    ExpectDataFormatError(WithSection(pair + "part = \"tip\"\ntarget = \"stock\"\n"),
                          "unknown part");
    ExpectDataFormatError(WithSection("[collision]\n[[collision.machine_pair]]\n"
                                      "targets = [\"tool\", \"Nope\"]\n"),
                          "target does not exist: Nope");
    ExpectDataFormatError(WithSection("[collision]\ndefault_clearance = -1\n"), "not >= 0");
    // ペア規則 (予約名へのsubtree) は読込では検査しない
    EXPECT_NO_THROW(ReadProjectText(WithSection(
            "[collision]\n[[collision.machine_pair]]\ntargets = [\"tool\", \"base\"]\n"
            "subtree = [true, false]\n")));
}

TEST(ProjectIoTest, Run_DefaultsAndOutput) {
    const auto defaults = ReadProjectText(MinimalProject());
    EXPECT_EQ(defaults.run.overtravel, mc::OvertravelPolicy::kError);
    EXPECT_EQ(defaults.run.collision, mc::CollisionPolicy::kWarning);
    EXPECT_FALSE(defaults.run.start_tool.has_value());
    EXPECT_FALSE(defaults.run.output.dir.has_value());
    EXPECT_EQ(mc::OutputDir(defaults), kProjectsDir / "output");

    const auto project = ReadProjectText(WithSection(
            "[run]\nstart_tool = 1\nstop_tool = 1\novertravel = \"ignore\"\ncollision = \"error\"\n"
            "[run.output]\ndir = \"out\"\nlog = \"run.log\"\nreport = \"report.json\"\n"
            "cut_stock = \"cut.obj\"\n"));
    EXPECT_EQ(project.run.start_tool.value_or(0), 1);
    EXPECT_EQ(project.run.stop_tool.value_or(0), 1);
    EXPECT_EQ(project.run.overtravel, mc::OvertravelPolicy::kIgnore);
    EXPECT_EQ(project.run.collision, mc::CollisionPolicy::kError);
    ASSERT_TRUE(project.run.output.dir.has_value());
    EXPECT_EQ(project.run.output.dir->raw, "out");
    EXPECT_EQ(mc::OutputDir(project), (kProjectsDir / "out").lexically_normal());
    EXPECT_EQ(project.run.output.log.value_or(""), "run.log");
    EXPECT_EQ(project.run.output.report.value_or(""), "report.json");
    EXPECT_EQ(project.run.output.cut_stock.value_or(""), "cut.obj");
    EXPECT_TRUE(project.warnings.empty());

    const auto extension = ReadProjectText(WithSection(
            "[run]\n[run.output]\ncut_stock = \"cut.step\"\n"));
    ExpectSingleWarning(extension, "cut_stock extension");
}

TEST(ProjectIoTest, Run_ThrowsDataFormatErrorWhenValuesAreUnknown) {
    ExpectDataFormatError(WithSection("[run]\novertravel = \"stop\"\n"), "unknown overtravel");
    ExpectDataFormatError(WithSection("[run]\ncollision = \"ignore\"\n"), "unknown collision");
    ExpectDataFormatError(WithSection("[run]\nstart_tool = 0\n"), "tool #0 is not defined");
    ExpectDataFormatError(WithSection("[run]\nstop_tool = 3\n"), "tool #3 is not defined");
}



/**
 * ---- retained・実例 ----
 */

TEST(ProjectIoTest, Retained_UnreadSectionsAreKept) {
    const auto project = ReadProjectText(
            "bar = 1\n" + Replace(MinimalProject(), "name = \"minimal\"",
                                  "name = \"minimal\"\next = { vendor = \"x\" }")
            + "\n[pnf]\nfile = \"pnf/x.pnf\"\ntool = 1\n"
              "[cutting]\nenabled = true\n[cutting.compare]\nenabled = true\n"
              "[settings.cspace_cam]\nversion = \"0.1.0\"\njson = '''\n{ \"a\": 1 }\n'''\n"
              "[foo]\nbaz = [1, 2]\n");
    ASSERT_EQ(project.retained.size(), 5u);
    EXPECT_EQ(project.retained[0].first, "bar");
    EXPECT_EQ(project.retained[1].first, "pnf");
    EXPECT_EQ(project.retained[2].first, "cutting");
    EXPECT_EQ(project.retained[3].first, "settings");
    EXPECT_EQ(project.retained[4].first, "foo");
    EXPECT_EQ(project.retained[0].second.Text(), "bar = 1\n");
    EXPECT_NE(project.retained[1].second.Text().find("[pnf]"), std::string::npos);
    EXPECT_NE(project.retained[1].second.Text().find("file = \"pnf/x.pnf\""), std::string::npos);
    EXPECT_NE(project.retained[2].second.Text().find("[cutting.compare]"), std::string::npos);
    EXPECT_NE(project.retained[3].second.Text().find("[settings.cspace_cam]"), std::string::npos);
    EXPECT_NE(project.retained[3].second.Text().find("{ \"a\": 1 }"), std::string::npos);
    // 読むセクション内の未知キー (`ext`) は保持されず、警告も出ない
    for (const auto& [key, opaque] : project.retained) EXPECT_NE(key, "project");
    EXPECT_TRUE(project.warnings.empty());
    EXPECT_TRUE(ReadProjectText(MinimalProject()).retained.empty());
}

TEST(ProjectIoTest, Sample_File_Reads) {
    const auto project = mc::ReadProject(kProjectsDir / "sample.toml", DefaultOptions());
    for (const auto& warning : project.warnings) ADD_FAILURE() << mc::FormatDiagnostic(warning);
    EXPECT_EQ(project.name, "sample op10");
    EXPECT_EQ(project.modified, "2026-09-02T10:00:00+09:00");
    EXPECT_EQ(project.source_name, "sample.toml");
    EXPECT_EQ(project.source_dir, kProjectsDir);
    ASSERT_TRUE(project.controller.has_value());
    EXPECT_EQ(project.controller->disabled_codes, (std::vector<std::string>{"G68.2"}));
    ASSERT_EQ(project.tool_libraries.size(), 1u);
    EXPECT_EQ(project.tool_libraries[0].alias, "std");
    ASSERT_EQ(project.tools.size(), 2u);
    EXPECT_NEAR(project.tools[1].gauge_length.value_or(0.0), 120.0, kTol);
    EXPECT_EQ(std::get<mc::SimpleToolSpec>(project.tools[1].source).command_point,
              mc::SimpleToolSpec::CommandPoint::kCenter);
    ASSERT_EQ(project.tool_offsets.size(), 2u);
    EXPECT_NEAR(project.tool_offsets[1].length_wear, -0.02, kTol);
    ASSERT_EQ(project.work_offsets.size(), 2u);
    EXPECT_EQ(project.work_offsets[1].attach, "fixture");
    ASSERT_EQ(project.models.size(), 3u);
    EXPECT_NEAR(project.models[0].geometry.file_unit_scale, 25.4, kTol);
    EXPECT_EQ(project.models[1].attach, "fixture");
    EXPECT_FALSE(project.models[2].visible);
    ASSERT_EQ(project.programs.size(), 2u);
    EXPECT_TRUE(fs::exists(project.programs[0].file.resolved));
    EXPECT_EQ(project.programs[0].encoding, mc::TextEncoding::kShiftJis);
    EXPECT_EQ(project.programs[1].tool.value_or(0), 2);
    EXPECT_EQ(project.initial_tool, mc::kNoTool);
    EXPECT_NEAR(project.initial_axes.At("Z"), 300.0, kTol);
    ASSERT_TRUE(project.collision.has_value());
    EXPECT_NEAR(project.collision->default_clearance, 0.5, kTol);
    EXPECT_EQ(project.collision->tool_pairs.size(), 1u);
    EXPECT_EQ(project.collision->machine_pairs.size(), 1u);
    EXPECT_EQ(project.run.output.cut_stock.value_or(""), "stock_cut.stl");
    ASSERT_EQ(project.retained.size(), 3u);
    EXPECT_EQ(project.retained[0].first, "pnf");
    EXPECT_EQ(project.retained[1].first, "cutting");
    EXPECT_EQ(project.retained[2].first, "settings");
}

TEST(ProjectIoTest, Input_ThrowsFileOpenErrorAndParseErrorWithSourceName) {
    EXPECT_THROW(mc::ReadProject(kProjectsDir / "missing.toml"), igesio::FileOpenError);
    try {
        ReadProjectText("[format\nname = 1");
        FAIL() << "DataFormatError was not thrown";
    } catch (const igesio::DataFormatError& e) {
        EXPECT_NE(std::string(e.what()).find("<test>"), std::string::npos) << e.what();
    }
}

}  // namespace
