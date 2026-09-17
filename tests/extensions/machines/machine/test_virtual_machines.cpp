/**
 * @file tests/extensions/machines/machine/test_virtual_machines.cpp
 * @brief 仮想機械の機械定義 (machine/virtual_machines) のテスト
 * @author Yayoi Habami
 * @date 2026-09-17
 * @copyright 2026 Yayoi Habami
 * @note 対象: MakeVirtualMachineDefinition
 *       - 正常系: 3種の`MachineModel`構築 (軸数、姿勢を決める回転軸、工具軸方向,
 *         取り付けフレーム、チェーン)、設定 (名前/回転角の解の選択方針)、任意の
 *         工具軸方向への到達 (`kHeadBc`/`kTableAc`)、傾斜軸の可動範囲、暗黙のG54で
 *         W_0 = I (`MakeProjectDefinition`+`MachiningSetup`)、TOMLの書き出しと
 *         読み戻し
 *       - 正常系 (退化): `kThreeAxis`で傾斜した工具軸方向は到達不能
 *         (`KinematicsError`)、傾斜軸の可動範囲の端
 *       - 異常系: 名前が空、`tilt_limit_rad`が正でない (`std::invalid_argument`)
 */
#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/machine/axis_values.h"
#include "igesio/extensions/machines/machine/forward_kinematics.h"
#include "igesio/extensions/machines/machine/inverse_kinematics.h"
#include "igesio/extensions/machines/machine/machine_definition.h"
#include "igesio/extensions/machines/machine/machine_io.h"
#include "igesio/extensions/machines/machine/machine_model.h"
#include "igesio/extensions/machines/machine/virtual_machines.h"
#include "igesio/extensions/machines/project/project_definition.h"
#include "igesio/extensions/machines/project/setup.h"

namespace {

namespace mc = igesio::extensions::machines;
using igesio::Matrix4d;
using igesio::Vector3d;
using mc::ToRadians;

/// @brief 角度・位置の比較の許容誤差
constexpr double kTol = 1e-9;

/// @brief 制御点の`tool_mount`フレームでの座標 (工具長100の先端)
const Vector3d kControl(0.0, 0.0, -100.0);

/// @brief 到達性の検証に用いる目標点 (ゼロポーズ機械座標)
const Vector3d kTarget(10.0, 20.0, 30.0);

/// @brief 傾斜`tilt_deg`・方位角`azimuth_deg`の工具軸方向を作る
Vector3d Direction(const double tilt_deg, const double azimuth_deg) {
    const double tilt = ToRadians(tilt_deg);
    const double azimuth = ToRadians(azimuth_deg);
    return Vector3d(std::sin(tilt) * std::cos(azimuth),
                    std::sin(tilt) * std::sin(azimuth), std::cos(tilt));
}

/// @brief 軸名の一覧を取得する (`MachineModel::Axes()`の順)
std::vector<std::string> AxisNames(const mc::MachineModel& model) {
    std::vector<std::string> names;
    for (const mc::AxisInfo& axis : model.Axes()) names.push_back(axis.register_name);
    return names;
}

/// @brief 姿勢を決める回転軸の軸名の一覧を取得する (先頭が外側)
std::vector<std::string> OrientationNames(const mc::MachineModel& model) {
    std::vector<std::string> names;
    for (const std::size_t index : model.OrientationAxes()) {
        names.push_back(model.Axes()[index].register_name);
    }
    return names;
}

/// @brief 全種類に共通の性質 (工具軸方向+Z、取り付けフレーム原点、無制限の直進軸)
///        を検証する
void ExpectCommonShape(const mc::MachineModel& model) {
    EXPECT_TRUE(model.ToolAxisHome().isApprox(Vector3d::UnitZ(), kTol));
    EXPECT_TRUE(model.MountPlacement(mc::MountKind::kToolMount).isIdentity(kTol));
    EXPECT_TRUE(model.MountPlacement(mc::MountKind::kWorkMount).isIdentity(kTol));
    for (const mc::AxisInfo& axis : model.Axes()) {
        EXPECT_TRUE(axis.unlimited) << axis.register_name;
        EXPECT_FALSE(axis.limits.has_value()) << axis.register_name;
        EXPECT_FALSE(axis.dynamics.rapid_feed.has_value()) << axis.register_name;
        EXPECT_NEAR(axis.initial, 0.0, kTol) << axis.register_name;
        if (axis.kind == mc::AxisKind::kLinear) {
            EXPECT_TRUE(axis.on_tool_chain) << axis.register_name;
        }
    }
    EXPECT_EQ(model.Component(model.ToolMountIndex()).name, "Tool");
    EXPECT_EQ(model.Component(model.WorkMountIndex()).name, "Table");
}

}  // namespace



/**
 * ---- 構造 ----
 */

