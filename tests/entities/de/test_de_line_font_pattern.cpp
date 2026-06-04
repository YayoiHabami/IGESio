/**
 * @file entities/de/test_de_line_font_pattern.cpp
 * @brief entities/de/de_line_font_pattern.hのテスト
 * @author Yayoi Habami
 * @date 2026-06-04
 * @copyright 2026 Yayoi Habami
 *
 * 対象:
 * - DELineFontPatternのint値コンストラクタの値検証
 *   (有効範囲 0〜5 の外はDataFormatError)
 * - 正常系 (代表値・境界値: 0=デフォルト, 1, 5)
 *
 * TODO: ポインタ (Line Font Definition Entity参照) 経路は
 *       test_de_color.cpp (DEFieldWrapper共通機構) で代表しているため未カバー
 */
#include <gtest/gtest.h>

#include "igesio/common/errors.h"
#include "igesio/entities/de/de_line_font_pattern.h"

namespace {

namespace i_ent = igesio::entities;

}  // namespace



/**
 * 正常系
 */

// デフォルトコンストラクタはパターン未指定 (デフォルト値) を保持する
TEST(DELineFontPatternTest, DefaultConstructor) {
    i_ent::DELineFontPattern pattern;

    EXPECT_EQ(i_ent::DEFieldValueType::kDefault, pattern.GetValueType());
    EXPECT_EQ(0, pattern.GetValue());
    EXPECT_EQ(i_ent::LineFontPattern::kNoPattern, pattern.GetPattern());
}

// 境界: 最小の有効パターン値 (1 = 実線)
TEST(DELineFontPatternTest, ConstructorWithMinValidInt) {
    i_ent::DELineFontPattern pattern(1);

    EXPECT_EQ(i_ent::DEFieldValueType::kPositive, pattern.GetValueType());
    EXPECT_EQ(1, pattern.GetValue());
    EXPECT_EQ(i_ent::LineFontPattern::kSolid, pattern.GetPattern());
}

// 境界: 最大の有効パターン値 (5 = 点線)
TEST(DELineFontPatternTest, ConstructorWithMaxValidInt) {
    i_ent::DELineFontPattern pattern(5);

    EXPECT_EQ(i_ent::DEFieldValueType::kPositive, pattern.GetValueType());
    EXPECT_EQ(5, pattern.GetValue());
    EXPECT_EQ(i_ent::LineFontPattern::kDotted, pattern.GetPattern());
}

// 境界: 0はデフォルト値 (パターン未指定) として扱われ、例外を投げない
TEST(DELineFontPatternTest, ConstructorWithZeroIsDefault) {
    i_ent::DELineFontPattern pattern(0);

    EXPECT_EQ(i_ent::DEFieldValueType::kDefault, pattern.GetValueType());
    EXPECT_EQ(0, pattern.GetValue());
    EXPECT_EQ(i_ent::LineFontPattern::kNoPattern, pattern.GetPattern());
}

// 列挙値を指定するコンストラクタ
TEST(DELineFontPatternTest, ConstructorWithEnum) {
    i_ent::DELineFontPattern pattern(i_ent::LineFontPattern::kDashed);

    EXPECT_EQ(i_ent::DEFieldValueType::kPositive, pattern.GetValueType());
    EXPECT_EQ(2, pattern.GetValue());
    EXPECT_EQ(i_ent::LineFontPattern::kDashed, pattern.GetPattern());
}



/**
 * エラー系
 */

// 境界: 有効範囲のすぐ外 (-1, 6) はDataFormatErrorを投げる
TEST(DELineFontPatternTest, Constructor_ThrowsDataFormatErrorWhenOutOfRange) {
    EXPECT_THROW(i_ent::DELineFontPattern pattern(-1), igesio::DataFormatError);
    EXPECT_THROW(i_ent::DELineFontPattern pattern(6), igesio::DataFormatError);
}
