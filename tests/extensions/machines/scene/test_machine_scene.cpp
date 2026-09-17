/**
 * @file tests/extensions/machines/scene/test_machine_scene.cpp
 * @brief シーン構築 (scene/machine_scene) のテスト
 * @author Yayoi Habami
 * @date 2026-09-16
 * @copyright 2026 Yayoi Habami
 * @note 対象: MachineScene
 *       - 構築: 木の形 (コンポーネント、形状、工具、ワーク座標系、置き場)、干渉専用
 *         形状の非表示、読込失敗の警告化、置き場のIDの安定、再構築、Clear
 *       - 姿勢と工具: ApplyPose/ResetToZeroPoseの大域変換、工具の選択とホルダ,
 *         工具軸線と制御点マーカー、3軸
 *       - 経路線: ワークオフセットと種別による分割、円弧の折れ線化、機械座標の移動
 *         による分割、未定義のワークオフセット、作り直し
 *       - 可視性: 機械部品と役割別のモデル、経路線、3軸
 *       - ワークビュー: 隠すIDと`work_mount`のワールド変換
 *       - 異常系: `nullptr`のルート (`invalid_argument`)、未構築での操作
 *         (`logic_error`)
 * @note フィクスチャは`MinimalXyzAc` (動特性なし. X軸にbox形状) と
 *       `MinimalProject` (簡易ボール工具#1・G54登録値・G55幾何形式・boxストック).
 *       形状はプリミティブのみで、ファイルを参照しない
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "igesio/common/color.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/entities/entity_base.h"
#include "igesio/entities/entity_type.h"
#include "igesio/entities/curves/line.h"
#include "igesio/entities/curves/linear_path.h"
#include "igesio/entities/curves/point.h"
#include "igesio/models/assembly.h"
#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/core/rotation.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/machine/axis_values.h"
#include "igesio/extensions/machines/machine/forward_kinematics.h"
#include "igesio/extensions/machines/machine/machine_model.h"
#include "igesio/extensions/machines/project/project_definition.h"
#include "igesio/extensions/machines/project/setup.h"
#include "igesio/extensions/machines/scene/machine_scene.h"
#include "igesio/extensions/machines/tools/tool_assembly.h"
#include "igesio/extensions/machines/tools/tool_entities.h"
#include "igesio/extensions/machines/tools/tool_profile.h"
#include "igesio/extensions/machines/toolpath/cl_program.h"
#include "igesio/extensions/machines/toolpath/cl_transform.h"
#include "../machine/machines_for_testing.h"
#include "../project/projects_for_testing.h"
#include "../simulation/motion_for_testing.h"
#include "scene_for_testing.h"

namespace {

namespace mc = igesio::extensions::machines;
namespace i_ent = igesio::entities;
namespace i_mod = igesio::models;
using igesio::Vector3d;
using igesio::Matrix4d;
using mc::ToRadians;
using machines_test::MinimalXyzAc;
using projects_test::MinimalProject;
using projects_test::Replace;
using motion_test::CountWarnings;
using motion_test::Goto;
using motion_test::MakeSetupWithMachine;
using motion_test::MakeSetupWithoutDynamics;
using motion_test::Program;
using motion_test::Words;
using motion_test::WithTool;
using scene_test::BuiltScene;
using scene_test::FindChild;

/// @brief 数値比較の許容誤差
constexpr double kTol = 1e-9;

/// @brief 座標の辞書順 (x, y, zの順) の比較
bool LessByCoordinate(const Vector3d& lhs, const Vector3d& rhs) {
    for (int i = 0; i < 3; ++i) {
        if (lhs[i] != rhs[i]) return lhs[i] < rhs[i];
    }
    return false;
}

/// @brief 直接所有する3次元折れ線 (Type 106) を集める
/// @return 先頭の頂点の辞書順 (アセンブリの所有順序は不定のため)
std::vector<std::shared_ptr<i_ent::LinearPath>> LinearPathsOf(
        const i_mod::Assembly& node) {
    std::vector<std::shared_ptr<i_ent::LinearPath>> paths;
    for (const auto& entity : node.FindEntitiesByType(i_ent::EntityType::kCopiousData)) {
        paths.push_back(std::dynamic_pointer_cast<i_ent::LinearPath>(entity));
    }
    std::sort(paths.begin(), paths.end(),
              [](const auto& lhs, const auto& rhs) {
                  return LessByCoordinate(lhs->Coordinate(0), rhs->Coordinate(0));
              });
    return paths;
}

/// @brief 直接所有する線分 (Type 110) を集める
/// @return 終点の辞書順 (アセンブリの所有順序は不定のため)
std::vector<std::shared_ptr<i_ent::Line>> LinesOf(const i_mod::Assembly& node) {
    std::vector<std::shared_ptr<i_ent::Line>> lines;
    for (const auto& entity : node.FindEntitiesByType(i_ent::EntityType::kLine)) {
        lines.push_back(std::dynamic_pointer_cast<i_ent::Line>(entity));
    }
    std::sort(lines.begin(), lines.end(),
              [](const auto& lhs, const auto& rhs) {
                  return LessByCoordinate(lhs->GetAnchorPoints().second,
                                          rhs->GetAnchorPoints().second);
              });
    return lines;
}

/// @brief 既定のフィクスチャでシーンを作る
BuiltScene MakeScene(const mc::SceneBuildOptions& options = {}) {
    return scene_test::MakeSceneWithoutDynamics(options);
}

/// @brief 3軸のアセンブリが長さ40の3本の線分で、各軸の色を持つことを確かめる
/// @note `LinesOf`は終点の辞書順なので、Z軸、Y軸、X軸の順に並ぶ
void ExpectTriad(const i_mod::Assembly& triad) {
    const auto lines = LinesOf(triad);
    ASSERT_EQ(lines.size(), 3u);
    const Vector3d axes[3] = {Vector3d::UnitX(), Vector3d::UnitY(), Vector3d::UnitZ()};
    for (std::size_t i = 0; i < 3; ++i) {
        const auto& line = lines[2 - i];
        const auto [start, end] = line->GetAnchorPoints();
        EXPECT_TRUE(start.isZero(kTol));
        EXPECT_TRUE(end.isApprox(axes[i] * mc::kTriadLength, kTol)) << end.transpose();
        const igesio::Color color = line->GetColor();
        EXPECT_NEAR(color.r, mc::kTriadColors[i].r, 1e-2);
        EXPECT_NEAR(color.g, mc::kTriadColors[i].g, 1e-2);
        EXPECT_NEAR(color.b, mc::kTriadColors[i].b, 1e-2);
    }
}

}  // namespace



/**
 * ---- 構築 ----
 */

