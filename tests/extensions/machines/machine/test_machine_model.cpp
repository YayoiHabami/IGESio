/**
 * @file tests/extensions/machines/machine/test_machine_model.cpp
 * @brief 運動学モデル (machine/machine_model) のテスト
 * @author Yayoi Habami
 * @date 2026-09-09
 * @copyright 2026 Yayoi Habami
 * @note 対象: MachineModel (構築・親先行順・チェーン・σ・軸一覧・
 *       ChainRegisters・MountPlacement・姿勢に寄与する回転軸・ToolAxisHome・
 *       WorkPivotReference)、IsWithinLimits
 *       - 正常系 (実例): `t-ZYX-b-AC-w.toml`の親先行順、チェーンとσ、軸の機械座標、
 *         マウント・姿勢に寄与する回転軸・回転中心
 *       - 正常系 (派生構成): 傾斜B軸上のC軸の従動、ヘッド型で工具の向きを決める
 *         回転軸、3軸機、`local_frame`の軸への適用、チェーン外の軸、
 *         両チェーン共通の軸
 *       - 異常系: C++で組み立てた定義の構造の矛盾 (`std::invalid_argument`)、
 *         範囲外のインデックス (`std::out_of_range`)
 *       - コピー: ムーブ渡しで構築後のコピーが自己完結すること
 * @note 順運動学そのもの (`Forward`・NC/軸変位量の変換) の検証は
 *       `test_forward_kinematics.cpp`側にある. ここでは構造を確かめる手段として使う.
 */
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/rotation.h"
#include "igesio/extensions/machines/core/tolerances.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/machine/forward_kinematics.h"
#include "igesio/extensions/machines/machine/machine_definition.h"
#include "igesio/extensions/machines/machine/machine_io.h"
#include "igesio/extensions/machines/machine/machine_model.h"
#include "./machines_for_testing.h"

