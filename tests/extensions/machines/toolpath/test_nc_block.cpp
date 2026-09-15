/**
 * @file tests/extensions/machines/toolpath/test_nc_block.cpp
 * @brief NCプログラムの字句 (toolpath/nc_block) のテスト
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note 対象: LexNc / IsIntegerAddress / FormatNcWord / FormatNcInteger
 *       - 正常系 (代表値): `( )`と`;`のコメント、`%`行と空行の除去、ブロックスキップ
 *         (`/`と`/n`のON/OFF)、ワードの分解 (小文字、`G 01`の空白、`H12D12`、
 *         `G1Z19.3198`、符号と`12.`の末尾小数点)、整数アドレスの丸め (G/Mは
 *         小数コードのため丸めない),
 *         `O`/`:`のプログラム番号、`N`番号、行範囲、各書式の整形
 *       - 正常系 (境界値): 空文字列、CRLF入力、末尾に改行の無い入力、行範囲が
 *         ファイル外、`-0`の整形、0桁の整形、桁数固定の負数
 *       - 正常系 (退化): コメントだけの行 (ブロックとして残る)、ワードの無い行
 *       - 警告: 閉じられていない`(`、解釈できない文字 (ブロックにつき1件)
 *       TODO: 例外を送出するAPIは無い (不正な入力は警告して読み飛ばす設計)
 */
#include <gtest/gtest.h>

#include <cstddef>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/toolpath/nc_block.h"

namespace {

namespace mc = igesio::extensions::machines;

/// @brief 数値比較の許容誤差
constexpr double kTol = 1e-12;

/// @brief ワードをアドレスと値の文字列にする (`"X10"`のように. 比較用)
std::string WordText(const mc::NcWord& word) {
    return std::string(1, word.address) + std::to_string(word.value);
}

/// @brief ブロックのワードをアドレス文字の並びにする (`"GXY"`のように)
std::string Addresses(const mc::NcBlock& block) {
    std::string text;
    for (const mc::NcWord& word : block.words) text.push_back(word.address);
    return text;
}

/// @brief 指定アドレスの最初のワードの値を取得する
/// @return 無ければ`std::nullopt`
std::optional<double> ValueOf(const mc::NcBlock& block, const char address) {
    for (const mc::NcWord& word : block.words) {
        if (word.address == address) return word.value;
    }
    return std::nullopt;
}

}  // namespace



// ---- LexNc: コメントと行の除去 ----

TEST(NcBlockTest, Lex_ParenCommentIsRemovedAndKept) {
    const mc::NcLexResult result = mc::LexNc("G01 (move to start) X10.\n");
    ASSERT_EQ(result.blocks.size(), 1u);
    EXPECT_EQ(result.blocks[0].text, "G01  X10.");
    ASSERT_EQ(result.blocks[0].comments.size(), 1u);
    EXPECT_EQ(result.blocks[0].comments[0], "move to start");
    EXPECT_EQ(Addresses(result.blocks[0]), "GX");
    EXPECT_TRUE(result.warnings.empty());
}

TEST(NcBlockTest, Lex_SemicolonCommentRunsToEndOfLine) {
    const mc::NcLexResult result = mc::LexNc("X1. ; Y2. (not a word)\n");
    ASSERT_EQ(result.blocks.size(), 1u);
    EXPECT_EQ(Addresses(result.blocks[0]), "X");
    ASSERT_EQ(result.blocks[0].comments.size(), 1u);
    EXPECT_EQ(result.blocks[0].comments[0], "Y2. (not a word)");
}

TEST(NcBlockTest, Lex_ParenCommentClosesAtFirstParen) {
    const mc::NcLexResult result = mc::LexNc("(a) X1. (b) Y2.\n");
    ASSERT_EQ(result.blocks.size(), 1u);
    EXPECT_EQ(Addresses(result.blocks[0]), "XY");
    ASSERT_EQ(result.blocks[0].comments.size(), 2u);
    EXPECT_EQ(result.blocks[0].comments[0], "a");
    EXPECT_EQ(result.blocks[0].comments[1], "b");
}

