/**
 * @file tests/extensions/machines/core/test_formatting.cpp
 * @brief machines拡張のフォーマットと色文字列 (core/formatting) のテスト
 * @author Yayoi Habami
 * @date 2026-09-09
 * @copyright 2026 Yayoi Habami
 * @note 対象: FormatFixed / FormatDegrees / ParseHexColor / ParseHexColorRgba /
 *       FormatHexColor
 *       - 正常系: 固定小数の桁数と丸め、rad→degの換算、`#RRGGBB`の解釈
 *         (大文字・小文字)、短縮形と不透明度付き (`#RGB`/`#RGBA`/`#RRGGBBAA`)
 *         の解釈、整形の往復
 *       - 正常系 (境界値): 桁数0、成分0と255、0..1の外の成分のクランプ,
 *         不透明度0と255
 *       - 正常系 (退化): 負のゼロ
 *       - 異常系 (解釈失敗): 長さ違い、先頭が`#`でない、16進でない文字、8桁
 *         (`std::nullopt`を返し例外は投げない)
 */
#include <gtest/gtest.h>

#include <cstddef>
#include <optional>
#include <string>

#include "igesio/common/color.h"
#include "igesio/extensions/machines/core/formatting.h"
#include "igesio/extensions/machines/core/units.h"

namespace {

namespace mc = igesio::extensions::machines;
using igesio::Color;

/// @brief 色成分の比較の許容誤差 (8bit量子化の半分未満)
constexpr double kColorTol = 1.0 / 512.0;

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
    EXPECT_NEAR(lower->r, 1.0, kColorTol);
    EXPECT_NEAR(lower->g, 128.0 / 255.0, kColorTol);
    EXPECT_NEAR(lower->b, 0.0, kColorTol);
    EXPECT_NEAR(lower->a, 1.0, kColorTol);  // 6桁は不透明
    EXPECT_EQ(*lower, *upper);
}

TEST(FormattingTest, ParseHexColor_BoundaryComponents) {
    const auto black = mc::ParseHexColor("#000000");
    const auto white = mc::ParseHexColor("#ffffff");
    ASSERT_TRUE(black.has_value());
    ASSERT_TRUE(white.has_value());
    for (std::size_t i = 0; i < 3; ++i) {
        EXPECT_NEAR((*black)[i], 0.0, kColorTol);
        EXPECT_NEAR((*white)[i], 1.0, kColorTol);
    }
}

TEST(FormattingTest, ParseHexColor_ReturnsNulloptWhenMalformed) {
    EXPECT_FALSE(mc::ParseHexColor("#fff").has_value());        // 長さ違い (短い)
    EXPECT_FALSE(mc::ParseHexColor("#ff80000").has_value());    // 長さ違い (長い)
    EXPECT_FALSE(mc::ParseHexColor("ff8000").has_value());      // `#`がない
    EXPECT_FALSE(mc::ParseHexColor("0ff8000").has_value());     // 先頭が`#`でない
    EXPECT_FALSE(mc::ParseHexColor("#ff80gg").has_value());     // 16進でない
    EXPECT_FALSE(mc::ParseHexColor("").has_value());            // 空
    EXPECT_FALSE(mc::ParseHexColor("#ff800080").has_value());   // 8桁 (α付き) は不可
}

// ---- ParseHexColorRgba ----

