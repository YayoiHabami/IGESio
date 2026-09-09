/**
 * @file tests/extensions/machines/machine/test_machine_io.cpp
 * @brief 機械定義の読込 (machine/machine_io) のテスト
 * @author Yayoi Habami
 * @date 2026-09-08
 * @copyright 2026 Yayoi Habami
 * @note 対象: ReadMachineDefinition / ReadMachineDefinitionFromString
 *       (書き出し WriteMachineDefinition / WriteMachineDefinitionToString は
 *       `test_machine_writer.cpp`)
 *       - 正常系 (実例): `t-ZYX-b-AC-w.toml`のコンポーネント順・フレーム・軸・
 *         干渉設定・形状・メタ情報、警告0件
 *       - 正常系 (単位): 既定単位で角度がradへ換算されること、inch・radで長さのみ
 *         換算されること、送り速度が毎分から毎秒へ換算されること、
 *         方向ベクトルは換算しないこと、整数リテラルの受理
 *       - 正常系 (要素): spindleテーブルの有無とmax_rpmの保持 (単位換算なし)、
 *         暗黙base、明示base、local_frame、回転3形式の一致、
 *         日付の2形式、形状の属性保持、STEPスキップ、IGES受理、拡張子の大文字、
 *         相対パスの解決
 *       - 正常系 (境界値): 単位ベクトルの許容誤差 (1e-3) の内側、initialがlimits端
 *       - 異常系: 仕様§5.1の各項目 (付録A) を代表1件ずつ、例外型と識別語で検証
 *       - 警告: minor版、可搬でないパス、右手系検査省略、両チェーン共通、exclude
 *       - 入力: 構文誤りにsource_nameが含まれること、ファイル不在
 *       TODO: 退化ケース (形状0個・コンポーネント最小構成) は`ThreeAxis`で兼ねる
 */
#include <gtest/gtest.h>

#include <cmath>
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
#include "./machines_for_testing.h"

namespace {

namespace fs = std::filesystem;
namespace mc = igesio::extensions::machines;
using igesio::Matrix3d;
using igesio::Matrix4d;
using igesio::Vector3d;
using mc::ToRadians;
using machines_test::kBaseDir;
using machines_test::kFixturePath;
using machines_test::FindComponent;
using machines_test::MinimalXyzAc;
using machines_test::Replace;
using machines_test::ThreeAxis;

/// @brief 数値比較の許容誤差
constexpr double kTol = 1e-12;

/// @brief 文字列入力で読み込む
mc::MachineDefinition ReadString(const std::string& toml) {
    return mc::ReadMachineDefinitionFromString(toml, kBaseDir, "<test>");
}

/// @brief `DataFormatError`が投げられ、メッセージに指定語を含むことを検証する
void ExpectDataFormatError(const std::string& toml, const std::string& keyword) {
    try {
        ReadString(toml);
        FAIL() << "DataFormatError was not thrown (expected: " << keyword << ")";
    } catch (const igesio::DataFormatError& e) {
        EXPECT_NE(std::string(e.what()).find(keyword), std::string::npos)
                << "message: " << e.what();
    }
}

/// @brief 警告がちょうど1件で、指定語を含むことを検証する
void ExpectSingleWarning(const mc::MachineDefinition& definition,
                         const std::string& keyword) {
    ASSERT_EQ(definition.warnings.size(), 1u);
    EXPECT_EQ(definition.warnings[0].severity, mc::Severity::kWarning);
    EXPECT_NE(definition.warnings[0].message.find(keyword), std::string::npos)
            << "message: " << definition.warnings[0].message;
}

/// @brief z軸まわり90°の回転行列
Matrix3d RotZ90() {
    return mc::RotationAboutAxis(Vector3d::UnitZ(), mc::kQuarterTurn);
}

}  // namespace



// ---- 正常系: 実例TOML ----

TEST(MachineReaderTest, Fixture_ReadsMetaAndComponentsInFileOrder) {
    const auto def = mc::ReadMachineDefinition(kFixturePath);
    EXPECT_EQ(def.name, "tool-ZYX-base-AC-work");
    EXPECT_EQ(def.author, "Yayoi Habami");
    EXPECT_EQ(def.date, "2026-08-31");
    EXPECT_EQ(def.format_version[0], 2);
    EXPECT_EQ(def.format_version[1], 0);
    EXPECT_EQ(def.source_name, "t-ZYX-b-AC-w.toml");
    EXPECT_EQ(def.source_dir, kFixturePath.parent_path());
    EXPECT_TRUE(def.warnings.empty());
    const std::vector<std::string> expected = {
            "cradle-frame", "A", "C", "Attach", "X", "Y", "Z", "Spindle", "Tool", "base"};
    ASSERT_EQ(def.components.size(), expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(def.components[i].name, expected[i]);
    }
    EXPECT_EQ(FindComponent(def, "base").type, mc::ComponentType::kBase);
    EXPECT_EQ(FindComponent(def, "cradle-frame").parent, "base");
    EXPECT_EQ(FindComponent(def, "Spindle").type, mc::ComponentType::kSpindle);
    EXPECT_GT(FindComponent(def, "A").line, 0);
}

TEST(MachineReaderTest, Fixture_MountFramesArePlacedInMachineCoordinates) {
    const auto def = mc::ReadMachineDefinition(kFixturePath);
    const auto& attach = FindComponent(def, "Attach");
    ASSERT_TRUE(attach.frame_placement.has_value());
    EXPECT_TRUE(attach.frame_placement->isApprox(Matrix4d::Identity(), kTol));
    const auto& tool = FindComponent(def, "Tool");
    ASSERT_TRUE(tool.frame_placement.has_value());
    EXPECT_TRUE(mc::RotationPart(*tool.frame_placement)
                        .isApprox(Matrix3d::Identity(), kTol));
    EXPECT_TRUE(mc::TranslationPart(*tool.frame_placement)
                        .isApprox(Vector3d(0.0, -180.0, 250.5), kTol));
}

TEST(MachineReaderTest, Fixture_RotaryAxesAreConvertedToRadians) {
    const auto def = mc::ReadMachineDefinition(kFixturePath);
    const auto& a = *FindComponent(def, "A").axis;
    EXPECT_EQ(a.register_name, "A");
    EXPECT_TRUE(a.direction.isApprox(Vector3d::UnitX(), kTol));
    ASSERT_TRUE(a.point.has_value());
    EXPECT_TRUE(a.point->isApprox(Vector3d(0.0, 0.0, 60.0), kTol));
    ASSERT_TRUE(a.limits.has_value());
    EXPECT_NEAR((*a.limits)[0], ToRadians(-90.0), kTol);
    EXPECT_NEAR((*a.limits)[1], ToRadians(90.0), kTol);
    EXPECT_FALSE(a.unlimited);
    // 送り速度はファイルのdeg/minから内部のrad/sへ (3600 deg/min = 60 deg/s)
    ASSERT_TRUE(a.dynamics.rapid_feed.has_value());
    EXPECT_NEAR(*a.dynamics.rapid_feed, ToRadians(60.0), kTol);
    EXPECT_NEAR(*a.dynamics.min_feed, ToRadians(1.0) / mc::kSecondsPerMinute, kTol);
    EXPECT_FALSE(a.dynamics.accel.has_value());
    const auto& c = *FindComponent(def, "C").axis;
    EXPECT_TRUE(c.unlimited);
    EXPECT_FALSE(c.limits.has_value());
    ASSERT_TRUE(c.wrap_start.has_value());
    EXPECT_NEAR(*c.wrap_start, 0.0, kTol);
}