TEST(NcBlockTest, Lex_UnclosedParenWarnsAndCommentsToEndOfLine) {
    const mc::NcLexResult result = mc::LexNc("X1. (unclosed Y2.\n");
    ASSERT_EQ(result.blocks.size(), 1u);
    EXPECT_EQ(Addresses(result.blocks[0]), "X");
    ASSERT_EQ(result.blocks[0].comments.size(), 1u);
    EXPECT_EQ(result.blocks[0].comments[0], "unclosed Y2.");
    ASSERT_EQ(result.warnings.size(), 1u);
    EXPECT_EQ(result.warnings[0].line, 1);
    EXPECT_TRUE(result.warnings[0].context.empty());
}

TEST(NcBlockTest, Lex_PercentAndBlankLinesAreDropped) {
    const mc::NcLexResult result = mc::LexNc("%\n\n  \nG00\n%\n");
    ASSERT_EQ(result.blocks.size(), 1u);
    EXPECT_EQ(result.blocks[0].line, 4);
}

TEST(NcBlockTest, Lex_CommentOnlyLineIsKeptAsBlock) {
    const mc::NcLexResult result = mc::LexNc("(header)\nG00\n");
    ASSERT_EQ(result.blocks.size(), 2u);
    EXPECT_TRUE(result.blocks[0].words.empty());
    EXPECT_TRUE(result.blocks[0].text.empty());
    ASSERT_EQ(result.blocks[0].comments.size(), 1u);
    EXPECT_EQ(result.blocks[0].comments[0], "header");
}

TEST(NcBlockTest, Lex_CrlfAndMissingTrailingNewline) {
    const mc::NcLexResult result = mc::LexNc("G00\r\nX1.\r\nY2.");
    ASSERT_EQ(result.blocks.size(), 3u);
    EXPECT_EQ(result.blocks[0].text, "G00");
    EXPECT_EQ(result.blocks[2].text, "Y2.");
    EXPECT_EQ(result.blocks[2].line, 3);
}

TEST(NcBlockTest, Lex_EmptyTextGivesNoBlocks) {
    const mc::NcLexResult result = mc::LexNc("");
    EXPECT_TRUE(result.blocks.empty());
    EXPECT_TRUE(result.programs.Empty());
    EXPECT_TRUE(result.warnings.empty());
}

// ---- LexNc: ブロックスキップ ----

TEST(NcBlockTest, BlockSkip_BareSlashIsSwitchOne) {
    mc::NcLexOptions options;
    options.block_skip = {1};
    const mc::NcLexResult result = mc::LexNc("/G00 X1.\nG01\n", options);
    ASSERT_EQ(result.blocks.size(), 1u);
    EXPECT_EQ(result.blocks[0].text, "G01");
}

TEST(NcBlockTest, BlockSkip_OffSwitchRemovesPrefixOnly) {
    mc::NcLexOptions options;
    options.block_skip = {2};
    const mc::NcLexResult result = mc::LexNc("/G00 X1.\n/2 G01\n", options);
    ASSERT_EQ(result.blocks.size(), 1u);
    EXPECT_EQ(result.blocks[0].text, "G00 X1.");
    ASSERT_TRUE(result.blocks[0].block_skip.has_value());
    EXPECT_EQ(*result.blocks[0].block_skip, 1);
    EXPECT_EQ(Addresses(result.blocks[0]), "GX");
}

TEST(NcBlockTest, BlockSkip_NoSwitchesKeepsAllBlocks) {
    const mc::NcLexResult result = mc::LexNc("/G00\n/2G01\nG02\n");
    ASSERT_EQ(result.blocks.size(), 3u);
    EXPECT_EQ(*result.blocks[1].block_skip, 2);
    EXPECT_FALSE(result.blocks[2].block_skip.has_value());
}

// ---- LexNc: ワード ----

