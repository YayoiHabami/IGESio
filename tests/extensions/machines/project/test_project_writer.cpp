/**
 * @file tests/extensions/machines/project/test_project_writer.cpp
 * @brief プロジェクト定義の書き出し (project/project_io) のテスト
 * @author Yayoi Habami
 * @date 2026-09-12
 * @copyright 2026 Yayoi Habami
 * @note 対象: WriteProject / WriteProjectToString
 *       - 全角パス: 全角名の参照を別ディレクトリに書き出したときの相対化と
 *         UTF-8での記載、読み戻しでの解決
 *       - 正常系 (往復): `sample.toml`を読込→書き出し→再読込して全フィールドと
 *         `retained`が一致し警告0件であること、C++で組み立てた定義 (ライブラリ参照
 *         工具 (代表形状/代替形状なし)・輪郭形式工具・幾何形式ワークオフセット・
 *         入れ子モデル・`machine_pair`・inch/rad宣言) の往復、輪郭形式工具 (TOML文字列) の往復、仮想機械の指定
 *         (TOML文字列、`MakeProjectDefinition`で作った定義) の往復
 *       - 正常系 (出力形式): 常に書くセクション、単位の差し替え出力、`file`/`library`の
 *         復元と相対化、仮想機械のキー (既定値の省略、`tool_side_limit`の宣言単位),
 *         軸値の宣言単位、既定値の省略 (輪郭形式の直線の`type`を含む)、
 *         `retained`の末尾配置、日時リテラル、`[tool.library]`と`format`のキー
 *       - テンプレート: `is_template`は`true`の場合のみ書くこと、新しいパスへの
 *         テンプレートの作成、既存テンプレートの上書き禁止 (`FileWriteProtectedError`・
 *         ファイル不変)、`overwrite_template`での上書き、別名での保存とその再保存,
 *         テンプレートと判定されない既存ファイル (`false`/真偽値でない/
 *         解析できない内容) の上書き
 *       - 異常系: 機械に無い軸名・`raw`空のライブラリ参照・ライブラリ参照の
 *         プログラム・セグメントを持たない部位要素・`fallback`と整合しない形状の
 *         有無・空の`alias`/`source`/工具識別子の`invalid_argument`、
 *         出力先がディレクトリの`FileOpenError`
 *       TODO: 退化ケース (工具・モデル等が全て空の定義) は`Omission_Defaults`の
 *             最小構成で兼ねる
 */
#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "igesio/common/color.h"
#include "igesio/common/errors.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/rotation.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/machine/machine_definition.h"
#include "igesio/extensions/machines/machine/machine_io.h"
#include "igesio/extensions/machines/machine/virtual_machines.h"
#include "igesio/extensions/machines/project/project_definition.h"
#include "igesio/extensions/machines/project/project_io.h"
#include "igesio/extensions/machines/tools/tool_assembly.h"
#include "igesio/extensions/machines/tools/tool_profile.h"
#include "../machine/machines_for_testing.h"
#include "igesio/utils/path_encoding.h"
#include "./projects_for_testing.h"
#include "unicode_test_path.h"

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
using projects_test::ProfileToolSection;
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

/// @brief `[machine]`の記載 (ファイル参照または仮想機械の指定) の一致を検証する
void ExpectSameMachineSource(
        const std::variant<mc::FileReference, mc::VirtualMachineSpec>& expected,
        const std::variant<mc::FileReference, mc::VirtualMachineSpec>& actual) {
    ASSERT_EQ(expected.index(), actual.index());
    if (const auto* file = std::get_if<mc::FileReference>(&expected)) {
        ExpectSameReference(*file, std::get<mc::FileReference>(actual));
        return;
    }
    const auto& spec = std::get<mc::VirtualMachineSpec>(expected);
    const auto& other = std::get<mc::VirtualMachineSpec>(actual);
    EXPECT_EQ(spec.kind, other.kind);
    EXPECT_EQ(spec.options.name, other.options.name);
    EXPECT_EQ(spec.options.branch, other.options.branch);
    ExpectSameOptional(spec.options.tool_side_limit_rad,
                       other.options.tool_side_limit_rad);
}

