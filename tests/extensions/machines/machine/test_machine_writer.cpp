/**
 * @file tests/extensions/machines/machine/test_machine_writer.cpp
 * @brief 機械定義の書き出し (machine/machine_io) のテスト
 * @author Yayoi Habami
 * @date 2026-09-09
 * @copyright 2026 Yayoi Habami
 * @note 対象: WriteMachineDefinition / WriteMachineDefinitionToString
 *       - 正常系 (往復): 実例TOMLを読込→書き出し→再読込して全フィールドが一致し
 *         警告0件であること、C++で組み立てた定義 (local_frame・回転付きframe・
 *         形状属性・spindle・詳細ペア) の往復
 *       - 正常系 (出力形式): [format]・[units]・[kinematics]の記載、単位の
 *         差し替え出力、raw_pathの保持と他ディレクトリへの相対化、暗黙baseの
 *         省略と明示baseの記載、簡易形/詳細形ペアの分離、[collision]の省略、
 *         [component.spindle]の有無と空テーブル、送り速度の毎秒→毎分の書き戻し
 *       - 正常系 (数値): 最短の往復桁数、単位ベクトル微小成分の0への丸め、
 *         initialの省略規則
 *       - 正常系 (境界値): file_unit_scaleの許容誤差の内側、IGESのunit非出力
 *       - 異常系: file_unit_scaleが単位の係数でない場合のinvalid_argument、
 *         出力先がディレクトリの場合のFileOpenError
 *       TODO: 退化ケース (コンポーネント0個) は読込側が拒否するため往復で
 *             検証できない。文字列化が例外を投げないことのみ確認する
 */
#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <filesystem>
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

