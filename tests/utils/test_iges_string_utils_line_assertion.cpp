/**
 * @file utils/test_iges_string_utils_line_assertion.cpp
 * @brief utils/iges_string_utils.hのうち、行長さの検証、および
 *        セクションや番号などの情報を取得する関数のテスト
 * @author Yayoi Habami
 * @date 2025-04-13
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <string>
#include <vector>
#include <optional>

#include "igesio/utils/iges_string_utils.h"

namespace {

namespace i_util = igesio::utils;

using SType = igesio::SectionType;

/**
 * 長さ検証用（文字列）
 */

/// @brief フラグセクションの1行目
constexpr std::string_view kValidFlagL1 =
"                                                                        C      1";
/// @brief スタートセクションの5行目、シーケンス番号は5
constexpr std::string_view kValidStartL5 =
"and Hello World! Now you can use IGES!                                  S      5";
/// @brief グローバルセクションの2行目、シーケンス番号は2
constexpr std::string_view kValidGlobalL2 =
"1,4HINCH,1,0.028,13H900729.231652,0.0005,100.0,                         G      2";
/// @brief ディレクトリエントリセクションの1行目、シーケンス番号は1
constexpr std::string_view kValidDirL1 =
"     124       1       1       1       0       0       0       0       0D      1";
/// @brief パラメータデータセクションの1行目、DEポインタは1
constexpr std::string_view kValidParamL1 =
"124,0.70710678,-0.70710678,0.0,1.0,0.70710678,0.70710678,0.0,          1P      1";
/// @brief パラメータデータセクションの2行目、DEポインタは1
constexpr std::string_view kValidParamL2 =
"1.0,0.0,0.0,1.0,0.0,0,0;                                               1P      2";
/// @brief ターミネートセクションの1行目、シーケンス番号は1
constexpr std::string_view kValidTermL1 =
"S      7G      3D    180P     96                                        T      1";

/// @brief 圧縮形式のデータセクション (72文字)
/// @note is_compressed = trueのときのみ有効
constexpr std::string_view kValidCompDataC72 =
"10,1.0953333333333,0.0,0.2,0.8,-3.5,0.0,-1,3.589793,0.0,0.0,0.0,0,0.02,0";
/// @brief 圧縮形式のデータセクション (73文字)
constexpr std::string_view kInvalidCompDataC73 =
"10,1.0953333333333,0.0,0.2,0.8,-3.5,0.0,-1,3.589793,0.0,0.0,0.0,0,0.02,01";

/// @brief 正しくないスタートセクション（シーケンス番号がない）
constexpr std::string_view kInvalidStart =
"and Hello World! Now you can use IGES!                                  S   ";

/// @brief std::string_viewからstd::stringに変換する
/// @param str 文字列
/// @return 変換した文字列
std::string SV2S(const std::string_view& str) {
    return std::string(str);
}

}  // namespace



/******************************************************************************
 * IGES文字列の長さ検証 (AssertLength) のテスト
 *****************************************************************************/

// AssertLength関数のテスト
// 通常形式の80文字のケース
TEST(AssertLengthTest, NormalCase) {
    // 通常形式（80文字）のIGES行の検証
    EXPECT_NO_THROW(i_util::AssertLength(SV2S(kValidFlagL1)));
    EXPECT_NO_THROW(i_util::AssertLength(SV2S(kValidStartL5)));
    EXPECT_NO_THROW(i_util::AssertLength(SV2S(kValidGlobalL2)));
    EXPECT_NO_THROW(i_util::AssertLength(SV2S(kValidDirL1)));
    EXPECT_NO_THROW(i_util::AssertLength(SV2S(kValidParamL1)));
    EXPECT_NO_THROW(i_util::AssertLength(SV2S(kValidParamL2)));
    EXPECT_NO_THROW(i_util::AssertLength(SV2S(kValidTermL1)));

    // エラーケース：通常形式で80文字以外
    EXPECT_THROW(i_util::AssertLength(SV2S(kValidCompDataC72)),
                 igesio::LineFormatError);
    EXPECT_THROW(i_util::AssertLength(SV2S(kValidFlagL1.substr(0, 79))),
                 igesio::LineFormatError);
    EXPECT_THROW(i_util::AssertLength(SV2S(std::string(kValidFlagL1) + "X")),
                 igesio::LineFormatError);

    // エラーケース：不完全な行
    EXPECT_THROW(i_util::AssertLength(SV2S(kInvalidStart)),
                 igesio::LineFormatError);
}

