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
/// @param[in,out] mesh 対象のメッシュ (normalsチャンネルを上書きする)
/// @note 各三角形の外積 (大きさ=面積の2倍) を頂点へ加算して正規化する.
///       面積重みは頂点まわりの面の大きさに応じた自然な平均を与える.
///       退化三角形 (外積がゼロ) は寄与しない. どの面にも属さない頂点の
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
    /// @brief 隣接する三角形の枚数 (3枚以上の非多様体稜線では3で飽和する)
    std::uint32_t adjacent_count = 0;
    /// @brief 隣接する三角形の番号 (先頭2枚のみ保持する)
    std::array<std::uint32_t, 2> triangles = {0, 0};
};

/// @brief 頂点まわりのコーナー (どの三角形の何番目の頂点か)
struct VertexCorner {
    /// @brief 頂点番号 (positionsの列番号)
    std::uint32_t vertex = 0;
    /// @brief 三角形番号
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

/// @brief 稜線の (v0, v1) 昇順を判定する比較関数
/// @param l 比較する稜線
/// @param r 比較する稜線
/// @return lがrより前に来る場合はtrue
inline bool IsEdgeLess(const CreaseEdge& l, const CreaseEdge& r) {
    return (l.v0 != r.v0) ? (l.v0 < r.v0) : (l.v1 < r.v1);
}

/// @brief メッシュの稜線と隣接三角形の対応を構築する
/// @param mesh 対象のメッシュ (Validateを通る整合したメッシュであること)
/// @return (v0, v1) の昇順でソートされた稜線列
/// @note 同一頂点を結ぶ長さゼロの稜線 (退化三角形の重複インデックス)
///       は列挙から除外する
template <typename Scalar>
std::vector<CreaseEdge> BuildEdgeAdjacency(const TriangleMeshT<Scalar>& mesh) {
    const auto triangle_count = mesh.TriangleCount();
    std::vector<CreaseEdge> records;
    records.reserve(mesh.indices.size());
    for (std::size_t t = 0; t < triangle_count; ++t) {
        for (std::size_t k = 0; k < 3; ++k) {
            const auto a = mesh.indices[3 * t + k];
            const auto b = mesh.indices[3 * t + (k + 1) % 3];
            if (a == b) continue;  // 長さゼロの稜線は除外
            records.push_back({std::min(a, b), std::max(a, b), 1,
                               {static_cast<std::uint32_t>(t), 0}});
        }
    }
    std::sort(records.begin(), records.end(), IsEdgeLess);

    // 同一稜線のものを1エントリへまとめる
    std::vector<CreaseEdge> edges;
    edges.reserve(records.size());
    for (std::size_t i = 0; i < records.size();) {
        CreaseEdge edge = records[i];
        edge.adjacent_count = 0;
        std::size_t j = i;
        while (j < records.size() && records[j].v0 == edge.v0 &&
               records[j].v1 == edge.v1) {
            if (edge.adjacent_count < 2) {
                edge.triangles[edge.adjacent_count] = records[j].triangles[0];
            }
            if (edge.adjacent_count < 3) ++edge.adjacent_count;
            ++j;
        }
        edges.push_back(edge);
        i = j;
    }
    return edges;
}

/// @brief 稜線の隣接情報を二分探索で引く
/// @param edges BuildEdgeAdjacencyの結果 (ソート済みであること)
/// @param a 稜線の端点
/// @param b 稜線の端点
/// @return 見つかった稜線へのポインタ. 存在しない場合はnullptr
inline const CreaseEdge* FindEdge(const std::vector<CreaseEdge>& edges,
                                  const std::uint32_t a,
                                  const std::uint32_t b) {
    const CreaseEdge key{std::min(a, b), std::max(a, b), 0, {0, 0}};
    const auto it = std::lower_bound(edges.begin(), edges.end(), key,
                                     IsEdgeLess);
    if (it == edges.end() || it->v0 != key.v0 || it->v1 != key.v1) {
        return nullptr;
    }
    return &(*it);
}

/// @brief 頂点番号でソートしたコーナー列を構築する
/// @param mesh 対象のメッシュ (Validateを通る整合したメッシュであること)
/// @return 頂点番号の昇順でソートされたコーナー列 (同一頂点が連続する)
/// @note 同一頂点内は (三角形番号, コーナー番号) の昇順で並べる. これにより
///       頂点の複製順とどのクラスタが元の頂点番号を引き継ぐかが一意に定まり、
///       同じ入力に対して常に同じメッシュが得られる
template <typename Scalar>
std::vector<VertexCorner> BuildSortedVertexCorners(
        const TriangleMeshT<Scalar>& mesh) {
    const auto triangle_count = mesh.TriangleCount();
    std::vector<VertexCorner> corners;
    corners.reserve(mesh.indices.size());
    for (std::size_t t = 0; t < triangle_count; ++t) {
        for (std::uint32_t k = 0; k < 3; ++k) {
            corners.push_back({mesh.indices[3 * t + k],
                               static_cast<std::uint32_t>(t), k});
        }
    }
    std::sort(corners.begin(), corners.end(),
              [](const VertexCorner& l, const VertexCorner& r) {
                  if (l.vertex != r.vertex) return l.vertex < r.vertex;
                  if (l.triangle != r.triangle) return l.triangle < r.triangle;
                  return l.corner < r.corner;
              });
    return corners;
}

/// @brief 三角形毎の外積ベクトルと単位面法線を計算する
/// @param mesh 対象のメッシュ (Validateを通る整合したメッシュであること)
/// @return 三角形毎の外積ベクトルと単位面法線
/// @note ComputeFaceNormalsと異なり、正規化前の外積 (面積重み) も返す
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

/// @brief union-findの根を求める (経路圧縮あり)
/// @param[in,out] parent 親配列
/// @param index 根を求める要素の番号
/// @return 根の番号
inline std::size_t FindRoot(std::vector<std::size_t>& parent,
                            std::size_t index) {
    while (parent[index] != index) {
        parent[index] = parent[parent[index]];
        index = parent[index];
    }
    return index;
}

/// @brief union-findで2要素の属する集合を併合する
/// @param[in,out] parent 親配列
/// @param a 併合する要素の番号
/// @param b 併合する要素の番号
inline void UniteRoots(std::vector<std::size_t>& parent, const std::size_t a,
                       const std::size_t b) {
    const auto root_a = FindRoot(parent, a);
    const auto root_b = FindRoot(parent, b);
    if (root_a != root_b) parent[root_b] = root_a;
}

/// @brief 1頂点まわりのコーナーを平滑クラスタへ併合する
/// @param mesh 対象のメッシュ (インデックスを張り替える前のものであること)
/// @param edges BuildEdgeAdjacencyの結果
/// @param faces ComputeFaceVectorsの結果
/// @param corners BuildSortedVertexCornersの結果
/// @param begin 対象頂点のコーナー列における開始位置
/// @param end 対象頂点のコーナー列における終端位置 (この位置は含まない)
/// @param crease_angle_cos 折り目判定のしきい値
/// @return union-findの親配列 (長さend-begin; コーナーの相対位置に対応する)
/// @note 対象頂点に接する稜線のうち、隣接三角形がちょうど2枚で、かつ両面の
///       単位面法線の内積がしきい値以上のものを通じて両側を併合する.
///       退化三角形が絡む稜線は併合しない (安全側で折り目として扱う)
template <typename Scalar>
std::vector<std::size_t> ClusterVertexCorners(
        const TriangleMeshT<Scalar>& mesh,
        const std::vector<CreaseEdge>& edges, const FaceVectors<Scalar>& faces,
        const std::vector<VertexCorner>& corners, const std::size_t begin,
        const std::size_t end, const double crease_angle_cos) {
    std::vector<std::size_t> parent(end - begin);
    for (std::size_t i = 0; i < parent.size(); ++i) parent[i] = i;

    for (std::size_t i = begin; i < end; ++i) {
        const auto triangle = corners[i].triangle;
        const auto corner = corners[i].corner;
        const auto vertex = corners[i].vertex;
        // 対象頂点に接する2本の稜線 (前後のコーナーへ向かう辺)
        const std::array<std::uint32_t, 2> others = {
                mesh.indices[3 * triangle + (corner + 1) % 3],
                mesh.indices[3 * triangle + (corner + 2) % 3]};

        for (const auto other : others) {
            if (other == vertex) continue;  // 長さゼロの稜線
            const auto* edge = FindEdge(edges, vertex, other);
            if (edge == nullptr || edge->adjacent_count != 2) continue;

            const auto t0 = edge->triangles[0];
            const auto t1 = edge->triangles[1];
            if (faces.unit[t0].squaredNorm() == Scalar(0) ||
                faces.unit[t1].squaredNorm() == Scalar(0)) continue;
            if (static_cast<double>(faces.unit[t0].dot(faces.unit[t1])) <
                crease_angle_cos) continue;

            // 折り目ではないため、稜線を挟む2三角形のコーナーを併合する
            const auto neighbor = (t0 == triangle) ? t1 : t0;
            for (std::size_t j = begin; j < end; ++j) {
                if (corners[j].triangle == neighbor) {
                    UniteRoots(parent, i - begin, j - begin);
                    break;
                }
            }
        }
    }
    return parent;
}

/// @brief 複製した頂点の位置とUVを複製元からコピーして頂点配列を拡張する
/// @param[in,out] mesh 対象のメッシュ
/// @param duplicated_from 複製する頂点の複製元列番号 (追加する順)
/// @note 法線チャンネルはここでは触らない (呼び出し側が最終的に書き込む)
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

}  // namespace detail

