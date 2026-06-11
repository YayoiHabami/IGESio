/**
 * @file entities/meshes/test_mesh_entity.cpp
 * @brief entities/meshes/mesh_entity.h (MeshEntity) のテスト
 * @author Yayoi Habami
 * @date 2026-06-10
 * @copyright 2026 Yayoi Habami
 * @note テスト対象:
 *       - `MeshEntity` のコンストラクタ (検証付き)
 *       - `Mesh` / `SetMesh` (リビジョンバンプ・不正メッシュの拒否)
 *       - `GetDefinedBoundingBox`
 *       - 識別子 (GetType / IsSupported)
 * @note Assemblyへの保存・WriteIgesスキップは非IGESエンティティ一般として
 *       tests/models/test_non_iges_entity.cppで検証済みのため対象外.
 */
#include <gtest/gtest.h>

#include <stdexcept>
#include <utility>

#include "igesio/numerics/meshes/triangle_mesh.h"
#include "igesio/entities/meshes/mesh_entity.h"

namespace {

namespace i_ent = igesio::entities;
namespace i_num = igesio::numerics;

/// @brief 浮動小数点比較の許容誤差
constexpr double kTol = 1e-12;

/// @brief z=0平面上の単位正方形 (4頂点・2三角形) を生成する
i_num::TriangleMeshd MakeUnitQuad() {
    i_num::TriangleMeshd mesh;
    mesh.positions.resize(3, 4);
    mesh.positions << 0.0, 1.0, 1.0, 0.0,
                      0.0, 0.0, 1.0, 1.0,
                      0.0, 0.0, 0.0, 0.0;
    mesh.indices = {0, 1, 2, 0, 2, 3};
    return mesh;
}

/// @brief 範囲外インデックスを含む不正メッシュを生成する
i_num::TriangleMeshd MakeInvalidMesh() {
    auto mesh = MakeUnitQuad();
    mesh.indices[0] = 10;  // 頂点数4に対して範囲外
    return mesh;
}

}  // namespace



/**
 * 正常系
 */

// コンストラクタ: メッシュを保持し、kNonIgesの識別子を持つ
TEST(MeshEntityTest, ConstructorStoresMesh) {
    i_ent::MeshEntity entity(MakeUnitQuad());

    EXPECT_EQ(entity.GetType(), i_ent::EntityType::kNonIges);
    EXPECT_EQ(entity.GetFormNumber(), 0);
    EXPECT_TRUE(entity.IsSupported());
    EXPECT_EQ(entity.Mesh().VertexCount(), 4u);
    EXPECT_EQ(entity.Mesh().TriangleCount(), 2u);
}

// GetDefinedBoundingBoxが全頂点を包含する
TEST(MeshEntityTest, DefinedBoundingBoxContainsVertices) {
    i_ent::MeshEntity entity(MakeUnitQuad());

    const auto bb = entity.GetDefinedBoundingBox();
    EXPECT_TRUE(bb.Contains({0.5, 0.5, 0.0}));
    EXPECT_FALSE(bb.Contains({2.0, 0.5, 0.0}));
}

// SetMeshがメッシュを差し替え、ジオメトリリビジョンをバンプする
TEST(MeshEntityTest, SetMeshBumpsGeometryRevision) {
    i_ent::MeshEntity entity(MakeUnitQuad());
    const auto rev0 = entity.GeometryRevision();

    auto larger = MakeUnitQuad();
    larger.positions *= 2.0;
    entity.SetMesh(std::move(larger));

    EXPECT_GT(entity.GeometryRevision(), rev0);
    const auto bb = entity.GetDefinedBoundingBox();
    EXPECT_TRUE(bb.Contains({1.5, 1.5, 0.0}));
}



/**
 * 異常系
 */

// 不正メッシュでの構築はstd::invalid_argument
TEST(MeshEntityTest, ConstructorThrowsInvalidArgumentWhenMeshInvalid) {
    EXPECT_THROW(i_ent::MeshEntity{MakeInvalidMesh()}, std::invalid_argument);
}

// 不正メッシュへのSetMeshは拒否され、保持内容・リビジョンが変化しない
TEST(MeshEntityTest, SetMeshRejectsInvalidMeshWithoutSideEffects) {
    i_ent::MeshEntity entity(MakeUnitQuad());
    const auto rev0 = entity.GeometryRevision();

    EXPECT_THROW(entity.SetMesh(MakeInvalidMesh()), std::invalid_argument);

    EXPECT_EQ(entity.GeometryRevision(), rev0);
    EXPECT_EQ(entity.Mesh().TriangleCount(), 2u);
    EXPECT_NEAR(entity.Mesh().positions(0, 1), 1.0, kTol);
}
