/**
 * @file common/serialization.h
 * @brief IGESデータ文字列からC++のデータ型への変換や、その逆の操作を定義する
 * @author Yayoi Habami
 * @date 2025-05-30
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_COMMON_SERIALIZATION_H_
#define IGESIO_COMMON_SERIALIZATION_H_

#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include "igesio/common/errors.h"
#include "igesio/common/iges_metadata.h"

namespace igesio {

/// @brief 整数表現のためのバイナリビット数のデフォルト値
/// @note C++側の制約 (for Global Parameter 7; IGES5.3ではデフォルト値は設定されていない)
constexpr int kDefaultIntegerBits = std::numeric_limits<unsigned int>::digits;
/// @brief 単精度浮動小数点数で表現可能な最大10の累乗のデフォルト値
/// @note C++側の制約 (for Global Parameter 8; IGES5.3ではデフォルト値は設定されていない)
constexpr int kDefaultSinglePrecisionPowerMax = std::numeric_limits<float>::max_exponent10;
/// @brief 単精度浮動小数点数の有効桁数のデフォルト値
/// @note C++側の制約 (for Global Parameter 9; IGES5.3ではデフォルト値は設定されていない)
constexpr int kDefaultSinglePrecisionDigits = std::numeric_limits<float>::digits10;
/// @brief 倍精度浮動小数点数で表現可能な最大10の累乗のデフォルト値
/// @note C++側の制約 (for Global Parameter 10; IGES5.3ではデフォルト値は設定されていない)
constexpr int kDefaultDoublePrecisionPowerMax = std::numeric_limits<double>::max_exponent10;
/// @brief 倍精度浮動小数点数の有効桁数のデフォルト値
/// @note C++側の制約 (for Global Parameter 11; IGES5.3ではデフォルト値は設定されていない)
constexpr int kDefaultDoublePrecisionDigits = std::numeric_limits<double>::digits10;

/// @brief 数値データを文字列化する際に必要なパラメータを保持する構造体
struct SerializationConfig {
    /// @brief 整数値のビット数
    int integer_bits = kDefaultIntegerBits;
    /// @brief 単精度浮動小数点数で表現可能な最大10の累乗
    int single_precision_power_max = kDefaultSinglePrecisionPowerMax;
    /// @brief 単精度浮動小数点数の有効桁数
    int single_precision_digits = kDefaultSinglePrecisionDigits;
    /// @brief 倍精度浮動小数点数で表現可能な最大10の累乗
    int double_precision_power_max = kDefaultDoublePrecisionPowerMax;
    /// @brief 倍精度浮動小数点数の有効桁数
    int double_precision_digits = kDefaultDoublePrecisionDigits;
};

/// @brief IGESファイルにおける、パラメータの型・表現形式を表す型
/// @note conformance rules のために使用、元の表現から極力差異を生まない
///       ようにする (See Section 1.4.7)
struct ValueFormat {
 public:
    /// @brief パラメータの型
    IGESParameterType type;
    /// @brief デフォルト値か (kLanguageStatement以外で使用)
    bool is_default = false;
    /// @brief 頭に+がつくか (kInteger, kRealの正値でのみ使用)
    /// @note kInteger, kRealの正値は、頭に+をオプションでつけることができる
    bool has_plus_sign = false;
    /// @brief 整数部があるか (kRealのみで使用)
    bool has_integer = false;
    /// @brief 小数部があるか (kRealのみで使用)
    bool has_fraction = false;
    /// @brief 指数部があるか (kRealのみで使用)
    bool has_exponent = false;
    /// @brief 単精度か (kRealのみで使用)
    /// @note 単精度の場合はEの指数部を使用し、倍精度の場合はDの指数部を使用する
    bool is_single_precision = false;

    /// @brief Logical型のValueFormatを生成
    /// @param is_default デフォルト値を使用したか (未指定または空白のみであったか)
    static ValueFormat Logical(bool is_default = false) {
        return {IGESParameterType::kLogical, is_default};
    }
    /// @brief Integer型のValueFormatを生成
    /// @param is_default デフォルト値を使用したか (未指定または空白のみであったか)
    /// @param has_plus_sign 正の整数値に+をつけるかどうか
    static ValueFormat Integer(bool is_default = false,
                               bool has_plus_sign = false) {
        return {IGESParameterType::kInteger, is_default, has_plus_sign};
    }
    /// @brief Real型のValueFormatを生成
    /// @param is_default デフォルト値を使用したか (未指定または空白のみであったか)
    /// @param has_plus_sign 正の実数値に+をつけるかどうか
    /// @param has_integer 整数部があるかどうか
    /// @param has_fraction 小数部があるかどうか
    /// @param has_exponent 指数部があるかどうか
    /// @param is_single_precision 単精度かどうか
    /// @throw DataFormatError 整数部と小数部の両方がfalseの場合
    /// @note 単精度の場合はEの指数部を使用し、倍精度の場合はDの指数部を使用する
    static ValueFormat Real(bool is_default = false,
                            bool has_plus_sign = false, bool has_integer = true,
                            bool has_fraction = true, bool has_exponent = false,
                            bool is_single_precision = false) {
        if (!has_integer && !has_fraction) {
            throw DataFormatError(
                "Real type must have at least one of integer or fraction part.");
        }
        return {IGESParameterType::kReal, is_default, has_plus_sign, has_integer,
                has_fraction, has_exponent, is_single_precision};
    }
    /// @brief Pointer型のValueFormatを生成
    /// @param is_default デフォルト値を使用したか (未指定または空白のみであったか)
    static ValueFormat Pointer(bool is_default = false) {
        return {IGESParameterType::kPointer, is_default};
    }
    /// @brief String型のValueFormatを生成
    /// @param is_default デフォルト値を使用したか (未指定または空白のみであったか)
    static ValueFormat String(bool is_default = false) {
        return {IGESParameterType::kString, is_default};
    }
    /// @brief Language Statement型のValueFormatを生成
    static ValueFormat LanguageStatement() {
        return {IGESParameterType::kLanguageStatement};
    }


    /// @brief 等価演算子
    /// @param other 比較対象のValueFormat
    /// @note 各typeに関係のない値は無視する. 例えば、Logical型では
    ///       is_default以外の値は無視される
    bool operator==(const ValueFormat&) const;

    /// @brief 不等価演算子
    /// @param other 比較対象のValueFormat
    /// @note 各typeに関係のない値は無視する. 例えば、Logical型では
    ///       is_default以外の値は無視される
    bool operator!=(const ValueFormat& other) const {
        return !(*this == other);
    }
};

/// @brief CppParameterTypeをValueFormatに変換するヘルパー関数
/// @param type 変換するCppParameterType
/// @return 対応するValueFormat
/// @note 全ての値はデフォルト値を使用する
/// @throw std::invalid_argument typeがサポートされていない場合
ValueFormat DefaultValueFormat(const CppParameterType);

/// @brief データ型からValueFormatを取得するヘルパー関数
/// @tparam T 取得するデータ型
/// @return 対応するValueFormat
/// @note 全ての値はデフォルト値を使用する
/// @throw std::invalid_argument Tがサポートされていない場合
template<typename T>
ValueFormat DefaultValueFormat() {
    if constexpr (std::is_same_v<T, bool>) {
        return ValueFormat::Logical();
    } else if constexpr (std::is_same_v<T, int>) {
        return ValueFormat::Integer();
    } else if constexpr (std::is_same_v<T, double>) {
        return ValueFormat::Real();
    } else if constexpr (std::is_same_v<T, uint64_t>) {
        return ValueFormat::Pointer();
    } else if constexpr (std::is_same_v<T, std::string>) {
        return ValueFormat::String();
    } else {
        throw std::invalid_argument("Unsupported type for ValueFormat conversion.");
    }
}



/**
 * トリム関数: 文字列の前後の空白を削除する
 */

