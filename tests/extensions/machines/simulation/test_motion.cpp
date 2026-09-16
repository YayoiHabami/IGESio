/**
 * @file tests/extensions/machines/simulation/test_motion.cpp
 * @brief 動作生成 (simulation/motion) のテスト
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note 対象: PlanMotion / SampleIndexAtTime / DisplayNc
 *       - 正常系: 区間時間の閉形式 (早送り/切削/送り不明)、ドウェル、初期姿勢
 *         サンプル、回転方向の正規化、fpsの低減、TCP区間と直接指令の補間,
 *         登録値相対の座標語と工具長補正、機械座標、工具軸方向の継続と仮定,
 *         到達不能、可動範囲外の扱い、円弧の分割、回転角の解の連続性、制御点,
 *         工具交換、動作でないレコード、時刻の二分探索、表示用の指令値,
 *         自己検証の間隔、実機NCの通し
 *       - 正常系 (境界値・退化): 動作レコードの無いプログラム、空のトラック,
 *         `interpolate = false`、始点不明の円弧
 *       - 異常系: 幾何形式のワークオフセットでの座標語 (`KinematicsError`),
 *         `kError`での可動範囲外 (`KinematicsError`)、不正な設定
 *         (`std::invalid_argument`)
 *       TODO: `NotImplementedError` (対応しない軸構成) は逆運動学側で検証済み
 *       (`test_inverse_kinematics.cpp`) のため本ファイルでは扱わない
 * @note フィクスチャは実例機 (工具側XYZ・ワーク側AC. 工具取り付け点 (0,-180,250.5),
 *       各軸に動特性あり) と`MinimalProject` (簡易ボール工具#1・G54登録値・
 *       G55幾何形式・初期Z=100). G54は`W_0 = I`、工具#1の制御点 (先端) の
 *       `tool_mount`フレーム座標は (0, 0, -90)
 */
#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/core/rotation.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/machine/axis_values.h"
#include "igesio/extensions/machines/machine/forward_kinematics.h"
#include "igesio/extensions/machines/machine/machine_model.h"
#include "igesio/extensions/machines/project/project_definition.h"
#include "igesio/extensions/machines/project/setup.h"
#include "igesio/extensions/machines/simulation/motion.h"
#include "igesio/extensions/machines/toolpath/cl_program.h"
#include "igesio/extensions/machines/toolpath/nc_dialect.h"
#include "igesio/extensions/machines/toolpath/nc_interpreter.h"
#include "../machine/machines_for_testing.h"
#include "../project/projects_for_testing.h"

