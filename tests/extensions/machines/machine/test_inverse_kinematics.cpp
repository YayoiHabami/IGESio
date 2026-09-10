/**
 * @file tests/extensions/machines/machine/test_inverse_kinematics.cpp
 * @brief 逆運動学 (machine/inverse_kinematics) のテスト
 * @author Yayoi Habami
 * @date 2026-09-09
 * @copyright 2026 Yayoi Habami
 * @note 対象: WrapAngleIntoLimits / SolveOrientation / SolvePosition / Solve /
 *       CheckSolution
 *       - 正常系 (往復): テーブルAC・傾斜BC・ヘッドBCの3機種で、指令値→順運動学→
 *         逆運動学が同じ指令値に戻ること (格子状の角度・位置)
 *       - 正常系 (枝): 正負2枝の対称性、`kContinuous`の直前値との近さ
 *         (無制限軸の畳み込みを含む)、可動範囲による枝の強制
 *       - 正常系 (回転軸数): 0本 (3軸機)・1本 (旋回角のみ) の解
 *       - 正常系 (境界値): `WrapAngleIntoLimits`の範囲端と±2πシフト、ストローク端
 *       - 正常系 (退化): 特異姿勢 (工具軸が主軸方向)、傾斜角が寄与しない構成
 *       - 異常系: 到達不能 (`KinematicsError`)、可動範囲外の警告、ストローク外の警告、
 *         回転軸3本・直進軸2本 (`NotImplementedError`)、方程式の退化、零ベクトル入力、
 *         直進軸への`WrapAngleIntoLimits` (`std::invalid_argument`)
 */
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/core/rotation.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/machine/forward_kinematics.h"
#include "igesio/extensions/machines/machine/inverse_kinematics.h"
#include "igesio/extensions/machines/machine/machine_definition.h"
#include "igesio/extensions/machines/machine/machine_model.h"
#include "./machines_for_testing.h"

