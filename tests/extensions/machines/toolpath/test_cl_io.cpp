/**
 * @file tests/extensions/machines/toolpath/test_cl_io.cpp
 * @brief CLファイルの読み書き (toolpath/cl_io) のテスト
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note 対象: ReadClText / ReadClFile / WriteClText / WriteClFile (3行1組・APT)
 *       - 正常系 (代表値): 3行1組の早送り/切削の判別と軸の正規化、行番号,
 *         単位換算と行範囲、APTの各文 (GOTO/RAPID/FEDRAT/LOADTL/SPINDL/COOLNT/
 *         DELAY/MULTAX/CIRCLE/PARTNO/PPRINT/$$/FINI)、出力と読み戻しの一致
 *       - 正常系 (境界値): 行数がちょうど3の倍数、GOTOの引数3個と6個
 *       - 正常系 (退化): 空の入力、工具軸の無い移動 (+Zを仮定)、FINIの無い出力
 *       - 異常系: 行数が3の倍数でない、数値でない行、ゼロベクトルの軸,
 *         引数の数の不備、ON/OFF以外の引数 (いずれも`DataFormatError`)
 *       TODO: `ReadClFile`の`FileOpenError`は`core/text_file`側で検証する
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
#include "igesio/extensions/machines/core/formatting.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/toolpath/cl_io.h"
#include "igesio/extensions/machines/toolpath/cl_program.h"
#include "../machine/machines_for_testing.h"

namespace {

namespace mc = igesio::extensions::machines;
using igesio::Vector3d;

/// @brief 座標・方向の比較の許容誤差
constexpr double kTol = 1e-9;

/// @brief 出力の小数桁 (5桁) による往復の許容誤差
constexpr double kRoundTripTol = 1e-5;

/// @brief CLテストデータのディレクトリ (tests/test_data/machines/cl)
const std::filesystem::path kClDir = machines_test::kFixturePath.parent_path() / "cl";

/// @brief 3行1組の最小入力 (早送り1組)
const std::string kOneTriplet = "1 2 3\n0 0 1\n7f\n";

/// @brief ベクトルの各成分を比較する
void ExpectVector(const Vector3d& actual, const Vector3d& expected,
                  const double tol = kTol) {
    EXPECT_NEAR(actual.x(), expected.x(), tol);
    EXPECT_NEAR(actual.y(), expected.y(), tol);
    EXPECT_NEAR(actual.z(), expected.z(), tol);
}

/// @brief 指定型のレコードを取得する (型が違えばテスト失敗)
template <typename T>
const T& RecordAs(const mc::ClProgram& program, const std::size_t index) {
    const T* record = std::get_if<T>(&program.records.at(index));
    EXPECT_NE(record, nullptr) << "record " << index << " is "
                               << mc::ClRecordKindName(program.records.at(index));
    static const T kEmpty{};
    return record != nullptr ? *record : kEmpty;
}

/// @brief 制御点と工具軸を持つ直線移動を作る
mc::ClGoto Goto(const mc::MotionKind kind, const Vector3d& point,
                const std::optional<Vector3d>& axis = std::nullopt) {
    mc::ClGoto motion;
    motion.kind = kind;
    motion.point = point;
    motion.tool_axis = axis;
    return motion;
}

/// @brief 円弧を作る (中心・終点・法線)
mc::ClArc Arc(const mc::MotionKind kind, const Vector3d& center, const Vector3d& end,
              const Vector3d& normal) {
    mc::ClArc arc;
    arc.kind = kind;
    arc.center = center;
    arc.end = end;
    arc.normal = normal;
    return arc;
}

/// @brief 円弧の実効法線 (時計回りは法線を反転した反時計回りと等価)
Vector3d EffectiveNormal(const mc::ClArc& arc) {
    return arc.kind == mc::MotionKind::kArcCw ? Vector3d(-arc.normal) : arc.normal;
}

/// @brief 3行1組の往復用プログラム
/// @note 早送り (軸+Z) → 切削 (軸省略 = 継続) → 四分円 (中心(10,10,0)、(10,0,0)→(20,10,0))
///       の動作と、表せない状態レコード (工具・送り) を含む
mc::ClProgram TripletSample() {
    mc::ClProgram program;
    program.records.push_back(mc::ClLoadTool{1});
    program.records.push_back(Goto(mc::MotionKind::kRapid, Vector3d(10.0, 0.0, 10.0),
                                   Vector3d(0.0, 0.0, 1.0)));
    program.records.push_back(mc::ClFeed{5.0});
    program.records.push_back(Goto(mc::MotionKind::kLinear, Vector3d(10.0, 0.0, 0.0)));
    program.records.push_back(Arc(mc::MotionKind::kArcCcw, Vector3d(10.0, 10.0, 0.0),
                                  Vector3d(20.0, 10.0, 0.0), Vector3d(0.0, 0.0, 1.0)));
    return program;
}

/// @brief APTの往復用プログラム
/// @note 全レコード種別を含む. 円弧は時計回り (法線+Z) で、出力では法線が反転する
mc::ClProgram AptSample() {
    mc::ClProgram program;
    program.name = "ROUND TRIP";
    program.records.push_back(mc::ClComment{"header"});
    program.records.push_back(mc::ClLoadTool{3});
    program.records.push_back(mc::ClSpindle{mc::ClSpindle::Mode::kCcw, 1200.0});
    program.records.push_back(mc::ClCoolant{true});
    program.records.push_back(mc::ClFeed{10.0});
    program.records.push_back(mc::ClSelectWorkOffset{"G55"});
    program.records.push_back(mc::ClLengthOffset{7});
    program.records.push_back(mc::ClMarker{mc::ClMarker::Kind::kPathBegin, "p1"});
    program.records.push_back(Goto(mc::MotionKind::kRapid, Vector3d(0.0, 0.0, 5.0),
                                   Vector3d(0.0, 0.0, 1.0)));
    program.records.push_back(Goto(mc::MotionKind::kLinear, Vector3d(10.0, 0.0, 0.0)));
    program.records.push_back(Arc(mc::MotionKind::kArcCw, Vector3d(10.0, 10.0, 0.0),
                                  Vector3d(0.0, 10.0, 0.0), Vector3d(0.0, 0.0, 1.0)));
    program.records.push_back(mc::ClDwell{0.25});
    program.records.push_back(mc::ClMarker{mc::ClMarker::Kind::kPathEnd, "p1"});
    program.records.push_back(mc::ClPassThrough{"apt", "CUTCOM/OFF"});
    program.records.push_back(mc::ClPassThrough{"nc", "M11M13"});
    program.records.push_back(mc::ClEnd{});
    return program;
}

/// @brief 一時ファイルのパス (テスト名ごとに分ける)
std::filesystem::path TempPath(const std::string& name) {
    return std::filesystem::temp_directory_path() / ("igesio_cl_io_" + name);
}

/// @brief 指定型のレコード数を数える
template <typename T>
std::size_t CountRecords(const mc::ClProgram& program) {
    std::size_t count = 0;
    for (const mc::ClRecord& record : program.records) {
        if (std::holds_alternative<T>(record)) ++count;
    }
    return count;
}

}  // namespace



// ---- 3行1組の読込 ----

TEST(ClIoTest, Triplet_RapidAndLinear) {
    const mc::ClProgram program =
            mc::ReadClFile(kClDir / "triplet.cl", mc::ClFileFormat::kTriplet);
    ASSERT_EQ(program.records.size(), 4u);
    EXPECT_TRUE(program.warnings.empty());
    EXPECT_TRUE(program.name.empty());

    // 連続する7fは2レコードになり、それ以外は切削
    EXPECT_EQ(RecordAs<mc::ClGoto>(program, 0).kind, mc::MotionKind::kRapid);
    EXPECT_EQ(RecordAs<mc::ClGoto>(program, 1).kind, mc::MotionKind::kRapid);
    EXPECT_EQ(RecordAs<mc::ClGoto>(program, 2).kind, mc::MotionKind::kLinear);
    EXPECT_EQ(RecordAs<mc::ClGoto>(program, 3).kind, mc::MotionKind::kLinear);
    ExpectVector(*RecordAs<mc::ClGoto>(program, 0).point, Vector3d(0.0, 0.0, 10.0));
    ExpectVector(*RecordAs<mc::ClGoto>(program, 3).point, Vector3d(20.0, 0.0, 0.0));

    // 軸は正規化される ((0,0,2) → (0,0,1))
    ExpectVector(*RecordAs<mc::ClGoto>(program, 3).tool_axis, Vector3d(0.0, 0.0, 1.0));

    // 出所は各組の1行目の元ファイル行番号 (空行・先頭空白を含む)
    ASSERT_EQ(program.sources.size(), 4u);
    EXPECT_EQ(program.sources[0].line, 1);
    EXPECT_EQ(program.sources[1].line, 4);
    EXPECT_EQ(program.sources[2].line, 8);
    EXPECT_EQ(program.sources[3].line, 11);
    EXPECT_EQ(program.sources[0].program_index, 0);
}

TEST(ClIoTest, Triplet_ProgramIndexIsRecorded) {
    mc::ClReadOptions options;
    options.program_index = 3;
    const mc::ClProgram program =
            mc::ReadClText(kOneTriplet, mc::ClFileFormat::kTriplet, options);
    ASSERT_EQ(program.sources.size(), 1u);
    EXPECT_EQ(program.sources[0].program_index, 3);
}

TEST(ClIoTest, Triplet_UnitScaleAndRange) {
    mc::ClReadOptions options;
    options.unit_scale = mc::kInchToMillimeter;
    const mc::ClProgram scaled =
            mc::ReadClFile(kClDir / "triplet.cl", mc::ClFileFormat::kTriplet, options);
    ASSERT_EQ(scaled.records.size(), 4u);
    // 座標は換算され、工具軸は換算されない
    ExpectVector(*RecordAs<mc::ClGoto>(scaled, 1).point,
                 Vector3d(10.0 * mc::kInchToMillimeter, 0.0, 10.0 * mc::kInchToMillimeter));
    ExpectVector(*RecordAs<mc::ClGoto>(scaled, 1).tool_axis, Vector3d(0.0, 0.0, 1.0));

    // 行範囲は空行を含む元ファイルの行番号で限定する
    mc::ClReadOptions tail;
    tail.start_line = 8;
    const mc::ClProgram last_two =
            mc::ReadClFile(kClDir / "triplet.cl", mc::ClFileFormat::kTriplet, tail);
    ASSERT_EQ(last_two.records.size(), 2u);
    EXPECT_EQ(last_two.sources[0].line, 8);

    mc::ClReadOptions head;
    head.end_line = 6;
    const mc::ClProgram first_two =
            mc::ReadClFile(kClDir / "triplet.cl", mc::ClFileFormat::kTriplet, head);
    ASSERT_EQ(first_two.records.size(), 2u);
    EXPECT_EQ(first_two.sources[1].line, 4);
}

TEST(ClIoTest, Triplet_EmptyInputHasNoRecords) {
    const mc::ClProgram program = mc::ReadClText("\n  \n", mc::ClFileFormat::kTriplet);
    EXPECT_TRUE(program.records.empty());
    EXPECT_TRUE(program.sources.empty());
}

TEST(ClIoTest, Triplet_AcceptsCrlfAndLowerUpperCode) {
    const mc::ClProgram program =
            mc::ReadClText("1 2 3\r\n0 0 1\r\n7F\r\n4 5 6\r\n0 1 0\r\n80\r\n",
                           mc::ClFileFormat::kTriplet);
    ASSERT_EQ(program.records.size(), 2u);
    EXPECT_EQ(RecordAs<mc::ClGoto>(program, 0).kind, mc::MotionKind::kRapid);
    EXPECT_EQ(RecordAs<mc::ClGoto>(program, 1).kind, mc::MotionKind::kLinear);
}

TEST(ClIoTest, Triplet_ThrowsDataFormatErrorWhenLineCountIsNotMultipleOf3) {
    // 4行は不備、3行 (境界) は受理
    EXPECT_THROW(mc::ReadClText(kOneTriplet + "1 2 3\n", mc::ClFileFormat::kTriplet),
                 igesio::DataFormatError);
    EXPECT_NO_THROW(mc::ReadClText(kOneTriplet, mc::ClFileFormat::kTriplet));
}

TEST(ClIoTest, Triplet_ThrowsDataFormatErrorWhenLineIsNotNumeric) {
    EXPECT_THROW(mc::ReadClText("1 2 x\n0 0 1\n7f\n", mc::ClFileFormat::kTriplet),
                 igesio::DataFormatError);
    EXPECT_THROW(mc::ReadClText("1 2 3\n0 abc 1\n7f\n", mc::ClFileFormat::kTriplet),
                 igesio::DataFormatError);
}

TEST(ClIoTest, Triplet_ThrowsDataFormatErrorWhenCoordinateCountIsNot3) {
    EXPECT_THROW(mc::ReadClText("1 2\n0 0 1\n7f\n", mc::ClFileFormat::kTriplet),
                 igesio::DataFormatError);
    EXPECT_THROW(mc::ReadClText("1 2 3\n0 0 1 1\n7f\n", mc::ClFileFormat::kTriplet),
                 igesio::DataFormatError);
}

TEST(ClIoTest, Triplet_ThrowsDataFormatErrorWhenToolAxisIsZero) {
    EXPECT_THROW(mc::ReadClText("1 2 3\n0 0 0\n7f\n", mc::ClFileFormat::kTriplet),
                 igesio::DataFormatError);
    // 行番号が文言に含まれる
    try {
        mc::ReadClText("1 2 3\n0 0 0\n7f\n", mc::ClFileFormat::kTriplet);
        FAIL() << "expected DataFormatError";
    } catch (const igesio::DataFormatError& e) {
        EXPECT_NE(std::string(e.what()).find("line 2"), std::string::npos) << e.what();
    }
}



// ---- 3行1組の出力 ----

TEST(ClIoTest, Triplet_WriteAndRoundTrip) {
    const mc::ClProgram sample = TripletSample();
    std::vector<mc::Diagnostic> warnings;
    const std::string text =
            mc::WriteClText(sample, mc::ClFileFormat::kTriplet, {}, &warnings);

    // 早送りは7f、切削は80、小数5桁
    EXPECT_NE(text.find("10.00000 0.00000 10.00000\n0.00000 0.00000 1.00000\n7f\n"),
              std::string::npos) << text;
    EXPECT_NE(text.find("10.00000 0.00000 0.00000\n0.00000 0.00000 1.00000\n80\n"),
              std::string::npos) << text;

    // 状態レコード (工具・送り) は省いて警告1件
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_NE(warnings[0].message.find("2 record(s)"), std::string::npos)
            << warnings[0].message;

    // 読み直すと全て移動レコードで、円弧は折れ線化されて終点で終わる
    const mc::ClProgram back = mc::ReadClText(text, mc::ClFileFormat::kTriplet);
    ASSERT_GE(back.records.size(), 3u);
    EXPECT_EQ(back.records.size(), CountRecords<mc::ClGoto>(back));
    EXPECT_EQ(RecordAs<mc::ClGoto>(back, 0).kind, mc::MotionKind::kRapid);
    ExpectVector(*RecordAs<mc::ClGoto>(back, 0).point, Vector3d(10.0, 0.0, 10.0),
                 kRoundTripTol);
    EXPECT_EQ(RecordAs<mc::ClGoto>(back, 1).kind, mc::MotionKind::kLinear);
    ExpectVector(*RecordAs<mc::ClGoto>(back, 1).point, Vector3d(10.0, 0.0, 0.0),
                 kRoundTripTol);
    const mc::ClGoto& last = RecordAs<mc::ClGoto>(back, back.records.size() - 1);
    ExpectVector(*last.point, Vector3d(20.0, 10.0, 0.0), kRoundTripTol);
    // 折れ線の通過点は円上にある
    for (std::size_t i = 2; i < back.records.size(); ++i) {
        const Vector3d p = *RecordAs<mc::ClGoto>(back, i).point;
        EXPECT_NEAR((p - Vector3d(10.0, 10.0, 0.0)).norm(), 10.0, kRoundTripTol);
    }
}

TEST(ClIoTest, Triplet_WriteAssumesZAxisWhenToolAxisIsUnknown) {
    mc::ClProgram program;
    program.records.push_back(Goto(mc::MotionKind::kLinear, Vector3d(1.0, 2.0, 3.0)));
    std::vector<mc::Diagnostic> warnings;
    const std::string text =
            mc::WriteClText(program, mc::ClFileFormat::kTriplet, {}, &warnings);
    EXPECT_EQ(text, "1.00000 2.00000 3.00000\n0.00000 0.00000 1.00000\n80\n");
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_NE(warnings[0].message.find("no tool axis"), std::string::npos);
}

TEST(ClIoTest, Triplet_WriteUsesDecimalsAndNewline) {
    mc::ClProgram program;
    program.records.push_back(Goto(mc::MotionKind::kRapid, Vector3d(1.5, 0.0, 0.0),
                                   Vector3d(0.0, 0.0, 1.0)));
    mc::ClWriteOptions options;
    options.decimals = 2;
    options.newline = mc::NewlineStyle::kCrlf;
    EXPECT_EQ(mc::WriteClText(program, mc::ClFileFormat::kTriplet, options),
              "1.50 0.00 0.00\r\n0.00 0.00 1.00\r\n7f\r\n");
}

TEST(ClIoTest, Triplet_WriteEmptyProgramIsEmptyText) {
    std::vector<mc::Diagnostic> warnings;
    EXPECT_TRUE(mc::WriteClText(mc::ClProgram{}, mc::ClFileFormat::kTriplet, {},
                                &warnings).empty());
    EXPECT_TRUE(warnings.empty());
}

TEST(ClIoTest, Triplet_WriteFileAndReadBack) {
    const std::filesystem::path path = TempPath("triplet_round_trip.cl");
    mc::ClProgram program;
    program.records.push_back(Goto(mc::MotionKind::kRapid, Vector3d(1.0, 2.0, 3.0),
                                   Vector3d(0.0, 0.0, 1.0)));
    mc::WriteClFile(path, program, mc::ClFileFormat::kTriplet);
    const mc::ClProgram back = mc::ReadClFile(path, mc::ClFileFormat::kTriplet);
    std::filesystem::remove(path);
    ASSERT_EQ(back.records.size(), 1u);
    ExpectVector(*RecordAs<mc::ClGoto>(back, 0).point, Vector3d(1.0, 2.0, 3.0));
}



// ---- APTの読込 ----

TEST(ClIoTest, Apt_FileStatements) {
    const mc::ClProgram program = mc::ReadClFile(kClDir / "apt.cl", mc::ClFileFormat::kApt);
    EXPECT_TRUE(program.warnings.empty());
    EXPECT_EQ(program.name, "APT SAMPLE");
    ASSERT_EQ(program.records.size(), 13u);
    EXPECT_EQ(RecordAs<mc::ClComment>(program, 0).text, "sample APT source");
    EXPECT_EQ(RecordAs<mc::ClLoadTool>(program, 1).number, 1);
    EXPECT_EQ(RecordAs<mc::ClSpindle>(program, 2).mode, mc::ClSpindle::Mode::kCw);
    EXPECT_NEAR(*RecordAs<mc::ClSpindle>(program, 2).rpm, 5000.0, kTol);
    EXPECT_TRUE(RecordAs<mc::ClCoolant>(program, 3).on);
    EXPECT_NEAR(RecordAs<mc::ClFeed>(program, 4).mm_per_s, 300.0 / mc::kSecondsPerMinute,
                kTol);
    // RAPIDは直後のGOTOにだけ効く
    EXPECT_EQ(RecordAs<mc::ClGoto>(program, 5).kind, mc::MotionKind::kRapid);
    ExpectVector(*RecordAs<mc::ClGoto>(program, 5).point, Vector3d(0.0, 0.0, 10.0));
    ExpectVector(*RecordAs<mc::ClGoto>(program, 5).tool_axis, Vector3d(0.0, 0.0, 1.0));
    EXPECT_EQ(RecordAs<mc::ClGoto>(program, 6).kind, mc::MotionKind::kLinear);
    EXPECT_EQ(RecordAs<mc::ClGoto>(program, 7).kind, mc::MotionKind::kLinear);
    // CIRCLE直後のGOTOは円弧の終点
    const mc::ClArc& arc = RecordAs<mc::ClArc>(program, 8);
    EXPECT_EQ(arc.kind, mc::MotionKind::kArcCcw);
    ExpectVector(arc.center, Vector3d(10.0, 10.0, 0.0));
    ExpectVector(arc.end, Vector3d(20.0, 10.0, 0.0));
    ExpectVector(arc.normal, Vector3d(0.0, 0.0, 1.0));
    EXPECT_EQ(arc.full_turns, 0);
    ExpectVector(*arc.tool_axis, Vector3d(0.0, 0.0, 1.0));
    EXPECT_EQ(RecordAs<mc::ClComment>(program, 9).text, "comment");
    EXPECT_NEAR(RecordAs<mc::ClDwell>(program, 10).seconds, 0.5, kTol);
    ExpectVector(*RecordAs<mc::ClGoto>(program, 11).point, Vector3d(20.0, 10.0, 10.0));
    EXPECT_TRUE(std::holds_alternative<mc::ClEnd>(program.records[12]));
    // 出所は元ファイルの行番号
    ASSERT_EQ(program.sources.size(), 13u);
    EXPECT_EQ(program.sources[0].line, 2);
    EXPECT_EQ(program.sources[8].line, 13);
    EXPECT_EQ(program.sources[12].line, 17);
}

TEST(ClIoTest, Apt_RapidAppliesToNextGotoOnly) {
    const mc::ClProgram program =
            mc::ReadClText("RAPID\nGOTO/1,0,0\nGOTO/2,0,0\n", mc::ClFileFormat::kApt);
    ASSERT_EQ(program.records.size(), 2u);
    EXPECT_EQ(RecordAs<mc::ClGoto>(program, 0).kind, mc::MotionKind::kRapid);
    EXPECT_EQ(RecordAs<mc::ClGoto>(program, 1).kind, mc::MotionKind::kLinear);
    EXPECT_FALSE(RecordAs<mc::ClGoto>(program, 0).tool_axis.has_value());
}

TEST(ClIoTest, Apt_FedratUnits) {
    const mc::ClProgram program = mc::ReadClText(
            "FEDRAT/10,IPM\nFEDRAT/120\nFEDRAT/60,MMPM\n", mc::ClFileFormat::kApt);
    ASSERT_EQ(program.records.size(), 3u);
    EXPECT_NEAR(RecordAs<mc::ClFeed>(program, 0).mm_per_s,
                10.0 * mc::kInchToMillimeter / mc::kSecondsPerMinute, kTol);
    EXPECT_NEAR(RecordAs<mc::ClFeed>(program, 1).mm_per_s, 2.0, kTol);
    EXPECT_NEAR(RecordAs<mc::ClFeed>(program, 2).mm_per_s, 1.0, kTol);
    EXPECT_TRUE(program.warnings.empty());
}

TEST(ClIoTest, Apt_FedratUnknownUnitWarnsAndAssumesMmpm) {
    const mc::ClProgram program = mc::ReadClText("FEDRAT/60,XYZ\n", mc::ClFileFormat::kApt);
    ASSERT_EQ(program.records.size(), 1u);
    EXPECT_NEAR(RecordAs<mc::ClFeed>(program, 0).mm_per_s, 1.0, kTol);
    ASSERT_EQ(program.warnings.size(), 1u);
    EXPECT_EQ(program.warnings[0].line, 1);
    EXPECT_TRUE(program.warnings[0].context.empty());
}

TEST(ClIoTest, Apt_MultaxOffIgnoresAxisWithWarning) {
    const mc::ClProgram program = mc::ReadClText(
            "GOTO/1,2,3,0,0,1\nMULTAX/ON\nGOTO/4,5,6,0,2,0\nMULTAX/OFF\nGOTO/7,8,9,1,0,0\n",
            mc::ClFileFormat::kApt);
    ASSERT_EQ(program.records.size(), 3u);
    // MULTAX/ON前とOFF後の軸は無視されて警告、ON中は正規化して保持
    EXPECT_FALSE(RecordAs<mc::ClGoto>(program, 0).tool_axis.has_value());
    ExpectVector(*RecordAs<mc::ClGoto>(program, 1).tool_axis, Vector3d(0.0, 1.0, 0.0));
    EXPECT_FALSE(RecordAs<mc::ClGoto>(program, 2).tool_axis.has_value());
    ASSERT_EQ(program.warnings.size(), 2u);
    EXPECT_EQ(program.warnings[0].line, 1);
    EXPECT_EQ(program.warnings[1].line, 5);
}

TEST(ClIoTest, Apt_GotoWithThreeValuesHasNoAxisEvenWhenMultaxOn) {
    const mc::ClProgram program =
            mc::ReadClText("MULTAX/ON\nGOTO/1,2,3\n", mc::ClFileFormat::kApt);
    ASSERT_EQ(program.records.size(), 1u);
    EXPECT_FALSE(RecordAs<mc::ClGoto>(program, 0).tool_axis.has_value());
    EXPECT_TRUE(program.warnings.empty());
}

TEST(ClIoTest, Apt_SpindlVariants) {
    const mc::ClProgram program = mc::ReadClText(
            "SPINDL/OFF\nSPINDL/800,CCLW\nSPINDL/900\n", mc::ClFileFormat::kApt);
    ASSERT_EQ(program.records.size(), 3u);
    EXPECT_EQ(RecordAs<mc::ClSpindle>(program, 0).mode, mc::ClSpindle::Mode::kOff);
    EXPECT_FALSE(RecordAs<mc::ClSpindle>(program, 0).rpm.has_value());
    EXPECT_EQ(RecordAs<mc::ClSpindle>(program, 1).mode, mc::ClSpindle::Mode::kCcw);
    EXPECT_NEAR(*RecordAs<mc::ClSpindle>(program, 1).rpm, 800.0, kTol);
    EXPECT_EQ(RecordAs<mc::ClSpindle>(program, 2).mode, mc::ClSpindle::Mode::kCw);
}

TEST(ClIoTest, Apt_CoolntLoadtlDelayCaseInsensitive) {
    const mc::ClProgram program = mc::ReadClText(
            "coolnt/off\nloadtl/12\ndelay/1.5\nfini\n", mc::ClFileFormat::kApt);
    ASSERT_EQ(program.records.size(), 4u);
    EXPECT_FALSE(RecordAs<mc::ClCoolant>(program, 0).on);
    EXPECT_EQ(RecordAs<mc::ClLoadTool>(program, 1).number, 12);
    EXPECT_NEAR(RecordAs<mc::ClDwell>(program, 2).seconds, 1.5, kTol);
    EXPECT_TRUE(std::holds_alternative<mc::ClEnd>(program.records[3]));
}

TEST(ClIoTest, Apt_PartnoPprintAndDollarComment) {
    const mc::ClProgram program = mc::ReadClText(
            "PARTNO PART A\nPPRINT/hello\n$$ note\nPPRINT  spaced text \n",
            mc::ClFileFormat::kApt);
    EXPECT_EQ(program.name, "PART A");
    ASSERT_EQ(program.records.size(), 3u);
    EXPECT_EQ(RecordAs<mc::ClComment>(program, 0).text, "hello");
    EXPECT_EQ(RecordAs<mc::ClComment>(program, 1).text, "note");
    EXPECT_EQ(RecordAs<mc::ClComment>(program, 2).text, "spaced text");
}

TEST(ClIoTest, Apt_UnknownStatementIsPassThroughWithWarning) {
    const mc::ClProgram program =
            mc::ReadClText("CUTCOM/LEFT\nGOTO/1,2,3\n", mc::ClFileFormat::kApt);
    ASSERT_EQ(program.records.size(), 2u);
    EXPECT_EQ(RecordAs<mc::ClPassThrough>(program, 0).dialect, "apt");
    EXPECT_EQ(RecordAs<mc::ClPassThrough>(program, 0).text, "CUTCOM/LEFT");
    ASSERT_EQ(program.warnings.size(), 1u);
    EXPECT_NE(program.warnings[0].message.find("CUTCOM"), std::string::npos);
    EXPECT_EQ(program.warnings[0].line, 1);
}

TEST(ClIoTest, Apt_LineRangeAndUnitScale) {
    mc::ClReadOptions options;
    options.start_line = 3;
    options.end_line = 4;
    options.unit_scale = 2.0;
    options.program_index = 1;
    const mc::ClProgram program = mc::ReadClText(
            "MULTAX/ON\nGOTO/9,9,9\nFEDRAT/60\nCIRCLE/1,0,0,0,0,1,1\nGOTO/2,0,0,0,0,1\n",
            mc::ClFileFormat::kApt);
    const mc::ClProgram ranged = mc::ReadClText(
            "MULTAX/ON\nGOTO/9,9,9\nFEDRAT/60\nCIRCLE/1,0,0,0,0,1,1\nGOTO/2,0,0,0,0,1\n",
            mc::ClFileFormat::kApt, options);
    ASSERT_EQ(program.records.size(), 3u);
    // 行範囲外のGOTO/9,9,9とGOTO/2,...は含まれず、CIRCLEは次のGOTOが無いので円弧にならない
    ASSERT_EQ(ranged.records.size(), 1u);
    EXPECT_NEAR(RecordAs<mc::ClFeed>(ranged, 0).mm_per_s, 2.0, kTol);
    EXPECT_EQ(ranged.sources[0].line, 3);
    EXPECT_EQ(ranged.sources[0].program_index, 1);
    // 単位換算は円弧の中心・終点にも掛かり、工具軸には掛からない
    mc::ClReadOptions scale_only;
    scale_only.unit_scale = 2.0;
    const mc::ClProgram scaled = mc::ReadClText(
            "MULTAX/ON\nCIRCLE/1,0,0,0,0,1,1\nGOTO/2,0,0,0,0,1\n",
            mc::ClFileFormat::kApt, scale_only);
    ASSERT_EQ(scaled.records.size(), 1u);
    ExpectVector(RecordAs<mc::ClArc>(scaled, 0).center, Vector3d(2.0, 0.0, 0.0));
    ExpectVector(RecordAs<mc::ClArc>(scaled, 0).end, Vector3d(4.0, 0.0, 0.0));
    ExpectVector(*RecordAs<mc::ClArc>(scaled, 0).tool_axis, Vector3d(0.0, 0.0, 1.0));
}

TEST(ClIoTest, Apt_EmptyInputHasNoRecords) {
    const mc::ClProgram program = mc::ReadClText("", mc::ClFileFormat::kApt);
    EXPECT_TRUE(program.records.empty());
    EXPECT_TRUE(program.name.empty());
}

TEST(ClIoTest, Apt_ThrowsDataFormatErrorWhenGotoArgumentCountIsInvalid) {
    EXPECT_THROW(mc::ReadClText("GOTO/1,2,3,4\n", mc::ClFileFormat::kApt),
                 igesio::DataFormatError);
    EXPECT_THROW(mc::ReadClText("GOTO/1,2\n", mc::ClFileFormat::kApt),
                 igesio::DataFormatError);
    EXPECT_THROW(mc::ReadClText("GOTO/1,2,3,4,5,6,7\n", mc::ClFileFormat::kApt),
                 igesio::DataFormatError);
    // 3個と6個 (境界) は受理
    EXPECT_NO_THROW(mc::ReadClText("GOTO/1,2,3\n", mc::ClFileFormat::kApt));
    EXPECT_NO_THROW(mc::ReadClText("GOTO/1,2,3,0,0,1\n", mc::ClFileFormat::kApt));
}

TEST(ClIoTest, Apt_ThrowsDataFormatErrorWhenValueIsNotNumeric) {
    EXPECT_THROW(mc::ReadClText("GOTO/1,x,3\n", mc::ClFileFormat::kApt),
                 igesio::DataFormatError);
    EXPECT_THROW(mc::ReadClText("FEDRAT/fast\n", mc::ClFileFormat::kApt),
                 igesio::DataFormatError);
    EXPECT_THROW(mc::ReadClText("DELAY/\n", mc::ClFileFormat::kApt),
                 igesio::DataFormatError);
}

TEST(ClIoTest, Apt_ThrowsDataFormatErrorWhenOnOffArgumentIsInvalid) {
    EXPECT_THROW(mc::ReadClText("COOLNT/FLOOD\n", mc::ClFileFormat::kApt),
                 igesio::DataFormatError);
    EXPECT_THROW(mc::ReadClText("MULTAX/YES\n", mc::ClFileFormat::kApt),
                 igesio::DataFormatError);
    EXPECT_NO_THROW(mc::ReadClText("COOLNT/ON\nMULTAX/OFF\n", mc::ClFileFormat::kApt));
}

TEST(ClIoTest, Apt_ThrowsDataFormatErrorWhenDirectionIsZero) {
    EXPECT_THROW(mc::ReadClText("MULTAX/ON\nGOTO/1,2,3,0,0,0\n", mc::ClFileFormat::kApt),
                 igesio::DataFormatError);
    EXPECT_THROW(mc::ReadClText("CIRCLE/0,0,0,0,0,0,1\n", mc::ClFileFormat::kApt),
                 igesio::DataFormatError);
}

TEST(ClIoTest, Apt_ThrowsDataFormatErrorWhenCircleArgumentCountIsNot7) {
    EXPECT_THROW(mc::ReadClText("CIRCLE/0,0,0,0,0,1\n", mc::ClFileFormat::kApt),
                 igesio::DataFormatError);
    EXPECT_NO_THROW(mc::ReadClText("CIRCLE/0,0,0,0,0,1,5\n", mc::ClFileFormat::kApt));
}



// ---- APTの出力 ----

TEST(ClIoTest, Apt_WriteLayout) {
    std::vector<mc::Diagnostic> warnings;
    const std::string text =
            mc::WriteClText(AptSample(), mc::ClFileFormat::kApt, {}, &warnings);
    EXPECT_EQ(text.rfind("PARTNO/ROUND TRIP\n", 0), 0u) << text;
    EXPECT_NE(text.find("PPRINT header\n"), std::string::npos);
    EXPECT_NE(text.find("LOADTL/3\n"), std::string::npos);
    EXPECT_NE(text.find("SPINDL/1200,CCLW\n"), std::string::npos);
    EXPECT_NE(text.find("COOLNT/ON\n"), std::string::npos);
    EXPECT_NE(text.find("FEDRAT/600.00000,MMPM\n"), std::string::npos);
    EXPECT_NE(text.find("PPRINT WORK OFFSET G55\n"), std::string::npos);
    EXPECT_NE(text.find("PPRINT LENGTH OFFSET 7\n"), std::string::npos);
    EXPECT_NE(text.find("PPRINT PATH BEGIN p1\n"), std::string::npos);
    // MULTAX/ONは工具軸を持つ最初のGOTOの前に1回だけ、RAPIDはそのGOTOの直前
    const std::size_t multax = text.find("MULTAX/ON\n");
    const std::size_t rapid = text.find("RAPID\nGOTO/0.00000,0.00000,5.00000,"
                                        "0.00000,0.00000,1.00000\n");
    ASSERT_NE(multax, std::string::npos);
    ASSERT_NE(rapid, std::string::npos);
    EXPECT_LT(multax, rapid);
    EXPECT_EQ(text.find("MULTAX/ON", multax + 1), std::string::npos);
    // 継続する工具軸も出力し、時計回りの円弧は法線を反転して書く
    EXPECT_NE(text.find("GOTO/10.00000,0.00000,0.00000,0.00000,0.00000,1.00000\n"),
              std::string::npos);
    EXPECT_NE(text.find("CIRCLE/10.00000,10.00000,0.00000,-0.00000,-0.00000,-1.00000,"
                        "10.00000\nGOTO/0.00000,10.00000,0.00000,0.00000,0.00000,1.00000\n"),
              std::string::npos) << text;
    EXPECT_NE(text.find("DELAY/0.25000\n"), std::string::npos);
    EXPECT_NE(text.find("PPRINT PATH END p1\n"), std::string::npos);
    EXPECT_NE(text.find("CUTCOM/OFF\n"), std::string::npos);
    EXPECT_NE(text.find("PPRINT M11M13\n"), std::string::npos);
    EXPECT_EQ(text.substr(text.size() - 5), "FINI\n");
    // PPRINTにした状態レコード2件の情報診断と、他方言の警告
    ASSERT_EQ(warnings.size(), 2u);
    EXPECT_EQ(warnings[0].severity, mc::Severity::kInfo);
    EXPECT_NE(warnings[0].message.find("2 record(s)"), std::string::npos);
    EXPECT_EQ(warnings[1].severity, mc::Severity::kWarning);
}

TEST(ClIoTest, Apt_WriteAndRoundTrip) {
    const mc::ClProgram sample = AptSample();
    const std::string text = mc::WriteClText(sample, mc::ClFileFormat::kApt);
    const mc::ClProgram back = mc::ReadClText(text, mc::ClFileFormat::kApt);
    EXPECT_EQ(back.name, "ROUND TRIP");
    // 同じ方言の`ClPassThrough` (CUTCOM/OFF) は再び未知の文として警告される
    ASSERT_EQ(back.warnings.size(), 1u);
    EXPECT_NE(back.warnings[0].message.find("CUTCOM"), std::string::npos);

    // 状態レコード (順序どおり)
    EXPECT_EQ(RecordAs<mc::ClComment>(back, 0).text, "header");
    EXPECT_EQ(RecordAs<mc::ClLoadTool>(back, 1).number, 3);
    EXPECT_EQ(RecordAs<mc::ClSpindle>(back, 2).mode, mc::ClSpindle::Mode::kCcw);
    EXPECT_NEAR(*RecordAs<mc::ClSpindle>(back, 2).rpm, 1200.0, kTol);
    EXPECT_TRUE(RecordAs<mc::ClCoolant>(back, 3).on);
    EXPECT_NEAR(RecordAs<mc::ClFeed>(back, 4).mm_per_s, 10.0, kRoundTripTol);
    // ワーク座標系・長さ補正・区切りはPPRINT (コメント) になる
    EXPECT_EQ(RecordAs<mc::ClComment>(back, 5).text, "WORK OFFSET G55");
    EXPECT_EQ(RecordAs<mc::ClComment>(back, 6).text, "LENGTH OFFSET 7");
    EXPECT_EQ(RecordAs<mc::ClComment>(back, 7).text, "PATH BEGIN p1");

    // 動作レコード
    const mc::ClGoto& rapid = RecordAs<mc::ClGoto>(back, 8);
    EXPECT_EQ(rapid.kind, mc::MotionKind::kRapid);
    ExpectVector(*rapid.point, Vector3d(0.0, 0.0, 5.0), kRoundTripTol);
    ExpectVector(*rapid.tool_axis, Vector3d(0.0, 0.0, 1.0), kRoundTripTol);
    const mc::ClGoto& linear = RecordAs<mc::ClGoto>(back, 9);
    EXPECT_EQ(linear.kind, mc::MotionKind::kLinear);
    ExpectVector(*linear.point, Vector3d(10.0, 0.0, 0.0), kRoundTripTol);
    // 円弧は幾何 (中心・終点・実効法線) で一致する (kArcCwは法線反転のkArcCcwで戻る)
    const mc::ClArc& original = RecordAs<mc::ClArc>(sample, 10);
    const mc::ClArc& arc = RecordAs<mc::ClArc>(back, 10);
    EXPECT_EQ(arc.kind, mc::MotionKind::kArcCcw);
    ExpectVector(arc.center, original.center, kRoundTripTol);
    ExpectVector(arc.end, original.end, kRoundTripTol);
    ExpectVector(EffectiveNormal(arc), EffectiveNormal(original), kRoundTripTol);
    EXPECT_NEAR(RecordAs<mc::ClDwell>(back, 11).seconds, 0.25, kRoundTripTol);
    EXPECT_EQ(RecordAs<mc::ClComment>(back, 12).text, "PATH END p1");
    // 同じ方言の文はそのまま (未知の文として再び保持)、他方言はコメント
    EXPECT_EQ(RecordAs<mc::ClPassThrough>(back, 13).text, "CUTCOM/OFF");
    EXPECT_EQ(RecordAs<mc::ClComment>(back, 14).text, "M11M13");
    EXPECT_TRUE(std::holds_alternative<mc::ClEnd>(back.records.at(15)));
    EXPECT_EQ(back.records.size(), 16u);
}

TEST(ClIoTest, Apt_WriteAppendsFiniWhenMissing) {
    mc::ClProgram program;
    program.records.push_back(Goto(mc::MotionKind::kLinear, Vector3d(1.0, 2.0, 3.0)));
    EXPECT_EQ(mc::WriteClText(program, mc::ClFileFormat::kApt),
              "GOTO/1.00000,2.00000,3.00000\nFINI\n");
}

TEST(ClIoTest, Apt_WriteWithoutCommentsOmitsPprintForCommentsAndMarkers) {
    mc::ClProgram program;
    program.records.push_back(mc::ClComment{"c"});
    program.records.push_back(mc::ClMarker{mc::ClMarker::Kind::kOperation, "op"});
    program.records.push_back(mc::ClSelectWorkOffset{"G54"});
    mc::ClWriteOptions options;
    options.comments = false;
    EXPECT_EQ(mc::WriteClText(program, mc::ClFileFormat::kApt, options),
              "PPRINT WORK OFFSET G54\nFINI\n");
}

TEST(ClIoTest, Apt_WriteOmitsMotionsWithoutPointWithWarning) {
    mc::ClProgram program;
    mc::ClGoto words;
    words.axis_words.Set("C", 1.0);
    program.records.push_back(words);
    mc::ClGoto machine;
    machine.frame = mc::MotionFrame::kMachine;
    program.records.push_back(machine);
    program.records.push_back(mc::ClEnd{});
    program.records.push_back(Goto(mc::MotionKind::kLinear, Vector3d(1.0, 1.0, 1.0)));
    std::vector<mc::Diagnostic> warnings;
    EXPECT_EQ(mc::WriteClText(program, mc::ClFileFormat::kApt, {}, &warnings), "FINI\n");
    // 軸の指令のみ、機械座標、終了後の3レコードを省いて警告1件
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_NE(warnings[0].message.find("3 record(s)"), std::string::npos)
            << warnings[0].message;
}

TEST(ClIoTest, Apt_WriteFileAndReadBack) {
    const std::filesystem::path path = TempPath("apt_round_trip.cl");
    mc::ClWriteOptions options;
    options.newline = mc::NewlineStyle::kCrlf;
    mc::WriteClFile(path, AptSample(), mc::ClFileFormat::kApt, options);
    const mc::ClProgram back = mc::ReadClFile(path, mc::ClFileFormat::kApt);
    std::filesystem::remove(path);
    EXPECT_EQ(back.name, "ROUND TRIP");
    EXPECT_EQ(back.records.size(), 16u);
}