TEST(VirtualMachinesTest, MakeVirtual_ThreeAxisStructure) {
    const mc::MachineDefinition definition =
            mc::MakeVirtualMachineDefinition(mc::VirtualMachineKind::kThreeAxis);
    EXPECT_EQ(definition.format_version, mc::kMachineFormatVersion);
    EXPECT_EQ(definition.name, "virtual");
    EXPECT_EQ(definition.branch, mc::BranchPolicy::kContinuous);
    EXPECT_TRUE(definition.warnings.empty());
    EXPECT_FALSE(definition.collision.has_value());
    // 暗黙のbaseと同様に末尾がbase
    ASSERT_FALSE(definition.components.empty());
    EXPECT_EQ(definition.components.back().name, mc::kBaseComponentName);
    EXPECT_EQ(definition.components.back().type, mc::ComponentType::kBase);

    const mc::MachineModel model(definition);
    ExpectCommonShape(model);
    EXPECT_EQ(AxisNames(model), (std::vector<std::string>{"X", "Y", "Z"}));
    EXPECT_TRUE(model.OrientationAxes().empty());
    // 工具側チェーンはbase→X→Y→Z→Tool、ワーク側はbase→Table
    EXPECT_EQ(model.ToolChain().size(), 5u);
    EXPECT_EQ(model.WorkChain().size(), 2u);
    EXPECT_EQ(model.ChainRegisters(), (std::vector<std::string>{"X", "Y", "Z"}));
    for (const mc::ComponentSpec& component : definition.components) {
        EXPECT_TRUE(component.geometries.empty()) << component.name;
        EXPECT_TRUE(component.local_frame.isIdentity(kTol)) << component.name;
    }
}

TEST(VirtualMachinesTest, MakeVirtual_HeadBcStructure) {
    mc::VirtualMachineOptions options;
    options.name = "head";
    options.branch = mc::BranchPolicy::kPositive;
    const mc::MachineDefinition definition =
            mc::MakeVirtualMachineDefinition(mc::VirtualMachineKind::kHeadBc, options);
    EXPECT_EQ(definition.name, "head");
    EXPECT_EQ(definition.branch, mc::BranchPolicy::kPositive);

    const mc::MachineModel model(definition);
    ExpectCommonShape(model);
    EXPECT_EQ(AxisNames(model), (std::vector<std::string>{"X", "Y", "Z", "C", "B"}));
    // 旋回軸C (外側) と傾斜軸B (内側) はいずれも工具側 (σ=+1)
    EXPECT_EQ(OrientationNames(model), (std::vector<std::string>{"C", "B"}));
    const mc::AxisInfo& c = model.Axes()[*model.FindAxis("C")];
    const mc::AxisInfo& b = model.Axes()[*model.FindAxis("B")];
    EXPECT_TRUE(c.direction_world.isApprox(Vector3d::UnitZ(), kTol));
    EXPECT_TRUE(b.direction_world.isApprox(Vector3d::UnitY(), kTol));
    EXPECT_TRUE(c.point_world.isZero(kTol));
    EXPECT_TRUE(b.point_world.isZero(kTol));
    ASSERT_TRUE(c.wrap_start.has_value());
    EXPECT_NEAR(*c.wrap_start, 0.0, kTol);
    EXPECT_FALSE(b.wrap_start.has_value());
    EXPECT_NEAR(c.sigma, 1.0, kTol);
    EXPECT_NEAR(b.sigma, 1.0, kTol);
    EXPECT_TRUE(c.on_tool_chain);
    EXPECT_TRUE(b.on_tool_chain);
    EXPECT_EQ(model.WorkChain().size(), 2u);
    EXPECT_FALSE(model.WorkPivotReference().has_value());
}

TEST(VirtualMachinesTest, MakeVirtual_TableAcStructure) {
    const mc::MachineModel model(
            mc::MakeVirtualMachineDefinition(mc::VirtualMachineKind::kTableAc));
    ExpectCommonShape(model);
    EXPECT_EQ(AxisNames(model), (std::vector<std::string>{"X", "Y", "Z", "A", "C"}));
    // 旋回軸C (外側) と傾斜軸A (内側) はいずれもワーク側 (σ=-1)
    EXPECT_EQ(OrientationNames(model), (std::vector<std::string>{"C", "A"}));
    const mc::AxisInfo& a = model.Axes()[*model.FindAxis("A")];
    const mc::AxisInfo& c = model.Axes()[*model.FindAxis("C")];
    EXPECT_TRUE(a.direction_world.isApprox(Vector3d::UnitX(), kTol));
    EXPECT_TRUE(c.direction_world.isApprox(Vector3d::UnitZ(), kTol));
    EXPECT_NEAR(a.sigma, -1.0, kTol);
    EXPECT_NEAR(c.sigma, -1.0, kTol);
    EXPECT_TRUE(a.on_work_chain);
    EXPECT_TRUE(c.on_work_chain);
    EXPECT_FALSE(a.wrap_start.has_value());
    ASSERT_TRUE(c.wrap_start.has_value());
    // ワーク側チェーンはbase→A→C→Table
    EXPECT_EQ(model.WorkChain().size(), 4u);
    ASSERT_TRUE(model.WorkPivotReference().has_value());
    EXPECT_TRUE(model.WorkPivotReference()->isZero(kTol));
}