namespace {

namespace mc = igesio::extensions::machines;
using igesio::Matrix4d;
using igesio::Vector3d;
using mc::ToRadians;
using machines_test::HeadBc;
using machines_test::MinimalXyzAc;
using machines_test::ReadDefinition;
using machines_test::ReadModel;
using machines_test::Replace;
using machines_test::ThreeAxis;
using machines_test::TiltedBc;

/// @brief 角度・位置の比較の許容誤差
constexpr double kTol = 1e-9;

/// @brief 制御点の`tool_mount`フレームでの座標 (工具長100の先端)
const Vector3d kControl(0.0, 0.0, -100.0);

/// @brief 往復テストの回転軸の角度 [deg]
constexpr std::array<double, 4> kSwivels = {0.0, 45.0, 170.0, 350.0};

/// @brief 往復テストの直進軸の位置
const std::array<Vector3d, 2> kPositions = {Vector3d(10.0, 50.0, -20.0),
                                            Vector3d(-100.0, 200.0, 100.0)};

/// @brief 指令値から作る目標姿勢 (ゼロポーズ機械座標)
struct Pose {
    /// @brief ワークに固定された工具軸方向
    Vector3d tool_axis = Vector3d::UnitZ();
    /// @brief 制御点が一致すべき点
    Vector3d target = Vector3d::Zero();
};

/// @brief 指令値で順運動学を評価し、ワーク側から見た工具軸と制御点位置を作る
Pose PoseFromNc(const mc::MachineModel& model, const mc::NcValues& nc) {
    const mc::JointVector q = mc::JointsFromNc(model, nc, mc::InitialJoints(model));
    const std::vector<Matrix4d> f = mc::Forward(model, q);
    const Matrix4d& tool = f[model.ToolMountIndex()];
    const Matrix4d work_inverse = mc::RigidInverse(f[model.WorkMountIndex()]);
    Pose pose;
    pose.tool_axis = mc::ApplyDirection(work_inverse,
                                        mc::ApplyDirection(tool, model.ToolAxisHome()));
    pose.target = mc::ApplyPoint(work_inverse, mc::ApplyPoint(tool, kControl));
    return pose;
}

/// @brief 角度を[0, 2π)へ畳む (無制限回転軸の比較用)
double Turn(const double angle) {
    const double folded = std::fmod(angle, mc::kFullTurn);
    return folded < 0.0 ? folded + mc::kFullTurn : folded;
}

/// @brief 指令値どおりに解けること (無制限軸は[0, 2π)で比較) と誤差0を検証する
void ExpectRecovered(const mc::MachineModel& model, const mc::NcValues& nc,
                     const mc::BranchPolicy policy) {
    const Pose pose = PoseFromNc(model, nc);
    const mc::IkSolution solution = mc::Solve(model, pose.tool_axis, pose.target, kControl,
                                              mc::InitialJoints(model), {}, policy);
    for (const auto& [name, expected] : nc.Entries()) {
        const mc::AxisInfo& axis = model.Axes()[*model.FindAxis(name)];
        ASSERT_TRUE(solution.nc.Contains(name)) << name;
        if (axis.unlimited) {
            EXPECT_NEAR(Turn(solution.nc.At(name)), Turn(expected), kTol) << name;
        } else {
            EXPECT_NEAR(solution.nc.At(name), expected, kTol) << name;
        }
    }
    ASSERT_TRUE(solution.error.has_value());
    EXPECT_LT(solution.error->angle, kTol);
    EXPECT_LT(solution.error->position, kTol);
    EXPECT_TRUE(solution.warnings.empty());
    EXPECT_FALSE(solution.singular);
}

/// @brief 回転軸2本と直進軸3本の指令値
mc::NcValues Command(const char* tilt_name, const double tilt_deg,
                     const char* swivel_name, const double swivel_deg,
                     const Vector3d& xyz) {
    return {{tilt_name, ToRadians(tilt_deg)}, {swivel_name, ToRadians(swivel_deg)},
            {"X", xyz.x()}, {"Y", xyz.y()}, {"Z", xyz.z()}};
}

/// @brief 回転軸1本 (A) のみの機械 (Cを`fixed`にした最小構成)
std::string OnlyA() {
    return Replace(MinimalXyzAc(),
                   "name = \"C\"\ntype = \"rotary\"\nparent = \"A\"\n\n[component.axis]\n"
                   "register = \"C\"\ndirection = [0, 0, 1]\npoint = [0, 0, 0]\n"
                   "unlimited = true\nwrap_start = 0\n",
                   "name = \"C\"\ntype = \"fixed\"\nparent = \"A\"\n");
}

/// @brief 警告がちょうど1件で識別語を含むことを検証する
void ExpectSingleWarning(const std::vector<mc::Diagnostic>& warnings,
                         const std::string& keyword) {
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_NE(warnings[0].message.find(keyword), std::string::npos)
            << "message: " << warnings[0].message;
}

/// @brief 可動範囲`[-90°, 90°]`の回転軸
mc::AxisInfo LimitedAxis() {
    mc::AxisInfo axis;
    axis.register_name = "A";
    axis.kind = mc::AxisKind::kRotary;
    axis.limits = std::array<double, 2>{ToRadians(-90.0), ToRadians(90.0)};
    axis.nc_range = axis.limits;
    return axis;
}

/// @brief `wrap_start = 0`の無制限回転軸
mc::AxisInfo WrappedAxis() {
    mc::AxisInfo axis;
    axis.register_name = "C";
    axis.kind = mc::AxisKind::kRotary;
    axis.unlimited = true;
    axis.wrap_start = 0.0;
    axis.nc_range = std::array<double, 2>{0.0, mc::kFullTurn};
    return axis;
}

}  // namespace



// ---- 正常系: 往復 ----

TEST(InverseKinematicsTest, RoundTrip_TableAc_Positive) {
    const mc::MachineModel model = ReadModel(MinimalXyzAc());
    for (const double a : {5.0, 30.0, 60.0, 85.0}) {
        for (const double c : kSwivels) {
            for (const Vector3d& xyz : kPositions) {
                ExpectRecovered(model, Command("A", a, "C", c, xyz),
                                mc::BranchPolicy::kPositive);
            }
        }
    }
}