namespace {

namespace mc = igesio::extensions::machines;
using igesio::Vector3d;
using mc::ToRadians;
using machines_test::MinimalXyzAc;
using projects_test::MinimalProject;
using projects_test::ReadProjectText;
using projects_test::ReadProjectWithMachine;
using projects_test::Replace;

/// @brief 数値比較の許容誤差
constexpr double kTol = 1e-9;
/// @brief 逆運動学を経た値の比較の許容誤差
constexpr double kIkTol = 1e-6;
/// @brief 工具#1の制御点 (先端) の`tool_mount`フレーム座標のz [mm] (ゲージ長90)
constexpr double kTipZ = -90.0;
/// @brief 実例機の直進軸の早送り/最大送り [mm/s] (6000 mm/min)
constexpr double kLinearFeed = 100.0;
/// @brief NCのテストデータのディレクトリ
const std::filesystem::path kNcDir = machines_test::kFixturePath.parent_path() / "nc";

/// @brief 実例機 (動特性あり) の最小構成プロジェクトからセットアップを作る
mc::MachiningSetup MakeSetup(const std::string& toml = MinimalProject()) {
    return mc::MachiningSetup(ReadProjectText(toml));
}

/// @brief 動特性の無い機械 (`MinimalXyzAc`. 幾何は実例機と同じ) でセットアップを作る
mc::MachiningSetup MakeSetupWithoutDynamics() {
    const std::string body = Replace(MinimalProject(),
                                     "[machine]\nlibrary = \"t-ZYX-b-AC-w.toml\"\n", "");
    return mc::MachiningSetup(ReadProjectWithMachine(MinimalXyzAc(), body,
                                                     "igesio_motion_no_dynamics"));
}

/// @brief 3軸機 (`ThreeAxis`. 工具取り付け点 (0,0,100)) でセットアップを作る
/// @note G54の登録値は工具取り付け点を原点に置く {Z = -100} にする (W_0 = I)
mc::MachiningSetup MakeSetupThreeAxis() {
    std::string body = Replace(MinimalProject(),
                               "[machine]\nlibrary = \"t-ZYX-b-AC-w.toml\"\n", "");
    body = Replace(body, "values = { X = 0.0, Y = 180.0, Z = -250.5 }",
                   "values = { X = 0.0, Y = 0.0, Z = -100.0 }");
    return mc::MachiningSetup(ReadProjectWithMachine(machines_test::ThreeAxis(), body,
                                                     "igesio_motion_three_axis"));
}

/// @brief 制御点と工具軸方向を持つ移動を作る
mc::ClGoto Goto(const Vector3d& point,
                const std::optional<Vector3d>& axis = std::nullopt,
                const mc::MotionKind kind = mc::MotionKind::kLinear) {
    mc::ClGoto motion;
    motion.kind = kind;
    motion.point = point;
    motion.tool_axis = axis;
    return motion;
}

/// @brief 軸の指令のみの移動 (登録値相対の座標語、または機械座標) を作る
mc::ClGoto Words(const mc::NcValues& words,
                 const mc::MotionFrame frame = mc::MotionFrame::kWork,
                 const mc::MotionKind kind = mc::MotionKind::kLinear) {
    mc::ClGoto motion;
    motion.kind = kind;
    motion.axis_words = words;
    motion.frame = frame;
    return motion;
}

/// @brief レコード列からプログラムを作る (行番号は索引+1)
mc::ClProgram Program(std::vector<mc::ClRecord> records) {
    mc::ClProgram program;
    program.records = std::move(records);
    for (std::size_t i = 0; i < program.records.size(); ++i) {
        program.sources.push_back(mc::SourceLocation{0, static_cast<int>(i) + 1});
    }
    return program;
}

/// @brief 工具#1を選択してから始まるプログラムを作る
mc::ClProgram WithTool(std::vector<mc::ClRecord> records) {
    records.insert(records.begin(), mc::ClLoadTool{1});
    return Program(std::move(records));
}

/// @brief 可動範囲外を無視して動作を生成する
/// @note フィクスチャのZ軸は-90 mmが下限で、テーブル上面付近の制御点は
///       可動範囲外になる. 可動範囲外の扱い自体は`Overtravel_*`で検証する
mc::MotionTrack Plan(const mc::MachiningSetup& setup, const mc::ClProgram& program,
                     mc::MotionOptions options = {}) {
    if (!options.overtravel.has_value()) {
        options.overtravel = mc::OvertravelPolicy::kIgnore;
    }
    return mc::PlanMotion(setup, program, options);
}

/// @brief レコードの終点のサンプルを索引順に集める
std::vector<mc::MotionSample> CommandSamples(const mc::MotionTrack& track) {
    std::vector<mc::MotionSample> found;
    for (const mc::MotionSample& sample : track.samples) {
        if (sample.is_command_point) found.push_back(sample);
    }
    return found;
}

/// @brief 文言に部分文字列を含む警告の数
std::size_t CountWarnings(const std::vector<mc::Diagnostic>& warnings,
                          const std::string& text) {
    std::size_t count = 0;
    for (const mc::Diagnostic& warning : warnings) {
        if (warning.message.find(text) != std::string::npos) ++count;
    }
    return count;
}

/// @brief 工具#1の制御点 (先端) のゼロポーズ機械座標 (`tool_mount`に固定. H_tm·c)
Vector3d ControlHome(const mc::MachineModel& model) {
    return mc::ApplyPoint(model.MountPlacement(mc::MountKind::kToolMount),
                          Vector3d(0.0, 0.0, kTipZ));
}

/// @brief 制御点のゼロポーズ機械座標 (`work_mount`基準) を順運動学で計算する
Vector3d ControlPointHome(const mc::MachineModel& model, const mc::JointVector& q,
                          const Vector3d& control_home) {
    const auto frames = mc::Forward(model, q);
    return mc::ApplyPoint(mc::RigidInverse(frames[model.WorkMountIndex()]),
                          mc::ApplyPoint(frames[model.ToolMountIndex()], control_home));
}

/// @brief 現在姿勢の工具軸方向 (ワーク座標. `work_mount`基準) を順運動学で計算する
Vector3d ToolAxisWork(const mc::MachineModel& model, const mc::JointVector& q) {
    const auto frames = mc::Forward(model, q);
    return mc::ApplyDirection(mc::RigidInverse(frames[model.WorkMountIndex()]),
                              mc::ApplyDirection(frames[model.ToolMountIndex()],
                                                 model.ToolAxisHome()));
}

/// @brief 指定軸のNC指令値を取得する
double NcOf(const mc::MachineModel& model, const mc::JointVector& q,
            const std::string& axis) {
    return mc::NcFromJoints(model, q).At(axis);
}

}  // namespace



/**
 * ---- 時間軸 ----
 */

TEST(MotionTest, Time_ClosedForm) {
    const auto setup = MakeSetup();
    // 早送り: 制御点は初期姿勢の (0, -180, 260.5) から (10, 0, 0) へ (Zの260.5 mmが最長)
    // 切削: Y方向に50 mmをF = 5 mm/sで10 s (軸の最大送りでは0.5 s)
    const mc::ClProgram program = WithTool({
            mc::ClFeed{5.0},
            Goto(Vector3d(10.0, 0.0, 0.0), Vector3d::UnitZ(), mc::MotionKind::kRapid),
            Goto(Vector3d(10.0, 50.0, 0.0))});
    const mc::MotionTrack track = Plan(setup, program);
    const auto commands = CommandSamples(track);
    ASSERT_EQ(commands.size(), 2u);
    EXPECT_NEAR(commands[0].time, 260.5 / kLinearFeed, kIkTol);
    EXPECT_NEAR(commands[1].time - commands[0].time, 50.0 / 5.0, kIkTol);
    EXPECT_NEAR(track.stats.duration_sec, commands[1].time, kTol);
    EXPECT_EQ(CountWarnings(track.warnings, "fallback feed"), 0u);
}

TEST(MotionTest, Time_FallbackFeedWhenUnknown) {
    const auto setup = MakeSetup();
    mc::MotionOptions options;
    options.fallback_feed = 20.0;
    const mc::ClProgram program = WithTool({
            Goto(Vector3d(10.0, 0.0, 0.0), Vector3d::UnitZ(), mc::MotionKind::kRapid),
            Goto(Vector3d(10.0, 50.0, 0.0))});
    const mc::MotionTrack track = Plan(setup, program, options);
    const auto commands = CommandSamples(track);
    ASSERT_EQ(commands.size(), 2u);
    EXPECT_NEAR(commands[1].time - commands[0].time, 50.0 / 20.0, kIkTol);
    EXPECT_EQ(CountWarnings(track.warnings, "fallback feed"), 1u);
}

TEST(MotionTest, Time_ZeroWithoutDynamics) {
    const auto setup = MakeSetupWithoutDynamics();
    const mc::ClProgram program = WithTool({
            Goto(Vector3d(10.0, 0.0, 0.0), Vector3d::UnitZ(), mc::MotionKind::kRapid),
            mc::ClFeed{5.0},
            Goto(Vector3d(10.0, 50.0, 0.0))});
    const mc::MotionTrack track = Plan(setup, program);
    const auto commands = CommandSamples(track);
    ASSERT_EQ(commands.size(), 2u);
    // 早送りは軸の動特性が無いので0 s、切削はL/Fのみ
    EXPECT_NEAR(commands[0].time, 0.0, kTol);
    EXPECT_NEAR(commands[1].time, 10.0, kIkTol);
}

