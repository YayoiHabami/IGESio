/**
 * @file tests/extensions/machines/toolpath/test_cl_transform.cpp
 * @brief CLプログラムの変換 (toolpath/cl_transform) のテスト
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note 対象: DiscretizeArc / ScaleLengths / LinearizeArcs / EnumeratePaths /
 *       InsertApproachRetract
 *       - 正常系 (代表値): 弦誤差による分割数 (D73の式)、通過点が円上、終点を含み
 *         始点を含まない、時計回りの向き、単位換算の対象 (座標・円弧・送り) と
 *         対象外 (工具軸、軸の指令)、円弧の折れ線化と工具軸のslerp、マーカーと
 *         早送りによる区間分割、アプローチ/リトラクトの挿入順と送りの比
 *       - 正常系 (境界値・退化): 全円 (`full_turns = 1`)、始点==終点で
 *         `full_turns = 0`、弦誤差が半径以上、半径0、始点不明の円弧は残す,
 *         空のプログラム、動作の無いプログラム、送り不明時は`ClFeed`を挿入しない
 *       - 異常系: 弦誤差が正でない・法線がゼロで`std::invalid_argument`
 *       - 警告: 制御点の無い区間、工具軸の無い区間でアプローチを省く
 */
#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/machine/axis_values.h"
#include "igesio/extensions/machines/toolpath/cl_program.h"
#include "igesio/extensions/machines/toolpath/cl_transform.h"

namespace {

namespace mc = igesio::extensions::machines;
using igesio::Vector3d;

/// @brief 座標の比較の許容誤差 [mm]
constexpr double kTol = 1e-9;
/// @brief 分割数の検証に用いる弦誤差 [mm]
constexpr double kChord = 0.05;
/// @brief 円弧の半径 [mm]
constexpr double kRadius = 10.0;

/// @brief 制御点 (と工具軸) を持つ切削の直線移動を作る
mc::ClGoto Linear(const Vector3d& point,
                  const std::optional<Vector3d>& axis = std::nullopt) {
    mc::ClGoto motion;
    motion.kind = mc::MotionKind::kLinear;
    motion.point = point;
    motion.tool_axis = axis;
    return motion;
}

/// @brief 制御点を持つ早送りを作る
mc::ClGoto Rapid(const Vector3d& point) {
    mc::ClGoto motion = Linear(point);
    motion.kind = mc::MotionKind::kRapid;
    return motion;
}

/// @brief 原点中心・XY平面・半径10の円弧を作る (始点は(10,0,0)を想定)
/// @param end 終点
/// @param kind 向き
/// @param full_turns 全周の数
mc::ClArc Arc(const Vector3d& end, const mc::MotionKind kind = mc::MotionKind::kArcCcw,
              const int full_turns = 0) {
    mc::ClArc arc;
    arc.kind = kind;
    arc.end = end;
    arc.center = Vector3d::Zero();
    arc.normal = Vector3d::UnitZ();
    arc.full_turns = full_turns;
    return arc;
}

/// @brief 始点 (10,0,0)
const Vector3d kStart(kRadius, 0.0, 0.0);
/// @brief 90°の終点 (0,10,0)
const Vector3d kQuarterEnd(0.0, kRadius, 0.0);

/// @brief D73の式による分割数
std::size_t ExpectedDivisions(const double sweep, const double radius,
                              const double chord) {
    const double step = chord >= radius ? mc::kQuarterTurn
                                        : 2.0 * std::acos(1.0 - chord / radius);
    return static_cast<std::size_t>(std::ceil(sweep / step));
}

/// @brief ベクトルの一致を検証する
void ExpectVector(const Vector3d& actual, const Vector3d& expected) {
    EXPECT_NEAR(actual.x(), expected.x(), kTol);
    EXPECT_NEAR(actual.y(), expected.y(), kTol);
    EXPECT_NEAR(actual.z(), expected.z(), kTol);
}

/// @brief `ClGoto`として取り出す (種別が違えばテスト失敗)
const mc::ClGoto& AsGoto(const mc::ClRecord& record) {
    const auto* motion = std::get_if<mc::ClGoto>(&record);
    EXPECT_NE(motion, nullptr) << "record is " << mc::ClRecordKindName(record);
    return *motion;
}

/// @brief `ClFeed`の送り [mm/s] を取り出す (種別が違えばテスト失敗)
double FeedOf(const mc::ClRecord& record) {
    const auto* feed = std::get_if<mc::ClFeed>(&record);
    EXPECT_NE(feed, nullptr) << "record is " << mc::ClRecordKindName(record);
    return feed == nullptr ? 0.0 : feed->mm_per_s;
}

/// @brief 警告の文言に部分文字列を含むものがあるか
bool HasWarning(const std::vector<mc::Diagnostic>& warnings,
                const std::string_view text) {
    for (const mc::Diagnostic& w : warnings) {
        if (w.message.find(text) != std::string::npos) return true;
    }
    return false;
}

/// @brief アプローチ/リトラクトのテスト用: 送り2 mm/s・工具軸+Zの直線2点の切削
/// @note レコード: ClFeed{2} / Linear(0,0,0; +Z) / Linear(10,0,0). sourcesは行1〜3
mc::ClProgram TwoPointCut() {
    mc::ClProgram program;
    program.records = {mc::ClFeed{2.0}, Linear(Vector3d::Zero(), Vector3d::UnitZ()),
                       Linear(Vector3d(10.0, 0.0, 0.0))};
    program.sources = {mc::SourceLocation{1, 1}, mc::SourceLocation{1, 2},
                       mc::SourceLocation{1, 3}};
    return program;
}

/// @brief 工具軸方向に5 mm退避し、送りを半分にする指定
mc::ApproachRetractSpec ToolAxisSpec() {
    mc::ApproachRetractSpec spec;
    spec.direction = mc::ApproachRetractSpec::Direction::kToolAxis;
    spec.distance = 5.0;
    spec.feed_ratio = 0.5;
    return spec;
}

}  // namespace



