/**
 * @file utils/test_iges_string_utils_global_section.cpp
 * @brief utils/iges_string_utils.hのうち、グローバルセクションの読み込み
 *        に関するテスト
 * @author Yayoi Habami
 * @date 2025-04-20
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <string>
#include <vector>
#include <optional>

#include "igesio/utils/iges_string_utils.h"

namespace {

namespace i_util = igesio::utils;

}  // namespace



/******************************************************************************
 * 文字が区切り文字として使用可能かの検証 (IsValidDelimiter) のテスト
 *****************************************************************************/

TEST(IsValidDelimiter, InvalidCharacters) {
    // 制御文字 (0x00-0x1F, 0x7F)
    for (int i = 0; i <= 0x1F; ++i) {
        EXPECT_FALSE(i_util::IsValidDelimiter(static_cast<char>(i)))
                << "Control character 0x" << std::hex << i << " should be invalid";
    }
    EXPECT_FALSE(i_util::IsValidDelimiter(static_cast<char>(0x7F)))
            << "DEL character (0x7F) should be invalid";

    // 半角スペース (0x20)
    EXPECT_FALSE(i_util::IsValidDelimiter(' '))
            << "Space character should be invalid";

    // 半角数字 (0x30-0x39)
    for (char c = '0'; c <= '9'; ++c) {
        EXPECT_FALSE(i_util::IsValidDelimiter(c))
                << "Digit character '" << c << "' should be invalid";
    }

    // +, -, . (0x2B, 0x2D, 0x2E)
    EXPECT_FALSE(i_util::IsValidDelimiter('+')) << "+ should be invalid";
    EXPECT_FALSE(i_util::IsValidDelimiter('-')) << "- should be invalid";
    EXPECT_FALSE(i_util::IsValidDelimiter('.')) << ". should be invalid";

    // D, E, H (0x44, 0x45, 0x48)
    EXPECT_FALSE(i_util::IsValidDelimiter('D')) << "D should be invalid";
    EXPECT_FALSE(i_util::IsValidDelimiter('E')) << "E should be invalid";
    EXPECT_FALSE(i_util::IsValidDelimiter('H')) << "H should be invalid";
}

TEST(IsValidDelimiter, ValidCharacters) {
    // デフォルトの区切り文字のテスト
    EXPECT_TRUE(i_util::IsValidDelimiter(',')) << ", should be valid";
    EXPECT_TRUE(i_util::IsValidDelimiter(';')) << "; should be valid";

    // その他いくつかの文字のテスト
    EXPECT_TRUE(i_util::IsValidDelimiter('A')) << "A should be valid";
    EXPECT_TRUE(i_util::IsValidDelimiter('N')) << "N should be valid";
    EXPECT_TRUE(i_util::IsValidDelimiter(':')) << ": should be valid";
    EXPECT_TRUE(i_util::IsValidDelimiter('/')) << "/ should be valid";
    EXPECT_TRUE(i_util::IsValidDelimiter('\\')) << "\\ should be valid";
    EXPECT_TRUE(i_util::IsValidDelimiter('|')) << "| should be valid";
    EXPECT_TRUE(i_util::IsValidDelimiter('=')) << "= should be valid";
    EXPECT_TRUE(i_util::IsValidDelimiter('*')) << "* should be valid";
    EXPECT_TRUE(i_util::IsValidDelimiter('@')) << "@ should be valid";
    EXPECT_TRUE(i_util::IsValidDelimiter('#')) << "# should be valid";
    EXPECT_TRUE(i_util::IsValidDelimiter('$')) << "$ should be valid";
    EXPECT_TRUE(i_util::IsValidDelimiter('%')) << "% should be valid";
    EXPECT_TRUE(i_util::IsValidDelimiter('&')) << "& should be valid";
    EXPECT_TRUE(i_util::IsValidDelimiter('?')) << "? should be valid";
}



/******************************************************************************
 * パラメータ区切り文字の取得 (GetParameterDelimiter) のテスト
 *****************************************************************************/