TEST(MachineReaderTest, Fixture_LinearAxesKeepMillimeters) {
    const auto def = mc::ReadMachineDefinition(kFixturePath);
    const auto& y = *FindComponent(def, "Y").axis;
    ASSERT_TRUE(y.limits.has_value());
    EXPECT_NEAR((*y.limits)[0], 0.0, kTol);
    EXPECT_NEAR((*y.limits)[1], 300.0, kTol);
    EXPECT_NEAR(y.initial, 0.0, kTol);
    // 送り速度はmm/minからmm/sへ (6000 mm/min = 100 mm/s)
    const auto& x = *FindComponent(def, "X").axis;
    EXPECT_NEAR(*x.dynamics.rapid_feed, 100.0, kTol);
    EXPECT_NEAR(*x.dynamics.max_feed, 100.0, kTol);
    EXPECT_NEAR(*x.dynamics.min_feed, 1.0 / mc::kSecondsPerMinute, kTol);
}

TEST(MachineReaderTest, Fixture_CollisionAndKinematicsAreKept) {
    const auto def = mc::ReadMachineDefinition(kFixturePath);
    EXPECT_EQ(def.branch, mc::BranchPolicy::kPositive);
    ASSERT_TRUE(def.collision.has_value());
    EXPECT_EQ(def.collision->mode, mc::CollisionMode::kPairs);
    EXPECT_NEAR(def.collision->default_clearance, 0.5, kTol);
    EXPECT_TRUE(def.collision->exclude.empty());
    ASSERT_EQ(def.collision->pairs.size(), 4u);
    EXPECT_EQ(def.collision->pairs[0].targets[0], "Spindle");
    EXPECT_EQ(def.collision->pairs[0].targets[1], "cradle-frame");
    EXPECT_FALSE(def.collision->pairs[0].subtree[0]);
    EXPECT_NEAR(def.collision->pairs[0].clearance, 0.5, kTol);
    EXPECT_TRUE(def.collision->pairs[0].enabled);
    EXPECT_EQ(def.collision->pairs[3].targets[0], "Z");
    EXPECT_EQ(def.collision->pairs[3].targets[1], "A");
    EXPECT_TRUE(def.collision->pairs[3].subtree[0]);
    EXPECT_TRUE(def.collision->pairs[3].subtree[1]);
}

TEST(MachineReaderTest, Fixture_GeometriesResolvePathsAndColors) {
    const auto def = mc::ReadMachineDefinition(kFixturePath);
    const auto& frame = FindComponent(def, "cradle-frame");
    ASSERT_EQ(frame.geometries.size(), 1u);
    const auto& geometry = frame.geometries[0];
    EXPECT_EQ(geometry.name, "cradle frame");
    EXPECT_EQ(geometry.raw_path, "tool-ZYX-base-AC-work/cradle-frame.STL");
    ASSERT_TRUE(std::holds_alternative<fs::path>(geometry.source));
    EXPECT_EQ(std::get<fs::path>(geometry.source),
              (kFixturePath.parent_path() / "tool-ZYX-base-AC-work/cradle-frame.STL")
                      .lexically_normal());
    EXPECT_NEAR(geometry.file_unit_scale, 1.0, kTol);
    ASSERT_TRUE(geometry.color.has_value());
    EXPECT_NEAR((*geometry.color)[0], 0xe0 / 255.0, 1e-6);
    EXPECT_NEAR((*geometry.color)[1], 0xd6 / 255.0, 1e-6);
    EXPECT_NEAR((*geometry.color)[2], 0xc8 / 255.0, 1e-6);
    EXPECT_TRUE(geometry.placement.isApprox(Matrix4d::Identity(), kTol));
    EXPECT_TRUE(geometry.visible);
    EXPECT_TRUE(geometry.collision);
    EXPECT_NEAR(geometry.opacity, 1.0, 1e-6);
    // 形状を持たないコンポーネント (Spindle) は空
    EXPECT_TRUE(FindComponent(def, "Spindle").geometries.empty());
}

// ---- 正常系: 単位 ----

TEST(MachineReaderTest, DefaultUnits_ConvertAnglesToRadiansAndKeepLengths) {
    const auto def = ReadString(MinimalXyzAc());
    EXPECT_NEAR(def.units.angle, mc::kDegreeToRadian, kTol);
    EXPECT_NEAR(def.units.length, 1.0, kTol);
    const auto& a = *FindComponent(def, "A").axis;
    EXPECT_NEAR((*a.limits)[0], ToRadians(-90.0), kTol);
    EXPECT_NEAR((*a.limits)[1], ToRadians(90.0), kTol);
    EXPECT_NEAR(a.point->z(), 60.0, kTol);
    const auto& x = *FindComponent(def, "X").axis;
    EXPECT_NEAR((*x.limits)[0], -400.0, kTol);
    EXPECT_NEAR((*x.limits)[1], 400.0, kTol);
}

TEST(MachineReaderTest, InchAndRadianUnits_ScaleLengthsOnly) {
    std::string toml = Replace(MinimalXyzAc(), "[machine]\nname = \"minimal-xyz-ac\"\n",
                               "[machine]\nname = \"minimal-xyz-ac\"\n\n"
                               "[units]\nlength = \"inch\"\nangle = \"rad\"\n");
    toml = Replace(toml, "limits = [-90, 90]\n",
                   "limits = [-1, 1]\ninitial = 0.5\n"
                   "[component.axis.dynamics]\nrapid_feed = 100\n");
    toml = Replace(toml, "limits = [-400, 400]\n",
                   "limits = [-400, 400]\n[component.axis.dynamics]\nrapid_feed = 100\n");
    toml += "\n[collision]\ndefault_clearance = 0.5\n";
    const auto def = ReadString(toml);
    EXPECT_EQ(def.units.length_unit, mc::LengthUnit::kInch);
    EXPECT_EQ(def.units.angle_unit, mc::AngleUnit::kRadian);
    const auto& a = *FindComponent(def, "A").axis;
    EXPECT_NEAR((*a.limits)[0], -1.0, kTol);           // radは無換算
    EXPECT_NEAR(a.initial, 0.5, kTol);
    // 角度は無換算でも、送り速度は毎分→毎秒の換算を受ける
    EXPECT_NEAR(*a.dynamics.rapid_feed, 100.0 / mc::kSecondsPerMinute, kTol);
    EXPECT_NEAR(a.point->z(), 60.0 * 25.4, kTol);      // 長さは25.4倍
    EXPECT_TRUE(a.direction.isApprox(Vector3d::UnitX(), kTol));   // 方向は不変
    const auto& x = FindComponent(def, "X");
    EXPECT_NEAR((*x.axis->limits)[1], 400.0 * 25.4, kTol);
    EXPECT_NEAR(*x.axis->dynamics.rapid_feed, 100.0 * 25.4 / mc::kSecondsPerMinute, kTol);
    const auto& box = std::get<mc::PrimitiveSpec>(x.geometries[0].source);
    EXPECT_TRUE(box.size.isApprox(Vector3d(10.0, 20.0, 30.0) * 25.4, kTol));
    EXPECT_TRUE(mc::TranslationPart(*FindComponent(def, "Tool").frame_placement)
                        .isApprox(Vector3d(0.0, -180.0, 250.5) * 25.4, kTol));
    EXPECT_NEAR(def.collision->default_clearance, 0.5 * 25.4, kTol);
}

