/**
 * @file tests/models/test_assembly.cpp
 * @brief models/assembly.hのテスト (P1: エンティティ管理・ツリー・クエリ)
 * @author Yayoi Habami
 * @date 2026-05-29
 * @copyright 2026 Yayoi Habami
 *
 * テスト対象 (Assembly):
 *   - エンティティ管理: AddEntity / GetEntity / GetEntityCount /
 *                       AreAllReferencesSet / GetUnresolvedReferences /
 *                       IsReady / Validate
 *   - ツリー: AddChildAssembly / GetParent / GetChildAssemblies / Root /
 *             FindOwner (再インデックス)
 *   - クエリ: GetEntityIDs / FindEntitiesByType / FindEntitiesByUseFlag /
 *             FindEntities
 *
 * 座標系・ビュー・WorldBB系 (ResolvePlacement/GetCurveView/GetSurfaceView/
 * GetWorldBoundingBox) は test_assembly_coords.cpp で扱う。
 *
 * TODO: 本テストは single_rounded_cube.iges のロード済みエンティティを用いる。
 *       ロード時に参照は解決済みのため、SetPointerIfUnsetのポインタ設定分岐自体は
 *       no-opとなる。参照配線はAssemblyの未解決判定(メンバシップ条件)の観点で検証する。
 * TODO: ロード済みルートのIsReady()/Validate().is_validは、エンティティ個別の厳密検証に
 *       依存し実ファイルでは偽となりうるため、現状は要求しない(読み込み時検証の緩和=
 *       docs/todo.md v0.7.x 後に強化する)。Assembly層の責務である参照解決のみ検証する。
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "igesio/reader.h"
#include "igesio/entities/entity_base.h"
#include "igesio/models/assembly.h"
#include "igesio/models/iges_data.h"

namespace {

namespace fs = std::filesystem;
namespace i_ent = igesio::entities;
namespace i_mdl = igesio::models;
using igesio::ObjectID;
using i_ent::EntityBase;
using i_mdl::Assembly;

/// @brief テスト用IGESファイル (一辺が丸められた立方体; 124/142/144の相互参照を含む)
const std::string kCubePath =
        fs::path(__FILE__).parent_path().parent_path()
            .append("test_data").append("single_rounded_cube.iges").string();

/// @brief エンティティの共有ポインタ型
using EntityPtr = std::shared_ptr<EntityBase>;

/// @brief ロード済みエンティティのマップを平坦なベクトルへ変換する
std::vector<EntityPtr> ToVector(const Assembly& root) {
    std::vector<EntityPtr> v;
    for (const auto& [id, e] : root.GetEntities()) v.push_back(e);
    return v;
}

/// @brief 同梱の別エンティティを参照しているエンティティ(referrer)と、
///        その参照先ID(referent)の組を探す
/// @return {referrer_id, referent_id}. 見つからない場合はstd::nullopt
std::optional<std::pair<ObjectID, ObjectID>>
FindReferencingPair(const Assembly& root) {
    const auto& ents = root.GetEntities();
    for (const auto& [id, e] : ents) {
        for (const auto& rid : e->GetReferencedEntityIDs()) {
            if (ents.count(rid)) return std::make_pair(id, rid);
        }
    }
    return std::nullopt;
}

}  // namespace



/// @brief ロード済みIGESデータを共有するフィクスチャ
class AssemblyTest : public ::testing::Test {
 protected:
    /// @brief 一度だけロードするIGESデータ
    inline static std::unique_ptr<i_mdl::IgesData> data_;

    static void SetUpTestSuite() {
        if (!data_) {
            data_ = std::make_unique<i_mdl::IgesData>(igesio::ReadIges(kCubePath));
        }
    }

    /// @brief ロード済みエンティティのベクトルを取得する
    static std::vector<EntityPtr> Entities() { return ToVector(data_->Root()); }
};



/**
 * エンティティ管理
 */

// AddEntityはIDを返し、GetEntity/GetEntityCountと整合する
TEST_F(AssemblyTest, AddEntity_ReturnsIDAndStoresEntity) {
    auto ents = Entities();
    ASSERT_FALSE(ents.empty());

    Assembly asm_node;
    const auto id = asm_node.AddEntity(ents[0]);
    EXPECT_EQ(id, ents[0]->GetID());
    EXPECT_EQ(asm_node.GetEntity(id), ents[0]);
    EXPECT_EQ(asm_node.GetEntityCount(), 1u);
}

