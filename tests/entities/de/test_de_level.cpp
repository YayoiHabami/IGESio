/**
 * @file entities/de/test_de_level.cpp
 * @brief entities/de/de_level.hのテスト
 * @author Yayoi Habami
 * @date 2026-06-04
 * @copyright 2026 Yayoi Habami
 *
 * 対象:
 * - DELevelのコンストラクタ・SetLevelNumberの値検証
 *   (レベル番号の負値はDataFormatError)
 * - 正常系 (代表値・境界値: 0=デフォルト, 正値)
 *
 * TODO: ポインタ (Definition Levels Property Entity参照) 経路は
 *       test_de_color.cpp (DEFieldWrapper共通機構) で代表しているため未カバー
 */
#include <gtest/gtest.h>

#include "igesio/common/errors.h"
#include "igesio/entities/de/de_level.h"

namespace {

namespace i_ent = igesio::entities;

}  // namespace



/**
 * 正常系
 */

// デフォルトコンストラクタはデフォルト値 (0) を保持する
TEST(DELevelTest, DefaultConstructor) {
    i_ent::DELevel level;

    EXPECT_EQ(i_ent::DEFieldValueType::kDefault, level.GetValueType());
    EXPECT_EQ(0, level.GetValue());
    EXPECT_EQ(0, level.GetLevelNumber());
}

// 正のレベル番号を指定するコンストラクタ
TEST(DELevelTest, ConstructorWithPositiveInt) {
    i_ent::DELevel level(5);

    EXPECT_EQ(i_ent::DEFieldValueType::kPositive, level.GetValueType());
    EXPECT_EQ(5, level.GetValue());
    EXPECT_EQ(5, level.GetLevelNumber());
}

// 境界: 0はデフォルト値として扱われ、例外を投げない
TEST(DELevelTest, ConstructorWithZeroIsDefault) {
    i_ent::DELevel level(0);

    EXPECT_EQ(i_ent::DEFieldValueType::kDefault, level.GetValueType());
    EXPECT_EQ(0, level.GetValue());
}

// SetLevelNumberで正の値を設定できる
TEST(DELevelTest, SetLevelNumber_AcceptsPositiveValue) {
    i_ent::DELevel level;
    level.SetLevelNumber(3);

    EXPECT_EQ(i_ent::DEFieldValueType::kPositive, level.GetValueType());
    EXPECT_EQ(3, level.GetLevelNumber());
}



/**
 * エラー系
 */

// 境界: 負のレベル番号はDataFormatErrorを投げる (-1は境界のすぐ外)
TEST(DELevelTest, Constructor_ThrowsDataFormatErrorWhenNegative) {
    EXPECT_THROW(i_ent::DELevel level(-1), igesio::DataFormatError);
}

// SetLevelNumberも負値でDataFormatErrorを投げる (境界: 0は許容)
TEST(DELevelTest, SetLevelNumber_ThrowsDataFormatErrorWhenNegative) {
    i_ent::DELevel level;

    EXPECT_THROW(level.SetLevelNumber(-1), igesio::DataFormatError);
    EXPECT_NO_THROW(level.SetLevelNumber(0));
}
