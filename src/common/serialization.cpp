/**
 * @file common/serialization.cpp
 * @brief IGESデータ文字列からC++のデータ型への変換や、その逆の操作を定義する
 * @author Yayoi Habami
 * @date 2025-05-30
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/common/serialization.h"

#include <cmath>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

#include "igesio/common/errors.h"



namespace {

/// @brief 文字列が空文字列/スペースのみの文字列ではないかを確認する
/// @param str 変換する文字列
/// @param is_default_set デフォルト値が設定されているかどうか
/// @return 変換可能な（空文字列/スペースのみの文字列ではない）場合はtrue
/// @throw iio::TypeConversionError 空文字列/スペースのみの文字列であり、
///        かつデフォルト値が設定されていない場合
bool AssertStrConvertibility(const std::string& str, const bool is_default_set) {
    // 空文字列・スペースのみの文字列でない場合は問題なし
    if (!(str.empty() || igesio::ltrim(str).empty())) return true;
    // デフォルト値が設定されている場合は問題なし
    if (is_default_set) return false;

    // デフォルト値が設定されていない場合はエラー
    throw igesio::TypeConversionError(
            "A value must always be provided for any field"
            " that is required and has no default value");
}

/// @brief 文字が0-9の数字であるかを確認する
/// @param c 文字
/// @return 0-9の数字である場合はtrue
bool IsDigit(char c) {
    return c >= '0' && c <= '9';
}

/// @brief 文字が0-9または'-'/'+'であるかを確認する
/// @param c 文字
/// @return 0-9または'-'/'+'である場合はtrue
/// @note '-'/'+'は符号を表すため、整数値の変換時に必要
bool IsDigitOrSign(char c) {
    return IsDigit(c) || c == '-' || c == '+';
}

/// @brief Dによる指数表記をEによる指数表記に変換する
/// @param str 変換する文字列
/// @return 変換後の文字列d
/// @note 例: "1.23D+4" -> "1.23E+4"
/// @note 互換性のため. 新たに記述する場合はEを使用すること
std::string ConvertDToE(const std::string& str) {
    std::string result = str;

    size_t pos = result.find('D');
    if (pos != std::string::npos) {
        result.replace(pos, 1, "E");
    }
    return result;
}

/// @brief 数値が単精度か否か
/// @param str Real型の文字列
/// @return 文字列内に 'E' が含まれている場合は単精度と判断
bool IsSinglePrecision(const std::string& str) {
    return str.find('E') != std::string::npos;
}

/// @brief 数値に整数部があるか否か
/// @param str Real型の文字列
/// @return 文字列の先頭 (または先頭が符号の場合はその次の文字)
///         が数字である場合は整数部があると判断
bool HasIntegerPart(const std::string& str) {
    if (str.empty()) return false;
    size_t start = (str[0] == '+' || str[0] == '-') ? 1 : 0;
    return start < str.size() && IsDigit(str[start]);
}

/// @brief 数値に小数部があるか否か
/// @param str Real型の文字列
/// @return 文字列内の '.' の次の文字が数字である場合は小数部があると判断
bool HasFractionPart(const std::string& str) {
    size_t dot_pos = str.find('.');
    if (dot_pos == std::string::npos) return false;
    // '.' の次の文字が数字であるかを確認
    return dot_pos + 1 < str.size() && IsDigit(str[dot_pos + 1]);
}

/// @brief 数値に指数部があるか否か
/// @param str Real型の文字列 (指数部は 'E' または 'D' で始まる)
/// @return 文字列内に 'E' または 'D' が含まれている場合は指数部があると判断
bool HasExponentPart(const std::string& str) {
    return str.find('E') != std::string::npos ||
           str.find('D') != std::string::npos;
}

}  // namespace



bool igesio::ValueFormat::operator==(const ValueFormat& other) const {
    if (type != other.type) return false;

    switch (type) {
        case IGESParameterType::kLanguageStatement:
            return true;
        case IGESParameterType::kLogical:
        case IGESParameterType::kPointer:
        case IGESParameterType::kString:
            // Logical, Pointer, Stringはデフォルト値の有無のみが差異
            return is_default == other.is_default;
        case IGESParameterType::kInteger:
            // Integerはデフォルト値と符号の有無が差異
            return is_default == other.is_default &&
                   has_plus_sign == other.has_plus_sign;
        case IGESParameterType::kReal:
            // Realでは全ての値を比較する
            return is_default == other.is_default &&
                   has_plus_sign == other.has_plus_sign &&
                   has_integer == other.has_integer &&
                   has_fraction == other.has_fraction &&
                   has_exponent == other.has_exponent &&
                   is_single_precision == other.is_single_precision;
        default:
            return false;  // 未知の型は等価ではない
    }
}



igesio::ValueFormat igesio::DefaultValueFormat(const CppParameterType type) {
    switch (type) {
        case CppParameterType::kBool:
            return ValueFormat::Logical();
        case CppParameterType::kInt:
            return ValueFormat::Integer();
        case CppParameterType::kDouble:
            return ValueFormat::Real();
        case CppParameterType::kPointer:
            return ValueFormat::Pointer();
        case CppParameterType::kString:
            return ValueFormat::String();
        default:
            throw std::invalid_argument("Unsupported CppParameterType for ValueFormat conversion.");
    }
}


/**
 * トリム関数
 */