TEST(InverseKinematicsTest, RoundTrip_TableAc_Negative) {
    const mc::MachineModel model = ReadModel(MinimalXyzAc());
    for (const double a : {-5.0, -30.0, -60.0, -85.0}) {
        for (const double c : kSwivels) {
            ExpectRecovered(model, Command("A", a, "C", c, kPositions[0]),
                            mc::BranchPolicy::kNegative);
        }
    }
}

TEST(InverseKinematicsTest, RoundTrip_TiltedBc) {
    const mc::MachineModel model = ReadModel(TiltedBc());
    for (const double b : {10.0, 60.0, 120.0, 170.0}) {
        for (const double c : kSwivels) {
            ExpectRecovered(model, Command("B", b, "C", c, kPositions[1]),
                            mc::BranchPolicy::kPositive);
        }
    }
}

TEST(InverseKinematicsTest, RoundTrip_HeadBc) {
    const mc::MachineModel model = ReadModel(HeadBc());
    for (const double b : {10.0, 60.0, 110.0}) {
        for (const double c : kSwivels) {
            ExpectRecovered(model, Command("B", b, "C", c, kPositions[0]),
                            mc::BranchPolicy::kPositive);
            ExpectRecovered(model, Command("B", -b, "C", c, kPositions[0]),
                            mc::BranchPolicy::kNegative);
        }
    }
}

// ---- 正常系: 枝 ----

TEST(InverseKinematicsTest, Branches_PositiveAndNegativeAreMirrored) {
    const mc::MachineModel model = ReadModel(MinimalXyzAc());
    const Pose pose = PoseFromNc(model, Command("A", 30.0, "C", 40.0, kPositions[0]));
    const mc::IkSolution positive = mc::Solve(model, pose.tool_axis, pose.target, kControl,
                                              mc::InitialJoints(model), {},
                                              mc::BranchPolicy::kPositive);
    const mc::IkSolution negative = mc::Solve(model, pose.tool_axis, pose.target, kControl,
                                              mc::InitialJoints(model), {},
                                              mc::BranchPolicy::kNegative);
    EXPECT_NEAR(positive.nc.At("A"), ToRadians(30.0), kTol);
    EXPECT_NEAR(negative.nc.At("A"), ToRadians(-30.0), kTol);
    EXPECT_NEAR(Turn(negative.nc.At("C") - positive.nc.At("C")), mc::kHalfTurn, kTol);
    ASSERT_TRUE(positive.error.has_value());
    ASSERT_TRUE(negative.error.has_value());
    EXPECT_LT(positive.error->position, kTol);
    EXPECT_LT(negative.error->position, kTol);
    EXPECT_LT(negative.error->angle, kTol);
}

TEST(InverseKinematicsTest, Branches_ContinuousPicksNearerToPrevious) {
    const mc::MachineModel model = ReadModel(MinimalXyzAc());
    const Pose pose = PoseFromNc(model, Command("A", 30.0, "C", 40.0, kPositions[0]));
    const mc::NcValues near_negative = {{"A", ToRadians(-25.0)}, {"C", ToRadians(215.0)}};
    const mc::NcValues near_positive = {{"A", ToRadians(20.0)}, {"C", ToRadians(50.0)}};
    const auto negative = mc::SolveOrientation(model, pose.tool_axis, near_negative,
                                               mc::BranchPolicy::kContinuous);
    const auto positive = mc::SolveOrientation(model, pose.tool_axis, near_positive,
                                               mc::BranchPolicy::kContinuous);
    const auto no_previous = mc::SolveOrientation(model, pose.tool_axis, {},
                                                  mc::BranchPolicy::kContinuous);
    EXPECT_NEAR(negative.nc.At("A"), ToRadians(-30.0), kTol);
    EXPECT_NEAR(positive.nc.At("A"), ToRadians(30.0), kTol);
    EXPECT_NEAR(no_previous.nc.At("A"), ToRadians(30.0), kTol);   // D10: 正枝
    EXPECT_FALSE(negative.q.has_value());   // 姿勢IKは変位量の基準を持たない
    // σ=-1: 指令値-30°に対する軸変位量は+30°
    const mc::JointVector q = mc::JointsFromNc(model, negative.nc,
                                               mc::InitialJoints(model));
    EXPECT_NEAR(q[*model.FindAxis("A")], ToRadians(30.0), kTol);
}