// 圧縮形式のデータセクションのテスト
TEST(AssertLengthTest, CompressedFormat) {
    // 圧縮形式（72文字以下）のデータセクションの検証
    EXPECT_NO_THROW(i_util::AssertLength(SV2S(kValidCompDataC72), true));

    // エラーケース：圧縮形式で73文字以上
    EXPECT_THROW(i_util::AssertLength(SV2S(kInvalidCompDataC73), true),
                 igesio::LineFormatError);
}

// 境界ケースのテスト
TEST(AssertLengthTest, EdgeCases) {
    // 空の文字列
    EXPECT_THROW(i_util::AssertLength(SV2S("")), igesio::LineFormatError);
    // 空白文字のみの文字列
    EXPECT_THROW(i_util::AssertLength(SV2S("    ")), igesio::LineFormatError);

    // 圧縮形式で空の文字列
    EXPECT_NO_THROW(i_util::AssertLength(SV2S(""), true));
    // 空白文字のみの文字列
    EXPECT_NO_THROW(i_util::AssertLength(SV2S("    "), true));
}



/******************************************************************************
 * セクションの種類を特定する (GetSectionType) のテスト
 *****************************************************************************/

// GetSectionTypeの正常系テスト
TEST(GetSectionTypeTest, NormalCase) {
    // 通常形式の各セクションの識別
    EXPECT_EQ(i_util::GetSectionType(SV2S(kValidFlagL1)), SType::kFlag);
    EXPECT_EQ(i_util::GetSectionType(SV2S(kValidStartL5)), SType::kStart);
    EXPECT_EQ(i_util::GetSectionType(SV2S(kValidGlobalL2)), SType::kGlobal);
    EXPECT_EQ(i_util::GetSectionType(SV2S(kValidDirL1)), SType::kDirectory);
    EXPECT_EQ(i_util::GetSectionType(SV2S(kValidParamL1)), SType::kParameter);
    EXPECT_EQ(i_util::GetSectionType(SV2S(kValidTermL1)), SType::kTerminate);
}

// GetSectionType関数の圧縮形式テスト
TEST(GetSectionTypeTest, CompressedFormat) {
    // 圧縮形式のデータセクション
    EXPECT_EQ(i_util::GetSectionType(SV2S(kValidCompDataC72), true), SType::kData);
}

// 無効なセクション識別子のテスト
TEST(GetSectionTypeTest, InvalidSectionIdentifier) {
    // セクション識別子が異なる
    auto invalid_section = SV2S(kValidFlagL1);
    invalid_section[72] = 'X';  // Cではなく X に変更

    EXPECT_THROW(i_util::GetSectionType(invalid_section), igesio::SectionFormatError);

    // セクション識別子がない
    std::string no_identifier(80, ' ');
    EXPECT_THROW(i_util::GetSectionType(no_identifier), igesio::SectionFormatError);
}

// 不正なフォーマットのテスト
TEST(GetSectionTypeTest, InvalidFormat) {
    // 80文字より短い
    auto too_short = SV2S(kValidFlagL1.substr(0, 75));
    EXPECT_THROW(i_util::GetSectionType(too_short), igesio::LineFormatError);

    // 80文字より長い
    auto too_long = SV2S(std::string(kValidFlagL1) + "XXX");
    EXPECT_THROW(i_util::GetSectionType(too_long), igesio::LineFormatError);

    // 圧縮形式で73文字以上の場合
    EXPECT_THROW(i_util::GetSectionType(SV2S(kInvalidCompDataC73), true), igesio::LineFormatError);
}

