/**
 * @file tests/extensions/inspection/test_instanced_entity.cpp
 * @brief InstancedEntity (extensions/inspection) のテスト
 * @author Yayoi Habami
 * @date 2026-06-12
 * @copyright 2026 Yayoi Habami
 * @note 対象は InstancedEntity の以下の振る舞い:
 *       - 正常系 (代表値): コンストラクタの格納 (単一エンティティ/複数メンバ)、
 *         MemberCount/InstanceCount、型/フォーム、GetDefinedBoundingBox の
 *         全複製包含 (恒等・平行移動・局所配置の和)
 *       - 正常系 (境界値): 複製先行列が空の場合のバウンディングボックス (0次元)
 *       - 正常系 (退化): SetTransforms/AddInstance による形状リビジョンのバンプ
 *       - 異常系: source/メンバsourceがnullptrの場合の例外
 *       - ファクトリ: 単一複製先・Assembly平坦化 (入れ子の大域変換の畳み込み)
 *
 * TODO: 描画クラス (InstancedEntityGraphics) はGL文脈を要するため本テストの
 *       対象外 (GUI/手動検証で確認)。
 */
#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/numerics/geometric/bounding_box.h"
#include "igesio/entities/entity_type.h"
#include "igesio/entities/non_iges_entity_base.h"
#include "igesio/entities/interfaces/i_geometry.h"
#include "igesio/entities/interfaces/i_entity_identifier.h"
#include "igesio/models/assembly.h"
#include "igesio/extensions/inspection/instanced_entity.h"

namespace {

namespace inspection = igesio::extensions::inspection;
namespace i_ent = igesio::entities;
namespace i_num = igesio::numerics;
namespace i_mod = igesio::models;
using igesio::Matrix4d;
using igesio::Vector3d;

/// @brief 単位立方体 [0,1]^3 を定義空間バウンディングボックスとする基準スタブ
/// @note InstancedEntityのバウンディングボックス計算 (各複製先行列を適用した
///       基準頂点群の和) を既知形状で検証するための最小のIGeometry実装.
///       変換行列を持たない非IGESエンティティ (Transformは恒等).
class UnitCubeStub
        : public i_ent::NonIgesEntityBase, public i_ent::IGeometry {
 public:
    /// @brief コンストラクタ
    UnitCubeStub() = default;

    /// @brief 定義空間のバウンディングボックス ([0,1]^3) を返す
    i_num::BoundingBox GetDefinedBoundingBox() const override {
        return i_num::BoundingBox(Vector3d(0.0, 0.0, 0.0),
                                  Vector3d(1.0, 1.0, 1.0));
    }

 protected:
    /// @brief 座標orベクトルを変換する (恒等)
    std::optional<Vector3d> Transform(
            const std::optional<Vector3d>& input,
            [[maybe_unused]] const bool is_point) const override {
        return input;
    }
};

/// @brief x軸方向の平行移動行列を作成する
Matrix4d TranslationX(const double dx) {
    Matrix4d m = Matrix4d::Identity();
    m(0, 3) = dx;
    return m;
}

}  // namespace



/// @brief 単一エンティティのコンストラクタがsourceを1メンバとして格納する (代表値)
TEST(InstancedEntity, Constructor_StoresSourceAsSingleMember) {
    auto source = std::make_shared<UnitCubeStub>();
    std::vector<Matrix4d> transforms = {Matrix4d::Identity(), TranslationX(10.0)};
    inspection::InstancedEntity instanced(source, transforms);

    ASSERT_EQ(instanced.MemberCount(), 1u);
    EXPECT_EQ(instanced.Members()[0].source.get(), source.get());
    EXPECT_EQ(instanced.InstanceCount(), 2u);
    EXPECT_EQ(instanced.Transforms().size(), 2u);
}

/// @brief 複数メンバのコンストラクタがメンバ列を格納する (代表値)
TEST(InstancedEntity, Constructor_StoresMembers) {
    auto a = std::make_shared<UnitCubeStub>();
    auto b = std::make_shared<UnitCubeStub>();
    std::vector<inspection::InstancedMember> members = {
        {a, Matrix4d::Identity()},
        {b, TranslationX(5.0)},
    };
    inspection::InstancedEntity instanced(members, {Matrix4d::Identity()});

    ASSERT_EQ(instanced.MemberCount(), 2u);
    EXPECT_EQ(instanced.Members()[0].source.get(), a.get());
    EXPECT_EQ(instanced.Members()[1].source.get(), b.get());
}

/// @brief 型・フォーム番号が非IGESエンティティの規約に従う (代表値)
TEST(InstancedEntity, TypeAndForm_AreNonIges) {
    auto source = std::make_shared<UnitCubeStub>();
    inspection::InstancedEntity instanced(source, {Matrix4d::Identity()});

    EXPECT_EQ(instanced.GetType(), i_ent::EntityType::kNonIges);
    EXPECT_EQ(instanced.GetFormNumber(), 0);
}

/// @brief バウンディングボックスが全複製 (恒等+平行移動) を包含する (代表値)
TEST(InstancedEntity, GetDefinedBoundingBox_UnionOfInstances) {
    auto source = std::make_shared<UnitCubeStub>();
    // 恒等 ([0,1]^3) と x方向+10 ([10,11]x[0,1]x[0,1]) の和
    inspection::InstancedEntity instanced(
            source, {Matrix4d::Identity(), TranslationX(10.0)});

    const auto bb = instanced.GetDefinedBoundingBox();
    EXPECT_EQ(bb.Dimension(), 3u);
    EXPECT_TRUE(bb.IsFinite());
    // 各複製の内部点を包含する
    EXPECT_TRUE(bb.Contains(Vector3d(0.5, 0.5, 0.5)));
    EXPECT_TRUE(bb.Contains(Vector3d(10.5, 0.5, 0.5)));
    // 和の軸平行ボックスの範囲外 (z方向) は包含しない
    EXPECT_FALSE(bb.Contains(Vector3d(0.5, 0.5, 2.0)));
}