TEST(MotionTest, Time_Dwell) {
    const auto setup = MakeSetup();
    const mc::ClProgram program = WithTool({
            Goto(Vector3d(10.0, 0.0, 0.0), Vector3d::UnitZ(), mc::MotionKind::kRapid),
            mc::ClDwell{2.5}});
    const mc::MotionTrack track = Plan(setup, program);
    const auto commands = CommandSamples(track);
    ASSERT_EQ(commands.size(), 2u);
    EXPECT_NEAR(commands[1].time - commands[0].time, 2.5, kTol);
    EXPECT_TRUE(commands[1].q == commands[0].q);
    EXPECT_EQ(commands[1].record_index, 2u);
    // ドウェルは補間しないので、サンプルは終点の1つだけ
    EXPECT_EQ(track.record_first_sample[2] + 1, track.samples.size());
}



/**
 * ---- サンプル列の構成 ----
 */

TEST(MotionTest, InitialSample_AtTimeZero) {
    const auto setup = MakeSetup();
    const mc::ClProgram program = Program({
            mc::ClComment{"head"}, mc::ClLoadTool{1},
            Goto(Vector3d(10.0, 0.0, 0.0), Vector3d::UnitZ(), mc::MotionKind::kRapid)});
    const mc::MotionTrack track = Plan(setup, program);
    ASSERT_GE(track.samples.size(), 2u);
    const mc::MotionSample& first = track.samples.front();
    EXPECT_NEAR(first.time, 0.0, kTol);
    EXPECT_TRUE(first.q == setup.BaseQ());
    EXPECT_EQ(first.tool_number, 1);
    EXPECT_EQ(first.record_index, 2u);
    EXPECT_FALSE(first.is_command_point);
    EXPECT_EQ(first.kind, mc::MotionKind::kRapid);
    ASSERT_EQ(track.record_first_sample.size(), 3u);
    EXPECT_EQ(track.record_first_sample[0], 0u);
    EXPECT_EQ(track.record_first_sample[2], 0u);
}

TEST(MotionTest, InitialSample_NoMotionGivesEmptyTrack) {
    const auto setup = MakeSetup();
    const mc::ClProgram program = Program({mc::ClLoadTool{1}, mc::ClComment{"only"}});
    const mc::MotionTrack track = Plan(setup, program);
    EXPECT_TRUE(track.samples.empty());
    EXPECT_EQ(track.stats.motion_record_count, 0u);
    ASSERT_EQ(track.record_first_sample.size(), 2u);
    EXPECT_EQ(track.record_first_sample[1], 0u);
}

TEST(MotionTest, NonMotion_RecordsAreTransparent) {
    const auto setup = MakeSetup();
    const mc::ClProgram program = WithTool({
            mc::ClComment{"c"},
            Goto(Vector3d(10.0, 0.0, 0.0), Vector3d::UnitZ(), mc::MotionKind::kRapid),
            mc::ClMarker{mc::ClMarker::Kind::kPathBegin, "p"},
            mc::ClSpindle{mc::ClSpindle::Mode::kCw, 1000.0},
            mc::ClPassThrough{"nc", "M1413"},
            Goto(Vector3d(10.0, 10.0, 0.0)),
            mc::ClCoolant{true}});
    const mc::MotionTrack track = Plan(setup, program);
    const std::vector<std::size_t>& first = track.record_first_sample;
    ASSERT_EQ(first.size(), 8u);
    EXPECT_EQ(first[0], 0u);
    EXPECT_EQ(first[2], 0u);
    const std::size_t second = first[6];
    EXPECT_GT(second, 0u);
    EXPECT_EQ(first[3], second);
    EXPECT_EQ(first[4], second);
    EXPECT_EQ(first[5], second);
    EXPECT_EQ(first[7], track.samples.size());
    EXPECT_EQ(track.stats.motion_record_count, 2u);
    for (const mc::MotionSample& sample : track.samples) {
        EXPECT_TRUE(sample.record_index == 2 || sample.record_index == 6);
    }
}

TEST(MotionTest, Fps_Reduced) {
    const auto setup = MakeSetup();
    // 制御点を260 mm下ろす早送り (2.6 s → 30 fpsで78サンプル) を上限20に収める
    const mc::ClProgram program = WithTool({
            Goto(Vector3d(0.0, 0.0, 0.0), Vector3d::UnitZ(), mc::MotionKind::kRapid)});
    mc::MotionOptions options;
    options.max_samples = 20;
    const mc::MotionTrack track = Plan(setup, program, options);
    EXPECT_TRUE(track.stats.fps_reduced);
    EXPECT_LT(track.stats.fps, 30.0);
    EXPECT_LE(track.samples.size(), 20u);
    EXPECT_GT(track.samples.size(), 2u);
    EXPECT_EQ(CountWarnings(track.warnings, "fps reduced"), 1u);

    options.interpolate = false;
    const mc::MotionTrack plain = Plan(setup, program, options);
    EXPECT_EQ(plain.samples.size(), 2u);
    EXPECT_FALSE(plain.stats.fps_reduced);
    EXPECT_NEAR(plain.stats.fps, 30.0, kTol);
}

