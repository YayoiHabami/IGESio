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
 *       - `RecomputeNormalsWithCrease` (折り目での頂点分割と法線)
 *       - `ComputeFaceNormals`
 *       - `ExtractMeshEdges` (全エッジ/特徴エッジの分類)
 *       - `RecomputeNormalsWithCreaseAndExtractEdges` (法線補完とエッジ抽出の共有)
 *       - `ExtractUniqueEdges` (分類なしの軽量版)
 *       - `CastScalar`
 * @note 折り目法線とエッジ抽出は、二分探索による素直な参照実装と、
 *       退化/非多様体/重複/孤立頂点/高次数頂点を含む乱数メッシュで突き合わせる.
 * @note TODO: 大規模メッシュの性能は対象外 (機能検証のみ).
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <random>
#include <utility>
#include <vector>

#include "igesio/numerics/meshes/triangle_mesh.h"
#include "igesio/numerics/meshes/algorithms.h"

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

/// @brief 退化/非多様体/重複/孤立頂点/高次数頂点を含む乱数メッシュを生成する
/// @param seed 乱数の種
/// @return 起伏のある格子面 (6×6頂点) に以下を加えたメッシュ (UVと面グループ付き):
///         (1) 格子の1辺を3枚で共有する非多様体のひれ,
///         (2) 面積ゼロの退化三角形と、同一頂点を重複参照する三角形,
///         (3) 格子の三角形と同じ頂点を持つ重複三角形,
///         (4) 1頂点を40枚の三角形が囲む円錐 (高次数頂点),
///         (5) どの三角形にも属さない孤立頂点
/// @note 格子の高さは乱数で大きく変え、折り目と滑らかな稜線を混在させる
TriangleMeshd MakeMixedRandomMesh(const unsigned seed) {
    std::mt19937 engine(seed);
    std::uniform_real_distribution<double> height(-0.6, 0.6);
    constexpr int kGrid = 6;
    constexpr int kFan = 40;
    std::vector<std::array<double, 3>> points;
    for (int y = 0; y < kGrid; ++y) {
        for (int x = 0; x < kGrid; ++x) {
            points.push_back({double(x), double(y), height(engine)});
        }
    }
    std::vector<std::uint32_t> indices;
    for (int y = 0; y + 1 < kGrid; ++y) {
        for (int x = 0; x + 1 < kGrid; ++x) {
            const auto v = static_cast<std::uint32_t>(y * kGrid + x);
            indices.insert(indices.end(), {v, v + 1, v + kGrid + 1});
            indices.insert(indices.end(), {v, v + kGrid + 1, v + kGrid});
        }
    }
    // (1) 辺(0,1)を共有する3枚目の三角形 (ひれ)
    points.push_back({0.5, -0.5, 1.0});
    indices.insert(indices.end(), {0, 1, std::uint32_t(points.size() - 1)});
    // (2) 退化三角形: 辺(1,2)上の点と、頂点の重複参照
    points.push_back({1.5, 0.0, 0.5 * (points[1][2] + points[2][2])});
    indices.insert(indices.end(), {1, std::uint32_t(points.size() - 1), 2});
    indices.insert(indices.end(), {3, 3, 4});
    // (3) 重複三角形 (格子の先頭の三角形と同じ頂点)
    indices.insert(indices.end(), {0, 1, kGrid + 1});
    // (4) 円錐: 頂点を共有する40枚の三角形
    const auto apex = static_cast<std::uint32_t>(points.size());
    points.push_back({10.0, 10.0, 3.0});
    const auto ring = static_cast<std::uint32_t>(points.size());
    for (int k = 0; k < kFan; ++k) {
        const double angle = 2.0 * igesio::kPi * k / kFan;
        points.push_back({10.0 + std::cos(angle), 10.0 + std::sin(angle),
                          height(engine) * 0.1});
    }
    for (int k = 0; k < kFan; ++k) {
        indices.insert(indices.end(), {apex, ring + std::uint32_t(k),
                                       ring + std::uint32_t((k + 1) % kFan)});
    }
    // (5) 孤立頂点
    points.push_back({-5.0, -5.0, -5.0});

    TriangleMeshd mesh;
    const auto count = static_cast<Eigen::Index>(points.size());
    mesh.positions.resize(3, count);
    mesh.uvs.resize(2, count);
    for (Eigen::Index c = 0; c < count; ++c) {
        const auto& p = points[static_cast<std::size_t>(c)];
        mesh.positions.col(c) << p[0], p[1], p[2];
        mesh.uvs.col(c) << 0.1 * p[0], 0.1 * p[1];
    }
    mesh.indices = std::move(indices);
    mesh.groups.push_back({"grid", "", 0, 50});
    mesh.groups.push_back({"rest", "m", 50,
                           std::uint32_t(mesh.TriangleCount() - 50)});
    return mesh;
}

