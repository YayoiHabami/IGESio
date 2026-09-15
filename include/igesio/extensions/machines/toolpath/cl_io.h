/**
 * @file extensions/machines/toolpath/cl_io.h
 * @brief CLファイル (3行1組/APT) の読み書き
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note 対応する形式は以下.
 *       (1) 3行1組 (`kTriplet`): 1行目XYZ、2行目IJK (工具軸)、3行目コード
 *           (`7f` = 早送り、それ以外 = 切削) の繰り返し. 状態レコードと円弧は
 *           表せない
 *       (2) APT (`kApt`): `GOTO`/`RAPID`/`FEDRAT`/`LOADTL`/`SPINDL`/`COOLNT`/
 *           `DELAY`/`MULTAX`/`CIRCLE`/`PARTNO`/`PPRINT`/`$$`/`FINI`の各文
 * @note `ProgramType` (`project_definition.h`) から`ClFileFormat`への対応は
 *       `simulation/program_loading.h`に置く
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_TOOLPATH_CL_IO_H_
#define IGESIO_EXTENSIONS_MACHINES_TOOLPATH_CL_IO_H_

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/core/formatting.h"
#include "igesio/extensions/machines/toolpath/cl_program.h"

namespace igesio::extensions::machines {

/// @brief CLファイルの形式
enum class ClFileFormat {
    /// @brief 3行1組 (`ProgramType::kCl`に対応)
    kTriplet,
    /// @brief APT文 (`ProgramType::kApt`に対応)
    kApt,
};

/// @brief CLファイルの読込設定
struct ClReadOptions {
    /// @brief 開始行 (元ファイルの行番号. 1始まり)
    int start_line = 1;
    /// @brief 終了行 (含む. 省略時は最終行)
    std::optional<int> end_line;
    /// @brief 長さの換算係数 (ファイル値 × `unit_scale` = mm)
    /// @note 読込後に`ScaleLengths`で適用する
    double unit_scale = 1.0;
    /// @brief `SourceLocation::program_index`に設定する値
    int program_index = 0;
};

/// @brief CLデータの文字列を読み込む
/// @param text CLデータ (改行はLF/CRLFの両方を受理する)
/// @param format 形式
/// @param options 読込設定
/// @return CLプログラム (警告は`ClProgram::warnings`)
/// @throw igesio::DataFormatError 3行1組で行数が3の倍数でない, 数値でない行が
///        ある, または工具軸がゼロベクトルの場合
/// @throw igesio::DataFormatError APTで引数の数または数値に不備がある場合
ClProgram ReadClText(const std::string& text, ClFileFormat format,
                     const ClReadOptions& options = {});

/// @brief CLファイルを読み込む
/// @param path ファイルのパス
/// @param format 形式
/// @param options 読込設定
/// @return CLプログラム (警告は`ClProgram::warnings`)
/// @throw igesio::FileOpenError ファイルを開けない場合
/// @throw igesio::DataFormatError `ReadClText`と同じ
ClProgram ReadClFile(const std::filesystem::path& path, ClFileFormat format,
                     const ClReadOptions& options = {});

/// @brief CLファイルの出力設定
struct ClWriteOptions {
    /// @brief 座標/ベクトルの小数桁
    int decimals = 5;
    /// @brief 改行の種類
    NewlineStyle newline = NewlineStyle::kLf;
    /// @brief APTで`ClComment`/`ClMarker`を`PPRINT`として出力する
    bool comments = true;
};

/// @brief CLプログラムを文字列化する
/// @param program 出力するプログラム
/// @param format 形式
/// @param options 出力設定
/// @param[out] warnings 表せないレコードを省いた等の警告 (種類ごとに件数を
///        集計した1件. `nullptr`なら報告しない)
/// @return CLデータの文字列
/// @note 形式ごとの扱いは以下.
///       (1) 3行1組: コードは`7f`/`80`固定. 制御点を持つ`kWork`の`ClGoto`と円弧
///           のみ出力し、円弧は弦誤差0.01 mm (`options.decimals`によらない) で
///           折れ線化する. 始点不明または法線がゼロベクトルの円弧、状態レコード
///           等は省いて警告する
///       (2) APT: `ClSelectWorkOffset`/`ClLengthOffset`は`PPRINT`にする.
///           `ClEnd`の後のレコードは省く
std::string WriteClText(const ClProgram& program, ClFileFormat format,
                        const ClWriteOptions& options = {},
                        std::vector<Diagnostic>* warnings = nullptr);

/// @brief CLプログラムをファイルに出力する
/// @param path 出力先
/// @param program 出力するプログラム
/// @param format 形式
/// @param options 出力設定
/// @param[out] warnings `WriteClText`と同じ
/// @throw igesio::FileOpenError ファイルを開けない場合
void WriteClFile(const std::filesystem::path& path, const ClProgram& program,
                 ClFileFormat format, const ClWriteOptions& options = {},
                 std::vector<Diagnostic>* warnings = nullptr);

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_TOOLPATH_CL_IO_H_