TEST(InverseKinematicsTest, Branches_ContinuousFoldsUnlimitedAxis) {
    const mc::MachineModel model = ReadModel(MinimalXyzAc());
    // 正枝が(A, C) = (30°, 1°)、負枝が(-30°, 181°)になる姿勢
    const Pose pose = PoseFromNc(model, Command("A", 30.0, "C", 1.0, kPositions[0]));
    const mc::NcValues previous = {{"A", ToRadians(20.0)}, {"C", ToRadians(359.0)}};
    const auto solution = mc::SolveOrientation(model, pose.tool_axis, previous,
                                               mc::BranchPolicy::kContinuous);
    // 畳み込みなしではCの差358°が支配して負枝が選ばれる
    EXPECT_NEAR(solution.nc.At("A"), ToRadians(30.0), kTol);
    EXPECT_NEAR(solution.nc.At("C"), ToRadians(1.0), kTol);
}

TEST(InverseKinematicsTest, Limits_ForceSingleBranch) {
    const mc::MachineModel model =
            ReadModel(Replace(MinimalXyzAc(), "limits = [-90, 90]", "limits = [0, 90]"));
    const Pose pose = PoseFromNc(model, Command("A", 30.0, "C", 40.0, kPositions[0]));
    const auto solution = mc::SolveOrientation(model, pose.tool_axis, {},
                                               mc::BranchPolicy::kNegative);
    EXPECT_NEAR(solution.nc.At("A"), ToRadians(30.0), kTol);
    EXPECT_NEAR(solution.nc.At("C"), ToRadians(40.0), kTol);
    EXPECT_TRUE(solution.warnings.empty());
}

TEST(InverseKinematicsTest, Limits_NoValidBranchWarnsAndReturnsPositive) {
    const mc::MachineModel model =
            ReadModel(Replace(MinimalXyzAc(), "limits = [-90, 90]", "limits = [-10, 10]"));
    const Pose pose = PoseFromNc(model, Command("A", 45.0, "C", 300.0, kPositions[0]));
    const auto solution = mc::SolveOrientation(model, pose.tool_axis, {},
                                               mc::BranchPolicy::kPositive);
    ExpectSingleWarning(solution.warnings, "out of range");
    EXPECT_NEAR(solution.nc.At("A"), ToRadians(45.0), kTol);
    EXPECT_GE(solution.nc.At("C"), 0.0);
    EXPECT_LT(solution.nc.At("C"), mc::kFullTurn);
    EXPECT_NEAR(solution.nc.At("C"), ToRadians(300.0), kTol);
}

// ---- 正常系: 回転軸数 ----

TEST(InverseKinematicsTest, SingleRotary_SolvesSwivelOnly) {
    const mc::MachineModel model = ReadModel(OnlyA());
    ASSERT_EQ(model.OrientationAxes().size(), 1u);
    const Pose pose = PoseFromNc(model, {{"A", ToRadians(30.0)}});
    const auto solution = mc::SolveOrientation(model, pose.tool_axis, {},
                                               mc::BranchPolicy::kPositive);
    ASSERT_EQ(solution.nc.Size(), 1u);
    EXPECT_NEAR(solution.nc.At("A"), ToRadians(30.0), kTol);
    EXPECT_FALSE(solution.singular);
    EXPECT_TRUE(solution.warnings.empty());
    // σ=-1: 指令値+30°に対する軸変位量は-30°
    const mc::JointVector q = mc::JointsFromNc(model, solution.nc,
                                               mc::InitialJoints(model));
    EXPECT_NEAR(q[*model.FindAxis("A")], ToRadians(-30.0), kTol);
    const mc::IkSolution full = mc::Solve(model, pose.tool_axis, pose.target, kControl,
                                          mc::InitialJoints(model), {},
                                          mc::BranchPolicy::kPositive);
    ASSERT_TRUE(full.error.has_value());
    EXPECT_LT(full.error->angle, kTol);
    EXPECT_LT(full.error->position, kTol);
}

