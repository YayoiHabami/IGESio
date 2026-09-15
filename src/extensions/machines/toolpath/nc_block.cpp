/**
 * @file extensions/machines/toolpath/nc_block.cpp
 * @brief NCプログラムの字句 (ブロックとワード) の読み書き
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/machines/toolpath/nc_block.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "extensions/machines/toolpath/text_utils.h"

namespace igesio::extensions::machines {

namespace {

using detail::IsAlpha;
using detail::IsDigit;
using detail::IsSpace;
using detail::PushWarning;
using detail::Trim;

/// @brief G/Mの小数コード (`G43.5`等) の判定と整形に用いる小数桁
constexpr int kCodeFractionDigits = 4;

/// @brief 小数部が0とみなす許容誤差 (G/Mの小数コードの判定用)
constexpr double kFractionTolerance = 1e-9;

/// @brief 行に分割する (LF区切り. 末尾のCRは除去する)
/// @param text 分割する文字列
/// @return 各行 (最後の改行の後の空文字列は含めない)
std::vector<std::string_view> SplitLines(const std::string& text) {
    std::vector<std::string_view> lines;
    std::string_view rest(text);
    while (!rest.empty()) {
        const std::size_t pos = rest.find('\n');
        std::string_view line = rest.substr(0, pos);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        lines.push_back(line);
        if (pos == std::string_view::npos) break;
        rest.remove_prefix(pos + 1);
    }
    return lines;
}

/// @brief コメント除去の結果
struct CommentStrip {
    /// @brief コメントを除去した本文
    std::string text;
    /// @brief コメント本文 (出現順. 前後の空白は除去)
    std::vector<std::string> comments;
    /// @brief 閉じられていない`(`があったか
    bool unclosed = false;
};

/// @brief `( … )`と`;`以降のコメントを取り除く
/// @param line 1行 (改行を含まない)
/// @return 本文とコメント. `(`は最初の`)`で閉じ、閉じなければ行末までをコメントにする
CommentStrip StripComments(const std::string_view line) {
    CommentStrip result;
    std::size_t i = 0;
    while (i < line.size()) {
        const char c = line[i];
        if (c == ';') {
            result.comments.emplace_back(Trim(line.substr(i + 1)));
            break;
        }
        if (c == '(') {
            const std::size_t close = line.find(')', i + 1);
            if (close == std::string_view::npos) {
                result.comments.emplace_back(Trim(line.substr(i + 1)));
                result.unclosed = true;
                break;
            }
            result.comments.emplace_back(Trim(line.substr(i + 1, close - i - 1)));
            i = close + 1;
            continue;
        }
        result.text.push_back(c);
        ++i;
    }
    return result;
}

/// @brief ブロックスキップの接頭辞 (`/n`)
struct BlockSkipPrefix {
    /// @brief スイッチ番号 (接頭辞が無ければ`std::nullopt`)
    std::optional<int> number;
    /// @brief 接頭辞の文字数 (除去する長さ)
    std::size_t length = 0;
};

/// @brief 行頭の`/n` (n省略 = 1) を読み取る
/// @param text 空白を除去した本文
/// @return 接頭辞の番号と長さ
BlockSkipPrefix ParseBlockSkip(const std::string_view text) {
    BlockSkipPrefix prefix;
    if (text.empty() || text.front() != '/') return prefix;

    prefix.length = 1;
    if (text.size() > 1 && IsDigit(text[1])) {
        prefix.number = text[1] - '0';
        prefix.length = 2;
    } else {
        prefix.number = 1;
    }
    return prefix;
}

/// @brief 数値の字句 (`[-+]?\d*\.?\d+`または`\d+\.`) を読み取る
/// @param text 読み取る文字列 (先頭から)
/// @param[out] length 読み取った文字数 (数値でなければ0)
/// @param[out] has_decimal_point 小数点を含むか
/// @return 数値. 数値でなければ`std::nullopt`
std::optional<double> ParseNumber(const std::string_view text, std::size_t& length,
                                  bool& has_decimal_point) {
    std::size_t i = 0;
    if (i < text.size() && (text[i] == '-' || text[i] == '+')) ++i;
    std::size_t digits = 0;
    while (i < text.size() && IsDigit(text[i])) { ++i; ++digits; }
    has_decimal_point = false;
    if (i < text.size() && text[i] == '.') {
        has_decimal_point = true;
        ++i;
        while (i < text.size() && IsDigit(text[i])) { ++i; ++digits; }
    }
    length = 0;
    if (digits == 0) return std::nullopt;

    length = i;
    return std::stod(std::string(text.substr(0, i)));
}

/// @brief 本文をワードに分解してブロックに格納する
/// @param text コメントと`/n`を除去した本文
/// @param[in,out] block 格納先 (`words`/`sequence`/`program_number`)
/// @param[out] warnings ワードとして読み取れない文字の警告 (ブロックにつき1件)
void ParseWords(const std::string_view text, NcBlock& block,
                std::vector<Diagnostic>& warnings) {
    std::size_t i = 0;
    bool first_word = true;
    bool reported = false;
    while (i < text.size()) {
        if (IsSpace(text[i])) { ++i; continue; }
        const char raw = text[i];
        // `:`はプログラム番号 (`O`) の代わりの記号で、ブロックの先頭でのみ許す
        const bool colon = (raw == ':' && first_word);
        std::size_t number_start = i + 1;
        while (number_start < text.size() && IsSpace(text[number_start])) {
            ++number_start;
        }
        std::size_t length = 0;
        bool has_point = false;
        std::optional<double> value;
        if (IsAlpha(raw) || colon) {
            value = ParseNumber(text.substr(number_start), length, has_point);
        }
        if (!value.has_value()) {
            if (!reported) {
                PushWarning(&warnings,
                            "unexpected text: " + std::string(Trim(text.substr(i))),
                            block.line);
                reported = true;
            }
            ++i;
            continue;
        }

        i = number_start + length;
        const char address = colon ? 'O'
                : static_cast<char>(std::toupper(static_cast<unsigned char>(raw)));
        if (address == 'N') {
            block.sequence = static_cast<int>(std::lround(*value));
        } else if (address == 'O' && first_word) {
            block.program_number = static_cast<int>(std::lround(*value));
        } else {
            // G/Mは小数コード (`G43.5`) を持つので丸めず、他の整数アドレスは丸める
            const bool keep_fraction = (address == 'G' || address == 'M');
            const double stored = (IsIntegerAddress(address) && !keep_fraction)
                    ? static_cast<double>(std::lround(*value)) : *value;
            block.words.push_back(NcWord{address, stored, has_point});
        }
        first_word = false;
    }
}

/// @brief 1行をブロックにする
/// @param line 元の行
/// @param line_number 行番号 (1始まり)
/// @param options 設定 (ブロックスキップ)
/// @param[out] warnings 警告の追記先
/// @return ブロック. `%`行、空行、スキップした行なら`std::nullopt`
std::optional<NcBlock> LexLine(
        const std::string_view line, const int line_number,
        const NcLexOptions& options, std::vector<Diagnostic>& warnings) {
    std::string_view body = Trim(line);
    if (body == "%" || body.empty()) return std::nullopt;

    NcBlock block;
    block.line = line_number;
    const BlockSkipPrefix prefix = ParseBlockSkip(body);
    if (prefix.number.has_value()) {
        const bool on = std::find(options.block_skip.begin(), options.block_skip.end(),
                                  *prefix.number) != options.block_skip.end();
        if (on) return std::nullopt;
        block.block_skip = prefix.number;
        body.remove_prefix(prefix.length);
    }

    CommentStrip stripped = StripComments(body);
    if (stripped.unclosed) PushWarning(&warnings, "unclosed comment", line_number);
    block.comments = std::move(stripped.comments);
    block.text = std::string(Trim(stripped.text));
    ParseWords(block.text, block, warnings);
    // コメントだけの行もブロックとして残す (`ClComment`の生成元になる)
    return block;
}

/// @brief 小数部を持つか (G/Mの小数コードの判定)
/// @param value 判定する値
bool HasFraction(const double value) {
    return std::abs(value - std::round(value)) > kFractionTolerance;
}

/// @brief 末尾の0を取り除く (`"12.500"` → `"12.5"`、`"12.000"` → `"12."`)
/// @param[in,out] text 整形する文字列 (小数点が無ければ変更しない)
void StripTrailingZeros(std::string& text) {
    if (text.find('.') == std::string::npos) return;

    while (!text.empty() && text.back() == '0') text.pop_back();
}

/// @brief 符号を除いた部分が0だけなら符号を取り除く (`"-0."` → `"0."`)
/// @param[in,out] text 整形する文字列
void RemoveNegativeZero(std::string& text) {
    if (text.empty() || text.front() != '-') return;

    const bool all_zero = std::all_of(text.begin() + 1, text.end(),
                                      [](const char c) { return c == '0' || c == '.'; });
    if (all_zero) text.erase(0, 1);
}

/// @brief G/Mの小数コード (`G43.5`) の数値部分を整形する (末尾の0は付けない)
/// @param value 数値
/// @return `"43.5"`等の文字列
std::string FormatFractionalCode(const double value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(kCodeFractionDigits) << value;
    std::string text = stream.str();
    StripTrailingZeros(text);
    if (!text.empty() && text.back() == '.') text.pop_back();
    return text;
}

}  // namespace



void ProgramTable::Register(const int number, const std::size_t block_index) {
    start_block_[number] = block_index;
}

std::optional<std::size_t> ProgramTable::Find(const int number) const {
    const auto found = start_block_.find(number);
    if (found == start_block_.end()) return std::nullopt;
    return found->second;
}

NcLexResult LexNc(const std::string& text, const NcLexOptions& options) {
    NcLexResult result;
    const std::vector<std::string_view> lines = SplitLines(text);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const int line_number = static_cast<int>(i) + 1;
        if (line_number < options.start_line) continue;
        if (options.end_line.has_value() && line_number > *options.end_line) break;
        std::optional<NcBlock> block =
                LexLine(lines[i], line_number, options, result.warnings);
        if (!block.has_value()) continue;
        if (block->program_number.has_value()) {
            result.programs.Register(*block->program_number, result.blocks.size());
        }
        result.blocks.push_back(std::move(*block));
    }
    return result;
}

bool IsIntegerAddress(const char address) {
    switch (address) {
        case 'G': case 'M': case 'T': case 'H': case 'D':
        case 'N': case 'O': case 'P': case 'L': case 'S':
            return true;
        default:
            return false;
    }
}

std::string FormatNcNumber(const double value, const NcNumberFormat& format) {
    std::string text;
    if (format.decimals <= 0) {
        text = std::to_string(std::lround(value));
        if (format.force_decimal_point) text.push_back('.');
    } else {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(format.decimals) << value;
        text = stream.str();
        if (!format.trailing_zeros) {
            StripTrailingZeros(text);
            if (text.back() == '.' && !format.force_decimal_point) text.pop_back();
        }
    }
    RemoveNegativeZero(text);
    if (!format.leading_zero) {
        // 小数点の後に数字がある場合のみ先頭の0を省く (`"0."`は残す)
        const std::size_t start = (text.front() == '-') ? 1 : 0;
        if (text.size() > start + 2 && text[start] == '0' && text[start + 1] == '.') {
            text.erase(start, 1);
        }
    }
    return text;
}

std::string FormatNcWord(const char address, const double value,
                         const NcNumberFormat& format) {
    std::string text;
    if ((address == 'G' || address == 'M') && HasFraction(value)) {
        text = FormatFractionalCode(value);
    } else if (IsIntegerAddress(address)) {
        text = std::to_string(std::lround(value));
    } else {
        text = FormatNcNumber(value, format);
    }
    return std::string(1, address) + text;
}

std::string FormatNcInteger(const char address, const int value,
                            const int min_digits) {
    std::ostringstream stream;
    stream << address;
    if (value < 0) stream << '-';
    stream << std::setw(std::max(min_digits, 0)) << std::setfill('0')
           << std::abs(value);
    return stream.str();
}

}  // namespace igesio::extensions::machines