namespace {

namespace fs = std::filesystem;
namespace mc = igesio::extensions::machines;
using igesio::Matrix3d;
using igesio::Matrix4d;
using igesio::Vector3d;
using mc::ToRadians;

/// @brief 往復比較の許容誤差 (単位換算の往復とdouble文字列化の丸めを含む)
constexpr double kTol = 1e-9;

/// @brief 色の往復の許容誤差 (`"#RRGGBB"`は8bit量子化のため最大0.5/255ずれる)
constexpr double kColorTol = 1.0 / 255.0;

/// @brief 実例TOMLのパス (tests/test_data/machines/)
const fs::path kFixturePath =
        fs::path(__FILE__).parent_path().parent_path().parent_path().parent_path()
        / "test_data" / "machines" / "t-ZYX-b-AC-w.toml";

/// @brief 文字列入力・出力の相対パス基準ディレクトリ
const fs::path kBaseDir = fs::path("C:/machines");

/// @brief 文字列に部分文字列が含まれるか
bool Contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

/// @brief 名前でコンポーネントを引く
/// @throw std::logic_error 見つからない場合
const mc::ComponentSpec& FindComponent(const mc::MachineDefinition& definition,
                                       const std::string& name) {
    for (const auto& component : definition.components) {
        if (component.name == name) return component;
    }
    throw std::logic_error("component not found: " + name);
}

/// @brief 名前でコンポーネントを引く (書き換え用)
/// @throw std::logic_error 見つからない場合
mc::ComponentSpec& FindComponentMutable(mc::MachineDefinition& definition,
                                        const std::string& name) {
    for (auto& component : definition.components) {
        if (component.name == name) return component;
    }
    throw std::logic_error("component not found: " + name);
}

/// @brief 書き出して同じ基準ディレクトリで読み戻す
mc::MachineDefinition RoundTrip(const mc::MachineDefinition& definition,
                                const fs::path& base_dir) {
    const std::string text = mc::WriteMachineDefinitionToString(definition, base_dir);
    return mc::ReadMachineDefinitionFromString(text, base_dir, "<round-trip>");
}

/// @brief 任意の実数が両方とも無いか、両方あって一致することを検証する
void ExpectSameOptional(const std::optional<double>& expected,
                        const std::optional<double>& actual) {
    ASSERT_EQ(expected.has_value(), actual.has_value());
    if (expected.has_value()) {
        EXPECT_NEAR(*expected, *actual, kTol);
    }
}

/// @brief 主軸属性が両方とも無いか、両方あって一致することを検証する
void ExpectSameSpindle(const std::optional<mc::SpindleSpec>& expected,
                       const std::optional<mc::SpindleSpec>& actual) {
    ASSERT_EQ(expected.has_value(), actual.has_value());
    if (expected.has_value()) ExpectSameOptional(expected->max_rpm, actual->max_rpm);
}

/// @brief 軸定義の一致を検証する
void ExpectSameAxis(const mc::AxisSpec& expected, const mc::AxisSpec& actual) {
    EXPECT_EQ(expected.register_name, actual.register_name);
    EXPECT_TRUE(expected.direction.isApprox(actual.direction, kTol));
    ASSERT_EQ(expected.point.has_value(), actual.point.has_value());
    if (expected.point.has_value()) {
        EXPECT_TRUE(expected.point->isApprox(*actual.point, kTol));
    }
    ASSERT_EQ(expected.limits.has_value(), actual.limits.has_value());
    if (expected.limits.has_value()) {
        EXPECT_NEAR((*expected.limits)[0], (*actual.limits)[0], kTol);
        EXPECT_NEAR((*expected.limits)[1], (*actual.limits)[1], kTol);
    }
    EXPECT_EQ(expected.unlimited, actual.unlimited);
    ExpectSameOptional(expected.wrap_start, actual.wrap_start);
    EXPECT_NEAR(expected.initial, actual.initial, kTol);
    ExpectSameOptional(expected.dynamics.rapid_feed, actual.dynamics.rapid_feed);
    ExpectSameOptional(expected.dynamics.max_feed, actual.dynamics.max_feed);
    ExpectSameOptional(expected.dynamics.min_feed, actual.dynamics.min_feed);
    ExpectSameOptional(expected.dynamics.accel, actual.dynamics.accel);
    ExpectSameOptional(expected.dynamics.decel, actual.dynamics.decel);
    ExpectSameOptional(expected.dynamics.resolution, actual.dynamics.resolution);
}

/// @brief 形状定義の一致を検証する (解決済みパスは基準ディレクトリに依存するため
///        `raw_path`と種別・寸法のみ比較する)
void ExpectSameGeometry(const mc::GeometrySpec& expected,
                        const mc::GeometrySpec& actual) {
    EXPECT_EQ(expected.name, actual.name);
    ASSERT_EQ(expected.source.index(), actual.source.index());
    if (std::holds_alternative<mc::PrimitiveSpec>(expected.source)) {
        const auto& e = std::get<mc::PrimitiveSpec>(expected.source);
        const auto& a = std::get<mc::PrimitiveSpec>(actual.source);
        EXPECT_EQ(e.kind, a.kind);
        EXPECT_TRUE(e.size.isApprox(a.size, kTol));
        EXPECT_NEAR(e.radius, a.radius, kTol);
        EXPECT_NEAR(e.height, a.height, kTol);
    } else {
        EXPECT_EQ(expected.raw_path, actual.raw_path);
        EXPECT_NEAR(expected.file_unit_scale, actual.file_unit_scale, kTol);
    }
    EXPECT_TRUE(expected.placement.isApprox(actual.placement, kTol))
            << "expected:\n" << expected.placement << "\nactual:\n" << actual.placement;
    ASSERT_EQ(expected.color.has_value(), actual.color.has_value());
    if (expected.color.has_value()) {
        for (std::size_t i = 0; i < 3; ++i) {
            EXPECT_NEAR((*expected.color)[i], (*actual.color)[i], kColorTol);
        }
    }
    EXPECT_NEAR(expected.opacity, actual.opacity, 1e-6);
    EXPECT_EQ(expected.collision, actual.collision);
    EXPECT_EQ(expected.visible, actual.visible);
}

/// @brief コンポーネントの一致を検証する
void ExpectSameComponent(const mc::ComponentSpec& expected,
                         const mc::ComponentSpec& actual) {
    EXPECT_EQ(expected.name, actual.name);
    EXPECT_EQ(expected.parent, actual.parent);
    EXPECT_EQ(expected.type, actual.type);
    EXPECT_TRUE(expected.local_frame.isApprox(actual.local_frame, kTol));
    ASSERT_EQ(expected.axis.has_value(), actual.axis.has_value());
    if (expected.axis.has_value()) ExpectSameAxis(*expected.axis, *actual.axis);
    ASSERT_EQ(expected.frame_placement.has_value(), actual.frame_placement.has_value());
    if (expected.frame_placement.has_value()) {
        EXPECT_TRUE(expected.frame_placement->isApprox(*actual.frame_placement, kTol))
                << "expected:\n" << *expected.frame_placement
                << "\nactual:\n" << *actual.frame_placement;
    }
    ExpectSameSpindle(expected.spindle, actual.spindle);
    ASSERT_EQ(expected.geometries.size(), actual.geometries.size());
    for (std::size_t i = 0; i < expected.geometries.size(); ++i) {
        ExpectSameGeometry(expected.geometries[i], actual.geometries[i]);
    }
}

/// @brief 干渉設定の一致を検証する
void ExpectSameCollision(const std::optional<mc::CollisionSettings>& expected,
                         const std::optional<mc::CollisionSettings>& actual) {
    ASSERT_EQ(expected.has_value(), actual.has_value());
    if (!expected.has_value()) return;
    EXPECT_EQ(expected->mode, actual->mode);
    EXPECT_NEAR(expected->default_clearance, actual->default_clearance, kTol);
    EXPECT_EQ(expected->exclude, actual->exclude);
    ASSERT_EQ(expected->pairs.size(), actual->pairs.size());
    for (std::size_t i = 0; i < expected->pairs.size(); ++i) {
        EXPECT_EQ(expected->pairs[i].targets, actual->pairs[i].targets);
        EXPECT_EQ(expected->pairs[i].subtree, actual->pairs[i].subtree);
        EXPECT_NEAR(expected->pairs[i].clearance, actual->pairs[i].clearance, kTol);
        EXPECT_EQ(expected->pairs[i].enabled, actual->pairs[i].enabled);
    }
}

/// @brief 機械定義全体の一致 (`source_*`・`line`・`warnings`を除く) を検証する
void ExpectSameDefinition(const mc::MachineDefinition& expected,
                          const mc::MachineDefinition& actual) {
    EXPECT_EQ(expected.name, actual.name);
    EXPECT_EQ(expected.description, actual.description);
    EXPECT_EQ(expected.author, actual.author);
    EXPECT_EQ(expected.date, actual.date);
    EXPECT_EQ(expected.units.length_unit, actual.units.length_unit);
    EXPECT_EQ(expected.units.angle_unit, actual.units.angle_unit);
    EXPECT_EQ(expected.branch, actual.branch);
    ExpectSameCollision(expected.collision, actual.collision);
    ASSERT_EQ(expected.components.size(), actual.components.size());
    for (std::size_t i = 0; i < expected.components.size(); ++i) {
        ExpectSameComponent(expected.components[i], actual.components[i]);
    }
}

/// @brief 直進軸のコンポーネントを作る (limits ±100mm)
mc::ComponentSpec MakeLinear(const std::string& name, const std::string& parent,
                             const Vector3d& direction) {
    mc::ComponentSpec component;
    component.name = name;
    component.parent = parent;
    component.type = mc::ComponentType::kLinear;
    mc::AxisSpec axis;
    axis.register_name = name;
    axis.direction = direction;
    axis.limits = std::array<double, 2>{-100.0, 100.0};
    component.axis = axis;
    return component;
}

/// @brief z軸まわり90°の回転行列
Matrix3d RotZ90() {
    return mc::RotationAboutAxis(Vector3d::UnitZ(), mc::kQuarterTurn);
}

/// @brief C++で組み立てた最小の機械定義 (工具側XYZ・ワーク側AC・暗黙base)
/// @note A軸は(0,0,60)を通るx軸 (limits ±90°、initial 10°、動特性2値)、
///       C軸は無制限 (wrap_start 0)、Toolは(0,-180,250.5)でz軸まわり90°回転、
///       Z軸はlocal_frame (原点(1,2,3)・z軸まわり90°. 軸方向zは不変) と
///       原点(5,0,0)の箱、Spindleはmax_rpm 12000、単位はmm・deg、
///       干渉は簡易1+詳細1
mc::MachineDefinition MakeMinimalDefinition() {
    mc::MachineDefinition definition;
    definition.name = "minimal-xyz-ac";
    definition.description = "書き出しテスト";
    definition.author = "tester";
    definition.date = "2026-09-09";
    definition.units = mc::MakeUnitScales(mc::LengthUnit::kMillimeter,
                                          mc::AngleUnit::kDegree);

    mc::ComponentSpec a;
    a.name = "A";
    a.parent = "base";
    a.type = mc::ComponentType::kRotary;
    mc::AxisSpec a_axis;
    a_axis.register_name = "A";
    a_axis.direction = Vector3d::UnitX();
    a_axis.point = Vector3d(0.0, 0.0, 60.0);
    a_axis.limits = std::array<double, 2>{ToRadians(-90.0), ToRadians(90.0)};
    a_axis.initial = ToRadians(10.0);
    a_axis.dynamics.rapid_feed = ToRadians(60.0);   // 60 deg/s = 3600 deg/min
    a_axis.dynamics.min_feed = 0.0;
    a.axis = a_axis;
    definition.components.push_back(a);

    mc::ComponentSpec c;
    c.name = "C";
    c.parent = "A";
    c.type = mc::ComponentType::kRotary;
    mc::AxisSpec c_axis;
    c_axis.register_name = "C";
    c_axis.direction = Vector3d::UnitZ();
    c_axis.point = Vector3d::Zero();
    c_axis.unlimited = true;
    c_axis.wrap_start = 0.0;
    c.axis = c_axis;
    definition.components.push_back(c);

    mc::ComponentSpec table;
    table.name = "Table";
    table.parent = "C";
    table.type = mc::ComponentType::kWorkMount;
    table.frame_placement = Matrix4d::Identity();
    definition.components.push_back(table);

    definition.components.push_back(MakeLinear("X", "base", Vector3d::UnitX()));
    definition.components.push_back(MakeLinear("Y", "X", Vector3d::UnitY()));
    mc::ComponentSpec z = MakeLinear("Z", "Y", Vector3d::UnitZ());
    z.local_frame = mc::MakeRigid(RotZ90(), Vector3d(1.0, 2.0, 3.0));
    mc::GeometrySpec box;
    box.name = "z-box";
    mc::PrimitiveSpec primitive;
    primitive.kind = mc::PrimitiveSpec::Kind::kBox;
    primitive.size = Vector3d(10.0, 20.0, 30.0);
    box.source = primitive;
    box.placement = z.local_frame * mc::Translation(Vector3d(5.0, 0.0, 0.0));
    box.color = std::array<float, 3>{1.0f, 0.5f, 0.0f};
    box.opacity = 0.5f;
    box.collision = false;
    box.visible = false;
    z.geometries.push_back(box);
    definition.components.push_back(z);

    mc::ComponentSpec spindle;
    spindle.name = "Spindle";
    spindle.parent = "Z";
    spindle.type = mc::ComponentType::kSpindle;
    spindle.spindle = mc::SpindleSpec{};
    spindle.spindle->max_rpm = 12000.0;
    definition.components.push_back(spindle);

    mc::ComponentSpec tool;
    tool.name = "Tool";
    tool.parent = "Spindle";
    tool.type = mc::ComponentType::kToolMount;
    tool.frame_placement = mc::MakeRigid(RotZ90(), Vector3d(0.0, -180.0, 250.5));
    definition.components.push_back(tool);

    mc::ComponentSpec base;
    base.name = "base";
    base.type = mc::ComponentType::kBase;
    definition.components.push_back(base);

    mc::CollisionSettings collision;
    collision.default_clearance = 1.0;
    mc::CollisionPair simple;
    simple.targets = {"Spindle", "A"};
    simple.clearance = 1.0;
    collision.pairs.push_back(simple);
    mc::CollisionPair detailed;
    detailed.targets = {"Z", "A"};
    detailed.subtree = {true, true};
    detailed.clearance = 2.5;
    detailed.enabled = false;
    collision.pairs.push_back(detailed);
    definition.collision = collision;
    return definition;
}

/// @brief 直進軸X に STL形状 (指定の`file_unit_scale`) を持たせる
mc::MachineDefinition WithStlGeometry(const std::string& file, const double unit_scale) {
    mc::MachineDefinition definition = MakeMinimalDefinition();
    mc::GeometrySpec geometry;
    geometry.source = (kBaseDir / file).lexically_normal();
    geometry.raw_path = file;
    geometry.file_unit_scale = unit_scale;
    definition.source_dir = kBaseDir;
    FindComponentMutable(definition, "X").geometries.push_back(geometry);
    return definition;
}

}  // namespace



