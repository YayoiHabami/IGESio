/**
 * @file common/validation_result.cpp
 * @brief パラメータなどの検証結果を格納する`ValidationError`クラスと`ValidationResult`クラスを定義する
 * @author Yayoi Habami
 * @date 2025-07-12
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/common/validation_result.h"

#include <string>
#include <utility>
#include <vector>

namespace {

using ValidationError = igesio::ValidationError;
using ValidationResult = igesio::ValidationResult;

}  // namespace



ValidationError& ValidationError::operator<<(const std::string& value) {
    error_message.append(value);
    return *this;
}

ValidationError& ValidationError::operator<<(const char* value) {
    error_message.append(value);
    return *this;
}

const std::string& ValidationError::str() const noexcept {
    return error_message;
}



ValidationResult ValidationResult::Success() {
    return ValidationResult{true, {}};
}

ValidationResult ValidationResult::Failure(const std::string& error) {
    return ValidationResult{false, { ValidationError(error) }};
}

void ValidationResult::Merge(const ValidationResult& other) {
    is_valid = is_valid && other.is_valid;
    errors.insert(errors.end(), other.errors.begin(), other.errors.end());
}

void ValidationResult::AddError(const ValidationError& error) {
    // kWarningはis_validに影響しない (描画/使用をブロックしない)
    if (error.severity == igesio::ValidationSeverity::kError) {
        is_valid = false;
    }
    errors.push_back(error);
}

std::string ValidationResult::Message() const {
    if (errors.empty()) {
        return "Validation succeeded.";
    }
    // is_valid (= kErrorなし) なら警告のみ。kErrorがあれば従来通り失敗扱い。
    std::string msg = is_valid ? "Validation passed with warnings:\n"
                               : "Validation failed with errors:\n";
    for (const auto& error : errors) {
        if (error.severity == igesio::ValidationSeverity::kWarning) {
            msg += "- [warning] " + error.str() + "\n";
        } else {
            msg += "- " + error.str() + "\n";
        }
    }
    return msg;
}

namespace {

/// @brief エラーリストにkErrorが1つでも含まれるか
bool HasError(const std::vector<ValidationError>& errors) {
    for (const auto& e : errors) {
        if (e.severity == igesio::ValidationSeverity::kError) return true;
    }
    return false;
}

}  // namespace

ValidationResult igesio::MakeValidationResult(std::vector<ValidationError>&& errors) {
    // kErrorが無ければis_valid=true (警告のみは有効扱い)
    const bool valid = !HasError(errors);
    return ValidationResult{valid, std::move(errors)};
}

ValidationResult igesio::MakeValidationResult(const std::vector<ValidationError>& errors) {
    const bool valid = !HasError(errors);
    return ValidationResult{valid, errors};
}
