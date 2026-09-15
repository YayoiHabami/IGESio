/**
 * @file tests/extensions/machines/toolpath/test_cl_program.cpp
 * @brief CLプログラムの公開データモデル (toolpath/cl_program) のテスト
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note 対象: ClState::Apply / IsMotion / ClRecordKindName / ValidateClProgram
 *       - 正常系 (代表値): 全レコード種別の`ClState`の更新、`axis_words`の`Merge`
 *         による累積、工具軸の継続 (`ClGoto`の省略・`ClArc`の終値)、主軸回転数の
 *         省略時の直前値、`kMachine`の`ClGoto`が何も更新しないこと (D121)
 *       - 正常系 (境界値・退化): 空のプログラムの検査、`sources`が空の検査,
 *         `ClEnd`の直後で終わるプログラム
 *       - 検査 (警告): `ClEnd`後のレコード (1件に集計)、`kMachine`で`point`あり,
 *         ゼロ法線の`ClArc`、`sources`の長さ不一致、`kArcCw`の`ClGoto`,
 *         `kLinear`の`ClArc`、行番号の転記
 *       TODO: 異常系 (例外) は該当なし (検査は例外にしない設計)
 */
#include <gtest/gtest.h>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/machine/axis_values.h"
#include "igesio/extensions/machines/tools/tool_assembly.h"
#include "igesio/extensions/machines/toolpath/cl_program.h"

namespace {

namespace mc = igesio::extensions::machines;
using igesio::Vector3d;

/// @brief 座標・方向の比較の許容誤差
constexpr double kTol = 1e-12;

/// @brief 制御点と工具軸を持つ直線移動を作る
/// @param point 制御点
/// @param axis 工具軸 (省略時は継続)
mc::ClGoto Goto(const Vector3d& point,
                const std::optional<Vector3d>& axis = std::nullopt) {
    mc::ClGoto motion;
    motion.kind = mc::MotionKind::kLinear;
    motion.point = point;
    motion.tool_axis = axis;
    return motion;
}

/// @brief 軸の指令のみの機械座標の移動 (G53相当) を作る
mc::ClGoto MachineGoto(const mc::NcValues& words) {
    mc::ClGoto motion;
    motion.kind = mc::MotionKind::kRapid;
    motion.frame = mc::MotionFrame::kMachine;
    motion.axis_words = words;
    return motion;
}

/// @brief 原点中心・XY平面・終点(0,10,0)の反時計回り円弧を作る
mc::ClArc QuarterArc(const std::optional<Vector3d>& axis = std::nullopt) {
    mc::ClArc arc;
    arc.kind = mc::MotionKind::kArcCcw;
    arc.end = Vector3d(0.0, 10.0, 0.0);
    arc.center = Vector3d::Zero();
    arc.normal = Vector3d::UnitZ();
    arc.tool_axis = axis;
    return arc;
}

/// @brief ベクトルの一致を検証する
void ExpectVector(const Vector3d& actual, const Vector3d& expected) {
    EXPECT_NEAR(actual.x(), expected.x(), kTol);
    EXPECT_NEAR(actual.y(), expected.y(), kTol);
    EXPECT_NEAR(actual.z(), expected.z(), kTol);
}

/// @brief 警告の文言に部分文字列を含むものがあるか
bool HasWarning(const std::vector<mc::Diagnostic>& warnings,
                const std::string_view text) {
    for (const mc::Diagnostic& w : warnings) {
        if (w.message.find(text) != std::string::npos) return true;
    }
    return false;
}

}  // namespace



// ---- ClState::Apply (状態レコード) ----

TEST(ClProgramTest, State_DefaultIsEmpty) {
    const mc::ClState state;
    EXPECT_EQ(state.tool, mc::kNoTool);
    EXPECT_TRUE(state.work_offset.empty());
    EXPECT_FALSE(state.feed.has_value());
    EXPECT_FALSE(state.length_offset.has_value());
    EXPECT_EQ(state.spindle.mode, mc::ClSpindle::Mode::kOff);
    EXPECT_FALSE(state.coolant);
    EXPECT_FALSE(state.position.has_value());
    EXPECT_FALSE(state.tool_axis.has_value());
    EXPECT_TRUE(state.axis_words.Empty());
    EXPECT_FALSE(state.ended);
}