/**
 * ---- 到達性 ----
 */

TEST(VirtualMachinesTest, MakeVirtual_ReachesArbitraryToolAxis) {
    for (const mc::VirtualMachineKind kind :
         {mc::VirtualMachineKind::kHeadBc, mc::VirtualMachineKind::kTableAc}) {
        const mc::MachineModel model(mc::MakeVirtualMachineDefinition(kind));
        for (const double tilt : {0.0, 30.0, 90.0, 150.0}) {
            for (const double azimuth : {0.0, 90.0, 225.0}) {
                const mc::IkSolution solution = mc::Solve(
                        model, Direction(tilt, azimuth), kTarget, kControl,
                        mc::InitialJoints(model), {}, mc::BranchPolicy::kContinuous);
                ASSERT_TRUE(solution.error.has_value());
                EXPECT_LT(solution.error->angle, kTol)
                        << "tilt " << tilt << " azimuth " << azimuth;
                EXPECT_LT(solution.error->position, kTol)
                        << "tilt " << tilt << " azimuth " << azimuth;
                // 無制限の軸なので可動範囲外にならず、特異姿勢も診断しない
                EXPECT_TRUE(solution.warnings.empty())
                        << "tilt " << tilt << " azimuth " << azimuth;
                EXPECT_EQ(solution.singular, tilt == 0.0);
            }
        }
    }
}

TEST(VirtualMachinesTest, MakeVirtual_ThreeAxisCannotTilt) {
    const mc::MachineModel model(
            mc::MakeVirtualMachineDefinition(mc::VirtualMachineKind::kThreeAxis));
    const mc::IkSolution upright = mc::Solve(
            model, Vector3d::UnitZ(), kTarget, kControl, mc::InitialJoints(model),
            {}, mc::BranchPolicy::kContinuous);
    EXPECT_NEAR(upright.nc.At("X"), kTarget.x(), kTol);
    EXPECT_NEAR(upright.nc.At("Y"), kTarget.y(), kTol);
    EXPECT_NEAR(upright.nc.At("Z"), kTarget.z() - kControl.z(), kTol);
    EXPECT_THROW(mc::Solve(model, Direction(30.0, 0.0), kTarget, kControl,
                           mc::InitialJoints(model), {},
                           mc::BranchPolicy::kContinuous),
                 mc::KinematicsError);
}

TEST(VirtualMachinesTest, MakeVirtual_TiltLimitRestrictsTiltAxis) {
    mc::VirtualMachineOptions options;
    options.tilt_limit_rad = ToRadians(90.0);
    const mc::MachineModel model(mc::MakeVirtualMachineDefinition(
            mc::VirtualMachineKind::kHeadBc, options));
    const mc::AxisInfo& b = model.Axes()[*model.FindAxis("B")];
    EXPECT_FALSE(b.unlimited);
    ASSERT_TRUE(b.limits.has_value());
    EXPECT_NEAR((*b.limits)[0], ToRadians(-90.0), kTol);
    EXPECT_NEAR((*b.limits)[1], ToRadians(90.0), kTol);
    // 旋回軸Cは無制限のまま
    EXPECT_TRUE(model.Axes()[*model.FindAxis("C")].unlimited);

    // 可動範囲の端 (傾斜90°) までは警告なく解け、超えると可動範囲外の警告
    const auto edge = mc::SolveOrientation(model, Direction(90.0, 0.0), {},
                                           mc::BranchPolicy::kContinuous);
    EXPECT_TRUE(edge.warnings.empty());
    EXPECT_NEAR(std::abs(edge.nc.At("B")), ToRadians(90.0), kTol);
    const auto beyond = mc::SolveOrientation(model, Direction(120.0, 0.0), {},
                                             mc::BranchPolicy::kContinuous);
    ASSERT_EQ(beyond.warnings.size(), 1u);
    EXPECT_EQ(beyond.warnings[0].context, "limits");
}



/**
 * ---- セットアップとの組み合わせ ----
 */

