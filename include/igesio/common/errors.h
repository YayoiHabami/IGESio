/**
 * @file common/errors.h
 * @brief 本プロジェクトで使用するエラーを定義する
 * @author Yayoi Habami
 * @date 2025-04-08
 * @copyright 2025 Yayoi Habami
 * @details 各エラーは、`IGESioError`を継承し、以下のように階層化されている.
 * IGESioError (基底; std::runtime_errorを継承)
 * ├── NotImplementedError      (未実装)
 * ├── ImplementationError      (実装エラー)
 * │
 * ├── ParseError               (解析・変換エラー)
 * |   └── TypeConversionError      (型変換エラー)
 * │
 * ├── FileError                (ファイル操作関連)
 * │   └── FileOpenError            (ファイルオープン失敗)
 * │
 * ├── FileFormatError          (フォーマット関連)
 * │   ├── LineFormatError          (行の形式エラー)
 * │   └── SectionFormatError       (セクション形式エラー)
 * │
 * └── IgesFormatError          (IGESフォーマットエラー)
 *      ├── SectionFormatError      (セクション形式エラー)
 *      ├── LineFormatError         (行の形式エラー)
 *      └── DataFormatError         (データ形式エラー)
 */
#ifndef IGESIO_COMMON_ERRORS_H_
#define IGESIO_COMMON_ERRORS_H_


#include <stdexcept>
#include <string>
#include <utility>

#include "igesio/common/iges_metadata.h"



namespace igesio {

/// @brief IGESioプロジェクトの基底例外クラス
class IGESioError : public std::runtime_error {
 public:
    /// @brief 例外を初期化する
    /// @param type エラータイプ
    /// @param message エラーメッセージ
    IGESioError(const std::string& type, const std::string& message)
        : std::runtime_error(formatMessage(type, message)), type_(type) {}

    /// @brief 例外を初期化する
    /// @param type エラータイプ
    /// @param message エラーメッセージ
    IGESioError(std::string_view type, std::string_view message)
        : IGESioError(std::string(type), std::string(message)) {}

    /// @brief 例外を初期化する（タイプなし）
    /// @param message エラーメッセージ
    explicit IGESioError(const std::string& message)
        : std::runtime_error(message), type_("") {}

    /// @brief デストラクタ
    virtual ~IGESioError() noexcept = default;

    /// @brief エラータイプを取得
    const std::string& type() const { return type_; }

 private:
    /// @brief エラーメッセージの冒頭に付加するエラータイプ
    std::string type_;

    /// @brief タイプとメッセージを結合し、重複を避ける
    static std::string formatMessage(const std::string& type, const std::string& message) {
        if (message.rfind(type + ": ", 0) == 0) {
            // メッセージが既にタイプで始まっていれば、そのままメッセージを返す
            return message;
        }
        return type.empty() ? message : type + ": " + message;
    }

 protected:
    /// @brief このクラスのエラー名
    static std::string ErrorName() { return "IGESioError"; }
};



/******************************************************************************
 * 未実装・実装エラー
 *****************************************************************************/

/// @brief 未実装
class NotImplementedError : public IGESioError {
 public:
    /// @brief 未実装の例外を初期化する
    /// @param message エラーの詳細メッセージ
    explicit NotImplementedError(const std::string& message)
        : IGESioError(ErrorName(), message) {}

    /// @brief デストラクタ
    virtual ~NotImplementedError() noexcept = default;

 protected:
    /// @brief このクラスのエラー名
    static std::string ErrorName() { return "NotImplementedError"; }
};

/// @brief 実装エラー
/// @note 間違った実装を検出するためのエラー
class ImplementationError : public IGESioError {
 public:
    /// @brief 実装エラーを初期化する
    /// @param message エラーの詳細メッセージ
    explicit ImplementationError(const std::string& message)
        : IGESioError(ErrorName(), message) {}

    /// @brief デストラクタ
    virtual ~ImplementationError() noexcept = default;

 protected:
    /// @brief このクラスのエラー名
    static std::string ErrorName() { return "ImplementationError"; }
};



/******************************************************************************
 * 解析・変換エラー
 *****************************************************************************/

/// @brief 解析・変換エラーのカテゴリ
class ParseError : public IGESioError {
 public:
    // 全てのコンストラクタを継承
    using IGESioError::IGESioError;

    /// @brief 解析・変換エラーを初期化する
    /// @param message エラーの詳細メッセージ
    explicit ParseError(const std::string& message)
        : IGESioError(ErrorName(), message) {}

    /// @brief デストラクタ
    virtual ~ParseError() noexcept = default;

 protected:
    /// @brief このクラスのエラー名
    static std::string ErrorName() { return "ParseError"; }
};

/// @brief 型変換エラー
class TypeConversionError : public ParseError {
 public:
    /// @brief 型変換エラーを初期化する
    /// @param message エラーの詳細メッセージ
    explicit TypeConversionError(const std::string& message)
        : ParseError(ErrorName(), message) {}

