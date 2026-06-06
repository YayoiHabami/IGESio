/**
 * @file tests/models/test_assembly.cpp
 * @brief models/assembly.hのテスト (P1: エンティティ管理・ツリー・クエリ)
 * @author Yayoi Habami
 * @date 2026-05-29
 * @copyright 2026 Yayoi Habami
 *
 * テスト対象 (Assembly):
 *   - エンティティ管理: AddEntity / AddEntities / GetEntity / GetEntityCount /
 *                       AreAllReferencesSet / GetUnresolvedReferences /
 *                       IsReady / Validate
 *   - ツリー: AddChildAssembly / GetParent / GetChildAssemblies / Root /
 *             FindOwner (再インデックス)
 *   - クエリ: GetEntityIDs / FindEntitiesByType / FindEntitiesByUseFlag /
 *             FindEntities
 *   - 編集 (P9 B-1/B-2/B-3/B-4): FindReferrers / RemoveEntity / RemoveChildAssembly /
 *                       Clear / MoveEntityTo / MoveChildAssemblyTo / Set*Recursive /
 *                       ComposeGlobalTransform / ValidateSelfContainedRecursive /
 *                       IsEffectivelyEditable (kReject/kCascade/kOrphan)
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
using i_mdl::MakeAssembly;

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

/// @brief assemblyに直接登録済みで、他のどのエンティティからも参照されていないIDを探す
/// @return 参照されていないエンティティのID. 見つからない場合はstd::nullopt
std::optional<ObjectID> FindUnreferenced(const Assembly& a) {
    for (const auto& id : a.GetEntityIDs(false)) {
        if (a.FindReferrers(id).empty()) return id;
    }
    return std::nullopt;
}

/// @brief referrerをroot直下、referentをchild内に配置した「外部→子内」参照ツリー
struct InboundFixture {
    /// @brief ルートAssembly
    std::shared_ptr<Assembly> root;
    /// @brief childサブツリー (referentを所有)
    std::shared_ptr<Assembly> child;
    /// @brief root直下に置いた参照元のID
    ObjectID referrer_id;
    /// @brief child内に置いた参照先のID
    ObjectID referent_id;
};

/// @brief srcの参照ペアを使い、referrer=root直下・referent=child内 のツリーを構築する
/// @param src 参照ペアの供給元 (ロード済みルート)
/// @return 構築したツリー. 参照ペアが見つからない場合はstd::nullopt
std::optional<InboundFixture> MakeInboundTree(const Assembly& src) {
    auto pair = FindReferencingPair(src);
    if (!pair.has_value()) return std::nullopt;
    auto root = MakeAssembly();
    auto child = MakeAssembly();
    root->AddChildAssembly(child);
    root->AddEntity(src.GetEntity(pair->first));    // referrer は root直下
    child->AddEntity(src.GetEntity(pair->second));  // referent は child内
    return InboundFixture{root, child, pair->first, pair->second};
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
 * エンティティ管理 (AddEntities: 一括追加)
 *
 * NOTE: Entities()はAddEntities経由でロード済み(reader.cpp)であり、各エンティティの
 *       参照ポインタは設定済みである。そのためAddEntities内部の参照解決ループ自体は
 *       no-opとなり、ここで検証するのはAssemblyの未解決判定(メンバシップ条件)という
 *       観測可能な契約である。AddEntityループとの等価性(MatchesPerEntityAddEntity)が
 *       最も強い保証となる。
 */

// 全件を一括追加すると、件数と各GetEntityが整合する
TEST_F(AssemblyTest, AddEntities_StoresAllAndIsCountConsistent) {
    auto ents = Entities();
    ASSERT_FALSE(ents.empty());

    Assembly node;
    node.AddEntities(ents);
    EXPECT_EQ(node.GetEntityCount(), ents.size());
    for (const auto& e : ents) {
        EXPECT_EQ(node.GetEntity(e->GetID()), e);
    }
}

