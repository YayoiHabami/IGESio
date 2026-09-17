/**
 * @file extensions/machines/core/formatting.cpp
 * @brief machines拡張におけるテキスト出力用のフォーマットおよび相互変換
 * @author Yayoi Habami
 * @date 2026-09-09
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/machines/core/formatting.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

#include "igesio/extensions/machines/core/units.h"

namespace igesio::extensions::machines {

namespace {

/// @brief 色表記の文字数 (`#`と16進6桁)
constexpr std::size_t kHexColorLength = 7;

/// @brief 短縮形 (`RGB`/`RGBA`) の各桁を2回繰り返して`RRGGBB`/`RRGGBBAA`にする
/// @param digits `#`を除いた16進の桁 (3桁または4桁)
/// @return 展開した桁
std::string ExpandShortHex(const std::string_view digits) {
    std::string expanded;
    for (const char c : digits) {
        expanded += c;
        expanded += c;
    }
    return expanded;
}

}  // namespace



std::string FormatFixed(const double value, const int digits) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(digits) << value;
    return stream.str();
}

std::string FormatDegrees(const double radians, const int digits) {
    return FormatFixed(ToDegrees(radians), digits);
}

std::optional<Color> ParseHexColor(const std::string_view text) {
    // ファイル上の色表記は`#rrggbb`のみとし、Color::TryParseHexで指定可能な
    // `#`の省略・8桁はここで弾く
    if (text.size() != kHexColorLength || text.front() != '#') return std::nullopt;
    return Color::TryParseHex(text);
}

std::optional<Color> ParseHexColorRgba(const std::string_view text) {
    if (text.empty() || text.front() != '#') return std::nullopt;
    const std::string_view digits = text.substr(1);
    // 16進かどうかの検査と6桁/8桁の変換はColor::TryParseHexで行う
    if (digits.size() == 3 || digits.size() == 4) {
        return Color::TryParseHex(ExpandShortHex(digits));
    }
    return Color::TryParseHex(digits);
}

std::string FormatHexColor(const Color& color) {
    // 丸め等はColor::ToHexで行い、ファイル上の表記対応 (小文字) のみここで行う
    std::string text = color.ToHex();
    std::transform(text.begin(), text.end(), text.begin(),
                   [](const unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                   });
    return text;
}



/**
 * ---- テキストファイルの改行と文字コード ----
 */

std::optional<NewlineStyle> ParseNewlineStyle(const std::string_view text) {
    if (text == "lf") return NewlineStyle::kLf;
    if (text == "crlf") return NewlineStyle::kCrlf;
    return std::nullopt;
}

std::string_view NewlineStyleName(const NewlineStyle style) {
    return style == NewlineStyle::kCrlf ? "crlf" : "lf";
}

std::string_view NewlineText(const NewlineStyle style) {
    return style == NewlineStyle::kCrlf ? "\r\n" : "\n";
}

std::optional<TextEncoding> ParseTextEncoding(const std::string_view text) {
    if (text == "utf-8") return TextEncoding::kUtf8;
    if (text == "shift_jis") return TextEncoding::kShiftJis;
    return std::nullopt;
}

std::string_view TextEncodingName(const TextEncoding encoding) {
    return encoding == TextEncoding::kShiftJis ? "shift_jis" : "utf-8";
}

}  // namespace igesio::extensions::machines