TEST(MachineSceneTest, Build_TreeShape) {
    const BuiltScene built = MakeScene();
    const mc::MachineScene& scene = built.scene;
    ASSERT_TRUE(scene.IsBuilt());
    ASSERT_EQ(built.root->GetChildAssemblies().size(), 1u);
    const auto machine = scene.MachineAssembly();
    EXPECT_EQ(machine->Metadata().name, "machine:minimal-xyz-ac");

    // 全コンポーネントが同じ階層に並び、名前で引ける
    const mc::MachineModel& model = scene.Model();
    for (std::size_t i = 0; i < model.ComponentCount(); ++i) {
        const std::string& name = model.Component(i).name;
        const auto component = scene.ComponentAssembly(name);
        ASSERT_NE(component, nullptr) << name;
        EXPECT_EQ(component->GetParent().lock(), machine);
        EXPECT_TRUE(component->GetGlobalTransform().isIdentity(kTol));
    }
    EXPECT_EQ(scene.ToolMountAssembly(), scene.ComponentAssembly("Tool"));
    EXPECT_EQ(scene.WorkMountAssembly(), scene.ComponentAssembly("Table"));

    // 形状: X軸のbox (機械部品) とストック (モデル)
    ASSERT_EQ(scene.GeometryCount(), 2u);
    const auto x_box = scene.GeometryAssembly(0);
    ASSERT_NE(x_box, nullptr);
    EXPECT_EQ(x_box->Metadata().name, "geometry:x-box");
    EXPECT_EQ(x_box->Metadata().role_tag, mc::kMachinePartRoleTag);
    EXPECT_EQ(x_box->GetParent().lock(), scene.ComponentAssembly("X"));
    const auto stock = scene.GeometryAssembly(1);
    ASSERT_NE(stock, nullptr);
    EXPECT_EQ(stock, scene.ModelAssembly("stock"));
    EXPECT_EQ(stock->Metadata().name, "model:stock");
    EXPECT_EQ(stock->Metadata().role_tag, "stock");
    EXPECT_EQ(stock->GetParent().lock(), scene.WorkMountAssembly());
    EXPECT_TRUE(stock->GetGlobalTransform().isApprox(
            mc::Translation(Vector3d(0.0, 0.0, 20.0)), kTol));

    // 工具は`tool_mount`の子
    const auto tool = scene.ToolAssembly(1);
    ASSERT_NE(tool, nullptr);
    EXPECT_EQ(tool->GetParent().lock(), scene.ToolMountAssembly());
    EXPECT_EQ(scene.ToolAssembly(2), nullptr);

    // ワーク座標系ごとの3軸、経路線、取り付け先は所属コンポーネントの子
    for (const std::string_view id : {"G54", "G55"}) {
        for (const auto& node : {scene.WorkFrameAssembly(id), scene.PathsAssembly(id),
                                 scene.AttachAssembly(id)}) {
            ASSERT_NE(node, nullptr) << id;
            EXPECT_EQ(node->GetParent().lock(), scene.WorkMountAssembly()) << id;
        }
    }
    EXPECT_EQ(scene.WorkFrameAssembly("G59"), nullptr);
    EXPECT_EQ(scene.InitialWorkOffset(), "G54");
    EXPECT_EQ(scene.Tools().size(), 1u);
    EXPECT_EQ(scene.WorkFrames().size(), 2u);
    EXPECT_TRUE(scene.Warnings().empty());
}