// ---- 正常系: 実例TOMLの往復 ----

TEST(MachineWriterTest, Fixture_RoundTripKeepsAllFields) {
    const auto original = mc::ReadMachineDefinition(kFixturePath);
    const auto restored = RoundTrip(original, kFixturePath.parent_path());
    EXPECT_TRUE(restored.warnings.empty());
    ExpectSameDefinition(original, restored);
    // 解決済みパスも一致する (同じ基準ディレクトリ)
    EXPECT_EQ(std::get<fs::path>(FindComponent(restored, "cradle-frame").geometries[0].source),
              std::get<fs::path>(FindComponent(original, "cradle-frame").geometries[0].source));
}

TEST(MachineWriterTest, Fixture_WritesFormatUnitsKinematicsAndFileValues) {
    const auto original = mc::ReadMachineDefinition(kFixturePath);
    const std::string text =
            mc::WriteMachineDefinitionToString(original, kFixturePath.parent_path());
    EXPECT_TRUE(Contains(text, "name = \"machine-definition\""));
    EXPECT_TRUE(Contains(text, "version = [2, 0]"));
    EXPECT_TRUE(Contains(text, "length = \"mm\""));
    EXPECT_TRUE(Contains(text, "angle = \"deg\""));
    EXPECT_TRUE(Contains(text, "branch = \"positive\""));
    EXPECT_TRUE(Contains(text, "date = \"2026-08-31\""));
    // 角度はdegへ戻る
    EXPECT_TRUE(Contains(text, "limits = [-90.0, 90.0]"));
    EXPECT_TRUE(Contains(text, "rapid_feed = 3600.0"));
    EXPECT_TRUE(Contains(text, "default_clearance = 0.5"));
    EXPECT_TRUE(Contains(text, "origin = [0.0, -180.0, 250.5]"));
    // 明示baseは形状を持つので書かれる
    EXPECT_TRUE(Contains(text, "type = \"base\""));
}

