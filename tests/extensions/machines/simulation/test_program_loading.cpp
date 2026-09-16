/**
 * @file tests/extensions/machines/simulation/test_program_loading.cpp
 * @brief プロジェクトの`[[program]]`からの読込 (simulation/program_loading) のテスト
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note 対象: ToClFileFormat / DialectForProject / LoadPrograms /
 *       ConcatenatePrograms / TrimToToolRange
 *       - 正常系: 種別の対応、無効化コードの正規化、NC/CLの配列順の読込と索引,
 *         CLの先頭の状態レコード、プログラム間の状態の引き継ぎ、`enabled = false`,
 *         連結時のマーカーと`ClEnd`、実行範囲の限定
 *       - 正常系 (退化): プログラムの無いプロジェクト、空の連結、`[run]`の省略
 *       - 異常系: `kGcode`の`ToClFileFormat` (`std::invalid_argument`),
 *         存在しない工具の`start_tool`/`stop_tool` (警告)
 *       TODO: `FileOpenError`/`DataFormatError`は読込側 (`test_nc_interpreter.cpp`/
 *       `test_cl_io.cpp`) で検証済みのため本ファイルでは扱わない
 * @note プログラムファイルは一時ディレクトリに書き、機械定義とともに
 *       文字列のプロジェクトから読む
 */
#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/project/project_definition.h"
#include "igesio/extensions/machines/project/project_io.h"
#include "igesio/extensions/machines/project/setup.h"
#include "igesio/extensions/machines/simulation/program_loading.h"
#include "igesio/extensions/machines/toolpath/cl_io.h"
#include "igesio/extensions/machines/toolpath/cl_program.h"
#include "igesio/extensions/machines/toolpath/nc_dialect.h"
#include "igesio/extensions/machines/toolpath/nc_interpreter.h"
#include "../machine/machines_for_testing.h"
#include "../project/projects_for_testing.h"

