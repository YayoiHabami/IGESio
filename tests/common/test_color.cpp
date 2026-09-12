/**
 * @file common/test_color.cpp
 * @brief common/color.h (igesio::Color) のテスト
 * @author Yayoi Habami
 * @date 2026-09-11
 * @copyright 2026 Yayoi Habami
 * @note 対象: 集成体としての構築・constexpr性、各スケール (8bit・IGES・float・
 *       16進文字列) との相互変換と往復性、飽和・丸め、派生 (WithAlpha/Clamped)・
 *       判定 (IsInUnitRange/SquaredDistanceRGB)・添字・厳密比較
 *
 * TODO: NaN成分に対するToRGB255/ToHex/Clampedの挙動は未検証
 *       (std::clampにNaNを渡した場合の結果が未規定であり、仕様未確定のため据え置き).
 *       IsInUnitRangeがNaNでfalseを返すことのみ規約として扱う.
 */
#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

#include "igesio/common/color.h"

namespace {

using igesio::Color;

/// @brief 浮動小数点比較の許容誤差
constexpr double kTol = 1e-9;

/// @brief 2つの色が各成分で許容誤差内に一致することを検査する
/// @param actual 検査する色
/// @param expected 期待する色
void ExpectColorNear(const Color& actual, const Color& expected) {
    EXPECT_NEAR(actual.r, expected.r, kTol);
    EXPECT_NEAR(actual.g, expected.g, kTol);
    EXPECT_NEAR(actual.b, expected.b, kTol);
    EXPECT_NEAR(actual.a, expected.a, kTol);
}

}  // namespace



/**
 * 構築
 */

// 既定構築は不透明な黒、3成分で構築した場合もa = 1.0
TEST(ColorTest, Aggregate_DefaultsToOpaqueBlack) {
    const Color black{};
    EXPECT_NEAR(black.r, 0.0, kTol);
    EXPECT_NEAR(black.g, 0.0, kTol);
    EXPECT_NEAR(black.b, 0.0, kTol);
    EXPECT_NEAR(black.a, 1.0, kTol);

    const Color rgb{0.1, 0.2, 0.3};
    EXPECT_NEAR(rgb.r, 0.1, kTol);
    EXPECT_NEAR(rgb.g, 0.2, kTol);
    EXPECT_NEAR(rgb.b, 0.3, kTol);
    EXPECT_NEAR(rgb.a, 1.0, kTol);
}

// 定数式で構築・変換・比較できる (描画既定色等をconstexpr定数にするため)
TEST(ColorTest, Constexpr_UsableInConstantExpressions) {
    constexpr Color c{1.0, 0.6, 0.0};
    static_assert(c.r == 1.0);
    static_assert(c.a == 1.0);
    static_assert(Color::FromRGB255(255, 0, 0) == Color{1.0, 0.0, 0.0});
    static_assert(Color::FromIgesRGB(100.0, 0.0, 0.0) == Color{1.0, 0.0, 0.0});
    static_assert(c.WithAlpha(0.5).a == 0.5);
    static_assert(c.IsInUnitRange());
    static_assert(c[1] == 0.6);
    constexpr std::array<float, 4> rgba = c.ToFloatRGBA();
    static_assert(rgba[0] == 1.0f);
    SUCCEED();
}



/**
 * 8bit (0〜255)
 */

// 全数性質テスト: 0〜255の全値で FromRGB255 → ToRGB255 が可逆
TEST(ColorTest, FromRGB255_ToRGB255_RoundTripExhaustive) {
    for (int v = 0; v <= 255; ++v) {
        const auto rgb = Color::FromRGB255(v, v, v).ToRGB255();
        ASSERT_EQ(rgb[0], v) << "v=" << v;
        ASSERT_EQ(rgb[1], v) << "v=" << v;
        ASSERT_EQ(rgb[2], v) << "v=" << v;
    }
}

// 8bit値のαも単位スケールへ変換される
TEST(ColorTest, FromRGB255_ConvertsAlpha) {
    const Color c = Color::FromRGB255(0, 0, 0, 51);
    EXPECT_NEAR(c.a, 0.2, kTol);
    EXPECT_NEAR(Color::FromRGB255(0, 0, 0).a, 1.0, kTol);
}

// 範囲検証は行わない (範囲外は単位範囲外のColorになる)
TEST(ColorTest, FromRGB255_OutOfRangeIsNotValidated) {
    EXPECT_FALSE(Color::FromRGB255(300, 0, 0).IsInUnitRange());
    EXPECT_FALSE(Color::FromRGB255(0, -1, 0).IsInUnitRange());
    EXPECT_TRUE(Color::FromRGB255(0, 0, 255).IsInUnitRange());
}

