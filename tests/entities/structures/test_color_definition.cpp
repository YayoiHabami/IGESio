/**
 * @file entities/structures/test_color_definition.cpp
 * @brief ColorDefinition (Type 314) エンティティのテスト
 * @author Yayoi Habami
 * @date 2026-05-29
 * @copyright 2026 Yayoi Habami
 *
 * 対象:
 * - ColorDefinition::ColorDefinition(RawEntityDE, IGESParameterVector, ...)
 *   （読み込み用コンストラクタ）における Color Number の正規化挙動
 * - ファクトリ関数: MakeColorDefinition / MakeColorDefinitionFromRGB255 /
 *   MakeColorDefinitionFromHex
 * - セッター: SetRGB (DE Color Number更新の不変条件含む) / SetColorName
 * - 変換・問い合わせ: GetRGB255 / GetHexCode / GetClosestColorNumber
 *
 * TODO: Color Number が 8 を超える値（範囲外）のケースは、DEColor の構築時点で
 *       igesio::DataFormatError を送出する既存挙動のため、本エンティティの正規化対象外。
 *       >8 の正規化を行う場合は別途 DEColor/EntityBase 側の対応が必要。
 * TODO: Color Number が負値（Color Definition への参照ポインタ）のケースは
 *       Type 314 では非現実的なため未カバー。
 * TODO: NaN成分の扱いは未検証（ValidatePDの範囲比較はNaNを検出しない。
 *       仕様未確定のため据え置き）。
 */
#include <gtest/gtest.h>

#include <array>
#include <stdexcept>
#include <string>

#include "igesio/common/errors.h"
#include "igesio/entities/de/de_color.h"
#include "igesio/entities/de/raw_entity_de.h"
#include "igesio/entities/structures/color_definition.h"

namespace {

namespace i_ent = igesio::entities;
using ColorDefinition = i_ent::ColorDefinition;
/// @brief 浮動小数点比較の許容誤差
constexpr double kTol = 1e-9;

/// @brief 指定した Color Number を持つ Type 314 の DE レコードを生成する
/// @param color_number DE フィールド13 (Color Number) に設定する値
/// @return ColorDefinition 用の RawEntityDE
i_ent::RawEntityDE MakeColorDE(const int color_number) {
    auto de = i_ent::RawEntityDE::ByDefault(i_ent::EntityType::kColorDefinition, 0);
    de.color_number = color_number;
    return de;
}

/// @brief 標準色「青」に厳密一致する RGB の PD パラメータ (0.0-100.0スケール)
/// @note GetClosestStandardColor がこの値に対し kBlue を返すことを利用する
const igesio::IGESParameterVector kBluePD{0.0, 0.0, 100.0};

}  // namespace



/**
 * Color Number 正規化のテスト
 */

// 正常系 (退化ケース): Color Number が 0/未指定の場合、定義RGBに最も近い標準色へ
// 正規化され、検証を通過する
TEST(ColorDefinitionTest, ColorNumber_NormalizedToClosestStandardColorWhenZero) {
    ColorDefinition color(MakeColorDE(0), kBluePD);

    // 0/未指定は仕様違反のため、最も近い標準色 (青=kBlue) へ正規化される
    EXPECT_EQ(color.GetColor().GetValue(),
              static_cast<int>(i_ent::ColorNumber::kBlue));
    // 正規化後は DE 検証 (1〜8 必須) を通過する
    const auto result = color.Validate();
    EXPECT_TRUE(result.is_valid) << result.Message();
}

// 正常系 (代表値): Color Number が既に妥当 (1〜8) な場合は変更しない
TEST(ColorDefinitionTest, ColorNumber_UnchangedWhenAlreadyValid) {
    constexpr int kValidColor = static_cast<int>(i_ent::ColorNumber::kGreen);  // 3
    ColorDefinition color(MakeColorDE(kValidColor), kBluePD);

    EXPECT_EQ(color.GetColor().GetValue(), kValidColor);
    const auto result = color.Validate();
    EXPECT_TRUE(result.is_valid) << result.Message();
}

// 正常系 (境界値): Color Number が下限 (1) の場合も変更しない
TEST(ColorDefinitionTest, ColorNumber_UnchangedAtLowerBound) {
    constexpr int kBlack = static_cast<int>(i_ent::ColorNumber::kBlack);  // 1
    ColorDefinition color(MakeColorDE(kBlack), kBluePD);

    EXPECT_EQ(color.GetColor().GetValue(), kBlack);
    EXPECT_TRUE(color.Validate().is_valid);
}