TEST(MachineReaderTest, IntegerLiterals_AreAcceptedAsReals) {
    // MinimalXyzAcのlimits・point・originは全て整数リテラル
    const auto def = ReadString(MinimalXyzAc());
    const auto& c = *FindComponent(def, "C").axis;
    EXPECT_TRUE(c.point->isApprox(Vector3d::Zero(), kTol));
    EXPECT_NEAR(*c.wrap_start, 0.0, kTol);
    const auto& z = *FindComponent(def, "Z").axis;
    EXPECT_NEAR((*z.limits)[1], 300.0, kTol);
}

// ---- 正常系: 要素 ----

TEST(MachineReaderTest, Spindle_IsAbsentWithoutTable) {
    // 実例・最小構成ともに[component.spindle]を持たない
    EXPECT_FALSE(FindComponent(mc::ReadMachineDefinition(kFixturePath), "Spindle")
                         .spindle.has_value());
    EXPECT_FALSE(FindComponent(ReadString(MinimalXyzAc()), "Spindle").spindle.has_value());
}

TEST(MachineReaderTest, Spindle_KeepsMaxRpmWithoutUnitConversion) {
    const std::string toml = Replace(MinimalXyzAc(), "type = \"spindle\"\nparent = \"Z\"\n",
                                     "type = \"spindle\"\nparent = \"Z\"\n\n"
                                     "[component.spindle]\nmax_rpm = 12000\n");
    const auto& spindle = FindComponent(ReadString(toml), "Spindle").spindle;
    ASSERT_TRUE(spindle.has_value());
    ASSERT_TRUE(spindle->max_rpm.has_value());
    EXPECT_NEAR(*spindle->max_rpm, 12000.0, kTol);   // min⁻¹のまま
}

TEST(MachineReaderTest, Spindle_EmptyTableYieldsSpecWithoutValues) {
    const std::string toml = Replace(MinimalXyzAc(), "type = \"spindle\"\nparent = \"Z\"\n",
                                     "type = \"spindle\"\nparent = \"Z\"\n\n"
                                     "[component.spindle]\n");
    const auto& spindle = FindComponent(ReadString(toml), "Spindle").spindle;
    ASSERT_TRUE(spindle.has_value());
    EXPECT_FALSE(spindle->max_rpm.has_value());
}

TEST(MachineReaderTest, ImplicitBase_IsAppendedLast) {
    const auto def = ReadString(MinimalXyzAc());
    ASSERT_FALSE(def.components.empty());
    const auto& base = def.components.back();
    EXPECT_EQ(base.name, "base");
    EXPECT_EQ(base.type, mc::ComponentType::kBase);
    EXPECT_TRUE(base.parent.empty());
    EXPECT_EQ(base.line, 0);
    EXPECT_TRUE(base.local_frame.isApprox(Matrix4d::Identity(), kTol));
    EXPECT_FALSE(base.axis.has_value());
    EXPECT_TRUE(base.geometries.empty());
}

TEST(MachineReaderTest, ExplicitBase_AcceptsGeometryAndLocalFrame) {
    const std::string toml = MinimalXyzAc() + R"(
[[component]]
name = "base"
type = "base"

[component.local_frame]
origin = [0, 0, -50]

[[component.geometry]]
primitive = "cylinder"
radius = 5
height = 20
)";
    const auto def = ReadString(toml);
    const auto& base = FindComponent(def, "base");
    EXPECT_EQ(base.type, mc::ComponentType::kBase);
    EXPECT_GT(base.line, 0);
    EXPECT_TRUE(mc::TranslationPart(base.local_frame).isApprox(Vector3d(0, 0, -50), kTol));
    ASSERT_EQ(base.geometries.size(), 1u);
    const auto& cylinder = std::get<mc::PrimitiveSpec>(base.geometries[0].source);
    EXPECT_EQ(cylinder.kind, mc::PrimitiveSpec::Kind::kCylinder);
    EXPECT_NEAR(cylinder.radius, 5.0, kTol);
    EXPECT_NEAR(cylinder.height, 20.0, kTol);
    // 形状の配置はlocal_frameを受ける
    EXPECT_TRUE(mc::TranslationPart(base.geometries[0].placement)
                        .isApprox(Vector3d(0, 0, -50), kTol));
}

TEST(MachineReaderTest, LocalFrame_AppliesToFrameAndIsKeptForAxis) {
    // Toolにlocal_frame (原点(1,2,3)・z軸まわり90°) を与える
    std::string toml = Replace(
            MinimalXyzAc(), "parent = \"Spindle\"\n",
            "parent = \"Spindle\"\n\n[component.local_frame]\norigin = [1, 2, 3]\n"
            "rotation_axis_angle = { axis = [0, 0, 1], angle = 90 }\n");
    // Aにも同じlocal_frame回転を与える (directionはローカルのまま保持される)
    toml = Replace(toml, "parent = \"base\"\n",
                   "parent = \"base\"\n\n[component.local_frame]\n"
                   "rotation_euler_ijk = [0, 0, 90]\n");
    const auto def = ReadString(toml);
    const auto& tool = FindComponent(def, "Tool");
    const Matrix3d r = RotZ90();
    EXPECT_TRUE(mc::RotationPart(tool.local_frame).isApprox(r, kTol));
    EXPECT_TRUE(mc::RotationPart(*tool.frame_placement).isApprox(r, kTol));
    const Vector3d expected = r * Vector3d(0.0, -180.0, 250.5) + Vector3d(1, 2, 3);
    EXPECT_TRUE(mc::TranslationPart(*tool.frame_placement).isApprox(expected, kTol));
    const auto& a = FindComponent(def, "A");
    EXPECT_TRUE(mc::RotationPart(a.local_frame).isApprox(r, kTol));
    EXPECT_TRUE(a.axis->direction.isApprox(Vector3d::UnitX(), kTol));
    EXPECT_TRUE(a.axis->point->isApprox(Vector3d(0, 0, 60), kTol));
}