TEST(ClProgramTest, State_AppliesEachStateRecord) {
    mc::ClState state;
    state.Apply(mc::ClLoadTool{3});
    state.Apply(mc::ClSelectWorkOffset{"G55"});
    state.Apply(mc::ClFeed{200.0 / 60.0});
    state.Apply(mc::ClLengthOffset{std::optional<int>(12)});
    state.Apply(mc::ClSpindle{mc::ClSpindle::Mode::kCw, 5000.0});
    state.Apply(mc::ClCoolant{true});
    EXPECT_EQ(state.tool, 3);
    EXPECT_EQ(state.work_offset, "G55");
    ASSERT_TRUE(state.feed.has_value());
    EXPECT_NEAR(*state.feed, 200.0 / 60.0, kTol);
    EXPECT_EQ(state.length_offset, std::optional<int>(12));
    EXPECT_EQ(state.spindle.mode, mc::ClSpindle::Mode::kCw);
    ASSERT_TRUE(state.spindle.rpm.has_value());
    EXPECT_NEAR(*state.spindle.rpm, 5000.0, kTol);
    EXPECT_TRUE(state.coolant);
    EXPECT_FALSE(state.ended);

    // 解除・停止・切
    state.Apply(mc::ClLengthOffset{std::nullopt});
    state.Apply(mc::ClCoolant{false});
    state.Apply(mc::ClEnd{});
    EXPECT_FALSE(state.length_offset.has_value());
    EXPECT_FALSE(state.coolant);
    EXPECT_TRUE(state.ended);
}

TEST(ClProgramTest, State_SpindleKeepsRpmWhenOmitted) {
    mc::ClState state;
    state.Apply(mc::ClSpindle{mc::ClSpindle::Mode::kCw, 3000.0});
    state.Apply(mc::ClSpindle{mc::ClSpindle::Mode::kOff, std::nullopt});
    EXPECT_EQ(state.spindle.mode, mc::ClSpindle::Mode::kOff);
    ASSERT_TRUE(state.spindle.rpm.has_value());
    EXPECT_NEAR(*state.spindle.rpm, 3000.0, kTol);
}

TEST(ClProgramTest, State_NonStateRecordsLeaveStateUnchanged) {
    mc::ClState state;
    state.Apply(mc::ClLoadTool{2});
    const mc::ClState before = state;
    state.Apply(mc::ClDwell{0.5});
    state.Apply(mc::ClComment{"c"});
    state.Apply(mc::ClMarker{mc::ClMarker::Kind::kPathBegin, "p"});
    state.Apply(mc::ClPassThrough{"nc", "M00"});
    EXPECT_EQ(state.tool, before.tool);
    EXPECT_EQ(state.feed, before.feed);
    EXPECT_FALSE(state.position.has_value());
    EXPECT_FALSE(state.ended);
}

// ---- ClState::Apply (動作レコード) ----

TEST(ClProgramTest, State_GotoUpdatesPositionAndMergesAxisWords) {
    mc::ClState state;
    mc::ClGoto first = Goto(Vector3d(1.0, 2.0, 3.0), Vector3d::UnitZ());
    first.axis_words = mc::NcValues{{"A", 0.1}, {"C", 0.2}};
    state.Apply(first);
    mc::ClGoto second = Goto(Vector3d(4.0, 5.0, 6.0));
    second.axis_words = mc::NcValues{{"C", 0.3}};
    state.Apply(second);

    ASSERT_TRUE(state.position.has_value());
    ExpectVector(*state.position, Vector3d(4.0, 5.0, 6.0));
    // Mergeで累積: Aは残り、Cは上書き
    EXPECT_EQ(state.axis_words.Size(), 2u);
    EXPECT_NEAR(state.axis_words.At("A"), 0.1, kTol);
    EXPECT_NEAR(state.axis_words.At("C"), 0.3, kTol);
}

TEST(ClProgramTest, State_ToolAxisContinuesWhenGotoOmitsIt) {
    mc::ClState state;
    const Vector3d axis = Vector3d(0.0, 1.0, 1.0).normalized();
    state.Apply(Goto(Vector3d(0.0, 0.0, 0.0), axis));
    state.Apply(Goto(Vector3d(1.0, 0.0, 0.0)));
    ASSERT_TRUE(state.tool_axis.has_value());
    ExpectVector(*state.tool_axis, axis);
}

