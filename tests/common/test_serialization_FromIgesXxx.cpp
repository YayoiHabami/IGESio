/**
 * @file utils/test_serialization_FromIgesXxx.cpp
 * @brief utils/serialization.hのうち、IGES文字列からの変換のテスト
 * @author Yayoi Habami
 * @date 2025-04-12
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <climits>
#include <string>
#include <optional>

#include "igesio/common/errors.h"
#include "igesio/common/serialization.h"



/******************************************************************************
 * FromIgesIntegerWithFormat関数のテスト
 *
 * 値の正しさの検証はFromIgesInteger関数のテストで行う
 *****************************************************************************/

// 有効な整数値のテスト
TEST(FromIgesIntegerWithFormat, ValidInput) {
    EXPECT_EQ(igesio::FromIgesIntegerWithFormat("42").second,
              igesio::ValueFormat::Integer(false, false));
    EXPECT_EQ(igesio::FromIgesIntegerWithFormat("  42  ").second,
              igesio::ValueFormat::Integer(false, false));
    EXPECT_EQ(igesio::FromIgesIntegerWithFormat("+42").second,
              igesio::ValueFormat::Integer(false, true));
    EXPECT_EQ(igesio::FromIgesIntegerWithFormat("-42").second,
              igesio::ValueFormat::Integer(false, false));
    EXPECT_EQ(igesio::FromIgesIntegerWithFormat("0").second,
              igesio::ValueFormat::Integer(false, false));

    // デフォルト値あり
    EXPECT_EQ(igesio::FromIgesIntegerWithFormat("42", 0).second,
              igesio::ValueFormat::Integer(false, false));
    EXPECT_EQ(igesio::FromIgesIntegerWithFormat("", 0).second,
              igesio::ValueFormat::Integer(true, false));
    EXPECT_EQ(igesio::FromIgesIntegerWithFormat("   ", 0).second,
              igesio::ValueFormat::Integer(true, false));
}

// 無効な入力のテスト
TEST(FromIgesIntegerWithFormat, InvalidInput) {
    EXPECT_THROW(igesio::FromIgesIntegerWithFormat("abc"), igesio::TypeConversionError);
    EXPECT_THROW(igesio::FromIgesIntegerWithFormat("42abc"), igesio::TypeConversionError);
    EXPECT_THROW(igesio::FromIgesIntegerWithFormat("abc42"), igesio::TypeConversionError);
    EXPECT_THROW(igesio::FromIgesIntegerWithFormat("42.5"), igesio::TypeConversionError);

    EXPECT_THROW(igesio::FromIgesIntegerWithFormat("4+2"), igesio::TypeConversionError);
    EXPECT_THROW(igesio::FromIgesIntegerWithFormat("4-2"), igesio::TypeConversionError);
    EXPECT_THROW(igesio::FromIgesIntegerWithFormat("+-42"), igesio::TypeConversionError);
    EXPECT_THROW(igesio::FromIgesIntegerWithFormat("42#"), igesio::TypeConversionError);
    EXPECT_THROW(igesio::FromIgesIntegerWithFormat("$42"), igesio::TypeConversionError);

    // 空文字列/スペースのみの文字列であり、デフォルト値が設定されていない場合
    EXPECT_THROW(igesio::FromIgesIntegerWithFormat("", std::nullopt),
            igesio::TypeConversionError);
    EXPECT_THROW(igesio::FromIgesIntegerWithFormat("   ", std::nullopt),
            igesio::TypeConversionError);
}



/******************************************************************************
 * FromIgesRealWithFormat関数のテスト
 *
 * 値の正しさの検証はFromIgesReal関数のテストで行う
 *****************************************************************************/

// 有効な実数値のテスト
TEST(FromIgesRealWithFormat, ValidInput) {
    // 基本的な実数値
    EXPECT_EQ(igesio::FromIgesRealWithFormat("256.091").second,
              igesio::ValueFormat::Real(false, false, true, true, false, false));
    EXPECT_EQ(igesio::FromIgesRealWithFormat("0.").second,
              igesio::ValueFormat::Real(false, false, true, false, false, false));
    EXPECT_EQ(igesio::FromIgesRealWithFormat("-0.58").second,
              igesio::ValueFormat::Real(false, false, true, true, false, false));
    EXPECT_EQ(igesio::FromIgesRealWithFormat("+4.21").second,
              igesio::ValueFormat::Real(false, true, true, true, false, false));

    // E指数表記
    EXPECT_EQ(igesio::FromIgesRealWithFormat("1.36E1").second,
              igesio::ValueFormat::Real(false, false, true, true, true, true));
    EXPECT_EQ(igesio::FromIgesRealWithFormat("-1.3E-02").second,
              igesio::ValueFormat::Real(false, false, true, true, true, true));
    EXPECT_EQ(igesio::FromIgesRealWithFormat("0.1E-3").second,
              igesio::ValueFormat::Real(false, false, true, true, true, true));
    EXPECT_EQ(igesio::FromIgesRealWithFormat("1.E+4").second,
              igesio::ValueFormat::Real(false, false, true, false, true, true));
    EXPECT_EQ(igesio::FromIgesRealWithFormat("-.43E2").second,
              igesio::ValueFormat::Real(false, false, false, true, true, true));

    // D指数表記 (倍精度)
    EXPECT_EQ(igesio::FromIgesRealWithFormat("145.98763D4").second,
              igesio::ValueFormat::Real(false, false, true, true, true, false));
    EXPECT_EQ(igesio::FromIgesRealWithFormat("-2145.980001D-5").second,
              igesio::ValueFormat::Real(false, false, true, true, true, false));
    EXPECT_EQ(igesio::FromIgesRealWithFormat("0.123456789D+09").second,
              igesio::ValueFormat::Real(false, false, true, true, true, false));

    // デフォルト値あり
    EXPECT_EQ(igesio::FromIgesRealWithFormat("256.091", 0.0).second,
              igesio::ValueFormat::Real(false, false, true, true, false, false));
    EXPECT_EQ(igesio::FromIgesRealWithFormat("", 0.0).second,
              igesio::ValueFormat::Real(true, false, true, true, false, false));
    EXPECT_EQ(igesio::FromIgesRealWithFormat("   ", 0.0).second,
              igesio::ValueFormat::Real(true, false, true, true, false, false));
}

