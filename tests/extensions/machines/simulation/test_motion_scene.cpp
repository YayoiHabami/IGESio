/**
 * @file tests/extensions/machines/simulation/test_motion_scene.cpp
 * @brief 動作のサンプル列からの表示オブジェクト (simulation/motion_scene) のテスト
 * @author Yayoi Habami
 * @date 2026-09-16
 * @copyright 2026 Yayoi Habami
 * @note 対象: ThinIndices / RebuildToolTrajectory / SetToolTrajectoryVisible /
 *       SetToolTrajectoryHolderVisible / ToolTrajectoryMetallicIds /
 *       RebuildMotionTrace / UpdateCurrentRecord
 *       - 正常系: 間引きの3方式、区間ごとの工具軌跡と同次変換 (work_mount相対),
 *         区間内の工具の切り替え、区間単位の可視性、動作軌跡の点と分割、機械座標の
 *         移動の包含、現在レコードの差し替え、金属材質のID
 *       - 正常系 (境界値・退化): 要素数2以下の間引き、工具なしと工具表に無い番号の
 *         サンプル、動作でないレコードと範囲外のレコードの強調、空のサンプル列
 *       - 異常系: 未構築のシーン (`invalid_argument`)、不正な間引きの指定
 *         (`invalid_argument`)
 * @note フィクスチャは実例機 (動特性あり) と`MinimalProject` (+工具#2).
 *       制御点は高さ200 mmで動かし、工具軸を傾けて回転軸も動かす
 */
#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "igesio/common/id_generator.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/entities/entity_type.h"
#include "igesio/entities/curves/linear_path.h"
#include "igesio/models/assembly.h"
#include "igesio/extensions/inspection/instanced_entity.h"
#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/core/rotation.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/machine/forward_kinematics.h"
#include "igesio/extensions/machines/machine/machine_model.h"
#include "igesio/extensions/machines/project/setup.h"
#include "igesio/extensions/machines/scene/machine_scene.h"
#include "igesio/extensions/machines/scene/tool_trajectory.h"
#include "igesio/extensions/machines/simulation/motion.h"
#include "igesio/extensions/machines/simulation/motion_scene.h"
#include "igesio/extensions/machines/tools/tool_assembly.h"
#include "igesio/extensions/machines/tools/tool_profile.h"
#include "igesio/extensions/machines/toolpath/cl_program.h"
#include "igesio/extensions/machines/toolpath/cl_transform.h"
#include "../project/projects_for_testing.h"
#include "../scene/scene_for_testing.h"
#include "motion_for_testing.h"

