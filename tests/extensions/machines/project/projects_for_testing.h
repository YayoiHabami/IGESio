/**
 * @file tests/extensions/machines/project/projects_for_testing.h
 * @brief machines拡張のテストで共有するプロジェクト定義のフィクスチャ
 * @author Yayoi Habami
 * @date 2026-09-12
 * @copyright 2026 Yayoi Habami
 * @note プロジェクトの入出力とセットアップのテストが同じTOML文字列と補助関数を
 *       使えるようにする. 機械定義は`tests/test_data/machines`をライブラリ検索
 *       ディレクトリとして`library`キーで解決する (相対パスの警告を避けるため).
 */
#ifndef TESTS_EXTENSIONS_MACHINES_PROJECT_PROJECTS_FOR_TESTING_H_
#define TESTS_EXTENSIONS_MACHINES_PROJECT_PROJECTS_FOR_TESTING_H_

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "igesio/extensions/machines/project/project_definition.h"
#include "igesio/extensions/machines/project/project_io.h"
#include "../machine/machines_for_testing.h"

namespace projects_test {

/// @brief 機械定義・制御装置・工具ライブラリのライブラリ検索ディレクトリ
///        (tests/test_data/machines)
inline const std::filesystem::path kMachinesDir =
        machines_test::kFixturePath.parent_path();

/// @brief プロジェクトのテストデータのディレクトリ (tests/test_data/machines/projects)
inline const std::filesystem::path kProjectsDir = kMachinesDir / "projects";

/// @brief 既定の読込設定 (ライブラリ検索ディレクトリは`kMachinesDir`のみ)
inline igesio::extensions::machines::ReadProjectOptions DefaultOptions() {
    igesio::extensions::machines::ReadProjectOptions options;
    options.library_dirs = {kMachinesDir};
    return options;
}

/// @brief 最小構成のプロジェクト (実例機・簡易ボール工具#1・G54登録値・G55幾何形式・
///        boxストック・初期状態)
/// @note 機械は`t-ZYX-b-AC-w.toml` (工具側XYZ・ワーク側AC. Toolは(0,-180,250.5)).
///       G54は`values = {X=0, Y=180, Z=-250.5}` (ゲージ点をテーブル中心に置く
///       機械位置)、G55はストック上面中心 (`attach = "stock"`, `origin = [0,0,20]`).
///       ストックは40×30×40のboxで`origin = [0,0,20]` (下面がテーブル上面)
inline std::string MinimalProject() {
    return R"([format]
name = "machining-project"
version = [1, 0]

[project]
name = "minimal"

[machine]
library = "t-ZYX-b-AC-w.toml"

[[tool]]
number = 1

[tool.simple]
cutter = "ball"
diameter = 10.0
cutting_length = 20.0
tool_length = 60.0
overhang = 40.0

[tool.simple.holder]
diameter = 40.0
length = 50.0

[[work_offset]]
id = "G54"
values = { X = 0.0, Y = 180.0, Z = -250.5 }

[[work_offset]]
id = "G55"
attach = "stock"
origin = [0.0, 0.0, 20.0]

[[model]]
name = "stock"
role = "stock"
primitive = "box"
size = [40.0, 30.0, 40.0]
origin = [0.0, 0.0, 20.0]

[initial]
tool = 1
work_offset = "G54"

[initial.axes]
Z = 100.0
)";
}

/// @brief 文字列入力でプロジェクト定義を読み込む (基準ディレクトリは`kProjectsDir`)
inline igesio::extensions::machines::ProjectDefinition ReadProjectText(
        const std::string& toml,
        const igesio::extensions::machines::ReadProjectOptions& options = DefaultOptions()) {
    return igesio::extensions::machines::ReadProjectFromString(
            toml, kProjectsDir, options, "<test>");
}

/// @brief 書き出して同じ基準ディレクトリで読み戻す
inline igesio::extensions::machines::ProjectDefinition RoundTrip(
        const igesio::extensions::machines::ProjectDefinition& project,
        const std::filesystem::path& base_dir,
        const igesio::extensions::machines::ReadProjectOptions& options = DefaultOptions()) {
    const std::string text =
            igesio::extensions::machines::WriteProjectToString(project, base_dir);
    return igesio::extensions::machines::ReadProjectFromString(
            text, base_dir, options, "<round-trip>");
}

/// @brief 文字列の最初の出現を置換する (異常系・派生構成の作成用)
/// @throw std::logic_error 置換元が見つからない場合 (テストの記述ミス)
inline std::string Replace(std::string base, const std::string& from,
                           const std::string& to) {
    return machines_test::Replace(std::move(base), from, to);
}

/// @brief 機械定義のTOML文字列を一時ディレクトリに書き、それを参照するプロジェクトを
///        文字列から読み込む (`MinimalXyzAc`以外の機械でセットアップを作る用途)
/// @param machine_toml 機械定義のTOML
/// @param project_toml プロジェクトのTOML (`[machine]`を含まないこと.
///        `[machine] file = "machine.toml"`を先頭に補う)
/// @param dir_name 一時ディレクトリ名 (テストごとに分ける)
inline igesio::extensions::machines::ProjectDefinition ReadProjectWithMachine(
        const std::string& machine_toml, const std::string& project_toml,
        const std::string& dir_name) {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / dir_name;
    std::filesystem::create_directories(dir);
    {
        std::ofstream stream(dir / "machine.toml", std::ios::binary | std::ios::trunc);
        stream << machine_toml;
    }
    return igesio::extensions::machines::ReadProjectFromString(
            "[machine]\nfile = \"machine.toml\"\n" + project_toml, dir,
            igesio::extensions::machines::ReadProjectOptions{}, "<test>");
}

}  // namespace projects_test

#endif  // TESTS_EXTENSIONS_MACHINES_PROJECT_PROJECTS_FOR_TESTING_H_
