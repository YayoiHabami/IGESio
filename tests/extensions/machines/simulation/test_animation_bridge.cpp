/**
 * @file tests/extensions/machines/simulation/test_animation_bridge.cpp
 * @brief アニメーションクリップの生成 (simulation/animation_bridge) のテスト
 * @author Yayoi Habami
 * @date 2026-09-16
 * @copyright 2026 Yayoi Habami
 * @note 対象: MakeMachineClip
 *       - 正常系: 変換キーの間引き (動かないコンポーネントにトラックが無い、動く区間
 *         だけにキー)、工具ごとの可視性キー、イベント (`"tool"`/`"record"`/
 *         `"program"`)、`AnimationPlayer`への結び付けと再生の一致、後方への移動と
 *         停止での復元、総時間
 *       - 正常系 (境界値・退化): 同時刻のサンプル (所要時間0) の合流、末尾の
 *         ドウェルで延びる総時間
 *       - 異常系: 未構築のシーンと空のサンプル列 (`invalid_argument`)
 * @note フィクスチャは実例機 (動特性あり. 形状ファイルは無いため警告のみ) と
 *       `MinimalProject` (+工具#2). 制御点は工具軸+Zのまま高さ200 mmで動かす
 */
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "igesio/common/id_generator.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/models/assembly.h"
#include "igesio/extensions/animation/animation_clip.h"
#include "igesio/extensions/animation/animation_player.h"
#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/machine/forward_kinematics.h"
#include "igesio/extensions/machines/machine/machine_model.h"
#include "igesio/extensions/machines/project/setup.h"
#include "igesio/extensions/machines/scene/machine_scene.h"
#include "igesio/extensions/machines/simulation/animation_bridge.h"
#include "igesio/extensions/machines/simulation/motion.h"
#include "igesio/extensions/machines/toolpath/cl_program.h"
#include "../project/projects_for_testing.h"
#include "../scene/scene_for_testing.h"
#include "motion_for_testing.h"

namespace {

namespace mc = igesio::extensions::machines;
namespace anim = igesio::extensions::animation;
using igesio::Matrix4d;
using igesio::ObjectID;
using igesio::Vector3d;
using projects_test::MinimalProject;
using motion_test::CountWarnings;
using motion_test::Goto;
using motion_test::MakeSetup;
using motion_test::MakeSetupWithoutDynamics;
using motion_test::Plan;
using motion_test::Program;
using scene_test::BuiltScene;
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

/// @brief 高さ200 mm (可動範囲内) の早送りの制御点
mc::ClGoto RapidTo(const double x, const double y) {
    return Goto(Vector3d(x, y, 200.0), std::nullopt, mc::MotionKind::kRapid);
}

/// @brief 工具1 → 2 → 工具なし → 工具表に無い3の順に動くプログラム
/// @note 動作レコードのインデックスは1, 3, 5, 7. レコード4以降は`program_index = 1`
mc::ClProgram ToolChangeProgram() {
    mc::ClProgram program = Program({
            mc::ClLoadTool{1}, RapidTo(0.0, 0.0),
            mc::ClLoadTool{2}, RapidTo(50.0, 0.0),
            mc::ClLoadTool{mc::kNoTool}, RapidTo(50.0, 50.0),
            mc::ClLoadTool{3}, RapidTo(0.0, 50.0)});
    for (std::size_t i = 4; i < program.sources.size(); ++i) {
        program.sources[i].program_index = 1;
    }
    return program;
}

/// @brief レコードの最初のサンプルの時刻
double FirstTime(const mc::MotionTrack& track, const std::size_t record_index) {
    return track.samples[track.record_first_sample[record_index]].time;
}

/// @brief 対象のIDの変換トラックを探す
const anim::AnimationTrack* FindTrack(const anim::AnimationClip& clip,
                                      const ObjectID& target) {
    for (const anim::AnimationTrack& track : clip.Tracks()) {
        if (track.target == target) return &track;
    }
    return nullptr;
}

/// @brief 対象のIDの可視性トラックのキーを (時刻, 可視性) の列にする
std::vector<std::pair<double, bool>> VisibilityKeys(const anim::AnimationClip& clip,
                                                    const ObjectID& target) {
    std::vector<std::pair<double, bool>> keys;
    for (const anim::VisibilityTrack& track : clip.VisibilityTracks()) {
        if (track.target != target) continue;
        for (const anim::VisibilityKeyframe& key : track.keys) {
            keys.emplace_back(key.time_sec, key.visible);
        }
    }
    return keys;
}

/// @brief 名前のイベントトラックのキーを (時刻, 値) の列にする
std::vector<std::pair<double, std::int64_t>> EventKeys(const anim::AnimationClip& clip,
                                                       const std::string& name) {
    std::vector<std::pair<double, std::int64_t>> keys;
    if (const anim::EventTrack* track = clip.FindEventTrack(name)) {
        for (const anim::EventKey& key : track->keys) {
            keys.emplace_back(key.time_sec, key.value);
        }
    }
    return keys;
}

/// @brief コンポーネントのアセンブリのID
ObjectID ComponentId(const mc::MachineScene& scene, const std::string& name) {
    return scene.ComponentAssembly(name)->GetID();
}

/// @brief クリップの生成前と結び付けの前に基準状態 (ゼロポーズ・初期工具) にする
void ResetToBase(mc::MachineScene& scene, const int initial_tool) {
    scene.ResetToZeroPose();
    scene.SetActiveTool(initial_tool);
}

}  // namespace