namespace {

namespace mc = igesio::extensions::machines;
using machines_test::MinimalXyzAc;
using projects_test::MinimalProject;
using projects_test::Replace;

/// @brief 座標の比較の許容誤差
constexpr double kTol = 1e-9;

/// @brief NCプログラム (T1・G43.5・移動1つ)
constexpr const char* kNcText =
        "G90 G54\nT1 M06\nG43.5 H1\nG01 X1. Y2. Z3. I0. J0. K1. F100\nM30\n";

/// @brief 3行1組のCLデータ (早送り1つ・切削1つ)
constexpr const char* kClText = "10 0 0\n0 0 1\n7f\n20 0 0\n0 0 1\n80\n";

/// @brief ファイルを書く
void WriteFile(const std::filesystem::path& path, const std::string& text) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

/// @brief プログラムファイルを置いた一時ディレクトリで、`[[program]]`を持つ
///        プロジェクトからセットアップを作る
/// @param programs `[[program]]`セクションのTOML
/// @param dir_name 一時ディレクトリ名 (テストごとに分ける)
mc::MachiningSetup MakeSetup(const std::string& programs, const std::string& dir_name) {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / dir_name;
    std::filesystem::create_directories(dir);
    WriteFile(dir / "machine.toml", MinimalXyzAc());
    WriteFile(dir / "prog.nc", kNcText);
    WriteFile(dir / "prog.cl", kClText);
    const std::string body = Replace(MinimalProject(),
                                     "[machine]\nlibrary = \"t-ZYX-b-AC-w.toml\"\n", "");
    const mc::ProjectDefinition project = mc::ReadProjectFromString(
            "[machine]\nfile = \"machine.toml\"\n" + body + "\n" + programs, dir,
            mc::ReadProjectOptions{}, "<test>");
    return mc::MachiningSetup(project);
}

/// @brief デフォルトの読込設定 (Fanuc方言)
mc::ProgramLoadOptions DefaultOptions() {
    mc::ProgramLoadOptions options;
    options.dialect = mc::DefaultFanucDialect();
    return options;
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

/// @brief 動作レコードの数
std::size_t MotionCount(const mc::ClProgram& program) {
    std::size_t count = 0;
    for (const mc::ClRecord& record : program.records) count += mc::IsMotion(record);
    return count;
}

/// @brief 制御点のみの移動を作る
mc::ClGoto Goto(const double x) {
    mc::ClGoto motion;
    motion.point = igesio::Vector3d(x, 0.0, 0.0);
    return motion;
}

}  // namespace



TEST(ProgramLoadingTest, Formats_Mapping) {
    EXPECT_EQ(mc::ToClFileFormat(mc::ProgramType::kCl), mc::ClFileFormat::kTriplet);
    EXPECT_EQ(mc::ToClFileFormat(mc::ProgramType::kApt), mc::ClFileFormat::kApt);
}

TEST(ProgramLoadingTest, Formats_ThrowsInvalidArgumentForGcode) {
    EXPECT_THROW(mc::ToClFileFormat(mc::ProgramType::kGcode), std::invalid_argument);
}

TEST(ProgramLoadingTest, Dialect_DisabledCodesCopied) {
    const mc::ProjectDefinition project = projects_test::ReadProjectText(
            MinimalProject() + "\n[controller]\nlibrary = \"controllers/generic-tcp.toml\"\n"
                               "disabled_codes = [\"G68.2\", \"g43.50\"]\n");
    mc::NcDialect base = mc::DefaultFanucDialect();
    base.name = "base";
    const mc::NcDialect dialect = mc::DialectForProject(project, base);
    EXPECT_EQ(dialect.name, "base");
    EXPECT_EQ(dialect.disabled_codes.count("G68.2"), 1u);
    EXPECT_EQ(dialect.disabled_codes.count("G43.5"), 1u);
    EXPECT_EQ(dialect.disabled_codes.size(), 2u);
    // [controller] が無ければそのまま
    EXPECT_TRUE(mc::DialectForProject(projects_test::ReadProjectText(MinimalProject()), base)
                        .disabled_codes.empty());
}

TEST(ProgramLoadingTest, Load_GcodeAndClInOrder) {
    const auto setup = MakeSetup(
            "[[program]]\nfile = \"prog.nc\"\ntype = \"gcode\"\n\n"
            "[[program]]\nfile = \"prog.cl\"\ntype = \"cl\"\nwork_offset = \"G55\"\n",
            "igesio_program_loading_order");
    std::vector<mc::Diagnostic> warnings;
    const std::vector<mc::LoadedProgram> loaded =
            mc::LoadPrograms(setup, DefaultOptions(), nullptr, &warnings);
    ASSERT_EQ(loaded.size(), 2u);
    EXPECT_EQ(loaded[0].program_index, 0);
    EXPECT_EQ(loaded[1].program_index, 1);
    EXPECT_EQ(loaded[0].program.name, "prog.nc");
    EXPECT_EQ(loaded[1].program.name, "prog.cl");
    const auto nc_gotos = Records<mc::ClGoto>(loaded[0].program);
    ASSERT_EQ(nc_gotos.size(), 1u);
    ASSERT_TRUE(nc_gotos[0]->point.has_value());
    EXPECT_NEAR(nc_gotos[0]->point->x(), 1.0, kTol);
    EXPECT_EQ(loaded[0].program.sources.back().program_index, 0);
    EXPECT_EQ(loaded[1].program.sources.back().program_index, 1);
    EXPECT_EQ(Records<mc::ClGoto>(loaded[1].program).size(), 2u);
    for (const mc::Diagnostic& warning : warnings) {
        EXPECT_FALSE(warning.context.empty()) << warning.message;
    }
}

TEST(ProgramLoadingTest, Load_ClGetsInitialRecords) {
    const auto setup = MakeSetup(
            "[[program]]\nfile = \"prog.cl\"\ntype = \"cl\"\ntool = 1\n"
            "work_offset = \"G55\"\n",
            "igesio_program_loading_cl_head");
    mc::NcState state;
    state.tool_number = 5;
    const std::vector<mc::LoadedProgram> loaded =
            mc::LoadPrograms(setup, DefaultOptions(), &state);
    ASSERT_EQ(loaded.size(), 1u);
    const mc::ClProgram& program = loaded[0].program;
    ASSERT_GE(program.records.size(), 4u);
    ASSERT_TRUE(std::holds_alternative<mc::ClLoadTool>(program.records[0]));
    EXPECT_EQ(std::get<mc::ClLoadTool>(program.records[0]).number, 1);
    ASSERT_TRUE(std::holds_alternative<mc::ClSelectWorkOffset>(program.records[1]));
    EXPECT_EQ(std::get<mc::ClSelectWorkOffset>(program.records[1]).id, "G55");
    EXPECT_TRUE(program.HasSources());
    EXPECT_EQ(program.sources[0].line, 0);
    EXPECT_EQ(program.sources[2].line, 1);
    // 読込後の状態はCLの末尾の状態
    EXPECT_EQ(state.tool_number, 1);
    EXPECT_EQ(state.work_offset_id, "G55");
}

TEST(ProgramLoadingTest, Load_StateCarriesAcrossPrograms) {
    const auto setup = MakeSetup(
            "[[program]]\nfile = \"prog.nc\"\ntype = \"gcode\"\n\n"
            "[[program]]\nfile = \"prog.cl\"\ntype = \"cl\"\n\n"
            "[[program]]\nfile = \"prog.nc\"\ntype = \"gcode\"\nstart_line = 4\n",
            "igesio_program_loading_state");
    const std::vector<mc::LoadedProgram> loaded = mc::LoadPrograms(setup, DefaultOptions());
    ASSERT_EQ(loaded.size(), 3u);
    // CLはNCの末尾の工具 (T1) とG54を先頭に持つ
    EXPECT_EQ(std::get<mc::ClLoadTool>(loaded[1].program.records[0]).number, 1);
    EXPECT_EQ(std::get<mc::ClSelectWorkOffset>(loaded[1].program.records[1]).id, "G54");
    // 3本目 (4行目から) はTコードが無いが、先頭の状態レコードで工具1を引き継ぐ
    const auto tools = Records<mc::ClLoadTool>(loaded[2].program);
    ASSERT_GE(tools.size(), 1u);
    EXPECT_EQ(tools[0]->number, 1);
}

TEST(ProgramLoadingTest, Load_DisabledSkipped) {
    const auto setup = MakeSetup(
            "[[program]]\nfile = \"prog.nc\"\ntype = \"gcode\"\nenabled = false\n\n"
            "[[program]]\nfile = \"prog.cl\"\ntype = \"cl\"\n",
            "igesio_program_loading_disabled");
    const std::vector<mc::LoadedProgram> loaded = mc::LoadPrograms(setup, DefaultOptions());
    ASSERT_EQ(loaded.size(), 1u);
    EXPECT_EQ(loaded[0].program_index, 1);
    // プログラムの無いプロジェクトは空
    EXPECT_TRUE(mc::LoadPrograms(MakeSetup("", "igesio_program_loading_none"),
                                 DefaultOptions()).empty());
}

TEST(ProgramLoadingTest, Concatenate_MarkersAndEnds) {
    const auto setup = MakeSetup(
            "[[program]]\nfile = \"prog.nc\"\ntype = \"gcode\"\n\n"
            "[[program]]\nfile = \"prog.nc\"\ntype = \"gcode\"\nname = \"second\"\n",
            "igesio_program_loading_concat");
    const std::vector<mc::LoadedProgram> loaded = mc::LoadPrograms(setup, DefaultOptions());
    ASSERT_EQ(loaded.size(), 2u);
    const mc::ClProgram joined = mc::ConcatenatePrograms(loaded);
    EXPECT_TRUE(joined.name.empty());
    EXPECT_TRUE(joined.HasSources());
    const auto markers = Records<mc::ClMarker>(joined);
    ASSERT_EQ(markers.size(), 2u);
    EXPECT_EQ(markers[0]->kind, mc::ClMarker::Kind::kOperation);
    EXPECT_EQ(markers[0]->name, "prog.nc");
    EXPECT_EQ(markers[1]->name, "second");
    EXPECT_EQ(Records<mc::ClEnd>(joined).size(), 1u);
    EXPECT_TRUE(std::holds_alternative<mc::ClEnd>(joined.records.back()));
    EXPECT_TRUE(std::holds_alternative<mc::ClMarker>(joined.records.front()));
    EXPECT_EQ(joined.sources.front().program_index, 0);
    EXPECT_EQ(joined.sources.back().program_index, 1);
    EXPECT_TRUE(mc::ValidateClProgram(joined).empty());
    // 1つだけならその名前を引き継ぐ
    EXPECT_EQ(mc::ConcatenatePrograms({loaded[1]}).name, "second");
    EXPECT_TRUE(mc::ConcatenatePrograms({}).records.empty());
}

TEST(ProgramLoadingTest, Trim_StartAndStopTool) {
    const auto make = []() {
        mc::ClProgram program;
        program.records = {mc::ClLoadTool{1}, Goto(1.0), mc::ClLoadTool{2}, Goto(2.0),
                           Goto(3.0), mc::ClLoadTool{3}, Goto(4.0), mc::ClEnd{}};
        return program;
    };
    mc::RunSettings run;
    run.start_tool = 2;
    run.stop_tool = 2;
    mc::ClProgram program = make();
    std::vector<mc::Diagnostic> warnings;
    mc::TrimToToolRange(program, run, &warnings);
    EXPECT_TRUE(warnings.empty());
    EXPECT_EQ(MotionCount(program), 2u);
    EXPECT_EQ(Records<mc::ClLoadTool>(program).size(), 3u);
    EXPECT_EQ(Records<mc::ClEnd>(program).size(), 1u);
    const auto gotos = Records<mc::ClGoto>(program);
    EXPECT_NEAR(gotos[0]->point->x(), 2.0, kTol);
    EXPECT_NEAR(gotos[1]->point->x(), 3.0, kTol);

    // start_tool のみ: 工具2の選択から末尾まで
    mc::RunSettings start_only;
    start_only.start_tool = 2;
    program = make();
    mc::TrimToToolRange(program, start_only);
    EXPECT_EQ(MotionCount(program), 3u);

    // [run] の省略 (どちらも無い) は変更しない
    program = make();
    mc::TrimToToolRange(program, mc::RunSettings{});
    EXPECT_EQ(MotionCount(program), 4u);
}

TEST(ProgramLoadingTest, Trim_WarnsWhenToolIsNeverSelected) {
    mc::ClProgram program;
    program.records = {mc::ClLoadTool{1}, Goto(1.0), Goto(2.0), mc::ClEnd{}};
    mc::RunSettings run;
    run.start_tool = 9;
    std::vector<mc::Diagnostic> warnings;
    mc::TrimToToolRange(program, run, &warnings);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_NE(warnings[0].message.find("start_tool #9"), std::string::npos);
    EXPECT_EQ(MotionCount(program), 0u);
    EXPECT_EQ(Records<mc::ClEnd>(program).size(), 1u);

    mc::ClProgram tail;
    tail.records = {mc::ClLoadTool{1}, Goto(1.0), Goto(2.0), mc::ClEnd{}};
    mc::RunSettings stop;
    stop.stop_tool = 9;
    warnings.clear();
    mc::TrimToToolRange(tail, stop, &warnings);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_NE(warnings[0].message.find("stop_tool #9"), std::string::npos);
    EXPECT_EQ(MotionCount(tail), 2u);
}
