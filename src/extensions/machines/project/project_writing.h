/**
 * @file extensions/machines/project/project_writing.h
 * @brief プロジェクト定義をTOML形式の文字列に変換する関数の定義 (内部ヘッダ)
 * @author Yayoi Habami
 * @date 2026-09-12
 * @copyright 2026 Yayoi Habami
 * @note `ProjectDefinition`をプロジェクトフォーマット (TOML) の文字列に変換する.
 *       toml11に依存するため公開ヘッダには含めない. `project_reading.h`と対になる.
 * @note 読込時に正規化された構造体から再生成する. 元ファイルの復元ではない.
 *       単位は`ProjectDefinition::units`の宣言単位に戻す. `values`/`[initial.axes]`
 *       は`project.machine`の軸種別で逆換算する. 内容の検証は行わない.
 */
#ifndef SRC_EXTENSIONS_MACHINES_PROJECT_PROJECT_WRITING_H_
#define SRC_EXTENSIONS_MACHINES_PROJECT_PROJECT_WRITING_H_

#include <filesystem>
#include <string>

#include "igesio/extensions/machines/project/project_definition.h"

namespace igesio::extensions::machines::detail {

/// @brief プロジェクト定義をプロジェクトフォーマット (TOML) に変換する
/// @param project 出力するプロジェクト定義 (内部単位)
/// @param base_dir 参照ファイルの相対パスの基準ディレクトリ.
///        `project.source_dir`と一致すれば`FileReference::raw`をそのまま出力し,
///        異なれば解決済みパスを`base_dir`からの相対にする
///        (相対化できなければ絶対パス). 区切りは常に`/`.
/// @return TOML形式の文字列 (先頭に見出しコメント1行)
/// @throw std::invalid_argument 機械に無い軸名の`values`/`[initial.axes]`,
///        `raw`が空のライブラリ参照、ライブラリ参照の`[[program]]`,
///        空の`[machine]`参照、STL/OBJモデルの`file_unit_scale`が
///        mm/inchのいずれでもないもの、またはTOMLに変換できない`retained`の
///        要素を含む場合
std::string FormatProject(const ProjectDefinition& project,
                          const std::filesystem::path& base_dir);

}  // namespace igesio::extensions::machines::detail

#endif  // SRC_EXTENSIONS_MACHINES_PROJECT_PROJECT_WRITING_H_