TEST(ClProgramTest, State_ArcSetsEndAndFinalToolAxis) {
    mc::ClState state;
    state.Apply(Goto(Vector3d(10.0, 0.0, 0.0), Vector3d::UnitZ()));
    state.Apply(QuarterArc(Vector3d::UnitX()));
    ASSERT_TRUE(state.position.has_value());
    ExpectVector(*state.position, Vector3d(0.0, 10.0, 0.0));
    ASSERT_TRUE(state.tool_axis.has_value());
    ExpectVector(*state.tool_axis, Vector3d::UnitX());

    // 工具軸を持たない円弧は直前の工具軸を保つ
    state.Apply(QuarterArc());
    ExpectVector(*state.tool_axis, Vector3d::UnitX());
}

TEST(ClProgramTest, State_MachineFrameWordsNotMerged) {
    mc::ClState state;
    mc::ClGoto work = Goto(Vector3d(1.0, 1.0, 1.0), Vector3d::UnitZ());
    work.axis_words = mc::NcValues{{"Z", 5.0}};
    state.Apply(work);
    state.Apply(MachineGoto(mc::NcValues{{"Z", 0.0}, {"B", 0.0}}));

    // 機械座標の軸の指令は登録値相対の軸の指令と同じ表に格納しない (D121)
    EXPECT_EQ(state.axis_words.Size(), 1u);
    EXPECT_NEAR(state.axis_words.At("Z"), 5.0, kTol);
    EXPECT_FALSE(state.axis_words.Contains("B"));
    ASSERT_TRUE(state.position.has_value());
    ExpectVector(*state.position, Vector3d(1.0, 1.0, 1.0));
    ASSERT_TRUE(state.tool_axis.has_value());
    ExpectVector(*state.tool_axis, Vector3d::UnitZ());
}

// ---- IsMotion / ClRecordKindName ----

TEST(ClProgramTest, Helpers_IsMotion) {
    EXPECT_TRUE(mc::IsMotion(mc::ClGoto{}));
    EXPECT_TRUE(mc::IsMotion(mc::ClArc{}));
    EXPECT_TRUE(mc::IsMotion(mc::ClDwell{}));
    EXPECT_FALSE(mc::IsMotion(mc::ClFeed{}));
    EXPECT_FALSE(mc::IsMotion(mc::ClSpindle{}));
    EXPECT_FALSE(mc::IsMotion(mc::ClCoolant{}));
    EXPECT_FALSE(mc::IsMotion(mc::ClLoadTool{}));
    EXPECT_FALSE(mc::IsMotion(mc::ClSelectWorkOffset{}));
    EXPECT_FALSE(mc::IsMotion(mc::ClLengthOffset{}));
    EXPECT_FALSE(mc::IsMotion(mc::ClComment{}));
    EXPECT_FALSE(mc::IsMotion(mc::ClMarker{}));
    EXPECT_FALSE(mc::IsMotion(mc::ClPassThrough{}));
    EXPECT_FALSE(mc::IsMotion(mc::ClEnd{}));
}

TEST(ClProgramTest, Helpers_KindName) {
    EXPECT_EQ(mc::ClRecordKindName(mc::ClGoto{}), "goto");
    EXPECT_EQ(mc::ClRecordKindName(mc::ClArc{}), "arc");
    EXPECT_EQ(mc::ClRecordKindName(mc::ClDwell{}), "dwell");
    EXPECT_EQ(mc::ClRecordKindName(mc::ClFeed{}), "feed");
    EXPECT_EQ(mc::ClRecordKindName(mc::ClSpindle{}), "spindle");
    EXPECT_EQ(mc::ClRecordKindName(mc::ClCoolant{}), "coolant");
    EXPECT_EQ(mc::ClRecordKindName(mc::ClLoadTool{}), "load_tool");
    EXPECT_EQ(mc::ClRecordKindName(mc::ClSelectWorkOffset{}), "select_work_offset");
    EXPECT_EQ(mc::ClRecordKindName(mc::ClLengthOffset{}), "length_offset");
    EXPECT_EQ(mc::ClRecordKindName(mc::ClComment{}), "comment");
    EXPECT_EQ(mc::ClRecordKindName(mc::ClMarker{}), "marker");
    EXPECT_EQ(mc::ClRecordKindName(mc::ClPassThrough{}), "pass_through");
    EXPECT_EQ(mc::ClRecordKindName(mc::ClEnd{}), "end");
}