namespace {

namespace mc = igesio::extensions::machines;
using igesio::Matrix4d;
using igesio::Vector3d;
using mc::ToRadians;
using machines_test::kFixturePath;
using machines_test::HeadBc;
using machines_test::MinimalXyzAc;
using machines_test::ReadDefinition;
using machines_test::ReadModel;
using machines_test::Replace;
using machines_test::ThreeAxis;
using machines_test::TiltedBc;

/// @brief 行列・ベクトル比較の許容誤差
constexpr double kTol = 1e-9;

/// @brief 実例TOMLから運動学モデルを作る
mc::MachineModel FixtureModel() {
    return mc::MachineModel(mc::ReadMachineDefinition(kFixturePath));
}

/// @brief コンポーネントのインデックス列を名前列にする
std::vector<std::string> Names(const mc::MachineModel& model,
                               const std::vector<std::size_t>& indices) {
    std::vector<std::string> names;
    for (const std::size_t index : indices) names.push_back(model.Component(index).name);
    return names;
}

/// @brief 軸一覧のレジスタ名列
std::vector<std::string> Registers(const mc::MachineModel& model) {
    std::vector<std::string> names;
    for (const auto& axis : model.Axes()) names.push_back(axis.register_name);
    return names;
}

/// @brief 名前で軸を引く
/// @throw std::logic_error 見つからない場合
const mc::AxisInfo& Axis(const mc::MachineModel& model, const std::string& name) {
    const auto index = model.FindAxis(name);
    if (!index.has_value()) throw std::logic_error("axis not found: " + name);
    return model.Axes()[*index];
}

/// @brief 工具の向きを決める回転軸のうちi番目 (0が外側) の軸を引く
/// @throw std::out_of_range 本数が足りない場合
const mc::AxisInfo& OrientationAxis(const mc::MachineModel& model, const std::size_t i) {
    return model.Axes()[model.OrientationAxes().at(i)];
}

/// @brief 名前でコンポーネントの剛体変換行列を探す
/// @throw std::logic_error 見つからない場合
const Matrix4d& Placement(const mc::MachineModel& model,
                          const std::vector<Matrix4d>& placements,
                          const std::string& name) {
    const auto index = model.FindComponent(name);
    if (!index.has_value()) throw std::logic_error("component not found: " + name);
    return placements[*index];
}

/// @brief 直進軸のコンポーネント定義を作る
mc::ComponentSpec MakeLinear(const std::string& name, const std::string& parent,
                             const Vector3d& direction) {
    mc::ComponentSpec spec;
    spec.name = name;
    spec.parent = parent;
    spec.type = mc::ComponentType::kLinear;
    mc::AxisSpec axis;
    axis.register_name = name;
    axis.direction = direction;
    axis.limits = std::array<double, 2>{-100.0, 100.0};
    spec.axis = axis;
    return spec;
}

/// @brief マウントのコンポーネント定義を作る
mc::ComponentSpec MakeMount(const std::string& name, const std::string& parent,
                            const mc::ComponentType type) {
    mc::ComponentSpec spec;
    spec.name = name;
    spec.parent = parent;
    spec.type = type;
    spec.frame_placement = Matrix4d::Identity();
    return spec;
}

/// @brief C++で組み立てた構造的に正しい最小の3軸機 (明示base・XYZ・Tool・Table)
/// @note 構造検証の異常系はこれを1箇所だけ壊して作る
mc::MachineDefinition ValidThreeAxis() {
    mc::MachineDefinition definition;
    definition.name = "built";
    mc::ComponentSpec base;
    base.name = "base";
    base.type = mc::ComponentType::kBase;
    definition.components.push_back(base);
    definition.components.push_back(MakeLinear("X", "base", Vector3d::UnitX()));
    definition.components.push_back(MakeLinear("Y", "X", Vector3d::UnitY()));
    definition.components.push_back(MakeLinear("Z", "Y", Vector3d::UnitZ()));
    definition.components.push_back(MakeMount("Tool", "Z", mc::ComponentType::kToolMount));
    definition.components.push_back(MakeMount("Table", "base",
                                              mc::ComponentType::kWorkMount));
    return definition;
}

/// @brief 名前でコンポーネント定義を引く (書き換え用)
/// @throw std::logic_error 見つからない場合
mc::ComponentSpec& Mutable(mc::MachineDefinition& definition, const std::string& name) {
    for (auto& component : definition.components) {
        if (component.name == name) return component;
    }
    throw std::logic_error("component not found: " + name);
}

/// @brief 構築が`std::invalid_argument`で失敗し、文言に識別語を含むことを検証する
void ExpectInvalid(const mc::MachineDefinition& definition, const std::string& keyword) {
    try {
        mc::MachineModel model(definition);
        FAIL() << "std::invalid_argument was not thrown (expected: " << keyword << ")";
    } catch (const std::invalid_argument& e) {
        EXPECT_NE(std::string(e.what()).find(keyword), std::string::npos)
                << "message: " << e.what();
    }
}

/// @brief チェーン外の直進軸`Door` (base直下・レジスタW) を加えた最小構成
std::string WithDoor() {
    return MinimalXyzAc() + R"(
[[component]]
name = "Door"
type = "linear"
parent = "base"

[component.axis]
register = "W"
direction = [0, 1, 0]
limits = [0, 100]
)";
}

/// @brief 両チェーンに共通する直進軸`W` (base直下でA系とX系の親) を持つ構成
std::string WithCommonAxis() {
    std::string toml = MinimalXyzAc();
    toml = Replace(toml, "name = \"A\"\ntype = \"rotary\"\nparent = \"base\"",
                   "name = \"A\"\ntype = \"rotary\"\nparent = \"W\"");
    toml = Replace(toml, "name = \"X\"\ntype = \"linear\"\nparent = \"base\"",
                   "name = \"X\"\ntype = \"linear\"\nparent = \"W\"");
    return toml + R"(
[[component]]
name = "W"
type = "linear"
parent = "base"

[component.axis]
register = "W"
direction = [0, 0, 1]
limits = [-10, 10]
)";
}

}  // namespace



// ---- 正常系: 実例TOML ----