TEST(InverseKinematicsTest, SingleRotary_ToolAxisAlongAxisIsSingular) {
    // Aのみ・A軸方向を主軸方向 (z) にすると、どの角度でも工具軸は不変
    const mc::MachineModel model =
            ReadModel(Replace(OnlyA(), "direction = [1, 0, 0]\npoint = [0, 0, 60]",
                              "direction = [0, 0, 1]\npoint = [0, 0, 60]"));
    const auto solution = mc::SolveOrientation(model, Vector3d::UnitZ(), {},
                                               mc::BranchPolicy::kPositive);
    EXPECT_TRUE(solution.singular);
    ExpectSingleWarning(solution.warnings, "singular");
    EXPECT_NEAR(solution.nc.At("A"), 0.0, kTol);
}

TEST(InverseKinematicsTest, NoRotary_RequiresSpindleDirection) {
    const mc::MachineModel model = ReadModel(ThreeAxis());
    const auto orientation = mc::SolveOrientation(model, Vector3d(0.0, 0.0, 2.0), {},
                                                  mc::BranchPolicy::kPositive);
    EXPECT_TRUE(orientation.nc.Empty());
    EXPECT_TRUE(orientation.warnings.empty());
    // 姿勢IKは全軸の変位量も自己検証も返さない
    EXPECT_FALSE(orientation.q.has_value());
    EXPECT_FALSE(orientation.error.has_value());
    const Pose pose = PoseFromNc(model, {{"X", 5.0}, {"Y", -6.0}, {"Z", 7.0}});
    const mc::IkSolution solution = mc::Solve(model, pose.tool_axis, pose.target, kControl,
                                              mc::InitialJoints(model), {},
                                              mc::BranchPolicy::kPositive);
    EXPECT_NEAR(solution.nc.At("X"), 5.0, kTol);
    EXPECT_NEAR(solution.nc.At("Y"), -6.0, kTol);
    EXPECT_NEAR(solution.nc.At("Z"), 7.0, kTol);
    EXPECT_EQ(solution.nc.Size(), 3u);
    ASSERT_TRUE(solution.error.has_value());
    EXPECT_LT(solution.error->position, kTol);
    EXPECT_THROW(mc::SolveOrientation(model, Vector3d(0.0, 0.1, 1.0), {},
                                      mc::BranchPolicy::kPositive),
                 mc::KinematicsError);
}

// ---- 正常系: 境界値 ----

TEST(InverseKinematicsTest, WrapAngleIntoLimits_ShiftsIntoRange) {
    const mc::AxisInfo limited = LimitedAxis();
    const auto shifted_up = mc::WrapAngleIntoLimits(ToRadians(-370.0), limited);
    ASSERT_TRUE(shifted_up.has_value());
    EXPECT_NEAR(*shifted_up, ToRadians(-10.0), kTol);
    const auto shifted_down = mc::WrapAngleIntoLimits(ToRadians(400.0), limited);
    ASSERT_TRUE(shifted_down.has_value());
    EXPECT_NEAR(*shifted_down, ToRadians(40.0), kTol);
    EXPECT_FALSE(mc::WrapAngleIntoLimits(ToRadians(95.0), limited).has_value());
    // 範囲端は許容誤差の内側なら受理、外側なら拒否
    const double edge = ToRadians(90.0);
    const double inside = edge + 0.5 * mc::kLimitTolerance;
    const double outside = edge + 2.0 * mc::kLimitTolerance;
    EXPECT_TRUE(mc::WrapAngleIntoLimits(inside, limited).has_value());
    EXPECT_FALSE(mc::WrapAngleIntoLimits(outside, limited).has_value());

    const mc::AxisInfo wrapped = WrappedAxis();
    EXPECT_NEAR(*mc::WrapAngleIntoLimits(ToRadians(-10.0), wrapped),
                ToRadians(350.0), kTol);
    EXPECT_NEAR(*mc::WrapAngleIntoLimits(ToRadians(730.0), wrapped),
                ToRadians(10.0), kTol);
    EXPECT_NEAR(*mc::WrapAngleIntoLimits(0.0, wrapped), 0.0, kTol);

    mc::AxisInfo free = WrappedAxis();
    free.wrap_start.reset();
    free.nc_range.reset();
    EXPECT_NEAR(*mc::WrapAngleIntoLimits(ToRadians(730.0), free),
                ToRadians(730.0), kTol);
}