/**
 * ---- トラックの生成 ----
 */

TEST(AnimationBridgeTest, Transform_KeysAreDeduplicated) {
    const mc::MachiningSetup setup = MakeSetup();
    BuiltScene built = MakeScene(setup);
    mc::MachineScene& scene = built.scene;
    // レコード1でYとZが動き、レコード2でXだけが動く
    const mc::ClProgram program = Program({
            mc::ClLoadTool{1}, RapidTo(0.0, 0.0), Goto(Vector3d(50.0, 0.0, 200.0))});
    const mc::MotionTrack track = Plan(setup, program);
    ResetToBase(scene, 1);
    const anim::AnimationClip clip = mc::MakeMachineClip(scene, track, program);

    // 動かないコンポーネントにはトラックが無い
    for (const std::string name : {"base", "cradle-frame", "A", "C", "Attach"}) {
        EXPECT_EQ(FindTrack(clip, ComponentId(scene, name)), nullptr) << name;
    }
    const anim::AnimationTrack* y_track = FindTrack(clip, ComponentId(scene, "Y"));
    const anim::AnimationTrack* x_track = FindTrack(clip, ComponentId(scene, "X"));
    ASSERT_NE(y_track, nullptr);
    ASSERT_NE(x_track, nullptr);

    // Xはレコード2の区間でだけ動く. Y (Xの子) はレコード1でも動くのでキーを持つ
    const double t2 = FirstTime(track, 2);
    ASSERT_FALSE(x_track->keys.empty());
    ASSERT_FALSE(y_track->keys.empty());
    for (const anim::TransformKeyframe& key : x_track->keys) EXPECT_GE(key.time_sec, t2);
    EXPECT_LE(y_track->keys.front().time_sec, t2);

    // 間引かなければ全コンポーネントが全サンプル時刻にキーを持つ
    mc::ClipBuildOptions options;
    options.skip_unchanged = false;
    const anim::AnimationClip full = mc::MakeMachineClip(scene, track, program, options);
    EXPECT_EQ(full.Tracks().size(), scene.Model().ComponentCount());
    const anim::AnimationTrack* base_track = FindTrack(full, ComponentId(scene, "base"));
    ASSERT_NE(base_track, nullptr);
    EXPECT_EQ(base_track->keys.size(), track.samples.size());
}

