/**
 * @file numerics/meshes/algorithms/mesh_line_intersection.h
 * @brief 三角形メッシュ (TriangleMeshT) と直線/半直線/線分の交差判定
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 * @note Möller–Trumbore法による全三角形の線形走査. ピッキングのような
 *       単発のクエリには十分な性能のため、加速構造 (BVH等) は必要になった
 *       時点で本階層へ追加する (将来課題).
 */
#ifndef IGESIO_NUMERICS_MESHES_ALGORITHMS_MESH_LINE_INTERSECTION_H_
#define IGESIO_NUMERICS_MESHES_ALGORITHMS_MESH_LINE_INTERSECTION_H_

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/numerics/geometric/bounding_box.h"
#include "igesio/numerics/meshes/triangle_mesh.h"



namespace igesio::numerics {

/// @brief メッシュと直線/半直線/線分の交差結果
struct MeshRayHit {
    /// @brief 交点の線パラメータ (交点 = p0 + t*(p1-p0))
    /// @note p1-p0が単位ベクトルの場合はp0からの符号付き距離に等しい
    double t = 0.0;
    /// @brief 交点座標 (メッシュと同じ空間)
    Vector3d position = Vector3d::Zero();
    /// @brief ヒットした三角形の番号
    std::uint32_t triangle_index = 0;
    /// @brief 交点の重心座標u
    /// @note 三角形 (v0, v1, v2) に対し 交点 = (1-u-v)*v0 + u*v1 + v*v2
    double barycentric_u = 0.0;
    /// @brief 交点の重心座標v
    double barycentric_v = 0.0;
};

/// @brief メッシュ交差判定の制御パラメータ
struct MeshIntersectionParams {
    /// @brief 平行・退化判定の相対許容値
    /// @note Möller–Trumboreの行列式の絶対値が、三角形の2辺と方向ベクトルの
    ///       ノルム積にこの係数を掛けた値以下の三角形をスキップする
    ///       (平行な三角形・退化三角形). モデルのスケールに依存しないよう
    ///       相対値で判定する
    double parallel_tol = 1e-12;
    /// @brief 重複解の除去に使用する3D空間距離の許容誤差
    /// @note 共有エッジ・共有頂点上のヒットは隣接三角形で重複しうるため、
    ///       交点間距離がこの値以下のヒットは近い方 (tが小さい方) のみ残す
    double dedup_tol = 1e-6;
};

/// @brief メッシュと直線/半直線/線分の交差点を求める
/// @tparam Scalar メッシュのスカラー型 (float / double. 計算はdoubleで行う)
/// @param mesh 対象のメッシュ (Validateを通る整合したメッシュであること)
/// @param p0 線の始点
/// @param p1 線の方向を定める点 (方向ベクトルはp1-p0)
/// @param direction_type 線の種類 (kSegment: 0<=t<=1, kRay: t>=0, kLine: 制限なし)
/// @param params 交差判定の制御パラメータ
/// @return 交差点のリスト (t昇順・重複除去済み). p0とp1が一致する場合は空リスト
/// @note 表裏は区別しない (裏面側からの交差もヒットする)
/// @note 三角形の境界 (辺・頂点) 上のヒットは、数値誤差により隣接三角形の
///       いずれか一方または両方で検出される (両方の場合はdedup_tolで1つに
///       除去される)
template <typename Scalar>
std::vector<MeshRayHit> IntersectMeshWithLine(
        const TriangleMeshT<Scalar>& mesh,
        const Vector3d& p0, const Vector3d& p1,
        const BoundingBox::DirectionType direction_type,
        const MeshIntersectionParams& params = {}) {
    std::vector<MeshRayHit> hits;
    const Vector3d dir = p1 - p0;
    const double dir_norm = dir.norm();
    if (dir_norm == 0.0) return hits;  // 方向が定義できない

    const auto triangle_count = mesh.TriangleCount();
    for (std::size_t tri = 0; tri < triangle_count; ++tri) {
        const Vector3d v0 =
                mesh.positions.col(mesh.indices[3 * tri]).template cast<double>();
        const Vector3d v1 = mesh.positions.col(
                mesh.indices[3 * tri + 1]).template cast<double>();
        const Vector3d v2 = mesh.positions.col(
                mesh.indices[3 * tri + 2]).template cast<double>();
        const Vector3d edge1 = v1 - v0;
        const Vector3d edge2 = v2 - v0;

        // Möller–Trumbore法. 平行・退化はスケール不変の相対判定でスキップする
        const Vector3d pvec = dir.cross(edge2);
        const double det = edge1.dot(pvec);
        const double scale = edge1.norm() * edge2.norm() * dir_norm;
        if (std::abs(det) <= params.parallel_tol * scale) continue;

        const double inv_det = 1.0 / det;
        const Vector3d tvec = p0 - v0;
        const double u = tvec.dot(pvec) * inv_det;
        if (u < 0.0 || u > 1.0) continue;
        const Vector3d qvec = tvec.cross(edge1);
        const double v = dir.dot(qvec) * inv_det;
        if (v < 0.0 || u + v > 1.0) continue;

        // 線種に応じたパラメータ範囲の確認 (境界値は含む)
        const double line_t = edge2.dot(qvec) * inv_det;
        if (direction_type == BoundingBox::DirectionType::kRay &&
            line_t < 0.0) {
            continue;
        }
        if (direction_type == BoundingBox::DirectionType::kSegment &&
            (line_t < 0.0 || line_t > 1.0)) {
            continue;
        }

        hits.push_back({line_t, p0 + line_t * dir,
                        static_cast<std::uint32_t>(tri), u, v});
    }

    std::sort(hits.begin(), hits.end(),
              [](const MeshRayHit& l, const MeshRayHit& r) {
                  return l.t < r.t;
              });

    // 共有エッジ・共有頂点上の重複ヒットを除去する (t昇順のため近い方が残る)
    std::vector<MeshRayHit> deduped;
    deduped.reserve(hits.size());
    for (const auto& hit : hits) {
        const bool duplicate = std::any_of(
                deduped.begin(), deduped.end(),
                [&hit, &params](const MeshRayHit& kept) {
                    return (kept.position - hit.position).norm() <=
                           params.dedup_tol;
                });
        if (!duplicate) deduped.push_back(hit);
    }
    return deduped;
}

}  // namespace igesio::numerics

#endif  // IGESIO_NUMERICS_MESHES_ALGORITHMS_MESH_LINE_INTERSECTION_H_
