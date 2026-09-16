/**
 * @file tests/extensions/machines/toolpath/test_nc_writer.cpp
 * @brief ポストプロセッサ (toolpath/nc_writer) のテスト
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note 対象: WriteNcToString / WriteNc / NcWriteOptions::Validate
 *       - 正常系 (代表値): 出力の並び (`%`/O番号/ヘッダ/フッタ/M30)、テンプレート変数,
 *         前置モーダル、モーダル省略 (動作コード/座標/平面/G53)、TCPの3形式,
 *         円弧 (3平面/法線の向き/折れ線化/全円)、Fコードの出力、各状態レコード,
 *         構文の置き換え (区切り/N番号/`%`/コメント形式/小数桁)、期待出力 (D200Z),
 *         NC→CL→NCの往復
 *       - 正常系 (境界値): プログラム番号0、弦誤差の微小正値、レコード無し
 *       - 正常系 (退化): 始点不明の円弧 (警告してスキップ)、工具軸無し (+Zを仮定)
 *       - 異常系: `Validate`の不備で`std::invalid_argument`、`kRotaryWords`で回転軸の指令が
 *         無い移動で`igesio::DataFormatError`
 *       TODO: `{date}`変数は実行日に依存するため値を検証しない (展開されることのみ)
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/core/formatting.h"
#include "igesio/extensions/machines/core/text_file.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/machine/axis_values.h"
#include "igesio/extensions/machines/toolpath/cl_io.h"
#include "igesio/extensions/machines/toolpath/cl_program.h"
#include "igesio/extensions/machines/toolpath/nc_block.h"
#include "igesio/extensions/machines/toolpath/nc_dialect.h"
#include "igesio/extensions/machines/toolpath/nc_interpreter.h"
#include "igesio/extensions/machines/toolpath/nc_writer.h"
#include "../machine/machines_for_testing.h"
#include "dialects_for_testing.h"

namespace {

namespace mc = igesio::extensions::machines;
namespace fs = std::filesystem;
using igesio::Vector3d;

/// @brief 座標・方向の比較の許容誤差
constexpr double kTol = 1e-6;
/// @brief 送りの比較の許容誤差 [mm/s]
constexpr double kFeedTol = 1e-9;

/// @brief テストデータのディレクトリ (tests/test_data/machines)
const fs::path kDataDir = machines_test::kFixturePath.parent_path();

/// @brief 制御点のみの直線移動 (工具軸は直前値を継続する)
mc::ClGoto Goto(const mc::MotionKind kind, const double x, const double y,
                const double z) {
    mc::ClGoto motion;
    motion.kind = kind;
    motion.point = Vector3d(x, y, z);
    return motion;
}

/// @brief 制御点と工具軸を持つ直線移動
mc::ClGoto GotoAxis(const mc::MotionKind kind, const double x, const double y,
                    const double z, const double i, const double j, const double k) {
    mc::ClGoto motion = Goto(kind, x, y, z);
    motion.tool_axis = Vector3d(i, j, k).normalized();
    return motion;
}

/// @brief 軸の指令のみの直線移動 (`frame`は指定する)
mc::ClGoto GotoWords(const mc::MotionKind kind, const mc::NcValues& words,
                     const mc::MotionFrame frame) {
    mc::ClGoto motion;
    motion.kind = kind;
    motion.axis_words = words;
    motion.frame = frame;
    return motion;
}

/// @brief 円弧 (法線は正規化する)
mc::ClArc Arc(const double ex, const double ey, const double ez, const double cx,
              const double cy, const double cz, const double nx, const double ny,
              const double nz, const mc::MotionKind kind = mc::MotionKind::kArcCcw) {
    mc::ClArc arc;
    arc.kind = kind;
    arc.end = Vector3d(ex, ey, ez);
    arc.center = Vector3d(cx, cy, cz);
    arc.normal = Vector3d(nx, ny, nz).normalized();
    return arc;
}

/// @brief レコード列からプログラムを作る (出所は持たない)
mc::ClProgram Program(std::vector<mc::ClRecord> records) {
    mc::ClProgram program;
    program.records = std::move(records);
    return program;
}

/// @brief ヘッダ/フッタ/前置モーダルを出力しない設定 (本文の検証用)
mc::NcWriteOptions Bare() {
    mc::NcWriteOptions options;
    options.write_header_footer = false;
    options.preamble_modal_codes = false;
    return options;
}

/// @brief 出力を行に分ける (末尾の空要素は除く)
std::vector<std::string> Lines(const std::string& text) {
    std::vector<std::string> lines;
    std::string current;
    for (const char c : text) {
        if (c == '\n') {
            lines.push_back(current);
            current.clear();
        } else if (c != '\r') {
            current.push_back(c);
        }
    }
    if (!current.empty()) lines.push_back(current);
    return lines;
}

/// @brief 本文 (`%`、O番号、末尾のM30を除いた行) を取得する
std::vector<std::string> Body(const std::string& text) {
    std::vector<std::string> lines;
    for (const std::string& line : Lines(text)) {
        if (line == "%") continue;
        if (lines.empty() && !line.empty() && line.front() == 'O') continue;
        lines.push_back(line);
    }
    if (!lines.empty() && lines.back() == "M30") lines.pop_back();
    return lines;
}

/// @brief `Bare()`設定で本文を出力する
std::vector<std::string> WriteBody(const mc::ClProgram& program,
                                   const mc::NcWriteOptions& options = Bare(),
                                   std::vector<mc::Diagnostic>* warnings = nullptr) {
    return Body(mc::WriteNcToString(program, mc::DefaultFanucDialect(), options,
                                    warnings));
}

/// @brief 部分文字列を含む警告の数
std::size_t CountWarnings(const std::vector<mc::Diagnostic>& warnings,
                          const std::string& fragment) {
    std::size_t count = 0;
    for (const mc::Diagnostic& warning : warnings) {
        if (warning.message.find(fragment) != std::string::npos) ++count;
    }
    return count;
}

/// @brief 動作レコードと、その直前のモーダル状態の組 (往復比較用)
struct MotionSnapshot {
    /// @brief 動作レコード
    mc::ClRecord record;
    /// @brief 直前の状態
    mc::ClState state;
};

/// @brief 動作レコードごとに直前の状態を添えて列挙する
/// @note 状態レコードの個数や順序の違い (先頭の初期化レコード等) に依存せず,
///       動作時点の状態だけを比較するための表現
std::vector<MotionSnapshot> Snapshots(const mc::ClProgram& program) {
    std::vector<MotionSnapshot> snapshots;
    mc::ClState state;
    for (const mc::ClRecord& record : program.records) {
        if (mc::IsMotion(record)) snapshots.push_back(MotionSnapshot{record, state});
        state.Apply(record);
    }
    return snapshots;
}

/// @brief 2つのベクトルが許容誤差内で一致するか
bool Near(const Vector3d& a, const Vector3d& b) {
    return (a - b).norm() <= kTol;
}

/// @brief 2つの`optional<Vector3d>`が一致するか (両方無しも一致)
bool NearOptional(const std::optional<Vector3d>& a, const std::optional<Vector3d>& b) {
    if (a.has_value() != b.has_value()) return false;
    return !a.has_value() || Near(*a, *b);
}

/// @brief 2つの軸の指令が許容誤差内で一致するか
bool NearWords(const mc::NcValues& a, const mc::NcValues& b) {
    if (a.Size() != b.Size()) return false;
    for (const mc::NcEntry& entry : a.Entries()) {
        const std::optional<double> other = b.Get(entry.register_name);
        if (!other.has_value() || std::abs(*other - entry.value) > kTol) return false;
    }
    return true;
}

/// @brief 直線移動の幾何が一致するか
bool SameGoto(const mc::ClGoto& a, const mc::ClGoto& b) {
    return a.kind == b.kind && a.frame == b.frame && NearOptional(a.point, b.point)
           && NearWords(a.axis_words, b.axis_words);
}

/// @brief 円弧の幾何が一致するか (向きは法線と回転方向の積で比較する)
bool SameArc(const mc::ClArc& a, const mc::ClArc& b) {
    const double sa = a.kind == mc::MotionKind::kArcCcw ? 1.0 : -1.0;
    const double sb = b.kind == mc::MotionKind::kArcCcw ? 1.0 : -1.0;
    return Near(a.end, b.end) && Near(a.center, b.center)
           && Near(a.normal * sa, b.normal * sb) && a.full_turns == b.full_turns;
}

/// @brief 動作時点の状態が一致するか (送りは切削のみ比較する)
bool SameState(const MotionSnapshot& a, const MotionSnapshot& b) {
    const bool cutting = !std::holds_alternative<mc::ClDwell>(a.record)
                         && !(std::holds_alternative<mc::ClGoto>(a.record)
                              && std::get<mc::ClGoto>(a.record).kind
                                         == mc::MotionKind::kRapid);
    if (cutting) {
        if (a.state.feed.has_value() != b.state.feed.has_value()) return false;
        if (a.state.feed.has_value()
            && std::abs(*a.state.feed - *b.state.feed) > kFeedTol) {
            return false;
        }
    }
    return a.state.tool == b.state.tool && a.state.work_offset == b.state.work_offset
           && a.state.length_offset == b.state.length_offset
           && a.state.spindle.mode == b.state.spindle.mode
           && a.state.spindle.rpm == b.state.spindle.rpm
           && a.state.coolant == b.state.coolant
           && NearOptional(a.state.tool_axis, b.state.tool_axis);
}

/// @brief 2つの動作レコードが一致するか
bool SameMotion(const mc::ClRecord& a, const mc::ClRecord& b) {
    if (a.index() != b.index()) return false;
    if (const auto* ga = std::get_if<mc::ClGoto>(&a)) {
        return SameGoto(*ga, std::get<mc::ClGoto>(b));
    }
    if (const auto* aa = std::get_if<mc::ClArc>(&a)) {
        return SameArc(*aa, std::get<mc::ClArc>(b));
    }
    return std::abs(std::get<mc::ClDwell>(a).seconds
                    - std::get<mc::ClDwell>(b).seconds) <= kFeedTol;
}

/// @brief NC→CL→NC→CLの往復で動作と状態が一致することを検証する
void ExpectRoundTrip(const fs::path& nc_path, const mc::TcpStyle tcp) {
    mc::NcInterpretOptions interpret;
    interpret.dialect = mc::DefaultFanucDialect();
    interpret.keep_comments = false;
    const mc::ClProgram original =
            mc::InterpretNcFile(nc_path, mc::NcLexOptions{}, interpret, nullptr);
    ASSERT_FALSE(original.records.empty());

    mc::NcWriteOptions options = Bare();
    options.tcp = tcp;
    options.comments = false;
    const std::string text =
            mc::WriteNcToString(original, mc::DefaultFanucDialect(), options);
    const mc::ClProgram reread =
            mc::InterpretNc(text, mc::NcLexOptions{}, interpret, nullptr);

    const std::vector<MotionSnapshot> a = Snapshots(original);
    const std::vector<MotionSnapshot> b = Snapshots(reread);
    ASSERT_EQ(a.size(), b.size()) << text;
    for (std::size_t i = 0; i < a.size(); ++i) {
        EXPECT_TRUE(SameMotion(a[i].record, b[i].record))
                << "motion " << i << " (" << mc::ClRecordKindName(a[i].record) << ")";
        EXPECT_TRUE(SameState(a[i], b[i])) << "state before motion " << i;
    }
}

}  // namespace



// ---- 出力の並び ----

TEST(NcWriterTest, Layout_PercentAndProgramNumber) {
    const std::vector<std::string> lines = Lines(mc::WriteNcToString(
            Program({Goto(mc::MotionKind::kRapid, 0, 0, 10)}),
            mc::DefaultFanucDialect(), mc::NcWriteOptions{}));
    ASSERT_GE(lines.size(), 4u);
    EXPECT_EQ(lines[0], "%");
    EXPECT_EQ(lines[1], "O0001");
    EXPECT_EQ(lines[lines.size() - 2], "M30");
    EXPECT_EQ(lines.back(), "%");
}

TEST(NcWriterTest, Layout_ProgramNumberUsesDigitsAndPrefix) {
    mc::NcDialect dialect = mc::DefaultFanucDialect();
    dialect.syntax.program_prefix = ":";
    dialect.syntax.program_digits = 2;
    mc::NcWriteOptions options = Bare();
    options.program_number = 7;
    const std::vector<std::string> lines =
            Lines(mc::WriteNcToString(Program({}), dialect, options));
    ASSERT_GE(lines.size(), 2u);
    EXPECT_EQ(lines[1], ":07");
}

TEST(NcWriterTest, Layout_EmptyProgramHasOnlyFrame) {
    const std::vector<std::string> lines = Lines(mc::WriteNcToString(
            Program({}), mc::DefaultFanucDialect(), mc::NcWriteOptions{}));
    const std::vector<std::string> expected = {"%", "O0001", "M30", "%"};
    EXPECT_EQ(lines, expected);
}

TEST(NcWriterTest, Layout_HeaderTemplateUsesFirstOccurrence) {
    mc::NcDialect dialect = mc::DefaultFanucDialect();
    dialect.header = {"T{tool}M06", "S{spindle}M03", "F{feed}", "{work_offset}",
                      "H{h}", "({name})"};
    mc::ClProgram program = Program({
            mc::ClLoadTool{3}, mc::ClSpindle{mc::ClSpindle::Mode::kCw, 1200.0},
            mc::ClFeed{10.0}, mc::ClSelectWorkOffset{"G55"}, mc::ClLengthOffset{7},
            mc::ClLoadTool{4}});
    program.name = "PART";
    const std::vector<std::string> lines =
            Lines(mc::WriteNcToString(program, dialect, mc::NcWriteOptions{}));
    ASSERT_GE(lines.size(), 8u);
    EXPECT_EQ(lines[2], "T3M06");
    EXPECT_EQ(lines[3], "S1200M03");
    EXPECT_EQ(lines[4], "F600");
    EXPECT_EQ(lines[5], "G55");
    EXPECT_EQ(lines[6], "H7");
    EXPECT_EQ(lines[7], "(PART)");
}

TEST(NcWriterTest, Layout_FooterUsesLatestValues) {
    mc::NcDialect dialect = mc::DefaultFanucDialect();
    dialect.footer = {"(T{tool} S{spindle})"};
    const mc::ClProgram program = Program({
            mc::ClLoadTool{3}, mc::ClSpindle{mc::ClSpindle::Mode::kCw, 1200.0},
            mc::ClLoadTool{4}, mc::ClSpindle{mc::ClSpindle::Mode::kCw, 800.0}});
    const std::vector<std::string> lines =
            Lines(mc::WriteNcToString(program, dialect, mc::NcWriteOptions{}));
    ASSERT_GE(lines.size(), 3u);
    EXPECT_EQ(lines[lines.size() - 3], "(T4 S800)");
}

TEST(NcWriterTest, Layout_HeaderUnknownVariableIsEmptyWithWarning) {
    mc::NcDialect dialect = mc::DefaultFanucDialect();
    dialect.header = {"({foo})", "({date})"};
    std::vector<mc::Diagnostic> warnings;
    const std::vector<std::string> lines = Lines(mc::WriteNcToString(
            Program({}), dialect, mc::NcWriteOptions{}, &warnings));
    ASSERT_GE(lines.size(), 4u);
    EXPECT_EQ(lines[2], "()");
    EXPECT_EQ(lines[3].size(), std::string("(2026-09-15)").size());
    EXPECT_EQ(CountWarnings(warnings, "foo"), 1u);
}

TEST(NcWriterTest, Layout_FooterPrecedesProgramEnd) {
    mc::NcDialect dialect = mc::DefaultFanucDialect();
    dialect.footer = {"G91G28Z0", "M05"};
    const std::vector<std::string> lines = Lines(mc::WriteNcToString(
            Program({Goto(mc::MotionKind::kRapid, 0, 0, 10), mc::ClEnd{}}),
            dialect, mc::NcWriteOptions{}));
    const std::vector<std::string> expected = {
            "%", "O0001", "G00X0.Y0.Z10.I0.J0.K1.", "G91G28Z0", "M05", "M30", "%"};
    EXPECT_EQ(lines, expected);
}

TEST(NcWriterTest, Layout_ProgramEndAppendedWhenNoClEnd) {
    const std::vector<std::string> lines = Lines(mc::WriteNcToString(
            Program({mc::ClCoolant{true}}), mc::DefaultFanucDialect(), Bare()));
    const std::vector<std::string> expected = {"%", "O0001", "M08", "M30", "%"};
    EXPECT_EQ(lines, expected);
}

TEST(NcWriterTest, Layout_RecordsAfterEndSkippedWithWarning) {
    std::vector<mc::Diagnostic> warnings;
    const std::vector<std::string> lines = Lines(mc::WriteNcToString(
            Program({mc::ClEnd{}, mc::ClCoolant{true}, mc::ClCoolant{false}}),
            mc::DefaultFanucDialect(), Bare(), &warnings));
    const std::vector<std::string> expected = {"%", "O0001", "M30", "%"};
    EXPECT_EQ(lines, expected);
    EXPECT_EQ(CountWarnings(warnings, "record(s) after end"), 1u);
}

TEST(NcWriterTest, Layout_HeaderFooterDisabled) {
    mc::NcDialect dialect = mc::DefaultFanucDialect();
    dialect.header = {"(HEADER)"};
    dialect.footer = {"(FOOTER)"};
    mc::NcWriteOptions options = Bare();
    const std::vector<std::string> lines =
            Lines(mc::WriteNcToString(Program({}), dialect, options));
    const std::vector<std::string> expected = {"%", "O0001", "M30", "%"};
    EXPECT_EQ(lines, expected);
}

TEST(NcWriterTest, Layout_HeaderAndFooterOverride) {
    mc::NcDialect dialect = mc::DefaultFanucDialect();
    dialect.header = {"(HEADER)"};
    dialect.footer = {"(FOOTER)"};
    mc::NcWriteOptions options;
    options.header = std::vector<std::string>{"(CUSTOM HEADER)"};
    options.footer = std::vector<std::string>{};
    const std::vector<std::string> lines =
            Lines(mc::WriteNcToString(Program({}), dialect, options));
    const std::vector<std::string> expected = {"%", "O0001", "(CUSTOM HEADER)", "M30", "%"};
    EXPECT_EQ(lines, expected);
}

TEST(NcWriterTest, Preamble_WritesCodesDifferingFromDefaults) {
    mc::NcDialect dialect = mc::DefaultFanucDialect();
    dialect.defaults.absolute = false;
    dialect.defaults.metric = false;
    dialect.defaults.feed_per_minute = false;
    dialect.defaults.plane = mc::ArcPlane::kZX;
    mc::NcWriteOptions options = Bare();
    options.preamble_modal_codes = true;
    const std::vector<std::string> lines =
            Lines(mc::WriteNcToString(Program({}), dialect, options));
    ASSERT_GE(lines.size(), 3u);
    EXPECT_EQ(lines[2], "G90G21G94G17");
}

TEST(NcWriterTest, Preamble_EmptyWhenDefaultsMatch) {
    mc::NcWriteOptions options = Bare();
    options.preamble_modal_codes = true;
    const std::vector<std::string> lines = Lines(
            mc::WriteNcToString(Program({}), mc::DefaultFanucDialect(), options));
    const std::vector<std::string> expected = {"%", "O0001", "M30", "%"};
    EXPECT_EQ(lines, expected);
}

// ---- モーダル省略 ----

TEST(NcWriterTest, Modal_MotionCodeSuppressed) {
    mc::NcWriteOptions options = Bare();
    options.tcp = mc::TcpStyle::kNone;
    const std::vector<std::string> body = WriteBody(Program({
            Goto(mc::MotionKind::kLinear, 0, 0, 0),
            Goto(mc::MotionKind::kLinear, 10, 0, 0),
            Goto(mc::MotionKind::kRapid, 10, 0, 5)}), options);
    const std::vector<std::string> expected = {"G01X0.Y0.Z0.", "X10.", "G00Z5."};
    EXPECT_EQ(body, expected);
}

TEST(NcWriterTest, Modal_MotionCodeWrittenWhenSuppressionOff) {
    mc::NcWriteOptions options = Bare();
    options.tcp = mc::TcpStyle::kNone;
    options.suppress.motion_code = false;
    const std::vector<std::string> body = WriteBody(Program({
            Goto(mc::MotionKind::kLinear, 0, 0, 0),
            Goto(mc::MotionKind::kLinear, 10, 0, 0)}), options);
    const std::vector<std::string> expected = {"G01X0.Y0.Z0.", "G01X10."};
    EXPECT_EQ(body, expected);
}

TEST(NcWriterTest, Modal_CoordinatesWrittenWhenSuppressionOff) {
    mc::NcWriteOptions options = Bare();
    options.tcp = mc::TcpStyle::kNone;
    options.suppress.coordinates = false;
    const std::vector<std::string> body = WriteBody(Program({
            Goto(mc::MotionKind::kLinear, 0, 0, 0),
            Goto(mc::MotionKind::kLinear, 10, 0, 0)}), options);
    const std::vector<std::string> expected = {"G01X0.Y0.Z0.", "X10.Y0.Z0."};
    EXPECT_EQ(body, expected);
}

TEST(NcWriterTest, Modal_PlaneCodeOnlyWhenChanged) {
    mc::NcWriteOptions options = Bare();
    options.tcp = mc::TcpStyle::kNone;
    const std::vector<std::string> body = WriteBody(Program({
            Goto(mc::MotionKind::kLinear, 10, 0, 0),
            Arc(0, 10, 0, 0, 0, 0, 0, 0, 1),
            Arc(-10, 0, 0, 0, 0, 0, 0, 0, 1),
            Arc(0, 0, 10, 0, 0, 0, 0, 1, 0)}), options);
    const std::vector<std::string> expected = {
            "G01X10.Y0.Z0.", "G03X0.Y10.I-10.J0.", "G03X-10.Y0.I0.J-10.",
            "G18G03X0.Z10.I10.K0."};
    EXPECT_EQ(body, expected);
}

TEST(NcWriterTest, Modal_PlaneCodeAlwaysWhenSuppressionOff) {
    mc::NcWriteOptions options = Bare();
    options.tcp = mc::TcpStyle::kNone;
    options.suppress.plane = false;
    const std::vector<std::string> body = WriteBody(Program({
            Goto(mc::MotionKind::kLinear, 10, 0, 0),
            Arc(0, 10, 0, 0, 0, 0, 0, 0, 1)}), options);
    ASSERT_EQ(body.size(), 2u);
    EXPECT_EQ(body[1], "G17G03X0.Y10.I-10.J0.");
}

TEST(NcWriterTest, Modal_MotionCodeWrittenAfterArc) {
    mc::NcWriteOptions options = Bare();
    options.tcp = mc::TcpStyle::kNone;
    const std::vector<std::string> body = WriteBody(Program({
            Goto(mc::MotionKind::kLinear, 10, 0, 0),
            Arc(0, 10, 0, 0, 0, 0, 0, 0, 1),
            Goto(mc::MotionKind::kLinear, 0, 20, 0)}), options);
    ASSERT_EQ(body.size(), 3u);
    EXPECT_EQ(body[2], "G01Y20.");
}

TEST(NcWriterTest, Modal_MachineFrameAlwaysFull) {
    mc::NcWriteOptions options = Bare();
    options.tcp = mc::TcpStyle::kNone;
    const std::vector<std::string> body = WriteBody(Program({
            Goto(mc::MotionKind::kRapid, 0, 0, 10),
            GotoWords(mc::MotionKind::kRapid, {{"Z", 0.0}, {"B", 0.0}},
                      mc::MotionFrame::kMachine),
            Goto(mc::MotionKind::kRapid, 0, 0, 10)}), options);
    const std::vector<std::string> expected = {
            "G00X0.Y0.Z10.", "G53G00Z0.B0.", "G00X0.Y0.Z10."};
    EXPECT_EQ(body, expected);
}

TEST(NcWriterTest, Modal_LinearAxisWordsUseAddressTable) {
    mc::NcDialect dialect = mc::DefaultFanucDialect();
    dialect.linear_address_to_register = {{'X', "X1"}, {'Y', "Y1"}, {'Z', "Z1"}};
    mc::NcWriteOptions options = Bare();
    options.tcp = mc::TcpStyle::kNone;
    const std::string text = mc::WriteNcToString(Program({
            GotoWords(mc::MotionKind::kLinear, {{"X1", 5.0}, {"Z1", -1.0}},
                      mc::MotionFrame::kWork)}), dialect, options);
    const std::vector<std::string> body = Body(text);
    const std::vector<std::string> expected = {"G01X5.Z-1."};
    EXPECT_EQ(body, expected);
}

TEST(NcWriterTest, Modal_UnknownAxisWordOmittedWithWarning) {
    mc::NcWriteOptions options = Bare();
    options.tcp = mc::TcpStyle::kNone;
    std::vector<mc::Diagnostic> warnings;
    const std::vector<std::string> body = WriteBody(Program({
            GotoWords(mc::MotionKind::kLinear, {{"U", 5.0}}, mc::MotionFrame::kWork)}),
            options, &warnings);
    const std::vector<std::string> expected = {"G01"};
    EXPECT_EQ(body, expected);
    EXPECT_EQ(CountWarnings(warnings, "no NC address"), 1u);
}

// ---- TCPの形式 ----

TEST(NcWriterTest, Tcp_VectorWritesIjkInVectorFormat) {
    const std::vector<std::string> body = WriteBody(Program({
            GotoAxis(mc::MotionKind::kRapid, 1, 2, 3, 0, 1, 1)}));
    const std::vector<std::string> expected = {"G00X1.Y2.Z3.I0.J0.707107K0.707107"};
    EXPECT_EQ(body, expected);
}

TEST(NcWriterTest, Tcp_VectorContinuesPreviousAxis) {
    const std::vector<std::string> body = WriteBody(Program({
            GotoAxis(mc::MotionKind::kRapid, 0, 0, 10, 1, 0, 0),
            Goto(mc::MotionKind::kLinear, 5, 0, 10)}));
    ASSERT_EQ(body.size(), 2u);
    EXPECT_EQ(body[1], "G01X5.I1.J0.K0.");
}

TEST(NcWriterTest, Tcp_VectorAssumesPlusZWithOneWarning) {
    std::vector<mc::Diagnostic> warnings;
    const std::vector<std::string> body = WriteBody(Program({
            Goto(mc::MotionKind::kRapid, 0, 0, 10),
            Goto(mc::MotionKind::kLinear, 5, 0, 10)}), Bare(), &warnings);
    const std::vector<std::string> expected = {
            "G00X0.Y0.Z10.I0.J0.K1.", "G01X5.I0.J0.K1."};
    EXPECT_EQ(body, expected);
    EXPECT_EQ(CountWarnings(warnings, "+Z is assumed"), 1u);
}

TEST(NcWriterTest, Tcp_VectorSuppressesUnchangedAxisWhenEnabled) {
    mc::NcWriteOptions options = Bare();
    options.suppress.tool_axis = true;
    const std::vector<std::string> body = WriteBody(Program({
            GotoAxis(mc::MotionKind::kRapid, 0, 0, 10, 0, 0, 1),
            GotoAxis(mc::MotionKind::kLinear, 5, 0, 10, 0, 0, 1),
            GotoAxis(mc::MotionKind::kLinear, 6, 0, 10, 1, 0, 0)}), options);
    const std::vector<std::string> expected = {
            "G00X0.Y0.Z10.I0.J0.K1.", "G01X5.", "X6.I1.J0.K0."};
    EXPECT_EQ(body, expected);
}

TEST(NcWriterTest, Tcp_VectorKeepsRotaryWordsOfAxisWordOnlyMotion) {
    const std::vector<std::string> body = WriteBody(Program({
            GotoWords(mc::MotionKind::kRapid, {{"B", 0.0}, {"C", mc::kQuarterTurn}},
                      mc::MotionFrame::kWork)}));
    const std::vector<std::string> expected = {"G00B0.C90."};
    EXPECT_EQ(body, expected);
}

TEST(NcWriterTest, Tcp_RotaryWordsWritesDegrees) {
    mc::NcWriteOptions options = Bare();
    options.tcp = mc::TcpStyle::kRotaryWords;
    mc::ClGoto motion = Goto(mc::MotionKind::kLinear, 1, 2, 3);
    motion.axis_words = {{"A", mc::kQuarterTurn / 2.0}, {"C", mc::kQuarterTurn}};
    const std::vector<std::string> body = WriteBody(Program({motion}), options);
    const std::vector<std::string> expected = {"G01X1.Y2.Z3.A45.C90."};
    EXPECT_EQ(body, expected);
}

TEST(NcWriterTest, Tcp_RotaryWordsModalWhenStateHasWords) {
    mc::NcWriteOptions options = Bare();
    options.tcp = mc::TcpStyle::kRotaryWords;
    mc::ClGoto first = Goto(mc::MotionKind::kLinear, 1, 2, 3);
    first.axis_words = {{"A", 0.0}};
    const std::vector<std::string> body = WriteBody(Program({
            first, Goto(mc::MotionKind::kLinear, 4, 2, 3)}), options);
    const std::vector<std::string> expected = {"G01X1.Y2.Z3.A0.", "X4."};
    EXPECT_EQ(body, expected);
}

TEST(NcWriterTest, Tcp_RotaryWordsThrowsDataFormatErrorWhenMissing) {
    mc::NcWriteOptions options = Bare();
    options.tcp = mc::TcpStyle::kRotaryWords;
    const mc::ClProgram program = Program({GotoAxis(mc::MotionKind::kLinear,
                                                    1, 2, 3, 0, 0, 1)});
    EXPECT_THROW(mc::WriteNcToString(program, mc::DefaultFanucDialect(), options),
                 igesio::DataFormatError);
}

TEST(NcWriterTest, Tcp_NoneOmitsToolAxis) {
    mc::NcWriteOptions options = Bare();
    options.tcp = mc::TcpStyle::kNone;
    const std::vector<std::string> body = WriteBody(Program({
            GotoAxis(mc::MotionKind::kRapid, 1, 2, 3, 0, 1, 1)}), options);
    const std::vector<std::string> expected = {"G00X1.Y2.Z3."};
    EXPECT_EQ(body, expected);
}

TEST(NcWriterTest, Tcp_LengthOffsetCodeFollowsStyle) {
    const mc::ClProgram program = Program({mc::ClLengthOffset{5},
                                           mc::ClLengthOffset{std::nullopt}});
    mc::NcWriteOptions vector = Bare();
    EXPECT_EQ(WriteBody(program, vector), (std::vector<std::string>{"G43.5H5", "G49"}));
    mc::NcWriteOptions rotary = Bare();
    rotary.tcp = mc::TcpStyle::kRotaryWords;
    EXPECT_EQ(WriteBody(program, rotary), (std::vector<std::string>{"G43.4H5", "G49"}));
    mc::NcWriteOptions none = Bare();
    none.tcp = mc::TcpStyle::kNone;
    EXPECT_EQ(WriteBody(program, none), (std::vector<std::string>{"G43H5", "G49"}));
}

TEST(NcWriterTest, Tcp_MissingLengthOffsetWarns) {
    std::vector<mc::Diagnostic> warnings;
    WriteBody(Program({GotoAxis(mc::MotionKind::kRapid, 0, 0, 10, 0, 0, 1)}),
              Bare(), &warnings);
    EXPECT_EQ(CountWarnings(warnings, "no length offset"), 1u);
}

TEST(NcWriterTest, Tcp_NoWarningWhenLengthOffsetPrecedesMotion) {
    std::vector<mc::Diagnostic> warnings;
    WriteBody(Program({mc::ClLengthOffset{1},
                       GotoAxis(mc::MotionKind::kRapid, 0, 0, 10, 0, 0, 1)}),
              Bare(), &warnings);
    EXPECT_EQ(CountWarnings(warnings, "no length offset"), 0u);
}

TEST(NcWriterTest, Tcp_NoLengthOffsetWarningForNone) {
    mc::NcWriteOptions options = Bare();
    options.tcp = mc::TcpStyle::kNone;
    std::vector<mc::Diagnostic> warnings;
    WriteBody(Program({GotoAxis(mc::MotionKind::kRapid, 0, 0, 10, 0, 0, 1)}),
              options, &warnings);
    EXPECT_EQ(CountWarnings(warnings, "no length offset"), 0u);
}

// ---- 円弧 ----

TEST(NcWriterTest, Arc_XyPlaneCounterclockwise) {
    mc::NcWriteOptions options = Bare();
    options.tcp = mc::TcpStyle::kNone;
    const std::vector<std::string> body = WriteBody(Program({
            Goto(mc::MotionKind::kLinear, 10, 0, 0),
            Arc(20, 10, 0, 10, 10, 0, 0, 0, 1)}), options);
    ASSERT_EQ(body.size(), 2u);
    EXPECT_EQ(body[1], "G03X20.Y10.I0.J10.");
}

TEST(NcWriterTest, Arc_ClockwiseKindWritesG02) {
    mc::NcWriteOptions options = Bare();
    options.tcp = mc::TcpStyle::kNone;
    const std::vector<std::string> body = WriteBody(Program({
            Goto(mc::MotionKind::kLinear, 10, 0, 0),
            Arc(20, 10, 0, 10, 10, 0, 0, 0, 1, mc::MotionKind::kArcCw)}), options);
    ASSERT_EQ(body.size(), 2u);
    EXPECT_EQ(body[1], "G02X20.Y10.I0.J10.");
}

TEST(NcWriterTest, Arc_NegativeNormalFlipsDirection) {
    mc::NcWriteOptions options = Bare();
    options.tcp = mc::TcpStyle::kNone;
    const std::vector<std::string> body = WriteBody(Program({
            Goto(mc::MotionKind::kLinear, 10, 0, 0),
            Arc(20, 10, 0, 10, 10, 0, 0, 0, -1)}), options);
    ASSERT_EQ(body.size(), 2u);
    EXPECT_EQ(body[1], "G02X20.Y10.I0.J10.");
}

TEST(NcWriterTest, Arc_ZxAndYzPlanes) {
    mc::NcWriteOptions options = Bare();
    options.tcp = mc::TcpStyle::kNone;
    const std::vector<std::string> body = WriteBody(Program({
            Goto(mc::MotionKind::kLinear, 10, 0, 0),
            Arc(0, 0, 10, 0, 0, 0, 0, 1, 0),
            Goto(mc::MotionKind::kLinear, 0, 10, 0),
            Arc(0, 0, 10, 0, 0, 0, 1, 0, 0)}), options);
    ASSERT_EQ(body.size(), 4u);
    EXPECT_EQ(body[1], "G18G03X0.Z10.I-10.K0.");
    EXPECT_EQ(body[3], "G19G03Y0.Z10.J-10.K0.");
}

TEST(NcWriterTest, Arc_ThirdCoordinateWrittenOnlyWhenChanged) {
    mc::NcWriteOptions options = Bare();
    options.tcp = mc::TcpStyle::kNone;
    const std::vector<std::string> body = WriteBody(Program({
            Goto(mc::MotionKind::kLinear, 10, 0, 0),
            Arc(0, 10, 2, 0, 0, 0, 0, 0, 1)}), options);
    ASSERT_EQ(body.size(), 2u);
    EXPECT_EQ(body[1], "G03X0.Y10.Z2.I-10.J0.");
}

TEST(NcWriterTest, Arc_FullCircle) {
    mc::NcWriteOptions options = Bare();
    options.tcp = mc::TcpStyle::kNone;
    mc::ClArc arc = Arc(10, 0, 0, 0, 0, 0, 0, 0, 1);
    arc.full_turns = 1;
    const std::vector<std::string> body = WriteBody(Program({
            Goto(mc::MotionKind::kLinear, 10, 0, 0), arc}), options);
    ASSERT_EQ(body.size(), 2u);
    EXPECT_EQ(body[1], "G03X10.Y0.I-10.J0.");
}

TEST(NcWriterTest, Arc_OffPlaneLinearizedWithWarning) {
    mc::NcWriteOptions options = Bare();
    options.tcp = mc::TcpStyle::kNone;
    std::vector<mc::Diagnostic> warnings;
    const std::vector<std::string> body = WriteBody(Program({
            Goto(mc::MotionKind::kLinear, 10, 0, 0),
            Arc(0, 10, 0, 0, 0, 0, 0.1, 0, 1)}), options, &warnings);
    ASSERT_GT(body.size(), 2u);
    for (const std::string& line : body) {
        EXPECT_EQ(line.find("G02"), std::string::npos) << line;
        EXPECT_EQ(line.find("G03"), std::string::npos) << line;
    }
    EXPECT_EQ(body.back().find("X0.Y10."), 0u) << body.back();
    EXPECT_EQ(CountWarnings(warnings, "1 arc(s) linearized"), 1u);
}

TEST(NcWriterTest, Arc_LinearizeOptionAppliesToPlanarArcs) {
    mc::NcWriteOptions options = Bare();
    options.tcp = mc::TcpStyle::kNone;
    options.arc = mc::ArcOutput::kLinearize;
    options.arc_chord_tolerance = 1.0;
    std::vector<mc::Diagnostic> warnings;
    const std::vector<std::string> body = WriteBody(Program({
            Goto(mc::MotionKind::kLinear, 10, 0, 0),
            Arc(0, 10, 0, 0, 0, 0, 0, 0, 1)}), options, &warnings);
    // R=10、90°、ε=1: N = ⌈(π/2) / (2 arccos(0.9))⌉ = 2
    const std::vector<std::string> expected = {
            "G01X10.Y0.Z0.", "X7.0711Y7.0711", "X0.Y10."};
    EXPECT_EQ(body, expected);
    EXPECT_EQ(CountWarnings(warnings, "linearized"), 1u);
}

TEST(NcWriterTest, Arc_LinearizedEndCarriesToolAxis) {
    mc::NcWriteOptions options = Bare();
    options.arc = mc::ArcOutput::kLinearize;
    options.arc_chord_tolerance = 1.0;
    mc::ClArc arc = Arc(0, 10, 0, 0, 0, 0, 0, 0, 1);
    arc.tool_axis = Vector3d(1, 0, 0);
    const std::vector<std::string> body = WriteBody(Program({
            mc::ClLengthOffset{1},
            GotoAxis(mc::MotionKind::kLinear, 10, 0, 0, 0, 0, 1), arc}), options);
    ASSERT_EQ(body.size(), 4u);
    EXPECT_EQ(body[2], "X7.0711Y7.0711I0.J0.K1.");
    EXPECT_EQ(body[3], "X0.Y10.I1.J0.K0.");
}

TEST(NcWriterTest, Arc_SkippedWithWarningWhenStartUnknown) {
    mc::NcWriteOptions options = Bare();
    options.tcp = mc::TcpStyle::kNone;
    std::vector<mc::Diagnostic> warnings;
    const std::vector<std::string> body = WriteBody(Program({
            Arc(0, 10, 0, 0, 0, 0, 0, 0, 1)}), options, &warnings);
    EXPECT_TRUE(body.empty());
    EXPECT_EQ(CountWarnings(warnings, "arc start position is unknown"), 1u);
}

TEST(NcWriterTest, Arc_ToolAxisChangeWarnsUnderVector) {
    std::vector<mc::Diagnostic> warnings;
    mc::ClArc arc = Arc(0, 10, 0, 0, 0, 0, 0, 0, 1);
    arc.tool_axis = Vector3d(1, 0, 0);
    const std::vector<std::string> body = WriteBody(Program({
            mc::ClLengthOffset{1},
            GotoAxis(mc::MotionKind::kLinear, 10, 0, 0, 0, 0, 1), arc}),
            Bare(), &warnings);
    ASSERT_EQ(body.size(), 3u);
    EXPECT_EQ(body[2], "G03X0.Y10.I-10.J0.");
    EXPECT_EQ(CountWarnings(warnings, "tool axis change along an arc"), 1u);
}

// ---- 送り ----

TEST(NcWriterTest, Feed_OnChangeWritesOnlyWhenChanged) {
    mc::NcWriteOptions options = Bare();
    options.tcp = mc::TcpStyle::kNone;
    const std::vector<std::string> body = WriteBody(Program({
            mc::ClFeed{5.0},
            Goto(mc::MotionKind::kLinear, 1, 0, 0),
            Goto(mc::MotionKind::kLinear, 2, 0, 0),
            mc::ClFeed{10.0},
            Goto(mc::MotionKind::kLinear, 3, 0, 0)}), options);
    const std::vector<std::string> expected = {
            "G01X1.Y0.Z0.F300", "X2.", "X3.F600"};
    EXPECT_EQ(body, expected);
}

TEST(NcWriterTest, Feed_EveryCutBlock) {
    mc::NcWriteOptions options = Bare();
    options.tcp = mc::TcpStyle::kNone;
    options.feed = mc::FeedOutput::kEveryCutBlock;
    const std::vector<std::string> body = WriteBody(Program({
            mc::ClFeed{5.0},
            Goto(mc::MotionKind::kLinear, 1, 0, 0),
            Goto(mc::MotionKind::kLinear, 2, 0, 0)}), options);
    const std::vector<std::string> expected = {"G01X1.Y0.Z0.F300", "X2.F300"};
    EXPECT_EQ(body, expected);
}

TEST(NcWriterTest, Feed_NeverOnRapid) {
    mc::NcWriteOptions options = Bare();
    options.tcp = mc::TcpStyle::kNone;
    const std::vector<std::string> body = WriteBody(Program({
            mc::ClFeed{5.0},
            Goto(mc::MotionKind::kRapid, 1, 0, 0),
            Goto(mc::MotionKind::kLinear, 2, 0, 0)}), options);
    const std::vector<std::string> expected = {"G00X1.Y0.Z0.", "G01X2.F300"};
    EXPECT_EQ(body, expected);
}

TEST(NcWriterTest, Feed_OmittedWhenUnknown) {
    mc::NcWriteOptions options = Bare();
    options.tcp = mc::TcpStyle::kNone;
    const std::vector<std::string> body = WriteBody(Program({
            Goto(mc::MotionKind::kLinear, 1, 0, 0)}), options);
    const std::vector<std::string> expected = {"G01X1.Y0.Z0."};
    EXPECT_EQ(body, expected);
}

// ---- 状態レコード ----

TEST(NcWriterTest, Records_DwellUsesScaleAndAddress) {
    EXPECT_EQ(WriteBody(Program({mc::ClDwell{0.5}})),
              (std::vector<std::string>{"G04P500"}));
    mc::NcDialect dialect = mc::DefaultFanucDialect();
    dialect.vocab.dwell_address = 'X';
    dialect.vocab.dwell_scale = 1.0;
    const std::vector<std::string> body =
            Body(mc::WriteNcToString(Program({mc::ClDwell{0.5}}), dialect, Bare()));
    EXPECT_EQ(body, (std::vector<std::string>{"G04X0.5"}));
}

TEST(NcWriterTest, Records_Spindle) {
    const std::vector<std::string> body = WriteBody(Program({
            mc::ClSpindle{mc::ClSpindle::Mode::kCw, 5000.0},
            mc::ClSpindle{mc::ClSpindle::Mode::kCcw, std::nullopt},
            mc::ClSpindle{mc::ClSpindle::Mode::kOff, std::nullopt},
            mc::ClSpindle{mc::ClSpindle::Mode::kOff, 100.0}}));
    const std::vector<std::string> expected = {"S5000M03", "M04", "M05", "S100M05"};
    EXPECT_EQ(body, expected);
}

TEST(NcWriterTest, Records_Coolant) {
    const std::vector<std::string> body = WriteBody(Program({
            mc::ClCoolant{true}, mc::ClCoolant{false}}));
    const std::vector<std::string> expected = {"M08", "M09"};
    EXPECT_EQ(body, expected);
}

TEST(NcWriterTest, Records_LoadToolWithTemplate) {
    mc::NcDialect dialect = mc::DefaultFanucDialect();
    dialect.tool_change = {"T{tool}", "M06 ({tool})"};
    const std::vector<std::string> body = Body(mc::WriteNcToString(
            Program({mc::ClLoadTool{7}}), dialect, Bare()));
    const std::vector<std::string> expected = {"T7", "M06 (7)"};
    EXPECT_EQ(body, expected);
}

TEST(NcWriterTest, Records_LoadToolWithoutTemplate) {
    mc::NcDialect dialect = mc::DefaultFanucDialect();
    dialect.syntax.word_separator = " ";
    const std::vector<std::string> body = Body(mc::WriteNcToString(
            Program({mc::ClLoadTool{7}}), dialect, Bare()));
    const std::vector<std::string> expected = {"T7 M06"};
    EXPECT_EQ(body, expected);
}

TEST(NcWriterTest, Records_WorkOffsetStandardWords) {
    const std::vector<std::string> body = WriteBody(Program({
            mc::ClSelectWorkOffset{"G55"}, mc::ClSelectWorkOffset{"G54.1P2"}}));
    const std::vector<std::string> expected = {"G55", "G54.1P2"};
    EXPECT_EQ(body, expected);
}

TEST(NcWriterTest, Records_WorkOffsetMappedId) {
    mc::NcDialect dialect = mc::DefaultFanucDialect();
    dialect.work_offset_ids = {{"G55", "fixture"}};
    const std::vector<std::string> body = Body(mc::WriteNcToString(
            Program({mc::ClSelectWorkOffset{"fixture"}}), dialect, Bare()));
    const std::vector<std::string> expected = {"G55"};
    EXPECT_EQ(body, expected);
}

TEST(NcWriterTest, Records_WorkOffsetUnknownIdCommentedWithWarning) {
    std::vector<mc::Diagnostic> warnings;
    const std::vector<std::string> body = WriteBody(Program({
            mc::ClSelectWorkOffset{"custom"}}), Bare(), &warnings);
    const std::vector<std::string> expected = {"(WORK OFFSET custom)"};
    EXPECT_EQ(body, expected);
    EXPECT_EQ(CountWarnings(warnings, "custom"), 1u);
}

TEST(NcWriterTest, Records_CommentAndMarkers) {
    const std::vector<std::string> body = WriteBody(Program({
            mc::ClComment{"hello"},
            mc::ClMarker{mc::ClMarker::Kind::kPathBegin, "p1"},
            mc::ClMarker{mc::ClMarker::Kind::kPathEnd, "p1"},
            mc::ClMarker{mc::ClMarker::Kind::kOperation, "op"}}));
    const std::vector<std::string> expected = {
            "(hello)", "(Path Start p1)", "(Path End p1)", "(Operation op)"};
    EXPECT_EQ(body, expected);
}

TEST(NcWriterTest, Records_CommentsDisabled) {
    mc::NcWriteOptions options = Bare();
    options.comments = false;
    const std::vector<std::string> body = WriteBody(Program({
            mc::ClComment{"hello"},
            mc::ClMarker{mc::ClMarker::Kind::kPathBegin, "p1"}}), options);
    EXPECT_TRUE(body.empty());
}

TEST(NcWriterTest, Records_CommentParenthesesStripped) {
    const std::vector<std::string> body = WriteBody(Program({mc::ClComment{"a(b)c"}}));
    const std::vector<std::string> expected = {"(abc)"};
    EXPECT_EQ(body, expected);
}

TEST(NcWriterTest, Records_RoleCommentsOnChange) {
    mc::NcWriteOptions options = Bare();
    options.tcp = mc::TcpStyle::kNone;
    options.role_comments = true;
    mc::ClGoto approach = Goto(mc::MotionKind::kRapid, 0, 0, 10);
    approach.role = mc::PathRole::kApproach;
    mc::ClGoto cut1 = Goto(mc::MotionKind::kLinear, 0, 0, 0);
    mc::ClGoto cut2 = Goto(mc::MotionKind::kLinear, 5, 0, 0);
    mc::ClGoto retract = Goto(mc::MotionKind::kLinear, 5, 0, 10);
    retract.role = mc::PathRole::kRetract;
    const std::vector<std::string> body =
            WriteBody(Program({approach, cut1, cut2, retract}), options);
    const std::vector<std::string> expected = {
            "(Approach)", "G00X0.Y0.Z10.", "(Cut)", "G01Z0.", "X5.",
            "(Retract)", "Z10."};
    EXPECT_EQ(body, expected);
}

TEST(NcWriterTest, Records_PassThroughNcVerbatim) {
    const std::vector<std::string> body = WriteBody(Program({
            mc::ClPassThrough{"nc", "M1413 S1"}}));
    const std::vector<std::string> expected = {"M1413 S1"};
    EXPECT_EQ(body, expected);
}

TEST(NcWriterTest, Records_PassThroughNcCommentedWhenDisabled) {
    mc::NcWriteOptions options = Bare();
    options.passthrough = false;
    std::vector<mc::Diagnostic> warnings;
    const std::vector<std::string> body = WriteBody(Program({
            mc::ClPassThrough{"nc", "M1413 S1"}}), options, &warnings);
    const std::vector<std::string> expected = {"(M1413 S1)"};
    EXPECT_EQ(body, expected);
    EXPECT_TRUE(warnings.empty());
}

TEST(NcWriterTest, Records_PassThroughAptCommentedWithWarning) {
    std::vector<mc::Diagnostic> warnings;
    const std::vector<std::string> body = WriteBody(Program({
            mc::ClPassThrough{"apt", "CUTTER/10"}}), Bare(), &warnings);
    const std::vector<std::string> expected = {"(CUTTER/10)"};
    EXPECT_EQ(body, expected);
    EXPECT_EQ(CountWarnings(warnings, "dialect 'apt'"), 1u);
}

// ---- 構文の置き換え ----

TEST(NcWriterTest, Syntax_WordSeparator) {
    mc::NcWriteOptions options = Bare();
    options.tcp = mc::TcpStyle::kNone;
    mc::NcSyntax syntax;
    syntax.word_separator = " ";
    options.syntax = syntax;
    const std::vector<std::string> body = WriteBody(Program({
            mc::ClFeed{5.0}, Goto(mc::MotionKind::kLinear, 10, 0, 0)}), options);
    const std::vector<std::string> expected = {"G01 X10. Y0. Z0. F300"};
    EXPECT_EQ(body, expected);
}

TEST(NcWriterTest, Syntax_LineNumbers) {
    mc::NcWriteOptions options = Bare();
    options.tcp = mc::TcpStyle::kNone;
    mc::NcSyntax syntax;
    syntax.line_number_step = 10;
    syntax.line_number_digits = 3;
    options.syntax = syntax;
    const std::vector<std::string> lines = Lines(mc::WriteNcToString(
            Program({mc::ClComment{"c"}, Goto(mc::MotionKind::kRapid, 1, 0, 0)}),
            mc::DefaultFanucDialect(), options));
    const std::vector<std::string> expected = {
            "%", "N010O0001", "N020(c)", "N030G00X1.Y0.Z0.", "N040M30", "%"};
    EXPECT_EQ(lines, expected);
}

TEST(NcWriterTest, Syntax_NoPercent) {
    mc::NcWriteOptions options = Bare();
    mc::NcSyntax syntax;
    syntax.percent = false;
    options.syntax = syntax;
    const std::vector<std::string> lines = Lines(
            mc::WriteNcToString(Program({}), mc::DefaultFanucDialect(), options));
    const std::vector<std::string> expected = {"O0001", "M30"};
    EXPECT_EQ(lines, expected);
}

TEST(NcWriterTest, Syntax_SemicolonComments) {
    mc::NcWriteOptions options = Bare();
    mc::NcSyntax syntax;
    syntax.comment_open = ";";
    syntax.comment_close = "";
    options.syntax = syntax;
    const std::vector<std::string> body =
            WriteBody(Program({mc::ClComment{"a(b)c"}}), options);
    const std::vector<std::string> expected = {";a(b)c"};
    EXPECT_EQ(body, expected);
}

TEST(NcWriterTest, Syntax_NumberFormats) {
    mc::NcWriteOptions options = Bare();
    mc::NcSyntax syntax;
    syntax.coordinate = mc::NcNumberFormat{2, true};
    syntax.vector = mc::NcNumberFormat{3, false, true, false};
    syntax.feed = mc::NcNumberFormat{1};
    options.syntax = syntax;
    const std::vector<std::string> body = WriteBody(Program({
            mc::ClLengthOffset{1}, mc::ClFeed{5.0},
            GotoAxis(mc::MotionKind::kLinear, 10, 0.5, 0, 0, 0.5, 0.5)}), options);
    const std::vector<std::string> expected = {
            "G43.5H1", "G01X10.00Y0.50Z0.00I0.J.707K.707F300."};
    EXPECT_EQ(body, expected);
}

TEST(NcWriterTest, Syntax_OptionsOverrideDialect) {
    mc::NcDialect dialect = mc::DefaultFanucDialect();
    dialect.syntax.word_separator = " ";
    mc::NcWriteOptions options = Bare();
    options.tcp = mc::TcpStyle::kNone;
    options.syntax = mc::NcSyntax{};
    const std::vector<std::string> body = Body(mc::WriteNcToString(
            Program({Goto(mc::MotionKind::kRapid, 1, 0, 0)}), dialect, options));
    const std::vector<std::string> expected = {"G00X1.Y0.Z0."};
    EXPECT_EQ(body, expected);
}

// ---- 設定の検査 ----

TEST(NcWriterTest, Validate_AcceptsDefaultsAndProgramNumberZero) {
    mc::NcWriteOptions options;
    EXPECT_FALSE(options.Validate().has_value());
    options.program_number = 0;
    EXPECT_FALSE(options.Validate().has_value());
    options.arc_chord_tolerance = 1e-9;
    EXPECT_FALSE(options.Validate().has_value());
}

TEST(NcWriterTest, Validate_ReturnsMessageForNegativeProgramNumber) {
    mc::NcWriteOptions options;
    options.program_number = -1;
    const std::optional<std::string> message = options.Validate();
    ASSERT_TRUE(message.has_value());
    EXPECT_NE(message->find("program_number"), std::string::npos);
}

TEST(NcWriterTest, Options_ThrowsInvalidArgumentWhenProgramNumberNegative) {
    mc::NcWriteOptions options;
    options.program_number = -1;
    EXPECT_THROW(mc::WriteNcToString(Program({}), mc::DefaultFanucDialect(), options),
                 std::invalid_argument);
}

TEST(NcWriterTest, Options_ThrowsInvalidArgumentWhenChordToleranceNotPositive) {
    mc::NcWriteOptions options;
    options.arc_chord_tolerance = 0.0;
    EXPECT_THROW(mc::WriteNcToString(Program({}), mc::DefaultFanucDialect(), options),
                 std::invalid_argument);
    options.arc_chord_tolerance = -0.1;
    EXPECT_THROW(mc::WriteNcToString(Program({}), mc::DefaultFanucDialect(), options),
                 std::invalid_argument);
    options.arc_chord_tolerance = 1e-9;
    EXPECT_NO_THROW(mc::WriteNcToString(Program({}), mc::DefaultFanucDialect(), options));
}

TEST(NcWriterTest, Options_ThrowsInvalidArgumentWhenSyntaxInvalid) {
    mc::NcWriteOptions options;
    mc::NcSyntax syntax;
    syntax.line_number_step = 0;
    options.syntax = syntax;
    EXPECT_THROW(mc::WriteNcToString(Program({}), mc::DefaultFanucDialect(), options),
                 std::invalid_argument);
    syntax.line_number_step = 1;
    syntax.program_digits = 0;
    options.syntax = syntax;
    EXPECT_THROW(mc::WriteNcToString(Program({}), mc::DefaultFanucDialect(), options),
                 std::invalid_argument);
    syntax.program_digits = 1;
    syntax.line_number_digits = 0;
    options.syntax = syntax;
    EXPECT_THROW(mc::WriteNcToString(Program({}), mc::DefaultFanucDialect(), options),
                 std::invalid_argument);
    syntax.line_number_digits = 1;
    options.syntax = syntax;
    EXPECT_NO_THROW(mc::WriteNcToString(Program({}), mc::DefaultFanucDialect(), options));
}

// ---- ファイル出力 ----

TEST(NcWriterTest, WriteNc_WritesFileWithRequestedNewline) {
    const fs::path dir = fs::temp_directory_path() / "igesio_nc_writer_test";
    fs::create_directories(dir);
    const fs::path path = dir / "out.nc";
    mc::NcWriteOptions options = Bare();
    options.newline = mc::NewlineStyle::kCrlf;
    mc::WriteNc(path, Program({mc::ClCoolant{true}}), mc::DefaultFanucDialect(),
                options);
    const std::string text = mc::ReadTextFile(path);
    EXPECT_EQ(text, "%\r\nO0001\r\nM08\r\nM30\r\n%\r\n");
    fs::remove_all(dir);
}

TEST(NcWriterTest, WriteNc_ThrowsInvalidArgumentBeforeCreatingFile) {
    const fs::path dir = fs::temp_directory_path() / "igesio_nc_writer_test_invalid";
    fs::create_directories(dir);
    const fs::path path = dir / "out.nc";
    mc::NcWriteOptions options;
    options.program_number = -1;
    EXPECT_THROW(mc::WriteNc(path, Program({}), mc::DefaultFanucDialect(), options),
                 std::invalid_argument);
    EXPECT_FALSE(fs::exists(path));
    fs::remove_all(dir);
}

// ---- 期待出力と往復 ----

TEST(NcWriterTest, Golden_D200z) {
    const mc::ClProgram program =
            mc::ReadClFile(kDataDir / "cl" / "apt.cl", mc::ClFileFormat::kApt);
    mc::NcWriteOptions options;
    options.program_number = 22;
    std::vector<mc::Diagnostic> warnings;
    const std::string text = mc::WriteNcToString(
            program, toolpath_test::MakinoD200zDialect(), options, &warnings);
    // 期待出力ファイルはgitの改行変換 (autocrlf) でCRLFになることがあるので,
    // 出力 (LF固定) と比較する前にCRを除く
    std::string expected = mc::ReadTextFile(kDataDir / "nc" / "expected_d200z.nc");
    expected.erase(std::remove(expected.begin(), expected.end(), '\r'), expected.end());
    EXPECT_EQ(text, expected);
    EXPECT_EQ(CountWarnings(warnings, "no length offset"), 1u);
}

TEST(NcWriterTest, RoundTrip_TcpIjk) {
    ExpectRoundTrip(kDataDir / "nc" / "tcp_ijk.nc", mc::TcpStyle::kVector);
}

TEST(NcWriterTest, RoundTrip_RotaryWords) {
    ExpectRoundTrip(kDataDir / "nc" / "rotary_words.nc", mc::TcpStyle::kRotaryWords);
}