TEST(MachineSceneTest, Build_HiddenGeometryIsInvisible) {
    BuiltScene built;
    built.scene.Build(MakeSetupWithMachine(
                              MinimalXyzAc(), "igesio_scene_hidden",
                              Replace(MinimalProject(), "role = \"stock\"\n",
                                      "role = \"stock\"\nvisible = false\n")),
                      built.root);
    const auto stock = built.scene.GeometryAssembly(1);
    ASSERT_NE(stock, nullptr);
    EXPECT_FALSE(stock->Display().visible);

    // 干渉専用の形状は役割別の切り替えの対象外
    built.scene.SetModelRoleVisible(mc::ModelRole::kStock, true);
    EXPECT_FALSE(stock->Display().visible);
}

TEST(MachineSceneTest, Build_LoadFailureIsWarning) {
    const std::string machine = Replace(
            MinimalXyzAc(), "size = [10, 20, 30]\n",
            "size = [10, 20, 30]\n\n[[component.geometry]]\nfile = \"missing.stl\"\n");
    BuiltScene built;
    built.scene.Build(MakeSetupWithMachine(machine, "igesio_scene_missing"), built.root);
    const mc::MachineScene& scene = built.scene;

    // 読めない形状は警告になり、他の形状は作られる
    EXPECT_EQ(CountWarnings(scene.Warnings(), "could not be loaded"), 1u);
    ASSERT_EQ(scene.GeometryCount(), 3u);
    EXPECT_NE(scene.GeometryAssembly(0), nullptr);
    EXPECT_EQ(scene.GeometryAssembly(1), nullptr);
    EXPECT_NE(scene.GeometryAssembly(2), nullptr);
    EXPECT_EQ(scene.GeometryAssembly(3), nullptr);
}

TEST(MachineSceneTest, Build_StableNodesExistAndRebuildReplaces) {
    BuiltScene built = MakeScene();
    mc::MachineScene& scene = built.scene;
    const auto trajectory = scene.TrajectoryAssembly();
    const auto machine_trace = scene.MachineTraceAssembly();
    const auto work_trace = scene.WorkTraceAssembly();
    const auto attach = scene.AttachAssembly("G54");
    for (const auto& node : {trajectory, machine_trace, work_trace, attach}) {
        ASSERT_NE(node, nullptr);
        EXPECT_EQ(node->GetEntityCount(), 0u);
        EXPECT_TRUE(node->GetChildAssemblies().empty());
    }
    EXPECT_EQ(trajectory->GetParent().lock(), scene.WorkMountAssembly());
    EXPECT_EQ(work_trace->GetParent().lock(), scene.WorkMountAssembly());
    EXPECT_EQ(machine_trace->GetParent().lock(), scene.MachineAssembly());

    // 経路線を作り直しても置き場は同じまま
    scene.RebuildPaths(WithTool({Goto(Vector3d(0.0, 0.0, 10.0)),
                                 Goto(Vector3d(10.0, 0.0, 10.0))}));
    EXPECT_EQ(scene.TrajectoryAssembly(), trajectory);
    EXPECT_EQ(scene.AttachAssembly("G54"), attach);

    // 再構築は先にClearし、ルートの子は1つのまま
    scene.Build(MakeSetupWithoutDynamics(), built.root);
    EXPECT_EQ(built.root->GetChildAssemblies().size(), 1u);
    EXPECT_NE(scene.TrajectoryAssembly(), trajectory);
}