// nullptrのエンティティ追加で例外
TEST_F(AssemblyTest, AddEntity_ThrowsInvalidArgumentWhenNull) {
    Assembly asm_node;
    const std::shared_ptr<EntityBase> null_entity;
    EXPECT_THROW(asm_node.AddEntity(null_entity), std::invalid_argument);
}

// 未知IDのGetEntityはnullptr
TEST_F(AssemblyTest, GetEntity_ReturnsNullForUnknownID) {
    auto ents = Entities();
    ASSERT_GE(ents.size(), 2u);

    Assembly asm_node;
    asm_node.AddEntity(ents[0]);
    // 追加していない別エンティティのID
    EXPECT_EQ(asm_node.GetEntity(ents[1]->GetID()), nullptr);
}

// ロード済みルートはAssembly層の参照解決が完了している
// NOTE: IsReady()/Validate().is_valid==true までは要求しない。これらはエンティティ個別の
//       Validate()(幾何的な厳密検証)に依存し、実IGESファイルでは現状の厳しめの読み込み時
//       検証で偽となりうる(docs/todo.md v0.7.x「読み込み時は検証を緩くする」の領域)。
//       Assembly P1の関心は参照配線であり、ここではそれを検証する。あわせて、退化BBで例外を
//       投げず評価できること(BoundingBox次元対応で解消済み)を確認する。
// TODO: 読み込み時検証の緩和(v0.7.x)後、IsReady()==true / Validate().is_valid==true を
//       要求するよう本テストを強化する。
TEST_F(AssemblyTest, References_FullyLoadedRootResolvesAllReferences) {
    const auto& root = data_->Root();
    EXPECT_TRUE(root.AreAllReferencesSet());
    EXPECT_TRUE(root.GetUnresolvedReferences().empty());
    EXPECT_NO_THROW(root.IsReady());
    EXPECT_NO_THROW(root.Validate());
}

// 参照先を欠いたセットは未解決・準備未完了・検証エラー
TEST_F(AssemblyTest, References_MissingReferentIsUnresolved) {
    auto pair = FindReferencingPair(data_->Root());
    ASSERT_TRUE(pair.has_value()) << "参照を持つエンティティが見つからない";
    const auto& [referrer_id, referent_id] = *pair;

    Assembly asm_node;
    asm_node.AddEntity(data_->Root().GetEntity(referrer_id));  // referentは追加しない

    const auto unresolved = asm_node.GetUnresolvedReferences();
    EXPECT_FALSE(unresolved.empty());
    EXPECT_TRUE(unresolved.count(referent_id) > 0);
    EXPECT_FALSE(asm_node.AreAllReferencesSet());
    EXPECT_FALSE(asm_node.IsReady());
    EXPECT_FALSE(asm_node.Validate().is_valid);
}

// 追加順序によらず、全エンティティを揃えれば全参照が解決する
TEST_F(AssemblyTest, References_ResolveRegardlessOfAddOrder) {
    auto ents = Entities();
    ASSERT_FALSE(ents.empty());

    // 正順
    Assembly forward;
    for (const auto& e : ents) forward.AddEntity(e);
    EXPECT_TRUE(forward.AreAllReferencesSet());

    // 逆順
    Assembly backward;
    for (auto it = ents.rbegin(); it != ents.rend(); ++it) {
        backward.AddEntity(*it);
    }
    EXPECT_TRUE(backward.AreAllReferencesSet());
}



/**
 * ツリー
 */

// AddChildAssemblyは親子リンクを張り、子リストに反映される
TEST_F(AssemblyTest, AddChildAssembly_SetsParentAndAppendsChild) {
    auto root = std::make_shared<Assembly>();
    auto child = std::make_shared<Assembly>();
    root->AddChildAssembly(child);

    EXPECT_EQ(child->GetParent().lock(), root);
    ASSERT_EQ(root->GetChildAssemblies().size(), 1u);
    EXPECT_EQ(root->GetChildAssemblies()[0], child);
}

// nullptrの子追加で例外
TEST_F(AssemblyTest, AddChildAssembly_ThrowsInvalidArgumentWhenNull) {
    auto root = std::make_shared<Assembly>();
    const std::shared_ptr<Assembly> null_child;
    EXPECT_THROW(root->AddChildAssembly(null_child), std::invalid_argument);
}