TEST(MotionTest, Fps_DisabledWhenCommandPointsExceedLimit) {
    const auto setup = MakeSetup();
    std::vector<mc::ClRecord> records;
    for (int i = 0; i < 5; ++i) {
        records.push_back(Goto(Vector3d(static_cast<double>(i), 0.0, 0.0), Vector3d::UnitZ(),
                               mc::MotionKind::kRapid));
    }
    mc::MotionOptions options;
    options.max_samples = 3;
    const mc::MotionTrack track = Plan(setup, WithTool(records), options);
    EXPECT_TRUE(track.stats.fps_reduced);
    EXPECT_EQ(track.samples.size(), 6u);
    EXPECT_EQ(CountWarnings(track.warnings, "interpolation is disabled"), 1u);
}



/**
 * ---- 補間 ----
 */

TEST(MotionTest, Interp_TcpRelinearsOnly) {
    const auto setup = MakeSetup();
    const mc::MachineModel& model = setup.Model();
    const Vector3d tilted(std::sin(ToRadians(30.0)), 0.0, std::cos(ToRadians(30.0)));
    const mc::ClProgram program = WithTool({
            Goto(Vector3d(0.0, 0.0, 0.0), Vector3d::UnitZ(), mc::MotionKind::kRapid),
            Goto(Vector3d(40.0, 20.0, 0.0), tilted)});
    const mc::MotionTrack track = Plan(setup, program);
    const std::size_t begin = track.record_first_sample[2];
    ASSERT_GT(track.samples.size() - begin, 3u);
    const mc::MotionSample& start = track.samples[begin - 1];
    const mc::MotionSample& end = track.samples.back();
    EXPECT_TRUE(end.is_command_point);
    const Vector3d control = ControlHome(model);
    const Vector3d p0 = ControlPointHome(model, start.q, control);
    const Vector3d p1 = ControlPointHome(model, end.q, control);
    const Vector3d direction = (p1 - p0).normalized();
    for (std::size_t i = begin; i + 1 < track.samples.size(); ++i) {
        const mc::MotionSample& sample = track.samples[i];
        EXPECT_FALSE(sample.is_command_point);
        // 制御点は始点と終点を結ぶ直線上
        const Vector3d p = ControlPointHome(model, sample.q, control);
        EXPECT_LT(((p - p0) - (p - p0).dot(direction) * direction).norm(), kIkTol);
        // 回転軸は補間係数に比例
        const double s = (sample.time - start.time) / (end.time - start.time);
        for (const char* axis : {"A", "C"}) {
            const double from = NcOf(model, start.q, axis);
            const double to = NcOf(model, end.q, axis);
            EXPECT_NEAR(NcOf(model, sample.q, axis), from + s * (to - from), kIkTol);
        }
    }
}

TEST(MotionTest, Interp_RotaryWordsKeepControlPointOnLine) {
    const auto setup = MakeSetup();
    const mc::MachineModel& model = setup.Model();
    // 回転軸の指令の形式: 制御点+A/Cの指令値 (工具軸方向は持たない)
    mc::ClGoto first = Goto(Vector3d(0.0, 0.0, 0.0), std::nullopt, mc::MotionKind::kRapid);
    first.axis_words = mc::NcValues{{"A", 0.0}, {"C", 0.0}};
    mc::ClGoto second = Goto(Vector3d(30.0, 0.0, 10.0));
    second.axis_words = mc::NcValues{{"A", ToRadians(40.0)}};
    const mc::MotionTrack track = Plan(setup, WithTool({first, second}));
    EXPECT_EQ(CountWarnings(track.warnings, "assumed"), 0u);
    const std::size_t begin = track.record_first_sample[2];
    ASSERT_GT(track.samples.size() - begin, 3u);
    const Vector3d control = ControlHome(model);
    const Vector3d p0 = ControlPointHome(model, track.samples[begin - 1].q, control);
    const Vector3d p1 = ControlPointHome(model, track.samples.back().q, control);
    EXPECT_TRUE(p1.isApprox(Vector3d(30.0, 0.0, 10.0), kIkTol)) << p1.transpose();
    EXPECT_NEAR(NcOf(model, track.samples.back().q, "A"), ToRadians(40.0), kIkTol);
    const Vector3d direction = (p1 - p0).normalized();
    for (std::size_t i = begin; i < track.samples.size(); ++i) {
        const Vector3d p = ControlPointHome(model, track.samples[i].q, control);
        EXPECT_LT(((p - p0) - (p - p0).dot(direction) * direction).norm(), kIkTol);
    }
}

TEST(MotionTest, Interp_AxisSpaceForAxisWords) {
    const auto setup = MakeSetup();
    const mc::ClProgram program = WithTool({
            Words(mc::NcValues{{"X", 10.0}, {"Y", 20.0}}, mc::MotionFrame::kWork,
                  mc::MotionKind::kRapid),
            Words(mc::NcValues{{"X", 30.0}, {"Y", 60.0}, {"C", ToRadians(90.0)}})});
    const mc::MotionTrack track = Plan(setup, program);
    const std::size_t begin = track.record_first_sample[2];
    ASSERT_GT(track.samples.size() - begin, 3u);
    const mc::MotionSample& start = track.samples[begin - 1];
    const mc::MotionSample& end = track.samples.back();
    for (std::size_t i = begin; i < track.samples.size(); ++i) {
        const mc::MotionSample& sample = track.samples[i];
        const double s = (sample.time - start.time) / (end.time - start.time);
        for (std::size_t j = 0; j < sample.q.Size(); ++j) {
            EXPECT_NEAR(sample.q[j], start.q[j] + s * (end.q[j] - start.q[j]), kTol);
        }
    }
}



/**
 * ---- 登録値相対の座標語・機械座標 ----
 */