TEST(InverseKinematicsTest, WrapAngleIntoLimits_ThrowsForLinearAxis) {
    // 直進軸の可動範囲には2πの周期が無く、範囲外の指令値に代替値も無いため、
    // 周期性を前提とする本関数の対象外とする (検査は`IsWithinLimits`)
    mc::AxisInfo linear;
    linear.register_name = "X";
    linear.kind = mc::AxisKind::kLinear;
    linear.limits = std::array<double, 2>{-400.0, 400.0};
    linear.nc_range = linear.limits;
    EXPECT_THROW(mc::WrapAngleIntoLimits(0.0, linear), std::invalid_argument);
    EXPECT_TRUE(mc::IsWithinLimits(linear, 400.0));
    EXPECT_FALSE(mc::IsWithinLimits(linear, 401.0));
}

// ---- 正常系: 退化 ----

TEST(InverseKinematicsTest, Singular_ToolAxisAlongSpindleGivesZeroSwivel) {
    const mc::MachineModel model = ReadModel(MinimalXyzAc());
    const mc::IkSolution solution = mc::Solve(model, Vector3d::UnitZ(),
                                              Vector3d(10.0, 20.0, -100.0), kControl,
                                              mc::InitialJoints(model), {},
                                              mc::BranchPolicy::kPositive);
    EXPECT_TRUE(solution.singular);
    ExpectSingleWarning(solution.warnings, "singular");
    EXPECT_NEAR(solution.nc.At("A"), 0.0, kTol);
    EXPECT_NEAR(solution.nc.At("C"), 0.0, kTol);
    EXPECT_NEAR(solution.nc.At("X"), 10.0, kTol);
    EXPECT_NEAR(solution.nc.At("Y"), 20.0, kTol);
    EXPECT_NEAR(solution.nc.At("Z"), 0.0, kTol);
    ASSERT_TRUE(solution.error.has_value());
    EXPECT_LT(solution.error->angle, kTol);
    EXPECT_LT(solution.error->position, kTol);
}

TEST(InverseKinematicsTest, Degenerate_ParallelAxesMakeTiltIndeterminate) {
    // A軸もz軸まわりにすると内側軸が工具軸に平行で傾斜角が寄与しない (ρ=0)
    const mc::MachineModel model =
            ReadModel(Replace(MinimalXyzAc(), "direction = [1, 0, 0]\npoint = [0, 0, 60]",
                              "direction = [0, 0, 1]\npoint = [0, 0, 60]"));
    const auto solution = mc::SolveOrientation(model, Vector3d::UnitZ(), {},
                                               mc::BranchPolicy::kPositive);
    EXPECT_NEAR(solution.nc.At("A"), 0.0, kTol);
    EXPECT_NEAR(solution.nc.At("C"), 0.0, kTol);
    ASSERT_EQ(solution.warnings.size(), 2u);
    EXPECT_NE(solution.warnings[0].message.find("indeterminate"), std::string::npos);
    EXPECT_THROW(mc::SolveOrientation(model, Vector3d(0.0, 0.1, 1.0), {},
                                      mc::BranchPolicy::kPositive),
                 mc::KinematicsError);
}

// ---- 異常系: 姿勢 ----

TEST(InverseKinematicsTest, Unreachable_OutsideConeThrowsKinematicsError) {
    const mc::MachineModel model = ReadModel(OnlyA());
    EXPECT_THROW(mc::SolveOrientation(model, Vector3d::UnitX(), {},
                                      mc::BranchPolicy::kPositive),
                 mc::KinematicsError);
    EXPECT_NO_THROW(mc::SolveOrientation(model, Vector3d(0.0, 1.0, 1.0), {},
                                         mc::BranchPolicy::kPositive));
}

