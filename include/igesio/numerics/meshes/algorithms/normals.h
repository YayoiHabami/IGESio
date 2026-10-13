/**
 * @file numerics/meshes/algorithms/normals.h
 * @brief 三角形メッシュ (TriangleMeshT) の頂点法線・面法線を計算するアルゴリズム
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 */
#ifndef IGESIO_NUMERICS_MESHES_ALGORITHMS_NORMALS_H_
#define IGESIO_NUMERICS_MESHES_ALGORITHMS_NORMALS_H_

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <utility>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/numerics/meshes/triangle_mesh.h"



namespace igesio::numerics {

/// @brief 頂点法線を面積重み平均で再計算する
/// @param[in,out] mesh 対象のメッシュ（頂点法線を上書きする）
/// @note 各三角形の外積（面積の2倍）を頂点へ加算して正規化する.
///       外積がゼロの退化した三角形は寄与せず、またどの面にも属さない頂点の
///       法線はゼロベクトルとなる
template <typename Scalar>
void RecomputeNormals(TriangleMeshT<Scalar>& mesh) {
    using Vec3 = Eigen::Matrix<Scalar, 3, 1>;
    mesh.normals.setZero(3, mesh.positions.cols());

    for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const auto i0 = mesh.indices[t];
        const auto i1 = mesh.indices[t + 1];
        const auto i2 = mesh.indices[t + 2];
        const Vec3 p0 = mesh.positions.col(i0);
        const Vec3 edge1 = Vec3(mesh.positions.col(i1)) - p0;
        const Vec3 edge2 = Vec3(mesh.positions.col(i2)) - p0;
        const Vec3 cross = edge1.cross(edge2);
        mesh.normals.col(i0) += cross;
        mesh.normals.col(i1) += cross;
        mesh.normals.col(i2) += cross;
    }

    for (Eigen::Index c = 0; c < mesh.normals.cols(); ++c) {
        const Scalar norm = mesh.normals.col(c).norm();
        if (norm > Scalar(0)) mesh.normals.col(c) /= norm;
    }
}

/// @brief 各三角形の単位面法線を計算する
/// @param mesh 対象のメッシュ
/// @return 三角形毎の単位面法線 (3×三角形数; 各列が1三角形).
///         退化三角形 (外積がゼロ) はゼロベクトル
template <typename Scalar>
Eigen::Matrix<Scalar, 3, Eigen::Dynamic> ComputeFaceNormals(
        const TriangleMeshT<Scalar>& mesh) {
    using Vec3 = Eigen::Matrix<Scalar, 3, 1>;
    const auto triangle_count = mesh.TriangleCount();
    Eigen::Matrix<Scalar, 3, Eigen::Dynamic> normals;
    normals.setZero(3, static_cast<Eigen::Index>(triangle_count));

    for (std::size_t t = 0; t < triangle_count; ++t) {
        const Vec3 p0 = mesh.positions.col(mesh.indices[3 * t]);
        const Vec3 edge1 = Vec3(mesh.positions.col(mesh.indices[3 * t + 1])) - p0;
        const Vec3 edge2 = Vec3(mesh.positions.col(mesh.indices[3 * t + 2])) - p0;
        const Vec3 cross = edge1.cross(edge2);
        const Scalar norm = cross.norm();
        if (norm > Scalar(0)) {
            normals.col(static_cast<Eigen::Index>(t)) = cross / norm;
        }
    }
    return normals;
}