// 相互参照する全件を一括追加すると、全参照が解決する
TEST_F(AssemblyTest, AddEntities_ResolvesAllReferences) {
    auto ents = Entities();
    ASSERT_FALSE(ents.empty());

    Assembly node;
    node.AddEntities(ents);
    EXPECT_TRUE(node.AreAllReferencesSet());
    EXPECT_TRUE(node.GetUnresolvedReferences().empty());
}

// 参照先が参照元より後ろに来る並び(前方参照を含む)でも、1パスで双方向に解決する
TEST_F(AssemblyTest, AddEntities_ResolvesForwardAndBackwardReferences) {
    auto ents = Entities();
    ASSERT_FALSE(ents.empty());
    // ベクタを逆順にし、前方参照を含む並びにする (バッチ解決の順序非依存を確認)
    std::vector<EntityPtr> reversed(ents.rbegin(), ents.rend());

    Assembly node;
    node.AddEntities(reversed);
    EXPECT_TRUE(node.AreAllReferencesSet());
    EXPECT_TRUE(node.GetUnresolvedReferences().empty());
}

// 同じ集合をAddEntitiesとAddEntityループで追加した結果が一致する (Option Bの肝)
TEST_F(AssemblyTest, AddEntities_MatchesPerEntityAddEntity) {
    auto ents = Entities();
    ASSERT_FALSE(ents.empty());

    Assembly batch;
    batch.AddEntities(ents);

    Assembly loop;
    for (const auto& e : ents) loop.AddEntity(e);

    EXPECT_EQ(batch.GetEntityCount(), loop.GetEntityCount());
    EXPECT_EQ(batch.AreAllReferencesSet(), loop.AreAllReferencesSet());
    EXPECT_EQ(batch.GetUnresolvedReferences(), loop.GetUnresolvedReferences());
}

// 一括追加後、ルート逆引きインデックスが機能する (RegisterInIndex実行の確認)
TEST_F(AssemblyTest, AddEntities_RegistersInRootIndex) {
    auto ents = Entities();
    ASSERT_FALSE(ents.empty());
    auto pair = FindReferencingPair(data_->Root());
    ASSERT_TRUE(pair.has_value());

    Assembly node;
    node.AddEntities(ents);

    // 全エンティティがFindOwnerで自ノードに解決される
    for (const auto& e : ents) {
        EXPECT_EQ(node.FindOwner(e->GetID()), &node);
    }
    // FindReferrersがreferentに対しreferrerを返す
    const auto referrers = node.FindReferrers(pair->second);
    EXPECT_NE(std::find(referrers.begin(), referrers.end(), pair->first),
              referrers.end());
}

// nullptrを含むベクタの一括追加で例外
TEST_F(AssemblyTest, AddEntities_ThrowsInvalidArgumentWhenNullPresent) {
    auto ents = Entities();
    ASSERT_FALSE(ents.empty());
    const std::vector<EntityPtr> with_null = {ents[0], nullptr};

    Assembly node;
    EXPECT_THROW(node.AddEntities(with_null), std::invalid_argument);
}

// 空ベクタの一括追加は例外を投げず、件数を変えない
TEST_F(AssemblyTest, AddEntities_EmptyVectorIsNoOp) {
    auto ents = Entities();
    ASSERT_FALSE(ents.empty());

    // 空ノードへの空追加
    Assembly empty_node;
    EXPECT_NO_THROW(empty_node.AddEntities({}));
    EXPECT_EQ(empty_node.GetEntityCount(), 0u);

    // 既存エンティティを持つノードへの空追加は件数不変
    Assembly populated;
    populated.AddEntity(ents[0]);
    EXPECT_NO_THROW(populated.AddEntities({}));
    EXPECT_EQ(populated.GetEntityCount(), 1u);
}



/**
 * ツリー
 */