// 境界ケースのテスト
TEST(GetSectionTypeTest, EdgeCases) {
    // 空の入力
    EXPECT_THROW(i_util::GetSectionType(SV2S("")), igesio::LineFormatError);

    // 圧縮形式で空の入力
    EXPECT_EQ(i_util::GetSectionType(SV2S(""), true), SType::kData);

    // 全ての有効なセクション識別子のテスト
    std::string section_line(80, ' ');
    section_line[72] = 'C';
    EXPECT_EQ(i_util::GetSectionType(section_line), SType::kFlag);

    section_line[72] = 'S';
    EXPECT_EQ(i_util::GetSectionType(section_line), SType::kStart);

    section_line[72] = 'G';
    EXPECT_EQ(i_util::GetSectionType(section_line), SType::kGlobal);

    section_line[72] = 'D';
    EXPECT_EQ(i_util::GetSectionType(section_line), SType::kDirectory);

    section_line[72] = 'P';
    EXPECT_EQ(i_util::GetSectionType(section_line), SType::kParameter);

    section_line[72] = 'T';
    EXPECT_EQ(i_util::GetSectionType(section_line), SType::kTerminate);
}



/******************************************************************************
 * シーケンス番号を取得する (GetSequenceNumber) のテスト
 *****************************************************************************/

// GetSequenceNumber関数の正常系テスト
TEST(GetSequenceNumberTest, NormalCase) {
    // 各セクションのシーケンス番号を正しく取得できるかテスト
    EXPECT_EQ(i_util::GetSequenceNumber(SV2S(kValidFlagL1)), 1);
    EXPECT_EQ(i_util::GetSequenceNumber(SV2S(kValidStartL5)), 5);
    EXPECT_EQ(i_util::GetSequenceNumber(SV2S(kValidGlobalL2)), 2);
    EXPECT_EQ(i_util::GetSequenceNumber(SV2S(kValidDirL1)), 1);
    EXPECT_EQ(i_util::GetSequenceNumber(SV2S(kValidParamL1)), 1);
    EXPECT_EQ(i_util::GetSequenceNumber(SV2S(kValidParamL2)), 2);
    EXPECT_EQ(i_util::GetSequenceNumber(SV2S(kValidTermL1)), 1);
}

// 圧縮形式のテスト
TEST(GetSequenceNumberTest, CompressedFormat) {
    // 圧縮形式の場合はエラーをスローするはず
    EXPECT_THROW(i_util::GetSequenceNumber(SV2S(kValidCompDataC72), true),
                 igesio::SectionFormatError);
}

// 無効な形式のテスト
TEST(GetSequenceNumberTest, InvalidFormat) {
    // シーケンス番号部分が数字でない
    auto invalid_seq = SV2S(kValidFlagL1);
    invalid_seq[76] = 'A';  // 数字を文字に置き換え
    EXPECT_THROW(i_util::GetSequenceNumber(invalid_seq), igesio::SectionFormatError);

    // 行の長さが不正
    EXPECT_THROW(i_util::GetSequenceNumber(SV2S(kValidFlagL1.substr(0, 79))),
                igesio::LineFormatError);

    // シーケンス番号が存在しない（空白のみ）
    auto empty_seq = SV2S(kValidFlagL1);
    for (size_t i = 73; i < 80; ++i) {
        empty_seq[i] = ' ';
    }
    EXPECT_THROW(i_util::GetSequenceNumber(empty_seq), igesio::SectionFormatError);
}

// 境界値テスト
TEST(GetSequenceNumberTest, BoundaryValues) {
    // シーケンス番号の下限値（1）はすでにテスト済み

    // シーケンス番号の上限値（9999999; 7桁のため）をテスト
    auto max_seq = SV2S(kValidFlagL1);
    for (size_t i = 73; i < 80; ++i) {
        max_seq[i] = '9';
    }
    EXPECT_EQ(i_util::GetSequenceNumber(max_seq), 9999999);

    // 先頭にゼロがある場合
    auto leading_zeros = SV2S(kValidFlagL1);
    leading_zeros[73] = '0';
    leading_zeros[74] = '0';
    leading_zeros[75] = '0';
    leading_zeros[76] = '0';
    leading_zeros[77] = '0';
    leading_zeros[78] = '0';
    leading_zeros[79] = '5';
    EXPECT_EQ(i_util::GetSequenceNumber(leading_zeros), 5);
}

// 空の入力テスト
TEST(GetSequenceNumberTest, EmptyInput) {
    // 空の入力
    EXPECT_THROW(i_util::GetSequenceNumber(SV2S("")), igesio::LineFormatError);
}



/******************************************************************************
 * パラメータセクションのDEポインタを取得する (GetDEPointer) のテスト
 *****************************************************************************/