TEST(AnimationBridgeTest, Visibility_PerTool) {
    const mc::MachiningSetup setup = MakeSetup(TwoToolProject());
    BuiltScene built = MakeScene(setup);
    mc::MachineScene& scene = built.scene;
    const mc::ClProgram program = ToolChangeProgram();
    const mc::MotionTrack track = Plan(setup, program);
    ResetToBase(scene, 1);
    std::vector<mc::Diagnostic> warnings;
    const anim::AnimationClip clip =
            mc::MakeMachineClip(scene, track, program, {}, &warnings);

    const auto ids = scene.ToolAssemblyIds();
    const double t3 = FirstTime(track, 3);
    const double t5 = FirstTime(track, 5);
    const auto tool1 = VisibilityKeys(clip, ids.at(1));
    ASSERT_EQ(tool1.size(), 2u);
    EXPECT_NEAR(tool1[0].first, 0.0, kTol);
    EXPECT_TRUE(tool1[0].second);
    EXPECT_NEAR(tool1[1].first, t3, kTol);
    EXPECT_FALSE(tool1[1].second);

    const auto tool2 = VisibilityKeys(clip, ids.at(2));
    ASSERT_EQ(tool2.size(), 3u);
    EXPECT_NEAR(tool2[0].first, 0.0, kTol);
    EXPECT_FALSE(tool2[0].second);
    EXPECT_NEAR(tool2[1].first, t3, kTol);
    EXPECT_TRUE(tool2[1].second);
    EXPECT_NEAR(tool2[2].first, t5, kTol);
    EXPECT_FALSE(tool2[2].second);

    // 工具表に無い番号は1回だけ警告する
    EXPECT_EQ(CountWarnings(warnings, "T3"), 1u);
}

TEST(AnimationBridgeTest, Events_ToolRecordProgram) {
    const mc::MachiningSetup setup = MakeSetup(TwoToolProject());
    BuiltScene built = MakeScene(setup);
    const mc::ClProgram program = ToolChangeProgram();
    const mc::MotionTrack track = Plan(setup, program);
    ResetToBase(built.scene, 1);
    const anim::AnimationClip clip = mc::MakeMachineClip(built.scene, track, program);

    const std::vector<std::pair<double, std::int64_t>> expected_tools = {
            {0.0, 1}, {FirstTime(track, 3), 2}, {FirstTime(track, 5), 0},
            {FirstTime(track, 7), 3}};
    const auto tools = EventKeys(clip, std::string(mc::kToolEventTrack));
    ASSERT_EQ(tools.size(), expected_tools.size());
    for (std::size_t i = 0; i < tools.size(); ++i) {
        EXPECT_NEAR(tools[i].first, expected_tools[i].first, kTol) << i;
        EXPECT_EQ(tools[i].second, expected_tools[i].second) << i;
    }

    // "record"は動作レコードの最初のサンプル時刻にそのインデックス
    const auto records = EventKeys(clip, std::string(mc::kRecordEventTrack));
    ASSERT_EQ(records.size(), 4u);
    const std::int64_t expected_records[4] = {1, 3, 5, 7};
    for (std::size_t i = 0; i < 4; ++i) {
        EXPECT_NEAR(records[i].first, FirstTime(track, expected_records[i]), kTol) << i;
        EXPECT_EQ(records[i].second, expected_records[i]) << i;
    }

    // "program"は`program_index`の変化時
    const auto programs = EventKeys(clip, std::string(mc::kProgramEventTrack));
    ASSERT_EQ(programs.size(), 2u);
    EXPECT_EQ(programs[0].second, 0);
    EXPECT_NEAR(programs[1].first, FirstTime(track, 5), kTol);
    EXPECT_EQ(programs[1].second, 1);

    // "record"を作らない設定と、`sources`の無いプログラム
    mc::ClipBuildOptions options;
    options.emit_record_events = false;
    mc::ClProgram without_sources = program;
    without_sources.sources.clear();
    const anim::AnimationClip sparse =
            mc::MakeMachineClip(built.scene, track, without_sources, options);
    EXPECT_EQ(sparse.FindEventTrack(std::string(mc::kRecordEventTrack)), nullptr);
    EXPECT_EQ(sparse.FindEventTrack(std::string(mc::kProgramEventTrack)), nullptr);
    EXPECT_NE(sparse.FindEventTrack(std::string(mc::kToolEventTrack)), nullptr);
}