TEST(MachineReaderTest, RotationForms_AgreeForSamePose) {
    const std::string toml = Replace(
            MinimalXyzAc(), "size = [10, 20, 30]\n",
            "size = [10, 20, 30]\norigin = [1, 0, 0]\n"
            "rotation = { x_axis = [0, 1, 0], y_axis = [-1, 0, 0], z_axis = [0, 0, 1] }\n"
            "\n[[component.geometry]]\nprimitive = \"box\"\nsize = [1, 1, 1]\n"
            "origin = [1, 0, 0]\nrotation_axis_angle = { axis = [0, 0, 1], angle = 90 }\n"
            "\n[[component.geometry]]\nprimitive = \"box\"\nsize = [1, 1, 1]\n"
            "origin = [1, 0, 0]\nrotation_euler_ijk = [0, 0, 90]\n");
    const auto def = ReadString(toml);
    const auto& x = FindComponent(def, "X");
    ASSERT_EQ(x.geometries.size(), 3u);
    const Matrix4d expected = mc::MakeRigid(RotZ90(), Vector3d(1, 0, 0));
    for (const auto& geometry : x.geometries) {
        EXPECT_TRUE(geometry.placement.isApprox(expected, kTol))
                << "actual:\n" << geometry.placement;
    }
}

TEST(MachineReaderTest, Date_AcceptsNativeDateAndString) {
    const std::string native = Replace(MinimalXyzAc(), "name = \"minimal-xyz-ac\"\n",
                                       "name = \"minimal-xyz-ac\"\ndate = 2026-08-31\n");
    EXPECT_EQ(ReadString(native).date, "2026-08-31");
    const std::string text = Replace(MinimalXyzAc(), "name = \"minimal-xyz-ac\"\n",
                                     "name = \"minimal-xyz-ac\"\ndate = \"2026-08-31\"\n");
    EXPECT_EQ(ReadString(text).date, "2026-08-31");
    EXPECT_TRUE(ReadString(MinimalXyzAc()).date.empty());
}

TEST(MachineReaderTest, Geometry_KeepsVisibilityCollisionOpacityAndColor) {
    const std::string toml = Replace(
            MinimalXyzAc(), "size = [10, 20, 30]\n",
            "size = [10, 20, 30]\nvisible = false\ncollision = false\n"
            "opacity = 0.5\ncolor = \"#FF8000\"\n");
    const auto def = ReadString(toml);
    const auto& geometry = FindComponent(def, "X").geometries.at(0);
    EXPECT_FALSE(geometry.visible);
    EXPECT_FALSE(geometry.collision);
    EXPECT_NEAR(geometry.opacity, 0.5, 1e-6);
    ASSERT_TRUE(geometry.color.has_value());
    EXPECT_NEAR((*geometry.color)[0], 1.0, 1e-6);
    EXPECT_NEAR((*geometry.color)[1], 0x80 / 255.0, 1e-6);
    EXPECT_NEAR((*geometry.color)[2], 0.0, 1e-6);
}

TEST(MachineReaderTest, Geometry_StepIsSkippedWithWarning) {
    const std::string toml = Replace(MinimalXyzAc(),
                                     "primitive = \"box\"\nsize = [10, 20, 30]\n",
                                     "file = \"models/x.step\"\n");
    const auto def = ReadString(toml);
    EXPECT_TRUE(FindComponent(def, "X").geometries.empty());
    ExpectSingleWarning(def, "skipped");
    EXPECT_EQ(def.warnings[0].context, "component[X].geometry[0]");
    EXPECT_GT(def.warnings[0].line, 0);
}

TEST(MachineReaderTest, Geometry_IgesIsAcceptedWithoutUnit) {
    const std::string toml = Replace(MinimalXyzAc(),
                                     "primitive = \"box\"\nsize = [10, 20, 30]\n",
                                     "file = \"models/x.igs\"\n");
    const auto def = ReadString(toml);
    const auto& geometry = FindComponent(def, "X").geometries.at(0);
    EXPECT_EQ(std::get<fs::path>(geometry.source),
              (kBaseDir / "models/x.igs").lexically_normal());
    EXPECT_NEAR(geometry.file_unit_scale, 1.0, kTol);
    EXPECT_TRUE(def.warnings.empty());
}

TEST(MachineReaderTest, Geometry_UpperCaseExtensionAndInchUnit) {
    const std::string toml = Replace(MinimalXyzAc(),
                                     "primitive = \"box\"\nsize = [10, 20, 30]\n",
                                     "file = \"models/x.STL\"\nunit = \"inch\"\n");
    const auto def = ReadString(toml);
    const auto& geometry = FindComponent(def, "X").geometries.at(0);
    EXPECT_EQ(geometry.raw_path, "models/x.STL");
    EXPECT_NEAR(geometry.file_unit_scale, 25.4, kTol);
}

TEST(MachineReaderTest, Geometry_UnitDefaultsToDeclaredLengthUnit) {
    std::string toml = Replace(MinimalXyzAc(), "[machine]\nname = \"minimal-xyz-ac\"\n",
                               "[machine]\nname = \"minimal-xyz-ac\"\n\n"
                               "[units]\nlength = \"inch\"\n");
    toml = Replace(toml, "primitive = \"box\"\nsize = [10, 20, 30]\n",
                   "file = \"models/x.obj\"\n");
    const auto def = ReadString(toml);
    EXPECT_NEAR(FindComponent(def, "X").geometries.at(0).file_unit_scale, 25.4, kTol);
}

TEST(MachineReaderTest, Geometry_RelativePathResolvesFromBaseDir) {
    const std::string toml = Replace(MinimalXyzAc(),
                                     "primitive = \"box\"\nsize = [10, 20, 30]\n",
                                     "file = \"sub/./part.stl\"\n");
    const auto def = ReadString(toml);
    const auto& geometry = FindComponent(def, "X").geometries.at(0);
    EXPECT_EQ(std::get<fs::path>(geometry.source),
              (kBaseDir / "sub/part.stl").lexically_normal());
    EXPECT_TRUE(def.warnings.empty());
}

TEST(MachineReaderTest, ThreeAxis_ReadsWithoutRotaryAxes) {
    const auto def = ReadString(ThreeAxis());
    EXPECT_EQ(def.components.size(), 6u);
    EXPECT_TRUE(def.warnings.empty());
    EXPECT_FALSE(def.collision.has_value());
    EXPECT_EQ(def.branch, mc::BranchPolicy::kPositive);
}

// ---- 正常系 (境界値) ----