TEST(MachineSceneTest, Clear_RemovesMachine) {
    BuiltScene built = MakeScene();
    built.scene.Clear();
    EXPECT_FALSE(built.scene.IsBuilt());
    EXPECT_TRUE(built.root->GetChildAssemblies().empty());
    EXPECT_EQ(built.scene.MachineAssembly(), nullptr);
    EXPECT_EQ(built.scene.GeometryCount(), 0u);
    EXPECT_EQ(built.scene.ToolAssembly(1), nullptr);
    EXPECT_EQ(built.scene.ComponentAssembly("X"), nullptr);
    EXPECT_TRUE(built.scene.WorkViewHiddenIds().empty());
    // 可視性の切り替えは未構築でも何もしない
    EXPECT_NO_THROW(built.scene.SetPathsVisible(false));

    built.scene.Build(MakeSetupWithoutDynamics(), built.root);
    EXPECT_TRUE(built.scene.IsBuilt());
    EXPECT_EQ(built.root->GetChildAssemblies().size(), 1u);
}

TEST(MachineSceneTest, Build_ThrowsInvalidArgumentWhenRootIsNull) {
    mc::MachineScene scene;
    EXPECT_THROW(scene.Build(MakeSetupWithoutDynamics(), nullptr), std::invalid_argument);
    EXPECT_FALSE(scene.IsBuilt());
}

TEST(MachineSceneTest, Operations_ThrowLogicErrorWhenNotBuilt) {
    mc::MachineScene scene;
    EXPECT_THROW(scene.Model(), std::logic_error);
    EXPECT_THROW(scene.ApplyPose(mc::JointVector(5)), std::logic_error);
    EXPECT_THROW(scene.ResetToZeroPose(), std::logic_error);
    EXPECT_THROW(scene.SetActiveTool(1), std::logic_error);
    EXPECT_THROW(scene.RebuildPaths(mc::ClProgram{}), std::logic_error);
    EXPECT_THROW(scene.WorkMountWorldTransform(), std::logic_error);
}



/**
 * ---- 姿勢と工具 ----
 */

TEST(MachineSceneTest, ApplyPose_WorldPlacement) {
    BuiltScene built = MakeScene();
    mc::MachineScene& scene = built.scene;
    const mc::MachineModel& model = scene.Model();
    const mc::JointVector q = mc::JointsFromNc(
            model, mc::NcValues{{"X", 10.0}, {"A", ToRadians(30.0)}},
            mc::InitialJoints(model));
    scene.ApplyPose(q);

    const std::vector<Matrix4d> frames = mc::Forward(model, q);
    for (std::size_t i = 0; i < model.ComponentCount(); ++i) {
        const auto component = scene.ComponentAssembly(model.Component(i).name);
        EXPECT_TRUE(component->GetGlobalTransform().isApprox(frames[i], kTol))
                << model.Component(i).name;
    }
    const Matrix4d expected_tool = frames[model.ToolMountIndex()]
                                   * model.MountPlacement(mc::MountKind::kToolMount)
                                   * mc::ToolMountOffset(scene.Tools().at(1));
    EXPECT_TRUE(scene.ToolAssembly(1)->GetWorldTransform().isApprox(expected_tool, kTol));

    scene.ResetToZeroPose();
    for (std::size_t i = 0; i < model.ComponentCount(); ++i) {
        const auto component = scene.ComponentAssembly(model.Component(i).name);
        EXPECT_TRUE(component->GetGlobalTransform().isIdentity(kTol));
    }
}

TEST(MachineSceneTest, ApplyPose_ThrowsInvalidArgumentWhenAxisCountDiffers) {
    BuiltScene built = MakeScene();
    const std::size_t axes = built.scene.Model().Axes().size();
    EXPECT_THROW(built.scene.ApplyPose(mc::JointVector(axes - 1)), std::invalid_argument);
    EXPECT_NO_THROW(built.scene.ApplyPose(mc::JointVector(axes)));
}

TEST(MachineSceneTest, Tools_ActiveAndHolder) {
    BuiltScene built = MakeScene();
    mc::MachineScene& scene = built.scene;
    const auto tool = scene.ToolAssembly(1);
    EXPECT_EQ(scene.ActiveTool(), 1);
    EXPECT_TRUE(tool->Display().visible);

    scene.SetActiveTool(mc::kNoTool);
    EXPECT_EQ(scene.ActiveTool(), mc::kNoTool);
    EXPECT_FALSE(tool->Display().visible);
    scene.SetActiveTool(1);
    EXPECT_TRUE(tool->Display().visible);
    // 工具表に無い番号は全工具を非表示にする
    scene.SetActiveTool(99);
    EXPECT_EQ(scene.ActiveTool(), mc::kNoTool);
    EXPECT_FALSE(tool->Display().visible);

    const auto holder = mc::FindToolPart(*tool, mc::ToolPart::kHolder);
    ASSERT_NE(holder, nullptr);
    scene.SetHolderVisible(false);
    EXPECT_FALSE(holder->Display().visible);
    scene.SetHolderVisible(true);
    EXPECT_TRUE(holder->Display().visible);

    const std::map<int, igesio::ObjectID> ids = scene.ToolAssemblyIds();
    ASSERT_EQ(ids.size(), 1u);
    EXPECT_EQ(ids.at(1), tool->GetID());
}

