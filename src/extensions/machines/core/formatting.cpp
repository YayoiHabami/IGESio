/**
 * @file extensions/machines/core/formatting.cpp
 * @brief machines拡張におけるテキスト出力用のフォーマットおよび相互変換
 * @author Yayoi Habami
 * @date 2026-09-09
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/machines/core/formatting.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <sstream>

#include "igesio/extensions/machines/core/units.h"

namespace igesio::extensions::machines {

namespace {

/// @brief 色成分の量子化段階数 (8bit)
constexpr double kColorLevels = 255.0;

/// @brief 16進1桁を数値にする
/// @param c 文字
/// @return 0~15. 16進でなければ`std::nullopt`
std::optional<int> HexDigit(const char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return std::nullopt;
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

std::optional<std::array<float, 3>> ParseHexColor(const std::string_view text) {
    if (text.size() != 7 || text[0] != '#') return std::nullopt;
    std::array<float, 3> rgb{};
    for (std::size_t i = 0; i < 3; ++i) {
        const std::optional<int> high = HexDigit(text[1 + 2 * i]);
        const std::optional<int> low = HexDigit(text[2 + 2 * i]);
        if (!high.has_value() || !low.has_value()) return std::nullopt;
        rgb[i] = static_cast<float>(*high * 16 + *low) / static_cast<float>(kColorLevels);
    }
    return rgb;
}

std::string FormatHexColor(const std::array<float, 3>& rgb) {
    std::ostringstream stream;
    stream << '#' << std::hex << std::setfill('0');
    for (const float channel : rgb) {
        const double clamped = std::clamp(static_cast<double>(channel), 0.0, 1.0);
        stream << std::setw(2) << std::lround(clamped * kColorLevels);
    }
    return stream.str();
}

}  // namespace igesio::extensions::machines