TEST(MotionTest, AxisWords_RegisteredValues) {
    const auto setup = MakeSetup();
    const mc::MachineModel& model = setup.Model();
    // G54 = {X=0, Y=180, Z=-250.5}. 工具#1 (先端) の補正はZに+90
    const mc::ClProgram program = WithTool({
            Words(mc::NcValues{{"X", 10.0}, {"Y", 20.0}, {"Z", 5.0}})});
    const mc::MotionTrack track = Plan(setup, program);
    const auto commands = CommandSamples(track);
    ASSERT_EQ(commands.size(), 1u);
    EXPECT_NEAR(NcOf(model, commands[0].q, "X"), 10.0, kTol);
    EXPECT_NEAR(NcOf(model, commands[0].q, "Y"), 200.0, kTol);
    EXPECT_NEAR(NcOf(model, commands[0].q, "Z"), 5.0 - 250.5 - kTipZ, kTol);
    EXPECT_NEAR(NcOf(model, commands[0].q, "A"), 0.0, kTol);
}

TEST(MotionTest, AxisWords_ThrowsKinematicsErrorUnderGeometricOffset) {
    const auto setup = MakeSetup();
    EXPECT_THROW(Plan(setup, WithTool({mc::ClSelectWorkOffset{"G55"},
                                                 Words(mc::NcValues{{"X", 1.0}})})),
                 mc::KinematicsError);
    // 回転軸のみのレコードとドウェルは登録値を要しない
    EXPECT_NO_THROW(Plan(setup, WithTool({mc::ClSelectWorkOffset{"G55"},
                                                    Words(mc::NcValues{{"C", 1.0}}),
                                                    mc::ClDwell{1.0}})));
}

TEST(MotionTest, AxisWords_LengthCompensation) {
    const std::string gauge = Replace(MinimalProject(), "[tool.simple]",
                                      "control_point = \"gauge\"\n\n[tool.simple]")
                              + "\n[[tool_offset]]\nnumber = 1\ntool = 1\n"
                                "length_wear = -0.02\n";
    const auto setup = MakeSetup(gauge);
    const mc::MachineModel& model = setup.Model();
    // length はゲージ長90が既定なので g43 = 89.98. Zの指令があるレコードだけ補正が効く
    const mc::ClProgram program = WithTool({
            mc::ClLengthOffset{1},
            Words(mc::NcValues{{"Z", 5.0}}),
            Words(mc::NcValues{{"X", 10.0}}),
            mc::ClLengthOffset{std::nullopt},
            Words(mc::NcValues{{"Z", 5.0}})});
    const mc::MotionTrack track = Plan(setup, program);
    const auto commands = CommandSamples(track);
    ASSERT_EQ(commands.size(), 3u);
    EXPECT_NEAR(NcOf(model, commands[0].q, "Z"), 5.0 - 250.5 + 89.98, kTol);
    EXPECT_NEAR(NcOf(model, commands[1].q, "Z"), 5.0 - 250.5 + 89.98, kTol);
    EXPECT_NEAR(NcOf(model, commands[1].q, "X"), 10.0, kTol);
    EXPECT_NEAR(NcOf(model, commands[2].q, "Z"), 5.0 - 250.5, kTol);

    // 先端が制御点の工具では補正番号を無視し、ゲージ長−指令点位置 (90) を加える
    const auto tip = MakeSetup(MinimalProject() + "\n[[tool_offset]]\nnumber = 1\n"
                                                  "tool = 1\nlength_wear = -0.02\n");
    const mc::MotionTrack tip_track = Plan(
            tip, WithTool({mc::ClLengthOffset{1}, Words(mc::NcValues{{"Z", 5.0}})}));
    ASSERT_EQ(CommandSamples(tip_track).size(), 1u);
    EXPECT_NEAR(NcOf(tip.Model(), CommandSamples(tip_track)[0].q, "Z"),
                5.0 - 250.5 - kTipZ, kTol);
}

TEST(MotionTest, MachineFrame_G53AndG28) {
    const auto setup = MakeSetup();
    const mc::MachineModel& model = setup.Model();
    const mc::ClProgram program = WithTool({
            Words(mc::NcValues{{"Z", 0.0}}, mc::MotionFrame::kMachine,
                  mc::MotionKind::kRapid),
            Words(mc::NcValues{{"X", 10.0}})});
    const mc::MotionTrack track = Plan(setup, program);
    const auto commands = CommandSamples(track);
    ASSERT_EQ(commands.size(), 2u);
    // 機械座標の指令には登録値も補正も加わらず、未指定のX/Yは初期値のまま
    EXPECT_NEAR(NcOf(model, commands[0].q, "Z"), 0.0, kTol);
    EXPECT_NEAR(NcOf(model, commands[0].q, "X"), 0.0, kTol);
    // その後の座標語で未指定のZは機械位置 (0) を保つ
    EXPECT_NEAR(NcOf(model, commands[1].q, "X"), 10.0, kTol);
    EXPECT_NEAR(NcOf(model, commands[1].q, "Z"), 0.0, kTol);
}



/**
 * ---- 工具軸方向・逆運動学 ----
 */

TEST(MotionTest, ToolAxis_ContinuesAndDefaults) {
    const auto setup = MakeSetup();
    const mc::MachineModel& model = setup.Model();
    const Vector3d tilted(std::sin(ToRadians(20.0)), 0.0, std::cos(ToRadians(20.0)));
    const mc::ClProgram program = WithTool({
            Goto(Vector3d(0.0, 0.0, 0.0), tilted, mc::MotionKind::kRapid),
            Goto(Vector3d(10.0, 0.0, 0.0))});
    const mc::MotionTrack track = Plan(setup, program);
    const auto commands = CommandSamples(track);
    ASSERT_EQ(commands.size(), 2u);
    EXPECT_NEAR(NcOf(model, commands[1].q, "A"), NcOf(model, commands[0].q, "A"), kIkTol);
    EXPECT_NEAR(NcOf(model, commands[1].q, "C"), NcOf(model, commands[0].q, "C"), kIkTol);
    EXPECT_LT(ToolAxisWork(model, commands[1].q).cross(tilted).norm(), kIkTol);
    EXPECT_EQ(CountWarnings(track.warnings, "assumed"), 0u);

    // 工具軸方向も回転軸の指令も無ければ+Zを仮定し、警告はプログラムにつき1件
    const mc::ClProgram bare = WithTool({
            Goto(Vector3d(0.0, 0.0, 0.0), std::nullopt, mc::MotionKind::kRapid),
            Goto(Vector3d(10.0, 0.0, 0.0))});
    const mc::MotionTrack bare_track = Plan(setup, bare);
    EXPECT_EQ(CountWarnings(bare_track.warnings, "assumed"), 1u);
    for (const mc::MotionSample& sample : CommandSamples(bare_track)) {
        EXPECT_NEAR(NcOf(model, sample.q, "A"), 0.0, kIkTol);
    }
}

