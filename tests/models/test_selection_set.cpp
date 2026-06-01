/**
 * @file tests/models/test_selection_set.cpp
 * @brief models::SelectionSet のテスト (GUI非依存・ヘッドレス)
 * @author Yayoi Habami
 * @date 2026-05-29
 * @copyright 2026 Yayoi Habami
 *
 * 対象: Select / Deselect / Toggle / Replace / Clear / Contains / Items /
 *       Size / Empty / Active / Version
 * TODO: マルチスレッドからの同時操作は対象外 (単一スレッド前提)
 */
#include <gtest/gtest.h>

#include "igesio/common/id_generator.h"
#include "igesio/models/selection_set.h"



namespace {

using igesio::ObjectID;
using igesio::models::SelectionSet;

/// @brief テスト用に一意なObjectIDを生成する
ObjectID MakeId() {
    return igesio::IDGenerator::Generate(igesio::ObjectType::kAssembly);
}

}  // namespace



// 選択するとContains/Size/Active/Versionが更新される
TEST(SelectionSetTest, Select_AddsAndSetsActive) {
    SelectionSet s;
    EXPECT_TRUE(s.Empty());
    const auto v0 = s.Version();
    const auto id = MakeId();

    s.Select(id);

    EXPECT_TRUE(s.Contains(id));
    EXPECT_EQ(s.Size(), 1u);
    EXPECT_FALSE(s.Empty());
    ASSERT_TRUE(s.Active().has_value());
    EXPECT_EQ(*s.Active(), id);
    EXPECT_GT(s.Version(), v0);
}

// 複数選択ではactiveが最後に選んだ要素になる
TEST(SelectionSetTest, Select_ActiveIsLastSelected) {
    SelectionSet s;
    const auto a = MakeId();
    const auto b = MakeId();
    s.Select(a);
    s.Select(b);

    EXPECT_EQ(s.Size(), 2u);
    EXPECT_TRUE(s.Contains(a));
    ASSERT_TRUE(s.Active().has_value());
    EXPECT_EQ(*s.Active(), b);
}

// Deselectで除去され、activeが対象なら解除される
TEST(SelectionSetTest, Deselect_RemovesAndClearsActiveWhenMatch) {
    SelectionSet s;
    const auto id = MakeId();
    s.Select(id);

    s.Deselect(id);

    EXPECT_FALSE(s.Contains(id));
    EXPECT_TRUE(s.Empty());
    EXPECT_FALSE(s.Active().has_value());
}

// 未選択IDのDeselectは何もしない (versionも変えない)
TEST(SelectionSetTest, Deselect_NoOpWhenNotSelected) {
    SelectionSet s;
    const auto id = MakeId();
    const auto v0 = s.Version();

    s.Deselect(id);

    EXPECT_EQ(s.Version(), v0);
    EXPECT_TRUE(s.Empty());
}

// Toggleは選択/非選択を反転する
TEST(SelectionSetTest, Toggle_FlipsMembership) {
    SelectionSet s;
    const auto id = MakeId();

    s.Toggle(id);
    EXPECT_TRUE(s.Contains(id));

    s.Toggle(id);
    EXPECT_FALSE(s.Contains(id));
}

// Replaceは既存をクリアして単一選択にする
TEST(SelectionSetTest, Replace_ClearsThenSelectsSingle) {
    SelectionSet s;
    s.Select(MakeId());
    s.Select(MakeId());
    const auto id = MakeId();

    s.Replace(id);

    EXPECT_EQ(s.Size(), 1u);
    EXPECT_TRUE(s.Contains(id));
    ASSERT_TRUE(s.Active().has_value());
    EXPECT_EQ(*s.Active(), id);
}

// Clearは全解除しactiveも消す
TEST(SelectionSetTest, Clear_RemovesAllAndActive) {
    SelectionSet s;
    s.Select(MakeId());
    s.Select(MakeId());

    s.Clear();

    EXPECT_TRUE(s.Empty());
    EXPECT_EQ(s.Size(), 0u);
    EXPECT_FALSE(s.Active().has_value());
}

// 空集合に対するClearはversionを変えない
TEST(SelectionSetTest, Clear_NoOpWhenEmpty) {
    SelectionSet s;
    const auto v0 = s.Version();

    s.Clear();

    EXPECT_EQ(s.Version(), v0);
}
