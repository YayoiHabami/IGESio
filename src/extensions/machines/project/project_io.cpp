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

#include "igesio/common/errors.h"
#include "extensions/machines/machine/toml_reading.h"
#include "extensions/machines/project/project_reading.h"
#include "extensions/machines/project/project_writing.h"

namespace igesio::extensions::machines {

ProjectDefinition ReadProject(const std::filesystem::path& path,
                              const ReadProjectOptions& options) {
    if (!std::filesystem::is_regular_file(path)) {
        throw igesio::FileOpenError(path.string());
    }
    const std::string source_name = path.filename().string();
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
                  const std::filesystem::path& path) {
    // 文字列化の例外 (invalid_argument) はファイルを作る前に出す
    const std::string text = detail::FormatProject(project, path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        throw igesio::FileOpenError("Failed to open project file: " +
                                    path.string());
    }
    stream << text;
    if (!stream) {
        throw igesio::FileOpenError("Failed to write project file: " +
                                    path.string());
    }
}

std::string WriteProjectToString(const ProjectDefinition& project,
                                 const std::filesystem::path& base_dir) {
    return detail::FormatProject(project, base_dir);
}

}  // namespace igesio::extensions::machines