TEST(MachineSceneTest, ToolAxis_AndControlPoint) {
    BuiltScene built = MakeScene();
    const mc::MachineScene& scene = built.scene;
    const auto tool = scene.ToolAssembly(1);
    const mc::ToolProfile& profile = scene.Tools().at(1).profile;

    const auto axis = FindChild(*tool, mc::kToolAxisLineName);
    ASSERT_NE(axis, nullptr);
    const auto lines = LinesOf(*axis);
    ASSERT_EQ(lines.size(), 1u);
    const auto [start, end] = lines[0]->GetAnchorPoints();
    EXPECT_TRUE(start.isZero(kTol));
    EXPECT_TRUE(end.isApprox(
            Vector3d(0.0, 0.0, profile.Reach() + mc::kToolAxisExtraLength), kTol))
            << end.transpose();

    // 制御点マーカーは工具座標 (先端原点) の指令点に置く
    const auto marker = FindChild(*tool, mc::kControlPointName);
    ASSERT_NE(marker, nullptr);
    const auto points = marker->FindEntitiesByType(i_ent::EntityType::kPoint);
    ASSERT_EQ(points.size(), 1u);
    const auto point = std::dynamic_pointer_cast<i_ent::Point>(points[0]);
    EXPECT_TRUE(point->GetPosition().isApprox(
            Vector3d(0.0, 0.0, profile.command_point_z), kTol));

    built.scene.SetToolAxisVisible(false);
    EXPECT_FALSE(axis->Display().visible);
    EXPECT_FALSE(marker->Display().visible);

    mc::SceneBuildOptions options;
    options.tool_axis_extra = 0.0;
    const BuiltScene without = MakeScene(options);
    EXPECT_EQ(FindChild(*without.scene.ToolAssembly(1), mc::kToolAxisLineName), nullptr);
    EXPECT_EQ(FindChild(*without.scene.ToolAssembly(1), mc::kControlPointName), nullptr);
}

TEST(MachineSceneTest, Triads_MachineAndToolMount) {
    BuiltScene built = MakeScene();
    mc::MachineScene& scene = built.scene;
    const auto machine_triad = FindChild(*scene.MachineAssembly(), mc::kMachineTriadName);
    const auto mount_triad =
            FindChild(*scene.ToolMountAssembly(), mc::kToolMountTriadName);
    ASSERT_NE(machine_triad, nullptr);
    ASSERT_NE(mount_triad, nullptr);
    EXPECT_TRUE(machine_triad->GetGlobalTransform().isIdentity(kTol));
    EXPECT_TRUE(mount_triad->GetGlobalTransform().isApprox(
            scene.Model().MountPlacement(mc::MountKind::kToolMount), kTol));
    ExpectTriad(*machine_triad);
    ExpectTriad(*mount_triad);

    scene.SetTriadsVisible(false);
    EXPECT_FALSE(machine_triad->Display().visible);
    EXPECT_FALSE(mount_triad->Display().visible);

    mc::SceneBuildOptions options;
    options.build_triads = false;
    const BuiltScene without = MakeScene(options);
    EXPECT_EQ(FindChild(*without.scene.MachineAssembly(), mc::kMachineTriadName),
              nullptr);
    EXPECT_EQ(FindChild(*without.scene.ToolMountAssembly(), mc::kToolMountTriadName),
              nullptr);
}

TEST(MachineSceneTest, WorkFrames_TriadGeometry) {
    BuiltScene built = MakeScene();
    mc::MachineScene& scene = built.scene;
    const auto frame = scene.WorkFrameAssembly("G54");
    ASSERT_NE(frame, nullptr);
    ExpectTriad(*frame);
    EXPECT_TRUE(frame->GetGlobalTransform().isApprox(scene.WorkFrames()[0].w0, kTol));
    EXPECT_TRUE(scene.WorkFrameAssembly("G55")->GetGlobalTransform().isApprox(
            scene.WorkFrames()[1].w0, kTol));

    scene.SetWorkFramesVisible(false);
    EXPECT_FALSE(frame->Display().visible);
    EXPECT_FALSE(scene.WorkFrameAssembly("G55")->Display().visible);

    mc::SceneBuildOptions options;
    options.build_work_frames = false;
    const BuiltScene without = MakeScene(options);
    EXPECT_EQ(without.scene.WorkFrameAssembly("G54"), nullptr);
    EXPECT_NE(without.scene.PathsAssembly("G54"), nullptr);
}



/**
 * ---- 経路線 ----
 */

