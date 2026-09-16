/**
 * @file tests/extensions/machines/simulation/test_axis_resolution.cpp
 * @brief 工具軸方向から回転軸の指令への変換 (simulation/axis_resolution) のテスト
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note 対象: ResolveAxisWords
 *       - 正常系: 回転軸の指令の書き込みと姿勢IKとの一致、`keep_tool_axis`,
 *         回転角の解の連続性、直前の指令値の引き継ぎ、G43.4形式での出力
 *       - 正常系 (退化): 対象外のレコード (機械座標、制御点なし、回転軸の指令あり)
 *         は変更しない、工具軸方向の無いプログラム (+Zの仮定)
 *       - 警告: 到達不能で直前の回転軸の指令を書く
 *       TODO: 対応しない軸構成の`NotImplementedError`は逆運動学側で検証済み
 */
#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <optional>
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
#include "igesio/extensions/machines/toolpath/cl_program.h"
#include "igesio/extensions/machines/toolpath/nc_dialect.h"
#include "igesio/extensions/machines/toolpath/nc_writer.h"
#include "../machine/machines_for_testing.h"
#include "../project/projects_for_testing.h"

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
