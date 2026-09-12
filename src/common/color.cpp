/**
 * @file common/color.cpp
 * @brief RGBA色の値型
 * @author Yayoi Habami
 * @date 2026-09-11
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/common/color.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace igesio {

namespace {

/// @brief 0.0〜1.0のdouble値を8bit値に変換する
/// @param value 成分 (0.0〜1.0)
/// @return 0〜255
int ToByte(const double value) {
    return static_cast<int>(std::lround(std::clamp(value, 0.0, 1.0) * 255.0));
}

/// @brief 16進2桁を整数に変換する
/// @param digits 16進数字2文字 (検査済みであること)
/// @return 0〜255
int ParseHexByte(const std::string_view digits) {
    return std::stoi(std::string(digits), nullptr, 16);
}

}  // namespace



std::optional<Color> Color::TryParseHex(std::string_view text) {
    // 先頭の'#'は省略可能. 残りは6桁 (RRGGBB) または8桁 (RRGGBBAA) の
    // 16進数字のみを受け付ける
    if (!text.empty() && text.front() == '#') text.remove_prefix(1);
    if (text.size() != 6 && text.size() != 8) return std::nullopt;
    const bool all_hex_digits = std::all_of(
            text.begin(), text.end(),
            [](const unsigned char c) { return std::isxdigit(c) != 0; });
    if (!all_hex_digits) return std::nullopt;

    const int r = ParseHexByte(text.substr(0, 2));
    const int g = ParseHexByte(text.substr(2, 2));
    const int b = ParseHexByte(text.substr(4, 2));
    const int a = (text.size() == 8) ? ParseHexByte(text.substr(6, 2)) : 255;
    return FromRGB255(r, g, b, a);
}

Color Color::FromHex(const std::string_view text) {
    const auto color = TryParseHex(text);
    if (!color.has_value()) {
        throw std::invalid_argument(
                "Color::FromHex: text must be in the form "
                "\"#RRGGBB\", \"RRGGBB\", \"#RRGGBBAA\" or \"RRGGBBAA\", "
                "but got \"" + std::string(text) + "\".");
    }
    return *color;
}

std::array<int, 3> Color::ToRGB255() const {
    return {ToByte(r), ToByte(g), ToByte(b)};
}

std::string Color::ToHex(const bool with_alpha) const {
    const auto rgb = ToRGB255();
    std::ostringstream oss;
    oss << '#' << std::hex << std::uppercase << std::setfill('0')
        << std::setw(2) << rgb[0]
        << std::setw(2) << rgb[1]
        << std::setw(2) << rgb[2];
    if (with_alpha) oss << std::setw(2) << ToByte(a);
    return oss.str();
}

}  // namespace igesio