TEST(MachineReaderTest, UnitVector_JustInsideToleranceIsRenormalized) {
    const std::string toml = Replace(MinimalXyzAc(), "direction = [1, 0, 0]\npoint",
                                     "direction = [1.0009, 0, 0]\npoint");
    const auto def = ReadString(toml);
    EXPECT_TRUE(FindComponent(def, "A").axis->direction.isApprox(Vector3d::UnitX(), kTol));
}

TEST(MachineReaderTest, Initial_AtLimitBoundaryIsAccepted) {
    const std::string toml = Replace(MinimalXyzAc(), "limits = [-90, 90]\n",
                                     "limits = [-90, 90]\ninitial = 90\n");
    const auto def = ReadString(toml);
    EXPECT_NEAR(FindComponent(def, "A").axis->initial, ToRadians(90.0), kTol);
}

// ---- 異常系: [format]・[machine]・[units] ----

TEST(MachineReaderTest, Format_ThrowsDataFormatErrorWhenNameMismatch) {
    ExpectDataFormatError(Replace(MinimalXyzAc(), "machine-definition", "cspace-machine"),
                          "[format].name");
}

TEST(MachineReaderTest, Format_ThrowsDataFormatErrorWhenVersionMalformed) {
    ExpectDataFormatError(Replace(MinimalXyzAc(), "version = [2, 0]", "version = [2.0, 0]"),
                          "version");
    ExpectDataFormatError(Replace(MinimalXyzAc(), "version = [2, 0]", "version = [2]"),
                          "version");
}

TEST(MachineReaderTest, Format_ThrowsDataFormatErrorWhenMajorUnsupported) {
    ExpectDataFormatError(Replace(MinimalXyzAc(), "version = [2, 0]", "version = [3, 0]"),
                          "unsupported format version");
}

TEST(MachineReaderTest, Format_WarnsWhenMinorIsNewer) {
    const auto def = ReadString(Replace(MinimalXyzAc(), "version = [2, 0]",
                                        "version = [2, 1]"));
    ExpectSingleWarning(def, "minor");
    EXPECT_EQ(def.warnings[0].context, "[format]");
    EXPECT_EQ(def.format_version[1], 1);
}

TEST(MachineReaderTest, Machine_ThrowsDataFormatErrorWhenNameMissing) {
    ExpectDataFormatError(Replace(MinimalXyzAc(), "name = \"minimal-xyz-ac\"\n", ""),
                          "[machine].name");
}

TEST(MachineReaderTest, Units_ThrowsDataFormatErrorWhenUnknown) {
    ExpectDataFormatError(Replace(MinimalXyzAc(), "[machine]\n",
                                  "[units]\nlength = \"cm\"\n\n[machine]\n"),
                          "[units].length");
    ExpectDataFormatError(Replace(MinimalXyzAc(), "[machine]\n",
                                  "[units]\nangle = \"grad\"\n\n[machine]\n"),
                          "[units].angle");
}

// ---- 異常系: コンポーネント構造 ----

TEST(MachineReaderTest, Component_ThrowsDataFormatErrorWhenNameDuplicated) {
    ExpectDataFormatError(Replace(MinimalXyzAc(), "name = \"Y\"", "name = \"X\""),
                          "duplicate component name");
}

TEST(MachineReaderTest, Component_ThrowsDataFormatErrorWhenNameReserved) {
    ExpectDataFormatError(Replace(MinimalXyzAc(), "name = \"Spindle\"", "name = \"tool\""),
                          "reserved name");
}

TEST(MachineReaderTest, Component_ThrowsDataFormatErrorWhenParentMissing) {
    ExpectDataFormatError(Replace(MinimalXyzAc(), "parent = \"Y\"", "parent = \"Q\""),
                          "parent does not exist");
    ExpectDataFormatError(Replace(MinimalXyzAc(), "parent = \"Y\"\n", ""),
                          "parent is missing");
}

TEST(MachineReaderTest, Component_ThrowsDataFormatErrorWhenCyclic) {
    // A.parent = "C" かつ C.parent = "A"
    ExpectDataFormatError(Replace(MinimalXyzAc(), "parent = \"base\"", "parent = \"C\""),
                          "cycle");
}

TEST(MachineReaderTest, Component_ThrowsDataFormatErrorWhenTypeUnknown) {
    ExpectDataFormatError(Replace(MinimalXyzAc(), "type = \"spindle\"", "type = \"motor\""),
                          "unknown type");
}

TEST(MachineReaderTest, Component_ThrowsDataFormatErrorWhenSubTableMismatch) {
    // linearにaxisがない
    ExpectDataFormatError(Replace(MinimalXyzAc(), "type = \"spindle\"", "type = \"linear\""),
                          "presence of axis");
    // fixedにframe
    ExpectDataFormatError(Replace(MinimalXyzAc(), "type = \"work_mount\"", "type = \"fixed\""),
                          "presence of frame");
}

TEST(MachineReaderTest, Component_ThrowsDataFormatErrorWhenMountCountIsNotOne) {
    // Toolをwork_mountにする → tool_mount 0個 (work_mount 2個より先に検出される)
    ExpectDataFormatError(Replace(MinimalXyzAc(), "type = \"tool_mount\"",
                                  "type = \"work_mount\""),
                          "exactly one tool_mount");
    // Tableをfixedにする (frameも外す) → work_mount 0個
    ExpectDataFormatError(Replace(MinimalXyzAc(),
                                  "type = \"work_mount\"\nparent = \"C\"\n\n"
                                  "[component.frame]\norigin = [0, 0, 0]\n",
                                  "type = \"fixed\"\nparent = \"C\"\n"),
                          "exactly one work_mount");
}

TEST(MachineReaderTest, Component_ThrowsDataFormatErrorWhenMountHasChild) {
    ExpectDataFormatError(Replace(MinimalXyzAc(), "parent = \"Spindle\"", "parent = \"Table\""),
                          "cannot have children");
}

TEST(MachineReaderTest, Component_ThrowsDataFormatErrorWhenSpindleMisplaced) {
    const std::string two = MinimalXyzAc()
            + "\n[[component]]\nname = \"S2\"\ntype = \"spindle\"\nparent = \"Z\"\n";
    ExpectDataFormatError(two, "at most one spindle");
    // ワーク側にspindle: Spindleをbase直下へ移すとToolも外れるので、A配下の別spindleを作る
    const std::string work_side = Replace(
            Replace(MinimalXyzAc(), "name = \"Spindle\"\ntype = \"spindle\"\nparent = \"Z\"\n",
                    "name = \"Spindle\"\ntype = \"spindle\"\nparent = \"A\"\n"),
            "parent = \"Spindle\"", "parent = \"Z\"");
    ExpectDataFormatError(work_side, "ancestor of the tool_mount");
}

// ---- 異常系: axis ----

TEST(MachineReaderTest, Axis_ThrowsDataFormatErrorWhenLimitsAndUnlimitedNotExclusive) {
    ExpectDataFormatError(Replace(MinimalXyzAc(), "limits = [-90, 90]\n",
                                  "limits = [-90, 90]\nunlimited = true\n"),
                          "exactly one of limits");
    ExpectDataFormatError(Replace(MinimalXyzAc(), "limits = [-90, 90]\n", ""),
                          "exactly one of limits");
}

