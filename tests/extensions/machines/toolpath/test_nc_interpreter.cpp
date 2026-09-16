/**
 * @file tests/extensions/machines/toolpath/test_nc_interpreter.cpp
 * @brief NCプログラムの解釈 (toolpath/nc_interpreter) のテスト
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note 対象: InterpretNc / InterpretNcFile / NcState
 *       - 正常系: 動作コードとモーダル、G90/G91、G20/G21、G43系の各形式、
 *         G53/G28/G30、ワークオフセット、G68.2/G53.1/G69、円弧 (IJK/R/全円/平面.
 *         TCP無効時は座標語の`ClGoto`列に折れ線化される), G04、T/M06、S/M03〜M05/M08/M09、サブプログラム (同一ファイル/外部),
 *         M30、未知のG/M、コメント、状態の引き継ぎ、実機NCの体裁
 *       - 正常系 (境界値・退化): 座標語のみのブロック、Gのみのブロック、IJK省略,
 *         無効化コードの表記揺れ
 *       - 異常系: 無効化コード、G43.5下のIJK中心指定、始点==終点のR指定,
 *         未定義のサブプログラム、深さ超過 (いずれも`DataFormatError`)
 *       TODO: `subprogram_loader`が返した外部プログラムの`block_skip`は
 *       主プログラムと同じ設定を用いる経路のみ (別設定は仕様に無い)
 */
#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/core/text_file.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/toolpath/cl_program.h"
#include "igesio/extensions/machines/toolpath/nc_block.h"
#include "igesio/extensions/machines/toolpath/nc_dialect.h"
#include "igesio/extensions/machines/toolpath/nc_interpreter.h"
#include "../machine/machines_for_testing.h"