TEST(MotionTest, Unreachable_KeepsPrevious) {
    // 回転軸の無い3軸機では、傾いた工具軸方向は到達不能 (AC機では可動範囲外で解ける)
    const auto setup = MakeSetupThreeAxis();
    const Vector3d tilted(std::sin(ToRadians(20.0)), 0.0, std::cos(ToRadians(20.0)));
    const mc::ClProgram program = WithTool({
            Goto(Vector3d(0.0, 0.0, 0.0), Vector3d::UnitZ(), mc::MotionKind::kRapid),
            Goto(Vector3d(10.0, 0.0, 0.0), tilted)});
    const mc::MotionTrack track = Plan(setup, program);
    const auto commands = CommandSamples(track);
    ASSERT_EQ(commands.size(), 2u);
    EXPECT_TRUE(commands[1].q == commands[0].q);
    EXPECT_EQ(track.stats.unreachable_count, 1u);
    ASSERT_TRUE(commands[1].warning.has_value());
    EXPECT_NE(track.warnings[*commands[1].warning].message.find("unreachable"),
              std::string::npos);
    EXPECT_EQ(track.warnings[*commands[1].warning].context, "record 2");
    EXPECT_EQ(track.warnings[*commands[1].warning].line, 3);
}

TEST(MotionTest, PrevNc_ContinuesAcrossRecords) {
    // 初期姿勢A=-20°から始め、kContinuousでは負側の回転角候補を採り続ける
    const auto setup = MakeSetup(Replace(MinimalProject(), "Z = 100.0", "Z = 100.0\nA = -20.0"));
    const mc::MachineModel& model = setup.Model();
    std::vector<mc::ClRecord> records;
    for (const double azimuth : {0.0, 20.0, 40.0, 20.0, 0.0, -20.0}) {
        const Vector3d axis(std::sin(ToRadians(30.0)) * std::cos(ToRadians(azimuth)),
                            std::sin(ToRadians(30.0)) * std::sin(ToRadians(azimuth)),
                            std::cos(ToRadians(30.0)));
        records.push_back(Goto(Vector3d(0.0, 0.0, 0.0), axis));
    }
    mc::MotionOptions options;
    options.branch = mc::BranchPolicy::kContinuous;
    const mc::MotionTrack track = Plan(setup, WithTool(records), options);
    const auto commands = CommandSamples(track);
    ASSERT_EQ(commands.size(), 6u);
    for (const mc::MotionSample& sample : commands) {
        EXPECT_LT(NcOf(model, sample.q, "A"), 0.0);
    }
    EXPECT_EQ(track.stats.unreachable_count, 0u);
}

TEST(MotionTest, Rotary_ShortestTurn) {
    const auto setup = MakeSetup();
    const mc::MachineModel& model = setup.Model();
    const mc::ClProgram program = WithTool({
            Words(mc::NcValues{{"C", ToRadians(350.0)}}),
            Words(mc::NcValues{{"C", ToRadians(10.0)}}),
            Words(mc::NcValues{{"C", ToRadians(200.0)}}),
            Words(mc::NcValues{{"A", ToRadians(80.0)}}),
            Words(mc::NcValues{{"A", ToRadians(-80.0)}})});
    const mc::MotionTrack track = Plan(setup, program);
    const auto commands = CommandSamples(track);
    ASSERT_EQ(commands.size(), 5u);
    // 無制限のC軸は直前の値に近い回転方向 (0→350は-10、-10→10は+20、10→200は-160)
    EXPECT_NEAR(NcOf(model, commands[0].q, "C"), ToRadians(-10.0), kTol);
    EXPECT_NEAR(NcOf(model, commands[1].q, "C"), ToRadians(10.0), kTol);
    EXPECT_NEAR(NcOf(model, commands[2].q, "C"), ToRadians(-160.0), kTol);
    // 制限のあるA軸は補正しない
    EXPECT_NEAR(NcOf(model, commands[4].q, "A"), ToRadians(-80.0), kTol);
    EXPECT_EQ(CountWarnings(track.warnings, "shortest direction"), 1u);
    // 表示用の指令値は wrap_start = 0 基準
    EXPECT_NEAR(mc::DisplayNc(model, commands[0].q).At("C"), ToRadians(350.0), kTol);
    EXPECT_NEAR(mc::DisplayNc(model, commands[2].q).At("C"), ToRadians(200.0), kTol);
}

TEST(MotionTest, Overtravel_Policies) {
    const auto setup = MakeSetup();
    // 直接指令 (X=500 > 400) と、位置IK経由 (Yが-10 < 0) の両方
    const mc::ClProgram program = WithTool({
            Words(mc::NcValues{{"X", 500.0}}, mc::MotionFrame::kWork, mc::MotionKind::kRapid),
            Goto(Vector3d(0.0, -190.0, 100.0), Vector3d::UnitZ())});
    mc::MotionOptions options;
    options.overtravel = mc::OvertravelPolicy::kWarning;
    const mc::MotionTrack warned = mc::PlanMotion(setup, program, options);
    EXPECT_EQ(warned.stats.overtravel_count, 2u);
    EXPECT_EQ(CountWarnings(warned.warnings, "out of range"), 1u);
    EXPECT_EQ(CountWarnings(warned.warnings, "out of stroke"), 1u);
    EXPECT_EQ(warned.stats.unreachable_count, 0u);

    options.overtravel = mc::OvertravelPolicy::kIgnore;
    const mc::MotionTrack ignored = mc::PlanMotion(setup, program, options);
    EXPECT_EQ(ignored.stats.overtravel_count, 0u);
    EXPECT_EQ(CountWarnings(ignored.warnings, "out of"), 0u);
}