namespace {

namespace mc = igesio::extensions::machines;
namespace i_ent = igesio::entities;
namespace i_mod = igesio::models;
namespace i_ins = igesio::extensions::inspection;
using igesio::Matrix4d;
using igesio::ObjectID;
using igesio::Vector3d;
using mc::ToRadians;
using projects_test::MinimalProject;
using motion_test::CountWarnings;
using motion_test::Goto;
using motion_test::MakeSetup;
using motion_test::Plan;
using motion_test::Program;
using motion_test::Words;
using scene_test::BuiltScene;
using scene_test::FindChild;
using scene_test::MakeScene;

/// @brief 数値比較の許容誤差
constexpr double kTol = 1e-9;

/// @brief 工具#2 (スクエアエンドミル) を加えたプロジェクト
std::string TwoToolProject() {
    return MinimalProject()
           + "\n[[tool]]\nnumber = 2\n\n[tool.simple]\ncutter = \"square\"\n"
             "diameter = 8.0\ncutting_length = 15.0\ntool_length = 50.0\n"
             "overhang = 30.0\n\n[tool.simple.holder]\ndiameter = 30.0\n"
             "length = 40.0\n";
}

/// @brief 20°傾けた工具軸 (A軸が動く)
Vector3d TiltedAxis() {
    return Vector3d(0.0, std::sin(ToRadians(20.0)), std::cos(ToRadians(20.0)));
}

/// @brief 高さ200 mmの制御点への移動
mc::ClGoto MoveTo(const double x, const double y,
                  const mc::MotionKind kind = mc::MotionKind::kLinear,
                  const std::optional<Vector3d>& axis = std::nullopt) {
    return Goto(Vector3d(x, y, 200.0), axis, kind);
}

/// @brief 早送り2 → 切削3 → 早送り1 (区間3つ) のプログラム
/// @note 動作レコードのインデックスは1〜6. 切削では工具軸を傾ける
mc::ClProgram ThreeRunProgram() {
    return Program({mc::ClLoadTool{1},
                    MoveTo(0.0, 0.0, mc::MotionKind::kRapid),
                    MoveTo(10.0, 0.0, mc::MotionKind::kRapid),
                    MoveTo(20.0, 0.0, mc::MotionKind::kLinear, TiltedAxis()),
                    MoveTo(20.0, 10.0),
                    MoveTo(30.0, 10.0),
                    MoveTo(30.0, 50.0, mc::MotionKind::kRapid)});
}

/// @brief 工具軌跡の区間`run<k>`のアセンブリ
std::shared_ptr<i_mod::Assembly> RunOf(const mc::MachineScene& scene,
                                       const std::size_t k) {
    return FindChild(*scene.TrajectoryAssembly(),
                     std::string(mc::kTrajectoryRunPrefix) + std::to_string(k));
}

/// @brief 区間の工具グループの本体 (切れ刃部+シャンク部の複製)
std::shared_ptr<i_ins::InstancedEntity> BodyOf(const i_mod::Assembly& group) {
    const auto ids = group.GetEntityIDs(false);
    if (ids.size() != 1) return nullptr;
    return group.GetEntityAs<i_ins::InstancedEntity>(ids[0]);
}

/// @brief 工具軌跡の期待する同次変換 F_wm(q)⁻¹·F_tm(q)·H_tm·T(0,0,-ゲージ長)
Matrix4d ExpectedPlacement(const mc::MachineScene& scene, const mc::JointVector& q,
                           const int tool_number) {
    const mc::MachineModel& model = scene.Model();
    const std::vector<Matrix4d> frames = mc::Forward(model, q);
    return mc::RigidInverse(frames[model.WorkMountIndex()])
           * frames[model.ToolMountIndex()]
           * model.MountPlacement(mc::MountKind::kToolMount)
           * mc::ToolMountOffset(scene.Tools().at(tool_number));
}

/// @brief 動作軌跡の期待する点 (機械座標) F_tm(q)·H_tm·(0,0,command_point_z-ゲージ長)
Vector3d ExpectedTracePoint(const mc::MachineScene& scene, const mc::JointVector& q,
                            const int tool_number) {
    const mc::MachineModel& model = scene.Model();
    const mc::ToolProfile& profile = scene.Tools().at(tool_number).profile;
    const Vector3d home = mc::ApplyPoint(
            model.MountPlacement(mc::MountKind::kToolMount),
            Vector3d(0.0, 0.0, profile.command_point_z - profile.GaugeLength(nullptr)));
    return mc::ApplyPoint(mc::Forward(model, q)[model.ToolMountIndex()], home);
}

/// @brief 直接所有する3次元折れ線 (Type 106) を集める
std::vector<std::shared_ptr<i_ent::LinearPath>> LinearPathsOf(
        const i_mod::Assembly& node) {
    std::vector<std::shared_ptr<i_ent::LinearPath>> paths;
    for (const auto& entity : node.FindEntitiesByType(i_ent::EntityType::kCopiousData)) {
        paths.push_back(std::dynamic_pointer_cast<i_ent::LinearPath>(entity));
    }
    return paths;
}

/// @brief 折れ線が指定した点を頂点に持つか
bool HasVertex(const i_ent::LinearPath& path, const Vector3d& point) {
    for (std::size_t i = 0; i < path.GetCount(); ++i) {
        if (path.Coordinate(i).isApprox(point, 1e-6)) return true;
    }
    return false;
}

/// @brief レコードの終点のサンプルのインデックスを索引順に集める
std::vector<std::size_t> CommandSampleIndices(const mc::MotionTrack& track,
                                              const std::size_t begin,
                                              const std::size_t end) {
    std::vector<std::size_t> indices;
    for (std::size_t i = 0; i < track.samples.size(); ++i) {
        const mc::MotionSample& sample = track.samples[i];
        if (sample.is_command_point && sample.record_index >= begin
            && sample.record_index < end) {
            indices.push_back(i);
        }
    }
    return indices;
}

}  // namespace



/**
 * ---- 間引き ----
 */