// ---- DiscretizeArc ----

TEST(ClTransformTest, Arc_QuarterDivisionCountMatchesFormula) {
    const auto points = mc::DiscretizeArc(kStart, Arc(kQuarterEnd), kChord);
    EXPECT_EQ(points.size(), ExpectedDivisions(mc::kQuarterTurn, kRadius, kChord));
    EXPECT_EQ(points.size(), 8u);
}

TEST(ClTransformTest, Arc_PointsLieOnCircleAndEndIsExact) {
    const auto points = mc::DiscretizeArc(kStart, Arc(kQuarterEnd), kChord);
    ASSERT_EQ(points.size(), 8u);
    for (const Vector3d& p : points) EXPECT_NEAR(p.norm(), kRadius, kTol);
    // 始点を含まず、等角なので最初の点は 90°/8 の位置
    const double first_angle = mc::kQuarterTurn / 8.0;
    ExpectVector(points.front(), Vector3d(kRadius * std::cos(first_angle),
                                          kRadius * std::sin(first_angle), 0.0));
    EXPECT_GT((points.front() - kStart).norm(), 1.0);
    ExpectVector(points.back(), kQuarterEnd);
}

TEST(ClTransformTest, Arc_ClockwiseSweepsTheLongWay) {
    // 同じ始点・終点でも時計回りは270°を回る
    const auto points = mc::DiscretizeArc(
            kStart, Arc(kQuarterEnd, mc::MotionKind::kArcCw), kChord);
    EXPECT_EQ(points.size(),
              ExpectedDivisions(3.0 * mc::kQuarterTurn, kRadius, kChord));
    EXPECT_LT(points.front().y(), 0.0);
    ExpectVector(points.back(), kQuarterEnd);
}

TEST(ClTransformTest, Arc_FullCircleSweepsTwoPi) {
    const auto points = mc::DiscretizeArc(
            kStart, Arc(kStart, mc::MotionKind::kArcCcw, 1), kChord);
    const std::size_t count = ExpectedDivisions(mc::kFullTurn, kRadius, kChord);
    ASSERT_EQ(points.size(), count);
    // 半周の点は始点の反対側
    ExpectVector(points[count / 2 - 1], Vector3d(-kRadius, 0.0, 0.0));
    ExpectVector(points.back(), kStart);
}

TEST(ClTransformTest, Arc_ZeroSweepGivesEndOnly) {
    const auto points = mc::DiscretizeArc(kStart, Arc(kStart), kChord);
    ASSERT_EQ(points.size(), 1u);
    ExpectVector(points.front(), kStart);
}

