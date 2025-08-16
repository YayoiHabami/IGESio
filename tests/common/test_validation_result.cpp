/**
 * @file common/test_validation_result.cpp
 * @brief common/validation_result.hのテスト
 * @author Yayoi Habami
 * @date 2025-08-11
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <string>
#include <utility>

#include "igesio/common/validation_result.h"



/**
 * ValidationError: コンストラクタなど
 */

TEST(ValidationErrorTest, ConstructorWithMessage) {
    igesio::ValidationError error("Test error message");
    ASSERT_EQ(error.str(), "Test error message");
}

TEST(ValidationErrorTest, MoveConstructor) {
    std::string msg = "Test error message";
    igesio::ValidationError error(std::move(msg));
    ASSERT_EQ(error.str(), "Test error message");
}

TEST(ValidationErrorTest, CopyConstructor) {
    igesio::ValidationError original("Test error message");
    igesio::ValidationError copy = original;
    ASSERT_EQ(copy.str(), "Test error message");
}

TEST(ValidationErrorTest, MoveAssignment) {
    igesio::ValidationError error("Initial message");
    igesio::ValidationError another("Another message");
    error = std::move(another);
    ASSERT_EQ(error.str(), "Another message");
}

TEST(ValidationErrorTest, CopyAssignment) {
    igesio::ValidationError error("Initial message");
    igesio::ValidationError another("Another message");
    error = another;
    ASSERT_EQ(error.str(), "Another message");
}



/**
 * ValidationError: 演算子オーバーロード
 */

TEST(ValidationErrorTest, StreamOutputOperatorString) {
    igesio::ValidationError error("Error: ");
    error << "test";
    ASSERT_EQ(error.str(), "Error: test");
}

TEST(ValidationErrorTest, StreamOutputOperatorChar) {
    igesio::ValidationError error("Error: ");
    error << "test";
    ASSERT_EQ(error.str(), "Error: test");
}

TEST(ValidationErrorTest, StreamOutputOperatorRvalue) {
    igesio::ValidationError error("Error: ");
    error << std::string("test");
    ASSERT_EQ(error.str(), "Error: test");
}

TEST(ValidationErrorTest, StreamOutputOperatorInt) {
    igesio::ValidationError error("Error code: ");
    error << 123;
    ASSERT_EQ(error.str(), "Error code: 123");
}

TEST(ValidationErrorTest, StreamOutputOperatorDouble) {
    igesio::ValidationError error("Value: ");
    error << 3.14;
    ASSERT_EQ(error.str(), "Value: 3.140000");
}



/**
 * ValidationResult: staticメンバ関数
 */

TEST(ValidationResultTest, Success) {
    igesio::ValidationResult result = igesio::ValidationResult::Success();
    ASSERT_TRUE(result.is_valid);
    ASSERT_TRUE(result.errors.empty());
}

TEST(ValidationResultTest, Failure) {
    igesio::ValidationResult result = igesio::ValidationResult::Failure("Test failure");
    ASSERT_FALSE(result.is_valid);
    ASSERT_EQ(result.errors.size(), 1);
    ASSERT_EQ(result.errors[0].str(), "Test failure");
}



/**
 * ValidationResult: メンバ関数
 */

TEST(ValidationResultTest, MergeSuccess) {
    igesio::ValidationResult result1 = igesio::ValidationResult::Success();
    igesio::ValidationResult result2 = igesio::ValidationResult::Success();
    result1.Merge(result2);
    ASSERT_TRUE(result1.is_valid);
    ASSERT_TRUE(result1.errors.empty());
}

TEST(ValidationResultTest, MergeFailure) {
    igesio::ValidationResult result1 = igesio::ValidationResult::Success();
    igesio::ValidationResult result2 = igesio::ValidationResult::Failure("Test failure");
    result1.Merge(result2);
    ASSERT_FALSE(result1.is_valid);
    ASSERT_EQ(result1.errors.size(), 1);
    ASSERT_EQ(result1.errors[0].str(), "Test failure");
}

TEST(ValidationResultTest, AddError) {
    igesio::ValidationResult result = igesio::ValidationResult::Success();
    igesio::ValidationError error("Test error");
    result.AddError(error);
    ASSERT_FALSE(result.is_valid);
    ASSERT_EQ(result.errors.size(), 1);
    ASSERT_EQ(result.errors[0].str(), "Test error");
}

TEST(ValidationResultTest, MessageSuccess) {
    igesio::ValidationResult result = igesio::ValidationResult::Success();
    ASSERT_EQ(result.Message(), "Validation succeeded.");
}

TEST(ValidationResultTest, MessageFailure) {
    igesio::ValidationResult result = igesio::ValidationResult::Failure("Test failure");
    ASSERT_EQ(result.Message(), "Validation failed with errors:\n- Test failure\n");
}

TEST(ValidationResultTest, MessageMultipleFailures) {
    igesio::ValidationResult result = igesio::ValidationResult::Success();
    result.AddError(igesio::ValidationError("Error 1"));
    result.AddError(igesio::ValidationError("Error 2"));
    std::string expectedMessage = "Validation failed with errors:\n- Error 1\n- Error 2\n";
    ASSERT_EQ(result.Message(), expectedMessage);
}



/**
 * MakeValidationResult
 */

TEST(MakeValidationResultTest, EmptyErrors) {
    std::vector<igesio::ValidationError> errors;
    igesio::ValidationResult result = igesio::MakeValidationResult(std::move(errors));
    ASSERT_TRUE(result.is_valid);
    ASSERT_TRUE(result.errors.empty());
}

TEST(MakeValidationResultTest, WithErrors) {
    std::vector<igesio::ValidationError> errors;
    errors.emplace_back("Error 1");
    errors.emplace_back("Error 2");
    igesio::ValidationResult result = igesio::MakeValidationResult(std::move(errors));
    ASSERT_FALSE(result.is_valid);
    ASSERT_EQ(result.errors.size(), 2);
    ASSERT_EQ(result.errors[0].str(), "Error 1");
    ASSERT_EQ(result.errors[1].str(), "Error 2");
}

TEST(MakeValidationResultTest, ConstEmptyErrors) {
    const std::vector<igesio::ValidationError> errors;
    igesio::ValidationResult result = igesio::MakeValidationResult(errors);
    ASSERT_TRUE(result.is_valid);
    ASSERT_TRUE(result.errors.empty());
}

TEST(MakeValidationResultTest, ConstWithErrors) {
    std::vector<igesio::ValidationError> errors;
    errors.emplace_back("Error 1");
    errors.emplace_back("Error 2");
    const std::vector<igesio::ValidationError>& constErrors = errors;
    igesio::ValidationResult result = igesio::MakeValidationResult(constErrors);
    ASSERT_FALSE(result.is_valid);
    ASSERT_EQ(result.errors.size(), 2);
    ASSERT_EQ(result.errors[0].str(), "Error 1");
    ASSERT_EQ(result.errors[1].str(), "Error 2");
}

