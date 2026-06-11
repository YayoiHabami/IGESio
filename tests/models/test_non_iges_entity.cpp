/**
 * @file models/test_non_iges_entity.cpp
 * @brief 非IGESエンティティ (NonIgesEntityBase) とAssembly・入出力の連携テスト
 * @author Yayoi Habami
 * @date 2026-06-10
 * @copyright 2026 Yayoi Habami
 * @note テスト対象:
 *       - `entities::NonIgesEntityBase` (ID採番・kNonIges・リビジョン)
 *       - `Assembly` の非IGESエンティティ受け入れ
 *         (AddEntity / GetEntity / GetEntityAs / FindEntitiesByType /
 *          RemoveEntity / MoveEntityTo / Validate / IsReady /
 *          GetWorldBoundingBox)
 *       - `WriteIges` の非IGESエンティティのスキップ (往復)
 *       - `models::Flatten` のスキップ報告
 * @note TODO: ID解放 (デストラクタ) の内部状態はIDGeneratorの実装詳細のため
 *       直接は検証せず、複数インスタンスのID一意性のみ確認する.
 *       描画(レンダラ)との連携はGLコンテキスト前提のため対象外.
 */
#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "igesio/entities/non_iges_entity_base.h"
#include "igesio/entities/entity_base.h"
#include "igesio/entities/curves/line.h"
#include "igesio/models/assembly.h"
#include "igesio/models/iges_data.h"
#include "igesio/models/flatten.h"
#include "igesio/reader.h"
#include "igesio/writer.h"

namespace {

namespace fs = std::filesystem;
namespace i_ent = igesio::entities;
namespace i_mdl = igesio::models;
using igesio::Vector3d;
using igesio::ObjectID;

/// @brief テストによる出力フォルダ
const std::string kOutputDirPath =
        fs::path(__FILE__).parent_path().parent_path()
            .append("test_data").append("output").string();

/// @brief 浮動小数点比較の許容誤差
constexpr double kTol = 1e-12;

/// @brief テスト用の非IGESエンティティ (軸平行ボックス状の点群を模す)
/// @note NonIgesEntityBase(識別)+IGeometry(BB計算)のみを実装した最小の
///       計算パイプライン対応エンティティ. 変換行列は持たない
class DummyPointCloud : public i_ent::NonIgesEntityBase,
                        public i_ent::IGeometry {
 public:
    /// @brief コンストラクタ
    /// @param lo ボックスの最小頂点
    /// @param hi ボックスの最大頂点
    DummyPointCloud(const Vector3d& lo, const Vector3d& hi)
        : lo_(lo), hi_(hi) {}

    /// @brief 定義空間におけるバウンディングボックスを取得する
    igesio::numerics::BoundingBox GetDefinedBoundingBox() const override {
        return igesio::numerics::BoundingBox(lo_, hi_);
    }

    /// @brief 範囲を変更する (形状編集としてリビジョンをバンプする)
    /// @param lo ボックスの最小頂点
    /// @param hi ボックスの最大頂点
    void SetExtent(const Vector3d& lo, const Vector3d& hi) {
        lo_ = lo;
        hi_ = hi;
        MarkGeometryModified();
    }

 protected:
    /// @brief 変換行列を持たないため、入力をそのまま返す
    std::optional<Vector3d> Transform(
            const std::optional<Vector3d>& input,
            [[maybe_unused]] const bool is_point) const override {
        return input;
    }

 private:
    /// @brief ボックスの最小頂点
    Vector3d lo_;
    /// @brief ボックスの最大頂点
    Vector3d hi_;
};

/// @brief 単位ボックス[0,1]^3のDummyPointCloudを生成する
std::shared_ptr<DummyPointCloud> MakeUnitCloud() {
    return std::make_shared<DummyPointCloud>(
            Vector3d{0.0, 0.0, 0.0}, Vector3d{1.0, 1.0, 1.0});
}

}  // namespace



/**
 * NonIgesEntityBase 単体
 */