// AddChildAssemblyは親子リンクを張り、子リストに反映される
TEST_F(AssemblyTest, AddChildAssembly_SetsParentAndAppendsChild) {
    auto root = MakeAssembly();
    auto child = MakeAssembly();
    root->AddChildAssembly(child);

    EXPECT_EQ(child->GetParent().lock(), root);
    ASSERT_EQ(root->GetChildAssemblies().size(), 1u);
    EXPECT_EQ(root->GetChildAssemblies()[0], child);
}

// nullptrの子追加で例外
TEST_F(AssemblyTest, AddChildAssembly_ThrowsInvalidArgumentWhenNull) {
    auto root = MakeAssembly();
    const std::shared_ptr<Assembly> null_child;
    EXPECT_THROW(root->AddChildAssembly(null_child), std::invalid_argument);
}

// Rootは最上位ノードを返す (ルート自身は自分)
TEST_F(AssemblyTest, Root_ReturnsTopmostAncestor) {
    auto root = MakeAssembly();
    auto child = MakeAssembly();
    auto grandchild = MakeAssembly();
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

    auto child = MakeAssembly();
    const auto id = child->AddEntity(ents[0]);  // アタッチ前に追加

    auto root = MakeAssembly();
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
    auto root = MakeAssembly();
    auto child = MakeAssembly();
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
    auto root = MakeAssembly();
    auto child = MakeAssembly();
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



/**
 * 編集・ライフサイクル (P9 B-1: FindReferrers / RemoveEntity)
 */

// FindReferrers: 参照しているエンティティ(referrer)を逆引きで返し、自己参照は含めない
TEST_F(AssemblyTest, FindReferrers_ReturnsReferringEntities) {
    auto pair = FindReferencingPair(data_->Root());
    ASSERT_TRUE(pair.has_value()) << "参照を持つエンティティが見つからない";
    const auto& [referrer_id, referent_id] = *pair;

    Assembly root;
    for (const auto& e : Entities()) root.AddEntity(e);

    const auto referrers = root.FindReferrers(referent_id);
    EXPECT_NE(std::find(referrers.begin(), referrers.end(), referrer_id),
              referrers.end());
    EXPECT_EQ(std::find(referrers.begin(), referrers.end(), referent_id),
              referrers.end());  // 自己参照は対象外
}

// RemoveEntity(kReject): 参照されている要素は拒否、参照なし要素は削除する
TEST_F(AssemblyTest, RemoveEntity_RejectRespectsReferrers) {
    auto pair = FindReferencingPair(data_->Root());
    ASSERT_TRUE(pair.has_value());
    const auto referent_id = pair->second;

    Assembly root;
    for (const auto& e : Entities()) root.AddEntity(e);

    // 参照されている要素(referent)はkRejectで削除できない
    EXPECT_FALSE(root.RemoveEntity(referent_id, i_mdl::RemovalPolicy::kReject));
    EXPECT_NE(root.GetEntity(referent_id), nullptr);

    // 参照されていない要素は削除でき、逆引きからも除去される
    const auto free_id = FindUnreferenced(root);
    ASSERT_TRUE(free_id.has_value());
    EXPECT_TRUE(root.RemoveEntity(*free_id, i_mdl::RemovalPolicy::kReject));
    EXPECT_EQ(root.GetEntity(*free_id), nullptr);
    EXPECT_EQ(root.FindOwner(*free_id), nullptr);
}

// RemoveEntity(kOrphan): 参照が残っても削除する (referrerは残る)
TEST_F(AssemblyTest, RemoveEntity_OrphanRemovesDespiteReferrers) {
    auto pair = FindReferencingPair(data_->Root());
    ASSERT_TRUE(pair.has_value());
    const auto& [referrer_id, referent_id] = *pair;

    Assembly root;
    for (const auto& e : Entities()) root.AddEntity(e);

    EXPECT_TRUE(root.RemoveEntity(referent_id, i_mdl::RemovalPolicy::kOrphan));
    EXPECT_EQ(root.GetEntity(referent_id), nullptr);  // referentは削除
    EXPECT_NE(root.GetEntity(referrer_id), nullptr);  // referrerは残る
}

// RemoveEntity(kCascade): 参照元(referrer)も連鎖削除する
TEST_F(AssemblyTest, RemoveEntity_CascadeRemovesReferrers) {
    auto pair = FindReferencingPair(data_->Root());
    ASSERT_TRUE(pair.has_value());
    const auto& [referrer_id, referent_id] = *pair;

    Assembly root;
    for (const auto& e : Entities()) root.AddEntity(e);

    EXPECT_TRUE(root.RemoveEntity(referent_id, i_mdl::RemovalPolicy::kCascade));
    EXPECT_EQ(root.GetEntity(referent_id), nullptr);  // referent
    EXPECT_EQ(root.GetEntity(referrer_id), nullptr);  // 参照元も連鎖削除
}

// RemoveEntity: 既に削除済み(未存在)のIDはfalseを返す
TEST_F(AssemblyTest, RemoveEntity_ReturnsFalseWhenAlreadyRemoved) {
    Assembly root;
    for (const auto& e : Entities()) root.AddEntity(e);
    const auto free_id = FindUnreferenced(root);
    ASSERT_TRUE(free_id.has_value());

    EXPECT_TRUE(root.RemoveEntity(*free_id, i_mdl::RemovalPolicy::kOrphan));
    EXPECT_FALSE(root.RemoveEntity(*free_id, i_mdl::RemovalPolicy::kOrphan));
}

// RemoveEntity: 編集ロックされたノードのエンティティは削除を拒否する
TEST_F(AssemblyTest, RemoveEntity_RejectsWhenLocked) {
    auto root = MakeAssembly();
    auto child = MakeAssembly();
    root->AddChildAssembly(child);
    auto ents = Entities();
    ASSERT_FALSE(ents.empty());
    const auto id = child->AddEntity(ents[0]);

    child->Metadata().lock.editable = false;  // 編集不可
    EXPECT_FALSE(root->RemoveEntity(id, i_mdl::RemovalPolicy::kOrphan));
    EXPECT_NE(child->GetEntity(id), nullptr);

    child->Metadata().lock.editable = true;   // 解除すれば削除可
    EXPECT_TRUE(root->RemoveEntity(id, i_mdl::RemovalPolicy::kOrphan));
    EXPECT_EQ(child->GetEntity(id), nullptr);
}



/**
 * 編集・ライフサイクル (P9 B-2: RemoveChildAssembly / Clear / Move)
 */

// RemoveChildAssembly: サブツリー内で閉じた参照は削除を妨げない
TEST_F(AssemblyTest, RemoveChildAssembly_AllowsIntraSubtreeReferences) {
    auto pair = FindReferencingPair(data_->Root());
    ASSERT_TRUE(pair.has_value());
    const auto& [referrer_id, referent_id] = *pair;

    auto root = MakeAssembly();
    auto child = MakeAssembly();
    root->AddChildAssembly(child);
    // referrer/referentの両方をchildに入れる(参照はサブツリー内で閉じる)
    child->AddEntity(data_->Root().GetEntity(referrer_id));
    child->AddEntity(data_->Root().GetEntity(referent_id));

    EXPECT_TRUE(root->RemoveChildAssembly(child->GetID(),
                                          i_mdl::RemovalPolicy::kReject));
    EXPECT_TRUE(root->GetChildAssemblies().empty());
    EXPECT_EQ(root->FindOwner(referent_id), nullptr);  // 逆引きからも除去
}

// RemoveChildAssembly(kReject): 外部からの参照(inbound)が残るなら拒否する
TEST_F(AssemblyTest, RemoveChildAssembly_RejectsInboundReference) {
    auto fx = MakeInboundTree(data_->Root());
    ASSERT_TRUE(fx.has_value());

    EXPECT_FALSE(fx->root->RemoveChildAssembly(fx->child->GetID(),
                                               i_mdl::RemovalPolicy::kReject));
    EXPECT_EQ(fx->root->GetChildAssemblies().size(), 1u);  // 無変更
    EXPECT_NE(fx->child->GetEntity(fx->referent_id), nullptr);
}

// RemoveChildAssembly(kOrphan): 外部参照が残っても削除する (referrerは残る)
TEST_F(AssemblyTest, RemoveChildAssembly_OrphanRemovesDespiteInbound) {
    auto fx = MakeInboundTree(data_->Root());
    ASSERT_TRUE(fx.has_value());

    EXPECT_TRUE(fx->root->RemoveChildAssembly(fx->child->GetID(),
                                              i_mdl::RemovalPolicy::kOrphan));
    EXPECT_TRUE(fx->root->GetChildAssemblies().empty());
    EXPECT_EQ(fx->root->FindOwner(fx->referent_id), nullptr);  // childは除去
    EXPECT_NE(fx->root->GetEntity(fx->referrer_id), nullptr);  // referrerは残る
}

// RemoveChildAssembly(kCascade): 外部の参照元も連鎖削除する
TEST_F(AssemblyTest, RemoveChildAssembly_CascadeRemovesOutsideReferrer) {
    auto fx = MakeInboundTree(data_->Root());
    ASSERT_TRUE(fx.has_value());

    EXPECT_TRUE(fx->root->RemoveChildAssembly(fx->child->GetID(),
                                              i_mdl::RemovalPolicy::kCascade));
    EXPECT_TRUE(fx->root->GetChildAssemblies().empty());
    EXPECT_EQ(fx->root->GetEntity(fx->referrer_id), nullptr);  // 参照元も削除
}

// RemoveChildAssembly: 直接の子でないIDはfalse、直接の子は削除できる
TEST_F(AssemblyTest, RemoveChildAssembly_OnlyTargetsDirectChildren) {
    auto root = MakeAssembly();
    auto child = MakeAssembly();
    auto grandchild = MakeAssembly();
    root->AddChildAssembly(child);
    child->AddChildAssembly(grandchild);

    // 孫(間接)は対象外
    EXPECT_FALSE(root->RemoveChildAssembly(grandchild->GetID()));
    EXPECT_EQ(root->GetChildAssemblies().size(), 1u);
    // 直接の子は削除できる (子孫も巻き込んで除去)
    EXPECT_TRUE(root->RemoveChildAssembly(child->GetID()));
    EXPECT_TRUE(root->GetChildAssemblies().empty());
}

// Clear: 自ノードのエンティティ・子Assemblyを除去し、逆引きからも消える
TEST_F(AssemblyTest, Clear_RemovesEntitiesChildrenAndDeindexes) {
    auto ents = Entities();
    ASSERT_GE(ents.size(), 2u);
    auto root = MakeAssembly();
    auto child = MakeAssembly();
    root->AddChildAssembly(child);
    const auto id0 = root->AddEntity(ents[0]);
    const auto id1 = child->AddEntity(ents[1]);

    root->Clear();
    EXPECT_EQ(root->GetEntityCount(), 0u);
    EXPECT_TRUE(root->GetChildAssemblies().empty());
    EXPECT_EQ(root->FindOwner(id0), nullptr);
    EXPECT_EQ(root->FindOwner(id1), nullptr);
}

// MoveEntityTo: 所有ノードを付け替え、逆引きownerを更新する
TEST_F(AssemblyTest, MoveEntityTo_ReparentsAndUpdatesOwner) {
    auto ents = Entities();
    ASSERT_FALSE(ents.empty());
    auto root = MakeAssembly();
    auto child = MakeAssembly();
    root->AddChildAssembly(child);
    const auto id = root->AddEntity(ents[0]);

    root->MoveEntityTo(id, *child);
    EXPECT_EQ(root->GetEntity(id), nullptr);
    EXPECT_NE(child->GetEntity(id), nullptr);
    EXPECT_EQ(root->FindOwner(id), child.get());
}

// MoveEntityTo: 異なるルート/未存在IDは例外
TEST_F(AssemblyTest, MoveEntityTo_ThrowsForDifferentRootOrUnknownId) {
    auto ents = Entities();
    ASSERT_GE(ents.size(), 2u);
    auto root1 = MakeAssembly();
    auto root2 = MakeAssembly();
    const auto id = root1->AddEntity(ents[0]);

    // 異なるルートへの移動は不可
    EXPECT_THROW(root1->MoveEntityTo(id, *root2), std::invalid_argument);
    // ツリーに無いIDの移動は不可
    auto child = MakeAssembly();
    root1->AddChildAssembly(child);
    EXPECT_THROW(root1->MoveEntityTo(ents[1]->GetID(), *child),
                 std::invalid_argument);
}

// MoveChildAssemblyTo: 子Assemblyを付け替え、逆引き(エンティティ→所有)は不変
TEST_F(AssemblyTest, MoveChildAssemblyTo_ReparentsChild) {
    auto ents = Entities();
    ASSERT_FALSE(ents.empty());
    auto root = MakeAssembly();
    auto a = MakeAssembly();
    auto b = MakeAssembly();
    root->AddChildAssembly(a);
    root->AddChildAssembly(b);
    const auto id = b->AddEntity(ents[0]);

    root->MoveChildAssemblyTo(b->GetID(), *a);
    ASSERT_EQ(root->GetChildAssemblies().size(), 1u);
    EXPECT_EQ(root->GetChildAssemblies()[0], a);
    ASSERT_EQ(a->GetChildAssemblies().size(), 1u);
    EXPECT_EQ(a->GetChildAssemblies()[0], b);
    EXPECT_EQ(b->GetParent().lock(), a);
    EXPECT_EQ(root->FindOwner(id), b.get());  // 同一ルートのため逆引きは不変
}

// MoveChildAssemblyTo: 循環(子孫への移動)・非直接子は例外
TEST_F(AssemblyTest, MoveChildAssemblyTo_ThrowsOnCycleOrNonDirectChild) {
    auto root = MakeAssembly();
    auto a = MakeAssembly();
    auto b = MakeAssembly();
    root->AddChildAssembly(a);
    a->AddChildAssembly(b);

    // aをその子孫bの下へ移動 → 循環
    EXPECT_THROW(root->MoveChildAssemblyTo(a->GetID(), *b),
                 std::invalid_argument);
    // bはrootの直接子でない
    EXPECT_THROW(root->MoveChildAssemblyTo(b->GetID(), *a),
                 std::invalid_argument);
}



/**
 * 編集・ライフサイクル (P9 B-3: 一斉処理/変換, B-4: 自己完結検証)
 */

// SetVisibleRecursive: 自ノードと全子孫へ適用される
TEST_F(AssemblyTest, SetVisibleRecursive_AppliesToSelfAndDescendants) {
    auto root = MakeAssembly();
    auto child = MakeAssembly();
    auto grandchild = MakeAssembly();
    root->AddChildAssembly(child);
    child->AddChildAssembly(grandchild);

    root->SetVisibleRecursive(false);
    EXPECT_FALSE(root->Metadata().visible);
    EXPECT_FALSE(child->Metadata().visible);
    EXPECT_FALSE(grandchild->Metadata().visible);

    root->SetVisibleRecursive(true);
    EXPECT_TRUE(grandchild->Metadata().visible);
}

// SetSuppressedRecursive: 自ノードと全子孫へ適用される
TEST_F(AssemblyTest, SetSuppressedRecursive_AppliesToSelfAndDescendants) {
    auto root = MakeAssembly();
    auto child = MakeAssembly();
    root->AddChildAssembly(child);

    root->SetSuppressedRecursive(true);
    EXPECT_TRUE(root->Metadata().suppressed);
    EXPECT_TRUE(child->Metadata().suppressed);
}

// SetColorOverrideRecursive: 設定と解除(nullopt)が全子孫へ伝播する
TEST_F(AssemblyTest, SetColorOverrideRecursive_AppliesAndClears) {
    auto root = MakeAssembly();
    auto child = MakeAssembly();
    root->AddChildAssembly(child);

    const std::array<float, 3> red{1.0f, 0.0f, 0.0f};
    root->SetColorOverrideRecursive(red);
    ASSERT_TRUE(child->Metadata().color_override.has_value());
    EXPECT_EQ(child->Metadata().color_override.value(), red);

    root->SetColorOverrideRecursive(std::nullopt);
    EXPECT_FALSE(root->Metadata().color_override.has_value());
    EXPECT_FALSE(child->Metadata().color_override.has_value());
}

// SetOpacityOverrideRecursive: 設定と解除(nullopt)が全子孫へ伝播する
TEST_F(AssemblyTest, SetOpacityOverrideRecursive_AppliesAndClears) {
    auto root = MakeAssembly();
    auto child = MakeAssembly();
    root->AddChildAssembly(child);

    root->SetOpacityOverrideRecursive(0.5f);
    ASSERT_TRUE(child->Metadata().opacity_override.has_value());
    EXPECT_FLOAT_EQ(child->Metadata().opacity_override.value(), 0.5f);

    root->SetOpacityOverrideRecursive(std::nullopt);
    EXPECT_FALSE(child->Metadata().opacity_override.has_value());
}

// ComposeGlobalTransform: 親フレーム合成(left-multiply)で、子孫は再帰しない
TEST_F(AssemblyTest, ComposeGlobalTransform_ComposesInParentFrameNonRecursive) {
    auto root = MakeAssembly();
    auto child = MakeAssembly();
    root->AddChildAssembly(child);

    // original: x方向に1並進, applied: Z軸90°回転 (非可換で順序を検証可能)
    igesio::Matrix4d original = igesio::Matrix4d::Identity();
    original(0, 3) = 1.0;
    igesio::Matrix4d applied = igesio::Matrix4d::Identity();
    applied(0, 0) = 0.0; applied(0, 1) = -1.0;
    applied(1, 0) = 1.0; applied(1, 1) = 0.0;

    root->SetGlobalTransform(original);
    root->ComposeGlobalTransform(applied);

    const igesio::Matrix4d expected = applied * original;
    const igesio::Matrix4d got = root->GetGlobalTransform();
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            EXPECT_NEAR(got(r, c), expected(r, c), 1e-9);
        }
    }
    // 子は再帰しない (恒等のまま)
    EXPECT_TRUE(child->GetGlobalTransform().isApprox(
            igesio::Matrix4d::Identity()));
}

// ValidateSelfContainedRecursive: 全参照が同一ノード内で解決すれば有効
TEST_F(AssemblyTest, ValidateSelfContainedRecursive_TrueForResolvedTree) {
    Assembly root;
    for (const auto& e : Entities()) root.AddEntity(e);  // 全て同一ノード

    EXPECT_TRUE(root.ValidateSelfContainedRecursive().is_valid);
}

// ValidateSelfContainedRecursive: ノードを跨ぐ参照を未解決として検出する
TEST_F(AssemblyTest, ValidateSelfContainedRecursive_DetectsCrossNodeReference) {
    auto fx = MakeInboundTree(data_->Root());
    ASSERT_TRUE(fx.has_value());

    // referrer(root直下)がreferent(child内)を参照 -> rootノードで未解決
    EXPECT_FALSE(fx->root->ValidateSelfContainedRecursive().is_valid);
}