// 無効な入力のテスト
TEST(FromIgesRealWithFormat, InvalidInput) {
    EXPECT_THROW(igesio::FromIgesRealWithFormat("abc"), igesio::TypeConversionError);
    EXPECT_THROW(igesio::FromIgesRealWithFormat("42.5abc"), igesio::TypeConversionError);
    EXPECT_THROW(igesio::FromIgesRealWithFormat("abc42.5"), igesio::TypeConversionError);
    EXPECT_THROW(igesio::FromIgesRealWithFormat("42..5"), igesio::TypeConversionError);

    EXPECT_THROW(igesio::FromIgesRealWithFormat("4+2.5"), igesio::TypeConversionError);
    EXPECT_THROW(igesio::FromIgesRealWithFormat("4-2.5"), igesio::TypeConversionError);
    EXPECT_THROW(igesio::FromIgesRealWithFormat("+-42.5"), igesio::TypeConversionError);
    EXPECT_THROW(igesio::FromIgesRealWithFormat("42.5#"), igesio::TypeConversionError);
    EXPECT_THROW(igesio::FromIgesRealWithFormat("$42.5"), igesio::TypeConversionError);

    // 不正な指数表記
    EXPECT_THROW(igesio::FromIgesRealWithFormat("1.36EE1"), igesio::TypeConversionError);
    EXPECT_THROW(igesio::FromIgesRealWithFormat("1.36E"), igesio::TypeConversionError);
    EXPECT_THROW(igesio::FromIgesRealWithFormat("1.36E+"), igesio::TypeConversionError);
    EXPECT_THROW(igesio::FromIgesRealWithFormat("E10"), igesio::TypeConversionError);

    // 空文字列/スペースのみの文字列であり、デフォルト値が設定されていない場合
    EXPECT_THROW(igesio::FromIgesRealWithFormat("", std::nullopt),
            igesio::TypeConversionError);
    EXPECT_THROW(igesio::FromIgesRealWithFormat("   ", std::nullopt),
            igesio::TypeConversionError);
}



/******************************************************************************
 * FromIgesStringWithFormat関数のテスト
 ***************************************************************************/

// 有効な文字列値のテスト
TEST(FromIgesStringWithFormat, ValidInput) {
    auto wo_default = igesio::ValueFormat::String(false);
    auto with_default = igesio::ValueFormat::String(true);
    EXPECT_EQ(igesio::FromIgesStringWithFormat("3H123").second, wo_default);
    EXPECT_EQ(igesio::FromIgesStringWithFormat("8H0.457E03").second, wo_default);
    EXPECT_EQ(igesio::FromIgesStringWithFormat("13HABC ., ; ABCD").second, wo_default);
    EXPECT_EQ(igesio::FromIgesStringWithFormat("12H HELLO THERE").second, wo_default);
    EXPECT_EQ(igesio::FromIgesStringWithFormat("1H ").second, wo_default);
    EXPECT_EQ(igesio::FromIgesStringWithFormat("0H").second, wo_default);

    // デフォルト値あり
    EXPECT_EQ(igesio::FromIgesStringWithFormat("3H123", "default").second, wo_default);
    EXPECT_EQ(igesio::FromIgesStringWithFormat("", "default").second, with_default);
    EXPECT_EQ(igesio::FromIgesStringWithFormat("   ", "default").second, with_default);
}

// 無効な入力のテスト
TEST(FromIgesStringWithFormat, InvalidInput) {
    EXPECT_THROW(igesio::FromIgesStringWithFormat("3HABCD"), igesio::TypeConversionError);
    EXPECT_THROW(igesio::FromIgesStringWithFormat("5HAB"), igesio::TypeConversionError);
    EXPECT_THROW(igesio::FromIgesStringWithFormat("0HABC"), igesio::TypeConversionError);
    EXPECT_THROW(igesio::FromIgesStringWithFormat("3ABC"), igesio::TypeConversionError);
    EXPECT_THROW(igesio::FromIgesStringWithFormat("10ABC"), igesio::TypeConversionError);
    EXPECT_THROW(igesio::FromIgesStringWithFormat("AHABC"), igesio::TypeConversionError);
    EXPECT_THROW(igesio::FromIgesStringWithFormat("?HABC"), igesio::TypeConversionError);
    EXPECT_THROW(igesio::FromIgesStringWithFormat("-3HABC"), igesio::TypeConversionError);
    EXPECT_THROW(igesio::FromIgesStringWithFormat("+3HABC"), igesio::TypeConversionError);

    // 空文字列/スペースのみの文字列であり、デフォルト値が設定されていない場合
    EXPECT_THROW(igesio::FromIgesStringWithFormat("", std::nullopt),
            igesio::TypeConversionError);
    EXPECT_THROW(igesio::FromIgesStringWithFormat("   ", std::nullopt),
            igesio::TypeConversionError);
}