/// @brief 文字列の末尾の空白を削除する
/// @param s 文字列
/// @return 右側の空白を削除した文字列
/// @note 厳密に半角スペースのみを削除する
std::string rtrim(const std::string&);

/// @brief 文字列の先頭にある空白を削除する
/// @param s 文字列
/// @return 左側の空白を削除した文字列
/// @note 厳密に半角スペースのみを削除する
std::string ltrim(const std::string&);

/// @brief 文字列の先頭と末尾の空白を削除する
/// @param s 文字列
/// @return 左右の空白を削除した文字列
/// @note 厳密に半角スペースのみを削除する
std::string trim(const std::string&);



/**
 * IGES文字列とデータ型との変換: 以下の型との変換を行う
 *
 * 1. Integer型, 2. Real型, 3. String型,
 * 4. Pointer型, 5. Language型 (MACROエンティティのみ、Stringとおおよそ同様)
 * 6. Logical型 (bool型)
 */

/// @brief 文字列を整数に変換し、そのフォーマットも返す
/// @param str 変換する文字列
/// @param default_value 変換できなかった場合のデフォルト値,
///        std::nulloptを指定した場合は、変換失敗時に例外を投げる
/// @return 変換した整数値、およびそのフォーマット
/// @throw igesio::TypeConversionError 入力文字列が変換できず（空白のみ）、
///        default_valueがstd::nulloptの場合.
///        数値に変換できない文字列が含まれている場合
std::pair<int, ValueFormat>
FromIgesIntegerWithFormat(const std::string&, const std::optional<int> = 0);

