/**
 * @file extensions/machines/toolpath/nc_dialect.cpp
 * @brief 制御装置の定義 (コード名、構文、アドレス表、テンプレート行)
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/machines/toolpath/nc_dialect.h"

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "extensions/machines/toolpath/text_utils.h"

namespace igesio::extensions::machines {

namespace {

using detail::IsAlpha;
using detail::IsDigit;
using detail::PushWarning;
using detail::ToUpper;
using detail::Trim;

/// @brief 全て数字か (空なら`true`)
/// @param text 判定する文字列
bool AllDigits(const std::string_view text) {
    for (const char c : text) {
        if (!IsDigit(c)) return false;
    }
    return true;
}

/// @brief コード名の数値部 (`"068.20"`) を正規化する
/// @param number 数値部 (`\d*(\.\d*)?`)
/// @return 先頭の0を除いた整数部 + (小数部が残れば) `.` + 末尾の0を除いた小数部.
///         数値の形式でなければ`std::nullopt`
std::optional<std::string> NormalizeNumberText(const std::string_view number) {
    const std::size_t point = number.find('.');
    const std::string_view integer = number.substr(0, point);
    const std::string_view fraction =
            point == std::string_view::npos ? std::string_view() : number.substr(point + 1);
    if (!AllDigits(integer) || !AllDigits(fraction)) return std::nullopt;
    if (integer.empty() && fraction.empty()) return std::nullopt;

    std::string_view int_part = integer;
    while (int_part.size() > 1 && int_part.front() == '0') int_part.remove_prefix(1);
    std::string result(int_part.empty() ? "0" : int_part);
    std::string_view frac_part = fraction;
    while (!frac_part.empty() && frac_part.back() == '0') frac_part.remove_suffix(1);
    if (!frac_part.empty()) {
        result.push_back('.');
        result.append(frac_part);
    }
    return result;
}

/// @brief 標準の形式のワーク座標系の選択コード (`G54`〜`G59`、`G54.1P<n>`) か
/// @param id 判定する文字列
bool IsStandardWorkOffsetWord(const std::string_view id) {
    if (id.size() == 3 && id[0] == 'G' && id[1] == '5' && id[2] >= '4' && id[2] <= '9') {
        return true;
    }

    constexpr std::string_view kPrefix = "G54.1P";
    if (id.size() <= kPrefix.size() || id.substr(0, kPrefix.size()) != kPrefix) {
        return false;
    }
    const std::string_view number = id.substr(kPrefix.size());
    return number.front() != '0' && AllDigits(number);
}

}  // namespace



NcDialect DefaultFanucDialect() {
    NcDialect dialect;
    dialect.name = "fanuc";
    return dialect;
}

std::string NormalizeCodeName(const std::string_view text) {
    const std::string_view trimmed = Trim(text);
    if (trimmed.empty()) return std::string();

    const std::optional<std::string> number = NormalizeNumberText(trimmed.substr(1));
    if (!number.has_value() || !IsAlpha(trimmed[0])) return ToUpper(trimmed);
    return ToUpper(trimmed.substr(0, 1)) + *number;
}

std::string ExpandTemplate(const std::string_view line,
                           const std::map<std::string, std::string>& variables,
                           std::vector<Diagnostic>* warnings) {
    std::string result;
    std::size_t i = 0;
    while (i < line.size()) {
        const std::size_t open = line.find('{', i);
        if (open == std::string_view::npos) {
            result.append(line.substr(i));
            break;
        }
        const std::size_t close = line.find('}', open + 1);
        // 閉じられていない`{`はそのまま残す
        if (close == std::string_view::npos) {
            result.append(line.substr(i));
            break;
        }

        result.append(line.substr(i, open - i));
        const std::string name(line.substr(open + 1, close - open - 1));
        const auto found = variables.find(name);
        if (found != variables.end()) {
            result.append(found->second);
        } else {
            PushWarning(warnings, "unknown template variable: " + name);
        }
        i = close + 1;
    }
    return result;
}

std::string WorkOffsetIdFromWord(const NcDialect& dialect, const std::string_view code,
                                 const std::optional<int> p) {
    std::string word(code);
    if (code == "G54.1" && p.has_value()) word += "P" + std::to_string(*p);
    const auto found = dialect.work_offset_ids.find(word);
    if (found != dialect.work_offset_ids.end()) return found->second;
    return word;
}

std::optional<std::string> WorkOffsetWordFromId(const NcDialect& dialect,
                                                const std::string_view id) {
    for (const auto& [word, mapped] : dialect.work_offset_ids) {
        if (mapped == id) return word;
    }
    if (IsStandardWorkOffsetWord(id)) return std::string(id);
    return std::nullopt;
}

}  // namespace igesio::extensions::machines