/******************************************************************************
 * FromIgesPointerWithFormat関数のテスト
 *
 * 値の正しさの検証はFromIgesPointer関数のテストで行う
 *****************************************************************************/

// 有効なポインタ値のテスト
TEST(FromIgesPointerWithFormat, ValidInput) {
    EXPECT_EQ(igesio::FromIgesPointerWithFormat("42").second,
              igesio::ValueFormat::Pointer(false));
    EXPECT_EQ(igesio::FromIgesPointerWithFormat("  -123  ").second,
              igesio::ValueFormat::Pointer(false));
    EXPECT_EQ(igesio::FromIgesPointerWithFormat("+3451").second,
              igesio::ValueFormat::Pointer(false));
    EXPECT_EQ(igesio::FromIgesPointerWithFormat("0").second,
              igesio::ValueFormat::Pointer(false));

    // デフォルト値あり
    EXPECT_EQ(igesio::FromIgesPointerWithFormat("42", 0).second,
              igesio::ValueFormat::Pointer(false));
    EXPECT_EQ(igesio::FromIgesPointerWithFormat("", 0).second,
              igesio::ValueFormat::Pointer(true));
    EXPECT_EQ(igesio::FromIgesPointerWithFormat("   ", 0).second,
              igesio::ValueFormat::Pointer(true));
}



/******************************************************************************
 * FromIgesLanguageWithFormat関数のテスト
 *****************************************************************************/

// 有効な言語値のテスト
TEST(FromIgesLanguageWithFormat, ValidInput) {
    EXPECT_EQ(igesio::FromIgesLanguageWithFormat("Hello").second,
              igesio::ValueFormat::LanguageStatement());
    EXPECT_EQ(igesio::FromIgesLanguageWithFormat("こんにちは").second,
              igesio::ValueFormat::LanguageStatement());
    EXPECT_EQ(igesio::FromIgesLanguageWithFormat("12345").second,
              igesio::ValueFormat::LanguageStatement());
    EXPECT_EQ(igesio::FromIgesLanguageWithFormat("!@#$%^&*()").second,
              igesio::ValueFormat::LanguageStatement());
}



/******************************************************************************
 * FromIgesLogicalWithFormat関数のテスト
 *
 * 値の正しさの検証はFromIgesLogical関数のテストで行う
 *****************************************************************************/

// 有効な論理値のテスト
TEST(FromIgesLogicalWithFormat, ValidInput) {
    EXPECT_EQ(igesio::FromIgesLogicalWithFormat("TRUE").second,
              igesio::ValueFormat::Logical(false));
    EXPECT_EQ(igesio::FromIgesLogicalWithFormat("FALSE").second,
              igesio::ValueFormat::Logical(false));
    EXPECT_EQ(igesio::FromIgesLogicalWithFormat("1").second,
              igesio::ValueFormat::Logical(false));
    EXPECT_EQ(igesio::FromIgesLogicalWithFormat("0").second,
              igesio::ValueFormat::Logical(false));

    // デフォルト値あり
    EXPECT_EQ(igesio::FromIgesLogicalWithFormat("TRUE", true).second,
              igesio::ValueFormat::Logical(false));
    EXPECT_EQ(igesio::FromIgesLogicalWithFormat("", true).second,
              igesio::ValueFormat::Logical(true));
    EXPECT_EQ(igesio::FromIgesLogicalWithFormat("   ", false).second,
              igesio::ValueFormat::Logical(true));
}



/******************************************************************************
 * FromIgesInteger関数のテスト
 *****************************************************************************/

// FromIgesInteger関数のテストフィクスチャ
class FromIgesIntegerTest : public ::testing::Test {
 protected:
    // FromIgesIntegerがTypeConversionErrorを投げるかチェックするヘルパー関数
    bool throwsConversionError(const std::string& input,
                               const std::optional<int> default_value = std::nullopt) {
        try {
            igesio::FromIgesInteger(input, default_value);
            return false;
        } catch (const igesio::TypeConversionError&) {
            return true;
        } catch (...) {
            // 異なる例外型が投げられた
            return false;
        }
    }
};

// 有効な入力のテスト
TEST_F(FromIgesIntegerTest, ValidInput) {
    EXPECT_EQ(igesio::FromIgesInteger("1"), 1);
    EXPECT_EQ(igesio::FromIgesInteger("150"), 150);
    EXPECT_EQ(igesio::FromIgesInteger("2147483647"), 2147483647);
    EXPECT_EQ(igesio::FromIgesInteger("+3451"), 3451);
    EXPECT_EQ(igesio::FromIgesInteger("0"), 0);
    EXPECT_EQ(igesio::FromIgesInteger("-10"), -10);
    EXPECT_EQ(igesio::FromIgesInteger("-2147483647"), -2147483647);
}

// 空白を含む有効な入力のテスト
TEST_F(FromIgesIntegerTest, ValidInputWithWhitespace) {
    EXPECT_EQ(igesio::FromIgesInteger(" 42"), 42);
    EXPECT_EQ(igesio::FromIgesInteger("42 "), 42);
    EXPECT_EQ(igesio::FromIgesInteger("  42  "), 42);
    EXPECT_EQ(igesio::FromIgesInteger("   -123   "), -123);
}