/// @brief 文字列を実数に変換し、そのフォーマットも返す
/// @param str 変換する文字列
/// @param default_value 変換できなかった場合のデフォルト値,
///        std::nulloptを指定した場合は、変換失敗時に例外を投げる
/// @return 変換した実数値、およびそのフォーマット
/// @throw igesio::TypeConversionError 入力文字列が変換できず（空白のみ）、
///        default_valueがstd::nulloptの場合.
///        数値に変換できない文字列が含まれている場合
/// @note 'E'による指数部の表現がなければ、倍精度として解釈する
std::pair<double, ValueFormat>
FromIgesRealWithFormat(const std::string&, const std::optional<double> = 0.0);

/// @brief 文字列 (IGES, H付)を文字列に変換し、そのフォーマットも返す
/// @param str 変換する文字列
/// @param default_value 変換できなかった場合のデフォルト値,
///        std::nulloptを指定した場合は、変換失敗時に例外を投げる
/// @return 変換した文字列、およびそのフォーマット
/// @throw igesio::TypeConversionError 入力文字列が変換できず（空白のみ）、
///        default_valueがstd::nulloptの場合.
///        文字列の長さがH指定と異なる場合（例: "5Hello")
std::pair<std::string, ValueFormat>
FromIgesStringWithFormat(const std::string&, const std::optional<std::string> = "");

/// @brief 文字列をポインタに変換し、そのフォーマットも返す
/// @param str 変換する文字列
/// @param default_value 変換できなかった場合のデフォルト値,
///        std::nulloptを指定した場合は、変換失敗時に例外を投げる
/// @return 変換したポインタ値、およびそのフォーマット
/// @note 負値も許容する（一部のポインターのため）
std::pair<int, ValueFormat>
FromIgesPointerWithFormat(const std::string&, const std::optional<int> = 0);

/// @brief 文字列をLanguage型に変換し、そのフォーマットも返す
/// @param str 変換する文字列
/// @param default_value 変換できなかった場合のデフォルト値,
///        std::nulloptを指定した場合は、変換失敗時に例外を投げる
/// @return 変換したLanguage型の値、およびそのフォーマット
std::pair<std::string, ValueFormat>
FromIgesLanguageWithFormat(const std::string&, const std::optional<std::string> = "");

/// @brief 文字列をLogical型に変換し、そのフォーマットも返す
/// @param str 変換する文字列
/// @param default_value 変換できなかった場合のデフォルト値,
///        std::nulloptを指定した場合は、変換失敗時に例外を投げる
/// @return 変換した論理型の値、およびそのフォーマット
/// @throw igesio::TypeConversionError 入力文字列が変換できず、
///        default_valueがstd::nulloptの場合.
///        論理値に変換できない文字列が含まれている場合
std::pair<bool, ValueFormat>
FromIgesLogicalWithFormat(const std::string&, const std::optional<bool> = false);



/// @brief 文字列を整数に変換する
/// @param str 変換する文字列
/// @param default_value 変換できなかった場合のデフォルト値,
///        std::nulloptを指定した場合は、変換失敗時に例外を投げる
/// @return 変換した整数値
/// @throw igesio::TypeConversionError 入力文字列が変換できず（空白のみ）、
///        default_valueがstd::nulloptの場合.
///        数値に変換できない文字列が含まれている場合
int FromIgesInteger(const std::string&, const std::optional<int> = 0);

/// @brief 文字列を実数に変換する
/// @param str 変換する文字列
/// @param default_value 変換できなかった場合のデフォルト値,
///        std::nulloptを指定した場合は、変換失敗時に例外を投げる
/// @return 変換した実数値
/// @throw igesio::TypeConversionError 入力文字列が変換できず（空白のみ）、
///        default_valueがstd::nulloptの場合.
///        数値に変換できない文字列が含まれている場合
double FromIgesReal(const std::string&, const std::optional<double> = 0.0);