TEST(MachineWriterTest, Fixture_RawPathIsKeptWhenBaseDirMatches) {
    const auto original = mc::ReadMachineDefinition(kFixturePath);
    const std::string text =
            mc::WriteMachineDefinitionToString(original, kFixturePath.parent_path());
    EXPECT_TRUE(Contains(text, "file = \"tool-ZYX-base-AC-work/cradle-frame.STL\""));
    EXPECT_FALSE(Contains(text, "unit = "));
}

TEST(MachineWriterTest, Fixture_PathIsRelativizedToAnotherBaseDir) {
    const auto original = mc::ReadMachineDefinition(kFixturePath);
    const fs::path out_dir = kFixturePath.parent_path() / "out";
    const std::string text = mc::WriteMachineDefinitionToString(original, out_dir);
    EXPECT_TRUE(Contains(text, "file = \"../tool-ZYX-base-AC-work/cradle-frame.STL\""));
    const auto restored = mc::ReadMachineDefinitionFromString(text, out_dir, "<out>");
    EXPECT_EQ(std::get<fs::path>(FindComponent(restored, "cradle-frame").geometries[0].source),
              std::get<fs::path>(FindComponent(original, "cradle-frame").geometries[0].source));
    // 親ディレクトリ越えの警告は読込側の仕様どおり1件
    ASSERT_EQ(restored.warnings.size(), 1u);
    EXPECT_TRUE(Contains(restored.warnings[0].message, "親ディレクトリ越え"));
}

