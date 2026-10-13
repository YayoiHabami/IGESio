/**
 * @file extensions/common/text_scanner.cpp
 * @brief テキスト形式のメッシュファイル（STL/OBJ）用の字句解析と数値の読取
 * @author Yayoi Habami
 * @date 2026-10-09
 * @copyright 2026 Yayoi Habami
 */
#include "extensions/common/text_scanner.h"

#include <charconv>
#include <cmath>
#include <limits>
#include <system_error>

#include <fast_float/fast_float.h>

namespace igesio::extensions::detail {

namespace {

/// @brief 指数部の記号か判定する
/// @param c 判定する文字
/// @return `e`または`E`の場合はtrue
bool IsExponentMark(const char c) {
    return c == 'e' || c == 'E';
}

/// @brief fast_floatで実数を読み取る
/// @tparam T 読み取る型（float/double）
/// @param first 数値の先頭の位置 (空白は読み飛ばし済みであること)
/// @param end 読み取り範囲の終端 (この位置は含まない)
/// @param[out] value 読み取った値
///             （失敗時はオーバーフローなら符号付きの最大値、それ以外は0）
/// @return 読み取れた場合は数値の直後の位置. 失敗した場合はnullptr
/// @note std::istreamの`>>`と同じ結果（成功/失敗、取得する値）になるようにする
template <typename T>
const char* ParseRealAsIstream(const char* first, const char* end, T& value) {
    // istreamは先頭の`+`を読み取れるが、fast_floatは読めないため削除する.
    // （`+`の後にさらに符号が続く場合はistreamでも失敗）
    if (first < end && *first == '+') {
        ++first;
        if (first < end && (*first == '+' || *first == '-')) {
            value = T(0);
            return nullptr;
        }
    }

    const auto result = fast_float::from_chars(first, end, value);
    if (result.ec == std::errc::invalid_argument) {
        value = T(0);
        return nullptr;
    }
    // istreamはinf/nanの綴りを数値として許可しない
    if (result.ec == std::errc() && !std::isfinite(value)) {
        value = T(0);
        return nullptr;
    }
    // istreamでは、オーバーフローは失敗し符号付きの最大値が設定される.
    // アンダーフロー（0または非正規化数）はistreamでは読み取れる
    if (result.ec == std::errc::result_out_of_range && std::isinf(value)) {
        value = std::signbit(value) ? std::numeric_limits<T>::lowest()
                                    : std::numeric_limits<T>::max();
        return nullptr;
    }
    // istreamは`e`の後の指数を読もうとし、指数が不完全な場合は失敗する
    // 読み取った部分に指数が既に含まれている場合は、次の`e`は数値の以外として残す
    if (result.ptr < end && IsExponentMark(*result.ptr)) {
        bool has_exponent = false;
        for (const char* c = first; c < result.ptr; ++c) {
            if (IsExponentMark(*c)) has_exponent = true;
        }
        if (!has_exponent) {
            value = T(0);
            return nullptr;
        }
    }

    return result.ptr;
}

}  // namespace



bool TextScanner::ReadReal(double& value) {
    SkipSpaces();
    const char* next = ParseRealAsIstream(p_, end_, value);
    if (next == nullptr) return false;

    p_ = next;
    return true;
}

bool TextScanner::ReadReal(float& value) {
    SkipSpaces();
    const char* next = ParseRealAsIstream(p_, end_, value);
    if (next == nullptr) return false;

    p_ = next;
    return true;
}

std::errc ParseInteger(std::string_view text, std::int64_t& value) {
    // std::from_charsは先頭の`+`を受け付けないため外す（"+-1"等は不正）
    if (!text.empty() && text.front() == '+') {
        text.remove_prefix(1);
        if (!text.empty() && text.front() == '-') {
            return std::errc::invalid_argument;
        }
    }
    std::int64_t parsed = 0;
    const auto result =
            std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (result.ec != std::errc()) return result.ec;
    if (result.ptr != text.data() + text.size()) {
        return std::errc::invalid_argument;
    }
    value = parsed;
    return std::errc();
}

}  // namespace igesio::extensions::detail