TEST(ClTransformTest, Arc_LargeToleranceUsesQuarterTurnStep) {
    // ε ≥ r なら ⌈2θ/π⌉: 180°で2分割
    const auto points = mc::DiscretizeArc(
            kStart, Arc(Vector3d(-kRadius, 0.0, 0.0)), kRadius);
    ASSERT_EQ(points.size(), 2u);
    ExpectVector(points.front(), kQuarterEnd);
    ExpectVector(points.back(), Vector3d(-kRadius, 0.0, 0.0));
    // 境界: ε = r で同じ、ε = r より少し小さいと式が切り替わって細かくなる
    EXPECT_EQ(mc::DiscretizeArc(kStart, Arc(kQuarterEnd), kRadius).size(), 1u);
    EXPECT_GE(mc::DiscretizeArc(kStart, Arc(kQuarterEnd), kRadius - 1e-3).size(), 1u);
}

TEST(ClTransformTest, Arc_ZeroRadiusGivesEndOnly) {
    const auto points = mc::DiscretizeArc(Vector3d::Zero(), Arc(kQuarterEnd), kChord);
    ASSERT_EQ(points.size(), 1u);
    ExpectVector(points.front(), kQuarterEnd);
}

TEST(ClTransformTest, Arc_ThrowsInvalidArgumentWhenToleranceIsNotPositive) {
    EXPECT_THROW(mc::DiscretizeArc(kStart, Arc(kQuarterEnd), 0.0),
                 std::invalid_argument);
    EXPECT_THROW(mc::DiscretizeArc(kStart, Arc(kQuarterEnd), -kChord),
                 std::invalid_argument);
    EXPECT_NO_THROW(mc::DiscretizeArc(kStart, Arc(kQuarterEnd), 1e-6));
}

TEST(ClTransformTest, Arc_ThrowsInvalidArgumentWhenNormalIsZero) {
    mc::ClArc arc = Arc(kQuarterEnd);
    arc.normal = Vector3d::Zero();
    EXPECT_THROW(mc::DiscretizeArc(kStart, arc, kChord), std::invalid_argument);
    // 正規化されていない法線は許容する
    arc.normal = Vector3d(0.0, 0.0, 2.0);
    EXPECT_NO_THROW(mc::DiscretizeArc(kStart, arc, kChord));
}

// ---- ScaleLengths ----

TEST(ClTransformTest, ScaleLengths_AppliesToPointsArcsAndFeed) {
    mc::ClProgram program;
    program.records = {Linear(Vector3d(1.0, 2.0, 3.0)), Arc(kQuarterEnd),
                       mc::ClFeed{10.0}};
    std::get<mc::ClArc>(program.records[1]).center = Vector3d(1.0, 0.0, 0.0);
    mc::ScaleLengths(program, mc::kInchToMillimeter);

    ExpectVector(*AsGoto(program.records[0]).point,
                 Vector3d(25.4, 50.8, 76.2));
    const auto& arc = std::get<mc::ClArc>(program.records[1]);
    ExpectVector(arc.end, Vector3d(0.0, 254.0, 0.0));
    ExpectVector(arc.center, Vector3d(25.4, 0.0, 0.0));
    EXPECT_NEAR(FeedOf(program.records[2]), 254.0, kTol);
}

TEST(ClTransformTest, ScaleLengths_LeavesToolAxisAndAxisWords) {
    mc::ClGoto motion = Linear(Vector3d(1.0, 0.0, 0.0),
                               Vector3d(0.0, 1.0, 1.0).normalized());
    motion.axis_words = mc::NcValues{{"X", 3.0}, {"C", 0.5}};
    mc::ClProgram program;
    program.records = {motion};
    mc::ScaleLengths(program, 2.0);

    const mc::ClGoto& scaled = AsGoto(program.records[0]);
    ExpectVector(*scaled.tool_axis, Vector3d(0.0, 1.0, 1.0).normalized());
    EXPECT_NEAR(scaled.axis_words.At("X"), 3.0, kTol);
    EXPECT_NEAR(scaled.axis_words.At("C"), 0.5, kTol);
}

// ---- LinearizeArcs ----

