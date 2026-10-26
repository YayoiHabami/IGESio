/**
 * @file extensions/machines/toolpath/output_lines.cpp
 * @brief 出力行の連結とレコードの出力行の対応付け
 * @author Yayoi Habami
 * @date 2026-10-26
 * @copyright 2026 Yayoi Habami
 */
#include "extensions/machines/toolpath/output_lines.h"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace igesio::extensions::machines::detail {

ProgramText JoinWithLineRanges(const std::vector<std::string>& elements,
                               const std::vector<ElementRange>& ranges,
                               const NewlineStyle newline) {
    const std::string_view separator = NewlineText(newline);
    ProgramText result;
    // 要素kの先頭の物理行（1始まり）. 末尾には最終行+1を設定
    std::vector<int> first_lines;
    first_lines.reserve(elements.size() + 1);
    int line = 1;
    for (const std::string& element : elements) {
        first_lines.push_back(line);
        line += 1 + static_cast<int>(
                std::count(element.begin(), element.end(), '\n'));
        result.text += element;
        result.text += separator;
    }
    first_lines.push_back(line);

    result.record_lines.reserve(ranges.size());
    for (const ElementRange& range : ranges) {
        if (range.begin > range.end || range.end > elements.size()) {
            throw std::out_of_range("element range is out of the output lines");
        }
        const int first = first_lines[range.begin];
        result.record_lines.push_back(
                OutputLineRange{first, first_lines[range.end] - first});
    }
    return result;
}

}  // namespace igesio::extensions::machines::detail