TEST(MotionSceneTest, ThinIndices_Modes) {
    const std::vector<std::size_t> all = {0, 1, 2, 3, 4};
    EXPECT_EQ(mc::ThinIndices(5, mc::Thinning{}), all);

    mc::Thinning rate;
    rate.mode = mc::ThinningMode::kRate;
    rate.rate = 0.5;
    EXPECT_EQ(mc::ThinIndices(5, rate), (std::vector<std::size_t>{0, 2, 4}));
    rate.rate = 1.0;
    EXPECT_EQ(mc::ThinIndices(5, rate), all);

    mc::Thinning count;
    count.mode = mc::ThinningMode::kMaxCount;
    count.max_count = 3;
    EXPECT_EQ(mc::ThinIndices(9, count), (std::vector<std::size_t>{0, 4, 8}));
    count.max_count = 5;
    EXPECT_EQ(mc::ThinIndices(5, count), all);
    count.max_count = 1;
    EXPECT_EQ(mc::ThinIndices(5, count), (std::vector<std::size_t>{0, 4}));

    // 要素数が2以下なら間引かない
    rate.rate = 0.1;
    EXPECT_EQ(mc::ThinIndices(2, rate), (std::vector<std::size_t>{0, 1}));
    EXPECT_EQ(mc::ThinIndices(0, count), std::vector<std::size_t>{});
}

TEST(MotionSceneTest, ThinIndices_ThrowsInvalidArgumentWhenSpecIsNotPositive) {
    mc::Thinning rate;
    rate.mode = mc::ThinningMode::kRate;
    rate.rate = 0.0;
    EXPECT_THROW(mc::ThinIndices(5, rate), std::invalid_argument);
    rate.rate = 1e-3;
    EXPECT_NO_THROW(mc::ThinIndices(5, rate));

    mc::Thinning count;
    count.mode = mc::ThinningMode::kMaxCount;
    count.max_count = 0;
    EXPECT_THROW(mc::ThinIndices(5, count), std::invalid_argument);
    count.max_count = 1;
    EXPECT_NO_THROW(mc::ThinIndices(5, count));
}



/**
 * ---- 工具軌跡 ----
 */

TEST(MotionSceneTest, Trajectory_GroupsByPathRange) {
    const mc::MachiningSetup setup = MakeSetup();
    BuiltScene built = MakeScene(setup);
    const mc::ClProgram program = ThreeRunProgram();
    const mc::MotionTrack track = Plan(setup, program);
    std::vector<mc::Diagnostic> warnings;
    mc::RebuildToolTrajectory(built.scene, track, program, {}, &warnings);
    EXPECT_TRUE(warnings.empty());

    const std::vector<mc::ClPathRange> ranges = mc::EnumeratePaths(program);
    ASSERT_EQ(ranges.size(), 3u);
    ASSERT_EQ(built.scene.TrajectoryAssembly()->GetChildAssemblies().size(), 3u);
    for (std::size_t k = 0; k < ranges.size(); ++k) {
        const auto run = RunOf(built.scene, k);
        ASSERT_NE(run, nullptr) << k;
        ASSERT_EQ(run->GetChildAssemblies().size(), 1u) << k;
        const auto group = run->GetChildAssemblies()[0];
        EXPECT_EQ(group->Metadata().name, "tool1");
        const auto body = BodyOf(*group);
        ASSERT_NE(body, nullptr) << k;
        EXPECT_EQ(body->InstanceCount(),
                  CommandSampleIndices(track, ranges[k].begin, ranges[k].end).size());
        // ホルダ部は子`holder`に置かれ、既定で表示する
        const auto holder = FindChild(*group, mc::kTrajectoryHolderName);
        ASSERT_NE(holder, nullptr);
        EXPECT_TRUE(holder->Display().visible);
    }
}