/// @brief 隠蔽用名前空間
/// @note 直接使用しないこと
namespace detail {

/// @brief 稜線とその隣接三角形
struct CreaseEdge {
    /// @brief 稜線の端点 (小さい方の頂点番号)
    std::uint32_t v0 = 0;
    /// @brief 稜線の端点 (大きい方の頂点番号)
    std::uint32_t v1 = 0;
    /// @brief 隣接する三角形の数
    /// @note 3以上の非多様体稜線については、それ以上カウントしない
    std::uint32_t adjacent_count = 0;
    /// @brief 隣接する三角形の番号（先頭の2つのみ保持する）
    std::array<std::uint32_t, 2> triangles = {0, 0};
};

/// @brief 頂点まわりのコーナー（どの三角形の何番目の頂点か）
struct VertexCorner {
    /// @brief 頂点番号
    /// @note `TriangleMeshT::positions`の列番号
    std::uint32_t vertex = 0;
    /// @brief 三角形番号
    /// @note メッシュの何番目の三角形か. 値が`t`であれば、その三角形の頂点は
    ///       `TriangleMeshT::indices`の3t, 3t+1, 3t+2番目にある.
    std::uint32_t triangle = 0;
    /// @brief 三角形内でのコーナー番号 (0-2)
    std::uint32_t corner = 0;
};

/// @brief 三角形毎の外積ベクトルと単位面法線
template <typename Scalar>
struct FaceVectors {
    /// @brief 三角形の外積ベクトル (大きさ=面積の2倍)
    std::vector<Eigen::Matrix<Scalar, 3, 1>> cross;
    /// @brief 三角形の単位面法線 (退化三角形はゼロベクトル)
    std::vector<Eigen::Matrix<Scalar, 3, 1>> unit;
};

/// @brief メッシュの稜線と、三角形の各辺から稜線への対応
struct EdgeAdjacency {
    /// @brief 稜線を持たない長さゼロの辺を表す稜線番号
    static constexpr std::uint32_t kNoEdge = 0xffffffffU;