// GetDEPointer関数の正常系テスト
TEST(GetDEPointerTest, NormalCase) {
    // 各パラメータセクションのDEポインタを正しく取得できるかテスト
    EXPECT_EQ(i_util::GetDEPointer(SV2S(kValidParamL1)), 1);
    EXPECT_EQ(i_util::GetDEPointer(SV2S(kValidParamL2)), 1);

    // DEポインタが複数桁の場合
    constexpr std::string_view param_line_pd123 =
    "124,0.70710678,-0.70710678,0.0,1.0,0.70710678,0.70710678,0.0,        123P      1";
    EXPECT_EQ(i_util::GetDEPointer(SV2S(param_line_pd123)), 123);
}

// セクション種別エラーのテスト
TEST(GetDEPointerTest, WrongSectionType) {
    // パラメータセクション以外でエラーをスローするかテスト
    EXPECT_THROW(i_util::GetDEPointer(SV2S(kValidFlagL1)), igesio::SectionFormatError);
    EXPECT_THROW(i_util::GetDEPointer(SV2S(kValidStartL5)), igesio::SectionFormatError);
    EXPECT_THROW(i_util::GetDEPointer(SV2S(kValidGlobalL2)), igesio::SectionFormatError);
    EXPECT_THROW(i_util::GetDEPointer(SV2S(kValidDirL1)), igesio::SectionFormatError);
    EXPECT_THROW(i_util::GetDEPointer(SV2S(kValidTermL1)), igesio::SectionFormatError);

    // 圧縮形式の場合
    EXPECT_THROW(i_util::GetDEPointer(SV2S(kValidCompDataC72)),
                 igesio::LineFormatError);
}

// 無効な形式のテスト
TEST(GetDEPointerTest, InvalidFormat) {
    // DEポインタ部分が数字でない
    auto invalid_pd = SV2S(kValidParamL1);
    invalid_pd[71] = 'A';  // 数字を文字に置き換え
    EXPECT_THROW(i_util::GetDEPointer(invalid_pd), igesio::SectionFormatError);

    // 行の長さが不正
    EXPECT_THROW(i_util::GetDEPointer(SV2S(kValidParamL1.substr(0, 79))),
                 igesio::LineFormatError);

    // DEポインタが存在しない（空白のみ）
    auto empty_pd = SV2S(kValidParamL1);
    for (size_t i = 64; i < 72; ++i) {
        empty_pd[i] = ' ';
    }
    EXPECT_THROW(i_util::GetDEPointer(empty_pd), igesio::SectionFormatError);
}

// 境界値テスト
TEST(GetDEPointerTest, BoundaryValues) {
    // DEポインタの下限値（1）はすでにテスト済み

    // DEポインタの上限値（99999999; 8桁）をテスト
    auto max_pd = SV2S(kValidParamL1);
    for (size_t i = 64; i < 72; ++i) {
        max_pd[i] = '9';
    }
    EXPECT_EQ(i_util::GetDEPointer(max_pd), 99999999);

    // 先頭にゼロがある場合
    auto leading_zeros = SV2S(kValidParamL1);
    leading_zeros[64] = '0';
    leading_zeros[65] = '0';
    leading_zeros[66] = '0';
    leading_zeros[67] = '0';
    leading_zeros[68] = '0';
    leading_zeros[69] = '0';
    leading_zeros[70] = '0';
    leading_zeros[71] = '5';
    EXPECT_EQ(i_util::GetDEPointer(leading_zeros), 5);
}

// 空の入力テスト
TEST(GetDEPointerTest, EmptyInput) {
    // 空の入力
    EXPECT_THROW(i_util::GetDEPointer(SV2S("")),
                 igesio::LineFormatError);
}

// DEポインタの配置位置に関するテスト
TEST(GetDEPointerTest, PositionTest) {
    // DEポインタフィールドの位置が正確に65-71列目であることを確認
    auto valid_param_line = SV2S(kValidParamL1);

    // 64列目を変更すると ("2      1") エラー
    valid_param_line[64] = '2';
    EXPECT_THROW(i_util::GetDEPointer(valid_param_line),
                 igesio::SectionFormatError);

    // 69列目を変更すると ("     5 1") エラー
    valid_param_line[64] = ' ';
    valid_param_line[69] = '5';
    EXPECT_THROW(i_util::GetDEPointer(valid_param_line),
                 igesio::SectionFormatError);

    // 70列目を変更 ("     521") 、数字が連続していればOK
    valid_param_line[70] = '2';
    EXPECT_EQ(i_util::GetDEPointer(valid_param_line), 521);
}