TEST(MotionSceneTest, Trajectory_PlacementIsWorkRelative) {
    const mc::MachiningSetup setup = MakeSetup();
    BuiltScene built = MakeScene(setup);
    mc::MachineScene& scene = built.scene;
    const mc::ClProgram program = ThreeRunProgram();
    const mc::MotionTrack track = Plan(setup, program);
    mc::RebuildToolTrajectory(scene, track, program);

    // 区間1 (切削3点. 工具軸が傾いて回転軸が動く) の同次変換
    const std::vector<mc::ClPathRange> ranges = mc::EnumeratePaths(program);
    const std::vector<std::size_t> samples =
            CommandSampleIndices(track, ranges[1].begin, ranges[1].end);
    const auto body = BodyOf(*RunOf(scene, 1)->GetChildAssemblies()[0]);
    ASSERT_EQ(body->InstanceCount(), samples.size());
    for (std::size_t j = 0; j < samples.size(); ++j) {
        const mc::JointVector& q = track.samples[samples[j]].q;
        EXPECT_TRUE(body->Transforms()[j].isApprox(ExpectedPlacement(scene, q, 1), kTol))
                << j;

        // `work_mount`の子として置いたときのワールド変換が同時刻の工具と一致する
        scene.ApplyPose(q);
        const Matrix4d world = scene.WorkMountAssembly()->GetWorldTransform()
                               * body->Transforms()[j];
        EXPECT_TRUE(world.isApprox(scene.ToolAssembly(1)->GetWorldTransform(), 1e-6))
                << j;
    }
}

TEST(MotionSceneTest, Trajectory_ThinningKeepsEnds) {
    const mc::MachiningSetup setup = MakeSetup();
    BuiltScene built = MakeScene(setup);
    // 切削5点の1区間
    const mc::ClProgram program = Program({
            mc::ClLoadTool{1}, MoveTo(0.0, 0.0), MoveTo(10.0, 0.0), MoveTo(20.0, 0.0),
            MoveTo(30.0, 0.0), MoveTo(40.0, 0.0)});
    const mc::MotionTrack track = Plan(setup, program);
    mc::RebuildToolTrajectory(built.scene, track, program);
    const std::vector<Matrix4d> full =
            BodyOf(*RunOf(built.scene, 0)->GetChildAssemblies()[0])->Transforms();
    ASSERT_EQ(full.size(), 5u);

    mc::ToolTrajectoryOptions options;
    options.thinning.mode = mc::ThinningMode::kMaxCount;
    options.thinning.max_count = 3;
    mc::RebuildToolTrajectory(built.scene, track, program, options);
    const std::vector<Matrix4d> thinned =
            BodyOf(*RunOf(built.scene, 0)->GetChildAssemblies()[0])->Transforms();
    ASSERT_EQ(thinned.size(), 3u);
    EXPECT_TRUE(thinned.front().isApprox(full.front(), kTol));
    EXPECT_TRUE(thinned.back().isApprox(full.back(), kTol));

    // 補間点を含めると全サンプルが対象になり、除外したサンプルは省かれる
    mc::ToolTrajectoryOptions interpolated;
    interpolated.command_points_only = false;
    interpolated.skip_sample = 3;
    mc::RebuildToolTrajectory(built.scene, track, program, interpolated);
    EXPECT_EQ(BodyOf(*RunOf(built.scene, 0)->GetChildAssemblies()[0])->InstanceCount(),
              track.samples.size() - 1);
}

TEST(MotionSceneTest, Trajectory_ToolChangeSplitsRun) {
    const mc::MachiningSetup setup = MakeSetup(TwoToolProject());
    BuiltScene built = MakeScene(setup);
    // 区切りで1区間にまとめ、区間内で工具1 → 2 → 工具なし → 工具表に無い9と変える
    const mc::ClProgram program = Program({
            mc::ClMarker{mc::ClMarker::Kind::kPathBegin, "p"},
            mc::ClLoadTool{1}, MoveTo(0.0, 0.0), MoveTo(10.0, 0.0),
            mc::ClLoadTool{2}, MoveTo(20.0, 0.0),
            mc::ClLoadTool{mc::kNoTool}, MoveTo(30.0, 0.0),
            mc::ClLoadTool{9}, MoveTo(40.0, 0.0),
            mc::ClMarker{mc::ClMarker::Kind::kPathEnd, "p"}});
    const mc::MotionTrack track = Plan(setup, program);
    std::vector<mc::Diagnostic> warnings;
    mc::RebuildToolTrajectory(built.scene, track, program, {}, &warnings);

    ASSERT_EQ(built.scene.TrajectoryAssembly()->GetChildAssemblies().size(), 1u);
    const auto run = RunOf(built.scene, 0);
    ASSERT_NE(run, nullptr);
    ASSERT_EQ(run->GetChildAssemblies().size(), 2u);
    EXPECT_EQ(run->GetChildAssemblies()[0]->Metadata().name, "tool1");
    EXPECT_EQ(BodyOf(*run->GetChildAssemblies()[0])->InstanceCount(), 2u);
    EXPECT_EQ(run->GetChildAssemblies()[1]->Metadata().name, "tool2");
    EXPECT_EQ(BodyOf(*run->GetChildAssemblies()[1])->InstanceCount(), 1u);
    EXPECT_EQ(CountWarnings(warnings, "no tool is selected"), 1u);
    EXPECT_EQ(CountWarnings(warnings, "T9"), 1u);
}