TEST(ClTransformTest, LinearizeArcs_ReplacesArcWithGotosAndKeepsSources) {
    mc::ClProgram program;
    program.records = {Linear(kStart, Vector3d::UnitZ()), Arc(kQuarterEnd)};
    program.sources = {mc::SourceLocation{0, 5}, mc::SourceLocation{0, 6}};
    mc::LinearizeArcs(program, kChord);

    ASSERT_EQ(program.records.size(), 1u + 8u);
    ASSERT_EQ(program.sources.size(), program.records.size());
    for (std::size_t i = 1; i < program.records.size(); ++i) {
        const mc::ClGoto& motion = AsGoto(program.records[i]);
        EXPECT_EQ(motion.kind, mc::MotionKind::kLinear);
        EXPECT_NEAR(motion.point->norm(), kRadius, kTol);
        EXPECT_EQ(program.sources[i].line, 6);
    }
    ExpectVector(*AsGoto(program.records.back()).point, kQuarterEnd);
}

TEST(ClTransformTest, LinearizeArcs_SlerpsToolAxis) {
    mc::ClArc arc = Arc(kQuarterEnd);
    arc.tool_axis = Vector3d::UnitX();
    mc::ClProgram program;
    program.records = {Linear(kStart, Vector3d::UnitZ()), arc};
    mc::LinearizeArcs(program, kChord);

    ASSERT_EQ(program.records.size(), 9u);
    // 4点目は t = 4/8 = 0.5 で+Zと+Xの中間
    ExpectVector(*AsGoto(program.records[4]).tool_axis,
                 Vector3d(1.0, 0.0, 1.0).normalized());
    ExpectVector(*AsGoto(program.records.back()).tool_axis, Vector3d::UnitX());
}

TEST(ClTransformTest, LinearizeArcs_SetsToolAxisOnlyAtEndWhenStartAxisUnknown) {
    mc::ClArc arc = Arc(kQuarterEnd);
    arc.tool_axis = Vector3d::UnitX();
    mc::ClProgram program;
    program.records = {Linear(kStart), arc};
    mc::LinearizeArcs(program, kChord);

    ASSERT_EQ(program.records.size(), 9u);
    EXPECT_FALSE(AsGoto(program.records[1]).tool_axis.has_value());
    ExpectVector(*AsGoto(program.records.back()).tool_axis, Vector3d::UnitX());
}

TEST(ClTransformTest, LinearizeArcs_KeepsArcWithUnknownStart) {
    mc::ClProgram program;
    program.records = {Arc(kQuarterEnd), mc::ClEnd{}};
    program.sources = {mc::SourceLocation{0, 1}, mc::SourceLocation{0, 2}};
    mc::LinearizeArcs(program, kChord);
    ASSERT_EQ(program.records.size(), 2u);
    EXPECT_TRUE(std::holds_alternative<mc::ClArc>(program.records[0]));
    EXPECT_EQ(program.sources.size(), 2u);
}

// ---- EnumeratePaths ----

TEST(ClTransformTest, EnumeratePaths_ByMarkers) {
    mc::ClProgram program;
    program.records = {mc::ClFeed{1.0},
                       mc::ClMarker{mc::ClMarker::Kind::kPathBegin, "a"},
                       Linear(Vector3d::Zero()), Linear(Vector3d::UnitX()),
                       mc::ClMarker{mc::ClMarker::Kind::kPathEnd, "a"},
                       Rapid(Vector3d::UnitY()),
                       mc::ClMarker{mc::ClMarker::Kind::kPathBegin, "b"},
                       Arc(kQuarterEnd),
                       mc::ClMarker{mc::ClMarker::Kind::kPathEnd, "b"}};
    const auto ranges = mc::EnumeratePaths(program);
    ASSERT_EQ(ranges.size(), 4u);
    EXPECT_EQ(ranges[0].begin, 0u);
    EXPECT_EQ(ranges[0].end, 1u);
    EXPECT_TRUE(ranges[0].name.empty());
    EXPECT_FALSE(ranges[0].rapid);
    EXPECT_EQ(ranges[1].begin, 1u);
    EXPECT_EQ(ranges[1].end, 5u);
    EXPECT_EQ(ranges[1].name, "a");
    EXPECT_FALSE(ranges[1].rapid);
    EXPECT_EQ(ranges[2].begin, 5u);
    EXPECT_EQ(ranges[2].end, 6u);
    EXPECT_TRUE(ranges[2].name.empty());
    EXPECT_TRUE(ranges[2].rapid);
    EXPECT_EQ(ranges[3].begin, 6u);
    EXPECT_EQ(ranges[3].end, 9u);
    EXPECT_EQ(ranges[3].name, "b");
    EXPECT_FALSE(ranges[3].rapid);
}