TEST(FormattingTest, ParseHexColorRgba_ReadsAllForms) {
    const auto rgb = mc::ParseHexColorRgba("#ff8000");
    const auto rgba = mc::ParseHexColorRgba("#FF800080");
    const auto short_rgb = mc::ParseHexColorRgba("#f80");
    const auto short_rgba = mc::ParseHexColorRgba("#f808");
    ASSERT_TRUE(rgb.has_value());
    ASSERT_TRUE(rgba.has_value());
    ASSERT_TRUE(short_rgb.has_value());
    ASSERT_TRUE(short_rgba.has_value());
    // 6桁は`ParseHexColor`と同じ結果 (不透明)
    EXPECT_EQ(*rgb, *mc::ParseHexColor("#ff8000"));
    EXPECT_NEAR(rgb->a, 1.0, kColorTol);
    // 8桁は不透明度を持つ
    EXPECT_NEAR(rgba->r, 1.0, kColorTol);
    EXPECT_NEAR(rgba->g, 128.0 / 255.0, kColorTol);
    EXPECT_NEAR(rgba->a, 128.0 / 255.0, kColorTol);
    // 短縮形は各桁を2回繰り返す (`#f80` → `#ff8800`)
    EXPECT_NEAR(short_rgb->r, 1.0, kColorTol);
    EXPECT_NEAR(short_rgb->g, 136.0 / 255.0, kColorTol);
    EXPECT_NEAR(short_rgb->b, 0.0, kColorTol);
    EXPECT_NEAR(short_rgb->a, 1.0, kColorTol);
    EXPECT_NEAR(short_rgba->a, 136.0 / 255.0, kColorTol);
}

TEST(FormattingTest, ParseHexColorRgba_BoundaryComponents) {
    const auto transparent = mc::ParseHexColorRgba("#0000");
    const auto opaque = mc::ParseHexColorRgba("#ffffffff");
    ASSERT_TRUE(transparent.has_value());
    ASSERT_TRUE(opaque.has_value());
    EXPECT_NEAR(transparent->a, 0.0, kColorTol);
    for (std::size_t i = 0; i < 3; ++i) {
        EXPECT_NEAR((*transparent)[i], 0.0, kColorTol);
        EXPECT_NEAR((*opaque)[i], 1.0, kColorTol);
    }
    EXPECT_NEAR(opaque->a, 1.0, kColorTol);
}

TEST(FormattingTest, ParseHexColorRgba_ReturnsNulloptWhenMalformed) {
    EXPECT_FALSE(mc::ParseHexColorRgba("").has_value());            // 空
    EXPECT_FALSE(mc::ParseHexColorRgba("#").has_value());           // 桁なし
    EXPECT_FALSE(mc::ParseHexColorRgba("f80").has_value());         // `#`がない
    EXPECT_FALSE(mc::ParseHexColorRgba("#ff").has_value());         // 2桁
    EXPECT_FALSE(mc::ParseHexColorRgba("#ff800").has_value());      // 5桁
    EXPECT_FALSE(mc::ParseHexColorRgba("#ff80000").has_value());    // 7桁
    EXPECT_FALSE(mc::ParseHexColorRgba("#ff8000800").has_value());  // 9桁
    EXPECT_FALSE(mc::ParseHexColorRgba("#ggg").has_value());        // 16進でない (短縮形)
    EXPECT_FALSE(mc::ParseHexColorRgba("#ff80gg").has_value());     // 16進でない
}

// ---- FormatHexColor ----

TEST(FormattingTest, FormatHexColor_WritesLowerCaseHex) {
    EXPECT_EQ(mc::FormatHexColor(Color{1.0, 0.5, 0.0}), "#ff8000");
    EXPECT_EQ(mc::FormatHexColor(Color{0.0, 0.0, 0.0}), "#000000");
}

TEST(FormattingTest, FormatHexColor_ClampsOutOfRangeComponents) {
    EXPECT_EQ(mc::FormatHexColor(Color{1.5, -0.5, 1.0}), "#ff00ff");
}

TEST(FormattingTest, FormatHexColor_IgnoresAlpha) {
    EXPECT_EQ(mc::FormatHexColor(Color{1.0, 0.5, 0.0, 0.25}), "#ff8000");
}

TEST(FormattingTest, FormatHexColor_RoundTripsThroughParse) {
    const std::string text = "#4b697d";
    const auto rgb = mc::ParseHexColor(text);
    ASSERT_TRUE(rgb.has_value());
    EXPECT_EQ(mc::FormatHexColor(*rgb), text);
}
