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
    is_valid = false;
    errors.push_back(error);
}

std::string ValidationResult::Message() const {
    if (is_valid) {
        return "Validation succeeded.";
    } else {
        std::string msg = "Validation failed with errors:\n";
        for (const auto& error : errors) {
            msg += "- " + error.str() + "\n";
        }
        return msg;
    }
}

ValidationResult igesio::MakeValidationResult(std::vector<ValidationError>&& errors) {
    if (errors.empty()) {
        return ValidationResult::Success();
    } else {
        return ValidationResult{false, std::move(errors)};
    }
}

ValidationResult igesio::MakeValidationResult(const std::vector<ValidationError>& errors) {
    if (errors.empty()) {
        return ValidationResult::Success();
    } else {
        return ValidationResult{false, errors};
    }
}