TEST(ClTransformTest, EnumeratePaths_ByRapidsIncludesStateRecordsInPreviousRange) {
    mc::ClProgram program;
    program.records = {mc::ClFeed{1.0}, Rapid(Vector3d::Zero()),
                       Rapid(Vector3d::UnitX()), Linear(Vector3d::UnitY()),
                       mc::ClDwell{0.1}, mc::ClFeed{2.0}, Arc(kQuarterEnd),
                       Rapid(Vector3d::UnitZ()), mc::ClComment{"tail"}};
    const auto ranges = mc::EnumeratePaths(program);
    ASSERT_EQ(ranges.size(), 3u);
    EXPECT_EQ(ranges[0].begin, 0u);
    EXPECT_EQ(ranges[0].end, 3u);
    EXPECT_TRUE(ranges[0].rapid);
    EXPECT_EQ(ranges[1].begin, 3u);
    EXPECT_EQ(ranges[1].end, 7u);
    EXPECT_FALSE(ranges[1].rapid);
    EXPECT_EQ(ranges[2].begin, 7u);
    EXPECT_EQ(ranges[2].end, 9u);
    EXPECT_TRUE(ranges[2].rapid);
    for (const mc::ClPathRange& range : ranges) EXPECT_TRUE(range.name.empty());
}

TEST(ClTransformTest, EnumeratePaths_NoMotionGivesSingleCutRange) {
    mc::ClProgram program;
    program.records = {mc::ClFeed{1.0}, mc::ClComment{"c"}};
    const auto ranges = mc::EnumeratePaths(program);
    ASSERT_EQ(ranges.size(), 1u);
    EXPECT_EQ(ranges[0].begin, 0u);
    EXPECT_EQ(ranges[0].end, 2u);
    EXPECT_FALSE(ranges[0].rapid);
}

TEST(ClTransformTest, EnumeratePaths_EmptyProgramGivesNoRange) {
    EXPECT_TRUE(mc::EnumeratePaths(mc::ClProgram{}).empty());
}

// ---- InsertApproachRetract ----

TEST(ClTransformTest, ApproachRetract_InsertsAlongToolAxisWithFeedRatio) {
    mc::ClProgram program = TwoPointCut();
    std::vector<mc::Diagnostic> warnings;
    mc::InsertApproachRetract(program, ToolAxisSpec(), &warnings);
    EXPECT_TRUE(warnings.empty());

    // Feed2 / R(0,0,5) / F1 / L(0,0,0) / F2 / L(0,0,0) / L(10,0,0) / F1 / L(10,0,5) / F2
    ASSERT_EQ(program.records.size(), 10u);
    const mc::ClGoto& approach_rapid = AsGoto(program.records[1]);
    EXPECT_EQ(approach_rapid.kind, mc::MotionKind::kRapid);
    EXPECT_EQ(approach_rapid.role, mc::PathRole::kApproach);
    ExpectVector(*approach_rapid.point, Vector3d(0.0, 0.0, 5.0));
    EXPECT_NEAR(FeedOf(program.records[2]), 1.0, kTol);
    const mc::ClGoto& approach_cut = AsGoto(program.records[3]);
    EXPECT_EQ(approach_cut.kind, mc::MotionKind::kLinear);
    EXPECT_EQ(approach_cut.role, mc::PathRole::kApproach);
    ExpectVector(*approach_cut.point, Vector3d::Zero());
    EXPECT_NEAR(FeedOf(program.records[4]), 2.0, kTol);
    // 元のレコードは変わらない
    EXPECT_EQ(AsGoto(program.records[5]).role, mc::PathRole::kCut);
    ExpectVector(*AsGoto(program.records[6]).point, Vector3d(10.0, 0.0, 0.0));
    EXPECT_NEAR(FeedOf(program.records[7]), 1.0, kTol);
    const mc::ClGoto& retract = AsGoto(program.records[8]);
    EXPECT_EQ(retract.kind, mc::MotionKind::kLinear);
    EXPECT_EQ(retract.role, mc::PathRole::kRetract);
    ExpectVector(*retract.point, Vector3d(10.0, 0.0, 5.0));
    EXPECT_NEAR(FeedOf(program.records[9]), 2.0, kTol);
}

