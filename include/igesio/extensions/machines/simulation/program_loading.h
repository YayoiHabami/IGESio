/**
 * @file extensions/machines/simulation/program_loading.h
 * @brief プロジェクト定義の`[[program]]`の読込 (`ClProgram`への変換)
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note `[[program]]`の列と`[controller]`を、NC/CLの読込 (`nc_interpreter.h`/
 *       `cl_io.h`) と制御装置の定義 (`nc_dialect.h`) に結び付ける.
 *       `project`と`toolpath`の両方に依存するため`toolpath`には置かない.
 * @note 読込の規則は以下.
 *       (1) `enabled`なプログラムを配列順に読む.
 *           モーダル状態 (`NcState`) はプログラム間で引き継ぐ.
 *       (2) `ProgramSpec::tool`/`work_offset`があれば読込前に`NcState`の
 *           工具番号/ワークオフセットidに設定する.
 *       (3) CL/APTは先頭に`ClLoadTool`/`ClSelectWorkOffset` (行番号0) を追加して
 *           単独で`ClState`が定まるようにし、読込後は末尾の`ClState`で`NcState`の
 *           工具番号/ワークオフセットidを更新する
 *       (4) バイト列として読むので`encoding`は区別しない (コメントの文字コード等)
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_SIMULATION_PROGRAM_LOADING_H_
#define IGESIO_EXTENSIONS_MACHINES_SIMULATION_PROGRAM_LOADING_H_

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/project/project_definition.h"
#include "igesio/extensions/machines/project/setup.h"
#include "igesio/extensions/machines/toolpath/cl_io.h"
#include "igesio/extensions/machines/toolpath/cl_program.h"
#include "igesio/extensions/machines/toolpath/nc_dialect.h"
#include "igesio/extensions/machines/toolpath/nc_interpreter.h"

namespace igesio::extensions::machines {

/// @brief プログラムの種別をCLファイルの形式に変換する
/// @param type プログラムの種別 (`kCl` / `kApt`)
/// @return `kCl` → `kTriplet`、`kApt` → `kApt`
/// @throw std::invalid_argument `kGcode`の場合 (CLファイルではない)
ClFileFormat ToClFileFormat(ProgramType type);

/// @brief プロジェクトの`[controller].disabled_codes`を制御装置定義に反映する
/// @param project プロジェクト定義
/// @param base 元の制御装置の定義
/// @return `disabled_codes`を正規化して追加したもの (他のフィールドは`base`のまま)
/// @note 制御装置定義ファイルは読まない
NcDialect DialectForProject(const ProjectDefinition& project,
                            const NcDialect& base);

/// @brief プログラム読込の設定
/// @note NC (`ProgramType::kGcode`) の読込のみに用いる. CL/APTの読込は
///       `ProgramSpec`と`[units]`から設定する
struct ProgramLoadOptions {
    /// @brief 制御装置の定義 (`DialectForProject`の結果)
    NcDialect dialect;
    /// @brief 同一ファイルに無いO番号のサブプログラムを取得する関数
    /// @note 引数はO番号. 未設定または`std::nullopt`を返した場合は
    ///       `DataFormatError`を送出する (`NcInterpretOptions`と同じ)
    std::function<std::optional<std::string>(int)> subprogram_loader;
    /// @brief コメントを`ClComment`にする
    bool keep_comments = true;
    /// @brief 未知のG/Mを`ClPassThrough`にする
    bool keep_unknown_words = true;
};

/// @brief 読み込んだプログラム
struct LoadedProgram {
    /// @brief `[[program]]`のインデックス (`SourceLocation::program_index`と同じ)
    int program_index = 0;
    /// @brief CLプログラム
    ClProgram program;
};

/// @brief `enabled`な`[[program]]`を配列順に読み込む
/// @param setup 加工セットアップ (プロジェクト定義と初期状態)
/// @param options 読込の設定
/// @param[in,out] state モーダル状態
///                (`nullptr`なら初期工具/初期ワークオフセットで開始し、破棄する)
/// @param[out] warnings 各プログラムの警告の転記先 (`nullptr`なら転記しない)
/// @return 読み込んだプログラム (配列順. `enabled = false`は含まない)
/// @throw igesio::FileOpenError ファイルを開けない場合
/// @throw igesio::DataFormatError NC/CLの内容に不備がある場合
/// @note `ClProgram::name`が空なら`DisplayName(spec)`にする
/// @note 転記する警告は`context`が空なら`DisplayName(spec)`にする.
///       `ClProgram::warnings`にも残す
std::vector<LoadedProgram> LoadPrograms(
        const MachiningSetup& setup, const ProgramLoadOptions& options,
        NcState* state = nullptr, std::vector<Diagnostic>* warnings = nullptr);

/// @brief 複数のプログラムを配列順に1つのCLプログラムにする
/// @param programs 読み込んだプログラム
/// @return 連結したプログラム. 各プログラムの先頭に`ClMarker{kOperation, name}`を
///         置き、最後のプログラム以外の`ClEnd`は除く. `sources`が無いプログラムは
///         `{program_index, 0}`で補う. `warnings`は連結する. `name`は
///         プログラムが1つならその名前、複数なら空
ClProgram ConcatenatePrograms(const std::vector<LoadedProgram>& programs);

/// @brief `[run].start_tool`/`stop_tool`で実行範囲を限定する
/// @param[in,out] program 対象のプログラム (動作レコードを取り除く)
/// @param run 実行制御
/// @param[out] warnings 該当する工具が選択されないときの警告 (`nullptr`なら
///        報告しない)
/// @note `start_tool`が最初に選択される`ClLoadTool`より前の動作レコードと,
///       `stop_tool`の使用区間の終了 (以降で最初の別番号の`ClLoadTool`) 以降の
///       動作レコードを除く. 状態レコードと`ClEnd`は残す. `start_tool`が
///       選択されなければ全動作レコードを除き、`stop_tool`が選択されなければ
///       末尾まで残す (いずれも警告)
void TrimToToolRange(ClProgram& program, const RunSettings& run,
                     std::vector<Diagnostic>* warnings = nullptr);

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_SIMULATION_PROGRAM_LOADING_H_
