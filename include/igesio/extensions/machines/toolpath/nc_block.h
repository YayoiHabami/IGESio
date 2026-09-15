/**
 * @file extensions/machines/toolpath/nc_block.h
 * @brief NCプログラムの字句 (ブロックとワード) の読み書き
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note 読込側 (`LexNc`) はコメント、ブロックスキップ、行範囲を処理してワードの
 *       列に分け、Gコードの判別は行わない (処理は`nc_interpreter.h`).
 *       出力側 (`FormatNcWord`) はNCのワードの数値書式を担い、整数アドレス
 *       (`IsIntegerAddress`) を読込と共用する.
 * @note 用語は以下のとおり.
 *       (1) ワード: NCのアドレス文字1つと数値の組 (`X12.5`等)
 *       (2) ブロック: 1行分のワードの列
 *       (3) 整数アドレス: 値を整数として扱うアドレス (`IsIntegerAddress`)
 * @note 入力はバイト列のまま扱う (Shift_JIS指定でもコメントの内容は処理しない).
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_TOOLPATH_NC_BLOCK_H_
#define IGESIO_EXTENSIONS_MACHINES_TOOLPATH_NC_BLOCK_H_

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "igesio/extensions/machines/core/diagnostics.h"

namespace igesio::extensions::machines {

/// @brief NCのワード (アドレス文字と数値)
struct NcWord {
    /// @brief アドレス (大文字に正規化済み)
    char address = 'G';
    /// @brief 数値
    /// @note G/M以外の整数アドレスは整数に丸めて格納する.
    ///       G/Mは小数コード (`G43.5`等) のため丸めない
    double value = 0.0;
    /// @brief 元の文字列に小数点があったか
    bool has_decimal_point = false;
};

/// @brief ブロック (1行)
struct NcBlock {
    /// @brief 元ファイルの行番号 (1始まり)
    int line = 0;
    /// @brief ワード (出現順)
    /// @note `N`と`O`は含めない (`sequence`/`program_number`に格納する)
    std::vector<NcWord> words;
    /// @brief シーケンス番号 (N)
    std::optional<int> sequence;
    /// @brief プログラム番号 (O)
    /// @note ブロックがプログラム定義の開始のときのみ持つ
    std::optional<int> program_number;
    /// @brief ブロックスキップ番号 (`/n`)
    /// @note 無ければ`std::nullopt`
    std::optional<int> block_skip;
    /// @brief コメント除去後の本文 (診断/GUI表示/`ClPassThrough`用)
    std::string text;
    /// @brief コメント本文 (`( … )`と`;`以降. `ClComment`用)
    std::vector<std::string> comments;
};

/// @brief 同一ファイル内のプログラム定義 (O番号→開始ブロック) の表
class ProgramTable {
 public:
    /// @brief プログラム定義を登録する
    /// @param number O番号
    /// @param block_index 開始ブロックの索引 (`NcLexResult::blocks`)
    /// @note 同じO番号は後の登録で上書きする
    void Register(int number, std::size_t block_index);

    /// @brief O番号の開始ブロックを探す
    /// @param number O番号
    /// @return `NcLexResult::blocks`の索引. 未定義なら`std::nullopt`
    std::optional<std::size_t> Find(int number) const;

    /// @brief 登録されたプログラム定義の数
    std::size_t Size() const { return start_block_.size(); }

    /// @brief 登録が無いか
    bool Empty() const { return start_block_.empty(); }

 private:
    /// @brief O番号 → 開始ブロックの索引
    std::map<int, std::size_t> start_block_;
};

/// @brief 字句解析の設定
struct NcLexOptions {
    /// @brief 開始行 (1始まり)
    int start_line = 1;
    /// @brief 終了行 (含む. 省略時は最終行)
    std::optional<int> end_line;
    /// @brief ONにするブロックスキップスイッチ番号 (該当するブロックを除去する)
    std::vector<int> block_skip;
};

/// @brief 字句解析の結果
struct NcLexResult {
    /// @brief ブロック列 (`%`行、空行、スキップしたブロックは含まない)
    std::vector<NcBlock> blocks;
    /// @brief プログラム定義
    ProgramTable programs;
    /// @brief 警告 (`context`は空、`line`は行番号)
    /// @note 閉じない括弧、ワードとして読み取れない文字を報告する
    std::vector<Diagnostic> warnings;
};

/// @brief NCプログラムの字句解析を行う
/// @param text NCプログラム (改行はLF/CRLFの両方を受理する)
/// @param options 設定
/// @return ブロック列、プログラム定義、警告
/// @note 規則は以下.
///       (1) `( … )`は最初の`)`で閉じる. 閉じなければ行末までをコメントにして
///           警告する. `;`以降は行末コメント
///       (2) 行頭の`/n` (n省略 = 1) は`block_skip`に含まれれば行を除去し,
///           含まれなければ記号を除去して続行する
///       (3) ワードは`[A-Za-z]\s*[-+]?(\d*\.?\d+|\d+\.)`で、小文字アドレスは
///           大文字に正規化する. `N`は`sequence`に格納する
///       (4) `O`番号 (または`:`) で始まるブロックは`program_number`を持ち,
///           `ProgramTable`に登録する. 先頭以外の`O`は通常のワード
///       (5) ワードとして読み取れない文字は警告して読み飛ばす (ブロックにつき1件)
NcLexResult LexNc(const std::string& text, const NcLexOptions& options = {});

/// @brief 整数として扱うアドレスか (G/M/T/H/D/N/O/P/L/S)
/// @param address アドレス (大文字)
/// @note 読込 (`LexNc`の丸め) と出力 (`FormatNcWord`) で同じ表を使う
bool IsIntegerAddress(char address);

/// @brief 実数値の書式 (座標語、IJK指令、Fコード、回転軸の角度指令で別々に持つ)
struct NcNumberFormat {
    /// @brief 小数桁 (0なら整数)
    int decimals = 4;
    /// @brief 末尾の0を残す (`"12.500"`)
    bool trailing_zeros = false;
    /// @brief 整数値でも小数点を付ける (`"12."`)
    bool force_decimal_point = true;
    /// @brief 先頭の0を付ける (`"0.5"`. `false`なら`".5"`)
    bool leading_zero = true;
};

/// @brief 実数をワードの数値部分 (アドレス無し) に整形する
/// @param value 数値
/// @param format 実数値の書式
/// @return 書式に従った文字列 (`"12.5"` / `"12."` / `".5"`等)
/// @note -0は0にする
std::string FormatNcNumber(double value, const NcNumberFormat& format);

/// @brief アドレスと数値をワードの文字列に整形する
/// @param address アドレス
/// @param value 数値
/// @param format 実数値の書式
/// @return 整形したワード (`"X12.5"`等)
/// @note 数値の書式は以下.
///       (1) G/Mで小数部があれば小数コードとして`value`の小数部を出力する
///           (`"G43.5"`)
///       (2) 他の整数アドレスは`format`を無視して整数にする
///       (3) それ以外は`FormatNcNumber`に従う
std::string FormatNcWord(char address, double value,
                         const NcNumberFormat& format);

/// @brief 桁数固定の整数のワードを整形する (`O0001` / `N0010`)
/// @param address アドレス
/// @param value 整数値
/// @param min_digits 最小桁数 (足りなければ先頭に0を付ける)
/// @return 整形したワード (負の値は符号の後に0を付ける)
std::string FormatNcInteger(char address, int value, int min_digits);

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_TOOLPATH_NC_BLOCK_H_
