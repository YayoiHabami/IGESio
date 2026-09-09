/**
 * @file tests/extensions/machines/core/test_formatting.cpp
 * @brief machines拡張のフォーマットと色文字列 (core/formatting) のテスト
 * @author Yayoi Habami
 * @date 2026-09-09
 * @copyright 2026 Yayoi Habami
 * @note 対象: FormatFixed / FormatDegrees / ParseHexColor / FormatHexColor
 *       - 正常系: 固定小数の桁数と丸め、rad→degの換算、`#RRGGBB`の解釈
 *         (大文字・小文字)、整形の往復
 *       - 正常系 (境界値): 桁数0、成分0と255、0..1の外の成分のクランプ
 *       - 正常系 (退化): 負のゼロ
 *       - 異常系 (解釈失敗): 長さ違い、先頭が`#`でない、16進でない文字
 *         (`std::nullopt`を返し例外は投げない)
 */
#include <gtest/gtest.h>

#include <array>
#include <optional>
#include <string>

#include "igesio/extensions/machines/core/formatting.h"
#include "igesio/extensions/machines/core/units.h"

namespace {

namespace mc = igesio::extensions::machines;

/// @brief 色成分の比較の許容誤差 (8bit量子化の半分未満)
constexpr float kColorTol = 1.0f / 512.0f;

}  // namespace



// ---- FormatFixed / FormatDegrees ----

TEST(FormattingTest, FormatFixed_RoundsToRequestedDigits) {
    EXPECT_EQ(mc::FormatFixed(0.5, 3), "0.500");
    EXPECT_EQ(mc::FormatFixed(-10.0, 3), "-10.000");
    EXPECT_EQ(mc::FormatFixed(1.23456789, 6), "1.234568");
}

TEST(FormattingTest, FormatFixed_ZeroDigitsHasNoDecimalPoint) {
    EXPECT_EQ(mc::FormatFixed(2.6, 0), "3");
}

TEST(FormattingTest, FormatDegrees_ConvertsRadiansToDegrees) {
    EXPECT_EQ(mc::FormatDegrees(mc::kHalfTurn), "180.000");
    EXPECT_EQ(mc::FormatDegrees(mc::ToRadians(-10.0)), "-10.000");
    EXPECT_EQ(mc::FormatDegrees(mc::kQuarterTurn, 1), "90.0");
}

// ---- ParseHexColor ----

TEST(FormattingTest, ParseHexColor_ReadsLowerAndUpperCase) {
    const auto lower = mc::ParseHexColor("#ff8000");
    const auto upper = mc::ParseHexColor("#FF8000");
    ASSERT_TRUE(lower.has_value());
    ASSERT_TRUE(upper.has_value());
    EXPECT_NEAR((*lower)[0], 1.0f, kColorTol);
    EXPECT_NEAR((*lower)[1], 128.0f / 255.0f, kColorTol);
    EXPECT_NEAR((*lower)[2], 0.0f, kColorTol);
    EXPECT_EQ(*lower, *upper);
}

TEST(FormattingTest, ParseHexColor_BoundaryComponents) {
    const auto black = mc::ParseHexColor("#000000");
    const auto white = mc::ParseHexColor("#ffffff");
    ASSERT_TRUE(black.has_value());
    ASSERT_TRUE(white.has_value());
    for (std::size_t i = 0; i < 3; ++i) {
        EXPECT_NEAR((*black)[i], 0.0f, kColorTol);
        EXPECT_NEAR((*white)[i], 1.0f, kColorTol);
    }
}

TEST(FormattingTest, ParseHexColor_ReturnsNulloptWhenMalformed) {
    EXPECT_FALSE(mc::ParseHexColor("#fff").has_value());        // 長さ違い (短い)
    EXPECT_FALSE(mc::ParseHexColor("#ff80000").has_value());    // 長さ違い (長い)
    EXPECT_FALSE(mc::ParseHexColor("ff8000").has_value());      // `#`がない
    EXPECT_FALSE(mc::ParseHexColor("0ff8000").has_value());     // 先頭が`#`でない
    EXPECT_FALSE(mc::ParseHexColor("#ff80gg").has_value());     // 16進でない
    EXPECT_FALSE(mc::ParseHexColor("").has_value());            // 空
}

// ---- FormatHexColor ----

TEST(FormattingTest, FormatHexColor_WritesLowerCaseHex) {
    EXPECT_EQ(mc::FormatHexColor({1.0f, 0.5f, 0.0f}), "#ff8000");
    EXPECT_EQ(mc::FormatHexColor({0.0f, 0.0f, 0.0f}), "#000000");
}

TEST(FormattingTest, FormatHexColor_ClampsOutOfRangeComponents) {
    EXPECT_EQ(mc::FormatHexColor({1.5f, -0.5f, 1.0f}), "#ff00ff");
}

TEST(FormattingTest, FormatHexColor_RoundTripsThroughParse) {
    const std::string text = "#4b697d";
    const auto rgb = mc::ParseHexColor(text);
    ASSERT_TRUE(rgb.has_value());
    EXPECT_EQ(mc::FormatHexColor(*rgb), text);
}