TEST(MotionTest, Overtravel_ThrowsKinematicsErrorWhenErrorPolicy) {
    const auto setup = MakeSetup();
    const mc::ClProgram program = WithTool({
            Words(mc::NcValues{{"X", 500.0}}, mc::MotionFrame::kWork, mc::MotionKind::kRapid)});
    mc::MotionOptions options;
    options.overtravel = mc::OvertravelPolicy::kError;
    EXPECT_THROW(mc::PlanMotion(setup, program, options), mc::KinematicsError);
    // 省略時はプロジェクトの [run].overtravel (デフォルト error)
    EXPECT_THROW(mc::PlanMotion(setup, program), mc::KinematicsError);
    // 可動範囲の端 (X=400) は例外にならない
    EXPECT_NO_THROW(mc::PlanMotion(setup, WithTool({Words(mc::NcValues{{"X", 400.0}})})));
}

TEST(MotionTest, Arc_IsDiscretized) {
    const auto setup = MakeSetup();
    const mc::MachineModel& model = setup.Model();
    const Vector3d tilted(std::sin(ToRadians(20.0)), 0.0, std::cos(ToRadians(20.0)));
    mc::ClArc arc;
    arc.kind = mc::MotionKind::kArcCcw;
    arc.end = Vector3d(-10.0, 0.0, 0.0);
    arc.center = Vector3d::Zero();
    arc.normal = Vector3d::UnitZ();
    arc.tool_axis = tilted;
    const mc::ClProgram program = WithTool({
            Goto(Vector3d(10.0, 0.0, 0.0), Vector3d::UnitZ(), mc::MotionKind::kRapid), arc});
    mc::MotionOptions options;
    options.interpolate = false;
    const mc::MotionTrack track = Plan(setup, program, options);
    const std::size_t begin = track.record_first_sample[2];
    ASSERT_GT(track.samples.size() - begin, 3u);
    const Vector3d control = ControlHome(model);
    for (std::size_t i = begin; i < track.samples.size(); ++i) {
        const mc::MotionSample& sample = track.samples[i];
        EXPECT_EQ(sample.record_index, 2u);
        EXPECT_EQ(sample.kind, mc::MotionKind::kArcCcw);
        EXPECT_EQ(sample.is_command_point, i + 1 == track.samples.size());
        // 通過点は半径10の円上、y ≥ 0 (反時計回りの半円)
        const Vector3d p = ControlPointHome(model, sample.q, control);
        EXPECT_NEAR(p.norm(), 10.0, kIkTol);
        EXPECT_GE(p.y(), -kIkTol);
    }
    EXPECT_LT(ToolAxisWork(model, track.samples.back().q).cross(tilted).norm(), kIkTol);
    EXPECT_EQ(track.stats.command_point_count, 2u);
}

TEST(MotionTest, Arc_WithoutStartIsSkipped) {
    const auto setup = MakeSetup();
    mc::ClArc arc;
    arc.end = Vector3d(-10.0, 0.0, 0.0);
    const mc::MotionTrack track = Plan(setup, WithTool({arc}));
    EXPECT_EQ(CountWarnings(track.warnings, "start point"), 1u);
    EXPECT_EQ(track.stats.command_point_count, 0u);
}

TEST(MotionTest, ControlLocal_G43AndTip) {
    const std::string offsets = "\n[[tool_offset]]\nnumber = 1\ntool = 1\nlength_wear = -0.02\n";
    const auto gauge = MakeSetup(Replace(MinimalProject(), "[tool.simple]",
                                         "control_point = \"gauge\"\n\n[tool.simple]")
                                 + offsets);
    const mc::ClProgram program = WithTool({
            mc::ClLengthOffset{1},
            Goto(Vector3d(0.0, 0.0, 0.0), Vector3d::UnitZ(), mc::MotionKind::kRapid)});
    const mc::MotionTrack gauge_track = Plan(gauge, program);
    // ゲージ点を工具長補正 (89.98) だけ上に置く: 250.5 + Z = 89.98
    EXPECT_NEAR(NcOf(gauge.Model(), CommandSamples(gauge_track)[0].q, "Z"),
                89.98 - 250.5, kIkTol);

    const auto tip = MakeSetup(MinimalProject() + offsets);
    const mc::MotionTrack tip_track = Plan(tip, program);
    EXPECT_NEAR(NcOf(tip.Model(), CommandSamples(tip_track)[0].q, "Z"),
                -kTipZ - 250.5, kIkTol);

    const mc::MotionTrack unknown = Plan(
            tip, WithTool({mc::ClLengthOffset{7},
                           Goto(Vector3d(0.0, 0.0, 0.0), Vector3d::UnitZ(),
                                mc::MotionKind::kRapid)}));
    EXPECT_EQ(CountWarnings(unknown.warnings, "length offset #7"), 1u);
}