// ---- ValidateClProgram ----

TEST(ClProgramTest, Validate_EmptyProgramHasNoWarnings) {
    EXPECT_TRUE(mc::ValidateClProgram(mc::ClProgram{}).empty());
}

TEST(ClProgramTest, Validate_ConsistentProgramHasNoWarnings) {
    mc::ClProgram program;
    program.records = {mc::ClLoadTool{1}, Goto(Vector3d::Zero(), Vector3d::UnitZ()),
                       QuarterArc(), MachineGoto(mc::NcValues{{"Z", 0.0}}),
                       mc::ClEnd{}};
    EXPECT_TRUE(mc::ValidateClProgram(program).empty());
    // sourcesが空でも警告しない
    EXPECT_TRUE(program.sources.empty());
}

TEST(ClProgramTest, Validate_ReportsRecordsAfterEndOnce) {
    mc::ClProgram program;
    program.records = {mc::ClEnd{}, mc::ClComment{"a"}, mc::ClComment{"b"}};
    const auto warnings = mc::ValidateClProgram(program);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_TRUE(HasWarning(warnings, "2 record(s) after end"));
    EXPECT_EQ(warnings[0].severity, mc::Severity::kWarning);
    EXPECT_TRUE(warnings[0].context.empty());
}

TEST(ClProgramTest, Validate_ReportsMachineFrameGotoWithPoint) {
    mc::ClGoto motion = MachineGoto(mc::NcValues{{"Z", 0.0}});
    motion.point = Vector3d::Zero();
    mc::ClProgram program;
    program.records = {motion};
    const auto warnings = mc::ValidateClProgram(program);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_TRUE(HasWarning(warnings, "machine-frame goto must not have a point"));
}

TEST(ClProgramTest, Validate_ReportsZeroArcNormal) {
    mc::ClArc arc = QuarterArc();
    arc.normal = Vector3d::Zero();
    mc::ClProgram program;
    program.records = {arc};
    const auto warnings = mc::ValidateClProgram(program);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_TRUE(HasWarning(warnings, "arc normal is zero"));
}

TEST(ClProgramTest, Validate_ReportsWrongMotionKinds) {
    mc::ClGoto motion = Goto(Vector3d::Zero());
    motion.kind = mc::MotionKind::kArcCw;
    mc::ClArc arc = QuarterArc();
    arc.kind = mc::MotionKind::kLinear;
    mc::ClProgram program;
    program.records = {motion, arc};
    const auto warnings = mc::ValidateClProgram(program);
    ASSERT_EQ(warnings.size(), 2u);
    EXPECT_TRUE(HasWarning(warnings, "goto must be rapid or linear"));
    EXPECT_TRUE(HasWarning(warnings, "arc must be clockwise or counterclockwise"));
}

TEST(ClProgramTest, Validate_ReportsSourcesLengthMismatch) {
    mc::ClProgram program;
    program.records = {mc::ClComment{"a"}, mc::ClComment{"b"}};
    program.sources = {mc::SourceLocation{0, 1}};
    const auto warnings = mc::ValidateClProgram(program);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_TRUE(HasWarning(warnings, "sources has 1 entries for 2 records"));
    EXPECT_EQ(warnings[0].line, 0);
}

TEST(ClProgramTest, Validate_CopiesLineFromSources) {
    mc::ClArc arc = QuarterArc();
    arc.normal = Vector3d::Zero();
    mc::ClProgram program;
    program.records = {mc::ClComment{"a"}, arc};
    program.sources = {mc::SourceLocation{0, 10}, mc::SourceLocation{0, 42}};
    const auto warnings = mc::ValidateClProgram(program);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_EQ(warnings[0].line, 42);
    EXPECT_TRUE(HasWarning(warnings, "record 1:"));
}
