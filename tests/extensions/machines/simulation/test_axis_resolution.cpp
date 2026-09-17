/**
 * @file tests/extensions/machines/simulation/test_axis_resolution.cpp
 * @brief 工具軸方向から回転軸の指令への変換 (simulation/axis_resolution) のテスト
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note 対象: ResolveAxisWords / SolveClTarget
 *       - 正常系: 回転軸の指令の書き込みと姿勢IKとの一致、`keep_tool_axis`,
 *         回転角の解の連続性、直前の指令値の引き継ぎ、G43.4形式での出力,
 *         1点の解と動作生成のサンプルの一致、工具長補正、無制限回転軸の回転方向,
 *         特異姿勢
 *       - 正常系 (退化): 対象外のレコード (機械座標、制御点なし、回転軸の指令あり)
 *         は変更しない、工具軸方向の無いプログラム (+Zの仮定)、工具なしと
 *         工具表に無い番号 (ゲージライン)
 *       - 警告: 到達不能で直前の回転軸の指令を書く、1点の解の到達不能 (`nullopt`)
 *       - 異常系: 未定義のワークオフセット、ゼロベクトルの工具軸方向、軸数と異なる
 *         `prev_q` (`std::invalid_argument`)
 *       TODO: 対応しない軸構成の`NotImplementedError`は逆運動学側で検証済み
 */
#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/machine/axis_values.h"
#include "igesio/extensions/machines/machine/forward_kinematics.h"
#include "igesio/extensions/machines/machine/inverse_kinematics.h"
#include "igesio/extensions/machines/project/setup.h"
#include "igesio/extensions/machines/simulation/axis_resolution.h"
#include "igesio/extensions/machines/simulation/motion.h"
#include "igesio/extensions/machines/tools/tool_assembly.h"
#include "igesio/extensions/machines/toolpath/cl_program.h"
#include "igesio/extensions/machines/toolpath/nc_dialect.h"
#include "igesio/extensions/machines/toolpath/nc_writer.h"
#include "../machine/machines_for_testing.h"
#include "../project/projects_for_testing.h"
#include "motion_for_testing.h"

namespace {

namespace mc = igesio::extensions::machines;
using igesio::Vector3d;
using mc::ToRadians;
using projects_test::MinimalProject;
using projects_test::ReadProjectText;
using projects_test::Replace;

/// @brief 逆運動学を経た値の比較の許容誤差
constexpr double kIkTol = 1e-9;

/// @brief 実例機の最小構成プロジェクトからセットアップを作る
mc::MachiningSetup MakeSetup(const std::string& toml = MinimalProject()) {
    return mc::MachiningSetup(ReadProjectText(toml));
}

/// @brief 制御点と工具軸方向を持つ切削の移動を作る
mc::ClGoto Goto(const Vector3d& point,
                const std::optional<Vector3d>& axis = std::nullopt) {
    mc::ClGoto motion;
    motion.point = point;
    motion.tool_axis = axis;
    return motion;
}

/// @brief 工具#1とG54を選択してから始まるプログラムを作る
mc::ClProgram WithHead(std::vector<mc::ClRecord> records) {
    records.insert(records.begin(), mc::ClSelectWorkOffset{"G54"});
    records.insert(records.begin(), mc::ClLoadTool{1});
    mc::ClProgram program;
    program.records = std::move(records);
    return program;
}

/// @brief 傾斜30°・方位角`azimuth`の工具軸方向を作る
Vector3d Tilted(const double azimuth_deg) {
    return Vector3d(std::sin(ToRadians(30.0)) * std::cos(ToRadians(azimuth_deg)),
                    std::sin(ToRadians(30.0)) * std::sin(ToRadians(azimuth_deg)),
                    std::cos(ToRadians(30.0)));
}

/// @brief 2πの周期を除いて角度が一致することを検証する (無制限軸は回転方向を正規化する)
void ExpectSameAngle(const double actual, const double expected) {
    EXPECT_NEAR(std::remainder(actual - expected, mc::kFullTurn), 0.0, kIkTol)
            << "actual: " << actual << ", expected: " << expected;
}

/// @brief 指定した型のレコードを順に集める
template <class T>
std::vector<const T*> Records(const mc::ClProgram& program) {
    std::vector<const T*> found;
    for (const mc::ClRecord& record : program.records) {
        if (const T* value = std::get_if<T>(&record)) found.push_back(value);
    }
    return found;
}

/// @brief 工具#1で高さ200 mmの点に工具軸方向`axis`で置く`ClTarget`を作る
/// @note 実例機の可動範囲 (Z ≥ -90 mm) に収まる高さにする
mc::ClTarget TargetAt(const Vector3d& axis,
                      const Vector3d& point = Vector3d(0.0, 0.0, 200.0)) {
    mc::ClTarget target;
    target.point = point;
    target.tool_axis = axis;
    target.tool = 1;
    return target;
}

/// @brief 2つの軸変位量が許容誤差内で一致することを検証する
void ExpectSameJoints(const mc::JointVector& actual, const mc::JointVector& expected) {
    ASSERT_EQ(actual.Size(), expected.Size());
    for (std::size_t i = 0; i < actual.Size(); ++i) {
        EXPECT_NEAR(actual[i], expected[i], kIkTol) << "axis " << i;
    }
}

}  // namespace