// 識別子: kNonIges・フォーム0・サポート扱い・ID一意性
TEST(NonIgesEntityBaseTest, IdentityBasics) {
    auto a = MakeUnitCloud();
    auto b = MakeUnitCloud();

    EXPECT_EQ(a->GetType(), i_ent::EntityType::kNonIges);
    EXPECT_EQ(a->GetFormNumber(), 0);
    EXPECT_TRUE(a->IsSupported());
    EXPECT_EQ(i_ent::ToString(a->GetType()), "Non-IGES");

    // IDは設定済みかつインスタンス毎に一意
    EXPECT_TRUE(a->GetID().IsSet());
    EXPECT_NE(a->GetID(), b->GetID());
    // GetSourceIDは既定 (自身のID)
    EXPECT_EQ(a->GetSourceID(), a->GetID());
}

// 参照グラフAPIの既定実装 (空) を継承する
TEST(NonIgesEntityBaseTest, ReferenceGraphDefaultsToEmpty) {
    auto cloud = MakeUnitCloud();
    EXPECT_TRUE(cloud->GetReferencedEntityIDs().empty());
    EXPECT_TRUE(cloud->GetChildIDs().empty());
}

// 形状編集 (MarkGeometryModified) でリビジョンが単調増加する
TEST(NonIgesEntityBaseTest, GeometryRevisionIncrementsOnEdit) {
    auto cloud = MakeUnitCloud();
    const auto rev0 = cloud->GeometryRevision();
    cloud->SetExtent({0.0, 0.0, 0.0}, {2.0, 2.0, 2.0});
    EXPECT_GT(cloud->GeometryRevision(), rev0);
    cloud->SetExtent({0.0, 0.0, 0.0}, {3.0, 3.0, 3.0});
    EXPECT_GT(cloud->GeometryRevision(), rev0 + 0);
}



/**
 * Assemblyとの連携
 */

// AddEntity/GetEntity/GetEntityAsで非IGESエンティティを保存・取得できる
TEST(NonIgesAssemblyTest, AddAndRetrieve) {
    auto root = i_mdl::MakeAssembly();
    auto cloud = MakeUnitCloud();

    const auto id = root->AddEntity(cloud);
    EXPECT_EQ(root->GetEntityCount(), 1u);
    EXPECT_EQ(root->GetEntity(id), cloud);

    // 型指定取得: 具象型は成功、EntityBaseとしてはnullptr
    EXPECT_EQ(root->GetEntityAs<DummyPointCloud>(id), cloud);
    EXPECT_NE(root->GetEntityAs<i_ent::IGeometry>(id), nullptr);
    EXPECT_EQ(root->GetEntityAs<i_ent::EntityBase>(id), nullptr);

    // 型検索: kNonIgesで一括ヒットする
    EXPECT_EQ(root->FindEntitiesByType(i_ent::EntityType::kNonIges).size(), 1u);
}

// EntityBase混在ツリーでも検証・参照解決が成立する (非EntityBaseは無視)
TEST(NonIgesAssemblyTest, ValidateAndIsReadyWithMixedMembers) {
    auto root = i_mdl::MakeAssembly();
    root->AddEntity(i_ent::MakeLine({0, 0, 0}, {1, 0, 0}));
    root->AddEntity(MakeUnitCloud());

    EXPECT_TRUE(root->AreAllReferencesSet());
    EXPECT_TRUE(root->IsReady());
    EXPECT_TRUE(root->Validate().is_valid);
    EXPECT_TRUE(root->ValidateSelfContainedRecursive().is_valid);
}

// 参照されない非IGESエンティティはkRejectでも削除できる
TEST(NonIgesAssemblyTest, RemoveEntityWithoutReferrers) {
    auto root = i_mdl::MakeAssembly();
    const auto id = root->AddEntity(MakeUnitCloud());

    EXPECT_TRUE(root->FindReferrers(id).empty());
    EXPECT_TRUE(root->RemoveEntity(id, i_mdl::RemovalPolicy::kReject));
    EXPECT_EQ(root->GetEntity(id), nullptr);
}