TEST(MachineWriterTest, Fixture_SimpleAndDetailedPairsAreSeparated) {
    const auto original = mc::ReadMachineDefinition(kFixturePath);
    const std::string text =
            mc::WriteMachineDefinitionToString(original, kFixturePath.parent_path());
    EXPECT_TRUE(Contains(text, "pairs = [\n"));
    EXPECT_TRUE(Contains(text, "[\"Spindle\", \"cradle-frame\"]"));
    EXPECT_TRUE(Contains(text, "[[collision.pair]]\ntargets = [\"Z\", \"A\"]\n"
                               "subtree = [true, true]"));
    EXPECT_FALSE(Contains(text, "enabled"));
    EXPECT_FALSE(Contains(text, "\nclearance = "));   // 既定値と同じペアは省略
}

// ---- 正常系: C++で組み立てた定義の往復 ----

TEST(MachineWriterTest, Constructed_RoundTripKeepsAllFields) {
    const auto original = MakeMinimalDefinition();
    const auto restored = RoundTrip(original, kBaseDir);
    EXPECT_TRUE(restored.warnings.empty());
    ExpectSameDefinition(original, restored);
}

TEST(MachineWriterTest, Constructed_WritesRelativeFrameAndGeometryAttributes) {
    const std::string text =
            mc::WriteMachineDefinitionToString(MakeMinimalDefinition(), kBaseDir);
    // Toolのframeは回転を持つのでx_axisも書く
    EXPECT_TRUE(Contains(text, "[component.frame]\norigin = [0.0, -180.0, 250.5]\n"
                               "z_axis = [0.0, 0.0, 1.0]\nx_axis = [0.0, 1.0, 0.0]"));
    // Tableのframeは単位行列なのでz_axisのみ
    EXPECT_TRUE(Contains(text, "[component.frame]\norigin = [0.0, 0.0, 0.0]\n"
                               "z_axis = [0.0, 0.0, 1.0]\n\n"));
    // Zのlocal_frameと、その相対で書かれる形状原点 (丸め誤差は0に丸める)
    EXPECT_TRUE(Contains(text, "[component.local_frame]\norigin = [1.0, 2.0, 3.0]\n"
                               "rotation = {x_axis = [0.0, 1.0, 0.0], "
                               "y_axis = [-1.0, 0.0, 0.0], z_axis = [0.0, 0.0, 1.0]}"));
    EXPECT_TRUE(Contains(text, "size = [10.0, 20.0, 30.0]\norigin = [5.0, 0.0, 0.0]\n"
                               "color = \"#ff8000\"\nopacity = 0.5\n"
                               "collision = false\nvisible = false"));
    EXPECT_TRUE(Contains(text, "[component.spindle]\nmax_rpm = 12000.0"));
    EXPECT_TRUE(Contains(text, "initial = 10.0"));
    EXPECT_TRUE(Contains(text, "unlimited = true\nwrap_start = 0.0"));
    EXPECT_TRUE(Contains(text, "min_feed = 0.0"));
    EXPECT_TRUE(Contains(text, "[[collision.pair]]\ntargets = [\"Z\", \"A\"]\n"
                               "subtree = [true, true]\nclearance = 2.5\nenabled = false"));
}

