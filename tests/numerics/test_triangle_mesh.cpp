/**
 * @file numerics/test_triangle_mesh.cpp
 * @brief numerics/mesh (TriangleMeshT + 基本アルゴリズム) のテスト
 * @author Yayoi Habami
 * @date 2026-06-10
 * @copyright 2026 Yayoi Habami
 * @note テスト対象:
 *       - `TriangleMeshT` のアクセサ (HasNormals/HasUVs/VertexCount/TriangleCount)
 *       - `Validate` (正常系・各異常系)
 *       - `ComputeBoundingBox`
 *       - `RecomputeNormals`
 *       - `CastScalar`
 * @note TODO: 大規模メッシュの性能は対象外 (機能検証のみ).
 */
#include <gtest/gtest.h>

#include "igesio/numerics/mesh/triangle_mesh.h"
#include "igesio/numerics/mesh/algorithms.h"

namespace {

namespace i_num = igesio::numerics;
using i_num::TriangleMeshd;
using i_num::TriangleMeshf;

/// @brief 浮動小数点比較の許容誤差
constexpr double kTol = 1e-12;

/// @brief z=0平面上の単位正方形 (4頂点・2三角形; CCW=+z向き) を生成する
TriangleMeshd MakeUnitQuad() {
    TriangleMeshd mesh;
    mesh.positions.resize(3, 4);
    mesh.positions << 0.0, 1.0, 1.0, 0.0,
                      0.0, 0.0, 1.0, 1.0,
                      0.0, 0.0, 0.0, 0.0;
    mesh.indices = {0, 1, 2, 0, 2, 3};
    return mesh;
}

}  // namespace



/**
 * アクセサ
 */

// 正常系: 頂点数・三角形数・チャンネル有無
TEST(TriangleMeshTest, AccessorsReportCountsAndChannels) {
    auto mesh = MakeUnitQuad();
    EXPECT_EQ(mesh.VertexCount(), 4u);
    EXPECT_EQ(mesh.TriangleCount(), 2u);
    EXPECT_FALSE(mesh.HasNormals());
    EXPECT_FALSE(mesh.HasUVs());

    mesh.normals.setZero(3, 4);
    mesh.uvs.setZero(2, 4);
    EXPECT_TRUE(mesh.HasNormals());
    EXPECT_TRUE(mesh.HasUVs());
}

// 正常系 (退化): 空メッシュのアクセサ
TEST(TriangleMeshTest, EmptyMeshAccessors) {
    TriangleMeshd mesh;
    EXPECT_EQ(mesh.VertexCount(), 0u);
    EXPECT_EQ(mesh.TriangleCount(), 0u);
    EXPECT_FALSE(mesh.HasNormals());
}



/**
 * Validate
 */

// 正常系: 整合の取れたメッシュ (チャンネル・グループ付き) と空メッシュ
TEST(TriangleMeshValidateTest, ValidMeshesPass) {
    auto mesh = MakeUnitQuad();
    EXPECT_TRUE(i_num::Validate(mesh).is_valid);

    mesh.normals.setZero(3, 4);
    mesh.uvs.setZero(2, 4);
    mesh.groups.push_back({"quad", "", 0, 2});
    EXPECT_TRUE(i_num::Validate(mesh).is_valid);

    EXPECT_TRUE(i_num::Validate(TriangleMeshd{}).is_valid);
}

// 異常系: インデックス数が3の倍数でない
TEST(TriangleMeshValidateTest, FailsWhenIndexCountNotMultipleOf3) {
    auto mesh = MakeUnitQuad();
    mesh.indices.push_back(0);
    EXPECT_FALSE(i_num::Validate(mesh).is_valid);
}

// 異常系: 範囲外インデックス (境界: 頂点数ちょうどは不正、頂点数-1は正当)
TEST(TriangleMeshValidateTest, FailsWhenIndexOutOfRange) {
    auto mesh = MakeUnitQuad();
    mesh.indices[0] = 4;  // 頂点数=4のため範囲外
    EXPECT_FALSE(i_num::Validate(mesh).is_valid);

    mesh.indices[0] = 3;  // 最大の正当値
    EXPECT_TRUE(i_num::Validate(mesh).is_valid);
}

// 異常系: 法線・UVチャンネルの列数不一致
TEST(TriangleMeshValidateTest, FailsWhenChannelSizeMismatch) {
    auto mesh = MakeUnitQuad();
    mesh.normals.setZero(3, 3);  // 頂点数4に対して3
    EXPECT_FALSE(i_num::Validate(mesh).is_valid);

    mesh.normals.setZero(3, 4);
    mesh.uvs.setZero(2, 5);  // 頂点数4に対して5
    EXPECT_FALSE(i_num::Validate(mesh).is_valid);
}