// 正常系 (回帰): 白 (100,100,100) + Color Number 0 は kWhite (8) へ正規化される
// NOTE: 旧実装は最近傍標準色の探索ループがkWhiteを評価せず、kYellowに
//       正規化されていた (バグ)。本テストはその回帰を防ぐ。
TEST(ColorDefinitionTest, ColorNumber_NormalizedToWhiteWhenZero) {
    ColorDefinition color(MakeColorDE(0),
                          igesio::IGESParameterVector{100.0, 100.0, 100.0});

    EXPECT_EQ(color.GetColor().GetValue(),
              static_cast<int>(i_ent::ColorNumber::kWhite));
    EXPECT_TRUE(color.Validate().is_valid);
}



/**
 * MakeColorDefinition のテスト
 */

// 代表: RGB・色名が格納され、DEのColor Numberに最近傍標準色が設定される
TEST(MakeColorDefinitionTest, StoresRGBNameAndClosestColorNumber) {
    const auto color = i_ent::MakeColorDefinition({0.0, 0.0, 100.0}, "Pure Blue");

    const auto rgb = color->GetRGB();
    EXPECT_NEAR(rgb[0], 0.0, kTol);
    EXPECT_NEAR(rgb[1], 0.0, kTol);
    EXPECT_NEAR(rgb[2], 100.0, kTol);
    EXPECT_EQ(color->GetColorName(), "Pure Blue");
    EXPECT_EQ(color->GetColor().GetValue(),
              static_cast<int>(i_ent::ColorNumber::kBlue));
}

// name省略時は色名なし
TEST(MakeColorDefinitionTest, DefaultNameEmpty) {
    const auto color = i_ent::MakeColorDefinition({50.0, 100.0, 30.0});

    EXPECT_EQ(color->GetColorName(), "");
}

// エラー: RGB成分が[0.0, 100.0]の範囲外
TEST(MakeColorDefinitionTest, ThrowsEntityValueErrorWhenOutOfRange) {
    EXPECT_THROW(i_ent::MakeColorDefinition({101.0, 0.0, 0.0}),
                 igesio::EntityValueError);
    EXPECT_THROW(i_ent::MakeColorDefinition({0.0, -0.1, 0.0}),
                 igesio::EntityValueError);
}

// 境界精度: 0.0と100.0ちょうどは有効
TEST(MakeColorDefinitionTest, RangeBoundaries_NoThrow) {
    EXPECT_NO_THROW(i_ent::MakeColorDefinition({0.0, 0.0, 0.0}));
    EXPECT_NO_THROW(i_ent::MakeColorDefinition({100.0, 100.0, 100.0}));
}



/**
 * MakeColorDefinitionFromRGB255 のテスト
 */

// 代表: 0〜255スケールが0〜100スケールへ変換され、nameも伝搬する
TEST(MakeColorDefinitionFromRGB255Test, ConvertsScale) {
    const auto color = i_ent::MakeColorDefinitionFromRGB255(
            127, 255, 76, "Light Green");

    const auto rgb = color->GetRGB();
    EXPECT_NEAR(rgb[0], 127.0 * 100.0 / 255.0, kTol);
    EXPECT_NEAR(rgb[1], 100.0, kTol);
    EXPECT_NEAR(rgb[2], 76.0 * 100.0 / 255.0, kTol);
    EXPECT_EQ(color->GetColorName(), "Light Green");
}

// 全数性質テスト: 0-255→0-100→0-255の変換が全値で可逆
// (スケール変換の丸め誤差により非可逆になる値が存在しないことを保証する)
TEST(MakeColorDefinitionFromRGB255Test, RoundTripExhaustive) {
    for (int v = 0; v <= 255; ++v) {
        const auto color = i_ent::MakeColorDefinitionFromRGB255(v, v, v);
        const auto rgb255 = color->GetRGB255();
        ASSERT_EQ(rgb255[0], v) << "v=" << v;
        ASSERT_EQ(rgb255[1], v) << "v=" << v;
        ASSERT_EQ(rgb255[2], v) << "v=" << v;
    }
}

// エラー: 成分が[0, 255]の範囲外 (境界両側: 0/255ちょうどは有効)
TEST(MakeColorDefinitionFromRGB255Test, ThrowsInvalidArgumentWhenOutOfRange) {
    EXPECT_THROW(i_ent::MakeColorDefinitionFromRGB255(-1, 0, 0),
                 std::invalid_argument);
    EXPECT_THROW(i_ent::MakeColorDefinitionFromRGB255(0, 256, 0),
                 std::invalid_argument);
    EXPECT_NO_THROW(i_ent::MakeColorDefinitionFromRGB255(0, 0, 0));
    EXPECT_NO_THROW(i_ent::MakeColorDefinitionFromRGB255(255, 255, 255));
}



