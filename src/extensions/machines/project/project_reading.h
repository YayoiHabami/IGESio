/**
 * @file extensions/machines/project/project_reading.h
 * @brief TOMLからプロジェクト定義を構築する関数の定義 (内部ヘッダ)
 * @author Yayoi Habami
 * @date 2026-09-12
 * @copyright 2026 Yayoi Habami
 * @note 解析済みのTOMLから`ProjectDefinition`を組み立てる. toml11に依存する
 *       ため公開ヘッダには含めない. `project_writing.h`と対になる.
 */
#ifndef SRC_EXTENSIONS_MACHINES_PROJECT_PROJECT_READING_H_
#define SRC_EXTENSIONS_MACHINES_PROJECT_PROJECT_READING_H_

#include <filesystem>
#include <string>

#include "igesio/extensions/machines/project/project_definition.h"
#include "igesio/extensions/machines/project/project_io.h"
#include "extensions/machines/machine/toml_reading.h"

namespace igesio::extensions::machines::detail {

/// @brief 解析済みのTOMLからプロジェクト定義を作る
/// @param root TOMLのルートテーブル
/// @param base_dir `file`キーの相対パス解決の基準ディレクトリ
/// @param source_name 診断と例外の文言に用いる入力の表示名
/// @param options 読込の設定 (ライブラリ検索ディレクトリ)
/// @return 検証済みのプロジェクト定義 (機械定義と警告を含む)
/// @throw igesio::DataFormatError 仕様に違反する、参照先のファイルが存在しない,
///        または機械定義の読込に失敗した場合
ProjectDefinition ReadProjectDocument(const TomlValue& root,
                                      const std::filesystem::path& base_dir,
                                      const std::string& source_name,
                                      const ReadProjectOptions& options);

}  // namespace igesio::extensions::machines::detail

#endif  // SRC_EXTENSIONS_MACHINES_PROJECT_PROJECT_READING_H_