TEST(ClTransformTest, ApproachRetract_SourcesOfInsertedRecordsHaveZeroLine) {
    mc::ClProgram program = TwoPointCut();
    mc::InsertApproachRetract(program, ToolAxisSpec());
    ASSERT_EQ(program.sources.size(), program.records.size());
    const std::vector<int> expected_lines = {1, 0, 0, 0, 0, 2, 3, 0, 0, 0};
    for (std::size_t i = 0; i < expected_lines.size(); ++i) {
        EXPECT_EQ(program.sources[i].line, expected_lines[i]) << "index " << i;
        EXPECT_EQ(program.sources[i].program_index, 1) << "index " << i;
    }
}

TEST(ClTransformTest, ApproachRetract_ClearanceAddsRapidPoints) {
    mc::ClProgram program = TwoPointCut();
    mc::ApproachRetractSpec spec = ToolAxisSpec();
    spec.clearance = Vector3d(0.0, 0.0, 20.0);
    mc::InsertApproachRetract(program, spec);

    ASSERT_EQ(program.records.size(), 12u);
    const mc::ClGoto& first = AsGoto(program.records[1]);
    EXPECT_EQ(first.kind, mc::MotionKind::kRapid);
    ExpectVector(*first.point, Vector3d(0.0, 0.0, 25.0));
    ExpectVector(*AsGoto(program.records[2]).point, Vector3d(0.0, 0.0, 5.0));
    const mc::ClGoto& last = AsGoto(program.records[11]);
    EXPECT_EQ(last.kind, mc::MotionKind::kRapid);
    EXPECT_EQ(last.role, mc::PathRole::kRetract);
    ExpectVector(*last.point, Vector3d(10.0, 0.0, 25.0));
}

TEST(ClTransformTest, ApproachRetract_WorkDirectionUsesFixedOffset) {
    mc::ClProgram program;
    program.records = {Linear(Vector3d::Zero()), Linear(Vector3d(10.0, 0.0, 0.0))};
    mc::ApproachRetractSpec spec;
    spec.direction = mc::ApproachRetractSpec::Direction::kWork;
    spec.offset = Vector3d(0.0, 3.0, 0.0);
    mc::InsertApproachRetract(program, spec);

    // 送りが不明で比が1なので ClFeed は挿入されない
    ASSERT_EQ(program.records.size(), 5u);
    ExpectVector(*AsGoto(program.records[0]).point, Vector3d(0.0, 3.0, 0.0));
    ExpectVector(*AsGoto(program.records[1]).point, Vector3d::Zero());
    ExpectVector(*AsGoto(program.records[4]).point, Vector3d(10.0, 3.0, 0.0));
    EXPECT_TRUE(program.sources.empty());
}

TEST(ClTransformTest, ApproachRetract_SkipsRapidRanges) {
    mc::ClProgram program;
    program.records = {Rapid(Vector3d::Zero()), Rapid(Vector3d::UnitX())};
    std::vector<mc::Diagnostic> warnings;
    mc::InsertApproachRetract(program, ToolAxisSpec(), &warnings);
    EXPECT_EQ(program.records.size(), 2u);
    EXPECT_TRUE(warnings.empty());
}

TEST(ClTransformTest, ApproachRetract_WarnsWhenPathHasNoWorkFramePoint) {
    mc::ClGoto machine;
    machine.kind = mc::MotionKind::kLinear;
    machine.frame = mc::MotionFrame::kMachine;
    machine.axis_words = mc::NcValues{{"Z", 0.0}};
    mc::ClProgram program;
    program.records = {machine};
    std::vector<mc::Diagnostic> warnings;
    mc::InsertApproachRetract(program, ToolAxisSpec(), &warnings);
    EXPECT_EQ(program.records.size(), 1u);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_TRUE(HasWarning(warnings, "no work-frame motion"));
}

TEST(ClTransformTest, ApproachRetract_WarnsWhenToolAxisIsUnknown) {
    mc::ClProgram program;
    program.records = {Linear(Vector3d::Zero()), Linear(Vector3d::UnitX())};
    std::vector<mc::Diagnostic> warnings;
    mc::InsertApproachRetract(program, ToolAxisSpec(), &warnings);
    EXPECT_EQ(program.records.size(), 2u);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_TRUE(HasWarning(warnings, "no tool axis"));
}