TEST(AxisResolutionTest, Resolve_WritesRotaryWords) {
    const auto setup = MakeSetup();
    const mc::MachineModel& model = setup.Model();
    const Vector3d axis = Tilted(20.0);
    const mc::ClProgram program = WithHead({Goto(Vector3d(0.0, 0.0, 0.0), axis)});
    std::vector<mc::Diagnostic> warnings;
    const mc::ClProgram resolved = mc::ResolveAxisWords(setup, program, {}, &warnings);
    EXPECT_TRUE(warnings.empty());
    const auto gotos = Records<mc::ClGoto>(resolved);
    ASSERT_EQ(gotos.size(), 1u);
    // G54はW_0 = Iなので、姿勢IKの入力はワーク座標の方向そのもの
    const mc::IkSolution expected = mc::SolveOrientation(
            model, axis, mc::NcFromJoints(model, setup.BaseQ()), mc::BranchPolicy::kPositive);
    EXPECT_NEAR(gotos[0]->axis_words.At("A"), expected.nc.At("A"), kIkTol);
    ExpectSameAngle(gotos[0]->axis_words.At("C"), expected.nc.At("C"));
    EXPECT_TRUE(gotos[0]->tool_axis.has_value());

    mc::AxisResolutionOptions options;
    options.keep_tool_axis = false;
    const mc::ClProgram stripped = mc::ResolveAxisWords(setup, program, options);
    EXPECT_FALSE(Records<mc::ClGoto>(stripped)[0]->tool_axis.has_value());
    EXPECT_TRUE(Records<mc::ClGoto>(stripped)[0]->axis_words.Contains("A"));
}

TEST(AxisResolutionTest, Resolve_ContinuesPreviousAxisWords) {
    const auto setup = MakeSetup();
    const mc::ClProgram program = WithHead({
            Goto(Vector3d(0.0, 0.0, 0.0), Tilted(20.0)),
            Goto(Vector3d(10.0, 0.0, 0.0))});
    const mc::ClProgram resolved = mc::ResolveAxisWords(setup, program);
    const auto gotos = Records<mc::ClGoto>(resolved);
    ASSERT_EQ(gotos.size(), 2u);
    // 工具軸方向を省略した移動は直前の工具軸方向を引き継ぎ、同じ指令になる
    EXPECT_NEAR(gotos[1]->axis_words.At("A"), gotos[0]->axis_words.At("A"), kIkTol);
    EXPECT_NEAR(gotos[1]->axis_words.At("C"), gotos[0]->axis_words.At("C"), kIkTol);
}