TEST(MachineWriterTest, ImplicitBase_IsOmittedAndRestoredLast) {
    const auto original = MakeMinimalDefinition();
    const std::string text = mc::WriteMachineDefinitionToString(original, kBaseDir);
    EXPECT_FALSE(Contains(text, "name = \"base\""));
    const auto restored = RoundTrip(original, kBaseDir);
    EXPECT_EQ(restored.components.back().name, "base");
    EXPECT_EQ(restored.components.back().type, mc::ComponentType::kBase);
}

TEST(MachineWriterTest, ExplicitBase_WithGeometryOrLocalFrameIsWritten) {
    auto with_geometry = MakeMinimalDefinition();
    mc::GeometrySpec cylinder;
    mc::PrimitiveSpec primitive;
    primitive.kind = mc::PrimitiveSpec::Kind::kCylinder;
    primitive.radius = 5.0;
    primitive.height = 20.0;
    cylinder.source = primitive;
    FindComponentMutable(with_geometry, "base").geometries.push_back(cylinder);
    const std::string text = mc::WriteMachineDefinitionToString(with_geometry, kBaseDir);
    EXPECT_TRUE(Contains(text, "name = \"base\"\ntype = \"base\"\n\n[[component.geometry]]\n"
                               "primitive = \"cylinder\"\nradius = 5.0\nheight = 20.0"));
    EXPECT_FALSE(Contains(text, "parent = \"\""));
    ExpectSameDefinition(with_geometry, RoundTrip(with_geometry, kBaseDir));

    auto with_frame = MakeMinimalDefinition();
    FindComponentMutable(with_frame, "base").local_frame =
            mc::Translation(Vector3d(0.0, 0.0, -50.0));
    EXPECT_TRUE(Contains(mc::WriteMachineDefinitionToString(with_frame, kBaseDir),
                         "type = \"base\"\n\n[component.local_frame]\n"
                         "origin = [0.0, 0.0, -50.0]"));
}

TEST(MachineWriterTest, Units_OverrideWritesConvertedValues) {
    auto original = mc::ReadMachineDefinition(kFixturePath);
    original.units = mc::MakeUnitScales(mc::LengthUnit::kInch, mc::AngleUnit::kRadian);
    const std::string text =
            mc::WriteMachineDefinitionToString(original, kFixturePath.parent_path());
    EXPECT_TRUE(Contains(text, "length = \"inch\""));
    EXPECT_TRUE(Contains(text, "angle = \"rad\""));
    // STLのmmは出力単位 (inch) と異なるのでunitを書く
    EXPECT_TRUE(Contains(text, "unit = \"mm\""));
    // 内部値は往復で不変
    const auto restored = mc::ReadMachineDefinitionFromString(
            text, kFixturePath.parent_path(), "<inch>");
    EXPECT_TRUE(restored.warnings.empty());
    ExpectSameDefinition(original, restored);
    EXPECT_NEAR((*FindComponent(restored, "A").axis->limits)[1], ToRadians(90.0), kTol);
    EXPECT_TRUE(mc::TranslationPart(*FindComponent(restored, "Tool").frame_placement)
                        .isApprox(Vector3d(0.0, -180.0, 250.5), kTol));
    EXPECT_NEAR(restored.collision->default_clearance, 0.5, kTol);
}

TEST(MachineWriterTest, Collision_IsOmittedWhenAbsent) {
    auto definition = MakeMinimalDefinition();
    definition.collision.reset();
    const std::string text = mc::WriteMachineDefinitionToString(definition, kBaseDir);
    EXPECT_FALSE(Contains(text, "[collision]"));
    EXPECT_FALSE(RoundTrip(definition, kBaseDir).collision.has_value());
}

