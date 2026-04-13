/**
 * @file entities/curves/algorithms.cpp
 * @brief 曲線関連のアルゴリズム定義
 * @author Yayoi Habami
 * @date 2025-10-29
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/curves/algorithms.h"

#include <utility>

#include "igesio/numerics/tolerance.h"
#include "entities/curves/algorithms/extremal_polygon.h"
#include "entities/curves/algorithms/polygonal_approximation.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using igesio::Vector3d;

}  // namespace



/**
 * 点と直線（or半直線or線分）の距離を計算する
 */

double i_ent::PointLineDistance(
        const Vector3d& point,
        const Vector3d& anchor1, const Vector3d& anchor2,
        const std::pair<bool, bool>& is_finite) {
    // 直線ベクトル
    Vector3d line_vec = anchor2 - anchor1;
    Vector3d point_vec = point - anchor1;
    double line_len_sq = line_vec.squaredNorm();
    if (i_num::IsApproxZero(line_len_sq, i_num::kGeometryTolerance)) {
        // anchor1とanchor2が同じ点の場合、anchor1までの距離を返す
        return (point - anchor1).norm();
    }

    // 点から直線への射影のパラメータ値tを計算
    // t=0のときanchor1、t=1のときanchor2と一致
    double t = point_vec.dot(line_vec) / line_len_sq;

    // 射影点（垂線の足）が線分・半直線の範囲内にあるか確認
    if (is_finite.first && t < 0.0) {
        // anchor1側が有限で、t<0の場合、anchor1までの距離を返す
        return (point - anchor1).norm();
    } else if (is_finite.second && t > 1.0) {
        // anchor2側が有限で、t>1の場合、anchor2までの距離を返す
        return (point - anchor2).norm();
    }

    // 射影点の座標を計算
    Vector3d projection = anchor1 + t * line_vec;
    return (point - projection).norm();
}



/**
 * 閉曲線の内包・外包・近似多角形を計算する
 */

i_num::CurveContainmentPolygons
i_ent::ComputeContainmentPolygons(
        const ICurve& curve,
        int n_vert,
        const Vector3d& reference_normal,
        double curvature_eps,
        double distance_eps) {
    auto circumscribed = ComputeCircumscribedPolygon(
        curve, n_vert, reference_normal, curvature_eps);
    auto inscribed = ComputeInscribedPolygon(
        curve, n_vert, reference_normal, curvature_eps);
    auto approximate = ComputeApproximatePolygon(
        curve, inscribed, circumscribed, distance_eps);
    return {circumscribed, inscribed, approximate};
}