/******************************************************************************
 * データ部分を取得する (GetDataPart) のテスト
 *****************************************************************************/

TEST(GetDataPartTest, FlagSection) {
    // フラグセクションは空文字列を返す
    EXPECT_EQ(i_util::GetDataPart(SV2S(kValidFlagL1), SType::kFlag), "");
}

TEST(GetDataPartTest, TerminateSection) {
    // ターミネートセクションは先頭32文字を返す
    EXPECT_EQ(i_util::GetDataPart(SV2S(kValidTermL1), SType::kTerminate),
             "S      7G      3D    180P     96");
}

TEST(GetDataPartTest, ParameterSection) {
    // パラメータセクションは先頭64文字を返す
    EXPECT_EQ(i_util::GetDataPart(SV2S(kValidParamL1), SType::kParameter),
             "124,0.70710678,-0.70710678,0.0,1.0,0.70710678,0.70710678,0.0,   ");
    EXPECT_EQ(i_util::GetDataPart(SV2S(kValidParamL2), SType::kParameter),
             "1.0,0.0,0.0,1.0,0.0,0,0;                                        ");
}

TEST(GetDataPartTest, OtherSections) {
    // それ以外のセクション（スタート、グローバル、ディレクトリ）は先頭72文字を返す
    EXPECT_EQ(i_util::GetDataPart(SV2S(kValidStartL5), SType::kStart),
             "and Hello World! Now you can use IGES!                                  ");
    EXPECT_EQ(i_util::GetDataPart(SV2S(kValidGlobalL2), SType::kGlobal),
             "1,4HINCH,1,0.028,13H900729.231652,0.0005,100.0,                         ");
    EXPECT_EQ(i_util::GetDataPart(SV2S(kValidDirL1), SType::kDirectory),
             "     124       1       1       1       0       0       0       0       0");
}

TEST(GetDataPartTest, CompressedData) {
    // 圧縮形式のデータセクションの場合、そのままの文字列を返す
    EXPECT_EQ(i_util::GetDataPart(SV2S(kValidCompDataC72), SType::kData),
             "10,1.0953333333333,0.0,0.2,0.8,-3.5,0.0,-1,3.589793,0.0,0.0,0.0,0,0.02,0");

    // 空のデータも許容
    EXPECT_EQ(i_util::GetDataPart("", SType::kData), "");
    EXPECT_EQ(i_util::GetDataPart("  ", SType::kData), "  ");
}

TEST(GetDataPartTest, InvalidInputLength) {
    // 行の長さが必要な文字数より短い場合
    EXPECT_THROW(i_util::GetDataPart(SV2S(kValidStartL5.substr(0, 71)), SType::kStart),
                 igesio::LineFormatError);
    EXPECT_THROW(i_util::GetDataPart(SV2S(kValidParamL1.substr(0, 63)), SType::kParameter),
                 igesio::LineFormatError);
    EXPECT_THROW(i_util::GetDataPart(SV2S(kValidTermL1.substr(0, 31)), SType::kTerminate),
                 igesio::LineFormatError);
}

TEST(GetDataPartTest, EdgeCases) {
    // ちょうど必要な長さの場合
    std::string exact_param(64, 'X');
    EXPECT_NO_THROW(i_util::GetDataPart(exact_param, SType::kParameter));
    EXPECT_EQ(i_util::GetDataPart(exact_param, SType::kParameter), exact_param);

    std::string exact_term(32, 'X');
    EXPECT_NO_THROW(i_util::GetDataPart(exact_term, SType::kTerminate));
    EXPECT_EQ(i_util::GetDataPart(exact_term, SType::kTerminate), exact_term);

    std::string exact_normal(72, 'X');
    EXPECT_NO_THROW(i_util::GetDataPart(exact_normal, SType::kStart));
    EXPECT_EQ(i_util::GetDataPart(exact_normal, SType::kStart), exact_normal);

    // 必要以上の長さの場合は切り捨てられるだけ
    std::string long_param(100, 'X');
    EXPECT_NO_THROW(i_util::GetDataPart(long_param, SType::kParameter));
    EXPECT_EQ(i_util::GetDataPart(long_param, SType::kParameter), std::string(64, 'X'));

    // フラグセクションには長さ制限がない
    EXPECT_NO_THROW(i_util::GetDataPart("", SType::kFlag));
    EXPECT_NO_THROW(i_util::GetDataPart(long_param, SType::kFlag));
    EXPECT_EQ(i_util::GetDataPart(long_param, SType::kFlag), "");
}