// 0.5丸め位置の値が四捨五入され、範囲外は[0, 255]に飽和する
TEST(ColorTest, ToRGB255_RoundsHalfUpAndSaturates) {
    EXPECT_EQ((Color{0.5, 1.0, 0.3}.ToRGB255()), (std::array<int, 3>{128, 255, 77}));
    EXPECT_EQ((Color{1.5, -0.1, 0.5}.ToRGB255()), (std::array<int, 3>{255, 0, 128}));
}



/**
 * IGESスケール (0〜100)
 */

// IGESスケールとの相互変換
TEST(ColorTest, FromIgesRGB_ToIgesRGB_Scale) {
    const Color c = Color::FromIgesRGB({50.0, 100.0, 30.0});
    ExpectColorNear(c, Color{0.5, 1.0, 0.3, 1.0});

    const auto iges = Color{0.5, 1.0, 0.3}.ToIgesRGB();
    EXPECT_NEAR(iges[0], 50.0, kTol);
    EXPECT_NEAR(iges[1], 100.0, kTol);
    EXPECT_NEAR(iges[2], 30.0, kTol);

    // 成分指定版と配列版は同じ結果
    EXPECT_EQ(Color::FromIgesRGB(50.0, 100.0, 30.0), c);
}



/**
 * float ([0, 1])
 */

// float → double → float は無損失
TEST(ColorTest, FromFloat_ToFloat_RoundTrip) {
    const std::array<float, 4> rgba = {0.1f, 0.2f, 0.3f, 0.4f};
    EXPECT_EQ(Color::FromFloatRGBA(rgba).ToFloatRGBA(), rgba);

    const std::array<float, 3> rgb = {0.7f, 0.8f, 0.9f};
    const Color c = Color::FromFloatRGB(rgb);
    EXPECT_EQ(c.ToFloatRGB(), rgb);
    EXPECT_NEAR(c.a, 1.0, kTol);
    EXPECT_NEAR(Color::FromFloatRGB(rgb, 0.25).a, 0.25, kTol);
}



/**
 * 16進カラーコード
 */

// "#RRGGBB"形式 (大文字) で出力される
TEST(ColorTest, ToHex_FormatsUppercaseWithHash) {
    EXPECT_EQ(Color::FromRGB255(127, 255, 76).ToHex(), "#7FFF4C");
    EXPECT_EQ((Color{}.ToHex()), "#000000");
    EXPECT_EQ((Color{1.0, 1.0, 1.0}.ToHex()), "#FFFFFF");
}

// with_alpha指定時は"#RRGGBBAA"形式で出力される
TEST(ColorTest, ToHex_WithAlpha) {
    EXPECT_EQ(Color::FromRGB255(127, 255, 76).ToHex(true), "#7FFF4CFF");
    EXPECT_EQ(Color::FromRGB255(127, 255, 76, 0).ToHex(true), "#7FFF4C00");
    // αも飽和・丸めの対象
    EXPECT_EQ((Color{0.0, 0.0, 0.0, 1.5}.ToHex(true)), "#000000FF");
}

// "#"の有無・大文字小文字・8桁を受け付ける
TEST(ColorTest, TryParseHex_AcceptsFormats) {
    const Color expected = Color::FromRGB255(127, 255, 76);
    const auto with_hash = Color::TryParseHex("#7FFF4C");
    const auto without_hash = Color::TryParseHex("7FFF4C");
    const auto lowercase = Color::TryParseHex("#7fff4c");
    ASSERT_TRUE(with_hash.has_value());
    ASSERT_TRUE(without_hash.has_value());
    ASSERT_TRUE(lowercase.has_value());
    EXPECT_EQ(*with_hash, expected);
    EXPECT_EQ(*without_hash, expected);
    EXPECT_EQ(*lowercase, expected);

    const auto with_alpha = Color::TryParseHex("#7FFF4C80");
    ASSERT_TRUE(with_alpha.has_value());
    EXPECT_EQ(*with_alpha, Color::FromRGB255(127, 255, 76, 128));
}