/// @brief 輪郭形式の工具の一致を検証する (部位要素・セグメント・色・指令点)
void ExpectSameProfile(const mc::ToolProfile& expected, const mc::ToolProfile& actual) {
    EXPECT_EQ(expected.name, actual.name);
    EXPECT_NEAR(expected.command_point_z, actual.command_point_z, kTol);
    ExpectSameOptional(expected.gauge_line_z, actual.gauge_line_z);
    ASSERT_EQ(expected.elements.size(), actual.elements.size());
    for (std::size_t i = 0; i < expected.elements.size(); ++i) {
        const mc::ToolProfileElement& element = expected.elements[i];
        const mc::ToolProfileElement& other = actual.elements[i];
        EXPECT_EQ(element.part, other.part);
        EXPECT_EQ(element.name, other.name);
        ASSERT_EQ(element.color.has_value(), other.color.has_value());
        if (element.color.has_value()) {
            EXPECT_NEAR(element.color->r, other.color->r, kTol);
            EXPECT_NEAR(element.color->g, other.color->g, kTol);
            EXPECT_NEAR(element.color->b, other.color->b, kTol);
        }
        EXPECT_NEAR(element.opacity, other.opacity, kTol);
        ASSERT_EQ(element.segments.size(), other.segments.size());
        for (std::size_t j = 0; j < element.segments.size(); ++j) {
            const mc::ProfileSegment& segment = element.segments[j];
            const mc::ProfileSegment& other_segment = other.segments[j];
            EXPECT_EQ(segment.kind, other_segment.kind);
            EXPECT_NEAR((segment.start - other_segment.start).norm(), 0.0, kTol);
            EXPECT_NEAR((segment.end - other_segment.end).norm(), 0.0, kTol);
            if (segment.kind == mc::ProfileSegment::Kind::kArc) {
                EXPECT_NEAR((segment.center - other_segment.center).norm(), 0.0, kTol);
                EXPECT_EQ(segment.counter_clockwise, other_segment.counter_clockwise);
            }
        }
    }
}