/// @brief バウンディングボックスが全メンバ (局所配置) を包含する (代表値)
TEST(InstancedEntity, GetDefinedBoundingBox_UnionOverMembers) {
    auto a = std::make_shared<UnitCubeStub>();
    auto b = std::make_shared<UnitCubeStub>();
    // メンバa ([0,1]^3) と メンバb (局所配置+5: [5,6]x[0,1]x[0,1])、複製先は恒等のみ
    std::vector<inspection::InstancedMember> members = {
        {a, Matrix4d::Identity()},
        {b, TranslationX(5.0)},
    };
    inspection::InstancedEntity instanced(members, {Matrix4d::Identity()});

    const auto bb = instanced.GetDefinedBoundingBox();
    EXPECT_TRUE(bb.Contains(Vector3d(0.5, 0.5, 0.5)));   // メンバa
    EXPECT_TRUE(bb.Contains(Vector3d(5.5, 0.5, 0.5)));   // メンバb (+5)
    EXPECT_FALSE(bb.Contains(Vector3d(0.5, 0.5, 2.0)));  // z範囲外
}

/// @brief 複製先行列が空の場合は0次元のバウンディングボックスとなる (境界値)
TEST(InstancedEntity, GetDefinedBoundingBox_EmptyWhenNoTransforms) {
    auto source = std::make_shared<UnitCubeStub>();
    inspection::InstancedEntity instanced(source, {});

    EXPECT_TRUE(instanced.GetDefinedBoundingBox().IsEmpty());
}

/// @brief SetTransformsが形状リビジョンをバンプする (退化)
TEST(InstancedEntity, SetTransforms_BumpsGeometryRevision) {
    auto source = std::make_shared<UnitCubeStub>();
    inspection::InstancedEntity instanced(source, {Matrix4d::Identity()});

    const auto before = instanced.GeometryRevision();
    instanced.SetTransforms({TranslationX(5.0), TranslationX(10.0)});
    EXPECT_GT(instanced.GeometryRevision(), before);
    EXPECT_EQ(instanced.InstanceCount(), 2u);
}

/// @brief AddInstanceが複製を追加し形状リビジョンをバンプする (退化)
TEST(InstancedEntity, AddInstance_AppendsAndBumpsRevision) {
    auto source = std::make_shared<UnitCubeStub>();
    inspection::InstancedEntity instanced(source, {Matrix4d::Identity()});

    const auto before = instanced.GeometryRevision();
    instanced.AddInstance(TranslationX(3.0));
    EXPECT_GT(instanced.GeometryRevision(), before);
    EXPECT_EQ(instanced.InstanceCount(), 2u);
}

/// @brief ファクトリ (単一複製先) が複製先1つのエンティティを生成する (代表値)
TEST(InstancedEntity, MakeInstancedEntity_SingleTransform) {
    auto source = std::make_shared<UnitCubeStub>();
    auto instanced = inspection::MakeInstancedEntity(source, TranslationX(7.0));

    ASSERT_NE(instanced, nullptr);
    EXPECT_EQ(instanced->MemberCount(), 1u);
    EXPECT_EQ(instanced->InstanceCount(), 1u);
}

/// @brief MakeInstancedAssemblyがAssemblyサブツリーを幾何メンバへ平坦化する (代表値)
/// @note 子Assemblyの大域変換 (+10) がメンバの局所配置へ畳み込まれることを、
///       バウンディングボックスの包含で確認する.
TEST(InstancedEntity, MakeInstancedAssembly_FlattensWithChildTransform) {
    auto root = i_mod::MakeAssembly("template");
    root->AddEntity(std::make_shared<UnitCubeStub>());
    auto child = i_mod::MakeAssembly("child");
    child->SetGlobalTransform(TranslationX(10.0));
    child->AddEntity(std::make_shared<UnitCubeStub>());
    root->AddChildAssembly(child);

    auto instanced = inspection::MakeInstancedAssembly(
            *root, {Matrix4d::Identity()});

    ASSERT_NE(instanced, nullptr);
    EXPECT_EQ(instanced->MemberCount(), 2u);
    const auto bb = instanced->GetDefinedBoundingBox();
    EXPECT_TRUE(bb.Contains(Vector3d(0.5, 0.5, 0.5)));    // ルートのメンバ
    EXPECT_TRUE(bb.Contains(Vector3d(10.5, 0.5, 0.5)));   // 子のメンバ (+10)
}

/// @brief sourceがnullptrの場合に例外を送出する (異常系)
TEST(InstancedEntity, Constructor_ThrowsInvalidArgumentWhenSourceIsNull) {
    EXPECT_THROW(
            inspection::InstancedEntity(nullptr, {Matrix4d::Identity()}),
            std::invalid_argument);
}

/// @brief メンバのsourceがnullptrの場合に例外を送出する (異常系)
TEST(InstancedEntity, Constructor_ThrowsInvalidArgumentWhenMemberSourceIsNull) {
    std::vector<inspection::InstancedMember> members = {
        {nullptr, Matrix4d::Identity()},
    };
    EXPECT_THROW(
            inspection::InstancedEntity(members, {Matrix4d::Identity()}),
            std::invalid_argument);
}
