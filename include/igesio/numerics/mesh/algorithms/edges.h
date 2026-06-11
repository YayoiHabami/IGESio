/**
 * @file numerics/mesh/algorithms/edges.h
 * @brief 三角形メッシュ (TriangleMeshT) のエッジ・特徴エッジを抽出するアルゴリズム
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 */
#ifndef IGESIO_NUMERICS_MESH_ALGORITHMS_EDGES_H_
#define IGESIO_NUMERICS_MESH_ALGORITHMS_EDGES_H_

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "igesio/numerics/mesh/algorithms/normals.h"
#include "igesio/numerics/mesh/triangle_mesh.h"



namespace igesio::numerics {

/// @brief メッシュのエッジ抽出結果 (頂点インデックスペア)
struct MeshEdgeSet {
    /// @brief 全ユニークエッジ (各ペアは昇順; [0] < [1])
    std::vector<std::array<std::uint32_t, 2>> all_edges;
    /// @brief 特徴エッジ (all_edgesの部分集合; 境界・非多様体・折り目)
    std::vector<std::array<std::uint32_t, 2>> feature_edges;
};

/// @brief メッシュのユニークエッジと特徴エッジを抽出する
/// @param mesh 対象のメッシュ (Validateを通る整合したメッシュであること)
/// @param crease_angle_cos 折り目判定のしきい値. 隣接2面の単位面法線の内積が
///        この値を下回るエッジを折り目とする (例: cos(30°))
/// @return エッジ抽出結果. 特徴エッジは以下のいずれかを満たすエッジ:
///         - 境界 (隣接三角形が1枚)
///         - 非多様体 (隣接三角形が3枚以上)
///         - 折り目 (隣接2枚の面法線の内積がしきい値未満)
///         - 退化三角形が隣接する (面法線が計算不能のため安全側で特徴扱い)
/// @note 同一頂点を結ぶ長さゼロのエッジ (退化三角形の重複インデックス) は
///       列挙から除外する
template <typename Scalar>
MeshEdgeSet ExtractMeshEdges(const TriangleMeshT<Scalar>& mesh,
                             const double crease_angle_cos) {
    MeshEdgeSet result;
    const auto triangle_count = mesh.TriangleCount();
    if (triangle_count == 0) return result;

    const auto face_normals = ComputeFaceNormals(mesh);

    // (小さい頂点番号, 大きい頂点番号, 三角形番号) を全三角形分列挙する
    struct EdgeRecord {
        std::uint32_t v0, v1, tri;
    };
    std::vector<EdgeRecord> records;
    records.reserve(mesh.indices.size());
    for (std::size_t t = 0; t < triangle_count; ++t) {
        for (int k = 0; k < 3; ++k) {
            const auto a = mesh.indices[3 * t + k];
            const auto b = mesh.indices[3 * t + (k + 1) % 3];
            if (a == b) continue;  // 長さゼロのエッジは除外
            records.push_back({std::min(a, b), std::max(a, b),
                               static_cast<std::uint32_t>(t)});
        }
    }
    std::sort(records.begin(), records.end(),
              [](const EdgeRecord& l, const EdgeRecord& r) {
                  return (l.v0 != r.v0) ? (l.v0 < r.v0) : (l.v1 < r.v1);
              });

    // 同一 (v0, v1) のラン = 1ユニークエッジとして隣接数と二面角で分類する
    for (std::size_t i = 0; i < records.size();) {
        std::size_t j = i;
        while (j < records.size() && records[j].v0 == records[i].v0 &&
               records[j].v1 == records[i].v1) {
            ++j;
        }
        const std::array<std::uint32_t, 2> edge = {records[i].v0, records[i].v1};
        result.all_edges.push_back(edge);

        const std::size_t adjacent_count = j - i;
        bool is_feature = (adjacent_count != 2);  // 境界または非多様体
        if (adjacent_count == 2) {
            const auto n0 = face_normals.col(records[i].tri);
            const auto n1 = face_normals.col(records[i + 1].tri);
            if (n0.squaredNorm() == Scalar(0) || n1.squaredNorm() == Scalar(0)) {
                is_feature = true;  // 退化三角形絡みは安全側で特徴扱い
            } else {
                is_feature = static_cast<double>(n0.dot(n1)) < crease_angle_cos;
            }
        }
        if (is_feature) result.feature_edges.push_back(edge);
        i = j;
    }
    return result;
}

/// @brief メッシュの全ユニークエッジを抽出する (分類なしの軽量版)
/// @param mesh 対象のメッシュ (Validateを通る整合したメッシュであること)
/// @return 全ユニークエッジ (各ペアは昇順; [0] < [1]).
///         ExtractMeshEdgesのall_edgesと同じ集合
/// @note 特徴エッジの分類 (面法線計算・隣接数の評価) を行わないため、
///       エッジ集合だけが必要な用途 (範囲選択サンプリング等) では
///       ExtractMeshEdgesより大幅に軽い. エッジを64bit整数へパックして
///       PODソートでユニーク化する
/// @note 同一頂点を結ぶ長さゼロのエッジ (退化三角形の重複インデックス) は
///       列挙から除外する
template <typename Scalar>
std::vector<std::array<std::uint32_t, 2>> ExtractUniqueEdges(
        const TriangleMeshT<Scalar>& mesh) {
    // (小頂点番号 << 32) | 大頂点番号 へパックして全エッジを列挙する
    std::vector<std::uint64_t> packed;
    packed.reserve(mesh.indices.size());
    const auto triangle_count = mesh.TriangleCount();
    for (std::size_t t = 0; t < triangle_count; ++t) {
        for (int k = 0; k < 3; ++k) {
            const auto a = mesh.indices[3 * t + k];
            const auto b = mesh.indices[3 * t + (k + 1) % 3];
            if (a == b) continue;  // 長さゼロのエッジは除外
            packed.push_back(
                    (static_cast<std::uint64_t>(std::min(a, b)) << 32) |
                    static_cast<std::uint64_t>(std::max(a, b)));
        }
    }
    std::sort(packed.begin(), packed.end());
    packed.erase(std::unique(packed.begin(), packed.end()), packed.end());

    std::vector<std::array<std::uint32_t, 2>> edges;
    edges.reserve(packed.size());
    for (const auto key : packed) {
        edges.push_back({static_cast<std::uint32_t>(key >> 32),
                         static_cast<std::uint32_t>(key & 0xffffffffULL)});
    }
    return edges;
}

}  // namespace igesio::numerics

#endif  // IGESIO_NUMERICS_MESH_ALGORITHMS_EDGES_H_