TEST(AnimationBridgeTest, Duration_MatchesStats) {
    const mc::MachiningSetup setup = MakeSetup();
    BuiltScene built = MakeScene(setup);
    // 末尾のドウェルは姿勢を変えないため変換キーは増えないが、総時間は延びる
    const mc::ClProgram program = Program({
            mc::ClLoadTool{1}, RapidTo(0.0, 0.0), RapidTo(50.0, 0.0),
            mc::ClDwell{1.0}});
    const mc::MotionTrack track = Plan(setup, program);
    ResetToBase(built.scene, 1);
    const anim::AnimationClip clip = mc::MakeMachineClip(built.scene, track, program);
    EXPECT_NEAR(clip.Duration(), track.stats.duration_sec, kTol);

    const anim::AnimationTrack* x_track = FindTrack(clip, ComponentId(built.scene, "X"));
    ASSERT_NE(x_track, nullptr);
    EXPECT_LT(x_track->keys.back().time_sec, track.stats.duration_sec);
}

TEST(AnimationBridgeTest, SameTime_KeysAreCoalesced) {
    // 動特性の無い機械では全サンプルが時刻0になる
    const mc::MachiningSetup setup = MakeSetupWithoutDynamics();
    BuiltScene built = MakeScene(setup);
    const mc::ClProgram program = Program({
            mc::ClLoadTool{1}, RapidTo(0.0, 0.0), RapidTo(50.0, 0.0),
            RapidTo(50.0, 50.0)});
    const mc::MotionTrack track = Plan(setup, program);
    ASSERT_NEAR(track.stats.duration_sec, 0.0, kTol);
    ResetToBase(built.scene, 1);

    anim::AnimationClip clip;
    ASSERT_NO_THROW(clip = mc::MakeMachineClip(built.scene, track, program));
    EXPECT_NEAR(clip.Duration(), 0.0, kTol);

    // 同時刻のキーは最後の値だけになる
    const anim::AnimationTrack* x_track = FindTrack(clip, ComponentId(built.scene, "X"));
    ASSERT_NE(x_track, nullptr);
    ASSERT_EQ(x_track->keys.size(), 1u);
    EXPECT_TRUE(x_track->keys[0].transform.isApprox(
            mc::Forward(built.scene.Model(), track.samples.back().q)
                    [*built.scene.Model().FindComponent("X")], kTol));
    const auto records = EventKeys(clip, std::string(mc::kRecordEventTrack));
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].second, 3);
}



/**
 * ---- AnimationPlayerとの結び付け ----
 */

TEST(AnimationBridgeTest, Bind_ResolvesAllTargets) {
    const mc::MachiningSetup setup = MakeSetup(TwoToolProject());
    BuiltScene built = MakeScene(setup);
    const mc::ClProgram program = ToolChangeProgram();
    const mc::MotionTrack track = Plan(setup, program);
    ResetToBase(built.scene, 1);
    const anim::AnimationClip clip = mc::MakeMachineClip(built.scene, track, program);

    anim::AnimationPlayer player;
    const std::vector<ObjectID> unresolved = player.Bind(built.root, clip);
    EXPECT_TRUE(unresolved.empty());
    EXPECT_NEAR(player.Duration(), track.stats.duration_sec, kTol);
}