// 形式不正はstd::nullopt
TEST(ColorTest, TryParseHex_ReturnsNulloptWhenMalformed) {
    EXPECT_FALSE(Color::TryParseHex("#FFF").has_value());      // 3桁短縮形
    EXPECT_FALSE(Color::TryParseHex("1234567").has_value());   // 7桁
    EXPECT_FALSE(Color::TryParseHex("#GGHHII").has_value());   // 16進数字以外
    EXPECT_FALSE(Color::TryParseHex("").has_value());          // 空
    EXPECT_FALSE(Color::TryParseHex("#").has_value());         // "#"のみ
    EXPECT_FALSE(Color::TryParseHex("##7FFF4C").has_value());  // "#"の重複
}

// FromHexは形式不正でstd::invalid_argument
TEST(ColorTest, FromHex_ThrowsInvalidArgument) {
    EXPECT_EQ(Color::FromHex("#7FFF4C"), Color::FromRGB255(127, 255, 76));
    EXPECT_THROW(Color::FromHex("#FFF"), std::invalid_argument);
    EXPECT_THROW(Color::FromHex(""), std::invalid_argument);
}

// 往復性: ToHexで出力した文字列をTryParseHexで読むと同じ8bit値に戻る
TEST(ColorTest, Hex_RoundTrip) {
    const Color c = Color::FromRGB255(75, 105, 125, 200);
    const auto parsed = Color::TryParseHex(c.ToHex(true));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, c);
}



/**
 * 派生・判定
 */

// WithAlphaはRGBを保ち、Clampedは範囲外を飽和させる
TEST(ColorTest, WithAlpha_Clamped_IsInUnitRange) {
    const Color c{0.2, 0.4, 0.6, 1.0};
    EXPECT_EQ(c.WithAlpha(0.5), (Color{0.2, 0.4, 0.6, 0.5}));
    EXPECT_TRUE(c.IsInUnitRange());

    const Color out{1.5, -0.5, 0.5, 2.0};
    EXPECT_FALSE(out.IsInUnitRange());
    EXPECT_EQ(out.Clamped(), (Color{1.0, 0.0, 0.5, 1.0}));
    EXPECT_TRUE(out.Clamped().IsInUnitRange());

    // 境界値 (0.0 / 1.0ちょうど) は範囲内
    EXPECT_TRUE((Color{0.0, 0.0, 0.0, 0.0}.IsInUnitRange()));
    EXPECT_TRUE((Color{1.0, 1.0, 1.0, 1.0}.IsInUnitRange()));
}

// NaNを含む色は単位範囲内とみなさない
TEST(ColorTest, IsInUnitRange_FalseForNaN) {
    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    EXPECT_FALSE((Color{kNaN, 0.0, 0.0, 1.0}.IsInUnitRange()));
    EXPECT_FALSE((Color{0.0, 0.0, 0.0, kNaN}.IsInUnitRange()));
}

// RGB距離はαを無視する
TEST(ColorTest, SquaredDistanceRGB_IgnoresAlpha) {
    const Color a{0.0, 0.0, 0.0, 1.0};
    const Color b{0.0, 0.0, 0.0, 0.0};
    EXPECT_NEAR(a.SquaredDistanceRGB(b), 0.0, kTol);

    const Color red{1.0, 0.0, 0.0};
    const Color green{0.0, 1.0, 0.0};
    EXPECT_NEAR(red.SquaredDistanceRGB(green), 2.0, kTol);
    EXPECT_NEAR(red.SquaredDistanceRGB(Color{}), 1.0, kTol);
}

// 添字アクセスと範囲外の例外
TEST(ColorTest, IndexOperator_ThrowsOutOfRange) {
    const Color c{0.1, 0.2, 0.3, 0.4};
    EXPECT_NEAR(c[0], 0.1, kTol);
    EXPECT_NEAR(c[1], 0.2, kTol);
    EXPECT_NEAR(c[2], 0.3, kTol);
    EXPECT_NEAR(c[3], 0.4, kTol);
    EXPECT_THROW(c[4], std::out_of_range);
}

// 厳密比較
TEST(ColorTest, Equality_IsExact) {
    const Color a{0.1, 0.2, 0.3, 0.4};
    EXPECT_TRUE(a == Color(a));
    EXPECT_FALSE(a != Color(a));
    EXPECT_TRUE(a != (Color{0.1, 0.2, 0.3, 0.5}));
    EXPECT_FALSE(a == (Color{0.1, 0.2, 0.3, 0.5}));
    // 微小なずれも不一致 (近似比較ではない)
    EXPECT_NE((Color{0.1 + 1e-15, 0.2, 0.3, 0.4}), a);
    EXPECT_NE((Color{0.1, 0.2, 0.3, 0.4 + 1e-15}), a);
}