TEST(MachineSceneTest, Paths_SplitByOffsetAndKind) {
    BuiltScene built = MakeScene();
    mc::MachineScene& scene = built.scene;
    const mc::ClProgram program = WithTool({
            Goto(Vector3d(0.0, 0.0, 50.0), std::nullopt, mc::MotionKind::kRapid),
            Goto(Vector3d(0.0, 0.0, 10.0), std::nullopt, mc::MotionKind::kRapid),
            Goto(Vector3d(10.0, 0.0, 10.0)),
            Goto(Vector3d(10.0, 10.0, 10.0)),
            Goto(Vector3d(0.0, 10.0, 10.0)),
            Goto(Vector3d(0.0, 10.0, 50.0), std::nullopt, mc::MotionKind::kRapid),
            mc::ClSelectWorkOffset{"G55"},
            Goto(Vector3d(0.0, 0.0, 5.0)),
            Goto(Vector3d(5.0, 0.0, 5.0))});
    std::vector<mc::Diagnostic> warnings;
    scene.RebuildPaths(program, &warnings);
    EXPECT_TRUE(warnings.empty());

    const auto g54 = scene.PathsAssembly("G54");
    const auto rapid = LinearPathsOf(*FindChild(*g54, mc::kRapidPathsName));
    const auto cut = LinearPathsOf(*FindChild(*g54, mc::kCutPathsName));
    ASSERT_EQ(rapid.size(), 2u);
    ASSERT_EQ(cut.size(), 1u);
    // 先頭の区間は直前の位置を持たず、以降の区間は直前の区間の終点から始まる
    EXPECT_EQ(rapid[0]->GetCount(), 2u);
    EXPECT_EQ(cut[0]->GetCount(), 4u);
    EXPECT_TRUE(cut[0]->Coordinate(0).isApprox(Vector3d(0.0, 0.0, 10.0), kTol));
    EXPECT_EQ(rapid[1]->GetCount(), 2u);
    EXPECT_TRUE(rapid[1]->Coordinate(0).isApprox(Vector3d(0.0, 10.0, 10.0), kTol));

    // 別のワークオフセットの区間は直前の位置を引き継がない
    const auto g55 = scene.PathsAssembly("G55");
    EXPECT_TRUE(LinearPathsOf(*FindChild(*g55, mc::kRapidPathsName)).empty());
    const auto g55_cut = LinearPathsOf(*FindChild(*g55, mc::kCutPathsName));
    ASSERT_EQ(g55_cut.size(), 1u);
    EXPECT_EQ(g55_cut[0]->GetCount(), 2u);
    EXPECT_TRUE(g55_cut[0]->Coordinate(0).isApprox(Vector3d(0.0, 0.0, 5.0), kTol));
}

TEST(MachineSceneTest, Paths_ArcIsDiscretized) {
    BuiltScene built = MakeScene();
    mc::MachineScene& scene = built.scene;
    mc::ClArc arc;
    arc.kind = mc::MotionKind::kArcCcw;
    arc.end = Vector3d(0.0, 10.0, 10.0);
    arc.center = Vector3d(0.0, 0.0, 10.0);
    arc.normal = Vector3d::UnitZ();
    const Vector3d start(10.0, 0.0, 10.0);
    scene.RebuildPaths(WithTool({Goto(start), arc}));

    const std::vector<Vector3d> expected = mc::DiscretizeArc(start, arc, 0.05);
    const auto cut = LinearPathsOf(*FindChild(*scene.PathsAssembly("G54"),
                                              mc::kCutPathsName));
    ASSERT_EQ(cut.size(), 1u);
    ASSERT_EQ(cut[0]->GetCount(), expected.size() + 1);
    EXPECT_TRUE(cut[0]->Coordinate(0).isApprox(start, kTol));
    for (std::size_t i = 0; i < expected.size(); ++i) {
        EXPECT_TRUE(cut[0]->Coordinate(i + 1).isApprox(expected[i], kTol)) << i;
    }
}