/**
 * MakeColorDefinitionFromHex のテスト
 */

// 代表: "#RRGGBB"形式のパース
TEST(MakeColorDefinitionFromHexTest, ParsesWithHash) {
    const auto color = i_ent::MakeColorDefinitionFromHex("#7FFF4C");

    EXPECT_EQ(color->GetRGB255(), (std::array<int, 3>{127, 255, 76}));
}

// "#"省略・小文字も受け付ける
TEST(MakeColorDefinitionFromHexTest, ParsesWithoutHashAndLowercase) {
    const auto color = i_ent::MakeColorDefinitionFromHex("7fff4c");

    EXPECT_EQ(color->GetRGB255(), (std::array<int, 3>{127, 255, 76}));
}

// 縮退: 黒と白
TEST(MakeColorDefinitionFromHexTest, BlackAndWhite) {
    EXPECT_EQ(i_ent::MakeColorDefinitionFromHex("#000000")->GetRGB255(),
              (std::array<int, 3>{0, 0, 0}));
    EXPECT_EQ(i_ent::MakeColorDefinitionFromHex("#FFFFFF")->GetRGB255(),
              (std::array<int, 3>{255, 255, 255}));
}

// 往復性: FromHexで作成した色のGetHexCodeは入力と一致する
TEST(MakeColorDefinitionFromHexTest, RoundTripWithGetHexCode) {
    const auto color = i_ent::MakeColorDefinitionFromHex("#7FFF4C");

    EXPECT_EQ(color->GetHexCode(), "#7FFF4C");
}

// エラー: 6桁hex以外の形式
TEST(MakeColorDefinitionFromHexTest, ThrowsInvalidArgumentWhenMalformed) {
    // 3桁短縮形は非対応
    EXPECT_THROW(i_ent::MakeColorDefinitionFromHex("#FFF"),
                 std::invalid_argument);
    // 桁数超過
    EXPECT_THROW(i_ent::MakeColorDefinitionFromHex("1234567"),
                 std::invalid_argument);
    // 16進数字以外を含む
    EXPECT_THROW(i_ent::MakeColorDefinitionFromHex("#GGHHII"),
                 std::invalid_argument);
    // 空文字列・"#"のみ
    EXPECT_THROW(i_ent::MakeColorDefinitionFromHex(""),
                 std::invalid_argument);
    EXPECT_THROW(i_ent::MakeColorDefinitionFromHex("#"),
                 std::invalid_argument);
}



/**
 * SetRGB のテスト
 */

// 代表: RGB値が更新される
TEST(ColorDefinitionSetRGBTest, UpdatesValue) {
    const auto color = i_ent::MakeColorDefinition({0.0, 0.0, 100.0});

    color->SetRGB({10.0, 20.0, 30.0});

    const auto rgb = color->GetRGB();
    EXPECT_NEAR(rgb[0], 10.0, kTol);
    EXPECT_NEAR(rgb[1], 20.0, kTol);
    EXPECT_NEAR(rgb[2], 30.0, kTol);
}

// DEのColor Numberも最近傍標準色へ更新される (構築時の不変条件を維持)
TEST(ColorDefinitionSetRGBTest, UpdatesDEColorNumber) {
    const auto color = i_ent::MakeColorDefinition({0.0, 0.0, 100.0});
    ASSERT_EQ(color->GetColor().GetValue(),
              static_cast<int>(i_ent::ColorNumber::kBlue));

    color->SetRGB({90.0, 5.0, 5.0});  // 赤系

    EXPECT_EQ(color->GetColor().GetValue(),
              static_cast<int>(i_ent::ColorNumber::kRed));
}

// エラー: 成分が[0.0, 100.0]の範囲外 (境界両側: 0.0/100.0ちょうどは有効)
TEST(ColorDefinitionSetRGBTest, ThrowsEntityValueErrorWhenOutOfRange) {
    const auto color = i_ent::MakeColorDefinition({0.0, 0.0, 100.0});

    EXPECT_THROW(color->SetRGB({-0.1, 50.0, 50.0}), igesio::EntityValueError);
    EXPECT_THROW(color->SetRGB({50.0, 50.0, 100.1}), igesio::EntityValueError);
    EXPECT_NO_THROW(color->SetRGB({0.0, 0.0, 0.0}));
    EXPECT_NO_THROW(color->SetRGB({100.0, 100.0, 100.0}));
}

