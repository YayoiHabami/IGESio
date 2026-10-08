/**
 * @file extensions/machines/project/project_io.cpp
 * @brief プロジェクト定義 (TOML) の読込・検証、ファイル出力
 * @author Yayoi Habami
 * @date 2026-09-12
 * @copyright 2026 Yayoi Habami
 * @note 読込のメイン処理は`project_reading.h`, 書き出しのメイン処理は
 *       `project_writing.h`に移譲する.
 */
#include "igesio/extensions/machines/project/project_io.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include "igesio/common/errors.h"
#include "igesio/utils/path_encoding.h"
#include "extensions/machines/machine/toml_reading.h"
#include "extensions/machines/project/project_reading.h"
#include "extensions/machines/project/project_writing.h"

namespace igesio::extensions::machines {

namespace {

/// @brief 既存のファイルがテンプレートのプロジェクト定義かを判定する
/// @param path 判定するファイルのパス
/// @return `[format].is_template`が`true`であれば`true`. ファイルが無い場合,
///         TOMLとして解析できない場合、またはキーが無いか真偽値でない場合は`false`
/// @note `[format].name`/`version`の検証や機械定義の読込は行わない
///       (プロジェクト定義として読めないファイルでも判定できるようにするため)
bool IsTemplateProjectFile(const std::filesystem::path& path) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error)) return false;

    detail::TomlValue root;
    try {
        root = detail::ParseTomlFile(path, utils::PathToUtf8(path.filename()));
    } catch (const igesio::IGESioError&) {
        return false;
    }
    const detail::TomlValue* format = detail::Find(root, "format");
    const detail::TomlValue* flag =
            format == nullptr ? nullptr : detail::Find(*format, "is_template");
    return flag != nullptr && flag->is_boolean() && flag->as_boolean();
}

}  // namespace


ProjectDefinition ReadProject(const std::filesystem::path& path,
                              const ReadProjectOptions& options) {
    if (!std::filesystem::is_regular_file(path)) {
        throw igesio::FileOpenError(utils::PathToUtf8(path));
    }
    const std::string source_name = utils::PathToUtf8(path.filename());
    const detail::TomlValue root = detail::ParseTomlFile(path, source_name);
    return detail::ReadProjectDocument(root, path.parent_path(),
                                       source_name, options);
}

ProjectDefinition ReadProjectFromString(
        const std::string& toml, const std::filesystem::path& base_dir,
        const ReadProjectOptions& options, const std::string& source_name) {
    const detail::TomlValue root = detail::ParseTomlString(toml, source_name);
    return detail::ReadProjectDocument(root, base_dir, source_name, options);
}

void WriteProject(const ProjectDefinition& project,
                  const std::filesystem::path& path,
                  const WriteProjectOptions& options) {
    // 文字列化の例外 (invalid_argument) はファイルを作る前に出す
    const std::string text = detail::FormatProject(project, path.parent_path());
    if (!options.overwrite_template && IsTemplateProjectFile(path)) {
        throw igesio::FileWriteProtectedError(utils::PathToUtf8(path));
    }

    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        throw igesio::FileOpenError("Failed to open project file: " +
                                    utils::PathToUtf8(path));
    }
    stream << text;
    if (!stream) {
        throw igesio::FileOpenError("Failed to write project file: " +
                                    utils::PathToUtf8(path));
    }
}

std::string WriteProjectToString(const ProjectDefinition& project,
                                 const std::filesystem::path& base_dir) {
    return detail::FormatProject(project, base_dir);
}

}  // namespace igesio::extensions::machines