TEST(InverseKinematicsTest, ThreeRotary_ThrowsNotImplementedError) {
    mc::MachineDefinition definition = ReadDefinition(MinimalXyzAc());
    mc::ComponentSpec b;
    b.name = "B";
    b.parent = "Z";
    b.type = mc::ComponentType::kRotary;
    mc::AxisSpec axis;
    axis.register_name = "B";
    axis.direction = Vector3d::UnitY();
    axis.point = Vector3d::Zero();
    axis.unlimited = true;
    b.axis = axis;
    definition.components.push_back(b);
    for (auto& component : definition.components) {
        if (component.name == "Spindle") component.parent = "B";
    }
    const mc::MachineModel model(definition);
    ASSERT_EQ(model.OrientationAxes().size(), 3u);
    EXPECT_THROW(mc::SolveOrientation(model, Vector3d::UnitZ(), {},
                                      mc::BranchPolicy::kPositive),
                 igesio::NotImplementedError);
}

TEST(InverseKinematicsTest, Inputs_ZeroToolAxisThrowsInvalidArgument) {
    const mc::MachineModel model = ReadModel(MinimalXyzAc());
    EXPECT_THROW(mc::SolveOrientation(model, Vector3d::Zero(), {},
                                      mc::BranchPolicy::kPositive),
                 std::invalid_argument);
    EXPECT_THROW(mc::CheckSolution(model, mc::InitialJoints(model), Vector3d::Zero(),
                                   Vector3d::Zero(), kControl),
                 std::invalid_argument);
}

// ---- 位置IK ----

TEST(InverseKinematicsTest, Position_WarnsOutsideStroke) {
    const mc::MachineModel model = ReadModel(MinimalXyzAc());   // Y: [0, 300]
    const Pose pose = PoseFromNc(model, Command("A", 0.0, "C", 0.0,
                                                Vector3d(0.0, -10.0, 0.0)));
    const auto solution = mc::SolvePosition(model, pose.target, kControl, {},
                                            mc::InitialJoints(model));
    ExpectSingleWarning(solution.warnings, "out of stroke");
    EXPECT_NE(solution.warnings[0].message.find("Y=-10.000"), std::string::npos);
    EXPECT_NEAR(solution.nc.At("Y"), -10.0, kTol);
    // ストローク端 (Y = 0) は警告なし
    const Pose edge = PoseFromNc(model, Command("A", 0.0, "C", 0.0, Vector3d::Zero()));
    EXPECT_TRUE(mc::SolvePosition(model, edge.target, kControl, {}, mc::InitialJoints(model))
                        .warnings.empty());
}

TEST(InverseKinematicsTest, Position_IgnoresOffChainAxes) {
    const mc::MachineModel model = ReadModel(MinimalXyzAc() + R"(
[[component]]
name = "Door"
type = "linear"
parent = "base"

[component.axis]
register = "W"
direction = [0, 1, 0]
limits = [0, 100]
)");
    const Pose pose = PoseFromNc(model, Command("A", 20.0, "C", 30.0, kPositions[0]));
    mc::JointVector moved = mc::InitialJoints(model);
    moved[*model.FindAxis("W")] = 50.0;
    const mc::IkSolution closed = mc::Solve(model, pose.tool_axis, pose.target, kControl,
                                            mc::InitialJoints(model), {},
                                            mc::BranchPolicy::kPositive);
    const mc::IkSolution opened = mc::Solve(model, pose.tool_axis, pose.target, kControl,
                                            moved, {}, mc::BranchPolicy::kPositive);
    for (const char* name : {"X", "Y", "Z", "A", "C"}) {
        EXPECT_NEAR(closed.nc.At(name), opened.nc.At(name), kTol) << name;
    }
    EXPECT_FALSE(opened.nc.Contains("W"));
    ASSERT_TRUE(opened.q.has_value());
    EXPECT_NEAR((*opened.q)[*model.FindAxis("W")], 50.0, kTol);   // base_qの値を保つ
}