// Rootは最上位ノードを返す (ルート自身は自分)
TEST_F(AssemblyTest, Root_ReturnsTopmostAncestor) {
    auto root = std::make_shared<Assembly>();
    auto child = std::make_shared<Assembly>();
    auto grandchild = std::make_shared<Assembly>();
    root->AddChildAssembly(child);
    child->AddChildAssembly(grandchild);

    EXPECT_EQ(grandchild->Root(), root);
    EXPECT_EQ(child->Root(), root);
    EXPECT_EQ(root->Root(), root);
}

// アタッチ前に子へ追加したエンティティが、アタッチ後にルートのFindOwnerで引ける
TEST_F(AssemblyTest, FindOwner_ResolvesEntityAfterReindex) {
    auto ents = Entities();
    ASSERT_GE(ents.size(), 2u);

    auto child = std::make_shared<Assembly>();
    const auto id = child->AddEntity(ents[0]);  // アタッチ前に追加

    auto root = std::make_shared<Assembly>();
    root->AddChildAssembly(child);  // ここで再インデックスされる

    EXPECT_EQ(root->FindOwner(id), child.get());
    // 未知IDはnullptr
    EXPECT_EQ(root->FindOwner(ents[1]->GetID()), nullptr);
}



/**
 * クエリ
 */

// GetEntityIDsのrecursive切替が子孫を含む/含まないを正しく反映する
TEST_F(AssemblyTest, GetEntityIDs_RecursiveTogglesDescendants) {
    auto ents = Entities();
    ASSERT_GE(ents.size(), 2u);

    const size_t split = ents.size() / 2;
    auto root = std::make_shared<Assembly>();
    auto child = std::make_shared<Assembly>();
    for (size_t i = 0; i < split; ++i) root->AddEntity(ents[i]);
    for (size_t i = split; i < ents.size(); ++i) child->AddEntity(ents[i]);
    root->AddChildAssembly(child);

    EXPECT_EQ(root->GetEntityIDs(false).size(), split);
    EXPECT_EQ(root->GetEntityIDs(true).size(), ents.size());
}

// FindEntitiesByType / ByUseFlag / FindEntitiesのrecursive切替を件数で検証
TEST_F(AssemblyTest, FindEntities_RecursiveCountsMatchSubsets) {
    auto ents = Entities();
    ASSERT_GE(ents.size(), 2u);

    const size_t split = ents.size() / 2;
    auto root = std::make_shared<Assembly>();
    auto child = std::make_shared<Assembly>();
    for (size_t i = 0; i < split; ++i) root->AddEntity(ents[i]);
    for (size_t i = split; i < ents.size(); ++i) child->AddEntity(ents[i]);
    root->AddChildAssembly(child);

    // 検証対象の型・用途フラグはルート側先頭エンティティから採る
    const auto target_type = ents[0]->GetType();
    const auto target_flag = ents[0]->GetEntityUseFlag();
    const auto count_in = [](const std::vector<EntityPtr>& v,
                             const std::function<bool(const EntityBase&)>& p) {
        return static_cast<size_t>(
            std::count_if(v.begin(), v.end(),
                          [&](const EntityPtr& e) { return p(*e); }));
    };

    std::vector<EntityPtr> root_ents(ents.begin(), ents.begin() + split);
    const auto type_pred = [&](const EntityBase& e) {
        return e.GetType() == target_type;
    };
    const auto flag_pred = [&](const EntityBase& e) {
        return e.GetEntityUseFlag() == target_flag;
    };

    // FindEntitiesByType
    EXPECT_EQ(root->FindEntitiesByType(target_type, false).size(),
              count_in(root_ents, type_pred));
    EXPECT_EQ(root->FindEntitiesByType(target_type, true).size(),
              count_in(ents, type_pred));

    // FindEntitiesByUseFlag
    EXPECT_EQ(root->FindEntitiesByUseFlag(target_flag, false).size(),
              count_in(root_ents, flag_pred));
    EXPECT_EQ(root->FindEntitiesByUseFlag(target_flag, true).size(),
              count_in(ents, flag_pred));

    // FindEntities (任意述語; 型一致で代表)
    EXPECT_EQ(root->FindEntities(type_pred, false).size(),
              count_in(root_ents, type_pred));
    EXPECT_EQ(root->FindEntities(type_pred, true).size(),
              count_in(ents, type_pred));
}