namespace {

namespace mc = igesio::extensions::machines;
using igesio::Vector3d;

/// @brief 座標・角度の比較の許容誤差
constexpr double kTol = 1e-9;

/// @brief 正規化した方向・換算後の座標の比較の許容誤差
constexpr double kLooseTol = 1e-6;

/// @brief NCのテストデータのディレクトリ (tests/test_data/machines/nc)
const std::filesystem::path kNcDir = machines_test::kFixturePath.parent_path() / "nc";

/// @brief デフォルトの解釈設定 (Fanuc方言・コメント保持・未知のG/M保持)
mc::NcInterpretOptions DefaultOptions() {
    mc::NcInterpretOptions options;
    options.dialect = mc::DefaultFanucDialect();
    return options;
}

/// @brief NC文字列を解釈する (状態は省略可)
mc::ClProgram Interpret(const std::string& text, mc::NcState* state = nullptr,
                        const mc::NcInterpretOptions& options = DefaultOptions(),
                        const mc::NcLexOptions& lex = {}) {
    return mc::InterpretNc(text, lex, options, state);
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

/// @brief 指定した型のレコードの数
template <class T>
std::size_t Count(const mc::ClProgram& program) {
    return Records<T>(program).size();
}

/// @brief 文言に部分文字列を含む警告があるか
bool HasWarning(const mc::ClProgram& program, const std::string& text) {
    for (const mc::Diagnostic& warning : program.warnings) {
        if (warning.message.find(text) != std::string::npos) return true;
    }
    return false;
}

/// @brief 3成分の近似比較
void ExpectVector(const Vector3d& actual, const double x, const double y,
                  const double z, const double tol = kTol) {
    EXPECT_NEAR(actual.x(), x, tol);
    EXPECT_NEAR(actual.y(), y, tol);
    EXPECT_NEAR(actual.z(), z, tol);
}

/// @brief 座標語のみの`ClGoto`列を点列にする (未指定の成分は直前の値、初期値は0)
/// @note TCP無効時の円弧は座標語の`ClGoto`列に折れ線化されるので、通過点の
///       幾何 (中心からの距離、終点、向き) をこの点列で検証する
std::vector<Vector3d> AxisWordPoints(const mc::ClProgram& program) {
    std::vector<Vector3d> points;
    Vector3d current = Vector3d::Zero();
    for (const mc::ClGoto* motion : Records<mc::ClGoto>(program)) {
        if (motion->point.has_value()) continue;
        current.x() = motion->axis_words.GetOr("X", current.x());
        current.y() = motion->axis_words.GetOr("Y", current.y());
        current.z() = motion->axis_words.GetOr("Z", current.z());
        points.push_back(current);
    }
    return points;
}

/// @brief 先頭を除く点列が中心から一定距離にあることを検証する
void ExpectOnCircle(const std::vector<Vector3d>& points, const Vector3d& center,
                    const double radius) {
    ASSERT_GE(points.size(), 3u);
    for (std::size_t i = 1; i < points.size(); ++i) {
        EXPECT_NEAR((points[i] - center).norm(), radius, kLooseTol)
                << "point " << i << ": " << points[i].transpose();
    }
}

}  // namespace



// ---- 動作コードとモーダル ----

TEST(NcInterpreterTest, Modal_GOnlyBlockGeneratesNoMotion) {
    const mc::ClProgram program = Interpret("G01 F100\nG00\n");
    EXPECT_EQ(Count<mc::ClGoto>(program), 0u);
    EXPECT_EQ(Count<mc::ClFeed>(program), 1u);
}

TEST(NcInterpreterTest, Modal_CoordinateOnlyBlockUsesModalMotion) {
    const mc::ClProgram program = Interpret("G90 G01 X10. F100\nY5.\n");
    const auto gotos = Records<mc::ClGoto>(program);
    ASSERT_EQ(gotos.size(), 2u);
    EXPECT_EQ(gotos[0]->kind, mc::MotionKind::kLinear);
    EXPECT_FALSE(gotos[0]->point.has_value());
    EXPECT_NEAR(gotos[0]->axis_words.At("X"), 10.0, kTol);
    EXPECT_FALSE(gotos[0]->axis_words.Contains("Y"));
    EXPECT_EQ(gotos[1]->kind, mc::MotionKind::kLinear);
    EXPECT_NEAR(gotos[1]->axis_words.At("Y"), 5.0, kTol);
    EXPECT_FALSE(gotos[1]->axis_words.Contains("X"));
}

TEST(NcInterpreterTest, Modal_CoordinatesBeforeMotionCodeWarn) {
    const mc::ClProgram program = Interpret("G90 X10.\n");
    EXPECT_EQ(Count<mc::ClGoto>(program), 0u);
    EXPECT_TRUE(HasWarning(program, "before a motion code"));
}

TEST(NcInterpreterTest, Modal_MultipleMotionCodesUseLastAndWarn) {
    const mc::ClProgram program = Interpret("G90 G01 G00 X10.\n");
    const auto gotos = Records<mc::ClGoto>(program);
    ASSERT_EQ(gotos.size(), 1u);
    EXPECT_EQ(gotos[0]->kind, mc::MotionKind::kRapid);
    EXPECT_TRUE(HasWarning(program, "multiple motion codes"));
}

TEST(NcInterpreterTest, Initial_RecordsReflectStateAtStart) {
    const mc::ClProgram fresh = Interpret("G01 X1. F100\n");
    ASSERT_GE(fresh.records.size(), 2u);
    ASSERT_TRUE(std::holds_alternative<mc::ClLoadTool>(fresh.records[0]));
    EXPECT_EQ(std::get<mc::ClLoadTool>(fresh.records[0]).number, mc::kNoTool);
    ASSERT_TRUE(std::holds_alternative<mc::ClSelectWorkOffset>(fresh.records[1]));
    EXPECT_EQ(std::get<mc::ClSelectWorkOffset>(fresh.records[1]).id, "G54");
    EXPECT_EQ(fresh.sources[0].line, 0);

    mc::NcState state;
    state.tool_number = 3;
    state.work_offset_id = "G55";
    state.feed = 2.0;
    state.length_comp = true;
    state.h_number = 7;
    const mc::ClProgram carried = Interpret("G01 X1.\n", &state);
    EXPECT_EQ(std::get<mc::ClLoadTool>(carried.records[0]).number, 3);
    EXPECT_EQ(std::get<mc::ClSelectWorkOffset>(carried.records[1]).id, "G55");
    ASSERT_TRUE(std::holds_alternative<mc::ClLengthOffset>(carried.records[2]));
    EXPECT_EQ(std::get<mc::ClLengthOffset>(carried.records[2]).number, 7);
    ASSERT_TRUE(std::holds_alternative<mc::ClFeed>(carried.records[3]));
    EXPECT_NEAR(std::get<mc::ClFeed>(carried.records[3]).mm_per_s, 2.0, kTol);
}

// ---- G90 / G91 ----

TEST(NcInterpreterTest, G91_AddsIncrementsToLinearWords) {
    const mc::ClProgram program =
            Interpret("G90 G01 X10. F100\nG91 X5. Y5.\nG90 X1.\n");
    const auto gotos = Records<mc::ClGoto>(program);
    ASSERT_EQ(gotos.size(), 3u);
    EXPECT_NEAR(gotos[1]->axis_words.At("X"), 15.0, kTol);
    EXPECT_NEAR(gotos[1]->axis_words.At("Y"), 5.0, kTol);
    EXPECT_NEAR(gotos[2]->axis_words.At("X"), 1.0, kTol);
}

TEST(NcInterpreterTest, G91_AddsIncrementsToRotaryWords) {
    const mc::ClProgram program = Interpret("G90 G01 B10. F100\nG91 B5.\n");
    const auto gotos = Records<mc::ClGoto>(program);
    ASSERT_EQ(gotos.size(), 2u);
    EXPECT_NEAR(gotos[1]->axis_words.At("B"), mc::ToRadians(15.0), kTol);
}

TEST(NcInterpreterTest, G91_AfterTcpChangeWarns) {
    const mc::ClProgram program = Interpret(
            "G90 G43.5 H1\nG01 X10. Y0. Z0. I0. J0. K1. F100\nG49\nG91 X5.\n");
    EXPECT_TRUE(HasWarning(program, "incremental move right after a TCP mode change"));
}

TEST(NcInterpreterTest, G91_WithoutTcpChangeDoesNotWarn) {
    const mc::ClProgram program = Interpret("G90 G01 X10. F100\nG91 X5.\nX5.\n");
    EXPECT_FALSE(HasWarning(program, "incremental move"));
}

// ---- G20 / G21 ----

TEST(NcInterpreterTest, G20_ScalesLinearWordsAndFeed) {
    const mc::ClProgram program = Interpret("G20 G90 G01 X1. F10\n");
    const auto gotos = Records<mc::ClGoto>(program);
    ASSERT_EQ(gotos.size(), 1u);
    EXPECT_NEAR(gotos[0]->axis_words.At("X"), mc::kInchToMillimeter, kTol);
    const auto feeds = Records<mc::ClFeed>(program);
    ASSERT_EQ(feeds.size(), 1u);
    EXPECT_NEAR(feeds[0]->mm_per_s, 10.0 * mc::kInchToMillimeter / mc::kSecondsPerMinute,
                kTol);
}

TEST(NcInterpreterTest, G20_ScalesArcCenterAndRadius) {
    const mc::ClProgram program =
            Interpret("G20 G90 G01 X0. Y0. F10\nG02 X1. Y1. I1. J0.\nG03 X0. Y0. R1.\n");
    EXPECT_EQ(Count<mc::ClArc>(program), 0u);
    // どちらの円弧も中心 (1, 0) inch・半径1 inch (R = 1 inch の劣弧は弦の左側が中心)
    const std::vector<Vector3d> points = AxisWordPoints(program);
    ExpectOnCircle(points, Vector3d(25.4, 0.0, 0.0), 25.4);
    ExpectVector(points.back(), 0.0, 0.0, 0.0, kLooseTol);
    bool reached_corner = false;
    for (const Vector3d& point : points) {
        reached_corner = reached_corner
                         || (point - Vector3d(25.4, 25.4, 0.0)).norm() < kLooseTol;
    }
    EXPECT_TRUE(reached_corner);
}

TEST(NcInterpreterTest, G20_DoesNotScaleRotaryWords) {
    const mc::ClProgram program = Interpret("G20 G90 G01 B10. F10\n");
    const auto gotos = Records<mc::ClGoto>(program);
    ASSERT_EQ(gotos.size(), 1u);
    EXPECT_NEAR(gotos[0]->axis_words.At("B"), mc::ToRadians(10.0), kTol);
}

// ---- G43系 ----

TEST(NcInterpreterTest, G43_EmitsLengthOffsetAndAxisWords) {
    const mc::ClProgram program = Interpret("G43 H1\nG90 G01 X10. Y20. Z30. F100\n");
    const auto offsets = Records<mc::ClLengthOffset>(program);
    ASSERT_EQ(offsets.size(), 1u);
    EXPECT_EQ(offsets[0]->number, 1);
    const auto gotos = Records<mc::ClGoto>(program);
    ASSERT_EQ(gotos.size(), 1u);
    EXPECT_FALSE(gotos[0]->point.has_value());
    EXPECT_FALSE(gotos[0]->tool_axis.has_value());
    EXPECT_NEAR(gotos[0]->axis_words.At("Z"), 30.0, kTol);
}

TEST(NcInterpreterTest, G434_EmitsPointAndRotaryWords) {
    const mc::ClProgram program =
            Interpret("G43.4 H1\nG90 G01 X10. Y20. Z30. B10. C20. F100\n");
    const auto gotos = Records<mc::ClGoto>(program);
    ASSERT_EQ(gotos.size(), 1u);
    ASSERT_TRUE(gotos[0]->point.has_value());
    ExpectVector(*gotos[0]->point, 10.0, 20.0, 30.0);
    EXPECT_FALSE(gotos[0]->tool_axis.has_value());
    EXPECT_NEAR(gotos[0]->axis_words.At("B"), mc::ToRadians(10.0), kTol);
    EXPECT_NEAR(gotos[0]->axis_words.At("C"), mc::ToRadians(20.0), kTol);
    EXPECT_FALSE(gotos[0]->axis_words.Contains("X"));
}

TEST(NcInterpreterTest, G435_EmitsPointAndToolAxis) {
    const mc::ClProgram program =
            Interpret("G43.5 H1\nG90 G01 X10. Y0. Z0. I3. J0. K4. F100\n");
    const auto gotos = Records<mc::ClGoto>(program);
    ASSERT_EQ(gotos.size(), 1u);
    ASSERT_TRUE(gotos[0]->point.has_value());
    ExpectVector(*gotos[0]->point, 10.0, 0.0, 0.0);
    ASSERT_TRUE(gotos[0]->tool_axis.has_value());
    ExpectVector(*gotos[0]->tool_axis, 0.6, 0.0, 0.8);
    EXPECT_TRUE(gotos[0]->axis_words.Empty());
}

TEST(NcInterpreterTest, G435_OmittedIjkContinuesPreviousAxis) {
    const mc::ClProgram program = Interpret(
            "G43.5 H1\nG90 G01 X0. Y0. Z0. I0. J0. K1. F100\nX10.\n");
    const auto gotos = Records<mc::ClGoto>(program);
    ASSERT_EQ(gotos.size(), 2u);
    EXPECT_FALSE(gotos[1]->tool_axis.has_value());
    ASSERT_TRUE(gotos[1]->point.has_value());
    ExpectVector(*gotos[1]->point, 10.0, 0.0, 0.0);
}

TEST(NcInterpreterTest, G49_EmitsCancelAndReturnsToAxisWords) {
    const mc::ClProgram program =
            Interpret("G43.5 H1\nG90 G01 X0. Y0. Z0. I0. J0. K1. F100\nG49\nX1.\n");
    const auto offsets = Records<mc::ClLengthOffset>(program);
    ASSERT_EQ(offsets.size(), 2u);
    EXPECT_EQ(offsets[0]->number, 1);
    EXPECT_FALSE(offsets[1]->number.has_value());
    const auto gotos = Records<mc::ClGoto>(program);
    ASSERT_EQ(gotos.size(), 2u);
    EXPECT_FALSE(gotos[1]->point.has_value());
    EXPECT_NEAR(gotos[1]->axis_words.At("X"), 1.0, kTol);
}

TEST(NcInterpreterTest, HAlone_UpdatesLengthOffsetWithoutMotion) {
    const mc::ClProgram program = Interpret("G43 H1\nH12D12\n");
    const auto offsets = Records<mc::ClLengthOffset>(program);
    ASSERT_EQ(offsets.size(), 2u);
    EXPECT_EQ(offsets[1]->number, 12);
    EXPECT_EQ(Count<mc::ClGoto>(program), 0u);
}

TEST(NcInterpreterTest, G44_IsIgnoredWithWarning) {
    const mc::ClProgram program = Interpret("G44 H1\n");
    EXPECT_TRUE(HasWarning(program, "G44"));
    EXPECT_EQ(Count<mc::ClLengthOffset>(program), 0u);
}

// ---- G53 / G28 / G30 ----

TEST(NcInterpreterTest, G53_MachineFrameForThatBlockOnly) {
    const mc::ClProgram program = Interpret("G90 G00 G53 Z0. B0.\nG01 X10. F100\n");
    const auto gotos = Records<mc::ClGoto>(program);
    ASSERT_EQ(gotos.size(), 2u);
    EXPECT_EQ(gotos[0]->frame, mc::MotionFrame::kMachine);
    EXPECT_EQ(gotos[0]->kind, mc::MotionKind::kRapid);
    EXPECT_FALSE(gotos[0]->point.has_value());
    EXPECT_NEAR(gotos[0]->axis_words.At("Z"), 0.0, kTol);
    EXPECT_NEAR(gotos[0]->axis_words.At("B"), 0.0, kTol);
    EXPECT_EQ(gotos[1]->frame, mc::MotionFrame::kWork);
}

TEST(NcInterpreterTest, G53_DoesNotUpdateWorkPosition) {
    const mc::ClProgram program = Interpret("G90 G01 X10. F100\nG53 X0.\nG91 X1.\n");
    const auto gotos = Records<mc::ClGoto>(program);
    ASSERT_EQ(gotos.size(), 3u);
    EXPECT_NEAR(gotos[2]->axis_words.At("X"), 11.0, kTol);
}

TEST(NcInterpreterTest, G28_EmitsIntermediateAndReferenceRecords) {
    const mc::ClProgram program = Interpret("G90 G01 X10. F100\nG91 G28 Z0.\n");
    const auto gotos = Records<mc::ClGoto>(program);
    ASSERT_EQ(gotos.size(), 3u);
    EXPECT_EQ(gotos[1]->frame, mc::MotionFrame::kWork);
    EXPECT_EQ(gotos[1]->kind, mc::MotionKind::kRapid);
    EXPECT_NEAR(gotos[1]->axis_words.At("Z"), 0.0, kTol);
    EXPECT_EQ(gotos[2]->frame, mc::MotionFrame::kMachine);
    EXPECT_NEAR(gotos[2]->axis_words.At("Z"), 0.0, kTol);
    EXPECT_FALSE(gotos[2]->axis_words.Contains("X"));
}

TEST(NcInterpreterTest, G28_ReferenceCoversAllSpecifiedAxes) {
    const mc::ClProgram program = Interpret("G90 G28 X0. Y0. C0.\n");
    const auto gotos = Records<mc::ClGoto>(program);
    ASSERT_EQ(gotos.size(), 2u);
    EXPECT_TRUE(gotos[1]->axis_words.Contains("X"));
    EXPECT_TRUE(gotos[1]->axis_words.Contains("Y"));
    EXPECT_TRUE(gotos[1]->axis_words.Contains("C"));
    EXPECT_FALSE(gotos[1]->axis_words.Contains("Z"));
}

TEST(NcInterpreterTest, G30_EmitsIntermediateOnlyWithWarning) {
    const mc::ClProgram program = Interpret("G91 G30 Z0.\n");
    EXPECT_EQ(Count<mc::ClGoto>(program), 1u);
    EXPECT_TRUE(HasWarning(program, "G30"));
}

TEST(NcInterpreterTest, G92_IsIgnoredWithWarning) {
    const mc::ClProgram program = Interpret("G90 G92 X0. Y0.\n");
    EXPECT_EQ(Count<mc::ClGoto>(program), 0u);
    EXPECT_TRUE(HasWarning(program, "G92"));
}

// ---- ワークオフセット ----

TEST(NcInterpreterTest, WorkOffsets_G54ToG59AndP) {
    const mc::ClProgram program = Interpret("G55\nG54.1 P2\nG59\nG59\n");
    const auto offsets = Records<mc::ClSelectWorkOffset>(program);
    // 先頭の初期レコード (G54) + 3回の変化 (同じG59の繰り返しは出力しない)
    ASSERT_EQ(offsets.size(), 4u);
    EXPECT_EQ(offsets[1]->id, "G55");
    EXPECT_EQ(offsets[2]->id, "G54.1P2");
    EXPECT_EQ(offsets[3]->id, "G59");
}

TEST(NcInterpreterTest, WorkOffsets_G541WithoutPWarns) {
    const mc::ClProgram program = Interpret("G54.1\n");
    EXPECT_TRUE(HasWarning(program, "G54.1 without P"));
}

TEST(NcInterpreterTest, WorkOffsets_DialectMapsWordToId) {
    mc::NcInterpretOptions options = DefaultOptions();
    options.dialect.work_offset_ids["G55"] = "fixture-a";
    const mc::ClProgram program = Interpret("G55\n", nullptr, options);
    const auto offsets = Records<mc::ClSelectWorkOffset>(program);
    ASSERT_EQ(offsets.size(), 2u);
    EXPECT_EQ(offsets[1]->id, "fixture-a");
}

// ---- 傾斜面 ----

TEST(NcInterpreterTest, G682_TransformsFeatureCoordinatesToWork) {
    const mc::ClProgram program = Interpret(
            "G90 G43 H1\nG68.2 X10. Y0. Z0. I0. J90. K0.\nG53.1\nG01 X0. Y0. Z5. F100\n");
    const auto gotos = Records<mc::ClGoto>(program);
    ASSERT_EQ(gotos.size(), 1u);
    ASSERT_TRUE(gotos[0]->point.has_value());
    // R = Ry(90°): 特徴z軸 = +X (ワーク座標)、特徴 (0,0,5) → (10,0,0) + 5·(1,0,0)
    ExpectVector(*gotos[0]->point, 15.0, 0.0, 0.0, kLooseTol);
    ASSERT_TRUE(gotos[0]->tool_axis.has_value());
    ExpectVector(*gotos[0]->tool_axis, 1.0, 0.0, 0.0, kLooseTol);
}

TEST(NcInterpreterTest, G682_ArcCenterAndNormalAreTransformed) {
    const mc::ClProgram program = Interpret(
            "G90 G43 H1\nG68.2 X0. Y0. Z0. I0. J90. K0.\nG53.1\n"
            "G01 X0. Y0. Z0. F100\nG03 X10. Y10. I10. J0.\n");
    const auto arcs = Records<mc::ClArc>(program);
    ASSERT_EQ(arcs.size(), 1u);
    // Ry(90°): (x, y, z) → (z, y, -x)
    ExpectVector(arcs[0]->center, 0.0, 0.0, -10.0, kLooseTol);
    ExpectVector(arcs[0]->end, 0.0, 10.0, -10.0, kLooseTol);
    ExpectVector(arcs[0]->normal, 1.0, 0.0, 0.0, kLooseTol);
}

TEST(NcInterpreterTest, G69_RestoresAxisWordForm) {
    const mc::ClProgram program = Interpret(
            "G90 G43 H1\nG68.2 X10. Y0. Z0. I0. J90. K0.\nG53.1\nG01 X0. Y0. Z5. F100\n"
            "G69\nG01 X1.\n");
    const auto gotos = Records<mc::ClGoto>(program);
    ASSERT_EQ(gotos.size(), 2u);
    EXPECT_FALSE(gotos[1]->point.has_value());
    EXPECT_NEAR(gotos[1]->axis_words.At("X"), 1.0, kTol);
}

TEST(NcInterpreterTest, G531_WithoutG682Warns) {
    const mc::ClProgram program = Interpret("G53.1\n");
    EXPECT_TRUE(HasWarning(program, "G53.1 without G68.2"));
}

// ---- 円弧 ----

TEST(NcInterpreterTest, Arc_IjkOnXyPlane) {
    const mc::ClProgram program =
            Interpret("G90 G01 X0. Y0. F100\nG02 X10. Y10. I10. J0.\n");
    // TCP無効の円弧は登録値相対の座標語の列になる (点は持たない)
    EXPECT_EQ(Count<mc::ClArc>(program), 0u);
    EXPECT_FALSE(HasWarning(program, "linearized"));
    const std::vector<Vector3d> points = AxisWordPoints(program);
    ExpectOnCircle(points, Vector3d(10.0, 0.0, 0.0), 10.0);
    ExpectVector(points.back(), 10.0, 10.0, 0.0);
    // 時計回りの1/4円: 通過点は x ≤ 10、y ≥ 0 の範囲 (反時計回りなら (10, -10) を通る)
    for (const Vector3d& point : points) {
        EXPECT_LE(point.x(), 10.0 + kLooseTol);
        EXPECT_GE(point.y(), -kLooseTol);
    }
    for (const mc::ClGoto* motion : Records<mc::ClGoto>(program)) {
        EXPECT_EQ(motion->kind, mc::MotionKind::kLinear);
        EXPECT_FALSE(motion->axis_words.Contains("Z"));
    }
}

TEST(NcInterpreterTest, Arc_RPositiveIsMinorArc) {
    const mc::ClProgram program =
            Interpret("G90 G01 X0. Y0. F100\nG03 X10. Y10. R10.\n");
    // 中心は弦の左側 (0, 10). 劣弧なので通過点は x ≥ 0
    const std::vector<Vector3d> points = AxisWordPoints(program);
    ExpectOnCircle(points, Vector3d(0.0, 10.0, 0.0), 10.0);
    ExpectVector(points.back(), 10.0, 10.0, 0.0);
    for (const Vector3d& point : points) EXPECT_GE(point.x(), -kLooseTol);
}

TEST(NcInterpreterTest, Arc_RNegativeIsMajorArc) {
    const mc::ClProgram program =
            Interpret("G90 G01 X0. Y0. F100\nG03 X10. Y10. R-10.\n");
    // 中心は弦の右側 (10, 0). 優弧なので (20, 0) 付近を通る
    const std::vector<Vector3d> points = AxisWordPoints(program);
    ExpectOnCircle(points, Vector3d(10.0, 0.0, 0.0), 10.0);
    ExpectVector(points.back(), 10.0, 10.0, 0.0);
    bool far_side = false;
    for (const Vector3d& point : points) far_side = far_side || point.x() > 15.0;
    EXPECT_TRUE(far_side);
}

TEST(NcInterpreterTest, Arc_ClockwiseRPositiveMirrorsCenter) {
    const mc::ClProgram program =
            Interpret("G90 G01 X0. Y0. F100\nG02 X10. Y10. R10.\n");
    // 時計回りでは中心が弦の右側 (10, 0) になり、劣弧なので通過点は x ≤ 10
    const std::vector<Vector3d> points = AxisWordPoints(program);
    ExpectOnCircle(points, Vector3d(10.0, 0.0, 0.0), 10.0);
    for (const Vector3d& point : points) EXPECT_LE(point.x(), 10.0 + kLooseTol);
}

TEST(NcInterpreterTest, Arc_SameStartAndEndWithIjkIsFullCircle) {
    const mc::ClProgram program =
            Interpret("G90 G01 X10. Y40. F100\nG02 X10. Y40. I5. J0.\n");
    // 全円: 中心 (15, 40)・半径5を一周して始点に戻る (反対側 (20, 40) を通る)
    const std::vector<Vector3d> points = AxisWordPoints(program);
    EXPECT_GE(points.size(), 20u);
    ExpectOnCircle(points, Vector3d(15.0, 40.0, 0.0), 5.0);
    ExpectVector(points.back(), 10.0, 40.0, 0.0);
    bool far_side = false;
    for (const Vector3d& point : points) far_side = far_side || point.x() > 19.0;
    EXPECT_TRUE(far_side);
}

TEST(NcInterpreterTest, Arc_G18AndG19PlanesUseTheirNormals) {
    const mc::ClProgram program = Interpret(
            "G90 G01 X0. Y0. Z0. F100\nG18 G02 X20. Z40. K10. I0.\n"
            "G19 G03 Y50. Z50. J10. K0.\n");
    // 平面の2成分の座標語だけを持つ (G18: Z/X、G19: Y/Z)
    const auto gotos = Records<mc::ClGoto>(program);
    ASSERT_GE(gotos.size(), 3u);
    const mc::ClGoto* zx = gotos[1];
    EXPECT_TRUE(zx->axis_words.Contains("X"));
    EXPECT_TRUE(zx->axis_words.Contains("Z"));
    EXPECT_FALSE(zx->axis_words.Contains("Y"));
    const mc::ClGoto* yz = gotos.back();
    EXPECT_TRUE(yz->axis_words.Contains("Y"));
    EXPECT_TRUE(yz->axis_words.Contains("Z"));
    EXPECT_FALSE(yz->axis_words.Contains("X"));
    EXPECT_NEAR(yz->axis_words.At("Y"), 50.0, kTol);
    EXPECT_NEAR(yz->axis_words.At("Z"), 50.0, kTol);
    const std::vector<Vector3d> points = AxisWordPoints(program);
    ExpectVector(points.back(), 20.0, 50.0, 50.0);
}

TEST(NcInterpreterTest, Arc_HelicalPitchWordWarns) {
    const mc::ClProgram program =
            Interpret("G90 G01 X0. Y0. F100\nG02 X10. Y10. I10. J0. K5.\n");
    EXPECT_TRUE(HasWarning(program, "helical"));
}

TEST(NcInterpreterTest, Arc_ThrowsDataFormatErrorWhenIjkUnderG435) {
    EXPECT_THROW(Interpret("G43.5 H1\nG90 G01 X0. Y0. Z0. I0. J0. K1. F100\n"
                           "G02 X10. Y10. I10. J0.\n"),
                 igesio::DataFormatError);
    EXPECT_NO_THROW(Interpret("G43.5 H1\nG90 G01 X0. Y0. Z0. I0. J0. K1. F100\n"
                              "G02 X10. Y10. R10.\n"));
}

TEST(NcInterpreterTest, Arc_ThrowsDataFormatErrorWhenRWithSamePoint) {
    EXPECT_THROW(Interpret("G90 G01 X0. Y0. F100\nG02 X0. Y0. R10.\n"),
                 igesio::DataFormatError);
    EXPECT_NO_THROW(Interpret("G90 G01 X0. Y0. F100\nG02 X0.001 Y0. R10.\n"));
}

TEST(NcInterpreterTest, Arc_ThrowsDataFormatErrorWhenRadiusTooSmall) {
    EXPECT_THROW(Interpret("G90 G01 X0. Y0. F100\nG02 X20. Y0. R9.99\n"),
                 igesio::DataFormatError);
    EXPECT_NO_THROW(Interpret("G90 G01 X0. Y0. F100\nG02 X20. Y0. R10.\n"));
}

TEST(NcInterpreterTest, Arc_ThrowsDataFormatErrorWhenNoCenterWord) {
    EXPECT_THROW(Interpret("G90 G01 X0. Y0. F100\nG02 X10. Y10.\n"),
                 igesio::DataFormatError);
}

TEST(NcInterpreterTest, Arc_UnderG434IsLinearizedWithRotaryWordsAtEnd) {
    const mc::ClProgram program = Interpret(
            "G43.4 H1\nG90 G01 X0. Y0. Z0. B0. F100\nG03 X10. Y10. R10. B10.\n");
    EXPECT_EQ(Count<mc::ClArc>(program), 0u);
    EXPECT_TRUE(HasWarning(program, "linearized"));
    const auto gotos = Records<mc::ClGoto>(program);
    ASSERT_GE(gotos.size(), 3u);
    const mc::ClGoto* last = gotos.back();
    ASSERT_TRUE(last->point.has_value());
    ExpectVector(*last->point, 10.0, 10.0, 0.0);
    EXPECT_NEAR(last->axis_words.At("B"), mc::ToRadians(10.0), kTol);
    EXPECT_FALSE(gotos[gotos.size() - 2]->axis_words.Contains("B"));
}

// ---- G04 ----

TEST(NcInterpreterTest, Dwell_PIsMillisecondsAndXIsSeconds) {
    const mc::ClProgram program = Interpret("G90 G01 X1. F100\nG04 P500\nG04 X0.5\n");
    const auto dwells = Records<mc::ClDwell>(program);
    ASSERT_EQ(dwells.size(), 2u);
    EXPECT_NEAR(dwells[0]->seconds, 0.5, kTol);
    EXPECT_NEAR(dwells[1]->seconds, 0.5, kTol);
    // G04 X はドウェル時間であり移動にならない
    EXPECT_EQ(Count<mc::ClGoto>(program), 1u);
}

// ---- 工具・主軸・クーラント ----

TEST(NcInterpreterTest, Tool_TThenM06ConfirmsTool) {
    const mc::ClProgram program = Interpret("T2\nG01 X1. F100\nM06\n");
    const auto tools = Records<mc::ClLoadTool>(program);
    ASSERT_EQ(tools.size(), 2u);
    EXPECT_EQ(tools[1]->number, 2);
    // ClLoadTool は M06 のブロックで出力される (移動より後)
    ASSERT_TRUE(std::holds_alternative<mc::ClLoadTool>(program.records.back()));
}

TEST(NcInterpreterTest, Tool_TAndM06InSameBlock) {
    const mc::ClProgram program = Interpret("T3M06\n");
    const auto tools = Records<mc::ClLoadTool>(program);
    ASSERT_EQ(tools.size(), 2u);
    EXPECT_EQ(tools[1]->number, 3);
}

TEST(NcInterpreterTest, Tool_M06WithoutTWarns) {
    const mc::ClProgram program = Interpret("M06\n");
    EXPECT_EQ(Count<mc::ClLoadTool>(program), 1u);
    EXPECT_TRUE(HasWarning(program, "M06 without"));
}

TEST(NcInterpreterTest, Spindle_SAndM03) {
    const mc::ClProgram program = Interpret("S1000 M03\nM05\n");
    const auto spindles = Records<mc::ClSpindle>(program);
    ASSERT_EQ(spindles.size(), 2u);
    EXPECT_EQ(spindles[0]->mode, mc::ClSpindle::Mode::kCw);
    ASSERT_TRUE(spindles[0]->rpm.has_value());
    EXPECT_NEAR(*spindles[0]->rpm, 1000.0, kTol);
    EXPECT_EQ(spindles[1]->mode, mc::ClSpindle::Mode::kOff);
}

TEST(NcInterpreterTest, Spindle_SOnlyUpdatesRpmWhileRunning) {
    const mc::ClProgram program = Interpret("S1000 M04\nS2000\n");
    const auto spindles = Records<mc::ClSpindle>(program);
    ASSERT_EQ(spindles.size(), 2u);
    EXPECT_EQ(spindles[1]->mode, mc::ClSpindle::Mode::kCcw);
    EXPECT_NEAR(*spindles[1]->rpm, 2000.0, kTol);
}

TEST(NcInterpreterTest, Spindle_SOnlyWhileStoppedEmitsNothing) {
    const mc::ClProgram program = Interpret("S2000\n");
    EXPECT_EQ(Count<mc::ClSpindle>(program), 0u);
}

TEST(NcInterpreterTest, Coolant_M08AndM09) {
    const mc::ClProgram program = Interpret("M08\nM09\n");
    const auto coolants = Records<mc::ClCoolant>(program);
    ASSERT_EQ(coolants.size(), 2u);
    EXPECT_TRUE(coolants[0]->on);
    EXPECT_FALSE(coolants[1]->on);
}

// ---- 送り ----

TEST(NcInterpreterTest, Feed_EmittedOnChangeOnly) {
    const mc::ClProgram program = Interpret("G90 G01 X1. F300\nX2. F300\nX3. F600\n");
    const auto feeds = Records<mc::ClFeed>(program);
    ASSERT_EQ(feeds.size(), 2u);
    EXPECT_NEAR(feeds[0]->mm_per_s, 300.0 / mc::kSecondsPerMinute, kTol);
    EXPECT_NEAR(feeds[1]->mm_per_s, 600.0 / mc::kSecondsPerMinute, kTol);
}

TEST(NcInterpreterTest, Feed_G95WarnsAndKeepsPerMinute) {
    const mc::ClProgram program = Interpret("G95 G01 X1. F0.5\n");
    EXPECT_TRUE(HasWarning(program, "G95"));
    const auto feeds = Records<mc::ClFeed>(program);
    ASSERT_EQ(feeds.size(), 1u);
    EXPECT_NEAR(feeds[0]->mm_per_s, 0.5 / mc::kSecondsPerMinute, kTol);
}

// ---- サブプログラム・終了 ----

TEST(NcInterpreterTest, Sub_SameFileRepeatsAndReturns) {
    const mc::ClProgram program = Interpret(
            "G90 G01 X0. F100\nM98 P100 L2\nX1.\nM30\nO0100\nG01 X5.\nM99\n");
    const auto gotos = Records<mc::ClGoto>(program);
    ASSERT_EQ(gotos.size(), 4u);
    EXPECT_NEAR(gotos[1]->axis_words.At("X"), 5.0, kTol);
    EXPECT_NEAR(gotos[2]->axis_words.At("X"), 5.0, kTol);
    EXPECT_NEAR(gotos[3]->axis_words.At("X"), 1.0, kTol);
    // サブプログラム側の行番号が出所になる
    EXPECT_EQ(program.sources[program.records.size() - 1].line, 4);
    bool found_sub_line = false;
    for (const mc::SourceLocation& source : program.sources) {
        found_sub_line = found_sub_line || source.line == 6;
    }
    EXPECT_TRUE(found_sub_line);
}

TEST(NcInterpreterTest, Sub_ThrowsDataFormatErrorWhenDepthExceeded) {
    mc::NcInterpretOptions options = DefaultOptions();
    options.max_subprogram_depth = 2;
    EXPECT_THROW(Interpret("O0001\nM98 P1\nM30\n", nullptr, options),
                 igesio::DataFormatError);
    // 深さ2まではエラーにならない (2段で M99 する)
    EXPECT_NO_THROW(Interpret("O0001\nM98 P2\nM30\nO0002\nM98 P3\nM99\nO0003\nM99\n",
                              nullptr, options));
}

TEST(NcInterpreterTest, Sub_ExternalLoaderProvidesProgram) {
    mc::NcInterpretOptions options = DefaultOptions();
    options.subprogram_loader = [](const int number) -> std::optional<std::string> {
        if (number != 200) return std::nullopt;
        return mc::ReadTextFile(kNcDir / "sub_o0200.nc");
    };
    const mc::ClProgram program =
            Interpret("G90 G01 X0. F100\nM98 P200\nM30\n", nullptr, options);
    const auto gotos = Records<mc::ClGoto>(program);
    ASSERT_EQ(gotos.size(), 3u);
    EXPECT_NEAR(gotos[2]->axis_words.At("X"), 10.0, kTol);
}

TEST(NcInterpreterTest, Sub_ThrowsDataFormatErrorWhenUndefined) {
    EXPECT_THROW(Interpret("M98 P300\n"), igesio::DataFormatError);
    mc::NcInterpretOptions options = DefaultOptions();
    options.subprogram_loader = [](int) -> std::optional<std::string> {
        return std::nullopt;
    };
    EXPECT_THROW(Interpret("M98 P300\n", nullptr, options), igesio::DataFormatError);
}

TEST(NcInterpreterTest, M30_EndsAndStopsReading) {
    mc::NcState state;
    const mc::ClProgram program = Interpret("G90 G01 X1. F100\nM30\nX2.\n", &state);
    EXPECT_EQ(Count<mc::ClGoto>(program), 1u);
    ASSERT_TRUE(std::holds_alternative<mc::ClEnd>(program.records.back()));
    EXPECT_TRUE(state.ended);
}

TEST(NcInterpreterTest, M99_InMainProgramEnds) {
    const mc::ClProgram program = Interpret("G90 G01 X1. F100\nM99\nX2.\n");
    EXPECT_EQ(Count<mc::ClGoto>(program), 1u);
    EXPECT_EQ(Count<mc::ClEnd>(program), 1u);
}

// ---- 無効化コード ----

TEST(NcInterpreterTest, Disabled_ThrowsDataFormatErrorWhenCodeAppears) {
    mc::NcInterpretOptions options = DefaultOptions();
    options.dialect.disabled_codes = {"G68.20"};
    EXPECT_THROW(mc::InterpretNcFile(kNcDir / "disabled.nc", {}, options, nullptr),
                 igesio::DataFormatError);
}

TEST(NcInterpreterTest, Disabled_OtherCodesDoNotThrow) {
    mc::NcInterpretOptions options = DefaultOptions();
    options.dialect.disabled_codes = {"G68.3"};
    EXPECT_NO_THROW(mc::InterpretNcFile(kNcDir / "disabled.nc", {}, options, nullptr));
}

// ---- 未知のG/M、コメント ----

TEST(NcInterpreterTest, UnknownM_PassThroughAndAggregated) {
    const mc::ClProgram program = Interpret("M1413 S1\nM11M13\nG90 G01 X1. F100\n");
    const auto passes = Records<mc::ClPassThrough>(program);
    ASSERT_EQ(passes.size(), 3u);
    EXPECT_EQ(passes[0]->dialect, "nc");
    EXPECT_EQ(passes[0]->text, "M1413 S1");
    EXPECT_EQ(passes[1]->text, "M11M13");
    EXPECT_EQ(passes[2]->text, "M11M13");
    // S1 は M1413 の引数であり主軸レコードにしない
    EXPECT_EQ(Count<mc::ClSpindle>(program), 0u);
    bool info = false;
    for (const mc::Diagnostic& warning : program.warnings) {
        if (warning.severity == mc::Severity::kInfo) {
            info = true;
            EXPECT_EQ(warning.message, "ignored M codes: M11 x1, M13 x1, M1413 x1");
        }
    }
    EXPECT_TRUE(info);
}

TEST(NcInterpreterTest, UnknownM_NotKeptWhenDisabled) {
    mc::NcInterpretOptions options = DefaultOptions();
    options.keep_unknown_words = false;
    const mc::ClProgram program = Interpret("M1413 S1\n", nullptr, options);
    EXPECT_EQ(Count<mc::ClPassThrough>(program), 0u);
    EXPECT_EQ(program.warnings.size(), 1u);
}

TEST(NcInterpreterTest, UnknownM_WithMotionKeepsOnlyTheCode) {
    const mc::ClProgram program = Interpret("G90 G01 X1. F100 M07\n");
    const auto passes = Records<mc::ClPassThrough>(program);
    ASSERT_EQ(passes.size(), 1u);
    EXPECT_EQ(passes[0]->text, "M7");
    EXPECT_EQ(Count<mc::ClGoto>(program), 1u);
}

TEST(NcInterpreterTest, StopCodes_M00AndM01ArePassThrough) {
    const mc::ClProgram program = Interpret("M00\nM01\n");
    const auto passes = Records<mc::ClPassThrough>(program);
    ASSERT_EQ(passes.size(), 2u);
    EXPECT_EQ(passes[0]->text, "M00");
    EXPECT_EQ(passes[1]->text, "M01");
    EXPECT_TRUE(program.warnings.empty());
}

TEST(NcInterpreterTest, UnknownG_PassThroughWithWarningKeepsMotion) {
    const mc::ClProgram program = Interpret("G90 G41 D1 G01 X1. F100\n");
    const auto passes = Records<mc::ClPassThrough>(program);
    ASSERT_EQ(passes.size(), 1u);
    EXPECT_EQ(passes[0]->text, "G41");
    EXPECT_TRUE(HasWarning(program, "G41"));
    EXPECT_EQ(Count<mc::ClGoto>(program), 1u);
}

TEST(NcInterpreterTest, Comments_KeptAsRecords) {
    const mc::ClProgram program = Interpret("(hello) G90 G01 X1. F100 ; tail\n");
    const auto comments = Records<mc::ClComment>(program);
    ASSERT_EQ(comments.size(), 2u);
    EXPECT_EQ(comments[0]->text, "hello");
    EXPECT_EQ(comments[1]->text, "tail");
}

TEST(NcInterpreterTest, Comments_DroppedWhenDisabled) {
    mc::NcInterpretOptions options = DefaultOptions();
    options.keep_comments = false;
    const mc::ClProgram program = Interpret("(hello) G90 G01 X1. F100\n", nullptr, options);
    EXPECT_EQ(Count<mc::ClComment>(program), 0u);
}

TEST(NcInterpreterTest, RotaryAddress_MappedByDialect) {
    mc::NcInterpretOptions options = DefaultOptions();
    options.dialect.rotary_address_to_register = {{'B', "B1"}};
    const mc::ClProgram program = Interpret("G90 G01 B10. F100\n", nullptr, options);
    const auto gotos = Records<mc::ClGoto>(program);
    ASSERT_EQ(gotos.size(), 1u);
    EXPECT_TRUE(gotos[0]->axis_words.Contains("B1"));
}

TEST(NcInterpreterTest, RotaryAddress_UnknownWarnsAndIgnored) {
    const mc::ClProgram program = Interpret("G90 G01 X1. U5. F100\n");
    EXPECT_TRUE(HasWarning(program, "unsupported address U"));
    const auto gotos = Records<mc::ClGoto>(program);
    ASSERT_EQ(gotos.size(), 1u);
    EXPECT_FALSE(gotos[0]->axis_words.Contains("U"));
}

TEST(NcInterpreterTest, LinearAddress_MappedByDialect) {
    mc::NcInterpretOptions options = DefaultOptions();
    options.dialect.linear_address_to_register = {{'X', "X1"}, {'Y', "Y"}, {'Z', "Z"}};
    const mc::ClProgram program = Interpret("G90 G01 X1. F100\n", nullptr, options);
    const auto gotos = Records<mc::ClGoto>(program);
    ASSERT_EQ(gotos.size(), 1u);
    EXPECT_NEAR(gotos[0]->axis_words.At("X1"), 1.0, kTol);
}

// ---- 状態の引き継ぎ ----

TEST(NcInterpreterTest, State_CarriesAcrossPrograms) {
    mc::NcState state;
    Interpret("T5M06\nG55\nG43.5 H2\nG90 G01 X0. Y0. Z0. I0. J0. K1. F300\n", &state);
    const mc::ClProgram second = Interpret("G01 X1. Y1. Z1.\n", &state);
    EXPECT_EQ(std::get<mc::ClLoadTool>(second.records[0]).number, 5);
    EXPECT_EQ(std::get<mc::ClSelectWorkOffset>(second.records[1]).id, "G55");
    EXPECT_EQ(std::get<mc::ClLengthOffset>(second.records[2]).number, 2);
    EXPECT_NEAR(std::get<mc::ClFeed>(second.records[3]).mm_per_s,
                300.0 / mc::kSecondsPerMinute, kTol);
    const auto gotos = Records<mc::ClGoto>(second);
    ASSERT_EQ(gotos.size(), 1u);
    ASSERT_TRUE(gotos[0]->point.has_value());
    ExpectVector(*gotos[0]->point, 1.0, 1.0, 1.0);
}

TEST(NcInterpreterTest, State_ResetAtProgramStartKeepsToolAndOffset) {
    mc::NcInterpretOptions options = DefaultOptions();
    options.dialect.reset_modal_at_program_start = true;
    mc::NcState state;
    Interpret("T5M06\nG55\nG43.5 H2\nG91 G01 X0. Y0. Z0. I0. J0. K1. F300\n",
              &state, options);
    const mc::ClProgram second = Interpret("G01 X1.\n", &state, options);
    EXPECT_EQ(std::get<mc::ClLoadTool>(second.records[0]).number, 5);
    EXPECT_EQ(std::get<mc::ClSelectWorkOffset>(second.records[1]).id, "G55");
    EXPECT_EQ(Count<mc::ClLengthOffset>(second), 0u);
    EXPECT_EQ(Count<mc::ClFeed>(second), 0u);
    EXPECT_TRUE(state.absolute);
    const auto gotos = Records<mc::ClGoto>(second);
    ASSERT_EQ(gotos.size(), 1u);
    EXPECT_FALSE(gotos[0]->point.has_value());
}

TEST(NcInterpreterTest, State_NullptrStartsFromDialectDefaults) {
    mc::NcInterpretOptions options = DefaultOptions();
    options.dialect.defaults.absolute = false;
    const mc::ClProgram program =
            Interpret("G01 X1. F100\nX1.\n", nullptr, options);
    const auto gotos = Records<mc::ClGoto>(program);
    ASSERT_EQ(gotos.size(), 2u);
    EXPECT_NEAR(gotos[1]->axis_words.At("X"), 2.0, kTol);
}

// ---- 字句解析の設定 ----

TEST(NcInterpreterTest, Lex_BlockSkipRemovesMarkedBlocks) {
    mc::NcLexOptions lex;
    lex.block_skip = {1};
    const mc::ClProgram program =
            Interpret("G90 G01 X1. F100\n/ X99.\n/2 X98.\n", nullptr, DefaultOptions(), lex);
    const auto gotos = Records<mc::ClGoto>(program);
    ASSERT_EQ(gotos.size(), 2u);
    EXPECT_NEAR(gotos[1]->axis_words.At("X"), 98.0, kTol);
}

TEST(NcInterpreterTest, Lex_LineRangeLimitsBlocks) {
    mc::NcLexOptions lex;
    lex.start_line = 2;
    lex.end_line = 2;
    const mc::ClProgram program =
            Interpret("G90 G01 X1. F100\nG90 G01 X2. F100\nX3.\n", nullptr,
                      DefaultOptions(), lex);
    const auto gotos = Records<mc::ClGoto>(program);
    ASSERT_EQ(gotos.size(), 1u);
    EXPECT_NEAR(gotos[0]->axis_words.At("X"), 2.0, kTol);
    EXPECT_EQ(program.sources.back().line, 2);
}

// ---- ファイル ----

TEST(NcInterpreterTest, File_CodesCoverageParsesWithoutError) {
    mc::NcLexOptions lex;
    lex.block_skip = {1};
    mc::NcInterpretOptions options = DefaultOptions();
    options.program_index = 3;
    const mc::ClProgram program =
            mc::InterpretNcFile(kNcDir / "codes.nc", lex, options, nullptr);
    EXPECT_EQ(Count<mc::ClEnd>(program), 1u);
    EXPECT_EQ(Count<mc::ClDwell>(program), 2u);
    // 6つの円弧はいずれもTCP無効 (G43) 下にあり、座標語の`ClGoto`列になる
    EXPECT_EQ(Count<mc::ClArc>(program), 0u);
    EXPECT_EQ(program.sources.back().program_index, 3);
    // ブロックスキップ1の行は読まれず、2の行は読まれる
    bool found_98 = false;
    bool found_99 = false;
    for (const mc::ClGoto* motion : Records<mc::ClGoto>(program)) {
        if (!motion->axis_words.Contains("X")) continue;
        found_98 = found_98 || std::abs(motion->axis_words.At("X") - 98.0) < kTol;
        found_99 = found_99 || std::abs(motion->axis_words.At("X") - 99.0) < kTol;
    }
    EXPECT_TRUE(found_98);
    EXPECT_FALSE(found_99);
    // 工具交換は T2 M06 と T3M06 の2回 (初期レコードを含めて3件)
    EXPECT_EQ(Count<mc::ClLoadTool>(program), 3u);
}

TEST(NcInterpreterTest, File_ThrowsFileOpenErrorWhenMissing) {
    EXPECT_THROW(mc::InterpretNcFile(kNcDir / "missing.nc", {}, DefaultOptions(), nullptr),
                 igesio::FileOpenError);
}

TEST(NcInterpreterTest, RealHeader_TcpIjkIsInterpreted) {
    mc::NcState state;
    const mc::ClProgram program =
            mc::InterpretNcFile(kNcDir / "tcp_ijk.nc", {}, DefaultOptions(), &state);
    // 警告はベンダーMの集計 (info) のみ
    for (const mc::Diagnostic& warning : program.warnings) {
        EXPECT_EQ(warning.severity, mc::Severity::kInfo) << warning.message;
    }
    // 先頭の G49 は初期状態 (補正なし) と同じなのでレコードにならない
    const auto offsets = Records<mc::ClLengthOffset>(program);
    ASSERT_EQ(offsets.size(), 1u);
    EXPECT_EQ(offsets[0]->number, 12);
    const auto tools = Records<mc::ClLoadTool>(program);
    ASSERT_EQ(tools.size(), 2u);
    EXPECT_EQ(tools[1]->number, 12);

    // G43.5 以降の移動は全て制御点を持ち、IJK を書いたブロックは工具軸を持つ
    bool after_tcp = false;
    std::size_t with_axis = 0;
    for (const mc::ClRecord& record : program.records) {
        if (const auto* offset = std::get_if<mc::ClLengthOffset>(&record)) {
            after_tcp = after_tcp || offset->number.has_value();
        }
        const auto* motion = std::get_if<mc::ClGoto>(&record);
        if (motion == nullptr || !after_tcp || motion->frame == mc::MotionFrame::kMachine) {
            continue;
        }
        EXPECT_TRUE(motion->point.has_value());
        if (motion->tool_axis.has_value()) ++with_axis;
    }
    EXPECT_EQ(with_axis, 37u);
    bool found_feed = false;
    for (const mc::ClFeed* feed : Records<mc::ClFeed>(program)) {
        found_feed = found_feed
                     || std::abs(feed->mm_per_s - 200.0 / mc::kSecondsPerMinute) < kTol;
    }
    EXPECT_TRUE(found_feed);
    EXPECT_TRUE(state.ended);
}

TEST(NcInterpreterTest, RealHeader_RotaryWordsIsInterpreted) {
    const mc::ClProgram program =
            mc::InterpretNcFile(kNcDir / "rotary_words.nc", {}, DefaultOptions(), nullptr);
    EXPECT_TRUE(program.warnings.empty());
    std::size_t with_rotary = 0;
    for (const mc::ClGoto* motion : Records<mc::ClGoto>(program)) {
        if (motion->axis_words.Contains("B")) ++with_rotary;
    }
    EXPECT_EQ(with_rotary, 5u);
}
