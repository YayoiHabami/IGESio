/**
 * @file extensions/machines/project/project_io.h
 * @brief プロジェクト定義 (TOML) の読込・検証、ファイル出力
 * @author Yayoi Habami
 * @date 2026-09-12
 * @copyright 2026 Yayoi Habami
 * @note 読込: プロジェクト定義のTOMLファイルを読み込み、ファイル内の整合性と
 *       機械定義ファイルとの突き合わせ (軸名・可動範囲・コンポーネント名) を行い,
 *       `ProjectDefinition`を作成する (併せて参照先の機械定義も読み込む).
 *       運動学の計算を要する検証 (チェーン判定/取り付け先の到達/閉路/干渉ペアの規則)
 * 　　　と参照の解決　(工具形状・暗黙のG54・工具オフセットの実効値) はここではなく
 * 　　　`MachiningSetup`で行う. 警告は例外にせず`ProjectDefinition::warnings`
 *       に格納する.
 * @note 本拡張が読むセクション (`project_definition.h`参照) 以外のトップレベルの
 *       キーは、文字列のまま`retained`に保持する (出力時はそのまま同じ位置に書出し).
 *       本拡張が読むセクション内の未知キーと`ext`テーブルは、機械定義と同様に
 *       検出も保持もしない
 * @note 出力: `ProjectDefinition`の内容をプロジェクトフォーマットのTOMLにする.
 *       読込時に正規化された構造体からの再生成であり、元ファイルの復元ではない.
 *       単位は`ProjectDefinition::units`の宣言単位へ戻す. `retained`の内容は
 *       単位換算せず、本拡張が対応するセクションの後に出現順で置く.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_PROJECT_PROJECT_IO_H_
#define IGESIO_EXTENSIONS_MACHINES_PROJECT_PROJECT_IO_H_

#include <filesystem>
#include <string>
#include <vector>

#include "igesio/extensions/machines/project/project_definition.h"

namespace igesio::extensions::machines {

/// @brief プロジェクトファイルの読込設定
struct ReadProjectOptions {
    /// @brief ライブラリ検索ディレクトリ (先頭優先)
    /// @note `library`キーの相対パスを指定された順に探し、最初に見つかった
    ///       ファイルを読み込む. 空の場合、相対パスの`library`は解決できず
    ///       `DataFormatError`を投げる
    ///       (実行ファイルのディレクトリ等は呼び出し側が与える)
    std::vector<std::filesystem::path> library_dirs;
};

/// @brief プロジェクトファイルを読み込む
/// @param path プロジェクトファイル (TOML) のパス.
///        `file`キーの相対パスはこのファイルのディレクトリを基準に解決する
/// @param options 読込設定
/// @return 検証済みのプロジェクト定義 (機械定義と警告を含む)
/// @throw igesio::FileOpenError ファイルが存在しない場合
/// @throw igesio::DataFormatError TOMLの構文間違い、仕様違反、参照先が存在しない場合,
///        または機械定義の読込エラー (文言の先頭に`"[machine]: "`を付ける)
ProjectDefinition ReadProject(const std::filesystem::path& path,
                              const ReadProjectOptions& options = {});

/// @brief プロジェクト定義をTOML文字列から読み込む
/// @param toml TOML本文
/// @param base_dir `file`キーの相対パス解決の基準ディレクトリ
/// @param options 読込の設定
/// @param source_name 診断・例外の文言に用いる入力の表示名
/// @return 検証済みのプロジェクト定義 (機械定義と警告を含む)
/// @throw igesio::DataFormatError TOMLの構文間違い、仕様違反,
///        参照先が存在しない場合、または機械定義の読込エラー
ProjectDefinition ReadProjectFromString(
        const std::string& toml, const std::filesystem::path& base_dir,
        const ReadProjectOptions& options = {},
        const std::string& source_name = "<string>");

/// @brief プロジェクト定義をTOMLファイルへ書き出す (同名ファイルは上書きする)
/// @param project 書き出すプロジェクト定義. 値は内部単位 (mm・rad) であること
/// @param path 出力するプロジェクトTOMLのパス. 参照ファイルの`file`はこのファイルの
///        ディレクトリからの相対パスで書く (相対化できなければ絶対パス)
/// @throw igesio::FileOpenError ファイルを開けない、または書き込めない場合
/// @throw std::invalid_argument TOMLで表現できない値を含む場合
///        (機械に無い軸名の`values`、`raw`が空のライブラリ参照等)
/// @note 書き出す要素と省略規則は`WriteProjectToString`と同じ
void WriteProject(const ProjectDefinition& project,
                  const std::filesystem::path& path);

/// @brief プロジェクト定義をTOML形式の文字列にする
/// @param project 書き出すプロジェクト定義. 値は内部単位 (mm・rad) であること
/// @param base_dir 参照ファイルの相対パスの基準ディレクトリ (出力先のディレクトリ).
///        `project.source_dir`と一致すれば読込時のパス文字列
///        (`FileReference::raw`) をそのまま書く
/// @return プロジェクトフォーマットのTOML本文
/// @throw std::invalid_argument TOMLで表現できない値を含む場合
/// @note 省略規則: 既定値と一致する任意キー (`enabled = true`,
///       `control_point = "tip"`, `from = "tool_mount"`,
///       `attach = "work_mount"`, `start_line = 1`, `type = "gcode"`,
///       `encoding = "utf-8"`, 摩耗量0等)、空の任意文字列・空配列,
///       値の無い`optional`は書かない.
///       `[[model]]`の`collision`は役割の既定と一致すれば,
///       `[[collision.tool_pair]]`の`enabled`は組の既定と一致すれば省略する.
///       `[format]`/`[project]`/`[units]`/`[machine]`は常に書き,
///       `[controller]`/`[collision]`は値を持つとき、`[initial]`/`[run]`
///       は既定と異なるときのみ書く
std::string WriteProjectToString(const ProjectDefinition& project,
                                 const std::filesystem::path& base_dir);

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_PROJECT_PROJECT_IO_H_