/// @brief 折り目で分割した頂点法線を再計算する
/// @param[in,out] mesh 対象のメッシュ
///        (positions・normals・uvs・indicesを更新する)
/// @param crease_angle_cos 折り目判定のしきい値. 稜線を挟む2面の単位面法線の
///        内積がこの値を下回る稜線を折り目とし、そこで法線を分割する
///        (例: cos(30°))
/// @note 各頂点まわりの面を「折り目でない稜線を辿って到達できる」集合ごとに
///       まとめ、集合単位で面積重み平均した法線を与える. 集合が2つ以上ある
///       頂点は集合の数だけ頂点を複製し、対応するインデックスを新しい頂点へ
///       張り替える (位置とUVは複製元からコピーする). これにより平らな面の
///       角は角のまま、滑らかに続く面は滑らかなまま陰影が付く
/// @note 元の頂点番号と三角形の並び順は保たれるため、groupsの三角形範囲と
///       どの面にも属さない頂点 (法線はゼロベクトル) はそのまま残る
/// @note 退化三角形 (外積がゼロ) は折り目判定に参加せず、法線にも寄与しない
/// @note crease_angle_cosに-1以下を渡すと全ての稜線で併合されるため、稜線を
///       共有せず頂点のみを共有する面 (蝶ネクタイ状の頂点) を除いて
///       RecomputeNormalsと同じ結果になる
template <typename Scalar>
void RecomputeNormalsWithCrease(TriangleMeshT<Scalar>& mesh,
                                const double crease_angle_cos) {
    using Vec3 = Eigen::Matrix<Scalar, 3, 1>;
    if (mesh.TriangleCount() == 0) {
        mesh.normals.setZero(3, mesh.positions.cols());
        return;
    }

    const auto faces = detail::ComputeFaceVectors(mesh);
    const auto edges = detail::BuildEdgeAdjacency(mesh);
    const auto corners = detail::BuildSortedVertexCorners(mesh);

    // インデックスの張り替えが稜線探索へ影響しないよう、別の配列へ書き出す
    const auto original_count = mesh.positions.cols();
    std::vector<Vec3> normal_sums(static_cast<std::size_t>(original_count),
                                  Vec3::Zero());
    std::vector<Eigen::Index> duplicated_from;
    auto new_indices = mesh.indices;

    for (std::size_t i = 0; i < corners.size();) {
        std::size_t j = i;
        while (j < corners.size() && corners[j].vertex == corners[i].vertex) {
            ++j;
        }
        auto parent = detail::ClusterVertexCorners(mesh, edges, faces, corners,
                                                   i, j, crease_angle_cos);

        // クラスタの根と割り当てた頂点番号の対応 (最初の根は元の頂点を再利用)
        std::vector<std::pair<std::size_t, std::uint32_t>> assigned;
        for (std::size_t c = i; c < j; ++c) {
            const auto root = detail::FindRoot(parent, c - i);
            auto it = std::find_if(
                    assigned.begin(), assigned.end(),
                    [root](const auto& a) { return a.first == root; });
            if (it == assigned.end()) {
                auto column = corners[c].vertex;
                if (!assigned.empty()) {
                    // 2つ目以降のクラスタには複製した頂点を割り当てる
                    column = static_cast<std::uint32_t>(
                            normal_sums.size());
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

    detail::AppendDuplicatedVertices(mesh, duplicated_from);
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

}  // namespace igesio::numerics

#endif  // IGESIO_NUMERICS_MESHES_ALGORITHMS_NORMALS_H_