// 無効な入力のテスト
TEST_F(FromIgesIntegerTest, InvalidInput) {
    // デフォルト値が提供されている場合
    EXPECT_TRUE(throwsConversionError("abc", 99));
    EXPECT_TRUE(throwsConversionError("42abc", 99));
    EXPECT_TRUE(throwsConversionError("abc42", 99));
    EXPECT_TRUE(throwsConversionError("42.5", 99));
    EXPECT_EQ(igesio::FromIgesInteger("", 99), 99);
    EXPECT_EQ(igesio::FromIgesInteger("   ", 99), 99);

    // 数字の前後に無効な文字が含まれる場合
    EXPECT_TRUE(throwsConversionError("4+2", 99));
    EXPECT_TRUE(throwsConversionError("4-2", 99));
    EXPECT_TRUE(throwsConversionError("+-42", 99));
}

// デフォルト値なしで例外を投げるテスト
TEST_F(FromIgesIntegerTest, ThrowsOnInvalidInputWithoutDefault) {
    EXPECT_TRUE(throwsConversionError("abc"));
    EXPECT_TRUE(throwsConversionError("42abc"));
    EXPECT_TRUE(throwsConversionError("abc42"));
    EXPECT_TRUE(throwsConversionError("42.5"));
    EXPECT_TRUE(throwsConversionError(""));
    EXPECT_TRUE(throwsConversionError("   "));

    // 半角スペース以外の空白文字を含む場合
    EXPECT_TRUE(throwsConversionError("42\t"));
    EXPECT_TRUE(throwsConversionError("\t42"));
    EXPECT_TRUE(throwsConversionError(" 42\n"));
    EXPECT_TRUE(throwsConversionError("\n42 "));
    EXPECT_TRUE(throwsConversionError("\t42\t"));
    EXPECT_TRUE(throwsConversionError("\n+50\n"));
    EXPECT_TRUE(throwsConversionError("\n-50\n"));
    EXPECT_TRUE(throwsConversionError("\n+50\t"));
}

// エッジケース
TEST_F(FromIgesIntegerTest, EdgeCases) {
    // 明示的にnulloptを提供
    EXPECT_EQ(igesio::FromIgesInteger("42", std::nullopt), 42);
    EXPECT_TRUE(throwsConversionError("abc", std::nullopt));

    // 境界値
    EXPECT_EQ(igesio::FromIgesInteger("2147483647"), INT_MAX);   // 最大整数値
    EXPECT_EQ(igesio::FromIgesInteger("-2147483648"), INT_MIN);  // 最小整数値

    // オーバーフロー条件 - デフォルト値を使用するか例外を投げると仮定
    EXPECT_TRUE(throwsConversionError("2147483648"));   // INT_MAX + 1
    EXPECT_TRUE(throwsConversionError("-2147483649"));  // INT_MIN - 1
    // デフォルト値を使用してもエラー
    EXPECT_TRUE(throwsConversionError("2147483648", 99));
    EXPECT_TRUE(throwsConversionError("-2147483649", 99));
}

// 特殊文字の処理
TEST_F(FromIgesIntegerTest, SpecialCharacters) {
    EXPECT_TRUE(throwsConversionError("42#"));
    EXPECT_TRUE(throwsConversionError("$42"));
    EXPECT_TRUE(throwsConversionError("42€"));
    EXPECT_TRUE(throwsConversionError("４２"));  // 全角数字
}



/******************************************************************************
 * FromIgesReal関数のテスト
 *****************************************************************************/

// FromIgesReal関数のテストフィクスチャ
class FromIgesRealTest : public ::testing::Test {
 protected:
    // FromIgesRealがTypeConversionErrorを投げるかチェックするヘルパー関数
    bool throwsConversionError(const std::string& input,
                               const std::optional<double> default_value = std::nullopt) {
        try {
            igesio::FromIgesReal(input, default_value);
            return false;
        } catch (const igesio::TypeConversionError&) {
            return true;
        } catch (...) {
            // 異なる例外型が投げられた
            return false;
        }
    }
};

// 有効な入力のテスト
TEST_F(FromIgesRealTest, ValidInput) {
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal("1"), 1.0);
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal("150"), 150.0);
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal("0"), 0.0);
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal("-10"), -10.0);
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal("256.091"), 256.091);
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal("0."), 0.0);
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal("-0.58"), -0.58);
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal("+4.21"), 4.21);
}

// 指数表記のテスト
TEST_F(FromIgesRealTest, ExponentialNotation) {
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal("1.36E1"), 13.6);
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal("-1.3E-02"), -0.013);
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal("0.1E-3"), 0.0001);
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal("1.E+4"), 10000.0);
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal("145.98763D4"), 1459876.3);
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal("-2145.980001D-5"), -0.02145980001);
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal("0.123456789D+09"), 123456789.0);
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal("-.43E2"), -43.0);
}

// 空白を含む有効な入力のテスト
TEST_F(FromIgesRealTest, ValidInputWithWhitespace) {
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal(" 42.5"), 42.5);
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal("42.5 "), 42.5);
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal("  42.5  "), 42.5);
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal("   -123.456   "), -123.456);
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal(" 1.36E1 "), 13.6);
}