// 非IGESエンティティを子Assemblyへ移動できる
TEST(NonIgesAssemblyTest, MoveEntityToChild) {
    auto root = i_mdl::MakeAssembly();
    auto child = i_mdl::MakeAssembly();
    root->AddChildAssembly(child);
    const auto id = root->AddEntity(MakeUnitCloud());

    root->MoveEntityTo(id, *child);
    EXPECT_EQ(root->GetEntity(id), nullptr);
    EXPECT_NE(child->GetEntity(id), nullptr);
    EXPECT_EQ(root->FindOwner(id), child.get());
}

// 非IGESエンティティの幾何がワールドBBに寄与する
TEST(NonIgesAssemblyTest, ContributesToWorldBoundingBox) {
    auto root = i_mdl::MakeAssembly();
    root->AddEntity(std::make_shared<DummyPointCloud>(
            Vector3d{-1.0, -2.0, -3.0}, Vector3d{1.0, 2.0, 3.0}));

    const auto bb = root->GetWorldBoundingBox();
    ASSERT_TRUE(bb.has_value());
    EXPECT_TRUE(bb->Is3D());

    // 頂点群の成分min/maxが設定した範囲と一致する
    const auto vertices = bb->GetFiniteVertices();
    ASSERT_FALSE(vertices.empty());
    Vector3d lo = vertices.front();
    Vector3d hi = vertices.front();
    for (const auto& v : vertices) {
        lo = lo.cwiseMin(v);
        hi = hi.cwiseMax(v);
    }
    EXPECT_NEAR(lo.x(), -1.0, kTol);
    EXPECT_NEAR(lo.y(), -2.0, kTol);
    EXPECT_NEAR(lo.z(), -3.0, kTol);
    EXPECT_NEAR(hi.x(), 1.0, kTol);
    EXPECT_NEAR(hi.y(), 2.0, kTol);
    EXPECT_NEAR(hi.z(), 3.0, kTol);
}



/**
 * IGES出力・フラット化との連携
 */

// WriteIgesは非IGESエンティティをスキップし、IGESエンティティのみ往復する
TEST(NonIgesIOTest, WriteIgesSkipsNonIgesEntities) {
    i_mdl::IgesData data;
    data.Root().AddEntity(i_ent::MakeLine({0, 0, 0}, {1, 0, 0}));
    data.Root().AddEntity(MakeUnitCloud());

    const std::string output_path =
            fs::path(kOutputDirPath).append("non_iges_skip.iges").string();
    // 非IGESエンティティはUnsupportedEntityではないため、
    // save_unsupported = false (既定) のままエラーにならないこと
    ASSERT_NO_THROW(igesio::WriteIges(data, output_path));
    ASSERT_TRUE(fs::exists(output_path));

    // 読み戻すとLineのみが存在する
    auto read = igesio::ReadIges(output_path);
    EXPECT_EQ(read.Root().GetEntityCount(), 1u);
    EXPECT_EQ(read.Root()
                      .FindEntitiesByType(i_ent::EntityType::kLine)
                      .size(), 1u);
}

// Flattenは非IGESエンティティをスキップし、2引数版でIDを報告する
TEST(NonIgesIOTest, FlattenReportsSkippedNonIgesEntities) {
    i_mdl::IgesData src;
    src.Root().AddEntity(i_ent::MakeLine({0, 0, 0}, {1, 0, 0}));
    auto cloud = MakeUnitCloud();
    const auto cloud_id = src.Root().AddEntity(cloud);

    std::vector<ObjectID> skipped;
    const auto out = i_mdl::Flatten(src, skipped);

    // スキップされた非IGESエンティティのIDが報告される
    ASSERT_EQ(skipped.size(), 1u);
    EXPECT_EQ(skipped[0], cloud_id);
    // 出力には非IGESエンティティが含まれない
    EXPECT_TRUE(out.Root()
                        .FindEntitiesByType(i_ent::EntityType::kNonIges)
                        .empty());
    EXPECT_EQ(out.Root()
                      .FindEntitiesByType(i_ent::EntityType::kLine)
                      .size(), 1u);

    // 1引数版は報告なしで同じ結果になる
    const auto out2 = i_mdl::Flatten(src);
    EXPECT_TRUE(out2.Root()
                        .FindEntitiesByType(i_ent::EntityType::kNonIges)
                        .empty());
}