TEST(MachineSceneTest, Paths_MachineFrameBreaksRuns) {
    BuiltScene built = MakeScene();
    mc::MachineScene& scene = built.scene;
    scene.RebuildPaths(WithTool({
            Goto(Vector3d(0.0, 0.0, 10.0)),
            mc::ClComment{"inside"},
            mc::ClMarker{mc::ClMarker::Kind::kOperation, "op"},
            Goto(Vector3d(10.0, 0.0, 10.0)),
            Words(mc::NcValues{{"Z", 0.0}}, mc::MotionFrame::kMachine),
            Goto(Vector3d(10.0, 10.0, 10.0)),
            Words(mc::NcValues{{"A", 0.5}}),
            Goto(Vector3d(0.0, 10.0, 10.0)),
            Goto(Vector3d(0.0, 0.0, 10.0))}));

    // 機械座標の移動と軸の指令のみの移動は含めず、前後で区間が分かれる.
    // コメントと区切りは区間を分けない
    const auto cut = LinearPathsOf(*FindChild(*scene.PathsAssembly("G54"),
                                              mc::kCutPathsName));
    ASSERT_EQ(cut.size(), 2u);
    EXPECT_EQ(cut[0]->GetCount(), 2u);
    EXPECT_TRUE(cut[0]->Coordinate(1).isApprox(Vector3d(10.0, 0.0, 10.0), kTol));
    EXPECT_EQ(cut[1]->GetCount(), 2u);
    EXPECT_TRUE(cut[1]->Coordinate(0).isApprox(Vector3d(0.0, 10.0, 10.0), kTol));
}

TEST(MachineSceneTest, Paths_UnknownOffsetIsSkipped) {
    BuiltScene built = MakeScene();
    mc::MachineScene& scene = built.scene;
    std::vector<mc::Diagnostic> warnings;
    scene.RebuildPaths(WithTool({
            Goto(Vector3d(0.0, 0.0, 10.0)),
            Goto(Vector3d(10.0, 0.0, 10.0)),
            mc::ClSelectWorkOffset{"G59"},
            Goto(Vector3d(20.0, 0.0, 10.0)),
            Goto(Vector3d(30.0, 0.0, 10.0)),
            mc::ClSelectWorkOffset{"G54"},
            Goto(Vector3d(40.0, 0.0, 10.0))}), &warnings);
    EXPECT_EQ(CountWarnings(warnings, "G59"), 1u);

    // 未定義のワークオフセットの区間は無く、戻った後は新しい区間になる
    const auto cut = LinearPathsOf(*FindChild(*scene.PathsAssembly("G54"),
                                              mc::kCutPathsName));
    ASSERT_EQ(cut.size(), 1u);
    EXPECT_EQ(cut[0]->GetCount(), 2u);
    EXPECT_EQ(scene.PathsAssembly("G59"), nullptr);
}

TEST(MachineSceneTest, Rebuild_ReplacesPaths) {
    BuiltScene built = MakeScene();
    mc::MachineScene& scene = built.scene;
    scene.RebuildPaths(WithTool({Goto(Vector3d(0.0, 0.0, 10.0)),
                                 Goto(Vector3d(10.0, 0.0, 10.0)),
                                 Goto(Vector3d(10.0, 0.0, 50.0), std::nullopt,
                                      mc::MotionKind::kRapid)}));
    scene.RebuildPaths(WithTool({Goto(Vector3d(0.0, 0.0, 10.0)),
                                 Goto(Vector3d(10.0, 0.0, 10.0))}));
    const auto paths = scene.PathsAssembly("G54");
    const auto rapid = FindChild(*paths, mc::kRapidPathsName);
    EXPECT_EQ(LinearPathsOf(*FindChild(*paths, mc::kCutPathsName)).size(), 1u);
    EXPECT_TRUE(LinearPathsOf(*rapid).empty());

    scene.SetPathsVisible(false);
    EXPECT_FALSE(paths->Display().visible);
    EXPECT_FALSE(scene.PathsAssembly("G55")->Display().visible);
    scene.SetPathsVisible(true);
    scene.SetRapidPathsVisible(false);
    EXPECT_TRUE(paths->Display().visible);
    EXPECT_FALSE(rapid->Display().visible);
}



/**
 * ---- 可視性とワークビュー ----
 */

TEST(MachineSceneTest, Visibility_MachinePartsAndRoles) {
    BuiltScene built = MakeScene();
    mc::MachineScene& scene = built.scene;
    const auto x_box = scene.GeometryAssembly(0);
    const auto stock = scene.ModelAssembly("stock");

    scene.SetMachinePartsVisible(false);
    EXPECT_FALSE(x_box->Display().visible);
    EXPECT_TRUE(stock->Display().visible);
    scene.SetMachinePartsVisible(true);
    EXPECT_TRUE(x_box->Display().visible);

    scene.SetModelRoleVisible(mc::ModelRole::kFixture, false);
    EXPECT_TRUE(stock->Display().visible);
    scene.SetModelRoleVisible(mc::ModelRole::kStock, false);
    EXPECT_FALSE(stock->Display().visible);
    EXPECT_TRUE(x_box->Display().visible);
}

