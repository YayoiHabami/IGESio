/**
 * @file tests/models/test_initial_tree.cpp
 * @brief リーダのツリー構築 (P6) のテスト
 * @author Yayoi Habami
 * @date 2026-05-29
 * @copyright 2026 Yayoi Habami
 *
 * テスト対象:
 *   - reader.cpp::BuildInitialTree (内部関数。ReadIgesの観測可能な木構造で検証する)
 *
 * 現状の契約 (assembly_class_design.md §12.2 P6):
 *   束ね系 (402 Group / 308 Subfigure / 184 Solid Assembly) が型付きエンティティ未対応の
 *   うちは、子Assemblyを生成せず全エンティティをルート直下に置くフラットな初期ツリーとする。
 *   物理従属メンバ (142や144の下位曲面など) も平坦マップに第一級で保持される (ラウンドトリップ
 *   源泉)。
 *
 * TODO: 束ね系が型付き対応した時点で「メンバ列→子Assembly生成」のケースを追加する。
 * TODO: 完全なvalidファイルはsingle_rounded_cube.igesのみのため、本テストはこれを用いる。
 */
#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_set>

#include "igesio/reader.h"
#include "igesio/entities/entity_base.h"
#include "igesio/entities/entity_type.h"
#include "igesio/models/assembly.h"
#include "igesio/models/iges_data.h"

namespace {

namespace fs = std::filesystem;
namespace i_ent = igesio::entities;
namespace i_mdl = igesio::models;

/// @brief テスト用IGESファイル (一辺が丸められた立方体; 124/142/144を含む)
const std::string kCubePath =
        fs::path(__FILE__).parent_path().parent_path()
            .append("test_data").append("single_rounded_cube.iges").string();

}  // namespace



/// @brief ロード済みIGESデータを共有するフィクスチャ
class InitialTreeTest : public ::testing::Test {
 protected:
    /// @brief 一度だけロードするIGESデータ
    inline static std::unique_ptr<i_mdl::IgesData> data_;

    static void SetUpTestSuite() {
        if (!data_) {
            data_ = std::make_unique<i_mdl::IgesData>(igesio::ReadIges(kCubePath));
        }
    }
};



// ルートは子Assemblyを持たない (現状フラット)
TEST_F(InitialTreeTest, RootHasNoChildAssemblies) {
    EXPECT_TRUE(data_->Root().GetChildAssemblies().empty());
}

// 全エンティティがルート直下に存在する (子に何も無い = 完全フラット)
TEST_F(InitialTreeTest, AllEntitiesLiveAtRoot) {
    const auto& root = data_->Root();
    ASSERT_GT(root.GetEntityCount(), 0u);
    EXPECT_EQ(root.GetEntityIDs(false).size(), root.GetEntityIDs(true).size());
    EXPECT_EQ(root.GetEntityIDs(false).size(), root.GetEntityCount());
}

// 各型について非再帰件数と再帰件数が一致する (子Assemblyにメンバが無いことの強検証)
TEST_F(InitialTreeTest, TypeCountsFlatEqualsRecursive) {
    const auto& root = data_->Root();

    // ロード済みエンティティに現れる全ての型を収集する
    std::unordered_set<i_ent::EntityType> types;
    for (const auto& [id, e] : root.GetEntities()) types.insert(e->GetType());
    ASSERT_FALSE(types.empty());

    for (const auto type : types) {
        EXPECT_EQ(root.FindEntitiesByType(type, false).size(),
                  root.FindEntitiesByType(type, true).size())
            << "型 " << static_cast<int>(type) << " の件数がフラットでない";
    }

    // cubeが含むはずの代表型が存在することを確認する
    EXPECT_FALSE(root.FindEntitiesByType(
            i_ent::EntityType::kTrimmedSurface).empty());
    EXPECT_FALSE(root.FindEntitiesByType(
            i_ent::EntityType::kTransformationMatrix).empty());
}

// 物理従属メンバも平坦マップに第一級で保持される (§8.1: ラウンドトリップ源泉)
TEST_F(InitialTreeTest, PhysicallyDependentMembersPresentAtRoot) {
    const auto& root = data_->Root();
    const auto dependents = root.FindEntities(
            [](const i_ent::EntityBase& e) {
                return e.GetSubordinateEntitySwitch()
                        == i_ent::SubordinateEntitySwitch::kPhysicallyDependent;
            });
    EXPECT_FALSE(dependents.empty())
        << "cubeは144の下位曲面・142等の物理従属メンバを含むはず";
}

// 木構築後もすべての参照が解決されている
TEST_F(InitialTreeTest, AllReferencesResolvedAfterLoad) {
    EXPECT_TRUE(data_->Root().AreAllReferencesSet());
    EXPECT_TRUE(data_->Root().GetUnresolvedReferences().empty());
}
