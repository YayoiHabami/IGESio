/**
 * @file tests/models/test_scene.cpp
 * @brief Sceneクラス (セッション状態の権威オブジェクト) の検証
 * @author Yayoi Habami
 * @date 2026-05-30
 * @copyright 2026 Yayoi Habami
 *
 * 対象:
 * - Scene::Root / RootPtr (rootの共有)
 * - Scene::ActiveSelection / ActiveSelectionId (既定の選択セット)
 * - Scene::CreateSelectionSet / ActivateSelectionSet / SelectionSetIds (複数セット切替)
 * - アンカー(SelectionSet::Active) と アクティブセット(Scene) の区別
 * - Scene::Filter (ピックフィルタ既定値)
 *
 * TODO: ロックを尊重した選択(TrySelectWithLock)はP5-3で追加するため本テスト対象外.
 *       編集コンテキスト(ActiveContext)は将来用の枠のみのため検証は最小限とする.
 */
#include <gtest/gtest.h>

#include <memory>

#include "igesio/common/id_generator.h"
#include "igesio/models/assembly.h"
#include "igesio/models/scene.h"



namespace {

namespace i_mod = igesio::models;

/// @brief ダミーのID (エンティティ/Assembly相当) を生成する
/// @note Sceneの選択操作はObjectIDのみを扱うため、実体は不要
igesio::ObjectID MakeId() {
    return igesio::IDGenerator::Generate(igesio::ObjectType::kAssembly);
}

}  // namespace



// 既定では空のアクティブ選択セットを1つ持つ
TEST(SceneTest, Constructor_HasOneEmptyActiveSelection) {
    auto root = std::make_shared<i_mod::Assembly>();
    i_mod::Scene scene(root);

    EXPECT_TRUE(scene.ActiveSelection().Empty());
    EXPECT_EQ(scene.SelectionSetIds().size(), 1u);
}

// RootPtr/Root はコンストラクタに渡したrootを共有する
TEST(SceneTest, RootPtr_SharesRootWithConstructorArg) {
    auto root = std::make_shared<i_mod::Assembly>();
    i_mod::Scene scene(root);

    EXPECT_EQ(scene.RootPtr().get(), root.get());
    EXPECT_EQ(&scene.Root(), root.get());
}

// CreateSelectionSet は新規セットを追加するがアクティブは変えない
TEST(SceneTest, CreateSelectionSet_AddsSetWithoutChangingActive) {
    auto root = std::make_shared<i_mod::Assembly>();
    i_mod::Scene scene(root);

    const auto active_before = scene.ActiveSelectionId();
    const auto handle = scene.CreateSelectionSet();

    EXPECT_FALSE(handle == active_before);                 // 別ハンドル
    EXPECT_EQ(scene.ActiveSelectionId(), active_before);   // アクティブは不変
    EXPECT_EQ(scene.SelectionSetIds().size(), 2u);
}

// アクティブセットの切替で、各セットの内容が独立に保たれる
TEST(SceneTest, ActivateSelectionSet_SwitchesIndependentSets) {
    auto root = std::make_shared<i_mod::Assembly>();
    i_mod::Scene scene(root);

    const auto set1 = scene.ActiveSelectionId();
    const auto id_a = MakeId();
    scene.ActiveSelection().Select(id_a);  // set1へid_aを選択

    const auto set2 = scene.CreateSelectionSet();
    ASSERT_TRUE(scene.ActivateSelectionSet(set2));
    EXPECT_EQ(scene.ActiveSelectionId(), set2);
    EXPECT_TRUE(scene.ActiveSelection().Empty());  // set2は独立(空)

    const auto id_b = MakeId();
    scene.ActiveSelection().Select(id_b);  // set2へid_bを選択
    EXPECT_TRUE(scene.ActiveSelection().Contains(id_b));
    EXPECT_FALSE(scene.ActiveSelection().Contains(id_a));

    ASSERT_TRUE(scene.ActivateSelectionSet(set1));  // set1へ戻す
    EXPECT_TRUE(scene.ActiveSelection().Contains(id_a));
    EXPECT_FALSE(scene.ActiveSelection().Contains(id_b));
}

// アンカー(主選択)はセット毎で、アクティブセット(Scene)とは別概念である
TEST(SceneTest, Anchor_IsPerSetDistinctFromActiveSet) {
    auto root = std::make_shared<i_mod::Assembly>();
    i_mod::Scene scene(root);

    const auto id_a = MakeId();
    scene.ActiveSelection().Select(id_a);
    // アンカーはセット内で最後に選んだ要素
    ASSERT_TRUE(scene.ActiveSelection().Active().has_value());
    EXPECT_EQ(scene.ActiveSelection().Active().value(), id_a);

    // 別セットへ切替えるとアンカーも別 (空)
    const auto set2 = scene.CreateSelectionSet();
    ASSERT_TRUE(scene.ActivateSelectionSet(set2));
    EXPECT_FALSE(scene.ActiveSelection().Active().has_value());
}

// 未知のハンドルに対する切替は失敗し、アクティブは変わらない
TEST(SceneTest, ActivateSelectionSet_ReturnsFalseForUnknownHandle) {
    auto root = std::make_shared<i_mod::Assembly>();
    i_mod::Scene scene(root);

    const auto active_before = scene.ActiveSelectionId();
    EXPECT_FALSE(scene.ActivateSelectionSet(MakeId()));   // 未登録ハンドル
    EXPECT_EQ(scene.ActiveSelectionId(), active_before);  // アクティブ不変
}

// PickFilterの既定値はすべて有効
TEST(SceneTest, Filter_DefaultsToAllTrue) {
    auto root = std::make_shared<i_mod::Assembly>();
    i_mod::Scene scene(root);

    const auto& f = scene.Filter();
    EXPECT_TRUE(f.faces);
    EXPECT_TRUE(f.edges);
    EXPECT_TRUE(f.vertices);
    EXPECT_TRUE(f.bodies);
}