TEST(MotionSceneTest, Trajectory_VisibilityAcrossRuns) {
    const mc::MachiningSetup setup = MakeSetup();
    BuiltScene built = MakeScene(setup);
    mc::MachineScene& scene = built.scene;
    const mc::ClProgram program = ThreeRunProgram();
    mc::RebuildToolTrajectory(scene, Plan(setup, program), program);
    const auto visible = [&scene](const std::size_t k) {
        return RunOf(scene, k)->Display().visible;
    };

    mc::TrajectoryVisibility only;
    only.only_run = 1;
    mc::SetToolTrajectoryVisible(scene, only);
    EXPECT_FALSE(visible(0));
    EXPECT_TRUE(visible(1));
    EXPECT_FALSE(visible(2));

    // 区間の間引きは両端を残し、`always_visible_run`は間引きの対象でも表示する
    mc::TrajectoryVisibility thinned;
    thinned.thinning.mode = mc::ThinningMode::kMaxCount;
    thinned.thinning.max_count = 2;
    mc::SetToolTrajectoryVisible(scene, thinned);
    EXPECT_TRUE(visible(0));
    EXPECT_FALSE(visible(1));
    EXPECT_TRUE(visible(2));
    thinned.always_visible_run = 1;
    mc::SetToolTrajectoryVisible(scene, thinned);
    EXPECT_TRUE(visible(1));

    mc::TrajectoryVisibility hidden;
    hidden.show = false;
    mc::SetToolTrajectoryVisible(scene, hidden);
    EXPECT_FALSE(visible(0));
    EXPECT_FALSE(visible(1));
    EXPECT_FALSE(visible(2));

    // ホルダ部の可視性は全区間に及ぶ
    mc::SetToolTrajectoryHolderVisible(scene, false);
    for (std::size_t k = 0; k < 3; ++k) {
        const auto holder = FindChild(*RunOf(scene, k)->GetChildAssemblies()[0],
                                      mc::kTrajectoryHolderName);
        EXPECT_FALSE(holder->Display().visible) << k;
    }
}

TEST(MotionSceneTest, Metallic_CollectsBodies) {
    const mc::MachiningSetup setup = MakeSetup();
    BuiltScene built = MakeScene(setup);
    EXPECT_TRUE(mc::ToolTrajectoryMetallicIds(built.scene).empty());

    const mc::ClProgram program = ThreeRunProgram();
    mc::RebuildToolTrajectory(built.scene, Plan(setup, program), program);
    const std::vector<ObjectID> ids = mc::ToolTrajectoryMetallicIds(built.scene);
    ASSERT_EQ(ids.size(), 3u);
    for (std::size_t k = 0; k < 3; ++k) {
        const auto group = RunOf(built.scene, k)->GetChildAssemblies()[0];
        EXPECT_EQ(ids[k], BodyOf(*group)->GetID());
    }
}



/**
 * ---- 動作軌跡 ----
 */