TEST(MachineWriterTest, Spindle_TableFollowsSpecPresence) {
    // テーブルの有無は`spindle`の有無に対応し、空のSpindleSpecは空テーブルとして往復する
    auto without = MakeMinimalDefinition();
    FindComponentMutable(without, "Spindle").spindle.reset();
    EXPECT_FALSE(Contains(mc::WriteMachineDefinitionToString(without, kBaseDir),
                          "[component.spindle]"));
    EXPECT_FALSE(FindComponent(RoundTrip(without, kBaseDir), "Spindle").spindle.has_value());

    auto empty = MakeMinimalDefinition();
    FindComponentMutable(empty, "Spindle").spindle = mc::SpindleSpec{};
    const std::string text = mc::WriteMachineDefinitionToString(empty, kBaseDir);
    EXPECT_TRUE(Contains(text, "[component.spindle]"));
    EXPECT_FALSE(Contains(text, "max_rpm"));
    const auto& restored = FindComponent(RoundTrip(empty, kBaseDir), "Spindle").spindle;
    ASSERT_TRUE(restored.has_value());
    EXPECT_FALSE(restored->max_rpm.has_value());
}

TEST(MachineWriterTest, Dynamics_FeedsAreWrittenPerMinute) {
    // 内部の毎秒 (mm/s・rad/s) をファイルの毎分へ戻し、加減速・分解能は毎秒基準のまま
    auto definition = MakeMinimalDefinition();
    auto& x = *FindComponentMutable(definition, "X").axis;
    x.dynamics.rapid_feed = 100.0;      // = 6000 mm/min
    x.dynamics.accel = 2.5;             // mm/s² (換算なし)
    x.dynamics.resolution = 0.001;      // mm (換算なし)
    const std::string text = mc::WriteMachineDefinitionToString(definition, kBaseDir);
    EXPECT_TRUE(Contains(text, "rapid_feed = 6000.0"));
    EXPECT_TRUE(Contains(text, "accel = 2.5"));
    EXPECT_TRUE(Contains(text, "resolution = 0.001"));
    const auto& restored = *FindComponent(RoundTrip(definition, kBaseDir), "X").axis;
    EXPECT_NEAR(*restored.dynamics.rapid_feed, 100.0, kTol);
    EXPECT_NEAR(*restored.dynamics.accel, 2.5, kTol);
    EXPECT_NEAR(*restored.dynamics.resolution, 0.001, kTol);
}

// ---- 正常系: 数値 ----

TEST(MachineWriterTest, Real_UsesShortestRoundTripDigits) {
    auto definition = MakeMinimalDefinition();
    FindComponentMutable(definition, "A").axis->point = Vector3d(0.1, 1.0 / 3.0, 250.5);
    const std::string text = mc::WriteMachineDefinitionToString(definition, kBaseDir);
    EXPECT_TRUE(Contains(text, "point = [0.1, 0.3333333333333333, 250.5]"));
    const auto restored = RoundTrip(definition, kBaseDir);
    EXPECT_NEAR(FindComponent(restored, "A").axis->point->y(), 1.0 / 3.0, 1e-15);
}

TEST(MachineWriterTest, Real_TinyValuesAreWrittenAsZero) {
    // 90°回転の列と、回転したlocal_frame相対に戻した原点には1e-16程度の値が現れる
    const std::string text =
            mc::WriteMachineDefinitionToString(MakeMinimalDefinition(), kBaseDir);
    EXPECT_FALSE(Contains(text, "e-1"));
    EXPECT_TRUE(Contains(text, "x_axis = [0.0, 1.0, 0.0]"));
    EXPECT_TRUE(Contains(text, "origin = [5.0, 0.0, 0.0]"));
    // 許容誤差 (1e-12) の境界: 内側は0、外側はそのまま
    auto definition = MakeMinimalDefinition();
    FindComponentMutable(definition, "A").axis->point = Vector3d(0.9e-12, 1.1e-12, 0.0);
    EXPECT_TRUE(Contains(mc::WriteMachineDefinitionToString(definition, kBaseDir),
                         "point = [0.0, 1.1e-12, 0.0]"));
}

TEST(MachineWriterTest, Initial_IsWrittenOnlyWhenNonZeroOrLimitsExcludeZero) {
    auto definition = MakeMinimalDefinition();
    auto& a_axis = *FindComponentMutable(definition, "A").axis;
    a_axis.initial = 0.0;
    EXPECT_FALSE(Contains(mc::WriteMachineDefinitionToString(definition, kBaseDir),
                          "initial"));
    a_axis.limits = std::array<double, 2>{ToRadians(10.0), ToRadians(90.0)};
    a_axis.initial = ToRadians(20.0);
    const std::string text = mc::WriteMachineDefinitionToString(definition, kBaseDir);
    EXPECT_TRUE(Contains(text, "limits = [10.0, 90.0]\ninitial = 20.0"));
    // limitsが0を含まない軸ではinitial = 0でも書く (読込側が省略を許さない)
    a_axis.initial = 0.0;
    EXPECT_TRUE(Contains(mc::WriteMachineDefinitionToString(definition, kBaseDir),
                         "initial = 0.0"));
}

