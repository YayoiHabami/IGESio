/**
 * @file extensions/machines/core/diagnostics.cpp
 * @brief machines拡張の警告/infoと運動学エラー
 * @author Yayoi Habami
 * @date 2026-09-08
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/machines/core/diagnostics.h"

#include <string>

namespace igesio::extensions::machines {

std::string FormatDiagnostic(const Diagnostic& diagnostic) {
    std::string text = diagnostic.severity == Severity::kWarning
            ? "[warning] " : "[info] ";
    if (!diagnostic.context.empty()) {
        text += diagnostic.context + ": ";
    }
    text += diagnostic.message;
    if (diagnostic.line > 0) {
        text += " (line " + std::to_string(diagnostic.line) + ")";
    }
    return text;
}

}  // namespace igesio::extensions::machines