TEST(AxisResolutionTest, Resolve_LeavesOtherRecordsUnchanged) {
    const auto setup = MakeSetup();
    mc::ClGoto machine;
    machine.frame = mc::MotionFrame::kMachine;
    machine.axis_words = mc::NcValues{{"Z", 0.0}};
    mc::ClGoto words;
    words.axis_words = mc::NcValues{{"X", 1.0}};
    mc::ClGoto rotary = Goto(Vector3d(0.0, 0.0, 0.0));
    rotary.axis_words = mc::NcValues{{"A", ToRadians(10.0)}, {"C", 0.0}};
    const mc::ClProgram program = WithHead({machine, words, rotary});
    const mc::ClProgram resolved = mc::ResolveAxisWords(setup, program);
    const auto gotos = Records<mc::ClGoto>(resolved);
    ASSERT_EQ(gotos.size(), 3u);
    EXPECT_FALSE(gotos[0]->axis_words.Contains("A"));
    EXPECT_FALSE(gotos[1]->axis_words.Contains("A"));
    EXPECT_NEAR(gotos[2]->axis_words.At("A"), ToRadians(10.0), kIkTol);
}

TEST(AxisResolutionTest, Resolve_AssumesZWhenNoToolAxis) {
    const auto setup = MakeSetup();
    const mc::ClProgram program = WithHead({
            Goto(Vector3d(0.0, 0.0, 0.0)), Goto(Vector3d(1.0, 0.0, 0.0))});
    std::vector<mc::Diagnostic> warnings;
    const mc::ClProgram resolved = mc::ResolveAxisWords(setup, program, {}, &warnings);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_NE(warnings[0].message.find("assumed"), std::string::npos);
    for (const mc::ClGoto* motion : Records<mc::ClGoto>(resolved)) {
        EXPECT_NEAR(motion->axis_words.At("A"), 0.0, kIkTol);
        EXPECT_NEAR(motion->axis_words.At("C"), 0.0, kIkTol);
    }
}

TEST(AxisResolutionTest, Resolve_ContinuousBranch) {
    const auto setup = MakeSetup(Replace(MinimalProject(), "Z = 100.0", "Z = 100.0\nA = -20.0"));
    std::vector<mc::ClRecord> records;
    for (const double azimuth : {0.0, 20.0, 40.0, 20.0, 0.0, -20.0}) {
        records.push_back(Goto(Vector3d(0.0, 0.0, 0.0), Tilted(azimuth)));
    }
    mc::AxisResolutionOptions options;
    options.branch = mc::BranchPolicy::kContinuous;
    const mc::ClProgram resolved = mc::ResolveAxisWords(setup, WithHead(records), options);
    // 初期姿勢A=-20°に近い負側の候補から始まり、往復しても符号が反転しない
    for (const mc::ClGoto* motion : Records<mc::ClGoto>(resolved)) {
        EXPECT_LT(motion->axis_words.At("A"), 0.0);
    }
}

TEST(AxisResolutionTest, Resolve_UnreachableKeepsPrevious) {
    // 回転軸の無い3軸機では、傾いた工具軸方向は到達不能 (回転軸の指令は無いまま)
    std::string body = Replace(MinimalProject(),
                               "[machine]\nlibrary = \"t-ZYX-b-AC-w.toml\"\n", "");
    body = Replace(body, "values = { X = 0.0, Y = 180.0, Z = -250.5 }",
                   "values = { X = 0.0, Y = 0.0, Z = -100.0 }");
    const mc::MachiningSetup setup(projects_test::ReadProjectWithMachine(
            machines_test::ThreeAxis(), body, "igesio_axis_resolution_three_axis"));
    const mc::ClProgram program = WithHead({
            Goto(Vector3d(0.0, 0.0, 0.0), Vector3d::UnitZ()),
            Goto(Vector3d(0.0, 0.0, 0.0), Tilted(0.0))});
    std::vector<mc::Diagnostic> warnings;
    const mc::ClProgram resolved = mc::ResolveAxisWords(setup, program, {}, &warnings);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_NE(warnings[0].message.find("unreachable"), std::string::npos);
    EXPECT_EQ(warnings[0].context, "record 3");
    const auto gotos = Records<mc::ClGoto>(resolved);
    ASSERT_EQ(gotos.size(), 2u);
    EXPECT_TRUE(gotos[0]->axis_words.Empty());
    EXPECT_TRUE(gotos[1]->axis_words.Empty());
}