TEST(NcBlockTest, Words_TypicalBlock) {
    const mc::NcLexResult result = mc::LexNc("G1X10.Y-2Z.5F200\n");
    ASSERT_EQ(result.blocks.size(), 1u);
    const mc::NcBlock& block = result.blocks[0];
    EXPECT_EQ(Addresses(block), "GXYZF");
    EXPECT_NEAR(*ValueOf(block, 'G'), 1.0, kTol);
    EXPECT_NEAR(*ValueOf(block, 'X'), 10.0, kTol);
    EXPECT_NEAR(*ValueOf(block, 'Y'), -2.0, kTol);
    EXPECT_NEAR(*ValueOf(block, 'Z'), 0.5, kTol);
    EXPECT_NEAR(*ValueOf(block, 'F'), 200.0, kTol);
    EXPECT_TRUE(block.words[1].has_decimal_point);
    EXPECT_FALSE(block.words[2].has_decimal_point);
    EXPECT_TRUE(result.warnings.empty());
}

TEST(NcBlockTest, Words_LowercaseAndSpaceBetweenAddressAndNumber) {
    const mc::NcLexResult result = mc::LexNc("g 01 x+1.5 y 2\n");
    ASSERT_EQ(result.blocks.size(), 1u);
    const mc::NcBlock& block = result.blocks[0];
    EXPECT_EQ(Addresses(block), "GXY");
    EXPECT_NEAR(*ValueOf(block, 'X'), 1.5, kTol);
    EXPECT_NEAR(*ValueOf(block, 'Y'), 2.0, kTol);
}

TEST(NcBlockTest, Words_AdjacentIntegerWords) {
    const mc::NcLexResult result = mc::LexNc("H12D12\nG1Z19.3198\nT12M06\n");
    ASSERT_EQ(result.blocks.size(), 3u);
    EXPECT_EQ(Addresses(result.blocks[0]), "HD");
    EXPECT_NEAR(*ValueOf(result.blocks[0], 'H'), 12.0, kTol);
    EXPECT_EQ(Addresses(result.blocks[1]), "GZ");
    EXPECT_NEAR(*ValueOf(result.blocks[1], 'Z'), 19.3198, kTol);
    EXPECT_EQ(Addresses(result.blocks[2]), "TM");
    EXPECT_NEAR(*ValueOf(result.blocks[2], 'M'), 6.0, kTol);
}

TEST(NcBlockTest, Words_IntegerAddressesAreRounded) {
    const mc::NcLexResult result = mc::LexNc("G43.5 M3.4 T2.6 X1.6\n");
    ASSERT_EQ(result.blocks.size(), 1u);
    const mc::NcBlock& block = result.blocks[0];
    // G/Mは小数コード (`G43.5`) を持つので丸めない
    EXPECT_NEAR(*ValueOf(block, 'G'), 43.5, kTol);
    EXPECT_NEAR(*ValueOf(block, 'M'), 3.4, kTol);
    EXPECT_NEAR(*ValueOf(block, 'T'), 3.0, kTol);
    EXPECT_NEAR(*ValueOf(block, 'X'), 1.6, kTol);
}

TEST(NcBlockTest, Words_UnexpectedTextWarnsOncePerBlock) {
    const mc::NcLexResult result = mc::LexNc("G01 ## X1. @@\nY2.\n");
    ASSERT_EQ(result.blocks.size(), 2u);
    EXPECT_EQ(Addresses(result.blocks[0]), "GX");
    ASSERT_EQ(result.warnings.size(), 1u);
    EXPECT_EQ(result.warnings[0].line, 1);
    EXPECT_NE(result.warnings[0].message.find("unexpected text"), std::string::npos);
}

TEST(NcBlockTest, Words_AddressWithoutNumberWarns) {
    const mc::NcLexResult result = mc::LexNc("G01 X\n");
    ASSERT_EQ(result.blocks.size(), 1u);
    EXPECT_EQ(Addresses(result.blocks[0]), "G");
    EXPECT_EQ(result.warnings.size(), 1u);
}