std::string igesio::rtrim(const std::string& s) {
    size_t end = s.find_last_not_of(" ");
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

std::string igesio::ltrim(const std::string& s) {
    size_t start = s.find_first_not_of(" ");
    return (start == std::string::npos) ? "" : s.substr(start);
}

std::string igesio::trim(const std::string& s) {
    return rtrim(ltrim(s));
}



/**
 * IGES文字列とデータ型との変換: 以下の型との変換を行う
 */

std::pair<int, igesio::ValueFormat> igesio::FromIgesIntegerWithFormat(
        const std::string& str,
        const std::optional<int> default_value) {
    // 文字列が空文字列/スペースのみの文字列ではないかを確認
    if (!AssertStrConvertibility(str, default_value.has_value())) {
        // 空文字列/スペースのみの文字列の場合、デフォルト値を返す
        // Assert...内でデフォルト値検証済
        return {default_value.value(), ValueFormat::Integer(true)};
    }

    // 前後から空白を削除して整数に変換
    auto trimmed = igesio::trim(str);
    if (!IsDigitOrSign(trimmed[0])) {
        // 先頭が数字でない場合 (\t等の::stripで削除されない空白文字がある場合)
        throw igesio::TypeConversionError(
                "Invalid integer value: '" + str + "'"
                " The string must begin with a digit or sign character");
    }

    try {
        std::size_t pos = 0;
        int value = std::stoi(trimmed, &pos);

        // 変換して残った文字列が空でない場合はエラー
        if (pos != trimmed.length()) {
            throw igesio::TypeConversionError(
                    "Invalid integer value: '" + str + "'");
        }
        return {value, ValueFormat::Integer(false, trimmed[0] == '+')};
    } catch (const std::invalid_argument&) {
        // 変換できなかった場合はエラー
        throw igesio::TypeConversionError(
                "Invalid integer value: '" + str + "'");
    } catch (const std::out_of_range&) {
        // 範囲外の場合はエラー
        throw igesio::TypeConversionError(
                "Integer value out of range: '" + str + "'");
    }
}

std::pair<double, igesio::ValueFormat> igesio::FromIgesRealWithFormat(
        const std::string& str,
        const std::optional<double> default_value) {
    // 文字列が空文字列/スペースのみの文字列ではないかを確認
    if (!AssertStrConvertibility(str, default_value.has_value())) {
        // 空文字列/スペースのみの文字列の場合、デフォルト値を返す
        // Assert...内でデフォルト値検証済
        return {default_value.value(), ValueFormat::Real(true)};
    }

    // 前後から空白を削除して実数に変換
    auto trimmed = igesio::trim(str);
    if (!IsDigitOrSign(trimmed[0])) {
        // 先頭が数字でない場合 (\t等の::stripで削除されない空白文字がある場合)
        throw igesio::TypeConversionError(
                "Invalid double value: '" + str + "'"
                " The string must begin with a digit or sign character");
    }

    try {
        // 単精度か否かを判断し、互換性のためにDをEに変換
        auto is_single_precision = IsSinglePrecision(trimmed);
        std::size_t pos = 0;
        double value = std::stod(ConvertDToE(trimmed), &pos);

        // その他のフォーマット情報を取得
        bool has_plus_sign = trimmed[0] == '+';
        bool has_integer = HasIntegerPart(trimmed);
        bool has_fraction = HasFractionPart(trimmed);
        bool has_exponent = HasExponentPart(trimmed);

        // 変換して残った文字列が空でない場合はエラー
        if (pos != trimmed.length()) {
            throw igesio::TypeConversionError(
                    "Invalid real value: '" + str + "'");
        }

        // アンダーフローの明示的なチェック
        // Windows&Clangでは、std::stodが2.225...e-309のような値を与えられても
        // 0.0を返すことがあるため、明示的にチェックする
        if (std::isfinite(value) && value != 0.0) {
            if (std::abs(value) < std::numeric_limits<double>::min()) {
                throw igesio::TypeConversionError(
                        "Real value underflow: '" + str + "'");
            }
        }

        return {value, ValueFormat::Real(
                false, has_plus_sign, has_integer, has_fraction,
                has_exponent, is_single_precision)};
    } catch (const std::invalid_argument&) {
        // 変換できなかった場合はエラー
        throw igesio::TypeConversionError(
                "Invalid real value: '" + str + "'");
    } catch (const std::out_of_range&) {
        // 範囲外の場合はエラー
        throw igesio::TypeConversionError(
                "Real value out of range: '" + str + "'");
    }
}

std::pair<std::string, igesio::ValueFormat> igesio::FromIgesStringWithFormat(
        const std::string& str,
        const std::optional<std::string> default_value) {
    // 文字列が空文字列/スペースのみの文字列ではないかを確認
    if (!AssertStrConvertibility(str, default_value.has_value())) {
        // 空文字列/スペースのみの文字列の場合、デフォルト値を返す
        // Assert...内でデフォルト値検証済
        return {default_value.value(), ValueFormat::String(true)};
    }

    // エラー文
    std::string error_msg = "Invalid string format: '" + str + "'"
            " The string must begin with the number of characters,"
            " followed by 'H' and the string itself (e.g., '5Hhello').";

    // Hの位置を取得
    size_t pos = str.find('H');
    if (pos == std::string::npos) {
        // Hが見つからない場合はフォーマットエラー
        throw igesio::TypeConversionError(error_msg);
    }
    // 冒頭の数値を取得
    int num;
    try {
        // 頭に+がつくことを許容しない
        if (str[0] == '+') throw std::invalid_argument("Invalid number format");
        // かつ、冒頭が数字であることを確認
        if (!IsDigitOrSign(str[0])) throw std::invalid_argument("Invalid number format");

        num = std::stoi(str.substr(0, pos));
    } catch (const std::invalid_argument&) {
        // 数値に変換できなかった場合はフォーマットエラー
        throw igesio::TypeConversionError(error_msg);
    } catch (const std::out_of_range&) {
        // 範囲外の場合はエラー
        throw igesio::TypeConversionError("String length out of range: '" + str + "'");
    }

    // Hの位置からの文字列長さが指定された長さと異なる場合はエラー
    if (str.substr(pos + 1).length() != static_cast<size_t>(num)) {
        throw igesio::TypeConversionError(
            error_msg + " Length mismatch: expected " +
            std::to_string(num) + " characters, but got " +
            std::to_string(str.substr(pos + 1).length()) + " characters.");
    }

    // Hの位置からの文字列を取得して返す
    return {str.substr(pos + 1), ValueFormat::String()};
}

std::pair<int, igesio::ValueFormat> igesio::FromIgesPointerWithFormat(
        const std::string& str,
        const std::optional<int> default_value) {
    try {
        auto [value, format] = FromIgesIntegerWithFormat(str, default_value);
        return {value, ValueFormat::Pointer(format.is_default)};
    } catch (const igesio::TypeConversionError&) {
        // 変換できなかった場合はエラー
        throw igesio::TypeConversionError(
                "Invalid pointer value: '" + str + "'");
    } catch (const std::out_of_range&) {
        // 範囲外の場合はエラー
        throw igesio::TypeConversionError(
                "Pointer value out of range: '" + str + "'");
    }
}

std::pair<std::string, igesio::ValueFormat> igesio::FromIgesLanguageWithFormat(
        const std::string& str,
        const std::optional<std::string> default_value) {
    // Language statementは、テキストの前に文字数とホスレス区切り文字(H)を含めない
    // 特にフォーマットはないため、デフォルトのValueFormatを使用する
    return {str, ValueFormat::LanguageStatement()};
}

std::pair<bool, igesio::ValueFormat> igesio::FromIgesLogicalWithFormat(
        const std::string& str,
        const std::optional<bool> default_value) {
    // トリム後文字列が'TRUE','1'に一致する場合はtrueを返す
    // 'FALSE','0'に一致する場合はfalseを返し、それ以外はエラー
    // NOTE: Logical型の表現が'TRUE'/'FALSE'なのか'1'/'0'なのかは不明だが、
    //   Type186, Type406-Form29, Type508-510, Type514のパラメータにしか出てこないようで、
    //   さらに https://people.math.sc.edu/Burkardt/data/iges/iges.html のサンプルを見る限り
    //   '1'/'0'のようである. 念のため、'TRUE'/'FALSE'からの変換も許容するようにしておく.
    auto trimmed = igesio::trim(str);
    if (trimmed == "TRUE" || trimmed == "1")
        return {true, ValueFormat::Logical()};
    if (trimmed == "FALSE" || trimmed == "0")
        return {false, ValueFormat::Logical()};

    // デフォルト値が設定されていて、入力が空文字列/スペースのみの場合はデフォルト値を返す
    if (default_value.has_value() && trimmed.empty())
            return {default_value.value(), ValueFormat::Logical(true)};

    // デフォルト値が設定されていない場合はエラー
    throw igesio::TypeConversionError(
            "Invalid logical value: '" + str + "' The string must be '1'/'0'");
}

int igesio::FromIgesInteger(
        const std::string& str,
        const std::optional<int> default_value) {
    return FromIgesIntegerWithFormat(str, default_value).first;
}

double igesio::FromIgesReal(
        const std::string& str,
        const std::optional<double> default_value) {
    return FromIgesRealWithFormat(str, default_value).first;
}

std::string igesio::FromIgesString(
        const std::string& str,
        const std::optional<std::string> default_value) {
    return FromIgesStringWithFormat(str, default_value).first;
}

int igesio::FromIgesPointer(
        const std::string& str,
        const std::optional<int> default_value) {
    return FromIgesPointerWithFormat(str, default_value).first;
}

std::string igesio::FromIgesLanguage(
        const std::string& str,
        const std::optional<std::string> default_value) {
    return FromIgesLanguageWithFormat(str, default_value).first;
}

bool igesio::FromIgesLogical(
        const std::string& str,
        const std::optional<bool> default_value) {
    return FromIgesLogicalWithFormat(str, default_value).first;
}



/**
 * C++のデータ型からIGES文字列への変換
 */

namespace {

/// @brief 初めて0以外の数字が出現する位置を取得する
/// @param value 浮動小数点数値
/// @note `123.456` -> 3、`0.00123` -> -3、`0.0` -> 0
int GetFirstNonZeroDigitPosition(double value) {
    if (value == 0.0) return 0;  // 0の場合は0を返す

    auto digits = static_cast<int>(std::floor(std::log10(std::abs(value))));
    if (std::abs(value) > 1.0) {
        return digits + 1;
    } else {
        // 1未満の場合はそのままの桁数を返す
        return digits;
    }
}

}  // namespace

std::string igesio::ToIgesInteger(
        const int value, const ValueFormat& format, const SerializationConfig&) {
    if (format.is_default && value == 0) {
        // デフォルト値の場合は空文字列を返す
        return "";
    }

    auto result = std::to_string(value);
    if (format.has_plus_sign && value >= 0) {
        // 正の整数値に+をつける
        result = "+" + result;
    }
    return result;
}

std::string igesio::ToIgesReal(
        const double value, const ValueFormat& format,
        [[maybe_unused]] const SerializationConfig& config) {
    if (format.is_default && value == 0.0) {
        // デフォルト値の場合は空文字列を返す
        return "";
    }

    std::stringstream ss;
    double abs_value = std::abs(value);

    // 符号の処理
    if (value < 0) {
        ss << '-';
    } else if (format.has_plus_sign) {
        ss << '+';
    }

    // 指数表記の処理 (0以外の数値を指数表記する場合は、仮数部を[1, 10)の範囲に正規化)
    int exponent = 0;
    if (format.has_exponent && abs_value != 0.0) {
        exponent = static_cast<int>(std::floor(std::log10(abs_value)));
        // 数値を正規化（例: 1234 -> 1.234）
        abs_value /= std::pow(10, exponent);
    }

    // 数値を整数部と小数部に分解
    double integer_part;
    double fraction_part = std::modf(abs_value, &integer_part);

    // 整数部の処理
    if (format.has_integer || format.has_exponent || integer_part >= 1.0 ||
            (integer_part == 0.0 && fraction_part == 0.0)) {
        ss << static_cast<int>(integer_part);
    }
    ss << '.';  // 小数点を追加

    // 小数部の処理
    if ((format.has_fraction && fraction_part > 0.0) ||
        (format.has_exponent && fraction_part > 0.0)) {
        // 小数部を文字列に変換（10進数に変換して末尾の0を削除）
        int decimal_digits = std::numeric_limits<double>::digits10;
        double scaled = std::round(fraction_part * std::pow(10, decimal_digits));

        // 末尾の0を削除
        while (scaled > 0 && std::fmod(scaled, 10.0) == 0.0) {
            scaled /= 10.0;
            decimal_digits--;
        }

        // 整数として文字列化、必要に応じて先頭に0を追加
        auto fraction_str = std::to_string(static_cast<int64_t>(scaled));
        while (fraction_str.length() < decimal_digits) {
            fraction_str = "0" + fraction_str;
        }
        ss << fraction_str;
    } else if (format.has_fraction) {
        // 小数部を表示するが、小数部がない場合
        ss << "0";
    }

    // 指数部の処理
    if (format.has_exponent) {
        // 指数部を文字列に変換
        ss << (format.is_single_precision ? 'E' : 'D');
        if (exponent >= 0) {
            ss << '+';  // 正の指数には+をつける
        }
        ss << exponent;
    }

    // TODO: configの設定に応じて、精度や指数部の桁数を調整する
    return ss.str();
}

std::string igesio::ToIgesString(
        const std::string& value, const ValueFormat& format) {
    if (format.is_default && value.empty()) {
        // デフォルト値の場合は空文字列を返す
        return "";
    }
    // H付き文字列に変換
    return std::to_string(value.length()) + "H" + value;
}

std::string igesio::ToIgesPointer(
        const int value, const ValueFormat& format) {
    if (format.is_default && value == 0) {
        // デフォルト値の場合は空文字列を返す
        return "";
    }
    // ポインタ値を文字列に変換
    return std::to_string(value);
}

std::string igesio::ToIgesLanguage(
        const std::string& value, const ValueFormat&) {
    // Language statementはそのまま文字列を返す
    return value;
}

std::string igesio::ToIgesLogical(
        const bool value, const ValueFormat& format) {
    if (format.is_default && !value) {
        // デフォルト値の場合は空文字列を返す
        return "";
    }
    // 論理値を文字列に変換
    return value ? "1" : "0";
}