TEST(MachineSceneTest, Attach_SurvivesRebuildPaths) {
    BuiltScene built = MakeScene();
    mc::MachineScene& scene = built.scene;
    const auto attach = scene.AttachAssembly("G54");
    const auto extra = i_mod::MakeAssembly("pnf");
    attach->AddChildAssembly(extra);

    scene.RebuildPaths(WithTool({Goto(Vector3d(0.0, 0.0, 10.0)),
                                 Goto(Vector3d(10.0, 0.0, 10.0))}));
    ASSERT_EQ(attach->GetChildAssemblies().size(), 1u);
    EXPECT_EQ(attach->GetChildAssemblies()[0], extra);

    scene.Clear();
    EXPECT_TRUE(built.root->GetChildAssemblies().empty());
}

TEST(MachineSceneTest, Metallic_CollectsCutterAndShank) {
    const BuiltScene built = MakeScene();
    const auto tool = built.scene.ToolAssembly(1);
    std::size_t expected = 0;
    for (const mc::ToolPart part : {mc::ToolPart::kCutter, mc::ToolPart::kShank}) {
        if (const auto container = mc::FindToolPart(*tool, part)) {
            expected += container->FindEntitiesByType(
                    i_ent::EntityType::kSurfaceOfRevolution, true).size();
        }
    }
    const auto holder = mc::FindToolPart(*tool, mc::ToolPart::kHolder);
    ASSERT_NE(holder, nullptr);
    const auto holder_ids = holder->GetEntityIDs(true);

    const std::vector<igesio::ObjectID> ids = built.scene.MetallicSurfaceIds();
    EXPECT_GT(expected, 0u);
    EXPECT_EQ(ids.size(), expected);
    for (const igesio::ObjectID& id : ids) {
        for (const igesio::ObjectID& excluded : holder_ids) EXPECT_NE(id, excluded);
    }
}

TEST(MachineSceneTest, WorkView_HiddenIdsAndTransform) {
    // テーブル自身の形状を持つ機械で、`show_work_mount_parts`の除外を確かめる
    const std::string machine = Replace(
            MinimalXyzAc(), "[component.frame]\norigin = [0, 0, 0]\n",
            "[component.frame]\norigin = [0, 0, 0]\n\n[[component.geometry]]\n"
            "name = \"table-box\"\nprimitive = \"box\"\nsize = [100, 100, 10]\n");
    BuiltScene built;
    built.scene.Build(MakeSetupWithMachine(machine, "igesio_scene_workview"), built.root);
    mc::MachineScene& scene = built.scene;
    ASSERT_EQ(scene.GeometryCount(), 3u);

    const auto contains = [](const std::vector<igesio::ObjectID>& ids,
                             const igesio::ObjectID& id) {
        for (const auto& item : ids) {
            if (item == id) return true;
        }
        return false;
    };
    const auto table_box = scene.GeometryAssembly(0);
    const auto x_box = scene.GeometryAssembly(1);
    ASSERT_EQ(table_box->Metadata().name, "geometry:table-box");
    const std::vector<igesio::ObjectID> hidden = scene.WorkViewHiddenIds();
    EXPECT_TRUE(contains(hidden, table_box->GetID()));
    EXPECT_TRUE(contains(hidden, x_box->GetID()));
    EXPECT_TRUE(contains(hidden, FindChild(*scene.MachineAssembly(),
                                           mc::kMachineTriadName)->GetID()));
    EXPECT_TRUE(contains(hidden, scene.MachineTraceAssembly()->GetID()));
    EXPECT_FALSE(contains(hidden, scene.ModelAssembly("stock")->GetID()));
    EXPECT_FALSE(contains(hidden, scene.WorkFrameAssembly("G54")->GetID()));
    EXPECT_FALSE(contains(hidden, scene.ToolAssembly(1)->GetID()));

    mc::WorkViewOptions options;
    options.show_work_mount_parts = true;
    const std::vector<igesio::ObjectID> with_table = scene.WorkViewHiddenIds(options);
    EXPECT_FALSE(contains(with_table, table_box->GetID()));
    EXPECT_TRUE(contains(with_table, x_box->GetID()));

    const std::vector<igesio::ObjectID> machine_hidden = scene.MachineViewHiddenIds();
    ASSERT_EQ(machine_hidden.size(), 1u);
    EXPECT_EQ(machine_hidden[0], scene.TrajectoryAssembly()->GetID());

    // `work_mount`のワールド変換は現在の姿勢のF_wm(q)
    const mc::MachineModel& model = scene.Model();
    const mc::JointVector q = mc::JointsFromNc(
            model, mc::NcValues{{"A", ToRadians(20.0)}, {"C", ToRadians(45.0)}},
            mc::InitialJoints(model));
    scene.ApplyPose(q);
    EXPECT_TRUE(scene.WorkMountWorldTransform().isApprox(
            mc::Forward(model, q)[model.WorkMountIndex()], kTol));
}