    /// @brief (v0, v1) の昇順に並べた稜線列
    std::vector<CreaseEdge> edges;
    /// @brief 三角形tのk番目の辺 (コーナーkからk+1への辺) の稜線番号
    /// @note インデックスは3t+k. 長さゼロの辺はkNoEdge
    std::vector<std::uint32_t> edge_of_side;
};

/// @brief メッシュの稜線と隣接三角形の対応を構築する
/// @param mesh 対象のメッシュ (Validateを通る整合したメッシュであること)
/// @return 稜線列と、三角形の各辺から稜線への対応
/// @note 同一頂点を結ぶ長さゼロの稜線 (退化三角形の重複インデックス)
///       は列挙から除外する
template <typename Scalar>
EdgeAdjacency BuildEdgeAdjacency(const TriangleMeshT<Scalar>& mesh) {
    // 稜線の両端を (小さい頂点番号 << 32) | 大きい頂点番号 へまとめた
    // 1つの整数で比較し、ソートを軽くする
    struct SideRecord {
        std::uint64_t key;
        std::uint32_t triangle;
        std::uint32_t side;
    };
    const auto triangle_count = mesh.TriangleCount();
    std::vector<SideRecord> records;
    records.reserve(mesh.indices.size());
    for (std::size_t t = 0; t < triangle_count; ++t) {
        for (std::size_t k = 0; k < 3; ++k) {
            const auto a = mesh.indices[3 * t + k];
            const auto b = mesh.indices[3 * t + (k + 1) % 3];
            if (a == b) continue;  // 長さゼロの稜線は除外
            records.push_back(
                    {(static_cast<std::uint64_t>(std::min(a, b)) << 32) |
                             static_cast<std::uint64_t>(std::max(a, b)),
                     static_cast<std::uint32_t>(t),
                     static_cast<std::uint32_t>(3 * t + k)});
        }
    }
    std::sort(records.begin(), records.end(),
              [](const SideRecord& l, const SideRecord& r) {
                  return l.key < r.key;
              });

    // 同一稜線のものを1エントリにまとめ、各辺から稜線番号を引けるようにする
    EdgeAdjacency adjacency;
    adjacency.edge_of_side.assign(mesh.indices.size(), EdgeAdjacency::kNoEdge);
    adjacency.edges.reserve(records.size() / 2 + 1);
    for (std::size_t i = 0; i < records.size();) {
        CreaseEdge edge;
        edge.v0 = static_cast<std::uint32_t>(records[i].key >> 32);
        edge.v1 = static_cast<std::uint32_t>(records[i].key & 0xffffffffULL);
        const auto edge_index =
                static_cast<std::uint32_t>(adjacency.edges.size());
        std::size_t j = i;
        while (j < records.size() && records[j].key == records[i].key) {
            if (edge.adjacent_count < 2) {
                edge.triangles[edge.adjacent_count] = records[j].triangle;
            }
            if (edge.adjacent_count < 3) ++edge.adjacent_count;
            adjacency.edge_of_side[records[j].side] = edge_index;
            ++j;
        }
        adjacency.edges.push_back(edge);
        i = j;
    }
    return adjacency;
}

/// @brief 頂点番号でソートしたコーナー列を構築する
/// @param mesh 対象のメッシュ（Validateを通る整合したメッシュであること）
/// @return 頂点番号の昇順でソートされたコーナー列（同一頂点が連続する）
/// @note 同一頂点内は(三角形番号, コーナー番号)の昇順で並べる. これにより
///       頂点の複製順とどのクラスタが元の頂点番号を引き継ぐかが一意に定まり、
///       同じ入力に対して常に同じメッシュが得られる
template <typename Scalar>
std::vector<VertexCorner> BuildSortedVertexCorners(
        const TriangleMeshT<Scalar>& mesh) {
    // 頂点番号をキーとする計数ソート. コーナーを(三角形番号, コーナー番号)の順に
    // 走査して並べるため、同一頂点内の順序は昇順になる
    const auto vertex_count = static_cast<std::size_t>(mesh.positions.cols());
    std::vector<std::uint32_t> next_slot(vertex_count + 1, 0);
    for (const auto index : mesh.indices) ++next_slot[index + 1];
    for (std::size_t v = 1; v < next_slot.size(); ++v) {
        next_slot[v] += next_slot[v - 1];
    }

    std::vector<VertexCorner> corners(mesh.indices.size());
    for (std::size_t s = 0; s < mesh.indices.size(); ++s) {
        const auto vertex = mesh.indices[s];
        corners[next_slot[vertex]++] = {vertex,
                                        static_cast<std::uint32_t>(s / 3),
                                        static_cast<std::uint32_t>(s % 3)};
    }
    return corners;
}

/// @brief 三角形ごとの外積ベクトルと単位面法線を計算する
/// @param mesh 対象のメッシュ（Validateを通る整合したメッシュであること）
/// @return 三角形ごとの外積ベクトルと単位面法線
/// @note ComputeFaceNormalsと異なり、正規化前の外積（面積重み）も返す
template <typename Scalar>
FaceVectors<Scalar> ComputeFaceVectors(const TriangleMeshT<Scalar>& mesh) {
    using Vec3 = Eigen::Matrix<Scalar, 3, 1>;
    const auto triangle_count = mesh.TriangleCount();
    FaceVectors<Scalar> faces;
    faces.cross.resize(triangle_count);
    faces.unit.resize(triangle_count);

    for (std::size_t t = 0; t < triangle_count; ++t) {
        const Vec3 p0 = mesh.positions.col(mesh.indices[3 * t]);
        const Vec3 edge1 =
                Vec3(mesh.positions.col(mesh.indices[3 * t + 1])) - p0;
        const Vec3 edge2 =
                Vec3(mesh.positions.col(mesh.indices[3 * t + 2])) - p0;
        faces.cross[t] = edge1.cross(edge2);
        const Scalar norm = faces.cross[t].norm();
        faces.unit[t] = (norm > Scalar(0)) ? Vec3(faces.cross[t] / norm)
                                           : Vec3(Vec3::Zero());
    }
    return faces;
}

/// @brief union-findのルート要素を求める（探索した要素の親をルートに近づける）
/// @param[in,out] parent 親配列
/// @param index ルート要素を求める要素の番号
/// @return ルート要素の番号
inline std::size_t FindRoot(std::vector<std::size_t>& parent,
                            std::size_t index) {
    while (parent[index] != index) {
        parent[index] = parent[parent[index]];
        index = parent[index];
    }
    return index;
}

/// @brief union-findで2要素の属する集合を1つにまとめる
/// @param[in,out] parent 親配列
/// @param a まとめる要素の番号
/// @param b まとめる要素の番号
inline void UniteRoots(std::vector<std::size_t>& parent, const std::size_t a,
                       const std::size_t b) {
    const auto root_a = FindRoot(parent, a);
    const auto root_b = FindRoot(parent, b);
    if (root_a != root_b) parent[root_b] = root_a;
}

/// @brief 1頂点まわりのコーナーを、法線を共有する集合にまとめる
/// @param adjacency BuildEdgeAdjacencyの結果
/// @param faces ComputeFaceVectorsの結果
/// @param corners BuildSortedVertexCornersの結果
/// @param begin 対象頂点のコーナー列における開始位置
/// @param end 対象頂点のコーナー列における終端位置 (この位置は含まない)
/// @param crease_angle_cos 折り目判定のしきい値
/// @param[out] parent union-findの親配列（長さは対象頂点のコーナー数、コーナーの
///        相対位置に対応）. 頂点ごとの確保を避けるため、呼び出し側の配列を再利用する
/// @note 対象頂点に接する稜線のうち、隣接三角形がちょうど2枚で、かつ両面の
///       単位面法線の内積がしきい値以上のものを通じて両側を同じ集合に統合する.
///       退化三角形の関わる稜線は統合しない（面法線を比較できないため折り目として扱う）
template <typename Scalar>
void ClusterVertexCorners(
        const EdgeAdjacency& adjacency, const FaceVectors<Scalar>& faces,
        const std::vector<VertexCorner>& corners, const std::size_t begin,
        const std::size_t end, const double crease_angle_cos,
        std::vector<std::size_t>& parent) {
    parent.resize(end - begin);
    for (std::size_t i = 0; i < parent.size(); ++i) parent[i] = i;

    for (std::size_t i = begin; i < end; ++i) {
        const auto triangle = corners[i].triangle;
        const auto corner = corners[i].corner;
        // 対象頂点に接する2本の稜線
        // （次のコーナーへ向かう辺と前のコーナーから来る辺）
        const std::array<std::uint32_t, 2> edge_indices = {
                adjacency.edge_of_side[3 * triangle + corner],
                adjacency.edge_of_side[3 * triangle + (corner + 2) % 3]};

        for (const auto edge_index : edge_indices) {
            if (edge_index == EdgeAdjacency::kNoEdge) continue;  // 長さゼロの稜線
            const auto& edge = adjacency.edges[edge_index];
            if (edge.adjacent_count != 2) continue;

            const auto t0 = edge.triangles[0];
            const auto t1 = edge.triangles[1];
            if (faces.unit[t0].squaredNorm() == Scalar(0) ||
                faces.unit[t1].squaredNorm() == Scalar(0)) continue;
            if (static_cast<double>(faces.unit[t0].dot(faces.unit[t1])) <
                crease_angle_cos) continue;

            // 折り目ではないため、稜線を挟む2三角形のコーナーを同じ集合に統合する
            const auto neighbor = (t0 == triangle) ? t1 : t0;
            for (std::size_t j = begin; j < end; ++j) {
                if (corners[j].triangle == neighbor) {
                    UniteRoots(parent, i - begin, j - begin);
                    break;
                }
            }
        }
    }
}

/// @brief 複製した頂点の位置とUVを複製元からコピーして頂点配列を拡張する
/// @param[in,out] mesh 対象のメッシュ
/// @param duplicated_from 複製する頂点の複製元の列番号（追加する順）
/// @note 頂点法線はここでは変更しない（呼び出し側で設定する）
template <typename Scalar>
void AppendDuplicatedVertices(
        TriangleMeshT<Scalar>& mesh,
        const std::vector<Eigen::Index>& duplicated_from) {
    if (duplicated_from.empty()) return;

    const bool has_uvs = mesh.HasUVs();
    const auto original_count = mesh.positions.cols();
    const auto total =
            original_count + static_cast<Eigen::Index>(duplicated_from.size());
    mesh.positions.conservativeResize(3, total);
    if (has_uvs) mesh.uvs.conservativeResize(2, total);

    for (std::size_t d = 0; d < duplicated_from.size(); ++d) {
        const auto destination = original_count + static_cast<Eigen::Index>(d);
        mesh.positions.col(destination) = mesh.positions.col(
                duplicated_from[d]);
        if (has_uvs) mesh.uvs.col(destination) = mesh.uvs.col(
                duplicated_from[d]);
    }
}

/// @brief 構築済みの面ベクトルと稜線から、折り目で分割した頂点法線を設定する
/// @param[in,out] mesh 対象のメッシュ（positions/normals/uvs/indicesを更新する）
/// @param faces meshから計算したComputeFaceVectorsの結果
/// @param adjacency meshから計算したBuildEdgeAdjacencyの結果
/// @param crease_angle_cos 折り目判定のしきい値
/// @note facesとadjacencyは、インデックスを書き換える前のmeshから
///       計算したものであること
template <typename Scalar>
void ApplyCreaseNormals(TriangleMeshT<Scalar>& mesh,
                        const FaceVectors<Scalar>& faces,
                        const EdgeAdjacency& adjacency,
                        const double crease_angle_cos) {
    using Vec3 = Eigen::Matrix<Scalar, 3, 1>;
    const auto corners = BuildSortedVertexCorners(mesh);

    // インデックスの書き換えが稜線探索に影響しないよう、別の配列に書き出す
    std::vector<Vec3> normal_sums(
            static_cast<std::size_t>(mesh.positions.cols()), Vec3::Zero());
    std::vector<Eigen::Index> duplicated_from;
    auto new_indices = mesh.indices;
    std::vector<std::size_t> parent;
    // 集合のルート要素と割り当てた頂点番号の対応（最初のルート要素には元の頂点を再利用）
    std::vector<std::pair<std::size_t, std::uint32_t>> assigned;

    for (std::size_t i = 0; i < corners.size();) {
        std::size_t j = i;
        while (j < corners.size() && corners[j].vertex == corners[i].vertex) {
            ++j;
        }
        ClusterVertexCorners(adjacency, faces, corners, i, j, crease_angle_cos,
                             parent);

        assigned.clear();
        for (std::size_t c = i; c < j; ++c) {
            const auto root = FindRoot(parent, c - i);
            auto it = std::find_if(
                    assigned.begin(), assigned.end(),
                    [root](const auto& a) { return a.first == root; });
            if (it == assigned.end()) {
                auto column = corners[c].vertex;
                if (!assigned.empty()) {
                    // 2つ目以降のクラスタには複製した頂点を割り当てる
                    column = static_cast<std::uint32_t>(normal_sums.size());
                    duplicated_from.push_back(
                            static_cast<Eigen::Index>(corners[c].vertex));
                    normal_sums.push_back(Vec3::Zero());
                }
                assigned.emplace_back(root, column);
                it = std::prev(assigned.end());
            }
            new_indices[3 * corners[c].triangle + corners[c].corner] =
                    it->second;
            normal_sums[it->second] += faces.cross[corners[c].triangle];
        }
        i = j;
    }

    AppendDuplicatedVertices(mesh, duplicated_from);
    mesh.indices = std::move(new_indices);

    // クラスタ毎の面積重み和を正規化して頂点法線とする
    mesh.normals.setZero(3, static_cast<Eigen::Index>(normal_sums.size()));
    for (std::size_t c = 0; c < normal_sums.size(); ++c) {
        const Scalar norm = normal_sums[c].norm();
        if (norm > Scalar(0)) {
            mesh.normals.col(static_cast<Eigen::Index>(c)) =
                    normal_sums[c] / norm;
        }
    }
}

}  // namespace detail