TEST(AxisResolutionTest, Resolve_ThenWriteRotaryWords) {
    const auto setup = MakeSetup();
    const mc::ClProgram program = WithHead({
            mc::ClLengthOffset{1},
            Goto(Vector3d(0.0, 0.0, 10.0), Tilted(0.0)),
            Goto(Vector3d(10.0, 0.0, 10.0), Tilted(30.0)),
            mc::ClEnd{}});
    const mc::ClProgram resolved = mc::ResolveAxisWords(setup, program);
    mc::NcWriteOptions options;
    options.tcp = mc::TcpStyle::kRotaryWords;
    options.write_header_footer = false;
    std::vector<mc::Diagnostic> warnings;
    std::string text;
    ASSERT_NO_THROW(text = mc::WriteNcToString(resolved, mc::DefaultFanucDialect(),
                                               options, &warnings));
    EXPECT_NE(text.find("G43.4"), std::string::npos);
    EXPECT_NE(text.find("A"), std::string::npos);
    EXPECT_NE(text.find("C"), std::string::npos);
    EXPECT_EQ(text.find("I0"), std::string::npos);
}



/**
 * ---- SolveClTarget ----
 */

TEST(AxisResolutionTest, SolveClTarget_MatchesPlanMotion) {
    const auto setup = MakeSetup();
    const mc::ClTarget target = TargetAt(Tilted(20.0));
    // 同じ制御点と工具軸方向を1レコードのプログラムにして動作生成した終点と一致する
    const mc::ClGoto motion = Goto(target.point, target.tool_axis);
    mc::MotionOptions options;
    options.interpolate = false;
    const mc::MotionTrack track =
            mc::PlanMotion(setup, WithHead({motion}), options);
    ASSERT_EQ(track.samples.size(), 2u);
    std::vector<mc::Diagnostic> warnings;
    const auto solution = mc::SolveClTarget(setup, target, setup.BaseQ(),
                                            std::nullopt, &warnings);
    ASSERT_TRUE(solution.has_value());
    EXPECT_TRUE(warnings.empty());
    ASSERT_TRUE(solution->q.has_value());
    ExpectSameJoints(*solution->q, track.samples.back().q);
    // `nc`は全軸、`error`は自己検証の結果
    EXPECT_EQ(solution->nc.Size(), setup.Model().Axes().size());
    ASSERT_TRUE(solution->error.has_value());
    EXPECT_LT(solution->error->angle, kIkTol);
    EXPECT_LT(solution->error->position, kIkTol);
    EXPECT_FALSE(solution->singular);

    // 工具軸方向が+Z (旋回軸と平行) なら特異姿勢. 診断は追加しない
    const auto singular = mc::SolveClTarget(setup, TargetAt(Vector3d::UnitZ()),
                                            setup.BaseQ(), std::nullopt, &warnings);
    ASSERT_TRUE(singular.has_value());
    EXPECT_TRUE(singular->singular);
    EXPECT_TRUE(warnings.empty());
    EXPECT_NEAR(singular->nc.At("A"), 0.0, kIkTol);
}

TEST(AxisResolutionTest, SolveClTarget_ControlPointFromToolAndG43) {
    const auto setup = MakeSetup();
    // 工具#1の先端 (ゲージ長90) と、工具なしのゲージライン、工具長補正50の比較.
    // W_0 = Iで工具軸は+Zなので、制御点が下がる分だけZが上がる
    mc::ClTarget tip = TargetAt(Vector3d::UnitZ());
    mc::ClTarget gauge = tip;
    gauge.tool = mc::kNoTool;
    mc::ClTarget compensated = gauge;
    compensated.g43_length = 50.0;
    std::vector<mc::Diagnostic> warnings;
    const auto at_tip = mc::SolveClTarget(setup, tip, setup.BaseQ());
    const auto at_gauge = mc::SolveClTarget(setup, gauge, setup.BaseQ(),
                                            std::nullopt, &warnings);
    const auto at_g43 = mc::SolveClTarget(setup, compensated, setup.BaseQ());
    ASSERT_TRUE(at_tip.has_value());
    ASSERT_TRUE(at_gauge.has_value());
    ASSERT_TRUE(at_g43.has_value());
    // 工具なし (`kNoTool`) は警告しない
    EXPECT_TRUE(warnings.empty());
    EXPECT_NEAR(at_tip->nc.At("Z") - at_gauge->nc.At("Z"), 90.0, kIkTol);
    EXPECT_NEAR(at_g43->nc.At("Z") - at_gauge->nc.At("Z"), 50.0, kIkTol);
}