/// @brief 稜線 (小さい頂点番号, 大きい頂点番号) から隣接三角形の番号列への対応
using ReferenceEdgeMap =
        std::map<std::pair<std::uint32_t, std::uint32_t>,
                 std::vector<std::uint32_t>>;

/// @brief 稜線ごとの隣接三角形を素直な方法で列挙する
/// @param mesh 対象のメッシュ
/// @return 稜線から隣接三角形 (三角形番号の昇順) への対応.
///         長さゼロの稜線は含まない
ReferenceEdgeMap BuildReferenceEdgeMap(const TriangleMeshd& mesh) {
    ReferenceEdgeMap edges;
    for (std::uint32_t t = 0; t < mesh.TriangleCount(); ++t) {
        for (std::uint32_t k = 0; k < 3; ++k) {
            const auto a = mesh.indices[3 * t + k];
            const auto b = mesh.indices[3 * t + (k + 1) % 3];
            if (a != b) edges[{std::min(a, b), std::max(a, b)}].push_back(t);
        }
    }
    return edges;
}

/// @brief 三角形ごとの外積 (面積の2倍の大きさ) を計算する
/// @param mesh 対象のメッシュ
/// @return 三角形ごとの外積ベクトル
std::vector<Eigen::Vector3d> ComputeReferenceCross(const TriangleMeshd& mesh) {
    std::vector<Eigen::Vector3d> cross(mesh.TriangleCount());
    for (std::size_t t = 0; t < cross.size(); ++t) {
        const Eigen::Vector3d p0 = mesh.positions.col(mesh.indices[3 * t]);
        const Eigen::Vector3d e1 =
                Eigen::Vector3d(mesh.positions.col(mesh.indices[3 * t + 1])) - p0;
        const Eigen::Vector3d e2 =
                Eigen::Vector3d(mesh.positions.col(mesh.indices[3 * t + 2])) - p0;
        cross[t] = e1.cross(e2);
    }
    return cross;
}

/// @brief 外積から単位面法線 (退化三角形はゼロベクトル) を計算する
/// @param cross 三角形の外積ベクトル
/// @return 単位面法線
Eigen::Vector3d ReferenceUnit(const Eigen::Vector3d& cross) {
    const double norm = cross.norm();
    return (norm > 0.0) ? Eigen::Vector3d(cross / norm)
                        : Eigen::Vector3d(Eigen::Vector3d::Zero());
}

/// @brief 稜線を挟む2三角形が滑らかにつながるか判定する
/// @param tris 稜線の隣接三角形
/// @param cross 三角形ごとの外積
/// @param crease_angle_cos 折り目判定のしきい値
/// @return 隣接がちょうど2枚、両方とも非退化、かつ法線の内積がしきい値以上の
///         場合はtrue
bool IsReferenceSmoothEdge(const std::vector<std::uint32_t>& tris,
                           const std::vector<Eigen::Vector3d>& cross,
                           const double crease_angle_cos) {
    if (tris.size() != 2) return false;
    const auto n0 = ReferenceUnit(cross[tris[0]]);
    const auto n1 = ReferenceUnit(cross[tris[1]]);
    if (n0.squaredNorm() == 0.0 || n1.squaredNorm() == 0.0) return false;
    return n0.dot(n1) >= crease_angle_cos;
}