TEST(MachineModelTest, Fixture_OrdersComponentsParentFirst) {
    const mc::MachineModel model = FixtureModel();
    ASSERT_EQ(model.ComponentCount(), 10u);
    EXPECT_EQ(model.Component(0).name, "base");
    EXPECT_FALSE(model.Component(0).parent.has_value());
    for (std::size_t i = 1; i < model.ComponentCount(); ++i) {
        ASSERT_TRUE(model.Component(i).parent.has_value()) << i;
        EXPECT_LT(*model.Component(i).parent, i);
        EXPECT_EQ(model.Spec(i).name, model.Component(i).name);
    }
    EXPECT_EQ(Names(model, model.Component(0).children),
              (std::vector<std::string>{"cradle-frame", "X"}));
    // 定義側はファイル順のまま (baseが末尾)
    EXPECT_EQ(model.Definition().components.back().name, "base");
}

TEST(MachineModelTest, Fixture_AssignsChainsAndSigma) {
    const mc::MachineModel model = FixtureModel();
    EXPECT_EQ(Names(model, model.ToolChain()),
              (std::vector<std::string>{"base", "X", "Y", "Z", "Spindle", "Tool"}));
    EXPECT_EQ(Names(model, model.WorkChain()),
              (std::vector<std::string>{"base", "cradle-frame", "A", "C", "Attach"}));
    for (const char* name : {"A", "C"}) {
        EXPECT_TRUE(Axis(model, name).on_work_chain);
        EXPECT_FALSE(Axis(model, name).on_tool_chain);
        EXPECT_DOUBLE_EQ(Axis(model, name).sigma, -1.0);
        EXPECT_TRUE(Axis(model, name).IsIkTarget());
    }
    for (const char* name : {"X", "Y", "Z"}) {
        EXPECT_TRUE(Axis(model, name).on_tool_chain);
        EXPECT_DOUBLE_EQ(Axis(model, name).sigma, 1.0);
    }
    EXPECT_EQ(model.ChainRegisters(), (std::vector<std::string>{"X", "Y", "Z", "A", "C"}));
    EXPECT_EQ(model.Component(model.ToolMountIndex()).name, "Tool");
    EXPECT_EQ(model.Component(model.WorkMountIndex()).name, "Attach");
}

TEST(MachineModelTest, Fixture_AxesHoldWorldGeometryAndRanges) {
    const mc::MachineModel model = FixtureModel();
    EXPECT_EQ(Registers(model), (std::vector<std::string>{"A", "C", "X", "Y", "Z"}));
    const mc::AxisInfo& a = Axis(model, "A");
    EXPECT_EQ(a.kind, mc::AxisKind::kRotary);
    EXPECT_TRUE(a.direction_world.isApprox(Vector3d::UnitX(), kTol));
    EXPECT_TRUE(a.point_world.isApprox(Vector3d(0.0, 0.0, 60.0), kTol));
    ASSERT_TRUE(a.nc_range.has_value());
    EXPECT_NEAR((*a.nc_range)[0], ToRadians(-90.0), kTol);
    EXPECT_NEAR((*a.nc_range)[1], ToRadians(90.0), kTol);
    const mc::AxisInfo& c = Axis(model, "C");
    EXPECT_TRUE(c.unlimited);
    ASSERT_TRUE(c.nc_range.has_value());
    EXPECT_NEAR((*c.nc_range)[0], 0.0, kTol);
    EXPECT_NEAR((*c.nc_range)[1], mc::kFullTurn, kTol);
    const mc::AxisInfo& x = Axis(model, "X");
    EXPECT_EQ(x.kind, mc::AxisKind::kLinear);
    EXPECT_TRUE(x.point_world.isZero());
    EXPECT_EQ(model.Component(x.component_index).name, "X");
    EXPECT_FALSE(model.FindAxis("W").has_value());
    EXPECT_FALSE(model.FindComponent("nowhere").has_value());
}