// 無効な入力のテスト
TEST_F(FromIgesRealTest, InvalidInput) {
    // デフォルト値が提供されている場合
    EXPECT_TRUE(throwsConversionError("abc", 99.9));
    EXPECT_TRUE(throwsConversionError("42.5abc", 99.9));
    EXPECT_TRUE(throwsConversionError("abc42.5", 99.9));
    EXPECT_EQ(igesio::FromIgesReal("", 99.9), 99.9);
    EXPECT_EQ(igesio::FromIgesReal("   ", 99.9), 99.9);

    // 数字の前後に無効な文字が含まれる場合
    EXPECT_TRUE(throwsConversionError("4+2.5", 99.9));
    EXPECT_TRUE(throwsConversionError("4-2.5", 99.9));
    EXPECT_TRUE(throwsConversionError("+-42.5", 99.9));
    EXPECT_TRUE(throwsConversionError("42..5", 99.9));
}

// デフォルト値なしで例外を投げるテスト
TEST_F(FromIgesRealTest, ThrowsOnInvalidInputWithoutDefault) {
    EXPECT_TRUE(throwsConversionError("abc"));
    EXPECT_TRUE(throwsConversionError("42.5abc"));
    EXPECT_TRUE(throwsConversionError("abc42.5"));
    EXPECT_TRUE(throwsConversionError(""));
    EXPECT_TRUE(throwsConversionError("   "));

    // 半角スペース以外の空白文字を含む場合
    EXPECT_TRUE(throwsConversionError("42.5\t"));
    EXPECT_TRUE(throwsConversionError("\t42.5"));
    EXPECT_TRUE(throwsConversionError(" 42.5\n"));
    EXPECT_TRUE(throwsConversionError("\n42.5 "));
    EXPECT_TRUE(throwsConversionError("\t42.5\t"));
    EXPECT_TRUE(throwsConversionError("\n+50.5\n"));
    EXPECT_TRUE(throwsConversionError("\n-50.5\n"));
}

// 特殊なフォーマットのテスト
TEST_F(FromIgesRealTest, SpecialFormats) {
    // 複数の小数点
    EXPECT_TRUE(throwsConversionError("42.5.3"));

    // 不正な指数表記
    EXPECT_TRUE(throwsConversionError("1.36EE1"));
    EXPECT_TRUE(throwsConversionError("1.36E"));
    EXPECT_TRUE(throwsConversionError("1.36E+"));
    EXPECT_TRUE(throwsConversionError("1.36E-"));
    EXPECT_TRUE(throwsConversionError("E10"));

    // 混在した記法
    EXPECT_TRUE(throwsConversionError("1.36E1D2"));
    EXPECT_TRUE(throwsConversionError("1.36D1E2"));
}

// エッジケース
TEST_F(FromIgesRealTest, EdgeCases) {
    // 明示的にnulloptを提供
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal("42.5", std::nullopt), 42.5);
    EXPECT_TRUE(throwsConversionError("abc", std::nullopt));

    // 境界値 (DBL_MAXとDBL_MIN付近)
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal("1.7976931348623157E+308"), DBL_MAX);
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal("2.2250738585072014E-308"), DBL_MIN);

    // オーバーフロー/アンダーフロー
    EXPECT_TRUE(throwsConversionError("1.7976931348623159E+308"));  // DBL_MAX超過
    EXPECT_TRUE(throwsConversionError("2.2250738585072013E-309"));  // DBL_MIN未満
    // デフォルト値を使用してもエラー
    EXPECT_TRUE(throwsConversionError("1.7976931348623159E+308", 99.9));
    EXPECT_TRUE(throwsConversionError("2.2250738585072013E-309", 99.9));
}

// 特殊文字の処理
TEST_F(FromIgesRealTest, SpecialCharacters) {
    EXPECT_TRUE(throwsConversionError("42.5#"));
    EXPECT_TRUE(throwsConversionError("$42.5"));
    EXPECT_TRUE(throwsConversionError("42.5€"));
    EXPECT_TRUE(throwsConversionError("４２.５"));  // 全角数字
}

// IGESの標準的な表記法テスト
TEST_F(FromIgesRealTest, IgesStandardNotation) {
    // DとEの両方の指数表記をサポート
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal("1.36E1"), 13.6);
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal("1.36D1"), 13.6);

    // 指数部の符号
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal("1.36E+1"), 13.6);
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal("1.36D+1"), 13.6);
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal("1.36E-1"), 0.136);
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal("1.36D-1"), 0.136);

    // 小数点なしの指数表記
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal("136E-1"), 13.6);
    EXPECT_DOUBLE_EQ(igesio::FromIgesReal("136D-1"), 13.6);
}



/******************************************************************************
 * FromIgesString関数のテスト
 *****************************************************************************/

// FromIgesString関数のテストフィクスチャ
class FromIgesStringTest : public ::testing::Test {
 protected:
    // FromIgesStringがTypeConversionErrorを投げるかチェックするヘルパー関数
    bool throwsConversionError(const std::string& input,
                              const std::optional<std::string> default_value = std::nullopt) {
        try {
            igesio::FromIgesString(input, default_value);
            return false;
        } catch (const igesio::TypeConversionError&) {
            return true;
        } catch (...) {
            // 異なる例外型が投げられた
            return false;
        }
    }
};

// 有効な入力のテスト
TEST_F(FromIgesStringTest, ValidInput) {
    EXPECT_EQ(igesio::FromIgesString("3H123"), "123");
    EXPECT_EQ(igesio::FromIgesString("8H0.457E03"), "0.457E03");
    EXPECT_EQ(igesio::FromIgesString("13HABC ., ; ABCD"), "ABC ., ; ABCD");
    EXPECT_EQ(igesio::FromIgesString("12H HELLO THERE"), " HELLO THERE");
    EXPECT_EQ(igesio::FromIgesString("1H "), " ");
    EXPECT_EQ(igesio::FromIgesString("0H"), "");
}

