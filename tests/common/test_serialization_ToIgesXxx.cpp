/**
 * @file utils/test_serialization_ToIgesXxx.cpp
 * @brief utils/serialization.hのうち、IGES文字列への変換のテスト
 * @author Yayoi Habami
 * @date 2025-05-31
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <string>
#include <tuple>
#include <vector>

#include "igesio/common/serialization.h"



namespace {

/// @brief ToIgesRealのテストケース
std::vector<std::tuple<double, bool, bool, bool, bool, std::string>> test_cases = {
    // 基本的なケース
    {1.5, false, true, true, false, "1.5"},
    {1.5, true, true, true, false, "+1.5"},
    {-1.5, false, true, true, false, "-1.5"},

    // 整数部/小数部の条件テスト
    {1.0, false, true, false, false, "1."},
    {1.0, false, true, true, false, "1.0"},
    {0.5, false, false, true, false, ".5"},
    {-0.5, false, false, true, false, "-.5"},

    // 指数表記のテスト
    {1500.0, false, true, true, true, "1.5E+3"},
    {0.0015, false, true, true, true, "1.5E-3"},
    {-1500.0, false, true, true, true, "-1.5E+3"},

    // 符号と指数の組み合わせ
    {1500.0, true, true, true, true, "+1.5E+3"},

    // 特殊なケース
    {0.0, false, true, true, false, "0.0"},
    {0.0, true, true, true, false, "+0.0"},

    // 組み合わせパターン
    {1.0, false, true, false, true, "1.E+0"},
    {0.5, false, false, true, true, "5.0E-1"},
    {1234.0, false, true, false, true, "1.234E+3"},
    // TODO: 浮動小数点出力の問題への対処; 以下のケースは、"4.5599999999999995E-3"
    // のように浮動小数点の精度の問題で、期待値と異なる場合がある.
    // {0.00456, false, false, true, true, "4.56E-3"}
};

}  // namespace

// ToIgesRealのテスト
TEST(ToIgesRealTest, ValidInput) {
    // デフォルト値なし (単精度)
    for (const auto& [value, has_plus_sign, has_integer,
                      has_fraction, has_exponent, expected] : test_cases) {
        auto real = igesio::ValueFormat::Real(
                false, has_plus_sign, has_integer, has_fraction, has_exponent, true);
        auto result = igesio::ToIgesReal(value, real, igesio::SerializationConfig());
        EXPECT_EQ(result, expected);
    }

    // デフォルト値なし (倍精度)
    auto real = igesio::ValueFormat::Real(false, false, true, true, true, false);
    auto result = igesio::ToIgesReal(1234.56, real, igesio::SerializationConfig());
    EXPECT_EQ(result, "1.23456D+3");

    // デフォルト値あり
    real = igesio::ValueFormat::Real(true);
    result = igesio::ToIgesReal(0.0, real, igesio::SerializationConfig());
    EXPECT_EQ(result, "");  // 0.0であればデフォルト値""として出力される
    result = igesio::ToIgesReal(1.0, real, igesio::SerializationConfig());
    EXPECT_EQ(result, "1.0");  // デフォルト値でない場合は変換される
}

// ToIgesStringのテスト
TEST(ToIgesStringTest, ValidInput) {
    EXPECT_EQ(igesio::ToIgesString("123", igesio::ValueFormat::String()),
              "3H123");
    EXPECT_EQ(igesio::ToIgesString("0.457E03", igesio::ValueFormat::String()),
              "8H0.457E03");
    EXPECT_EQ(igesio::ToIgesString("ABC ., ; ABCD", igesio::ValueFormat::String()),
              "13HABC ., ; ABCD");
    EXPECT_EQ(igesio::ToIgesString(" HELLO THERE", igesio::ValueFormat::String()),
              "12H HELLO THERE");
    EXPECT_EQ(igesio::ToIgesString(" ", igesio::ValueFormat::String()),
              "1H ");
    EXPECT_EQ(igesio::ToIgesString("", igesio::ValueFormat::String()),
              "0H");
}

TEST(ToIgesStringTest, DefaultValue) {
    // デフォルト設定でも、値がデフォルトでない場合は変換される
    EXPECT_EQ(igesio::ToIgesString("123", igesio::ValueFormat::String(true)),
              "3H123");
    // 空文字列の場合は変換されない
    EXPECT_EQ(igesio::ToIgesString("", igesio::ValueFormat::String(true)),
              "");
}

// ToIgesPointerのテスト
TEST(ToIgesPointerTest, ValidInput) {
    EXPECT_EQ(igesio::ToIgesPointer(0, igesio::ValueFormat::Pointer()), "0");
    EXPECT_EQ(igesio::ToIgesPointer(1, igesio::ValueFormat::Pointer()), "1");
    EXPECT_EQ(igesio::ToIgesPointer(123, igesio::ValueFormat::Pointer()), "123");
    EXPECT_EQ(igesio::ToIgesPointer(-456, igesio::ValueFormat::Pointer()), "-456");
}

TEST(ToIgesPointerTest, DefaultValue) {
    // 0以外が指定されている場合、デフォルト設定でも変換される
    EXPECT_EQ(igesio::ToIgesPointer(0, igesio::ValueFormat::Pointer(true)),
              "");
    EXPECT_EQ(igesio::ToIgesPointer(123, igesio::ValueFormat::Pointer(true)),
              "123");
    EXPECT_EQ(igesio::ToIgesPointer(-456, igesio::ValueFormat::Pointer(true)),
              "-456");
}

// ToIgesLanguageのテスト
TEST(ToIgesLanguageTest, ValidInput) {
    EXPECT_EQ(igesio::ToIgesLanguage("WHILE", igesio::ValueFormat::LanguageStatement()),
              "WHILE");
    EXPECT_EQ(igesio::ToIgesLanguage("EXECUTE", igesio::ValueFormat::LanguageStatement()),
              "EXECUTE");
    EXPECT_EQ(igesio::ToIgesLanguage("", igesio::ValueFormat::LanguageStatement()),
              "");
    EXPECT_EQ(igesio::ToIgesLanguage("123ABC", igesio::ValueFormat::LanguageStatement()),
              "123ABC");
}

// ToIgesLogicalのテスト
TEST(ToIgesLogicalTest, ValidInput) {
    EXPECT_EQ(igesio::ToIgesLogical(true, igesio::ValueFormat::Logical()), "1");
    EXPECT_EQ(igesio::ToIgesLogical(false, igesio::ValueFormat::Logical()), "0");
}

TEST(ToIgesLogicalTest, DefaultValue) {
    // trueの場合は常に"1"として出力される
    EXPECT_EQ(igesio::ToIgesLogical(true, igesio::ValueFormat::Logical(true)),
              "1");
    EXPECT_EQ(igesio::ToIgesLogical(false, igesio::ValueFormat::Logical(true)),
              "");
}