TEST(NcBlockTest, Words_WordTextIsAddressAndValue) {
    const mc::NcLexResult result = mc::LexNc("X10\n");
    ASSERT_EQ(result.blocks.size(), 1u);
    EXPECT_EQ(WordText(result.blocks[0].words[0]), "X10.000000");
}

// ---- LexNc: プログラム番号とN番号 ----

TEST(NcBlockTest, Programs_ONumbersAreRegistered) {
    const mc::NcLexResult result = mc::LexNc("O0001\nG00\nM99\n:0100\nM99\nO0200 G00\n");
    ASSERT_EQ(result.blocks.size(), 6u);
    ASSERT_EQ(result.programs.Size(), 3u);
    EXPECT_EQ(result.programs.Find(1).value(), 0u);
    EXPECT_EQ(result.programs.Find(100).value(), 3u);
    EXPECT_EQ(result.programs.Find(200).value(), 5u);
    EXPECT_FALSE(result.programs.Find(2).has_value());
    EXPECT_EQ(*result.blocks[0].program_number, 1);
    EXPECT_TRUE(result.blocks[0].words.empty());
    EXPECT_EQ(Addresses(result.blocks[5]), "G");
}

TEST(NcBlockTest, Programs_ONotAtStartIsPlainWord) {
    const mc::NcLexResult result = mc::LexNc("G00 O5\n");
    ASSERT_EQ(result.blocks.size(), 1u);
    EXPECT_FALSE(result.blocks[0].program_number.has_value());
    EXPECT_EQ(Addresses(result.blocks[0]), "GO");
    EXPECT_TRUE(result.programs.Empty());
}

TEST(NcBlockTest, Sequence_NNumberIsKeptOutsideWords) {
    const mc::NcLexResult result = mc::LexNc("N10 G01 X1.\nG02\n");
    ASSERT_EQ(result.blocks.size(), 2u);
    ASSERT_TRUE(result.blocks[0].sequence.has_value());
    EXPECT_EQ(*result.blocks[0].sequence, 10);
    EXPECT_EQ(Addresses(result.blocks[0]), "GX");
    EXPECT_FALSE(result.blocks[1].sequence.has_value());
}

// ---- LexNc: 行範囲 ----

TEST(NcBlockTest, LineRange_StartAndEndAreInclusive) {
    mc::NcLexOptions options;
    options.start_line = 2;
    options.end_line = 3;
    const mc::NcLexResult result = mc::LexNc("G00\nG01\nG02\nG03\n", options);
    ASSERT_EQ(result.blocks.size(), 2u);
    EXPECT_EQ(result.blocks[0].text, "G01");
    EXPECT_EQ(result.blocks[0].line, 2);
    EXPECT_EQ(result.blocks[1].text, "G02");
}

TEST(NcBlockTest, LineRange_OutsideFileGivesNoBlocks) {
    mc::NcLexOptions options;
    options.start_line = 10;
    const mc::NcLexResult result = mc::LexNc("G00\nG01\n", options);
    EXPECT_TRUE(result.blocks.empty());
}

TEST(NcBlockTest, LineRange_EndBeyondFileReadsToEnd) {
    mc::NcLexOptions options;
    options.end_line = 100;
    const mc::NcLexResult result = mc::LexNc("G00\nG01\n", options);
    EXPECT_EQ(result.blocks.size(), 2u);
}

// ---- IsIntegerAddress ----

TEST(NcBlockTest, IsIntegerAddress_Table) {
    for (const char c : std::string("GMTHDNOPLS")) EXPECT_TRUE(mc::IsIntegerAddress(c));
    for (const char c : std::string("XYZIJKFABCRUVWEQ")) {
        EXPECT_FALSE(mc::IsIntegerAddress(c)) << c;
    }
}

// ---- FormatNcWord ----

/// @brief 書式の組み合わせ: (アドレス, 値, 書式, 期待する文字列)
using FormatCase = std::tuple<char, double, mc::NcNumberFormat, std::string>;

/// @brief `FormatNcWord`の書式の組み合わせのテスト
class FormatNcWordTest : public ::testing::TestWithParam<FormatCase> {};