TEST(MachineModelTest, IsWithinLimits_ChecksNcValueAgainstLimits) {
    const mc::MachineModel model = FixtureModel();
    const mc::AxisInfo& x = Axis(model, "X");
    ASSERT_TRUE(x.limits.has_value());
    const double lo = (*x.limits)[0];
    const double hi = (*x.limits)[1];
    EXPECT_TRUE(mc::IsWithinLimits(x, 0.5 * (lo + hi)));
    // 範囲端は許容誤差の内側なら受理、外側なら拒否
    EXPECT_TRUE(mc::IsWithinLimits(x, hi + 0.5 * mc::kLimitTolerance));
    EXPECT_FALSE(mc::IsWithinLimits(x, hi + 2.0 * mc::kLimitTolerance));
    EXPECT_TRUE(mc::IsWithinLimits(x, lo - 0.5 * mc::kLimitTolerance));
    EXPECT_FALSE(mc::IsWithinLimits(x, lo - 2.0 * mc::kLimitTolerance));
    // `limits`を持たない軸 (無制限) は常に範囲内
    const mc::AxisInfo& c = Axis(model, "C");
    ASSERT_FALSE(c.limits.has_value());
    EXPECT_TRUE(mc::IsWithinLimits(c, 100.0 * mc::kFullTurn));
}

TEST(MachineModelTest, Fixture_Mounts) {
    const mc::MachineModel model = FixtureModel();
    EXPECT_TRUE(mc::TranslationPart(model.MountPlacement(mc::MountKind::kToolMount))
                        .isApprox(Vector3d(0.0, -180.0, 250.5), kTol));
    EXPECT_TRUE(model.MountPlacement(mc::MountKind::kWorkMount).isIdentity(kTol));
    EXPECT_TRUE(model.ToolAxisHome().isApprox(Vector3d::UnitZ(), kTol));
    ASSERT_TRUE(model.WorkPivotReference().has_value());
    EXPECT_TRUE(model.WorkPivotReference()->isApprox(Vector3d(0.0, 0.0, 60.0), kTol));
    ASSERT_EQ(model.OrientationAxes().size(), 2u);
    EXPECT_EQ(OrientationAxis(model, 0).register_name, "C");
    EXPECT_EQ(OrientationAxis(model, 1).register_name, "A");
    EXPECT_DOUBLE_EQ(OrientationAxis(model, 0).sigma, -1.0);
    EXPECT_TRUE(OrientationAxis(model, 1).point_world.isApprox(
            Vector3d(0.0, 0.0, 60.0), kTol));
}

// ---- 正常系: 派生構成 ----

TEST(MachineModelTest, TiltedBc_DependentAxisFollowsParentRotation) {
    const mc::MachineModel model = ReadModel(TiltedBc());
    const double b = ToRadians(30.0), c = ToRadians(70.0);
    const mc::JointVector q = mc::JointsFromNc(model, {{"B", b}, {"C", c}},
                                               mc::InitialJoints(model));
    const std::vector<Matrix4d> f = mc::Forward(model, q);
    const mc::AxisInfo& axis_b = Axis(model, "B");
    const mc::AxisInfo& axis_c = Axis(model, "C");
    const Matrix4d j_b = mc::RotationAboutLine(axis_b.direction_world, axis_b.point_world,
                                               axis_b.sigma * b);
    const Matrix4d expected =
            mc::RotationAboutLine(mc::ApplyDirection(j_b, axis_c.direction_world),
                                  mc::ApplyPoint(j_b, axis_c.point_world),
                                  axis_c.sigma * c) * j_b;
    EXPECT_TRUE(Placement(model, f, "Attach").isApprox(expected, kTol));
}

TEST(MachineModelTest, TiltedBc_OrientationAxesOrderAndPivot) {
    const mc::MachineModel model = ReadModel(TiltedBc());
    ASSERT_EQ(model.OrientationAxes().size(), 2u);
    EXPECT_EQ(OrientationAxis(model, 0).register_name, "C");
    EXPECT_EQ(OrientationAxis(model, 1).register_name, "B");
    EXPECT_TRUE(OrientationAxis(model, 1).direction_world.isApprox(
            Vector3d(1.0, 0.0, 1.0).normalized(), kTol));
    ASSERT_TRUE(model.WorkPivotReference().has_value());
    EXPECT_TRUE(model.WorkPivotReference()->isApprox(Vector3d::Zero(), kTol));
    EXPECT_EQ(model.ChainRegisters(), (std::vector<std::string>{"X", "Z", "Y", "B", "C"}));
}