/// @brief 二分探索 (std::map) で稜線を引く素直な実装で折り目法線を計算する
/// @param[in,out] mesh 対象のメッシュ
/// @param crease_angle_cos 折り目判定のしきい値
/// @note 頂点の複製順と法線の加算順は公開実装と同じ規約 (頂点番号→三角形番号→
///       コーナー番号の昇順) に従う
void ReferenceRecomputeNormalsWithCrease(TriangleMeshd& mesh,
                                         const double crease_angle_cos) {
    const auto cross = ComputeReferenceCross(mesh);
    const auto edges = BuildReferenceEdgeMap(mesh);
    std::vector<std::array<std::uint32_t, 3>> corners;
    for (std::uint32_t s = 0; s < mesh.indices.size(); ++s) {
        corners.push_back({mesh.indices[s], s / 3, s % 3});
    }
    std::sort(corners.begin(), corners.end());

    std::vector<Eigen::Vector3d> sums(std::size_t(mesh.positions.cols()),
                                      Eigen::Vector3d::Zero());
    std::vector<Eigen::Index> duplicated_from;
    auto new_indices = mesh.indices;
    for (std::size_t i = 0; i < corners.size();) {
        std::size_t j = i;
        while (j < corners.size() && corners[j][0] == corners[i][0]) ++j;
        // 頂点まわりのコーナーを、滑らかな稜線で隣り合うもの同士で併合する
        std::vector<std::size_t> parent(j - i);
        for (std::size_t q = 0; q < parent.size(); ++q) parent[q] = q;
        const auto root_of = [&parent](std::size_t x) {
            while (parent[x] != x) x = parent[x];
            return x;
        };
        for (std::size_t c = i; c < j; ++c) {
            const auto v = corners[c][0], t = corners[c][1], k = corners[c][2];
            for (const auto other : {mesh.indices[3 * t + (k + 1) % 3],
                                     mesh.indices[3 * t + (k + 2) % 3]}) {
                if (other == v) continue;
                const auto& tris = edges.at({std::min(v, other),
                                             std::max(v, other)});
                if (!IsReferenceSmoothEdge(tris, cross, crease_angle_cos)) {
                    continue;
                }
                const auto neighbor = (tris[0] == t) ? tris[1] : tris[0];
                for (std::size_t q = i; q < j; ++q) {
                    if (corners[q][1] != neighbor) continue;
                    parent[root_of(q - i)] = root_of(c - i);
                    break;
                }
            }
        }
        // 最初に現れたクラスタが元の頂点番号を引き継ぎ、以降は複製する
        std::vector<std::pair<std::size_t, std::uint32_t>> assigned;
        for (std::size_t c = i; c < j; ++c) {
            const auto root = root_of(c - i);
            auto it = std::find_if(assigned.begin(), assigned.end(),
                                   [root](const auto& a) {
                                       return a.first == root;
                                   });
            if (it == assigned.end()) {
                auto column = corners[c][0];
                if (!assigned.empty()) {
                    column = static_cast<std::uint32_t>(sums.size());
                    duplicated_from.push_back(corners[c][0]);
                    sums.push_back(Eigen::Vector3d::Zero());
                }
                assigned.emplace_back(root, column);
                it = std::prev(assigned.end());
            }
            new_indices[3 * corners[c][1] + corners[c][2]] = it->second;
            sums[it->second] += cross[corners[c][1]];
        }
        i = j;
    }

    const auto original = mesh.positions.cols();
    const auto total = original + Eigen::Index(duplicated_from.size());
    mesh.positions.conservativeResize(3, total);
    if (mesh.HasUVs()) mesh.uvs.conservativeResize(2, total);
    for (std::size_t d = 0; d < duplicated_from.size(); ++d) {
        const auto dst = original + Eigen::Index(d);
        mesh.positions.col(dst) = mesh.positions.col(duplicated_from[d]);
        if (mesh.uvs.cols() == total) {
            mesh.uvs.col(dst) = mesh.uvs.col(duplicated_from[d]);
        }
    }
    mesh.indices = std::move(new_indices);
    mesh.normals.setZero(3, total);
    for (std::size_t c = 0; c < sums.size(); ++c) {
        mesh.normals.col(Eigen::Index(c)) = ReferenceUnit(sums[c]);
    }
}

/// @brief 素直な実装 (std::map) でエッジ抽出の参照結果を計算する
/// @param mesh 対象のメッシュ
/// @param crease_angle_cos 折り目判定のしきい値
/// @return ExtractMeshEdgesと同じ規約のエッジ抽出結果
i_num::MeshEdgeSet ReferenceExtractMeshEdges(const TriangleMeshd& mesh,
                                             const double crease_angle_cos) {
    const auto cross = ComputeReferenceCross(mesh);
    i_num::MeshEdgeSet result;
    for (const auto& [key, tris] : BuildReferenceEdgeMap(mesh)) {
        const std::array<std::uint32_t, 2> pair = {key.first, key.second};
        result.all_edges.push_back(pair);
        if (!IsReferenceSmoothEdge(tris, cross, crease_angle_cos)) {
            result.feature_edges.push_back(pair);
        }
    }
    return result;
}