TEST(VirtualMachinesTest, MakeVirtual_ImplicitG54IsIdentity) {
    for (const mc::VirtualMachineKind kind :
         {mc::VirtualMachineKind::kThreeAxis, mc::VirtualMachineKind::kHeadBc,
          mc::VirtualMachineKind::kTableAc}) {
        const mc::ProjectDefinition project = mc::MakeProjectDefinition(
                mc::MakeVirtualMachineDefinition(kind), "virtual-project");
        EXPECT_EQ(project.format_version, mc::kProjectFormatVersion);
        EXPECT_EQ(project.name, "virtual-project");
        EXPECT_EQ(project.source_name, "virtual-project");
        EXPECT_EQ(project.machine.name, "virtual");
        EXPECT_TRUE(project.warnings.empty());
        EXPECT_TRUE(project.work_offsets.empty());
        EXPECT_TRUE(project.tools.empty());
        EXPECT_NEAR(project.units.length, 1.0, kTol);

        const mc::MachiningSetup setup(project);
        EXPECT_TRUE(setup.Warnings().empty());
        ASSERT_EQ(setup.WorkFrames().size(), 1u);
        EXPECT_EQ(setup.WorkFrames()[0].id, mc::kImplicitWorkOffsetId);
        EXPECT_EQ(setup.InitialWorkOffset(), mc::kImplicitWorkOffsetId);
        // 取り付けフレームが原点なので、ワーク座標 = ゼロポーズ機械座標
        EXPECT_TRUE(setup.WorkFrames()[0].w0.isIdentity(kTol));
        EXPECT_EQ(setup.WorkFrames()[0].carrier, setup.Model().WorkMountIndex());
        for (std::size_t i = 0; i < setup.BaseQ().Size(); ++i) {
            EXPECT_NEAR(setup.BaseQ()[i], 0.0, kTol);
        }
        EXPECT_TRUE(setup.Geometries().empty());
    }
}

TEST(VirtualMachinesTest, MakeVirtual_RoundTripsThroughToml) {
    const std::filesystem::path base_dir("C:/virtual");
    for (const mc::VirtualMachineKind kind :
         {mc::VirtualMachineKind::kThreeAxis, mc::VirtualMachineKind::kHeadBc,
          mc::VirtualMachineKind::kTableAc}) {
        mc::VirtualMachineOptions options;
        options.tilt_limit_rad = ToRadians(120.0);
        const mc::MachineDefinition original =
                mc::MakeVirtualMachineDefinition(kind, options);
        const std::string text = mc::WriteMachineDefinitionToString(original, base_dir);
        const mc::MachineDefinition restored =
                mc::ReadMachineDefinitionFromString(text, base_dir, "<round-trip>");
        EXPECT_TRUE(restored.warnings.empty());
        EXPECT_EQ(restored.name, original.name);
        EXPECT_EQ(restored.branch, original.branch);
        const mc::MachineModel before(original);
        const mc::MachineModel after(restored);
        EXPECT_EQ(AxisNames(after), AxisNames(before));
        EXPECT_EQ(OrientationNames(after), OrientationNames(before));
        for (std::size_t i = 0; i < before.Axes().size(); ++i) {
            const mc::AxisInfo& expected = before.Axes()[i];
            const mc::AxisInfo& actual = after.Axes()[i];
            EXPECT_EQ(actual.unlimited, expected.unlimited) << expected.register_name;
            EXPECT_EQ(actual.limits.has_value(), expected.limits.has_value())
                    << expected.register_name;
            if (expected.limits.has_value() && actual.limits.has_value()) {
                EXPECT_NEAR((*actual.limits)[0], (*expected.limits)[0], kTol);
                EXPECT_NEAR((*actual.limits)[1], (*expected.limits)[1], kTol);
            }
            EXPECT_TRUE(actual.direction_world.isApprox(expected.direction_world, kTol))
                    << expected.register_name;
        }
    }
}



/**
 * ---- 異常系 ----
 */

TEST(VirtualMachinesTest, MakeVirtual_ThrowsInvalidArgumentWhenNameIsEmpty) {
    mc::VirtualMachineOptions options;
    options.name = "";
    EXPECT_THROW(mc::MakeVirtualMachineDefinition(mc::VirtualMachineKind::kHeadBc,
                                                  options),
                 std::invalid_argument);
}

TEST(VirtualMachinesTest, MakeVirtual_ThrowsInvalidArgumentWhenTiltLimitIsNotPositive) {
    mc::VirtualMachineOptions options;
    options.tilt_limit_rad = 0.0;
    EXPECT_THROW(mc::MakeVirtualMachineDefinition(mc::VirtualMachineKind::kHeadBc,
                                                  options),
                 std::invalid_argument);
    options.tilt_limit_rad = -ToRadians(90.0);
    EXPECT_THROW(mc::MakeVirtualMachineDefinition(mc::VirtualMachineKind::kTableAc,
                                                  options),
                 std::invalid_argument);
    // 正の値なら受理する (3軸機では使わないが検証はする)
    options.tilt_limit_rad = 1e-9;
    EXPECT_NO_THROW(mc::MakeVirtualMachineDefinition(
            mc::VirtualMachineKind::kThreeAxis, options));
}