TEST(MachineModelTest, HeadBc_OrientationAxesAreToolSide) {
    const mc::MachineModel model = ReadModel(HeadBc());
    ASSERT_EQ(model.OrientationAxes().size(), 2u);
    EXPECT_EQ(OrientationAxis(model, 0).register_name, "C");
    EXPECT_EQ(OrientationAxis(model, 1).register_name, "B");
    EXPECT_DOUBLE_EQ(OrientationAxis(model, 0).sigma, 1.0);
    EXPECT_DOUBLE_EQ(OrientationAxis(model, 1).sigma, 1.0);
    EXPECT_FALSE(model.WorkPivotReference().has_value());
}

TEST(MachineModelTest, ThreeAxis_HasNoOrientationAxes) {
    const mc::MachineModel model = ReadModel(ThreeAxis());
    EXPECT_TRUE(model.OrientationAxes().empty());
    EXPECT_EQ(model.ChainRegisters(), (std::vector<std::string>{"X", "Y", "Z"}));
    const mc::JointVector q = mc::JointsFromNc(
            model, {{"X", 1.0}, {"Y", 2.0}, {"Z", 3.0}}, mc::InitialJoints(model));
    EXPECT_TRUE(Placement(model, mc::Forward(model, q), "Tool").isApprox(
            mc::Translation(Vector3d(1.0, 2.0, 3.0)), kTol));
}

TEST(MachineModelTest, LocalFrame_MapsAxisToWorld) {
    std::string toml = Replace(
            MinimalXyzAc(),
            "parent = \"Y\"\n\n[component.axis]\nregister = \"Z\"\ndirection = [0, 0, 1]",
            "parent = \"Y\"\n\n[component.local_frame]\norigin = [1, 2, 3]\n"
            "rotation_axis_angle = { axis = [1, 0, 0], angle = 90 }\n\n"
            "[component.axis]\nregister = \"Z\"\ndirection = [0, 1, 0]");
    toml = Replace(toml, "parent = \"base\"\n\n[component.axis]\nregister = \"A\"",
                   "parent = \"base\"\n\n[component.local_frame]\norigin = [0, 0, 10]\n\n"
                   "[component.axis]\nregister = \"A\"");
    const mc::MachineModel model = ReadModel(toml);
    EXPECT_TRUE(Axis(model, "Z").direction_world.isApprox(Vector3d::UnitZ(), kTol));
    EXPECT_TRUE(Axis(model, "A").point_world.isApprox(Vector3d(0.0, 0.0, 70.0), kTol));
}

TEST(MachineModelTest, OffChainAxis_IsNotIkTargetButMoves) {
    const mc::MachineModel model = ReadModel(WithDoor());
    const mc::AxisInfo& door = Axis(model, "W");
    EXPECT_FALSE(door.on_tool_chain);
    EXPECT_FALSE(door.on_work_chain);
    EXPECT_FALSE(door.IsIkTarget());
    EXPECT_DOUBLE_EQ(door.sigma, 1.0);
    const mc::JointVector q = mc::JointsFromNc(model, {{"W", 25.0}},
                                               mc::InitialJoints(model));
    EXPECT_TRUE(Placement(model, mc::Forward(model, q), "Door").isApprox(
            mc::Translation(Vector3d(0.0, 25.0, 0.0)), kTol));
    EXPECT_EQ(model.ChainRegisters(), (std::vector<std::string>{"X", "Y", "Z", "A", "C"}));
}

TEST(MachineModelTest, CommonAxis_IsExcludedFromIk) {
    const mc::MachineModel model = ReadModel(WithCommonAxis());
    const mc::AxisInfo& w = Axis(model, "W");
    EXPECT_TRUE(w.on_tool_chain);
    EXPECT_TRUE(w.on_work_chain);
    EXPECT_FALSE(w.IsIkTarget());
    EXPECT_DOUBLE_EQ(w.sigma, -1.0);
    EXPECT_EQ(model.ChainRegisters(),
              (std::vector<std::string>{"W", "X", "Y", "Z", "A", "C"}));
    EXPECT_EQ(model.OrientationAxes().size(), 2u);
}