TEST(MotionTest, Tool_ChangeAndNoTool) {
    const auto setup = MakeSetup();
    const mc::ClProgram program = Program({
            mc::ClLoadTool{1},
            Goto(Vector3d(0.0, 0.0, 0.0), Vector3d::UnitZ(), mc::MotionKind::kRapid),
            mc::ClLoadTool{mc::kNoTool},
            Goto(Vector3d(10.0, 0.0, 0.0)),
            Goto(Vector3d(20.0, 0.0, 0.0))});
    const mc::MotionTrack track = Plan(setup, program);
    const auto commands = CommandSamples(track);
    ASSERT_EQ(commands.size(), 3u);
    EXPECT_EQ(commands[0].tool_number, 1);
    EXPECT_EQ(commands[1].tool_number, mc::kNoTool);
    EXPECT_EQ(commands[2].tool_number, mc::kNoTool);
    EXPECT_EQ(CountWarnings(track.warnings, "no tool"), 1u);

    // ClLoadTool が無ければ初期工具 (#1)
    const mc::MotionTrack initial = Plan(
            setup, Program({Goto(Vector3d(0.0, 0.0, 0.0), Vector3d::UnitZ())}));
    EXPECT_EQ(CommandSamples(initial)[0].tool_number, 1);
    EXPECT_EQ(CountWarnings(initial.warnings, "no tool"), 0u);
}

TEST(MotionTest, Check_Every100Targets) {
    const auto setup = MakeSetup();
    std::vector<mc::ClRecord> records;
    for (int i = 0; i < 101; ++i) {
        records.push_back(Goto(Vector3d(static_cast<double>(i) * 0.1, 0.0, 0.0),
                               Vector3d::UnitZ()));
    }
    mc::MotionOptions options;
    options.interpolate = false;
    const mc::MotionTrack track = Plan(setup, WithTool(records), options);
    EXPECT_EQ(track.stats.check_count, 2u);
    EXPECT_LT(track.stats.max_angle_error, kIkTol);
    EXPECT_LT(track.stats.max_position_error, kIkTol);
}



/**
 * ---- 補助関数 ----
 */

TEST(MotionTest, SampleIndexAtTime_Bisects) {
    mc::MotionTrack track;
    for (const double time : {0.0, 1.0, 2.0}) {
        mc::MotionSample sample;
        sample.time = time;
        track.samples.push_back(sample);
    }
    EXPECT_EQ(mc::SampleIndexAtTime(track, -1.0), 0u);
    EXPECT_EQ(mc::SampleIndexAtTime(track, 0.0), 0u);
    EXPECT_EQ(mc::SampleIndexAtTime(track, 0.5), 0u);
    EXPECT_EQ(mc::SampleIndexAtTime(track, 1.0), 1u);
    EXPECT_EQ(mc::SampleIndexAtTime(track, 2.5), 2u);
    EXPECT_EQ(mc::SampleIndexAtTime(mc::MotionTrack{}, 1.0), 0u);
}

TEST(MotionTest, DisplayNc_WrapsUnlimitedOnly) {
    const auto setup = MakeSetup();
    const mc::MachineModel& model = setup.Model();
    const mc::JointVector q = mc::JointsFromNc(
            model, mc::NcValues{{"C", ToRadians(370.0)}, {"A", ToRadians(-30.0)}},
            setup.BaseQ());
    const mc::NcValues display = mc::DisplayNc(model, q);
    EXPECT_NEAR(display.At("C"), ToRadians(10.0), kTol);
    EXPECT_NEAR(display.At("A"), ToRadians(-30.0), kTol);
    EXPECT_NEAR(display.At("Z"), 100.0, kTol);
}

TEST(MotionTest, Options_ThrowsInvalidArgument) {
    const auto setup = MakeSetup();
    const mc::ClProgram program = WithTool({Goto(Vector3d::Zero(), Vector3d::UnitZ())});
    mc::MotionOptions options;
    options.fps = 0.0;
    EXPECT_THROW(Plan(setup, program, options), std::invalid_argument);
    options = {};
    options.max_samples = 0;
    EXPECT_THROW(Plan(setup, program, options), std::invalid_argument);
    options = {};
    options.arc_chord_tolerance = 0.0;
    EXPECT_THROW(Plan(setup, program, options), std::invalid_argument);
    options = {};
    options.fallback_feed = -1.0;
    EXPECT_THROW(Plan(setup, program, options), std::invalid_argument);
    options = {};
    options.max_samples = 1;
    EXPECT_NO_THROW(Plan(setup, program, options));
}



/**
 * ---- 実機NC ----
 */

TEST(MotionTest, RealNc_EndToEnd) {
    const auto setup = MakeSetup();
    mc::NcInterpretOptions interpret;
    interpret.dialect = mc::DefaultFanucDialect();
    const mc::ClProgram program =
            mc::InterpretNcFile(kNcDir / "tcp_ijk.nc", {}, interpret, nullptr);
    mc::MotionOptions options;
    options.overtravel = mc::OvertravelPolicy::kWarning;
    const mc::MotionTrack track = mc::PlanMotion(setup, program, options);
    EXPECT_EQ(track.stats.unreachable_count, 0u);
    EXPECT_EQ(CountWarnings(track.warnings, "unreachable"), 0u);
    // 工具#12と補正番号12はプロジェクトに無く、B軸は機械に無い (いずれも1件)
    EXPECT_EQ(CountWarnings(track.warnings, "tool #12"), 1u);
    EXPECT_EQ(CountWarnings(track.warnings, "length offset #12"), 1u);
    EXPECT_EQ(CountWarnings(track.warnings, "axis 'B'"), 1u);
    ASSERT_EQ(track.record_first_sample.size(), program.records.size());
    for (std::size_t i = 1; i < track.record_first_sample.size(); ++i) {
        EXPECT_GE(track.record_first_sample[i], track.record_first_sample[i - 1]);
    }
    std::size_t motions = 0;
    for (const mc::ClRecord& record : program.records) motions += mc::IsMotion(record);
    EXPECT_EQ(track.stats.motion_record_count, motions);
    EXPECT_EQ(track.stats.sample_count, track.samples.size());
    EXPECT_EQ(track.stats.command_point_count, CommandSamples(track).size());
    EXPECT_GT(track.stats.duration_sec, 0.0);
    for (std::size_t i = 1; i < track.samples.size(); ++i) {
        EXPECT_GE(track.samples[i].time, track.samples[i - 1].time);
    }
}