namespace {

// 区切り文字の定義; αとβは任意のASCII文字を表す
//   ただし例外の文字は除く

/// @brief 有効 (',,') -> param: ',', record: ';'
constexpr std::string_view kUnspecUnspec = ",,  ";
/// @brief 有効 ('1Hαα1Hβα') -> param: ',', record: ';'
constexpr std::string_view kCommaSemicolon = "1H,,1H;,";
/// @brief 有効 ('1Hαα1Hβα') -> param: 'x', record: ';'
constexpr std::string_view kXSemicolon = "1Hxx1H;x";
/// @brief 有効 ('1Hαα1Hβα') -> param: ',', record: '%'
constexpr std::string_view kCommaPercent = "1H,,1H%,";
/// @brief 有効 ('1Hααα') -> param: ',', record: ';'
constexpr std::string_view kCommaUnspec = "1H,,,";
/// @brief 有効 ('1Hααα') -> param: '?', record: ';'
constexpr std::string_view kQuestionUnspec = "1H???";
/// @brief 有効 (',1Hβ,) -> param: ',', record: ';'
constexpr std::string_view kUnspecSemicolon = ",1H;,";
/// @brief 有効 (',1Hβ,) -> param: ',', record: '$'
constexpr std::string_view kUnspecDollar = ",1H$,";

// 無効パターン1: 区切り文字が許容されない文字の場合
/// @brief 無効 ('1Hαα1Hβα') -> param: ' '(無効), record: ';'
constexpr std::string_view kInvalidSpaceSemicolon = "1H  1H; ";
/// @brief 無効 ('1Hαα1Hβα') -> param: '&', record: '.'(無効)
constexpr std::string_view kInvalidAmpersandDot = "1H&&1H.&";
/// @brief 無効 ('1Hααα') -> param: ' '(無効), record: ';'
constexpr std::string_view kInvalidSpaceUnspec = "1H  ";
/// @brief 無効 (',1Hβ,) -> param: ',', record: '0'(無効)
constexpr std::string_view kInvalidUnspecZero = ",1H0,";

// 無効パターン2: 区切り文字がIGES文字列の形式として正しくない場合
// -> 1Hで1文字以外を指定するなど
/// @brief 無効 ('1Hαα1Hβα') -> param: ''(無効), record: ';'
constexpr std::string_view kInvalidEmptySemicolon = "1H1H;";
/// @brief 無効 ('1Hαα1Hβα') -> param: "xyz"(無効), record: ';'
constexpr std::string_view kInvalidXYZSemicolon = "3Hxyzxyz1H;xyz";
/// @brief 無効 ('1Hαα1Hβα') -> param: '*', record: ''(無効)
constexpr std::string_view kInvalidAsteriskEmpty = "1H**1H*";
/// @brief 無効 ('1Hαα1Hβα') -> param: ',', record: '%;'(無効)
constexpr std::string_view kInvalidCommaPercentSemicolon = "1H,,1H%;,";
/// @brief 無効 ('1Hαα1Hβα') -> param: '/', record: ';;'(無効)
constexpr std::string_view kInvalidSlashDoubleSemicolon = "1H//1H;;/";
/// @brief 無効 (',1Hβ,) -> param: ',', record: ''(無効)
constexpr std::string_view kInvalidUnspecEmpty = ",1H,";

/// @brief string_viewからstringに変換する
std::string SV2S(const std::string_view& sv) {
    return std::string(sv.data(), sv.size());
}

}  // namespace

TEST(GetParameterDelimiter, ValidDelimiters) {
    // 有効な区切り文字のテスト
    EXPECT_EQ(i_util::GetParameterDelimiter(SV2S(kUnspecUnspec)), ',')
            << "Expected ',' for " << kUnspecUnspec;
    EXPECT_EQ(i_util::GetParameterDelimiter(SV2S(kCommaSemicolon)), ',')
            << "Expected ',' for " << kCommaSemicolon;
    EXPECT_EQ(i_util::GetParameterDelimiter(SV2S(kXSemicolon)), 'x')
            << "Expected 'x' for " << kXSemicolon;
    EXPECT_EQ(i_util::GetParameterDelimiter(SV2S(kCommaPercent)), ',')
            << "Expected ',' for " << kCommaPercent;
    EXPECT_EQ(i_util::GetParameterDelimiter(SV2S(kCommaUnspec)), ',')
            << "Expected ',' for " << kCommaUnspec;
    EXPECT_EQ(i_util::GetParameterDelimiter(SV2S(kQuestionUnspec)), '?')
            << "Expected '?' for " << kQuestionUnspec;
    EXPECT_EQ(i_util::GetParameterDelimiter(SV2S(kUnspecSemicolon)), ',')
            << "Expected ',' for " << kUnspecSemicolon;
    EXPECT_EQ(i_util::GetParameterDelimiter(SV2S(kUnspecDollar)), ',')
            << "Expected ',' for " << kUnspecDollar;
}