/// @brief 文字列(IGES, H付)を文字列に変換する
/// @param str 変換する文字列
/// @param default_value 変換できなかった場合のデフォルト値,
///        std::nulloptを指定した場合は、変換失敗時に例外を投げる
/// @return 変換した文字列
/// @throw igesio::TypeConversionError 入力文字列が変換できず（空白のみ）、
///        default_valueがstd::nulloptの場合.
///        文字列の長さがH指定と異なる場合（例: "5Hello")
/// @note "5Hhello" -> "hello", "10Habcdefghij" -> "abcdefghij".
///       ASCII文字以外を含めないこと
std::string
FromIgesString(const std::string&, const std::optional<std::string> = "");

/// @brief 文字列をポインタに変換する
/// @param str 変換する文字列
/// @param default_value 変換できなかった場合のデフォルト値,
///        std::nulloptを指定した場合は、変換失敗時に例外を投げる
/// @return 変換したポインタ値
/// @note 負値も許容する（一部のポインターのため）
/// @throw igesio::TypeConversionError 入力文字列が変換できず（空白のみ）、
///        default_valueがstd::nulloptの場合.
///        数値に変換できない文字列が含まれている場合
int FromIgesPointer(const std::string&, const std::optional<int> = 0);

/// @brief 文字列をLanguage型に変換する
/// @param str 変換する文字列
/// @param default_value 変換できなかった場合のデフォルト値,
///        std::nulloptを指定した場合は、変換失敗時に例外を投げる
/// @return 変換したLanguage型の値
/// @note 内部的には与えられた文字列をそのまま返す
std::string FromIgesLanguage(const std::string&, const std::optional<std::string> = "");

/// @brief 文字列を論理型に変換する
/// @param str 変換する文字列
/// @param default_value 変換できなかった場合のデフォルト値,
///        std::nulloptを指定した場合は、変換失敗時に例外を投げる
/// @return 変換した論理型の値
/// @throw igesio::TypeConversionError 入力文字列が変換できず、
///        default_valueがstd::nulloptの場合.
///        論理値に変換できない文字列が含まれている場合
bool FromIgesLogical(const std::string&, const std::optional<bool> = false);



/**
 * C++のデータ型からIGES文字列への変換: 以下の型との変換を行う
 *
 * 1. Integer型, 2. Real型, 3. String型,
 * 4. Pointer型, 5. Language型 (MACROエンティティのみ、Stringとおおよそ同様)
 * 6. Logical型 (bool型)
 */

/// @brief 整数値を文字列に変換する
/// @param value 変換する整数値
/// @param format 変換後のValueFormat
/// @param config 変換に使用する設定
std::string
ToIgesInteger(const int, const ValueFormat&, const SerializationConfig&);

/// @brief 実数値を文字列に変換する
/// @param value 変換する実数値
/// @param format 変換後のValueFormat
/// @param config 変換に使用する設定
std::string
ToIgesReal(const double, const ValueFormat&,
           [[maybe_unused]] const SerializationConfig&);

/// @brief 文字列をIGESのString型に変換する
/// @param value 変換する文字列
/// @param format 変換後のValueFormat
std::string
ToIgesString(const std::string&, const ValueFormat&);

/// @brief ポインタ値を文字列に変換する
/// @param value 変換するポインタ値
/// @param format 変換後のValueFormat
std::string
ToIgesPointer(const int, const ValueFormat&);

/// @brief Language型の値を文字列に変換する
/// @param value 変換する文字列
/// @param format 変換後のValueFormat
std::string
ToIgesLanguage(const std::string&, const ValueFormat&);

/// @brief 論理型の値を文字列に変換する
/// @param value 変換する論理型の値
/// @param format 変換後のValueFormat
std::string
ToIgesLogical(const bool, const ValueFormat&);

/// @brief C++のデータ型からIGES文字列への変換を行う
/// @tparam T 変換するデータ型
/// @param value 変換する値
/// @param format 変換後のValueFormat
/// @param config 変換に使用する設定
template<typename T>
std::string ToIgesValue(const T& value, const ValueFormat& format,
                        const SerializationConfig& config) {
    if constexpr (std::is_same_v<T, int>) {
        return ToIgesInteger(value, format, config);
    } else if constexpr (std::is_same_v<T, double>) {
        return ToIgesReal(value, format, config);
    } else if constexpr (std::is_same_v<T, std::string>) {
        return ToIgesString(value, format);
    } else if constexpr (std::is_same_v<T, uint64_t>) {
        return ToIgesPointer(static_cast<int>(value), format);
    } else if constexpr (std::is_same_v<T, bool>) {
        return ToIgesLogical(value, format);
    } else {
        throw std::invalid_argument("Unsupported type for IGES serialization: " +
                                    std::string(typeid(T).name()));
    }
}

}  // namespace igesio

#endif  // IGESIO_COMMON_SERIALIZATION_H_