// ---- 正常系 (境界値)・異常系: 形状の単位 ----

TEST(MachineWriterTest, Geometry_UnitIsWrittenWhenScaleDiffersFromOutputUnit) {
    const std::string text = mc::WriteMachineDefinitionToString(
            WithStlGeometry("models/x.STL", 25.4), kBaseDir);
    EXPECT_TRUE(Contains(text, "file = \"models/x.STL\"\nunit = \"inch\""));
    // 許容誤差 (1e-9) の内側は同じ単位とみなす
    EXPECT_NO_THROW(mc::WriteMachineDefinitionToString(
            WithStlGeometry("models/x.stl", 25.4 + 1e-10), kBaseDir));
}

TEST(MachineWriterTest, Geometry_ThrowsInvalidArgumentWhenFileUnitScaleUnsupported) {
    EXPECT_THROW(mc::WriteMachineDefinitionToString(
                         WithStlGeometry("models/x.stl", 25.4 + 1e-6), kBaseDir),
                 std::invalid_argument);
    EXPECT_THROW(mc::WriteMachineDefinitionToString(
                         WithStlGeometry("models/x.obj", 10.0), kBaseDir),
                 std::invalid_argument);
}

TEST(MachineWriterTest, Geometry_IgesIgnoresFileUnitScale) {
    const std::string text = mc::WriteMachineDefinitionToString(
            WithStlGeometry("models/x.igs", 25.4), kBaseDir);
    EXPECT_TRUE(Contains(text, "file = \"models/x.igs\"\n"));
    EXPECT_FALSE(Contains(text, "unit = "));
}

TEST(MachineWriterTest, Geometry_PathIsRelativizedWhenBaseDirDiffers) {
    // 読込時の基準 (C:/machines) と異なる出力先 (C:/machines/out) へ相対化する
    const auto definition = WithStlGeometry("models/x.stl", 1.0);
    EXPECT_TRUE(Contains(mc::WriteMachineDefinitionToString(definition, kBaseDir / "out"),
                         "file = \"../models/x.stl\""));
    // 相対化できない (別ドライブ) 場合は解決済みパスをそのまま書く
    EXPECT_TRUE(Contains(mc::WriteMachineDefinitionToString(definition, "D:/elsewhere"),
                         "file = \"C:/machines/models/x.stl\""));
}

TEST(MachineWriterTest, EmptyComponents_FormatsWithoutThrowing) {
    mc::MachineDefinition definition;
    definition.name = "empty";
    EXPECT_NO_THROW(mc::WriteMachineDefinitionToString(definition, kBaseDir));
}

// ---- ファイル出力 ----

TEST(MachineWriterTest, WriteFile_WritesAndReadsBack) {
    const fs::path dir = fs::temp_directory_path() / "igesio_machine_writer_test";
    fs::create_directories(dir);
    const fs::path path = dir / "written.toml";
    // 形状ファイルの相対パスは出力先基準になるため、プリミティブのみの定義で比較する
    const auto original = MakeMinimalDefinition();
    mc::WriteMachineDefinition(original, path);
    ASSERT_TRUE(fs::is_regular_file(path));
    const auto restored = mc::ReadMachineDefinition(path);
    EXPECT_EQ(restored.source_name, "written.toml");
    EXPECT_EQ(restored.source_dir, dir);
    EXPECT_TRUE(restored.warnings.empty());
    ExpectSameDefinition(original, restored);
    fs::remove_all(dir);
}

TEST(MachineWriterTest, WriteFile_ResolvesGeometryPathsFromOutputDirectory) {
    const fs::path dir = fs::temp_directory_path() / "igesio_machine_writer_test_paths";
    fs::create_directories(dir);
    const fs::path path = dir / "written.toml";
    const auto original = mc::ReadMachineDefinition(fs::absolute(kFixturePath));
    mc::WriteMachineDefinition(original, path);
    const auto restored = mc::ReadMachineDefinition(path);
    // 記載は変わるが、解決済みパスは元と同じ場所を指す
    EXPECT_EQ(std::get<fs::path>(FindComponent(restored, "cradle-frame").geometries[0].source),
              std::get<fs::path>(FindComponent(original, "cradle-frame").geometries[0].source));
    fs::remove_all(dir);
}

TEST(MachineWriterTest, WriteFile_ThrowsFileOpenErrorWhenPathIsDirectory) {
    const fs::path dir = fs::temp_directory_path() / "igesio_machine_writer_test_dir";
    fs::create_directories(dir);
    EXPECT_THROW(mc::WriteMachineDefinition(MakeMinimalDefinition(), dir),
                 igesio::FileOpenError);
    fs::remove_all(dir);
}