TEST(GetDataPartTest, CharacterPreservation) {
    // 特殊文字や空白文字が保持されることを確認
    std::string special_chars = "!@#$%^&*()_+{}[]|:;'<>,.?/`~\"\\-=";

    std::string param_line = special_chars + std::string(64 - special_chars.length(), ' ');
    EXPECT_EQ(i_util::GetDataPart(param_line, SType::kParameter), param_line);

    std::string start_line = special_chars + std::string(72 - special_chars.length(), ' ');
    EXPECT_EQ(i_util::GetDataPart(start_line, SType::kStart), start_line);

    std::string term_line = special_chars + std::string(32 - special_chars.length(), ' ');
    EXPECT_EQ(i_util::GetDataPart(term_line, SType::kTerminate), term_line);
}



/******************************************************************************
 * セクションのデータ部をパースする (ParseFreeFormattedData) のテスト
 *****************************************************************************/

// 通常のテスト
TEST(ParseFreeFormattedDataTest, NormalCase) {
    // グローバルセクションの一部
    std::vector<std::string> lines = {
        "10HTEST.CASES,1.0,9,2HUM,1,,13H900729.231212,0.01,300.0,                ",
        "26Example Global Data Section,8HIPO/NIST,3,0;                           "
    };
    std::vector<std::string> expected = {
        "10HTEST.CASES", "1.0", "9", "2HUM", "1", "", "13H900729.231212",
        "0.01", "300.0", "26Example Global Data Section", "8HIPO/NIST", "3", "0"
    };
    auto result = i_util::ParseFreeFormattedData(lines, ',', ';');
    EXPECT_EQ(result, expected);

    // グローバルセクションの一部（途中でH文字列の折り返しがある場合）
    lines = {
        "14He_rounded_cube,1.,2,2HMM,50,0.125,13H250408.163937,1E-08,499990.,11HY",
        "ayoiHabami,,11,0,13H250408.163937;                                      "
    };
    expected = {
        "14He_rounded_cube", "1.", "2", "2HMM", "50", "0.125",
        "13H250408.163937", "1E-08", "499990.", "11HYayoiHabami", "", "11", "0",
        "13H250408.163937"
    };
    result = i_util::ParseFreeFormattedData(lines, ',', ';');
    EXPECT_EQ(result, expected);

    // H文字列の中に区切り文字がある場合
    lines[0][4] = ',';
    lines[0][12] = ';';
    expected[0] = "14He,rounded;cube";
    result = i_util::ParseFreeFormattedData(lines, ',', ';');
    EXPECT_EQ(result, expected);
}

// 途中でレコード区切り文字がある場合
TEST(ParseFreeFormattedDataTest, RecordSeparator) {
        // グローバルセクションの一部だが、途中でレコード区切り文字があるもの
        // （フォーマットとしては正しくない）
        std::vector<std::string> lines = {
            "10HTEST.CASES,1.0,9,2HUM,1,,13H900729.231212,0.01;300.0,                ",
            "26Example Global Data Section,8HIPO/NIST,3,0;                           "
        };
        std::vector<std::string> expected = {
            "10HTEST.CASES", "1.0", "9", "2HUM", "1", "", "13H900729.231212", "0.01"
        };
        auto result = i_util::ParseFreeFormattedData(lines, ',', ';');
        EXPECT_EQ(result, expected);
}

// 異常系のテスト
TEST(ParseFreeFormattedDataTest, ErrorCase) {
    // H文字列の後ろに区切り文字がない場合
    std::vector<std::string> lines = {
        "11HTEST.CASES,1.0,9,2HUM,1,,13H900729.231212,0.01,300.0,                ",
        "26Example Global Data Section,8HIPO/NIST,3,0;                           "
    };
    EXPECT_THROW(i_util::ParseFreeFormattedData(lines, ',', ';'),
                 igesio::SectionFormatError);
}
