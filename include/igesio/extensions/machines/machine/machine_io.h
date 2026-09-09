/**
 * @file extensions/machines/machine/machine_io.h
 * @brief 機械定義 (TOML) の読込・検証と書き出し
 * @author Yayoi Habami
 * @date 2026-09-08
 * @copyright 2026 Yayoi Habami
 * @note 読込: 機械定義フォーマットのTOMLを読み、仕様に基づく検証を行って
 *       `MachineDefinition`を返す. 警告は例外にせず`MachineDefinition::warnings`
 *       へ入れる. 本仕様で定義されないキーは検出も警告もしない
 *       (必要なキーだけを名前で読む. 利用側でキーを追加できるようにする).
 * @note 書き出し: `MachineDefinition`の内容を機械定義フォーマットのTOMLにする.
 *       読込時に正規化された構造体からの再生成であり、元ファイルの復元ではない.
 *       本拡張が読まない要素 (他の処理系のセクション・未知キー・コメント・
 *       回転の記法・`pairs`簡易形と`[[collision.pair]]`の区別) は失われる.
 *       内容の検証は行わない.
 * @note 読み込み時に、内部単位 (長さはmm、角度はrad、時間はs) への換算を行う.
 *       書き出しは`MachineDefinition::units`の宣言単位 (`length_unit`・
 *       `angle_unit`) と毎分の送り速度へ戻して書くため、`units`を差し替えれば
 *       単位変換出力になる.
 *       診断・例外の文言に角度値を含める場合はdegへ換算して示す.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_MACHINE_MACHINE_IO_H_
#define IGESIO_EXTENSIONS_MACHINES_MACHINE_MACHINE_IO_H_

#include <filesystem>
#include <string>

#include "igesio/extensions/machines/machine/machine_definition.h"

namespace igesio::extensions::machines {

/// @brief 機械定義ファイルを読み込む
/// @param path 機械定義TOMLのパス.
///        形状ファイルの相対パスはこのファイルのディレクトリを基準に解決する
/// @return 検証済みの機械定義 (警告を含む)
/// @throw igesio::FileOpenError ファイルが存在しない場合
/// @throw igesio::DataFormatError TOMLの構文誤り、または仕様違反
MachineDefinition ReadMachineDefinition(const std::filesystem::path& path);

/// @brief 機械定義をTOML文字列から読み込む
/// @param toml TOML本文
/// @param base_dir 形状ファイルの相対パス解決の基準ディレクトリ
/// @param source_name 診断・例外の文言に用いる入力の表示名
/// @return 検証済みの機械定義 (警告を含む)
/// @throw igesio::DataFormatError TOMLの構文誤り、または仕様違反
MachineDefinition ReadMachineDefinitionFromString(
        const std::string& toml, const std::filesystem::path& base_dir,
        const std::string& source_name = "<string>");

/// @brief 機械定義をTOMLファイルへ書き出す (同名ファイルは上書きする)
/// @param definition 書き出す機械定義. 値は内部単位 (mm・rad・s) であること
/// @param path 出力する機械定義TOMLのパス. 形状ファイルのパスはこのファイルの
///        ディレクトリからの相対パスで書く (相対化できなければ絶対パス)
/// @throw igesio::FileOpenError ファイルを開けない、または書き込めない場合
/// @throw std::invalid_argument TOMLで表現できない値を含む場合
///        (STL/OBJの`file_unit_scale`がmm・inchのいずれでもない等)
/// @note 書き出す要素と省略規則は`WriteMachineDefinitionToString`と同じ
void WriteMachineDefinition(const MachineDefinition& definition,
                            const std::filesystem::path& path);

/// @brief 機械定義をTOML文字列にする
/// @param definition 書き出す機械定義. 値は内部単位 (mm・rad・s) であること
/// @param base_dir 形状ファイルの相対パスの基準ディレクトリ (出力先のディレクトリ).
///        `definition.source_dir`と一致すれば読込時のパス文字列
///        (`GeometrySpec::raw_path`) をそのまま書く
/// @return 機械定義フォーマットのTOML本文
/// @throw std::invalid_argument TOMLで表現できない値を含む場合
/// @note 省略規則: 既定値と一致する任意キー (`initial = 0`, `opacity = 1`,
///       `collision`/`visible`/`enabled`が`true`等) と、単位行列の`local_frame`
///       /回転、ゼロの`origin`は書かない. 形状も`local_frame`も持たない`base`は
///       暗黙のbaseとして省略する. `[units]`/`[kinematics]`は常に書き,
///       `[collision]`は`collision`が値を持つときのみ書く
std::string WriteMachineDefinitionToString(
        const MachineDefinition& definition, const std::filesystem::path& base_dir);

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_MACHINE_MACHINE_IO_H_