    /// @brief デストラクタ
    virtual ~TypeConversionError() noexcept = default;

 protected:
    /// @brief このクラスのエラー名
    static std::string ErrorName() { return "TypeConversionError"; }
};


/******************************************************************************
 * ファイル操作関連
 *****************************************************************************/

/// @brief ファイル操作関連のエラーカテゴリ
class FileError : public IGESioError {
 public:
    // 全てのコンストラクタを継承
    using IGESioError::IGESioError;

    /// @brief ファイル操作関連の例外を初期化する
    /// @param message エラーの詳細メッセージ
    explicit FileError(const std::string& message)
        : IGESioError(ErrorName(), message) {}

    /// @brief デストラクタ
    virtual ~FileError() noexcept = default;

 protected:
    /// @brief このクラスのエラー名
    static std::string ErrorName() { return "FileError"; }
};

/// @brief ファイルを開けなかった場合のエラー
class FileOpenError : public FileError {
 private:
    std::string filename_;

 public:
    /// @brief ファイルオープン失敗の例外を初期化する
    /// @param filename 開けなかったファイルのパス
    explicit FileOpenError(const std::string& filename)
        : FileError(ErrorName(), filename),
          filename_(filename) {}

    /// @brief デストラクタ
    virtual ~FileOpenError() noexcept = default;

    /// @brief 開けなかったファイル名を取得する
    /// @return ファイルのパス
    const std::string& getFilename() const noexcept {
        return filename_;
    }

 protected:
    /// @brief このクラスのエラー名
    static std::string ErrorName() { return "FileOpenError"; }
};



/******************************************************************************
 * フォーマット関連
 *****************************************************************************/

/// @brief IGESファイルフォーマット関連のエラーカテゴリ
class FileFormatError : public IGESioError {
 public:
    // 全てのコンストラクタを継承
    using IGESioError::IGESioError;

    /// @brief フォーマット関連の例外を初期化する
    /// @param message エラーの詳細メッセージ
    explicit FileFormatError(const std::string& message)
        : IGESioError(ErrorName(), message) {}

    /// @brief デストラクタ
    virtual ~FileFormatError() noexcept = default;

 protected:
    /// @brief このクラスのエラー名
    static std::string ErrorName() { return "FileFormatError"; }
};

/// @brief 行の形式エラー
/// @note 行の長さ制限違反など
class LineFormatError : public FileFormatError {
 public:
    /// @brief 行の形式エラーを初期化する
    /// @param message エラーの詳細メッセージ
    explicit LineFormatError(const std::string& message)
        : FileFormatError(ErrorName(), message) {}

    /// @brief 行の形式エラーを初期化する
    /// @param line エラーの行
    /// @param message エラーの詳細メッセージ
    LineFormatError(const std::string& line, const std::string& message)
        : FileFormatError(ErrorName(), message + ": " + line) {}

    /// @brief デストラクタ
    virtual ~LineFormatError() noexcept = default;

 protected:
    /// @brief このクラスのエラー名
    static std::string ErrorName() { return "LineFormatError"; }
};

/// @brief セクション形式エラー
/// @note セクション識別子や区切り文字の不正など
class SectionFormatError : public FileFormatError {
 public:
    /// @brief セクション形式エラーを初期化する
    /// @param message エラーの詳細メッセージ
    explicit SectionFormatError(const std::string& message)
        : FileFormatError(ErrorName(), message) {}

    /// @brief セクション形式エラーを初期化する
    /// @param section_type セクションの種類
    /// @param message エラーの詳細メッセージ
    SectionFormatError(SectionType section_type, const std::string& message)
        : FileFormatError(ErrorName(),
                          message + " (at " + SectionTypeToString(section_type) +
                          " section)") {}

    /// @brief デストラクタ
    virtual ~SectionFormatError() noexcept = default;

 protected:
    /// @brief このクラスのエラー名
    static std::string ErrorName() { return "SectionFormatError"; }
};

/// @brief データ形式エラー
/// @note データの不正な形式や値など
class DataFormatError : public FileFormatError {
 public:
    /// @brief データ形式エラーを初期化する
    /// @param message エラーの詳細メッセージ
    explicit DataFormatError(const std::string& message)
        : FileFormatError(ErrorName(), message) {}

    /// @brief データ形式エラーを初期化する
    /// @param section_type セクションの種類
    /// @param message エラーの詳細メッセージ
    DataFormatError(SectionType section_type, const std::string& message)
        : FileFormatError(ErrorName(),
                          message + " (at " + SectionTypeToString(section_type) +
                          " section)") {}

    /// @brief デストラクタ
    virtual ~DataFormatError() noexcept = default;

 protected:
    /// @brief このクラスのエラー名
    static std::string ErrorName() { return "DataFormatError"; }
};

}  // namespace igesio

#endif  // IGESIO_COMMON_ERRORS_H_