/// @brief 2つの行列の成分ごとの差の絶対値の最大を計算する
/// @param a 比較する行列
/// @param b 比較する行列 (aと同じ大きさ)
/// @return 差の絶対値の最大. 空の行列の場合は0
double MaxAbsDifference(const Eigen::MatrixXd& a, const Eigen::MatrixXd& b) {
    if (a.size() == 0) return 0.0;
    return (a - b).cwiseAbs().maxCoeff();
}

/// @brief 2つのメッシュの頂点/法線/UV/インデックス/グループが一致することを検証する
/// @param actual 検証対象のメッシュ
/// @param expected 期待するメッシュ
void ExpectSameMesh(const TriangleMeshd& actual, const TriangleMeshd& expected) {
    ASSERT_EQ(actual.positions.cols(), expected.positions.cols());
    ASSERT_EQ(actual.normals.cols(), expected.normals.cols());
    ASSERT_EQ(actual.uvs.cols(), expected.uvs.cols());
    EXPECT_EQ(actual.indices, expected.indices);
    EXPECT_NEAR(MaxAbsDifference(actual.positions, expected.positions), 0.0,
                kTol);
    EXPECT_NEAR(MaxAbsDifference(actual.normals, expected.normals), 0.0, kTol);
    EXPECT_NEAR(MaxAbsDifference(actual.uvs, expected.uvs), 0.0, kTol);
    ASSERT_EQ(actual.groups.size(), expected.groups.size());
    for (std::size_t g = 0; g < actual.groups.size(); ++g) {
        EXPECT_EQ(actual.groups[g].first_triangle,
                  expected.groups[g].first_triangle);
        EXPECT_EQ(actual.groups[g].triangle_count,
                  expected.groups[g].triangle_count);
    }
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
 * RecomputeNormalsWithCrease
 */

// 正常系: 同一平面の2三角形は分割されず、全頂点で+z法線になる
TEST(TriangleMeshCreaseNormalsTest, PlanarQuadIsNotSplit) {
    auto mesh = MakeUnitQuad();
    i_num::RecomputeNormalsWithCrease(mesh, kCos30Deg);

    ASSERT_EQ(mesh.VertexCount(), 4u);  // 複製なし
    EXPECT_EQ(mesh.indices,
              std::vector<std::uint32_t>({0, 1, 2, 0, 2, 3}));  // 張り替えなし
    for (int c = 0; c < 4; ++c) {
        EXPECT_NEAR(mesh.normals(0, c), 0.0, kTol);
        EXPECT_NEAR(mesh.normals(1, c), 0.0, kTol);
        EXPECT_NEAR(mesh.normals(2, c), 1.0, kTol);
    }
}

// 正常系 (境界値): しきい値をわずかに下回る折り角では法線が共有される
TEST(TriangleMeshCreaseNormalsTest, FoldJustBelowThresholdKeepsSharedNormals) {
    auto mesh = MakeFoldedQuad(29.0 * igesio::kPi / 180.0);
    i_num::RecomputeNormalsWithCrease(mesh, kCos30Deg);

    ASSERT_EQ(mesh.VertexCount(), 4u);
    // 2三角形は同面積のため、共有辺の頂点は両面法線のちょうど中間を向く
    const auto faces = i_num::ComputeFaceNormals(mesh);
    const igesio::Vector3d average = (faces.col(0) + faces.col(1)).normalized();
    EXPECT_NEAR((mesh.normals.col(0) - average).norm(), 0.0, kTol);
    EXPECT_NEAR((mesh.normals.col(1) - average).norm(), 0.0, kTol);
}

// 正常系 (境界値): しきい値をわずかに超える折り角では共有辺の頂点が分割される
TEST(TriangleMeshCreaseNormalsTest, FoldJustAboveThresholdSplitsSharedVertices) {
    auto mesh = MakeFoldedQuad(31.0 * igesio::kPi / 180.0);
    i_num::RecomputeNormalsWithCrease(mesh, kCos30Deg);

    // 共有辺の2頂点が2クラスタへ分かれ、頂点が2つ増える
    ASSERT_EQ(mesh.VertexCount(), 6u);
    ASSERT_EQ(mesh.TriangleCount(), 2u);

    // 各コーナーの法線は、そのコーナーが属する三角形の面法線に一致する
    const auto faces = i_num::ComputeFaceNormals(mesh);
    for (std::size_t t = 0; t < 2; ++t) {
        for (std::size_t k = 0; k < 3; ++k) {
            const auto column = mesh.indices[3 * t + k];
            EXPECT_NEAR((mesh.normals.col(column) - faces.col(t)).norm(),
                        0.0, kTol);
        }
    }
}

// 正常系: 閉メッシュ (四面体) は全稜線が折り目のため頂点が面毎に分かれる
TEST(TriangleMeshCreaseNormalsTest, TetrahedronSplitsIntoPerFaceCorners) {
    auto mesh = MakeTetrahedron();
    i_num::RecomputeNormalsWithCrease(mesh, kCos30Deg);

    ASSERT_EQ(mesh.VertexCount(), 12u);  // 4頂点 × 各3面
    ASSERT_EQ(mesh.TriangleCount(), 4u);
    EXPECT_TRUE(i_num::Validate(mesh).is_valid);  // 分割後も整合している

    const auto faces = i_num::ComputeFaceNormals(mesh);
    for (std::size_t t = 0; t < 4; ++t) {
        for (std::size_t k = 0; k < 3; ++k) {
            const auto column = mesh.indices[3 * t + k];
            EXPECT_NEAR((mesh.normals.col(column) - faces.col(t)).norm(),
                        0.0, kTol);
        }
    }
}

// 正常系 (境界値): しきい値-1では全稜線で併合され、RecomputeNormalsと一致する
TEST(TriangleMeshCreaseNormalsTest, ThresholdMinusOneMatchesRecomputeNormals) {
    auto expected = MakeTetrahedron();
    i_num::RecomputeNormals(expected);
    auto mesh = MakeTetrahedron();
    i_num::RecomputeNormalsWithCrease(mesh, -1.0);

    ASSERT_EQ(mesh.VertexCount(), expected.VertexCount());
    EXPECT_EQ(mesh.indices, expected.indices);
    for (int c = 0; c < 4; ++c) {
        EXPECT_NEAR((mesh.normals.col(c) - expected.normals.col(c)).norm(),
                    0.0, kTol);
    }
}

// 正常系: 分割で複製した頂点はUVを引き継ぎ、groupsの三角形範囲は変わらない
TEST(TriangleMeshCreaseNormalsTest, SplitCopiesUVsAndKeepsGroups) {
    auto mesh = MakeFoldedQuad(90.0 * igesio::kPi / 180.0);
    mesh.uvs.resize(2, 4);
    mesh.uvs << 0.0, 1.0, 0.5, 0.5,
                0.0, 0.0, 1.0, 1.0;
    mesh.groups.push_back({"fold", "mat", 0, 2});

    i_num::RecomputeNormalsWithCrease(mesh, kCos30Deg);

    ASSERT_EQ(mesh.VertexCount(), 6u);
    ASSERT_EQ(mesh.uvs.cols(), 6);
    // 複製した列4・5は、複製元である頂点0・1のUVを引き継ぐ
    EXPECT_NEAR((mesh.uvs.col(4) - mesh.uvs.col(0)).norm(), 0.0, kTol);
    EXPECT_NEAR((mesh.uvs.col(5) - mesh.uvs.col(1)).norm(), 0.0, kTol);

    ASSERT_EQ(mesh.groups.size(), 1u);
    EXPECT_EQ(mesh.groups[0].first_triangle, 0u);
    EXPECT_EQ(mesh.groups[0].triangle_count, 2u);
}

// 正常系 (退化): どの面にも属さない頂点は列が残り、法線はゼロベクトルになる
TEST(TriangleMeshCreaseNormalsTest, IsolatedVertexIsKeptWithZeroNormal) {
    auto mesh = MakeUnitQuad();
    mesh.positions.conservativeResize(3, 5);
    mesh.positions.col(4) << 5.0, 5.0, 5.0;  // 孤立頂点
    i_num::RecomputeNormalsWithCrease(mesh, kCos30Deg);

    ASSERT_EQ(mesh.VertexCount(), 5u);
    EXPECT_NEAR(mesh.normals.col(4).norm(), 0.0, kTol);
    EXPECT_NEAR(mesh.normals(2, 0), 1.0, kTol);  // 既存頂点は影響なし
}

// 正常系 (退化): 三角形を持たないメッシュの法線は全てゼロベクトルになる
TEST(TriangleMeshCreaseNormalsTest, MeshWithoutTrianglesGetsZeroNormals) {
    TriangleMeshd mesh;
    mesh.positions.resize(3, 2);
    mesh.positions << 0.0, 1.0,
                      0.0, 0.0,
                      0.0, 0.0;
    i_num::RecomputeNormalsWithCrease(mesh, kCos30Deg);

    ASSERT_EQ(mesh.normals.cols(), 2);
    EXPECT_NEAR(mesh.normals.norm(), 0.0, kTol);
}

// 正常系 (退化): 退化三角形が絡む稜線では併合せず、法線にも寄与しない
TEST(TriangleMeshCreaseNormalsTest, DegenerateTriangleDoesNotContribute) {
    auto mesh = MakeUnitQuad();
    mesh.positions.conservativeResize(3, 5);
    mesh.positions.col(4) << 2.0, 0.0, 0.0;  // 頂点1・2と一直線上ではない位置
    mesh.indices.insert(mesh.indices.end(), {1, 1, 4});  // 面積ゼロの三角形

    i_num::RecomputeNormalsWithCrease(mesh, kCos30Deg);

    // 退化三角形のみに属する頂点4の法線はゼロベクトル
    EXPECT_NEAR(mesh.normals.col(4).norm(), 0.0, kTol);
    // 頂点1は退化三角形の寄与を受けず、平面の+z法線を保つ
    EXPECT_NEAR(mesh.normals(2, 1), 1.0, kTol);
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
 * 参照実装との突き合わせ (RecomputeNormalsWithCrease / ExtractMeshEdges /
 * RecomputeNormalsWithCreaseAndExtractEdges)
 */

/// @brief 乱数メッシュの種を与える突き合わせテスト
class TriangleMeshReferenceTest : public ::testing::TestWithParam<unsigned> {};

// 正常系: 折り目法線が参照実装と一致する (頂点の複製順/インデックス/法線/UV)
TEST_P(TriangleMeshReferenceTest, RecomputeNormalsWithCrease_MatchesReference) {
    auto actual = MakeMixedRandomMesh(GetParam());
    auto expected = actual;
    i_num::RecomputeNormalsWithCrease(actual, kCos30Deg);
    ReferenceRecomputeNormalsWithCrease(expected, kCos30Deg);
    ExpectSameMesh(actual, expected);
}

// 正常系: エッジ抽出が参照実装と一致する (全エッジ/特徴エッジの集合と順序)
TEST_P(TriangleMeshReferenceTest, ExtractMeshEdges_MatchesReference) {
    const auto mesh = MakeMixedRandomMesh(GetParam());
    const auto actual = i_num::ExtractMeshEdges(mesh, kCos30Deg);
    const auto expected = ReferenceExtractMeshEdges(mesh, kCos30Deg);
    EXPECT_EQ(actual.all_edges, expected.all_edges);
    EXPECT_EQ(actual.feature_edges, expected.feature_edges);
}

// 正常系: 共有版は、法線補完とエッジ抽出を個別に呼んだ結果と一致する
TEST_P(TriangleMeshReferenceTest,
       RecomputeNormalsWithCreaseAndExtractEdges_MatchesSeparateCalls) {
    const auto mesh = MakeMixedRandomMesh(GetParam());
    auto combined = mesh;
    const auto combined_edges =
            i_num::RecomputeNormalsWithCreaseAndExtractEdges(combined,
                                                             kCos30Deg);
    auto separate = mesh;
    i_num::RecomputeNormalsWithCrease(separate, kCos30Deg);
    const auto separate_edges = i_num::ExtractMeshEdges(mesh, kCos30Deg);

    ExpectSameMesh(combined, separate);
    EXPECT_EQ(combined_edges.all_edges, separate_edges.all_edges);
    EXPECT_EQ(combined_edges.feature_edges, separate_edges.feature_edges);
}

// 乱数の種 (各種で格子の起伏と円錐の高さが変わる)
INSTANTIATE_TEST_SUITE_P(Seeds, TriangleMeshReferenceTest,
                         ::testing::Values(1u, 2u, 3u, 42u, 2026u));

// 正常系 (退化): 三角形の無いメッシュでは、共有版も法線をゼロにし空のエッジを返す
TEST(TriangleMeshCreaseAndEdgesTest, MeshWithoutTrianglesGetsZeroNormals) {
    TriangleMeshd mesh;
    mesh.positions.setZero(3, 2);
    const auto edges =
            i_num::RecomputeNormalsWithCreaseAndExtractEdges(mesh, kCos30Deg);

    ASSERT_EQ(mesh.normals.cols(), 2);
    EXPECT_NEAR(mesh.normals.cwiseAbs().maxCoeff(), 0.0, kTol);
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
