/**
 * @file common/validation_result.h
 * @brief パラメータなどの検証結果を格納する`ValidationError`クラスと`ValidationResult`クラスを定義する
 * @author Yayoi Habami
 * @date 2025-07-12
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_COMMON_VALIDATION_RESULT_H_
#define IGESIO_COMMON_VALIDATION_RESULT_H_

#include <string>
#include <vector>
#include <utility>

namespace igesio {

/// @brief パラメータ検証時に生じたエラー情報を提供する構造体
/// @note `v_err << "message"`のように、ストリーム出力演算子を使用して
///       エラーメッセージを追加することができる.
struct ValidationError {
    /// @brief 検証に失敗した場合のエラーメッセージ
    std::string error_message;

    /// @brief デフォルトコンストラクタ (無効化)
    ValidationError() = delete;

    /// @brief エラーメッセージを指定するコンストラクタ
    /// @param message エラーメッセージ
    explicit ValidationError(const std::string& message) : error_message(message) {}
    explicit ValidationError(std::string&& message) : error_message(std::move(message)) {}

    /// @brief コピーコンストラクタ
    ValidationError(const ValidationError& other) = default;

    /// @brief ムーブコンストラクタ
    ValidationError(ValidationError&& other) noexcept = default;

    /// @brief コピー代入演算子
    ValidationError& operator=(const ValidationError& other) = default;

    /// @brief ムーブ代入演算子
    ValidationError& operator=(ValidationError&& other) noexcept = default;

    /// @brief デストラクタ
    ~ValidationError() = default;

    /// @brief ストリーム出力演算子（左辺値）
    /// @param value 出力する値
    /// @return 自身への参照
    template<typename T>
    ValidationError& operator<<(const T& value) {
        error_message.append(std::to_string(value));
        return *this;
    }

    /// @brief ストリーム出力演算子（左辺値）
    /// @param value 出力する値
    /// @return 自身への参照
    ValidationError& operator<<(const std::string& value);

    /// @brief ストリーム出力演算子（左辺値）
    /// @param value 出力する値
    /// @return 自身への参照
    ValidationError& operator<<(const char* value);

    /// @brief ストリーム出力演算子（右辺値）
    /// @param value 出力する値
    /// @return 自身への参照
    template<typename T>
    ValidationError&& operator<<(T&& value) && {
        *this << std::forward<T>(value);
        return std::move(*this);
    }

    /// @brief エラーメッセージを文字列として取得
    /// @return エラーメッセージ
    const std::string& str() const noexcept;
};



/// @brief パラメータの検証結果を返すための構造体
struct ValidationResult {
    /// @brief 検証が成功したかどうか
    bool is_valid = true;
    /// @brief 検証に失敗した場合のエラー情報
    std::vector<ValidationError> errors;

    /// @brief エラーなしの場合
    static ValidationResult Success();
    /// @brief 単一のエラーを持つ場合
    /// @param error_msg 検証エラーのメッセージ
    static ValidationResult Failure(const std::string& error);

    /// @brief 他の検証結果をマージする
    /// @param other 他の検証結果
    void Merge(const ValidationResult& other);

    /// @brief エラーを追加する
    /// @param error 検証エラー
    void AddError(const ValidationError& error);

    /// @brief メッセージを生成する
    /// @return 検証結果のメッセージ
    std::string Message() const;
};



/// @brief エラー内容のリストから検証結果を生成する
/// @param errors 検証エラーのリスト (`std::move`で呼び出すこと)
/// @return 検証結果 (errorsが空であればSuccessを、そうでなければFailureを返す)
ValidationResult MakeValidationResult(std::vector<ValidationError>&& errors);

/// @brief エラー内容のリストから検証結果を生成する
/// @param errors 検証エラーのリスト
/// @return 検証結果 (errorsが空であればSuccessを、そうでなければFailureを返す)
ValidationResult MakeValidationResult(const std::vector<ValidationError>& errors);

}  // namespace igesio

#endif  // IGESIO_COMMON_VALIDATION_RESULT_H_
