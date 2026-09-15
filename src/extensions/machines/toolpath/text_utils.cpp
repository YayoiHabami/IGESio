/**
 * @file extensions/machines/toolpath/text_utils.cpp
 * @brief NC/CLの読み書きで共用する文字列と診断関連の関数群
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 */
#include "extensions/machines/toolpath/text_utils.h"

#include <cctype>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace igesio::extensions::machines::detail {

bool IsSpace(const char c) {
    return std::isspace(static_cast<unsigned char>(c)) != 0;
}

bool IsDigit(const char c) {
    return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

bool IsAlpha(const char c) {
    return std::isalpha(static_cast<unsigned char>(c)) != 0;
}

std::string_view Trim(std::string_view text) {
    while (!text.empty() && IsSpace(text.front())) text.remove_prefix(1);
    while (!text.empty() && IsSpace(text.back())) text.remove_suffix(1);
    return text;
}

std::string ToUpper(const std::string_view text) {
    std::string upper(text);
    for (char& c : upper) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return upper;
}

void PushDiagnostic(std::vector<Diagnostic>* diagnostics, const Severity severity,
                    std::string message, const int line) {
    if (diagnostics == nullptr) return;
    diagnostics->push_back(Diagnostic{severity, "", std::move(message), line});
}

void PushWarning(std::vector<Diagnostic>* diagnostics, std::string message,
                 const int line) {
    PushDiagnostic(diagnostics, Severity::kWarning, std::move(message), line);
}

}  // namespace igesio::extensions::machines::detail