TEST(AnimationBridgeTest, Playback_MatchesTrack) {
    const mc::MachiningSetup setup = MakeSetup(TwoToolProject());
    BuiltScene built = MakeScene(setup);
    mc::MachineScene& scene = built.scene;
    const mc::ClProgram program = ToolChangeProgram();
    const mc::MotionTrack track = Plan(setup, program);
    ResetToBase(scene, 1);
    anim::AnimationPlayer player;
    player.Bind(built.root, mc::MakeMachineClip(scene, track, program));

    const mc::MachineModel& model = scene.Model();
    const double duration = track.stats.duration_sec;
    for (const double ratio : {0.0, 0.2, 0.45, 0.7, 0.95, 1.0}) {
        const double t = duration * ratio;
        player.Seek(t);
        const mc::MotionSample& sample = track.samples[mc::SampleIndexAtTime(track, t)];
        const std::vector<Matrix4d> frames = mc::Forward(model, sample.q);
        for (std::size_t i = 0; i < model.ComponentCount(); ++i) {
            const auto component = scene.ComponentAssembly(model.Component(i).name);
            EXPECT_TRUE(component->GetGlobalTransform().isApprox(frames[i], kTol))
                    << model.Component(i).name << " at t = " << t;
        }
        for (const int number : {1, 2}) {
            EXPECT_EQ(scene.ToolAssembly(number)->Display().visible,
                      sample.tool_number == number) << "T" << number << " at t = " << t;
        }
    }
}

TEST(AnimationBridgeTest, Seek_BackwardRestoresTool) {
    const mc::MachiningSetup setup = MakeSetup(TwoToolProject());
    BuiltScene built = MakeScene(setup);
    mc::MachineScene& scene = built.scene;
    const mc::ClProgram program = ToolChangeProgram();
    const mc::MotionTrack track = Plan(setup, program);
    ResetToBase(scene, 1);
    anim::AnimationPlayer player;
    player.Bind(built.root, mc::MakeMachineClip(scene, track, program));

    // 工具2の区間から工具1の区間に戻る
    player.Seek((FirstTime(track, 3) + FirstTime(track, 5)) / 2.0);
    EXPECT_FALSE(scene.ToolAssembly(1)->Display().visible);
    EXPECT_TRUE(scene.ToolAssembly(2)->Display().visible);
    player.Seek(FirstTime(track, 3) / 2.0);
    EXPECT_TRUE(scene.ToolAssembly(1)->Display().visible);
    EXPECT_FALSE(scene.ToolAssembly(2)->Display().visible);

    // 停止で基準状態 (初期工具・ゼロポーズ) に戻る
    player.Seek(track.stats.duration_sec);
    player.Stop();
    EXPECT_TRUE(scene.ToolAssembly(1)->Display().visible);
    EXPECT_FALSE(scene.ToolAssembly(2)->Display().visible);
    const mc::MachineModel& model = scene.Model();
    for (std::size_t i = 0; i < model.ComponentCount(); ++i) {
        EXPECT_TRUE(scene.ComponentAssembly(model.Component(i).name)
                            ->GetGlobalTransform().isIdentity(kTol));
    }
}



/**
 * ---- 異常系 ----
 */

TEST(AnimationBridgeTest, MakeMachineClip_ThrowsInvalidArgumentWhenNotBuilt) {
    const mc::MachiningSetup setup = MakeSetup();
    const mc::ClProgram program = Program({mc::ClLoadTool{1}, RapidTo(0.0, 0.0)});
    const mc::MotionTrack track = Plan(setup, program);
    mc::MachineScene scene;
    EXPECT_THROW(mc::MakeMachineClip(scene, track, program), std::invalid_argument);
}

TEST(AnimationBridgeTest, MakeMachineClip_ThrowsInvalidArgumentWhenTrackIsEmpty) {
    const mc::MachiningSetup setup = MakeSetup();
    BuiltScene built = MakeScene(setup);
    const mc::ClProgram program = Program({mc::ClLoadTool{1}});
    const mc::MotionTrack track = Plan(setup, program);
    ASSERT_TRUE(track.samples.empty());
    EXPECT_THROW(mc::MakeMachineClip(built.scene, track, program), std::invalid_argument);
}
