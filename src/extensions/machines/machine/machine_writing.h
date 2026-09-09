/**
 * @file extensions/machines/machine/machine_writing.h
 * @brief 機械定義のTOML書き出し (内部ヘッダ)
 * @author Yayoi Habami
 * @date 2026-09-09
 * @copyright 2026 Yayoi Habami
 * @note `MachineDefinition`を機械定義フォーマットのTOML文字列にする.
 *       toml11に依存するため公開ヘッダには含めない. `toml_reading.h`と対になる.
 * @note 読込時に正規化された構造体からの再生成であり、元ファイルの復元ではない.
 *       単位は`MachineDefinition::units`の宣言単位へ戻し、`local_frame`
 *       込みで保持している配置 (`frame_placement`/`GeometrySpec::placement`)
 *       は`local_frame`相対へ戻して書く. 内容の検証は行わない.
 */
#ifndef SRC_EXTENSIONS_MACHINES_MACHINE_MACHINE_WRITING_H_
#define SRC_EXTENSIONS_MACHINES_MACHINE_MACHINE_WRITING_H_

#include <filesystem>
#include <string>

#include "igesio/extensions/machines/machine/machine_definition.h"

namespace igesio::extensions::machines::detail {

/// @brief 機械定義を機械定義フォーマット (TOML) に変換する
/// @param definition 書き出す機械定義 (内部単位)
/// @param base_dir 形状ファイルの相対パスの基準ディレクトリ.
///        `definition.source_dir`と一致すれば`GeometrySpec::raw_path`を
///        そのまま書き、異なれば解決済みパスを`base_dir`からの相対にする
///        (相対化できなければ絶対パス). 区切りは常に`/`.
/// @return TOML (先頭に見出しコメント1行)
/// @throw std::invalid_argument STL/OBJ形状の`file_unit_scale`がmm・inchの
///        いずれの係数でもない場合 (`unit`キーで表現できない)
std::string FormatMachineDefinition(const MachineDefinition& definition,
                                    const std::filesystem::path& base_dir);

}  // namespace igesio::extensions::machines::detail

#endif  // SRC_EXTENSIONS_MACHINES_MACHINE_MACHINE_WRITING_H_