TEST(InverseKinematicsTest, Position_UsesRotaryNcFromWords) {
    const mc::MachineModel model = ReadModel(MinimalXyzAc());
    const mc::NcValues nc = Command("A", 30.0, "C", 45.0, kPositions[1]);
    const Pose pose = PoseFromNc(model, nc);
    const auto position = mc::SolvePosition(model, pose.target, kControl,
                                            {{"A", nc.At("A")}, {"C", nc.At("C")}},
                                            mc::InitialJoints(model));
    EXPECT_EQ(position.nc.Size(), 3u);
    EXPECT_NEAR(position.nc.At("X"), kPositions[1].x(), kTol);
    EXPECT_NEAR(position.nc.At("Y"), kPositions[1].y(), kTol);
    EXPECT_NEAR(position.nc.At("Z"), kPositions[1].z(), kTol);
    // qは回転軸の指令値と解いた直進軸をbase_qに重ねた全軸の変位量、errorは未設定
    const mc::JointVector expected_q =
            mc::JointsFromNc(model, nc, mc::InitialJoints(model));
    ASSERT_TRUE(position.q.has_value());
    ASSERT_EQ(position.q->Size(), expected_q.Size());
    for (std::size_t i = 0; i < expected_q.Size(); ++i) {
        EXPECT_NEAR((*position.q)[i], expected_q[i], kTol) << i;
    }
    EXPECT_FALSE(position.error.has_value());
    EXPECT_THROW(mc::SolvePosition(model, pose.target, kControl, {{"Q", 0.0}},
                                   mc::InitialJoints(model)),
                 std::invalid_argument);
}

TEST(InverseKinematicsTest, Position_ThrowsNotImplementedErrorForTwoLinearAxes) {
    mc::MachineDefinition definition = ReadDefinition(ThreeAxis());
    for (auto& component : definition.components) {
        if (component.name != "Z") continue;
        component.type = mc::ComponentType::kFixed;
        component.axis.reset();
    }
    const mc::MachineModel model(definition);
    EXPECT_THROW(mc::SolvePosition(model, Vector3d::Zero(), kControl, {},
                                   mc::InitialJoints(model)),
                 igesio::NotImplementedError);
}

TEST(InverseKinematicsTest, Position_ThrowsKinematicsErrorWhenDegenerate) {
    mc::MachineDefinition definition = ReadDefinition(ThreeAxis());
    for (auto& component : definition.components) {
        if (component.name == "Y") component.axis->direction = Vector3d::UnitX();
    }
    const mc::MachineModel model(definition);
    EXPECT_THROW(mc::SolvePosition(model, Vector3d::Zero(), kControl, {},
                                   mc::InitialJoints(model)),
                 mc::KinematicsError);
}

// ---- 自己検証 ----

TEST(InverseKinematicsTest, CheckSolution_ReportsInjectedErrors) {
    const mc::MachineModel model = ReadModel(MinimalXyzAc());
    const mc::NcValues nc = Command("A", 30.0, "C", 45.0, kPositions[0]);
    const Pose pose = PoseFromNc(model, nc);
    const mc::JointVector q = mc::JointsFromNc(model, nc, mc::InitialJoints(model));
    const mc::SolutionError exact =
            mc::CheckSolution(model, q, pose.tool_axis, pose.target, kControl);
    EXPECT_NEAR(exact.angle, 0.0, kTol);
    EXPECT_NEAR(exact.position, 0.0, kTol);

    mc::JointVector tilted = q;
    tilted[*model.FindAxis("A")] += ToRadians(1.0);
    const mc::SolutionError angle_error =
            mc::CheckSolution(model, tilted, pose.tool_axis, pose.target, kControl);
    EXPECT_NEAR(angle_error.angle, ToRadians(1.0), kTol);

    mc::JointVector shifted = q;
    shifted[*model.FindAxis("X")] += 0.5;
    const mc::SolutionError position_error =
            mc::CheckSolution(model, shifted, pose.tool_axis, pose.target, kControl);
    EXPECT_NEAR(position_error.angle, 0.0, kTol);
    EXPECT_NEAR(position_error.position, 0.5, kTol);
}