/// @brief 折り目で分割した頂点法線を再計算する
/// @param[in,out] mesh 対象のメッシュ
///                （positions・normals・uvs・indicesを更新する）
/// @param crease_angle_cos 折り目判定のしきい値. 稜線を挟む2面の単位面法線の
///        内積がこの値を下回る稜線を折り目とし、そこで法線を分割する
///        （例: cos(30°)）
/// @note 各頂点まわりの面を、折り目でない稜線を経由して到達できる集合ごとに集約し,
///       集合単位で面積重み平均した法線を与える. 集合が2つ以上ある頂点は
///       集合の数だけ頂点を複製し、対応するインデックスを新しい頂点に置き換える
///       （位置とUVはコピーする）. 以上により、平らな面の角は角のまま,
///       滑らかに続く面は滑らかなまま陰影が付く
/// @note 元の頂点番号と三角形の並び順は保たれるため、groupsの三角形範囲と
///       どの面にも属さない頂点（法線がゼロベクトル）はそのまま残る
/// @note 外積がゼロの退化三角形は折り目判定の対象とせず、法線にも寄与しない
/// @note crease_angle_cosに-1以下を渡すと全ての稜線で集合がまとめられるため,
///       稜線を共有せず頂点のみを共有する面を除いてRecomputeNormalsと同じ結果になる
template <typename Scalar>
void RecomputeNormalsWithCrease(TriangleMeshT<Scalar>& mesh,
                                const double crease_angle_cos) {
    if (mesh.TriangleCount() == 0) {
        mesh.normals.setZero(3, mesh.positions.cols());
        return;
    }

    const auto faces = detail::ComputeFaceVectors(mesh);
    const auto adjacency = detail::BuildEdgeAdjacency(mesh);
    detail::ApplyCreaseNormals(mesh, faces, adjacency, crease_angle_cos);
}

}  // namespace igesio::numerics

#endif  // IGESIO_NUMERICS_MESHES_ALGORITHMS_NORMALS_H_