TEST(GetParameterDelimiter, InvalidDelimiters) {
    // 無効な区切り文字のテスト
    EXPECT_THROW(i_util::GetParameterDelimiter(SV2S(kInvalidSpaceSemicolon)),
                 igesio::SectionFormatError)
            << "Expected SectionFormatError for " << kInvalidSpaceSemicolon;
    EXPECT_EQ(i_util::GetParameterDelimiter(SV2S(kInvalidAmpersandDot)), '&')
            << "Expected '&' for " << kInvalidAmpersandDot;
    EXPECT_THROW(i_util::GetParameterDelimiter(SV2S(kInvalidSpaceUnspec)),
                 igesio::SectionFormatError)
            << "Expected SectionFormatError for " << kInvalidSpaceUnspec;
    EXPECT_EQ(i_util::GetParameterDelimiter(SV2S(kInvalidUnspecZero)), ',')
            << "Expected ',' for " << kInvalidUnspecZero;

    // 指定文字が無効なIGES文字列の形式の場合
    EXPECT_THROW(i_util::GetParameterDelimiter(SV2S(kInvalidEmptySemicolon)),
                 igesio::SectionFormatError)
            << "Expected SectionFormatError for " << kInvalidEmptySemicolon;
    EXPECT_THROW(i_util::GetParameterDelimiter(SV2S(kInvalidXYZSemicolon)),
                 igesio::SectionFormatError)
            << "Expected SectionFormatError for " << kInvalidXYZSemicolon;
    EXPECT_EQ(i_util::GetParameterDelimiter(SV2S(kInvalidUnspecEmpty)), ',')
            << "Expected ',' for " << kInvalidUnspecEmpty;
}



/******************************************************************************
 * レコード区切り文字の取得 (GetRecordDelimiter) のテスト
 *****************************************************************************/

TEST(GetRecordDelimiter, ValidSectionTypes) {
    // 有効なセクションタイプのテスト
    EXPECT_EQ(i_util::GetRecordDelimiter(SV2S(kUnspecUnspec), ','), ';')
            << "Expected ';' for " << kUnspecUnspec;
    EXPECT_EQ(i_util::GetRecordDelimiter(SV2S(kCommaSemicolon), ','), ';')
            << "Expected ';' for " << kCommaSemicolon;
    EXPECT_EQ(i_util::GetRecordDelimiter(SV2S(kXSemicolon), 'x'), ';')
            << "Expected ';' for " << kXSemicolon;
    EXPECT_EQ(i_util::GetRecordDelimiter(SV2S(kCommaPercent), ','), '%')
            << "Expected '%' for " << kCommaPercent;
    EXPECT_EQ(i_util::GetRecordDelimiter(SV2S(kCommaUnspec), ','), ';')
            << "Expected ';' for " << kCommaUnspec;
    EXPECT_EQ(i_util::GetRecordDelimiter(SV2S(kQuestionUnspec), '?'), ';')
            << "Expected ';' for " << kQuestionUnspec;
    EXPECT_EQ(i_util::GetRecordDelimiter(SV2S(kUnspecSemicolon), ','), ';')
            << "Expected ';' for " << kUnspecSemicolon;
    EXPECT_EQ(i_util::GetRecordDelimiter(SV2S(kUnspecDollar), ','), '$')
            << "Expected '$' for " << kUnspecDollar;
}

TEST(GetRecordDelimiter, InvalidSectionTypes) {
    // 無効なセクションタイプのテスト
    // kInvalidSpaceSemicolonはパラメータ区切り文字が無効なためパス
    EXPECT_THROW(i_util::GetRecordDelimiter(SV2S(kInvalidAmpersandDot), '&'),
                 igesio::SectionFormatError)
            << "Expected SectionFormatError for " << kInvalidAmpersandDot;
    // kInvalidSpaceUnspecはパラメータ区切り文字が無効なためパス
    EXPECT_THROW(i_util::GetRecordDelimiter(SV2S(kInvalidUnspecZero), ','),
                 igesio::SectionFormatError)
            << "Expected SectionFormatError for " << kInvalidUnspecZero;

    // 指定文字が無効なIGES文字列の形式の場合
    // kInvalidEmptySemicolonはパラメータ区切り文字が無効なためパス
    // kInvalidXYZSemicolonはパラメータ区切り文字が無効なためパス
    EXPECT_THROW(i_util::GetRecordDelimiter(SV2S(kInvalidAsteriskEmpty), '*'),
                 igesio::TypeConversionError)
            << "Expected TypeConversionError for " << kInvalidAsteriskEmpty;
    EXPECT_THROW(i_util::GetRecordDelimiter(SV2S(kInvalidSlashDoubleSemicolon), '/'),
                 igesio::TypeConversionError)
            << "Expected TypeConversionError for " << kInvalidSlashDoubleSemicolon;
    EXPECT_THROW(i_util::GetRecordDelimiter(SV2S(kInvalidCommaPercentSemicolon), ','),
                 igesio::TypeConversionError)
            << "Expected TypeConversionError for " << kInvalidCommaPercentSemicolon;
    EXPECT_THROW(i_util::GetRecordDelimiter(SV2S(kInvalidUnspecEmpty), ','),
                 igesio::TypeConversionError)
            << "Expected TypeConversionError for " << kInvalidUnspecEmpty;
}