TEST(MachineReaderTest, Axis_ThrowsDataFormatErrorWhenLimitsNotIncreasing) {
    ExpectDataFormatError(Replace(MinimalXyzAc(), "limits = [-90, 90]", "limits = [90, 90]"),
                          "min < max");
    ExpectDataFormatError(Replace(MinimalXyzAc(), "limits = [-90, 90]", "limits = [-90]"),
                          "two real numbers");
}

TEST(MachineReaderTest, Axis_ThrowsDataFormatErrorWhenLinearHasPoint) {
    ExpectDataFormatError(Replace(MinimalXyzAc(), "limits = [-400, 400]\n",
                                  "limits = [-400, 400]\npoint = [0, 0, 0]\n"),
                          "only for rotary");
}

TEST(MachineReaderTest, Axis_ThrowsDataFormatErrorWhenWrapStartOnLimitedAxis) {
    ExpectDataFormatError(Replace(MinimalXyzAc(), "limits = [-90, 90]\n",
                                  "limits = [-90, 90]\nwrap_start = 0\n"),
                          "wrap_start");
}

TEST(MachineReaderTest, Axis_ThrowsDataFormatErrorWhenInitialOutOfLimits) {
    ExpectDataFormatError(Replace(MinimalXyzAc(), "limits = [-90, 90]\n",
                                  "limits = [-90, 90]\ninitial = 90.001\n"),
                          "initial is outside limits");
}

TEST(MachineReaderTest, Axis_ThrowsDataFormatErrorWhenInitialOmittedOutsideLimits) {
    ExpectDataFormatError(Replace(MinimalXyzAc(), "limits = [-90, 90]", "limits = [10, 90]"),
                          "initial cannot be omitted");
}

TEST(MachineReaderTest, Axis_ThrowsDataFormatErrorWhenDynamicsInvalid) {
    ExpectDataFormatError(Replace(MinimalXyzAc(), "limits = [-90, 90]\n",
                                  "limits = [-90, 90]\n[component.axis.dynamics]\n"
                                  "min_feed = 10\nmax_feed = 5\n"),
                          "min_feed");
    ExpectDataFormatError(Replace(MinimalXyzAc(), "limits = [-90, 90]\n",
                                  "limits = [-90, 90]\n[component.axis.dynamics]\n"
                                  "rapid_feed = 0\n"),
                          "not positive");
}

TEST(MachineReaderTest, Spindle_ThrowsDataFormatErrorWhenMaxRpmNotPositive) {
    ExpectDataFormatError(Replace(MinimalXyzAc(), "type = \"spindle\"\nparent = \"Z\"\n",
                                  "type = \"spindle\"\nparent = \"Z\"\n\n"
                                  "[component.spindle]\nmax_rpm = 0\n"),
                          "max_rpm");
}

TEST(MachineReaderTest, Axis_ThrowsDataFormatErrorWhenRegisterDuplicated) {
    ExpectDataFormatError(Replace(MinimalXyzAc(), "register = \"Y\"", "register = \"X\""),
                          "duplicate register");
}

TEST(MachineReaderTest, Axis_ThrowsDataFormatErrorWhenDirectionNotUnit) {
    ExpectDataFormatError(Replace(MinimalXyzAc(), "direction = [1, 0, 0]\npoint",
                                  "direction = [1.0011, 0, 0]\npoint"),
                          "not a unit vector");
}

// ---- 異常系: frame ----

TEST(MachineReaderTest, Frame_ThrowsDataFormatErrorWhenOriginMissing) {
    ExpectDataFormatError(Replace(MinimalXyzAc(), "origin = [0, -180, 250.5]\n", ""),
                          "origin");
}

TEST(MachineReaderTest, Frame_ThrowsDataFormatErrorWhenRotationAndAxesMixed) {
    ExpectDataFormatError(Replace(MinimalXyzAc(), "z_axis = [0, 0, 1]\n",
                                  "z_axis = [0, 0, 1]\nrotation_euler_ijk = [0, 0, 0]\n"),
                          "exclusive");
}

TEST(MachineReaderTest, Frame_ThrowsDataFormatErrorWhenXAxisNotOrthogonal) {
    ExpectDataFormatError(Replace(MinimalXyzAc(), "z_axis = [0, 0, 1]\n",
                                  "z_axis = [0, 0, 1]\nx_axis = [0.8, 0, 0.6]\n"),
                          "not orthogonal");
}

TEST(MachineReaderTest, Frame_ThrowsDataFormatErrorWhenDefaultXAxisDegenerates) {
    ExpectDataFormatError(Replace(MinimalXyzAc(), "z_axis = [0, 0, 1]\n", "z_axis = [1, 0, 0]\n"),
                          "cannot determine the x axis");
}

TEST(MachineReaderTest, Frame_AcceptsExplicitXAxisWithProjection) {
    // x_axisは許容誤差内の直交ずれを射影で吸収する
    const std::string toml = Replace(MinimalXyzAc(), "z_axis = [0, 0, 1]\n",
                                     "z_axis = [1, 0, 0]\nx_axis = [0.0005, 1, 0]\n");
    const auto def = ReadString(toml);
    const Matrix3d r = mc::RotationPart(*FindComponent(def, "Tool").frame_placement);
    EXPECT_TRUE(r.col(2).isApprox(Vector3d::UnitX(), kTol));
    EXPECT_TRUE(r.col(0).isApprox(Vector3d::UnitY(), 1e-9));
    EXPECT_TRUE(r.col(1).isApprox(Vector3d::UnitZ(), 1e-9));
}

// ---- 異常系: geometry ----

TEST(MachineReaderTest, Geometry_ThrowsDataFormatErrorWhenSourceNotExclusive) {
    ExpectDataFormatError(Replace(MinimalXyzAc(), "primitive = \"box\"\n",
                                  "primitive = \"box\"\nfile = \"x.stl\"\n"),
                          "exactly one of file and primitive");
    ExpectDataFormatError(Replace(MinimalXyzAc(), "primitive = \"box\"\nsize = [10, 20, 30]\n",
                                  "color = \"#000000\"\n"),
                          "exactly one of file and primitive");
}

TEST(MachineReaderTest, Geometry_ThrowsDataFormatErrorWhenOpacityOutOfRange) {
    ExpectDataFormatError(Replace(MinimalXyzAc(), "size = [10, 20, 30]\n",
                                  "size = [10, 20, 30]\nopacity = 1.5\n"),
                          "opacity");
}

