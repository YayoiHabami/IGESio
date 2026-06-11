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
 *       - `ComputeFaceNormals`
 *       - `ExtractMeshEdges` (全エッジ/特徴エッジの分類)
 *       - `ExtractUniqueEdges` (分類なしの軽量版)
 *       - `CastScalar`
 * @note TODO: 大規模メッシュの性能は対象外 (機能検証のみ).
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "igesio/numerics/mesh/triangle_mesh.h"
#include "igesio/numerics/mesh/algorithms.h"

namespace {

namespace i_num = igesio::numerics;
using i_num::TriangleMeshd;
using i_num::TriangleMeshf;

/// @brief 浮動小数点比較の許容誤差
constexpr double kTol = 1e-12;

/// @brief 折り目判定のしきい値 (cos(30°); ExtractMeshEdgesのテスト用)
const double kCos30Deg = std::cos(30.0 * igesio::kPi / 180.0);

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

/// @brief 共有辺(頂点0-1)で指定角度だけ折った2三角形を生成する
/// @param fold_angle_rad 2面の単位面法線のなす角 [rad] (0=同一平面)
/// @note 三角形(0,1,2)はz=0平面 (法線+z)、三角形(0,3,1)の法線は
///       (0, sinφ, cosφ) となり、法線間の角度がちょうどφになる
TriangleMeshd MakeFoldedQuad(const double fold_angle_rad) {
    TriangleMeshd mesh;
    mesh.positions.resize(3, 4);
    mesh.positions << 0.0, 1.0, 0.5, 0.5,
                      0.0, 0.0, 1.0, -std::cos(fold_angle_rad),
                      0.0, 0.0, 0.0, std::sin(fold_angle_rad);
    mesh.indices = {0, 1, 2, 0, 3, 1};
    return mesh;
}

/// @brief 外向き一貫の向き付けをした単位四面体 (閉メッシュ・境界なし) を生成する
/// @note 隣接面法線のなす角は全エッジで90°以上 (折り目しきい値30°を大きく超える)
TriangleMeshd MakeTetrahedron() {
    TriangleMeshd mesh;
    mesh.positions.resize(3, 4);
    mesh.positions << 0.0, 1.0, 0.0, 0.0,
                      0.0, 0.0, 1.0, 0.0,
                      0.0, 0.0, 0.0, 1.0;
    mesh.indices = {0, 2, 1,   // 底面 (-z)
                    0, 1, 3,   // -y側
                    0, 3, 2,   // -x側
                    1, 2, 3};  // 斜面
    return mesh;
}

/// @brief エッジ集合が指定の頂点ペア (昇順) を含むか
bool ContainsEdge(const std::vector<std::array<std::uint32_t, 2>>& edges,
                  const std::uint32_t v0, const std::uint32_t v1) {
    const std::array<std::uint32_t, 2> target = {v0, v1};
    return std::find(edges.begin(), edges.end(), target) != edges.end();
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
 * ComputeFaceNormals
 */

// 正常系: z=0平面のCCW三角形の面法線は+z
TEST(TriangleMeshFaceNormalsTest, PlanarQuadGetsPlusZFaceNormals) {
    const auto mesh = MakeUnitQuad();
    const auto normals = i_num::ComputeFaceNormals(mesh);

    ASSERT_EQ(normals.cols(), 2);
    for (int c = 0; c < 2; ++c) {
        EXPECT_NEAR(normals(0, c), 0.0, kTol);
        EXPECT_NEAR(normals(1, c), 0.0, kTol);
        EXPECT_NEAR(normals(2, c), 1.0, kTol);
    }
}

// 正常系 (退化): 面積ゼロの三角形の面法線はゼロベクトル
TEST(TriangleMeshFaceNormalsTest, DegenerateTriangleGetsZeroNormal) {
    TriangleMeshd mesh;
    mesh.positions.resize(3, 3);
    mesh.positions << 0.0, 1.0, 0.5,
                      0.0, 0.0, 0.0,
                      0.0, 0.0, 0.0;  // 3点が一直線上
    mesh.indices = {0, 1, 2};

    const auto normals = i_num::ComputeFaceNormals(mesh);
    ASSERT_EQ(normals.cols(), 1);
    EXPECT_NEAR(normals.col(0).norm(), 0.0, kTol);
}



/**
 * ExtractMeshEdges
 */

// 正常系: 同一平面の2三角形は外周4辺のみが特徴エッジ (共有対角線は含まない)
TEST(TriangleMeshExtractEdgesTest, FlatQuadHasBoundaryFeatureEdgesOnly) {
    const auto edges = i_num::ExtractMeshEdges(MakeUnitQuad(), kCos30Deg);

    EXPECT_EQ(edges.all_edges.size(), 5u);      // 外周4 + 対角線1
    EXPECT_EQ(edges.feature_edges.size(), 4u);  // 境界 (外周) のみ
    EXPECT_TRUE(ContainsEdge(edges.all_edges, 0, 2));      // 対角線は全エッジに含む
    EXPECT_FALSE(ContainsEdge(edges.feature_edges, 0, 2));  // 特徴エッジには含まない
}

// 正常系 (境界値): しきい値をわずかに超える折り角は折り目と判定される
TEST(TriangleMeshExtractEdgesTest, FoldJustAboveThresholdIsCrease) {
    const auto mesh = MakeFoldedQuad(31.0 * igesio::kPi / 180.0);
    const auto edges = i_num::ExtractMeshEdges(mesh, kCos30Deg);

    EXPECT_EQ(edges.all_edges.size(), 5u);
    EXPECT_EQ(edges.feature_edges.size(), 5u);  // 境界4 + 折り目 (共有辺)
    EXPECT_TRUE(ContainsEdge(edges.feature_edges, 0, 1));
}

// 正常系 (境界値): しきい値をわずかに下回る折り角は折り目と判定されない
TEST(TriangleMeshExtractEdgesTest, FoldJustBelowThresholdIsNotCrease) {
    const auto mesh = MakeFoldedQuad(29.0 * igesio::kPi / 180.0);
    const auto edges = i_num::ExtractMeshEdges(mesh, kCos30Deg);

    EXPECT_EQ(edges.all_edges.size(), 5u);
    EXPECT_EQ(edges.feature_edges.size(), 4u);  // 境界のみ
    EXPECT_FALSE(ContainsEdge(edges.feature_edges, 0, 1));
}

// 正常系: 閉メッシュ (四面体) は境界を持たず、全エッジが折り目になる
TEST(TriangleMeshExtractEdgesTest, ClosedTetrahedronHasAllCreaseEdges) {
    const auto edges = i_num::ExtractMeshEdges(MakeTetrahedron(), kCos30Deg);

    EXPECT_EQ(edges.all_edges.size(), 6u);
    EXPECT_EQ(edges.feature_edges.size(), 6u);
}

// 正常系 (退化): 1辺を3枚が共有する非多様体エッジは特徴エッジになる
TEST(TriangleMeshExtractEdgesTest, NonManifoldEdgeIsFeature) {
    TriangleMeshd mesh;
    mesh.positions.resize(3, 5);
    mesh.positions << 0.0, 1.0, 0.5, 0.5, 0.5,
                      0.0, 0.0, 1.0, -1.0, 0.0,
                      0.0, 0.0, 0.0, 0.0, 1.0;
    mesh.indices = {0, 1, 2, 0, 3, 1, 0, 1, 4};  // 辺(0,1)を3枚が共有

    const auto edges = i_num::ExtractMeshEdges(mesh, kCos30Deg);
    EXPECT_EQ(edges.all_edges.size(), 7u);
    EXPECT_TRUE(ContainsEdge(edges.feature_edges, 0, 1));
}

// 正常系 (退化): 面法線が計算不能な退化三角形が絡むエッジは特徴エッジ扱い
TEST(TriangleMeshExtractEdgesTest, EdgeAdjacentToDegenerateTriangleIsFeature) {
    TriangleMeshd mesh;
    mesh.positions.resize(3, 4);
    mesh.positions << 0.0, 1.0, 0.5, 0.5,
                      0.0, 0.0, 1.0, 0.0,
                      0.0, 0.0, 0.0, 0.0;  // 頂点3は辺(0,1)上 (面積ゼロ)
    mesh.indices = {0, 1, 2, 0, 3, 1};

    const auto edges = i_num::ExtractMeshEdges(mesh, kCos30Deg);
    // 辺(0,1)は隣接2枚だが、片方が退化のため安全側で特徴エッジになる
    EXPECT_TRUE(ContainsEdge(edges.feature_edges, 0, 1));
}

// 正常系 (退化): 同一頂点を結ぶ長さゼロのエッジは列挙されない
TEST(TriangleMeshExtractEdgesTest, ZeroLengthEdgeIsExcluded) {
    TriangleMeshd mesh;
    mesh.positions.resize(3, 2);
    mesh.positions << 0.0, 1.0,
                      0.0, 0.0,
                      0.0, 0.0;
    mesh.indices = {0, 0, 1};  // 頂点0を重複参照する退化三角形

    const auto edges = i_num::ExtractMeshEdges(mesh, kCos30Deg);
    ASSERT_EQ(edges.all_edges.size(), 1u);  // (0,1)のみ ((0,0)は除外)
    EXPECT_TRUE(ContainsEdge(edges.all_edges, 0, 1));
}

// 正常系 (退化): 空メッシュは空のエッジ集合を返す
TEST(TriangleMeshExtractEdgesTest, EmptyMeshYieldsNoEdges) {
    const auto edges = i_num::ExtractMeshEdges(TriangleMeshd{}, kCos30Deg);
    EXPECT_TRUE(edges.all_edges.empty());
    EXPECT_TRUE(edges.feature_edges.empty());
}



/**
 * ExtractUniqueEdges
 */

// 正常系: 共有辺を持つ平面正方形は5本のユニークエッジ (4辺+対角線) を返す
TEST(TriangleMeshExtractUniqueEdgesTest, FlatQuadHasFiveUniqueEdges) {
    const auto edges = i_num::ExtractUniqueEdges(MakeUnitQuad());
    ASSERT_EQ(edges.size(), 5u);
    EXPECT_TRUE(ContainsEdge(edges, 0, 1));
    EXPECT_TRUE(ContainsEdge(edges, 1, 2));
    EXPECT_TRUE(ContainsEdge(edges, 2, 3));
    EXPECT_TRUE(ContainsEdge(edges, 0, 3));
    EXPECT_TRUE(ContainsEdge(edges, 0, 2));  // 対角線 (共有辺は1本に集約)
}

// 正常系: ExtractMeshEdgesのall_edgesと同じ集合・同じ順序を返す
TEST(TriangleMeshExtractUniqueEdgesTest, MatchesExtractMeshEdgesAllEdges) {
    const auto mesh = MakeTetrahedron();
    const auto unique_edges = i_num::ExtractUniqueEdges(mesh);
    const auto classified = i_num::ExtractMeshEdges(mesh, kCos30Deg);
    // 両者とも (小頂点, 大頂点) の辞書順で列挙するため完全一致する
    EXPECT_EQ(unique_edges, classified.all_edges);
    EXPECT_EQ(unique_edges.size(), 6u);  // 四面体のエッジ数
}

// 正常系 (退化): 同一頂点を結ぶ長さゼロのエッジは列挙されない
TEST(TriangleMeshExtractUniqueEdgesTest, ZeroLengthEdgeIsExcluded) {
    TriangleMeshd mesh;
    mesh.positions.resize(3, 2);
    mesh.positions << 0.0, 1.0,
                      0.0, 0.0,
                      0.0, 0.0;
    mesh.indices = {0, 0, 1};  // 頂点0を重複参照する退化三角形

    const auto edges = i_num::ExtractUniqueEdges(mesh);
    ASSERT_EQ(edges.size(), 1u);  // (0,1)のみ ((0,0)は除外)
    EXPECT_TRUE(ContainsEdge(edges, 0, 1));
}

// 正常系 (退化): 空メッシュは空のエッジ集合を返す
TEST(TriangleMeshExtractUniqueEdgesTest, EmptyMeshYieldsNoEdges) {
    EXPECT_TRUE(i_num::ExtractUniqueEdges(TriangleMeshd{}).empty());
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