// ---- 異常系 ----

TEST(MachineModelTest, Constructor_ThrowsInvalidArgumentOnStructuralErrors) {
    {
        mc::MachineDefinition def = ValidThreeAxis();
        def.components.erase(def.components.begin());   // base欠落
        ExpectInvalid(def, "exactly one base");
    }
    {
        mc::MachineDefinition def = ValidThreeAxis();
        def.components.push_back(def.components.front());   // base重複 (名前も重複)
        ExpectInvalid(def, "duplicate component name");
    }
    {
        mc::MachineDefinition def = ValidThreeAxis();
        Mutable(def, "Y").parent = "nowhere";
        ExpectInvalid(def, "parent does not exist");
    }
    {
        mc::MachineDefinition def = ValidThreeAxis();
        Mutable(def, "X").parent = "Z";   // X→Z→Y→X の閉路
        ExpectInvalid(def, "cycle");
    }
    {
        mc::MachineDefinition def = ValidThreeAxis();
        Mutable(def, "Y").axis.reset();
        ExpectInvalid(def, "presence of axis");
    }
    {
        mc::MachineDefinition def = ValidThreeAxis();
        Mutable(def, "Y").type = mc::ComponentType::kRotary;   // pointなし
        ExpectInvalid(def, "rotary axis has no point");
    }
    {
        mc::MachineDefinition def = ValidThreeAxis();
        Mutable(def, "Tool").frame_placement.reset();
        ExpectInvalid(def, "presence of frame_placement");
    }
    {
        mc::MachineDefinition def = ValidThreeAxis();
        def.components.push_back(MakeMount("Tool2", "Z", mc::ComponentType::kToolMount));
        ExpectInvalid(def, "exactly one tool_mount");
    }
    {
        mc::MachineDefinition def = ValidThreeAxis();
        Mutable(def, "Y").axis->register_name = "X";
        ExpectInvalid(def, "duplicate register");
    }
    {
        mc::MachineDefinition def = ValidThreeAxis();
        Mutable(def, "Y").axis->direction = Vector3d::Zero();
        ExpectInvalid(def, "zero vector");
    }
    {
        mc::MachineDefinition def = ValidThreeAxis();
        Mutable(def, "base").parent = "X";
        ExpectInvalid(def, "base must not have a parent");
    }
    EXPECT_NO_THROW(mc::MachineModel{ValidThreeAxis()});
}

TEST(MachineModelTest, Component_ThrowsOutOfRangeWhenIndexIsOutOfRange) {
    const mc::MachineModel model = FixtureModel();
    EXPECT_THROW(model.Component(model.ComponentCount()), std::out_of_range);
    EXPECT_THROW(model.Spec(model.ComponentCount()), std::out_of_range);
    EXPECT_NO_THROW(model.Component(model.ComponentCount() - 1));
    EXPECT_NO_THROW(model.Spec(model.ComponentCount() - 1));
}

// ---- コピー ----

TEST(MachineModelTest, Copy_IsIndependentOfSource) {
    mc::MachineDefinition definition = ReadDefinition(MinimalXyzAc());
    const mc::MachineModel original(std::move(definition));
    const mc::MachineModel copy = original;
    EXPECT_EQ(copy.ComponentCount(), original.ComponentCount());
    EXPECT_EQ(copy.Definition().name, "minimal-xyz-ac");
    EXPECT_EQ(copy.Spec(*copy.FindComponent("X")).geometries.size(), 1u);
    const mc::JointVector q = mc::JointsFromNc(copy, {{"A", ToRadians(20.0)}},
                                               mc::InitialJoints(copy));
    const std::vector<Matrix4d> a = mc::Forward(original, q);
    const std::vector<Matrix4d> b = mc::Forward(copy, q);
    for (std::size_t i = 0; i < a.size(); ++i) EXPECT_TRUE(a[i].isApprox(b[i], kTol));
}