TEST_P(FormatNcWordTest, Format_MatchesExpected) {
    const auto& [address, value, format, expected] = GetParam();
    EXPECT_EQ(mc::FormatNcWord(address, value, format), expected);
}

INSTANTIATE_TEST_SUITE_P(
        NumberStyles, FormatNcWordTest,
        ::testing::Values(
                // 末尾の0を省き、小数点は残す (デフォルト)
                FormatCase{'X', 12.5, mc::NcNumberFormat{4}, "X12.5"},
                // 整数値でも小数点を付ける
                FormatCase{'X', 12.0, mc::NcNumberFormat{4}, "X12."},
                // 末尾の0を残す
                FormatCase{'X', 12.5, mc::NcNumberFormat{3, true}, "X12.500"},
                // 0桁・小数点なし (Fコードのデフォルト)
                FormatCase{'F', 200.0, mc::NcNumberFormat{0, false, false}, "F200"},
                // 0桁・小数点あり
                FormatCase{'F', 200.0, mc::NcNumberFormat{0}, "F200."},
                // 0桁の丸め
                FormatCase{'F', 199.6, mc::NcNumberFormat{0, false, false}, "F200"},
                // 負のゼロは符号を落とす
                FormatCase{'X', -0.00001, mc::NcNumberFormat{4}, "X0."},
                // 角度0
                FormatCase{'B', 0.0, mc::NcNumberFormat{3}, "B0."},
                // 先頭の0を省く
                FormatCase{'X', 0.5, mc::NcNumberFormat{4, false, true, false}, "X.5"},
                // 先頭の0を省く (負数)
                FormatCase{'X', -0.5, mc::NcNumberFormat{4, false, true, false}, "X-.5"},
                // 小数点を付けない・末尾の0を省く
                FormatCase{'X', 12.0, mc::NcNumberFormat{4, false, false}, "X12"},
                // 桁数での丸め
                FormatCase{'X', 1.23456, mc::NcNumberFormat{4}, "X1.2346"},
                // 負数
                FormatCase{'Y', -2.0, mc::NcNumberFormat{4}, "Y-2."},
                // ベクトル (6桁)
                FormatCase{'I', 0.7071067811865, mc::NcNumberFormat{6}, "I0.707107"},
                // G/Mの小数コードは小数部を保つ
                FormatCase{'G', 43.5, mc::NcNumberFormat{}, "G43.5"},
                FormatCase{'G', 68.2, mc::NcNumberFormat{}, "G68.2"},
                // G/Mの整数コードは先頭の0を付けない
                FormatCase{'G', 1.0, mc::NcNumberFormat{}, "G1"},
                FormatCase{'M', 6.0, mc::NcNumberFormat{}, "M6"},
                // 他の整数アドレスは書式を無視して整数
                FormatCase{'H', 12.0, mc::NcNumberFormat{4}, "H12"},
                FormatCase{'P', 500.4, mc::NcNumberFormat{4}, "P500"}));

// ---- FormatNcInteger ----

TEST(NcBlockTest, FormatNcInteger_PadsToMinimumDigits) {
    EXPECT_EQ(mc::FormatNcInteger('O', 1, 4), "O0001");
    EXPECT_EQ(mc::FormatNcInteger('N', 10, 1), "N10");
    EXPECT_EQ(mc::FormatNcInteger('N', 10, 4), "N0010");
}

TEST(NcBlockTest, FormatNcInteger_LongerValueIsNotTruncated) {
    EXPECT_EQ(mc::FormatNcInteger('O', 12345, 4), "O12345");
}

TEST(NcBlockTest, FormatNcInteger_NegativeKeepsSignAfterAddress) {
    EXPECT_EQ(mc::FormatNcInteger('P', -5, 3), "P-005");
}

TEST(NcBlockTest, FormatNcInteger_ZeroDigits) {
    EXPECT_EQ(mc::FormatNcInteger('N', 0, 0), "N0");
}