TEST(MachineReaderTest, Geometry_ThrowsDataFormatErrorWhenPathInvalid) {
    ExpectDataFormatError(Replace(MinimalXyzAc(), "primitive = \"box\"\nsize = [10, 20, 30]\n",
                                  "file = 'models\\x.stl'\n"),
                          "path separator");
    ExpectDataFormatError(Replace(MinimalXyzAc(), "primitive = \"box\"\nsize = [10, 20, 30]\n",
                                  "file = \"models/x.3mf\"\n"),
                          "unsupported model format");
    ExpectDataFormatError(Replace(MinimalXyzAc(), "primitive = \"box\"\nsize = [10, 20, 30]\n",
                                  "file = \"models/x.iges\"\nunit = \"mm\"\n"),
                          "unit");
    ExpectDataFormatError(Replace(MinimalXyzAc(), "primitive = \"box\"\nsize = [10, 20, 30]\n",
                                  "file = \"models/x.stl\"\nunit = \"cm\"\n"),
                          "unknown unit");
}

TEST(MachineReaderTest, Geometry_ThrowsDataFormatErrorWhenPrimitiveInvalid) {
    ExpectDataFormatError(Replace(MinimalXyzAc(), "size = [10, 20, 30]", "size = [10, 0, 30]"),
                          "not all positive");
    ExpectDataFormatError(Replace(MinimalXyzAc(), "primitive = \"box\"\nsize = [10, 20, 30]\n",
                                  "primitive = \"cylinder\"\nheight = 10\n"),
                          "radius and height");
    ExpectDataFormatError(Replace(MinimalXyzAc(), "primitive = \"box\"", "primitive = \"cone\""),
                          "unknown primitive");
    ExpectDataFormatError(Replace(MinimalXyzAc(), "size = [10, 20, 30]\n",
                                  "size = [10, 20, 30]\nunit = \"mm\"\n"),
                          "unit");
}

TEST(MachineReaderTest, Geometry_ThrowsDataFormatErrorWhenColorMalformed) {
    ExpectDataFormatError(Replace(MinimalXyzAc(), "size = [10, 20, 30]\n",
                                  "size = [10, 20, 30]\ncolor = \"#12345\"\n"),
                          "#RRGGBB");
    ExpectDataFormatError(Replace(MinimalXyzAc(), "size = [10, 20, 30]\n",
                                  "size = [10, 20, 30]\ncolor = \"#12345G\"\n"),
                          "#RRGGBB");
}

TEST(MachineReaderTest, Geometry_WarnsOnceForNonPortablePaths) {
    const std::string toml = Replace(
            MinimalXyzAc(), "primitive = \"box\"\nsize = [10, 20, 30]\n",
            "file = \"/abs/a.stl\"\n"
            "\n[[component.geometry]]\nfile = \"D:/abs/b.stl\"\n"
            "\n[[component.geometry]]\nfile = \"../up/c.stl\"\n"
            "\n[[component.geometry]]\nfile = \"..dots/d.stl\"\n");
    const auto def = ReadString(toml);
    EXPECT_EQ(FindComponent(def, "X").geometries.size(), 4u);
    ExpectSingleWarning(def, "3 non-portable geometry path(s)");
    EXPECT_NE(def.warnings[0].message.find("absolute: 2"), std::string::npos);
    EXPECT_NE(def.warnings[0].message.find("above base directory: 1"),
              std::string::npos);
    // 絶対パスは基準ディレクトリを付けない
    EXPECT_EQ(std::get<fs::path>(FindComponent(def, "X").geometries[1].source),
              fs::path("D:/abs/b.stl").lexically_normal());
}

// ---- 異常系・警告: チェーン ----

TEST(MachineReaderTest, Chain_ThrowsDataFormatErrorWhenLinearAxesDoNotSpan) {
    ExpectDataFormatError(Replace(MinimalXyzAc(), "direction = [0, 0, 1]\nlimits = [-90, 300]",
                                  "direction = [1, 0, 0]\nlimits = [-90, 300]"),
                          "span 3 dimensions");
}

TEST(MachineReaderTest, Chain_ThrowsDataFormatErrorWhenXyzLeftHanded) {
    ExpectDataFormatError(Replace(MinimalXyzAc(), "direction = [0, 0, 1]\nlimits = [-90, 300]",
                                  "direction = [0, 0, -1]\nlimits = [-90, 300]"),
                          "not right-handed");
}

TEST(MachineReaderTest, Chain_WarnsWhenXyzRegistersIncomplete) {
    const auto def = ReadString(Replace(MinimalXyzAc(), "register = \"Z\"", "register = \"W\""));
    ExpectSingleWarning(def, "right-handedness check");
}

TEST(MachineReaderTest, Chain_ThrowsDataFormatErrorWhenThreeRotaryAxes) {
    const std::string toml = Replace(
            MinimalXyzAc(), "name = \"Spindle\"\ntype = \"spindle\"\nparent = \"Z\"\n",
            "name = \"B\"\ntype = \"rotary\"\nparent = \"Z\"\n\n[component.axis]\n"
            "register = \"B\"\ndirection = [0, 1, 0]\npoint = [0, 0, 0]\n"
            "limits = [-90, 90]\n\n[[component]]\nname = \"Spindle\"\ntype = \"spindle\"\n"
            "parent = \"B\"\n");
    ExpectDataFormatError(toml, "more than 2 rotary axes");
}

TEST(MachineReaderTest, Chain_WarnsWhenComponentIsSharedByBothChains) {
    // Aの親をXにすると、Xが両チェーンに共通になる
    const auto def = ReadString(Replace(MinimalXyzAc(), "parent = \"base\"", "parent = \"X\""));
    ExpectSingleWarning(def, "common to both chains");
    EXPECT_NE(def.warnings[0].message.find("[X]"), std::string::npos);
}

// ---- 異常系・警告: [kinematics]・[collision] ----

TEST(MachineReaderTest, Kinematics_ThrowsDataFormatErrorWhenBranchUnknown) {
    ExpectDataFormatError(MinimalXyzAc() + "\n[kinematics]\nbranch = \"left\"\n",
                          "[kinematics].branch");
    EXPECT_EQ(ReadString(MinimalXyzAc() + "\n[kinematics]\nbranch = \"continuous\"\n").branch,
              mc::BranchPolicy::kContinuous);
}

TEST(MachineReaderTest, Collision_ThrowsDataFormatErrorWhenModeUnknown) {
    ExpectDataFormatError(MinimalXyzAc() + "\n[collision]\nmode = \"all\"\n",
                          "[collision].mode");
}

TEST(MachineReaderTest, Collision_WarnsWhenExcludeUsedWithPairsMode) {
    const auto def = ReadString(MinimalXyzAc()
                                + "\n[collision]\nexclude = [[\"X\", \"A\"]]\n");
    ExpectSingleWarning(def, "exclude");
    EXPECT_EQ(def.collision->exclude.size(), 1u);
    const auto quiet = ReadString(MinimalXyzAc()
                                  + "\n[collision]\nmode = \"all_except_adjacent\"\n"
                                    "exclude = [[\"X\", \"A\"]]\n");
    EXPECT_TRUE(quiet.warnings.empty());
    EXPECT_EQ(quiet.collision->mode, mc::CollisionMode::kAllExceptAdjacent);
}