// 無効な入力のテスト
TEST_F(FromIgesStringTest, InvalidInput) {
    // 長さ指定と実際の長さが合わない場合
    EXPECT_TRUE(throwsConversionError("3HABCD"));
    EXPECT_TRUE(throwsConversionError("5HAB"));
    EXPECT_TRUE(throwsConversionError("0HABC"));

    // 'H'がない場合
    EXPECT_TRUE(throwsConversionError("3ABC"));
    EXPECT_TRUE(throwsConversionError("10ABC"));

    // 長さ指定が数値でない場合
    EXPECT_TRUE(throwsConversionError("AHABC"));
    EXPECT_TRUE(throwsConversionError("?HABC"));

    // 空白入力
    EXPECT_TRUE(throwsConversionError(""));
    EXPECT_TRUE(throwsConversionError("   "));

    // 長さ指定が不正な値
    EXPECT_TRUE(throwsConversionError("-3HABC"));
    EXPECT_TRUE(throwsConversionError("+3HABC"));
}

// デフォルト値を持つ無効な入力のテスト
TEST_F(FromIgesStringTest, InvalidInputWithDefault) {
    // デフォルト値が提供されている場合
    // 空文字列や空白のみの文字列は許容される
    EXPECT_EQ(igesio::FromIgesString("", "default"), "default");
    EXPECT_EQ(igesio::FromIgesString("   ", "default"), "default");

    // デフォルト値が提供されている場合でも、空/空白以外は許容しない
    EXPECT_TRUE(throwsConversionError("3HABCD", "default"));
    EXPECT_TRUE(throwsConversionError("5HAB", "default"));
    EXPECT_TRUE(throwsConversionError("3ABC", "default"));
}

// 空白の扱いに関するテスト
TEST_F(FromIgesStringTest, WhitespaceHandling) {
    // 前後の空白は許容しないため例外が投げられる
    EXPECT_TRUE(throwsConversionError(" 3HABC"));
    EXPECT_TRUE(throwsConversionError("3HABC "));
    EXPECT_TRUE(throwsConversionError(" 3HABC "));

    // タブや改行などの空白文字も同様
    EXPECT_TRUE(throwsConversionError("\t3HABC"));
    EXPECT_TRUE(throwsConversionError("3HABC\n"));
    EXPECT_TRUE(throwsConversionError("\r3HABC"));
}

// エッジケース
TEST_F(FromIgesStringTest, EdgeCases) {
    // 明示的にnulloptを提供
    EXPECT_EQ(igesio::FromIgesString("3HABC", std::nullopt), "ABC");
    EXPECT_TRUE(throwsConversionError("3HABCD", std::nullopt));

    // 空文字列を表す有効なケース
    EXPECT_EQ(igesio::FromIgesString("0H"), "");

    // 非常に長い文字列
    std::string longInput = "1000H" + std::string(1000, 'A');
    EXPECT_EQ(igesio::FromIgesString(longInput), std::string(1000, 'A'));

    // 異なる長さの数値指定
    EXPECT_EQ(igesio::FromIgesString("1H1"), "1");
    EXPECT_EQ(igesio::FromIgesString("10HABCDEFGHIJ"), "ABCDEFGHIJ");
    EXPECT_EQ(igesio::FromIgesString("100H" + std::string(100, 'X')), std::string(100, 'X'));
}

// 特殊文字を含む文字列のテスト
TEST_F(FromIgesStringTest, SpecialCharacters) {
    EXPECT_EQ(igesio::FromIgesString("5H!@#$%"), "!@#$%");
    EXPECT_EQ(igesio::FromIgesString("5H\\\"\'\t\n"), "\\\"\'\t\n");
    EXPECT_EQ(igesio::FromIgesString("4H    "), "    ");  // すべて空白
    EXPECT_EQ(igesio::FromIgesString("8H12345678"), "12345678");  // 数字のみ
}

// 複雑なケースのテスト
TEST_F(FromIgesStringTest, ComplexCases) {
    // H自体が文字列の一部として含まれる
    EXPECT_EQ(igesio::FromIgesString("5HABCDH"), "ABCDH");
    EXPECT_EQ(igesio::FromIgesString("7HHHHHHHH"), "HHHHHHH");

    // 数字+Hが文字列の一部として含まれる
    EXPECT_EQ(igesio::FromIgesString("8H12H34H56"), "12H34H56");

    // 長さ指定が複数桁の場合
    EXPECT_EQ(igesio::FromIgesString("12HABCDEFGHIJKL"), "ABCDEFGHIJKL");
    EXPECT_EQ(igesio::FromIgesString("123H" + std::string(123, 'Z')), std::string(123, 'Z'));
}



/******************************************************************************
 * FromIgesPointer関数のテスト
 *****************************************************************************/

// FromIgesPointer関数のテストフィクスチャ
class FromIgesPointerTest : public ::testing::Test {
 protected:
    // FromIgesPointerがTypeConversionErrorを投げるかチェックするヘルパー関数
    bool throwsConversionError(const std::string& input,
                               const std::optional<int> default_value = std::nullopt) {
        try {
            igesio::FromIgesPointer(input, default_value);
            return false;
        } catch (const igesio::TypeConversionError&) {
            return true;
        } catch (...) {
            // 異なる例外型が投げられた
            return false;
        }
    }
};