TEST(MotionSceneTest, Trace_MachineAndWork) {
    const mc::MachiningSetup setup = MakeSetup();
    BuiltScene built = MakeScene(setup);
    mc::MachineScene& scene = built.scene;
    // 早送り1 → 切削2 (工具軸を傾ける)
    const mc::ClProgram program = Program({
            mc::ClLoadTool{1}, MoveTo(0.0, 0.0, mc::MotionKind::kRapid),
            MoveTo(20.0, 0.0, mc::MotionKind::kLinear, TiltedAxis()),
            MoveTo(20.0, 10.0)});
    const mc::MotionTrack track = Plan(setup, program);
    mc::RebuildMotionTrace(scene, track);

    const auto machine_trace = scene.MachineTraceAssembly();
    const auto work_trace = scene.WorkTraceAssembly();
    const auto machine_rapid =
            LinearPathsOf(*FindChild(*machine_trace, mc::kRapidPathsName));
    const auto machine_cut = LinearPathsOf(*FindChild(*machine_trace, mc::kCutPathsName));
    ASSERT_EQ(machine_rapid.size(), 1u);
    ASSERT_EQ(machine_cut.size(), 1u);
    const auto current = FindChild(*machine_trace, mc::kCurrentRecordName);
    ASSERT_NE(current, nullptr);
    EXPECT_FALSE(current->Display().visible);

    // 早送りの折れ線は初期姿勢からレコード1の終点まで、切削はその終点から末尾まで
    const std::size_t first_cut = track.record_first_sample[2];
    EXPECT_EQ(machine_rapid[0]->GetCount(), first_cut);
    EXPECT_EQ(machine_cut[0]->GetCount(), track.samples.size() - first_cut + 1);
    for (std::size_t i = 0; i < track.samples.size(); ++i) {
        const Vector3d expected = ExpectedTracePoint(scene, track.samples[i].q, 1);
        const auto& path = i < first_cut ? machine_rapid[0] : machine_cut[0];
        const std::size_t offset = i < first_cut ? 0 : first_cut - 1;
        EXPECT_TRUE(path->Coordinate(i - offset).isApprox(expected, 1e-6)) << i;
    }

    // `work_mount`座標の折れ線はF_wm(q_i)⁻¹を掛けた点
    const auto work_cut = LinearPathsOf(*FindChild(*work_trace, mc::kCutPathsName));
    ASSERT_EQ(work_cut.size(), 1u);
    const mc::MachineModel& model = scene.Model();
    for (std::size_t i = first_cut; i < track.samples.size(); ++i) {
        const mc::JointVector& q = track.samples[i].q;
        const Vector3d expected = mc::ApplyPoint(
                mc::RigidInverse(mc::Forward(model, q)[model.WorkMountIndex()]),
                ExpectedTracePoint(scene, q, 1));
        EXPECT_TRUE(work_cut[0]->Coordinate(i - first_cut + 1).isApprox(expected, 1e-6))
                << i;
    }

    // 片側だけを作る設定では他方の置き場が空になる
    mc::MotionTraceOptions options;
    options.machine_frame = false;
    mc::RebuildMotionTrace(scene, track, options);
    EXPECT_TRUE(machine_trace->GetChildAssemblies().empty());
    EXPECT_EQ(work_trace->GetChildAssemblies().size(), 3u);
}

TEST(MotionSceneTest, Trace_IncludesMachineFrameMoves) {
    const mc::MachiningSetup setup = MakeSetup();
    BuiltScene built = MakeScene(setup);
    const mc::ClProgram program = Program({
            mc::ClLoadTool{1}, MoveTo(0.0, 0.0, mc::MotionKind::kRapid),
            Words(mc::NcValues{{"Z", 50.0}}, mc::MotionFrame::kMachine,
                  mc::MotionKind::kRapid),
            MoveTo(20.0, 0.0)});
    const mc::MotionTrack track = Plan(setup, program);
    built.scene.RebuildPaths(program);
    mc::RebuildMotionTrace(built.scene, track);

    // 経路線には機械座標の移動が無く、動作軌跡には含まれる
    const auto paths = built.scene.PathsAssembly("G54");
    EXPECT_TRUE(LinearPathsOf(*FindChild(*paths, mc::kRapidPathsName)).empty());
    const auto rapid = LinearPathsOf(*FindChild(*built.scene.MachineTraceAssembly(),
                                                mc::kRapidPathsName));
    ASSERT_EQ(rapid.size(), 1u);
    const std::size_t machine_move = track.record_first_sample[2];
    const mc::MotionSample& sample = track.samples[track.record_first_sample[3] - 1];
    ASSERT_EQ(sample.record_index, 2u);
    EXPECT_GT(track.record_first_sample[3], machine_move);
    EXPECT_TRUE(HasVertex(*rapid[0], ExpectedTracePoint(built.scene, sample.q, 1)));
}