// 失敗時はRGB・DEのColor Numberとも変更されない
TEST(ColorDefinitionSetRGBTest, StateUnchangedOnThrow) {
    const auto color = i_ent::MakeColorDefinition({0.0, 0.0, 100.0});

    EXPECT_THROW(color->SetRGB({150.0, 0.0, 0.0}), igesio::EntityValueError);

    const auto rgb = color->GetRGB();
    EXPECT_NEAR(rgb[0], 0.0, kTol);
    EXPECT_NEAR(rgb[1], 0.0, kTol);
    EXPECT_NEAR(rgb[2], 100.0, kTol);
    EXPECT_EQ(color->GetColor().GetValue(),
              static_cast<int>(i_ent::ColorNumber::kBlue));
}



/**
 * SetColorName のテスト
 */

// 代表: 色名が更新される
TEST(ColorDefinitionSetColorNameTest, UpdatesName) {
    const auto color = i_ent::MakeColorDefinition({0.0, 0.0, 100.0});

    color->SetColorName("Sky");

    EXPECT_EQ(color->GetColorName(), "Sky");
}

// 空文字列で色名なしとなり、PDパラメータの4番目が省略される
TEST(ColorDefinitionSetColorNameTest, EmptyOmitsPDParameter) {
    const auto color = i_ent::MakeColorDefinition({0.0, 0.0, 100.0}, "Named");
    ASSERT_EQ(color->GetParameters().size(), 4u);

    color->SetColorName("");

    EXPECT_EQ(color->GetColorName(), "");
    EXPECT_EQ(color->GetParameters().size(), 3u);
}



/**
 * GetRGB255 / GetHexCode のテスト
 */

// 0.5丸め位置の値が四捨五入される (127.5→128, 76.5→77)
TEST(ColorDefinitionConversionTest, GetRGB255_RoundsHalfUp) {
    const auto color = i_ent::MakeColorDefinition({50.0, 100.0, 30.0});

    EXPECT_EQ(color->GetRGB255(), (std::array<int, 3>{128, 255, 77}));
}

// 範囲外のRGB値 (検証なしの読み込み経路で構築) は[0, 255]に飽和する
TEST(ColorDefinitionConversionTest, GetRGB255_SaturatesOutOfRangeRGB) {
    ColorDefinition color(MakeColorDE(1),
                          igesio::IGESParameterVector{150.0, -10.0, 50.0});

    EXPECT_EQ(color.GetRGB255(), (std::array<int, 3>{255, 0, 128}));
}

// "#RRGGBB"形式 (大文字) で出力される
TEST(ColorDefinitionConversionTest, GetHexCode_FormatsUppercaseWithHash) {
    EXPECT_EQ(i_ent::MakeColorDefinitionFromRGB255(127, 255, 76)->GetHexCode(),
              "#7FFF4C");
    EXPECT_EQ(i_ent::MakeColorDefinition({0.0, 0.0, 0.0})->GetHexCode(),
              "#000000");
}



/**
 * GetClosestColorNumber のテスト
 */

// 標準8色 (kBlack〜kWhite) はすべて自分自身に写像される
// NOTE: kWhiteの写像は旧実装のバグ (探索ループがkWhiteを除外) の回帰テスト
TEST(ColorDefinitionClosestColorTest, PureStandardColors) {
    for (int i = 1; i <= 8; ++i) {
        const auto color = i_ent::MakeColorDefinition(i_ent::kColorVectors[
                static_cast<size_t>(i)]);
        EXPECT_EQ(color->GetClosestColorNumber(),
                  static_cast<i_ent::ColorNumber>(i)) << "color number " << i;
    }
}

// 標準色近傍の色は最も近い標準色へ写像される
TEST(ColorDefinitionClosestColorTest, NearColors) {
    // 白に近い灰色 (旧実装ではkYellowに写像されていた)
    EXPECT_EQ(i_ent::MakeColorDefinition({90.0, 90.0, 90.0})
                      ->GetClosestColorNumber(),
              i_ent::ColorNumber::kWhite);
    // 黒に近い灰色
    EXPECT_EQ(i_ent::MakeColorDefinition({10.0, 10.0, 10.0})
                      ->GetClosestColorNumber(),
              i_ent::ColorNumber::kBlack);
}