// 異常系: 面グループの範囲超過 (境界: ちょうど収まる場合は正当)
TEST(TriangleMeshValidateTest, FailsWhenGroupRangeExceeds) {
    auto mesh = MakeUnitQuad();
    mesh.groups.push_back({"g", "", 1, 2});  // [1, 3) > 三角形数2
    EXPECT_FALSE(i_num::Validate(mesh).is_valid);

    mesh.groups.back() = {"g", "", 1, 1};  // [1, 2) はちょうど収まる
    EXPECT_TRUE(i_num::Validate(mesh).is_valid);
}



/**
 * ComputeBoundingBox
 */

// 正常系: 全頂点を包含する軸平行BB
TEST(TriangleMeshBoundingBoxTest, ContainsAllVertices) {
    auto mesh = MakeUnitQuad();
    mesh.positions.col(2) << 1.0, 1.0, 2.0;  // 1頂点をz方向へ持ち上げる

    const auto bb = i_num::ComputeBoundingBox(mesh);
    const auto vertices = bb.GetFiniteVertices();
    ASSERT_FALSE(vertices.empty());
    igesio::Vector3d lo = vertices.front();
    igesio::Vector3d hi = vertices.front();
    for (const auto& v : vertices) {
        lo = lo.cwiseMin(v);
        hi = hi.cwiseMax(v);
    }
    EXPECT_NEAR(lo.x(), 0.0, kTol);
    EXPECT_NEAR(lo.y(), 0.0, kTol);
    EXPECT_NEAR(lo.z(), 0.0, kTol);
    EXPECT_NEAR(hi.x(), 1.0, kTol);
    EXPECT_NEAR(hi.y(), 1.0, kTol);
    EXPECT_NEAR(hi.z(), 2.0, kTol);
}

// 正常系 (退化): 空メッシュは0次元 (原点・大きさ0) のBB
TEST(TriangleMeshBoundingBoxTest, EmptyMeshYieldsDegenerateBox) {
    const auto bb = i_num::ComputeBoundingBox(TriangleMeshd{});
    EXPECT_TRUE(bb.Is0D());
}



/**
 * RecomputeNormals
 */

// 正常系: z=0平面のCCW三角形は全頂点で+z法線になる
TEST(TriangleMeshRecomputeNormalsTest, PlanarQuadGetsPlusZNormals) {
    auto mesh = MakeUnitQuad();
    i_num::RecomputeNormals(mesh);

    ASSERT_TRUE(mesh.HasNormals());
    ASSERT_EQ(mesh.normals.cols(), 4);
    for (int c = 0; c < 4; ++c) {
        EXPECT_NEAR(mesh.normals(0, c), 0.0, kTol);
        EXPECT_NEAR(mesh.normals(1, c), 0.0, kTol);
        EXPECT_NEAR(mesh.normals(2, c), 1.0, kTol);
    }
}

// 正常系 (退化): どの面にも属さない頂点の法線はゼロベクトル
TEST(TriangleMeshRecomputeNormalsTest, IsolatedVertexGetsZeroNormal) {
    auto mesh = MakeUnitQuad();
    mesh.positions.conservativeResize(3, 5);
    mesh.positions.col(4) << 5.0, 5.0, 5.0;  // 孤立頂点
    i_num::RecomputeNormals(mesh);

    EXPECT_NEAR(mesh.normals.col(4).norm(), 0.0, kTol);
    EXPECT_NEAR(mesh.normals(2, 0), 1.0, kTol);  // 既存頂点は影響なし
}



/**
 * CastScalar
 */

// 正常系: double→float→doubleの往復で値・構造が保持される
TEST(TriangleMeshCastScalarTest, RoundTripPreservesData) {
    auto mesh = MakeUnitQuad();
    i_num::RecomputeNormals(mesh);
    mesh.uvs.setZero(2, 4);
    mesh.groups.push_back({"quad", "mat", 0, 2});

    const auto as_float = i_num::CastScalar<float>(mesh);
    const auto round_trip = i_num::CastScalar<double>(as_float);

    ASSERT_EQ(round_trip.VertexCount(), mesh.VertexCount());
    ASSERT_EQ(round_trip.indices, mesh.indices);
    ASSERT_EQ(round_trip.groups.size(), 1u);
    EXPECT_EQ(round_trip.groups[0].name, "quad");
    EXPECT_EQ(round_trip.groups[0].material_name, "mat");
    // 単位正方形の座標はfloatで正確に表現できるため一致する
    EXPECT_TRUE(round_trip.positions.isApprox(mesh.positions));
    EXPECT_TRUE(round_trip.normals.isApprox(mesh.normals));
}