TEST(AxisResolutionTest, SolveClTarget_UnresolvedToolWarnsAndUsesGaugeLine) {
    const auto setup = MakeSetup();
    mc::ClTarget unresolved = TargetAt(Vector3d::UnitZ());
    unresolved.tool = 9;
    mc::ClTarget gauge = unresolved;
    gauge.tool = mc::kNoTool;
    std::vector<mc::Diagnostic> warnings;
    const auto solution = mc::SolveClTarget(setup, unresolved, setup.BaseQ(),
                                            std::nullopt, &warnings);
    const auto expected = mc::SolveClTarget(setup, gauge, setup.BaseQ());
    ASSERT_TRUE(solution.has_value());
    ASSERT_TRUE(expected.has_value());
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_NE(warnings[0].message.find("tool #9"), std::string::npos);
    EXPECT_NE(warnings[0].message.find("gauge line"), std::string::npos);
    ExpectSameJoints(*solution->q, *expected->q);
}

TEST(AxisResolutionTest, SolveClTarget_ContinuesUnlimitedRotaryFromPrevious) {
    const auto setup = MakeSetup();
    const mc::MachineModel& model = setup.Model();
    const mc::ClTarget target = TargetAt(Tilted(20.0));
    const auto first = mc::SolveClTarget(setup, target, setup.BaseQ());
    ASSERT_TRUE(first.has_value());
    // 直前の姿勢のCを1回転進めておくと、同じ目標でも1回転進んだ側の解になる
    const double c0 = first->nc.At("C");
    const mc::JointVector turned = mc::JointsFromNc(
            model, mc::NcValues{{"C", c0 + mc::kFullTurn}}, *first->q);
    const auto second = mc::SolveClTarget(setup, target, turned);
    ASSERT_TRUE(second.has_value());
    EXPECT_NEAR(second->nc.At("C"), c0 + mc::kFullTurn, kIkTol);
    EXPECT_NEAR(second->nc.At("A"), first->nc.At("A"), kIkTol);
}

TEST(AxisResolutionTest, SolveClTarget_UnreachableReturnsNullopt) {
    // 回転軸の無い3軸機では、傾いた工具軸方向は到達不能
    const auto setup = motion_test::MakeSetupThreeAxis();
    std::vector<mc::Diagnostic> warnings;
    const auto solution = mc::SolveClTarget(setup, TargetAt(Tilted(0.0)),
                                            setup.BaseQ(), std::nullopt, &warnings);
    EXPECT_FALSE(solution.has_value());
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_NE(warnings[0].message.find("unreachable"), std::string::npos);
    // +Zなら到達できる
    EXPECT_TRUE(mc::SolveClTarget(setup, TargetAt(Vector3d::UnitZ()),
                                  setup.BaseQ()).has_value());
}

TEST(AxisResolutionTest, SolveClTarget_ThrowsInvalidArgumentOnBadInput) {
    const auto setup = MakeSetup();
    mc::ClTarget unknown_offset = TargetAt(Vector3d::UnitZ());
    unknown_offset.work_offset = "G99";
    EXPECT_THROW(mc::SolveClTarget(setup, unknown_offset, setup.BaseQ()),
                 std::invalid_argument);
    // 定義済みのidと空 (初期ワークオフセット) は受理する
    mc::ClTarget known_offset = TargetAt(Vector3d::UnitZ());
    known_offset.work_offset = "G54";
    EXPECT_NO_THROW(mc::SolveClTarget(setup, known_offset, setup.BaseQ()));
    EXPECT_THROW(mc::SolveClTarget(setup, TargetAt(Vector3d::Zero()), setup.BaseQ()),
                 std::invalid_argument);
    EXPECT_THROW(mc::SolveClTarget(setup, TargetAt(Vector3d::UnitZ()),
                                   mc::JointVector(2, 0.0)),
                 std::invalid_argument);
}