/// @brief 工具エントリの一致を検証する
void ExpectSameTool(const mc::ToolEntry& expected, const mc::ToolEntry& actual) {
    EXPECT_EQ(expected.number, actual.number);
    EXPECT_EQ(expected.name, actual.name);
    ExpectSameOptional(expected.gauge_length, actual.gauge_length);
    EXPECT_EQ(expected.control_point, actual.control_point);
    ASSERT_EQ(expected.library.has_value(), actual.library.has_value());
    if (expected.library.has_value()) {
        EXPECT_EQ(expected.library->source, actual.library->source);
        EXPECT_EQ(expected.library->id, actual.library->id);
        EXPECT_EQ(expected.library->fallback, actual.library->fallback);
    }
    ASSERT_EQ(expected.shape.has_value(), actual.shape.has_value());
    if (!expected.shape.has_value()) return;

    ASSERT_EQ(expected.shape->index(), actual.shape->index());
    if (const auto* simple = std::get_if<mc::SimpleToolSpec>(&*expected.shape);
        simple != nullptr) {
        const auto& other = std::get<mc::SimpleToolSpec>(*actual.shape);
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
        ExpectSameProfile(std::get<mc::ToolProfile>(*expected.shape),
                          std::get<mc::ToolProfile>(*actual.shape));
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
    EXPECT_EQ(expected.is_template, actual.is_template);
    EXPECT_EQ(expected.name, actual.name);
    EXPECT_EQ(expected.description, actual.description);
    EXPECT_EQ(expected.author, actual.author);
    EXPECT_EQ(expected.modified, actual.modified);
    EXPECT_EQ(expected.units.length_unit, actual.units.length_unit);
    EXPECT_EQ(expected.units.angle_unit, actual.units.angle_unit);
    ExpectSameMachineSource(expected.machine_source, actual.machine_source);
    EXPECT_EQ(expected.machine.name, actual.machine.name);
    ASSERT_EQ(expected.controller.has_value(), actual.controller.has_value());
    if (expected.controller.has_value()) {
        ExpectSameReference(expected.controller->file, actual.controller->file);
        EXPECT_EQ(expected.controller->disabled_codes, actual.controller->disabled_codes);
    }
    ASSERT_EQ(expected.tool_libraries.size(), actual.tool_libraries.size());
    for (std::size_t i = 0; i < expected.tool_libraries.size(); ++i) {
        EXPECT_EQ(expected.tool_libraries[i].alias, actual.tool_libraries[i].alias);
        EXPECT_EQ(expected.tool_libraries[i].format, actual.tool_libraries[i].format);
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

/// @brief C++で組み立てた輪郭形式の工具 (内部単位mm. inch宣言の往復用)
/// @note スクエアの切れ刃部 (円柱、1インチ径・2インチ長) と、色付きで半透明の
///       ホルダ部 (2インチ径、z=2〜5インチ) の2要素. 母線は回転軸上で閉じておく.
///       指令点は先端 (0. 出力時は省略される)
mc::ToolProfile BuiltProfile() {
    using igesio::Vector2d;
    mc::ToolProfile profile;
    profile.name = "Built profile";
    mc::ToolProfileElement cutter;
    cutter.part = mc::ToolPart::kCutter;
    cutter.segments = {
            mc::ProfileSegment::Line(Vector2d(0.0, 0.0), Vector2d(12.7, 0.0)),
            mc::ProfileSegment::Line(Vector2d(12.7, 0.0), Vector2d(12.7, 50.8)),
            mc::ProfileSegment::Line(Vector2d(12.7, 50.8), Vector2d(0.0, 50.8))};
    mc::ToolProfileElement holder;
    holder.part = mc::ToolPart::kHolder;
    holder.name = "collet";
    holder.color = igesio::Color::FromRGB255(0x10, 0x20, 0x30);
    holder.opacity = 0.75f;
    holder.segments = {
            mc::ProfileSegment::Line(Vector2d(0.0, 50.8), Vector2d(25.4, 50.8)),
            mc::ProfileSegment::Arc(Vector2d(25.4, 50.8), Vector2d(50.8, 76.2),
                                    Vector2d(50.8, 50.8), false),
            mc::ProfileSegment::Line(Vector2d(50.8, 76.2), Vector2d(50.8, 127.0)),
            mc::ProfileSegment::Line(Vector2d(50.8, 127.0), Vector2d(0.0, 127.0))};
    profile.elements = {cutter, holder};
    return profile;
}

/// @brief C++で組み立てた定義 (ライブラリ2つ・ライブラリ参照工具2本 (代表形状の#3と
///        代替形状なしの#9)・幾何形式ワークオフセット・入れ子モデル・`machine_pair`・
///        inch/deg宣言・保持断片)
/// @note 単位換算の往復を検証するため、内部値はmm・radで与える
mc::ProjectDefinition BuiltInCpp() {
    mc::ProjectDefinition project;
    project.format_version = mc::kProjectFormatVersion;
    project.name = "built";
    project.description = "assembled in C++";
    project.author = "test";
    project.modified = "2026-09-12";
    project.units = mc::MakeUnitScales(mc::LengthUnit::kInch, mc::AngleUnit::kDegree);
    project.machine_source = MachineReference();
    project.machine = mc::ReadMachineDefinition(kFixturePath);
    project.source_dir = kProjectsDir;

    mc::ToolLibrarySpec std_lib;
    std_lib.alias = "std";
    std_lib.format = "igesio-test";
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
    library_tool.library =
            mc::LibraryToolRef{"local", "LIB-7", mc::ToolFallback::kApproximate};
    library_tool.gauge_length = 254.0;
    library_tool.control_point = mc::ControlPoint::kGauge;
    mc::ToolEntry no_shape_tool;
    no_shape_tool.number = 9;
    no_shape_tool.name = "Lib 9";
    no_shape_tool.library = mc::LibraryToolRef{"std", "9", mc::ToolFallback::kNone};
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
    simple_tool.shape = simple;
    library_tool.shape = simple;
    mc::ToolEntry profile_tool;
    profile_tool.number = 7;
    profile_tool.name = "Built profile";
    profile_tool.shape = BuiltProfile();
    project.tools = {library_tool, simple_tool, profile_tool, no_shape_tool};

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

/// @brief ファイルの内容をバイト列のまま読み込む
/// @param path 読み込むファイルのパス
/// @return ファイルの内容 (開けなければ空)
std::string ReadFileBytes(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream),
                       std::istreambuf_iterator<char>());
}

/// @brief テキストをファイルにバイト列のまま書き込む (既存ファイルは上書き)
/// @param path 書き込むファイルのパス
/// @param text 書き込む内容
void WriteFileBytes(const fs::path& path, const std::string& text) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

/// @brief テンプレートのプロジェクト定義を作成する
/// @return `MinimalProject()`を読み込み、`is_template = true`にしたもの
mc::ProjectDefinition MinimalTemplate() {
    auto project = ReadProjectText(MinimalProject());
    project.is_template = true;
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

TEST(ProjectWriterTest, RoundTrip_ProfileTool) {
    const auto original = ReadProjectText(MinimalProject() + "\n" + ProfileToolSection());
    const auto restored = RoundTrip(original, kProjectsDir);
    EXPECT_TRUE(restored.warnings.empty());
    ExpectSameProject(original, restored);

    // 読込時に補った軸までの直線も含めて、閉じた母線をそのまま書く
    const std::string text = mc::WriteProjectToString(original, kProjectsDir);
    EXPECT_TRUE(Contains(text, "[tool.profile]\ncommand_point_z = 3.0")) << text;
    EXPECT_TRUE(Contains(text, "[[tool.profile.element]]\npart = \"cutter\"\nstart = [0.0, 0.0]"))
            << text;
    EXPECT_TRUE(Contains(text, "part = \"shank\"\nname = \"neck\"\nstart = [0.0, 10.0]"))
            << text;
    EXPECT_TRUE(Contains(text, "{to = [0.0, 40.0]}")) << text;
    EXPECT_TRUE(Contains(text, "{type = \"arc\", to = [3.0, 3.0], center = [0.0, 3.0], "
                               "direction = \"ccw\"}"))
            << text;
    EXPECT_TRUE(Contains(text, "direction = \"cw\"")) << text;
    EXPECT_TRUE(Contains(text, "color = \"#606060\"\nopacity = 0.5")) << text;
    // 直線の`type`、切れ刃部の空の`name`、デフォルトの`opacity`は書かない
    EXPECT_FALSE(Contains(text, "type = \"line\"")) << text;
    EXPECT_FALSE(Contains(text, "name = \"\"")) << text;
    EXPECT_EQ(text.find("opacity"), text.rfind("opacity")) << text;
}

TEST(ProjectWriterTest, RoundTrip_VirtualMachine) {
    // 全キーを既定と異なる値にした仮想機械 (deg宣言)
    const auto original = ReadProjectText(Replace(
            MinimalProject(), "library = \"t-ZYX-b-AC-w.toml\"",
            "virtual = \"table_ac\"\nname = \"cam\"\nbranch = \"positive\"\n"
            "tool_side_limit = 120.0"));
    const auto restored = RoundTrip(original, kProjectsDir);
    EXPECT_TRUE(restored.warnings.empty());
    ExpectSameProject(original, restored);
    EXPECT_EQ(restored.machine.name, "cam");
    EXPECT_EQ(restored.machine.components.size(), original.machine.components.size());
}

TEST(ProjectWriterTest, RoundTrip_VirtualMachineFromCpp) {
    // `MakeProjectDefinition`で仮想機械から作った定義を書き出せる
    mc::VirtualMachineSpec spec;
    spec.kind = mc::VirtualMachineKind::kHeadBc;
    spec.options.tool_side_limit_rad = ToRadians(100.0);
    mc::ProjectDefinition original = mc::MakeProjectDefinition(spec, "virtual-project");
    original.source_dir = kProjectsDir;
    const auto restored = RoundTrip(original, kProjectsDir);
    EXPECT_TRUE(restored.warnings.empty());
    ExpectSameProject(original, restored);
}



/**
 * ---- 出力形式 ----
 */

TEST(ProjectWriterTest, Sections_AlwaysWritten) {
    auto project = ReadProjectText(MinimalProject());
    const std::string text = mc::WriteProjectToString(project, kProjectsDir);
    EXPECT_TRUE(Contains(text, "# machining-project 2.1"));
    EXPECT_TRUE(Contains(text, "[format]\nname = \"machining-project\"\nversion = [2, 1]"));
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
    EXPECT_NEAR(std::get<mc::SimpleToolSpec>(*restored.tools[0].shape).diameter, 10.0, kTol);
}

TEST(ProjectWriterTest, Format_IsTemplateWrittenOnlyWhenTrue) {
    auto project = ReadProjectText(MinimalProject());
    EXPECT_FALSE(Contains(mc::WriteProjectToString(project, kProjectsDir), "is_template"));

    project.is_template = true;
    const std::string text = mc::WriteProjectToString(project, kProjectsDir);
    EXPECT_TRUE(Contains(text, "version = [2, 1]\nis_template = true\n")) << text;
    const auto restored = mc::ReadProjectFromString(text, kProjectsDir, DefaultOptions());
    EXPECT_TRUE(restored.is_template);
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

TEST(ProjectWriterTest, Machine_VirtualKeysAndDefaultsOmitted) {
    auto project = ReadProjectText(Replace(MinimalProject(),
                                           "library = \"t-ZYX-b-AC-w.toml\"",
                                           "virtual = \"head_bc\""));
    // 既定と一致する`name`/`branch`と、値の無い`tool_side_limit`は書かない
    const std::string text = mc::WriteProjectToString(project, kProjectsDir);
    EXPECT_TRUE(Contains(text, "[machine]\nvirtual = \"head_bc\"\n")) << text;
    EXPECT_FALSE(Contains(text, "name = \"virtual\"")) << text;
    EXPECT_FALSE(Contains(text, "tool_side_limit")) << text;
    EXPECT_FALSE(Contains(text, "branch")) << text;

    // `tool_side_limit`は宣言単位で書く (rad宣言では換算しない)
    auto& spec = std::get<mc::VirtualMachineSpec>(project.machine_source);
    spec.options.tool_side_limit_rad = ToRadians(90.0);
    spec.options.branch = mc::BranchPolicy::kPositive;
    const std::string deg = mc::WriteProjectToString(project, kProjectsDir);
    EXPECT_TRUE(Contains(deg, "virtual = \"head_bc\"\nbranch = \"positive\"\n"
                              "tool_side_limit = 90.0")) << deg;
    project.units = mc::MakeUnitScales(mc::LengthUnit::kMillimeter,
                                       mc::AngleUnit::kRadian);
    spec.options.tool_side_limit_rad = 1.5;
    const std::string rad = mc::WriteProjectToString(project, kProjectsDir);
    EXPECT_TRUE(Contains(rad, "tool_side_limit = 1.5")) << rad;
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
    bare.machine_source = MachineReference();
    bare.machine = mc::ReadMachineDefinition(kFixturePath);
    const std::string minimal = mc::WriteProjectToString(bare, kProjectsDir);
    EXPECT_FALSE(Contains(minimal, "[initial]"));
    EXPECT_FALSE(Contains(minimal, "[collision]"));
    EXPECT_FALSE(Contains(minimal, "[["));
    const auto restored = mc::ReadProjectFromString(minimal, kProjectsDir, DefaultOptions());
    EXPECT_TRUE(restored.work_offsets.empty());
    EXPECT_TRUE(restored.warnings.empty());
}

TEST(ProjectWriterTest, ToolLibrary_KeysWritten) {
    const std::string text = mc::WriteProjectToString(BuiltInCpp(), kProjectsDir);
    EXPECT_TRUE(Contains(text, "[[tool_library]]\nalias = \"std\"\nformat = \"igesio-test\"\n"
                               "library = \"tools/tools.json\""))
            << text;
    // `format`が空なら書かない
    EXPECT_TRUE(Contains(text, "[[tool_library]]\nalias = \"local\"\nfile = "));
    // `[tool.library]`はスカラーのキーの後、形状の前に置く
    const std::size_t library = text.find("[tool.library]\nsource = \"local\"\nid = \"LIB-7\"\n"
                                          "fallback = \"approximate\"");
    ASSERT_NE(library, std::string::npos) << text;
    EXPECT_LT(text.find("control_point = \"gauge\""), library);
    EXPECT_LT(library, text.find("[tool.simple]", library));
    // 代替形状なしの工具は形状のサブテーブルを持たない
    EXPECT_TRUE(Contains(text, "[tool.library]\nsource = \"std\"\nid = \"9\"\n"
                               "fallback = \"none\"\n"));
    EXPECT_FALSE(Contains(text.substr(text.find("id = \"9\"")), "[tool.simple]"));
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
        // from_library かつ raw が空
        std::get<mc::FileReference>(project.machine_source).raw.clear();
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
    {
        auto project = BuiltInCpp();
        // セグメントを持たない部位要素は`start`を決められない
        std::get<mc::ToolProfile>(*project.tools[2].shape).elements[0].segments.clear();
        EXPECT_THROW(mc::WriteProjectToString(project, kProjectsDir), std::invalid_argument);
    }
}

TEST(ProjectWriterTest, Throws_InvalidArgumentWhenToolShapeDoesNotMatchFallback) {
    {
        // ライブラリ参照なしで形状が無い
        auto project = BuiltInCpp();
        project.tools[1].shape.reset();
        EXPECT_THROW(mc::WriteProjectToString(project, kProjectsDir), std::invalid_argument);
    }
    {
        // 代表形状のはずが形状が無い
        auto project = BuiltInCpp();
        project.tools[0].shape.reset();
        EXPECT_THROW(mc::WriteProjectToString(project, kProjectsDir), std::invalid_argument);
    }
    {
        // 代替形状なしのはずが形状を持つ
        auto project = BuiltInCpp();
        project.tools[3].shape = project.tools[0].shape;
        EXPECT_THROW(mc::WriteProjectToString(project, kProjectsDir), std::invalid_argument);
    }
}

TEST(ProjectWriterTest, Throws_InvalidArgumentWhenLibraryAliasOrSourceIsEmpty) {
    {
        auto project = BuiltInCpp();
        project.tool_libraries[0].alias.clear();
        EXPECT_THROW(mc::WriteProjectToString(project, kProjectsDir), std::invalid_argument);
    }
    {
        auto project = BuiltInCpp();
        project.tools[0].library->source.clear();
        EXPECT_THROW(mc::WriteProjectToString(project, kProjectsDir), std::invalid_argument);
    }
}

TEST(ProjectWriterTest, Throws_InvalidArgumentWhenLibraryToolIdIsEmpty) {
    auto project = BuiltInCpp();
    project.tools[0].library->id.clear();
    EXPECT_THROW(mc::WriteProjectToString(project, kProjectsDir), std::invalid_argument);
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



/**
 * ---- テンプレートの上書き禁止 ----
 */

TEST(ProjectWriterTest, WriteProject_CreatesTemplateAtNewPath) {
    const igesio::tests::UnicodeTempDir dir("project_template_create");
    const fs::path path = dir.Join("template.toml");
    mc::WriteProject(MinimalTemplate(), path);
    EXPECT_TRUE(Contains(ReadFileBytes(path), "is_template = true"));
}

TEST(ProjectWriterTest, WriteProject_ThrowsFileWriteProtectedErrorWhenOverwritingTemplate) {
    const igesio::tests::UnicodeTempDir dir("project_template_protect");
    const fs::path path = dir.Join("template.toml");
    mc::WriteProject(MinimalTemplate(), path);
    const std::string before = ReadFileBytes(path);

    // 書き出す定義がテンプレートかによらず、既存のテンプレートは上書きしない
    auto edited = MinimalTemplate();
    edited.description = "edited";
    EXPECT_THROW(mc::WriteProject(edited, path), igesio::FileWriteProtectedError);
    edited.is_template = false;
    try {
        mc::WriteProject(edited, path);
        FAIL() << "FileWriteProtectedError was not thrown";
    } catch (const igesio::FileWriteProtectedError& e) {
        EXPECT_EQ(e.getFilename(), igesio::utils::PathToUtf8(path));
    }
    EXPECT_EQ(ReadFileBytes(path), before);
}

TEST(ProjectWriterTest, WriteProject_OverwritesTemplateWhenExplicitlyAllowed) {
    const igesio::tests::UnicodeTempDir dir("project_template_allow");
    const fs::path path = dir.Join("template.toml");
    mc::WriteProject(MinimalTemplate(), path);

    auto edited = MinimalTemplate();
    edited.description = "edited";
    mc::WriteProjectOptions options;
    options.overwrite_template = true;
    mc::WriteProject(edited, path, options);
    const std::string text = ReadFileBytes(path);
    EXPECT_TRUE(Contains(text, "description = \"edited\"")) << text;
    EXPECT_TRUE(Contains(text, "is_template = true")) << text;
}

TEST(ProjectWriterTest, WriteProject_SavesProjectFromTemplateToAnotherPath) {
    const igesio::tests::UnicodeTempDir dir("project_template_save_as");
    const fs::path template_path = dir.Join("template.toml");
    mc::WriteProject(MinimalTemplate(), template_path);
    const std::string before = ReadFileBytes(template_path);

    // テンプレートから作ったプロジェクトは`is_template = false`にして別名で保存し,
    // 保存したファイルはその後も上書きできる
    auto project = MinimalTemplate();
    project.is_template = false;
    const fs::path path = dir.Join("project.toml");
    mc::WriteProject(project, path);
    project.description = "edited";
    mc::WriteProject(project, path);
    const std::string text = ReadFileBytes(path);
    EXPECT_TRUE(Contains(text, "description = \"edited\"")) << text;
    EXPECT_FALSE(Contains(text, "is_template")) << text;
    EXPECT_EQ(ReadFileBytes(template_path), before);
}

TEST(ProjectWriterTest, WriteProject_OverwritesFileNotMarkedAsTemplate) {
    const igesio::tests::UnicodeTempDir dir("project_template_overwrite");
    const fs::path path = dir.Join("existing.toml");
    // 既存ファイルがテンプレートと判定されない場合: (1) `is_template = false`,
    // (2) 真偽値でない`is_template`, (3) TOMLとして解析できない内容
    const std::string contents[] = {
            "[format]\nis_template = false\n",
            "[format]\nis_template = \"true\"\n",
            "[format\nis_template = true\n",
    };
    for (const std::string& content : contents) {
        WriteFileBytes(path, content);
        // 通常のファイルをテンプレートとして保存し直すことも許可する
        EXPECT_NO_THROW(mc::WriteProject(MinimalTemplate(), path)) << content;
        EXPECT_TRUE(Contains(ReadFileBytes(path), "is_template = true")) << content;
    }
}



/**
 * ---- 全角パス ----
 */

TEST(ProjectWriterTest, UnicodePath_WritesRelativizedUtf8ReferencesAndReadsBack) {
    using igesio::tests::kUnicodeName;
    const igesio::tests::UnicodeTempDir dir("project_writer");
    const auto original =
            mc::ReadProject(projects_test::WriteUnicodeProject(dir), mc::ReadProjectOptions{});

    // 別のディレクトリに書き出し、参照を出力先基準に相対化させる
    const fs::path out_dir = dir.Join("out_" + kUnicodeName);
    fs::create_directories(out_dir);
    const fs::path path = out_dir / igesio::utils::PathFromUtf8(kUnicodeName + ".toml");
    mc::WriteProject(original, path);
    std::string text;
    {
        std::ifstream stream(path, std::ios::binary);
        text.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }
    EXPECT_TRUE(Contains(text, "file = \"../" + kUnicodeName + "/" + kUnicodeName + ".toml\""))
            << text;
    EXPECT_TRUE(Contains(text, "file = \"../" + kUnicodeName + ".nc\"")) << text;

    const auto restored = mc::ReadProject(path, mc::ReadProjectOptions{});
    EXPECT_EQ(std::get<mc::FileReference>(restored.machine_source).resolved,
              std::get<mc::FileReference>(original.machine_source).resolved);
    EXPECT_EQ(restored.programs.at(0).file.resolved, original.programs.at(0).file.resolved);
}

TEST(ProjectWriterTest, UnicodePath_ThrowsFileOpenErrorWithUtf8PathWhenDirectoryMissing) {
    using igesio::tests::kUnicodeName;
    const igesio::tests::UnicodeTempDir dir("project_writer_missing");
    const auto original = ReadProjectText(MinimalProject());
    const fs::path path = dir.Join(kUnicodeName + "/" + kUnicodeName + ".toml");
    try {
        mc::WriteProject(original, path);
        FAIL() << "FileOpenError was not thrown";
    } catch (const igesio::FileOpenError& e) {
        EXPECT_NE(std::string(e.what()).find(dir.JoinUtf8(kUnicodeName)), std::string::npos);
    }
}