// 有効な入力のテスト
TEST_F(FromIgesPointerTest, ValidInput) {
    EXPECT_EQ(igesio::FromIgesPointer("1"), 1);
    EXPECT_EQ(igesio::FromIgesPointer("150"), 150);
    EXPECT_EQ(igesio::FromIgesPointer("2147483647"), 2147483647);
    EXPECT_EQ(igesio::FromIgesPointer("+3451"), 3451);
    EXPECT_EQ(igesio::FromIgesPointer("0"), 0);
    EXPECT_EQ(igesio::FromIgesPointer("-10"), -10);
    EXPECT_EQ(igesio::FromIgesPointer("-2147483647"), -2147483647);
}

// 空白を含む有効な入力のテスト
TEST_F(FromIgesPointerTest, ValidInputWithWhitespace) {
    EXPECT_EQ(igesio::FromIgesPointer(" 42"), 42);
    EXPECT_EQ(igesio::FromIgesPointer("42 "), 42);
    EXPECT_EQ(igesio::FromIgesPointer("  42  "), 42);
    EXPECT_EQ(igesio::FromIgesPointer("   -123   "), -123);
}

// 無効な入力のテスト
TEST_F(FromIgesPointerTest, InvalidInput) {
    // デフォルト値が提供されている場合
    EXPECT_TRUE(throwsConversionError("abc", 99));
    EXPECT_TRUE(throwsConversionError("42abc", 99));
    EXPECT_TRUE(throwsConversionError("abc42", 99));
    EXPECT_TRUE(throwsConversionError("42.5", 99));
    EXPECT_EQ(igesio::FromIgesPointer("", 99), 99);
    EXPECT_EQ(igesio::FromIgesPointer("   ", 99), 99);

    // 数字の前後に無効な文字が含まれる場合
    EXPECT_TRUE(throwsConversionError("4+2", 99));
    EXPECT_TRUE(throwsConversionError("4-2", 99));
    EXPECT_TRUE(throwsConversionError("+-42", 99));
}

// デフォルト値なしで例外を投げるテスト
TEST_F(FromIgesPointerTest, ThrowsOnInvalidInputWithoutDefault) {
    EXPECT_TRUE(throwsConversionError("abc"));
    EXPECT_TRUE(throwsConversionError("42abc"));
    EXPECT_TRUE(throwsConversionError("abc42"));
    EXPECT_TRUE(throwsConversionError("42.5"));
    EXPECT_TRUE(throwsConversionError(""));
    EXPECT_TRUE(throwsConversionError("   "));

    // 半角スペース以外の空白文字を含む場合
    EXPECT_TRUE(throwsConversionError("42\t"));
    EXPECT_TRUE(throwsConversionError("\t42"));
    EXPECT_TRUE(throwsConversionError(" 42\n"));
    EXPECT_TRUE(throwsConversionError("\n42 "));
    EXPECT_TRUE(throwsConversionError("\t42\t"));
    EXPECT_TRUE(throwsConversionError("\n+50\n"));
    EXPECT_TRUE(throwsConversionError("\n-50\n"));
    EXPECT_TRUE(throwsConversionError("\n+50\t"));
}

// エッジケース
TEST_F(FromIgesPointerTest, EdgeCases) {
    // 明示的にnulloptを提供
    EXPECT_EQ(igesio::FromIgesPointer("42", std::nullopt), 42);
    EXPECT_TRUE(throwsConversionError("abc", std::nullopt));

    // 境界値
    EXPECT_EQ(igesio::FromIgesPointer("2147483647"), INT_MAX);   // 最大整数値
    EXPECT_EQ(igesio::FromIgesPointer("-2147483648"), INT_MIN);  // 最小整数値

    // オーバーフロー条件 - デフォルト値を使用するか例外を投げると仮定
    EXPECT_TRUE(throwsConversionError("2147483648"));   // INT_MAX + 1
    EXPECT_TRUE(throwsConversionError("-2147483649"));  // INT_MIN - 1
    // デフォルト値を使用してもエラー
    EXPECT_TRUE(throwsConversionError("2147483648", 99));
    EXPECT_TRUE(throwsConversionError("-2147483649", 99));
}

// 特殊文字の処理
TEST_F(FromIgesPointerTest, SpecialCharacters) {
    EXPECT_TRUE(throwsConversionError("42#"));
    EXPECT_TRUE(throwsConversionError("$42"));
    EXPECT_TRUE(throwsConversionError("42€"));
    EXPECT_TRUE(throwsConversionError("４２"));  // 全角数字
}



/******************************************************************************
 * FromIgesLanguage関数のテスト
 *****************************************************************************/

// FromIgesLanguage関数のテスト
TEST(FromIgesLanguageTest, ValidInput) {
    // とにかくどんな文字列であっても、入れたまま返す
    EXPECT_EQ(igesio::FromIgesLanguage("Hello"), "Hello");
    EXPECT_EQ(igesio::FromIgesLanguage("こんにちは"), "こんにちは");
    EXPECT_EQ(igesio::FromIgesLanguage("12345"), "12345");
    EXPECT_EQ(igesio::FromIgesLanguage("!@#$%^&*()"), "!@#$%^&*()");
}



/******************************************************************************
 * FromIgesLogical関数のテスト
 *****************************************************************************/

// FromIgesLogical関数のテストフィクスチャ
class FromIgesLogicalTest : public ::testing::Test {
 protected:
    // FromIgesLogicalがTypeConversionErrorを投げるかチェックするヘルパー関数
    bool throwsConversionError(const std::string& input,
                               const std::optional<bool> default_value = std::nullopt) {
        try {
            igesio::FromIgesLogical(input, default_value);
            return false;
        } catch (const igesio::TypeConversionError&) {
            return true;
        } catch (...) {
            // 異なる例外型が投げられた
            return false;
        }
    }
};