TEST(MachineReaderTest, Collision_ThrowsDataFormatErrorWhenTargetMissing) {
    ExpectDataFormatError(MinimalXyzAc() + "\n[collision]\npairs = [[\"X\", \"Q\"]]\n",
                          "target does not exist");
    ExpectDataFormatError(MinimalXyzAc() + "\n[collision]\nexclude = [[\"X\", \"Q\"]]\n",
                          "target does not exist");
}

TEST(MachineReaderTest, Collision_ThrowsDataFormatErrorWhenReservedHasSubtree) {
    ExpectDataFormatError(MinimalXyzAc() + "\n[[collision.pair]]\ntargets = [\"tool\", \"A\"]\n"
                                           "subtree = [true, false]\n",
                          "subtree = true");
}

TEST(MachineReaderTest, Collision_ThrowsDataFormatErrorWhenPairDuplicated) {
    ExpectDataFormatError(MinimalXyzAc() + "\n[collision]\n"
                                           "pairs = [[\"X\", \"A\"], [\"A\", \"X\"]]\n",
                          "duplicate collision pair");
}

TEST(MachineReaderTest, Collision_ThrowsDataFormatErrorWhenGroupsIntersect) {
    // Zのsubtree (Z・Spindle・Tool・@tool) と tool (@tool) が交差する
    ExpectDataFormatError(MinimalXyzAc() + "\n[[collision.pair]]\ntargets = [\"tool\", \"Z\"]\n"
                                           "subtree = [false, true]\n",
                          "intersect");
}

TEST(MachineReaderTest, Collision_DetailedPairKeepsClearanceAndEnabled) {
    const auto def = ReadString(MinimalXyzAc()
                                + "\n[collision]\ndefault_clearance = 1\n"
                                  "\n[[collision.pair]]\ntargets = [\"Z\", \"A\"]\n"
                                  "subtree = [true, true]\nclearance = 2.5\nenabled = false\n"
                                  "\n[[collision.pair]]\ntargets = [\"X\", \"C\"]\n");
    ASSERT_EQ(def.collision->pairs.size(), 2u);
    EXPECT_NEAR(def.collision->pairs[0].clearance, 2.5, kTol);
    EXPECT_FALSE(def.collision->pairs[0].enabled);
    EXPECT_TRUE(def.collision->pairs[0].subtree[1]);
    EXPECT_NEAR(def.collision->pairs[1].clearance, 1.0, kTol);
    EXPECT_TRUE(def.collision->pairs[1].enabled);
}

// ---- 入力 ----

TEST(MachineReaderTest, FromString_ThrowsDataFormatErrorWithSourceNameOnSyntaxError) {
    try {
        mc::ReadMachineDefinitionFromString("[format\nname = 1", kBaseDir, "unit-test.toml");
        FAIL() << "DataFormatError was not thrown";
    } catch (const igesio::DataFormatError& e) {
        const std::string message = e.what();
        EXPECT_NE(message.find("unit-test.toml"), std::string::npos) << message;
        EXPECT_NE(message.find("TOML parse error"), std::string::npos) << message;
    }
}

TEST(MachineReaderTest, FromString_KeepsBaseDirAndSourceName) {
    const auto def = ReadString(MinimalXyzAc());
    EXPECT_EQ(def.source_dir, kBaseDir);
    EXPECT_EQ(def.source_name, "<test>");
}

TEST(MachineReaderTest, ReadFile_ThrowsFileOpenErrorWhenMissing) {
    EXPECT_THROW(mc::ReadMachineDefinition(kFixturePath.parent_path() / "missing.toml"),
                 igesio::FileOpenError);
}

TEST(MachineReaderTest, ReadFile_ThrowsDataFormatErrorWhenComponentsMissing) {
    ExpectDataFormatError("[format]\nname = \"machine-definition\"\nversion = [2, 0]\n"
                          "[machine]\nname = \"m\"\n",
                          "[[component]]");
}

// ---- 語彙 (machine_definition.h の予約名・種別・形式) ----

TEST(MachineDefinitionTest, ReservedCollisionTarget_MatchesToolAndWorkOnly) {
    EXPECT_TRUE(mc::IsReservedCollisionTarget(mc::kToolCollisionTarget));
    EXPECT_TRUE(mc::IsReservedCollisionTarget(mc::kWorkCollisionTarget));
    EXPECT_FALSE(mc::IsReservedCollisionTarget(mc::kBaseComponentName));
    EXPECT_FALSE(mc::IsReservedCollisionTarget("Tool"));
    EXPECT_FALSE(mc::IsReservedCollisionTarget(""));
    EXPECT_EQ(mc::ParseComponentType(mc::kBaseComponentName), mc::ComponentType::kBase);
}

TEST(MachineDefinitionTest, PrimitiveKind_RoundTripsThroughName) {
    for (const auto kind : {mc::PrimitiveSpec::Kind::kBox, mc::PrimitiveSpec::Kind::kCylinder}) {
        const auto parsed = mc::ParsePrimitiveKind(mc::PrimitiveKindName(kind));
        ASSERT_TRUE(parsed.has_value());
        EXPECT_EQ(*parsed, kind);
    }
    EXPECT_FALSE(mc::ParsePrimitiveKind("sphere").has_value());
    EXPECT_FALSE(mc::ParsePrimitiveKind("Box").has_value());
}

TEST(MachineDefinitionTest, ClassifyGeometryFile_IgnoresExtensionCase) {
    EXPECT_EQ(mc::ClassifyGeometryFile("a/b.stl"), mc::GeometryFileFormat::kStl);
    EXPECT_EQ(mc::ClassifyGeometryFile("a/b.STL"), mc::GeometryFileFormat::kStl);
    EXPECT_EQ(mc::ClassifyGeometryFile("b.obj"), mc::GeometryFileFormat::kObj);
    EXPECT_EQ(mc::ClassifyGeometryFile("b.igs"), mc::GeometryFileFormat::kIges);
    EXPECT_EQ(mc::ClassifyGeometryFile("b.IGES"), mc::GeometryFileFormat::kIges);
    EXPECT_EQ(mc::ClassifyGeometryFile("b.stp"), mc::GeometryFileFormat::kStep);
    EXPECT_EQ(mc::ClassifyGeometryFile("b.step"), mc::GeometryFileFormat::kStep);
    EXPECT_EQ(mc::ClassifyGeometryFile("b.ply"), mc::GeometryFileFormat::kUnknown);
    EXPECT_EQ(mc::ClassifyGeometryFile("noext"), mc::GeometryFileFormat::kUnknown);
    EXPECT_EQ(mc::ClassifyGeometryFile("dir.stl/model"), mc::GeometryFileFormat::kUnknown);
}
