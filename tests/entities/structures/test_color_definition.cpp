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
 *
 * TODO: Color Number が 8 を超える値（範囲外）のケースは、DEColor の構築時点で
 *       igesio::DataFormatError を送出する既存挙動のため、本エンティティの正規化対象外。
 *       >8 の正規化を行う場合は別途 DEColor/EntityBase 側の対応が必要。
 * TODO: Color Number が負値（Color Definition への参照ポインタ）のケースは
 *       Type 314 では非現実的なため未カバー。
 */
#include <gtest/gtest.h>

#include <array>
#include <string>

#include "igesio/entities/de/de_color.h"
#include "igesio/entities/de/raw_entity_de.h"
#include "igesio/entities/structures/color_definition.h"

namespace {

namespace i_ent = igesio::entities;
using ColorDefinition = i_ent::ColorDefinition;

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
