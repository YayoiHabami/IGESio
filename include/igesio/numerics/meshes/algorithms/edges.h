/**
 * @file numerics/meshes/algorithms/edges.h
 * @brief 三角形メッシュ (TriangleMeshT) のエッジ・特徴エッジを抽出するアルゴリズム
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 */
#ifndef IGESIO_NUMERICS_MESHES_ALGORITHMS_EDGES_H_
#define IGESIO_NUMERICS_MESHES_ALGORITHMS_EDGES_H_

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "igesio/numerics/meshes/algorithms/normals.h"
#include "igesio/numerics/meshes/triangle_mesh.h"



namespace igesio::numerics {

/// @brief メッシュのエッジ抽出結果 (頂点インデックスペア)
struct MeshEdgeSet {
    /// @brief 全ユニークエッジ (各ペアは昇順; [0] < [1])
    std::vector<std::array<std::uint32_t, 2>> all_edges;
    /// @brief 特徴エッジ (all_edgesの部分集合; 境界・非多様体・折り目)
    std::vector<std::array<std::uint32_t, 2>> feature_edges;
};

/// @brief 隠蔽用名前空間
/// @note 直接使用しないこと
namespace detail {

/// @brief 稜線を全エッジと特徴エッジに分類する
/// @param adjacency BuildEdgeAdjacencyの結果
/// @param crease_angle_cos 折り目判定のしきい値
/// @param unit_normal_of 三角形番号から単位面法線を取得する関数
////       （退化三角形はゼロベクトルを返すこと）
/// @return エッジ抽出結果（ExtractMeshEdgesの戻り値と同じ規約）
template <typename UnitNormalOf>
MeshEdgeSet ClassifyMeshEdges(const EdgeAdjacency& adjacency,
                              const double crease_angle_cos,
                              const UnitNormalOf& unit_normal_of) {
    MeshEdgeSet result;
    result.all_edges.reserve(adjacency.edges.size());
    for (const auto& edge : adjacency.edges) {
        const std::array<std::uint32_t, 2> pair = {edge.v0, edge.v1};
        result.all_edges.push_back(pair);

        // 境界と非多様体は特徴エッジ. 退化三角形が絡む場合は面法線が
        // 計算できないため、特徴エッジとして扱う
        bool is_feature = (edge.adjacent_count != 2);
        if (!is_feature) {
            const auto n0 = unit_normal_of(edge.triangles[0]);
            const auto n1 = unit_normal_of(edge.triangles[1]);
            if (n0.squaredNorm() == 0 || n1.squaredNorm() == 0) {
                is_feature = true;
            } else {
                is_feature =
                        static_cast<double>(n0.dot(n1)) < crease_angle_cos;
            }
        }
        if (is_feature) result.feature_edges.push_back(pair);
    }
    return result;
}

}  // namespace detail

/// @brief メッシュのユニークエッジと特徴エッジを抽出する
/// @param mesh 対象のメッシュ（Validateを通る整合したメッシュであること）
/// @param crease_angle_cos 折り目判定のしきい値. 隣接する2面の単位面法線の内積が
///        この値を下回るエッジを折り目とする（例: cos(30°)）
/// @return エッジ抽出結果. 特徴エッジは以下のいずれかを満たすエッジ：
///         - 境界（隣接する三角形が1つ）,
///         - 非多様体（隣接する三角形が3つ以上）,
///         - 折り目（隣接する2面の面法線の内積がしきい値未満）,
///         - 退化三角形が隣接する（このとき、面法線は計算できないため特徴エッジとして扱う）
/// @note 同一頂点を結ぶ長さゼロのエッジ（退化三角形の重複インデックス）は
///       列挙から除外する
template <typename Scalar>
MeshEdgeSet ExtractMeshEdges(const TriangleMeshT<Scalar>& mesh,
                             const double crease_angle_cos) {
    if (mesh.TriangleCount() == 0) return {};

    const auto face_normals = ComputeFaceNormals(mesh);
    const auto adjacency = detail::BuildEdgeAdjacency(mesh);
    const auto unit_normal_of = [&face_normals](const std::uint32_t t) {
        return face_normals.col(static_cast<Eigen::Index>(t));
    };
    return detail::ClassifyMeshEdges(adjacency, crease_angle_cos,
                                     unit_normal_of);
}

/// @brief 折り目で分割した頂点法線を再計算し、分割前のエッジを抽出する
/// @param[in,out] mesh 対象のメッシュ
///                （positions/normals/uvs/indicesを更新する）
/// @param crease_angle_cos 折り目判定のしきい値（例: cos(30°)）
/// @return 法線を再計算する前のmeshに対するExtractMeshEdgesの結果
/// @note meshの更新内容はRecomputeNormalsWithCreaseと同じ.
///       両方の結果が必要な場合に、面法線と稜線の隣接情報を一括で構築する
template <typename Scalar>
MeshEdgeSet RecomputeNormalsWithCreaseAndExtractEdges(
        TriangleMeshT<Scalar>& mesh, const double crease_angle_cos) {
    if (mesh.TriangleCount() == 0) {
        mesh.normals.setZero(3, mesh.positions.cols());
        return {};
    }

    const auto faces = detail::ComputeFaceVectors(mesh);
    const auto adjacency = detail::BuildEdgeAdjacency(mesh);
    const auto unit_normal_of = [&faces](const std::uint32_t t) {
        return faces.unit[t];
    };
    auto edges = detail::ClassifyMeshEdges(adjacency, crease_angle_cos,
                                           unit_normal_of);
    // 稜線の分類はインデックスを書き換える前の位相に対して行う
    detail::ApplyCreaseNormals(mesh, faces, adjacency, crease_angle_cos);
    return edges;
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

#endif  // IGESIO_NUMERICS_MESHES_ALGORITHMS_EDGES_H_
