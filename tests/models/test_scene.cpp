/**
 * @file tests/models/test_scene.cpp
 * @brief Sceneクラス (セッション状態を一元管理するオブジェクト) の検証
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
 * - Assembly::IsEffectivelySelectable / Scene::TrySelectWithLock (lock/フィルタ尊重)
 * - Scene::SelectOwningAssembly (所有Assemblyのメンバ一括選択)
 * - Scene::DeselectOwningAssembly (所有Assemblyのメンバ一括解除; グループトグル用)
 * - Scene::Granularity / SetGranularity (選択粒度の既定値と切替)
 *
 * TODO: 編集コンテキスト(ActiveContext)は将来用の枠のみのため検証は最小限とする.
 *       サブエンティティ選択(faces/edges/vertices)は将来課題のため対象外.
 */
#include <gtest/gtest.h>

#include <memory>

#include "igesio/common/id_generator.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/entities/curves/circular_arc.h"
#include "igesio/models/assembly.h"
#include "igesio/models/scene.h"



namespace {

namespace i_mod = igesio::models;
namespace i_ent = igesio::entities;

/// @brief ダミーのID (エンティティ/Assembly相当) を生成する
/// @note Sceneの選択操作はObjectIDのみを扱うため、実体は不要
igesio::ObjectID MakeId() {
    return igesio::IDGenerator::Generate(igesio::ObjectType::kAssembly);
}

/// @brief 単純な円弧を生成する (ロック判定用の実エンティティ)
std::shared_ptr<i_ent::CircularArc> MakeArc() {
    return std::make_shared<i_ent::CircularArc>(
        igesio::Vector2d(0.0, 0.0), igesio::Vector2d(1.0, 0.0),
        igesio::Vector2d(0.0, 1.0), 0.0);
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



// 祖先がロックされていなければ実効的に選択可能
TEST(SceneTest, IsEffectivelySelectable_TrueWhenUnlocked) {
    auto root = std::make_shared<i_mod::Assembly>();
    auto child = std::make_shared<i_mod::Assembly>();
    root->AddChildAssembly(child);
    auto arc = MakeArc();
    child->AddEntity(arc);

    EXPECT_TRUE(root->IsEffectivelySelectable(arc->GetID()));
}

// 所有ノードまたは祖先がロックされていれば選択不可
TEST(SceneTest, IsEffectivelySelectable_FalseWhenAncestorLocked) {
    auto root = std::make_shared<i_mod::Assembly>();
    auto child = std::make_shared<i_mod::Assembly>();
    root->AddChildAssembly(child);
    auto arc = MakeArc();
    child->AddEntity(arc);
    const auto id = arc->GetID();

    child->Metadata().lock.selectable = false;  // 所有ノードをロック
    EXPECT_FALSE(root->IsEffectivelySelectable(id));

    child->Metadata().lock.selectable = true;
    root->Metadata().lock.selectable = false;   // 祖先(root)をロック
    EXPECT_FALSE(root->IsEffectivelySelectable(id));
}

// TrySelectWithLock: 非ロック要素は選択され、trueを返す
TEST(SceneTest, TrySelectWithLock_SelectsWhenUnlocked) {
    auto root = std::make_shared<i_mod::Assembly>();
    auto arc = MakeArc();
    root->AddEntity(arc);
    i_mod::Scene scene(root);

    EXPECT_TRUE(scene.TrySelectWithLock(scene.ActiveSelection(), arc->GetID()));
    EXPECT_TRUE(scene.ActiveSelection().Contains(arc->GetID()));
}

// TrySelectWithLock: ロック要素は拒否されるが、SelectionSet直呼びは迂回できる
TEST(SceneTest, TrySelectWithLock_RejectsWhenAncestorLocked) {
    auto root = std::make_shared<i_mod::Assembly>();
    auto child = std::make_shared<i_mod::Assembly>();
    root->AddChildAssembly(child);
    auto arc = MakeArc();
    child->AddEntity(arc);
    child->Metadata().lock.selectable = false;
    i_mod::Scene scene(root);
    const auto id = arc->GetID();

    EXPECT_FALSE(scene.TrySelectWithLock(scene.ActiveSelection(), id));
    EXPECT_FALSE(scene.ActiveSelection().Contains(id));

    // ヘッドレスな直接選択はロックを無視できる (SelectionSetは純粋)
    scene.ActiveSelection().Select(id);
    EXPECT_TRUE(scene.ActiveSelection().Contains(id));
}

// TrySelectWithLock: bodiesフィルタが無効なら拒否する
TEST(SceneTest, TrySelectWithLock_RejectsWhenBodiesFilterDisabled) {
    auto root = std::make_shared<i_mod::Assembly>();
    auto arc = MakeArc();
    root->AddEntity(arc);
    i_mod::Scene scene(root);
    scene.Filter().bodies = false;

    EXPECT_FALSE(scene.TrySelectWithLock(scene.ActiveSelection(), arc->GetID()));
    EXPECT_FALSE(scene.ActiveSelection().Contains(arc->GetID()));
}



// SelectOwningAssembly: 所有ノードのメンバのみ一括選択し、所有AssemblyのIDを返す
TEST(SceneTest, SelectOwningAssembly_SelectsMembersOfOwningNode) {
    auto root = std::make_shared<i_mod::Assembly>();
    auto child = std::make_shared<i_mod::Assembly>();
    root->AddChildAssembly(child);
    auto arc_root = MakeArc();  // root直接所有 (所有ノードの外)
    auto arc_a = MakeArc();
    auto arc_b = MakeArc();
    root->AddEntity(arc_root);
    child->AddEntity(arc_a);
    child->AddEntity(arc_b);
    i_mod::Scene scene(root);

    const auto owner = scene.SelectOwningAssembly(
            scene.ActiveSelection(), arc_a->GetID());
    ASSERT_TRUE(owner.has_value());
    EXPECT_EQ(owner.value(), child->GetID());        // 所有ノードはchild
    const auto& sel = scene.ActiveSelection();
    EXPECT_TRUE(sel.Contains(arc_a->GetID()));
    EXPECT_TRUE(sel.Contains(arc_b->GetID()));       // 同ノードの兄弟も選択
    EXPECT_FALSE(sel.Contains(arc_root->GetID()));   // 所有ノード外は非選択
}

// SelectOwningAssembly: ロックされた子ノードのメンバはスキップする
TEST(SceneTest, SelectOwningAssembly_SkipsLockedMembers) {
    auto root = std::make_shared<i_mod::Assembly>();
    auto locked = std::make_shared<i_mod::Assembly>();
    root->AddChildAssembly(locked);
    auto arc_a = MakeArc();  // root直下 (選択可)
    auto arc_b = MakeArc();  // lockedノード内 (選択不可)
    root->AddEntity(arc_a);
    locked->AddEntity(arc_b);
    locked->Metadata().lock.selectable = false;
    i_mod::Scene scene(root);

    // arc_aをピック -> 所有ノードはroot -> 全子孫(arc_a, arc_b)が対象
    const auto owner = scene.SelectOwningAssembly(
            scene.ActiveSelection(), arc_a->GetID());
    ASSERT_TRUE(owner.has_value());
    EXPECT_EQ(owner.value(), root->GetID());
    const auto& sel = scene.ActiveSelection();
    EXPECT_TRUE(sel.Contains(arc_a->GetID()));    // 選択可
    EXPECT_FALSE(sel.Contains(arc_b->GetID()));   // ロックでスキップ
}

// SelectOwningAssembly: ツリーに無いIDは nullopt を返し、選択は変化しない
TEST(SceneTest, SelectOwningAssembly_ReturnsNulloptForUnknownId) {
    auto root = std::make_shared<i_mod::Assembly>();
    auto arc = MakeArc();
    root->AddEntity(arc);
    i_mod::Scene scene(root);

    const auto owner = scene.SelectOwningAssembly(
            scene.ActiveSelection(), MakeId());
    EXPECT_FALSE(owner.has_value());
    EXPECT_TRUE(scene.ActiveSelection().Empty());
}

// DeselectOwningAssembly: 所有ノードのメンバのみ一括解除し、他ノードは残す
TEST(SceneTest, DeselectOwningAssembly_DeselectsMembersOfOwningNode) {
    auto root = std::make_shared<i_mod::Assembly>();
    auto child = std::make_shared<i_mod::Assembly>();
    root->AddChildAssembly(child);
    auto arc_root = MakeArc();  // root直接所有 (所有ノードの外)
    auto arc_a = MakeArc();
    auto arc_b = MakeArc();
    root->AddEntity(arc_root);
    child->AddEntity(arc_a);
    child->AddEntity(arc_b);
    i_mod::Scene scene(root);
    auto& sel = scene.ActiveSelection();
    // 全体を選択しておく (root所有 = 全子孫)
    scene.SelectOwningAssembly(sel, arc_root->GetID());
    ASSERT_TRUE(sel.Contains(arc_a->GetID()));
    ASSERT_TRUE(sel.Contains(arc_root->GetID()));

    const auto owner = scene.DeselectOwningAssembly(sel, arc_a->GetID());
    ASSERT_TRUE(owner.has_value());
    EXPECT_EQ(owner.value(), child->GetID());      // 所有ノードはchild
    EXPECT_FALSE(sel.Contains(arc_a->GetID()));    // childのメンバは解除
    EXPECT_FALSE(sel.Contains(arc_b->GetID()));
    EXPECT_TRUE(sel.Contains(arc_root->GetID()));  // 所有ノード外は残る
}

// DeselectOwningAssembly: ツリーに無いIDは nullopt を返し、選択は変化しない
TEST(SceneTest, DeselectOwningAssembly_ReturnsNulloptForUnknownId) {
    auto root = std::make_shared<i_mod::Assembly>();
    auto arc = MakeArc();
    root->AddEntity(arc);
    i_mod::Scene scene(root);
    scene.ActiveSelection().Select(arc->GetID());

    const auto owner = scene.DeselectOwningAssembly(
            scene.ActiveSelection(), MakeId());
    EXPECT_FALSE(owner.has_value());
    EXPECT_TRUE(scene.ActiveSelection().Contains(arc->GetID()));  // 変化なし
}

// 選択粒度の既定はbody単位で、SetGranularityで切り替えられる
TEST(SceneTest, Granularity_DefaultsToBodyAndIsSettable) {
    auto root = std::make_shared<i_mod::Assembly>();
    i_mod::Scene scene(root);

    EXPECT_EQ(scene.Granularity(), i_mod::SelectionGranularity::kBody);
    scene.SetGranularity(i_mod::SelectionGranularity::kAssembly);
    EXPECT_EQ(scene.Granularity(), i_mod::SelectionGranularity::kAssembly);
}