// 有効な入力のテスト（TRUE, FALSE）
TEST_F(FromIgesLogicalTest, ValidTrueFalseInput) {
    EXPECT_TRUE(igesio::FromIgesLogical("TRUE"));
    EXPECT_FALSE(igesio::FromIgesLogical("FALSE"));
    // 大文字小文字混合は無効
    EXPECT_TRUE(throwsConversionError("True"));
    EXPECT_TRUE(throwsConversionError("False"));
    EXPECT_TRUE(throwsConversionError("true"));
    EXPECT_TRUE(throwsConversionError("false"));
}

// 有効な入力のテスト（0, 1）
TEST_F(FromIgesLogicalTest, ValidNumericInput) {
    EXPECT_FALSE(igesio::FromIgesLogical("0"));
    EXPECT_TRUE(igesio::FromIgesLogical("1"));
    // 他の数値は無効
    EXPECT_TRUE(throwsConversionError("2"));
    EXPECT_TRUE(throwsConversionError("-1"));
}

// 空白を含む有効な入力のテスト
TEST_F(FromIgesLogicalTest, ValidInputWithWhitespace) {
    EXPECT_TRUE(igesio::FromIgesLogical(" TRUE"));
    EXPECT_TRUE(igesio::FromIgesLogical("TRUE "));
    EXPECT_TRUE(igesio::FromIgesLogical("  TRUE  "));

    EXPECT_FALSE(igesio::FromIgesLogical(" FALSE"));
    EXPECT_FALSE(igesio::FromIgesLogical("FALSE "));
    EXPECT_FALSE(igesio::FromIgesLogical("  FALSE  "));

    EXPECT_TRUE(igesio::FromIgesLogical(" 1"));
    EXPECT_TRUE(igesio::FromIgesLogical("1 "));
    EXPECT_TRUE(igesio::FromIgesLogical("  1  "));

    EXPECT_FALSE(igesio::FromIgesLogical(" 0"));
    EXPECT_FALSE(igesio::FromIgesLogical("0 "));
    EXPECT_FALSE(igesio::FromIgesLogical("  0  "));
}

// 無効な入力のテスト
TEST_F(FromIgesLogicalTest, InvalidInput) {
    // デフォルト値が提供されている場合
    EXPECT_TRUE(throwsConversionError("abc", true));
    EXPECT_TRUE(throwsConversionError("YES", false));
    EXPECT_TRUE(throwsConversionError("NO", true));
    EXPECT_TRUE(throwsConversionError("01", false));
    EXPECT_TRUE(throwsConversionError("10", true));
    EXPECT_TRUE(throwsConversionError("T", false));
    EXPECT_TRUE(throwsConversionError("F", true));

    // 空文字列や空白のみの場合はデフォルト値を返す
    EXPECT_TRUE(igesio::FromIgesLogical("", true));
    EXPECT_FALSE(igesio::FromIgesLogical("", false));
    EXPECT_TRUE(igesio::FromIgesLogical("   ", true));
    EXPECT_FALSE(igesio::FromIgesLogical("   ", false));
}

// デフォルト値なしで例外を投げるテスト
TEST_F(FromIgesLogicalTest, ThrowsOnInvalidInputWithoutDefault) {
    EXPECT_TRUE(throwsConversionError("abc"));
    EXPECT_TRUE(throwsConversionError("YES"));
    EXPECT_TRUE(throwsConversionError("NO"));
    EXPECT_TRUE(throwsConversionError(""));
    EXPECT_TRUE(throwsConversionError("   "));
    EXPECT_TRUE(throwsConversionError("TRU"));
    EXPECT_TRUE(throwsConversionError("FALS"));
    EXPECT_TRUE(throwsConversionError("2"));

    // 半角スペース以外の空白文字を含む場合
    EXPECT_TRUE(throwsConversionError("TRUE\t"));
    EXPECT_TRUE(throwsConversionError("\tTRUE"));
    EXPECT_TRUE(throwsConversionError(" TRUE\n"));
    EXPECT_TRUE(throwsConversionError("\nTRUE "));
    EXPECT_TRUE(throwsConversionError("\t1\t"));
    EXPECT_TRUE(throwsConversionError("\n0\n"));
}

// エッジケース
TEST_F(FromIgesLogicalTest, EdgeCases) {
    // 明示的にnulloptを提供
    EXPECT_TRUE(igesio::FromIgesLogical("TRUE", std::nullopt));
    EXPECT_FALSE(igesio::FromIgesLogical("FALSE", std::nullopt));
    EXPECT_TRUE(throwsConversionError("abc", std::nullopt));

    // 大文字小文字の混在、余分な文字
    EXPECT_TRUE(throwsConversionError("TRUE1"));
    EXPECT_TRUE(throwsConversionError("1TRUE"));
    EXPECT_TRUE(throwsConversionError("TRUEE"));
    EXPECT_TRUE(throwsConversionError("FFALSE"));
}

// 特殊文字の処理
TEST_F(FromIgesLogicalTest, SpecialCharacters) {
    EXPECT_TRUE(throwsConversionError("TRUE#"));
    EXPECT_TRUE(throwsConversionError("$TRUE"));
    EXPECT_TRUE(throwsConversionError("FALSE€"));
    EXPECT_TRUE(throwsConversionError("１"));  // 全角数字
    EXPECT_TRUE(throwsConversionError("０"));  // 全角数字
}