TEST(MotionSceneTest, CurrentRecord_Updates) {
    const mc::MachiningSetup setup = MakeSetup();
    BuiltScene built = MakeScene(setup);
    mc::MachineScene& scene = built.scene;
    const mc::ClProgram program = ThreeRunProgram();
    const mc::MotionTrack track = Plan(setup, program);
    mc::RebuildMotionTrace(scene, track);
    const auto current = FindChild(*scene.MachineTraceAssembly(), mc::kCurrentRecordName);
    const auto work_current =
            FindChild(*scene.WorkTraceAssembly(), mc::kCurrentRecordName);

    // レコード4のサンプル列に直前のサンプルを加えた折れ線
    mc::UpdateCurrentRecord(scene, track, 4);
    EXPECT_TRUE(current->Display().visible);
    EXPECT_TRUE(work_current->Display().visible);
    const std::size_t first = track.record_first_sample[4];
    const std::size_t end = track.record_first_sample[5];
    auto paths = LinearPathsOf(*current);
    ASSERT_EQ(paths.size(), 1u);
    ASSERT_EQ(paths[0]->GetCount(), end - first + 1);
    for (std::size_t i = first - 1; i < end; ++i) {
        EXPECT_TRUE(paths[0]->Coordinate(i - first + 1).isApprox(
                ExpectedTracePoint(scene, track.samples[i].q, 1), 1e-6)) << i;
    }

    // 別のレコードに差し替えると古い折れ線は残らない
    mc::UpdateCurrentRecord(scene, track, 5);
    paths = LinearPathsOf(*current);
    ASSERT_EQ(paths.size(), 1u);
    EXPECT_EQ(paths[0]->GetCount(),
              track.record_first_sample[6] - track.record_first_sample[5] + 1);

    // 動作でないレコードと範囲外のレコードでは非表示にする
    mc::UpdateCurrentRecord(scene, track, 0);
    EXPECT_FALSE(current->Display().visible);
    EXPECT_TRUE(LinearPathsOf(*current).empty());
    mc::UpdateCurrentRecord(scene, track, 99);
    EXPECT_FALSE(current->Display().visible);
}

TEST(MotionSceneTest, Trace_EmptyTrackClearsPlaces) {
    const mc::MachiningSetup setup = MakeSetup();
    BuiltScene built = MakeScene(setup);
    const mc::ClProgram program = ThreeRunProgram();
    mc::RebuildMotionTrace(built.scene, Plan(setup, program));
    ASSERT_FALSE(built.scene.MachineTraceAssembly()->GetChildAssemblies().empty());

    const mc::MotionTrack empty = Plan(setup, Program({mc::ClLoadTool{1}}));
    ASSERT_TRUE(empty.samples.empty());
    mc::RebuildMotionTrace(built.scene, empty);
    EXPECT_TRUE(built.scene.MachineTraceAssembly()->GetChildAssemblies().empty());
    EXPECT_TRUE(built.scene.WorkTraceAssembly()->GetChildAssemblies().empty());
    EXPECT_NO_THROW(mc::UpdateCurrentRecord(built.scene, empty, 0));
}



/**
 * ---- 異常系 ----
 */

TEST(MotionSceneTest, Rebuild_ThrowsInvalidArgumentWhenNotBuilt) {
    const mc::MachiningSetup setup = MakeSetup();
    const mc::ClProgram program = ThreeRunProgram();
    const mc::MotionTrack track = Plan(setup, program);
    mc::MachineScene scene;
    EXPECT_THROW(mc::RebuildToolTrajectory(scene, track, program), std::invalid_argument);
    EXPECT_THROW(mc::RebuildMotionTrace(scene, track), std::invalid_argument);
    EXPECT_THROW(mc::UpdateCurrentRecord(scene, track, 1), std::invalid_argument);
    EXPECT_NO_THROW(mc::SetToolTrajectoryVisible(scene, {}));
    EXPECT_NO_THROW(mc::SetToolTrajectoryHolderVisible(scene, false));
    EXPECT_TRUE(mc::ToolTrajectoryMetallicIds(scene).empty());
}

TEST(MotionSceneTest, Rebuild_ThrowsInvalidArgumentWhenThinningIsInvalid) {
    const mc::MachiningSetup setup = MakeSetup();
    BuiltScene built = MakeScene(setup);
    const mc::ClProgram program = ThreeRunProgram();
    const mc::MotionTrack track = Plan(setup, program);
    mc::ToolTrajectoryOptions options;
    options.thinning.mode = mc::ThinningMode::kRate;
    options.thinning.rate = 0.0;
    EXPECT_THROW(mc::RebuildToolTrajectory(built.scene, track, program, options),
                 std::invalid_argument);

    mc::TrajectoryVisibility visibility;
    visibility.thinning.mode = mc::ThinningMode::kMaxCount;
    visibility.thinning.max_count = 0;
    EXPECT_THROW(mc::SetToolTrajectoryVisible(built.scene, visibility),
                 std::invalid_argument);
}
