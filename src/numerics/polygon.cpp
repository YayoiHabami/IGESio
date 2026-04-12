/**
 * @file numerics/polygon.cpp
 * @brief 多角形データ構造の実装
 * @author Yayoi Habami
 * @date 2026-04-10
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/numerics/polygon.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>



namespace {

/// @brief Non-Zero Winding則に基づくwinding numberを計算する
/// @param point 判定する点 (x, y 成分のみ使用)
/// @param vertices 多角形の頂点列
/// @return Winding number. 非ゼロなら点は多角形の内部に存在する
int WindingNumber(
        const igesio::Vector3d& point,
        const std::vector<igesio::Vector3d>& vertices) {
    int winding = 0;
    const int n = static_cast<int>(vertices.size());
    const double px = point[0], py = point[1];
    for (int i = 0; i < n; ++i) {
        const double ax = vertices[i][0], ay = vertices[i][1];
        const double bx = vertices[(i + 1) % n][0];
        const double by = vertices[(i + 1) % n][1];
        if (ay <= py) {
            if (by > py) {
                // 上向き交差: 点が辺の左側なら+1
                if ((bx - ax) * (py - ay) - (by - ay) * (px - ax) > 0.0) {
                    ++winding;
                }
            }
        } else {
            if (by <= py) {
                // 下向き交差: 点が辺の右側なら-1
                if ((bx - ax) * (py - ay) - (by - ay) * (px - ax) < 0.0) {
                    --winding;
                }
            }
        }
    }
    return winding;
}

/// @brief 点pから線分 (a, b) への距離の二乗を返す
/// @note x, y 成分のみ使用する
double PointSegmentDistSq(
        const igesio::Vector3d& p,
        const igesio::Vector3d& a, const igesio::Vector3d& b) {
    const double abx = b[0] - a[0], aby = b[1] - a[1];
    const double ab_sq = abx * abx + aby * aby;
    if (ab_sq == 0.0) {
        const double dx = p[0] - a[0], dy = p[1] - a[1];
        return dx * dx + dy * dy;
    }
    const double t = std::clamp(
        ((p[0] - a[0]) * abx + (p[1] - a[1]) * aby) / ab_sq, 0.0, 1.0);
    const double dx = p[0] - (a[0] + t * abx);
    const double dy = p[1] - (a[1] + t * aby);
    return dx * dx + dy * dy;
}

/// @brief 多角形の全辺の中から点に最も近い辺のインデックスと距離の二乗を返す
/// @param point 判定する点 (x, y 成分のみ使用)
/// @param polygon 対象の多角形
/// @return (最近傍辺のインデックス, 距離の二乗)
std::pair<int, double> FindNearestEdge(
        const igesio::Vector3d& point,
        const igesio::numerics::PolygonData& polygon) {
    const int m = polygon.Count();
    double min_d = std::numeric_limits<double>::infinity();
    int nearest = 0;
    for (int i = 0; i < m; ++i) {
        const double d = PointSegmentDistSq(
            point,
            polygon.vertices[i],
            polygon.vertices[(i + 1) % m]);
        if (d < min_d) {
            min_d = d;
            nearest = i;
        }
    }
    return {nearest, min_d};
}

/// @brief 多角形から i_start〜i_end (両端含む) の頂点列を返す
/// @param polygon 対象の多角形
/// @param i_start 開始頂点インデックス
/// @param i_end 終了頂点インデックス
/// @param is_wrap true の場合は折り返し (末尾→先頭) 処理を行う
/// @return 頂点列
std::vector<igesio::Vector3d> ExtractVerticesRange(
        const igesio::numerics::PolygonData& polygon,
        int i_start, int i_end, bool is_wrap) {
    const auto& v = polygon.vertices;
    if (!is_wrap) {
        return std::vector<igesio::Vector3d>(
            v.begin() + i_start, v.begin() + i_end + 1);
    }
    std::vector<igesio::Vector3d> result(
        v.begin() + i_start, v.end());
    result.insert(result.end(), v.begin(), v.begin() + i_end + 1);
    return result;
}

/// @brief 近似多角形から指定パラメータ範囲に対応する頂点列を返す
/// @param approx 近似多角形 (全頂点 on_curve=true, curve_params は昇順)
/// @param param_start パラメータ範囲の開始値
/// @param param_end パラメータ範囲の終了値
/// @param is_wrap true の場合は折り返し (param_start > param_end) として処理する
/// @return 対応する頂点列 (param_start から param_end の順)
std::vector<igesio::Vector3d> ExtractApproxByParam(
        const igesio::numerics::PolygonData& approx,
        double param_start, double param_end, bool is_wrap) {
    const int n = approx.Count();
    std::vector<igesio::Vector3d> result;
    if (!is_wrap) {
        for (int i = 0; i < n; ++i) {
            const double p = approx.curve_params[i];
            if (p >= param_start && p <= param_end) {
                result.push_back(approx.vertices[i]);
            }
        }
        return result;
    }
    // 折り返し: param_start以上の部分 (後半) → param_end以下の部分 (前半)
    for (int i = 0; i < n; ++i) {
        if (approx.curve_params[i] >= param_start) {
            result.push_back(approx.vertices[i]);
        }
    }
    for (int i = 0; i < n; ++i) {
        if (approx.curve_params[i] <= param_end) {
            result.push_back(approx.vertices[i]);
        }
    }
    return result;
}

}  // namespace



namespace igesio::numerics {

int PolygonData::Count() const noexcept {
    return static_cast<int>(vertices.size());
}

std::pair<int, int> PolygonData::GetCurveParamIndex(int edge_index) const {
    const int m = Count();
    if (edge_index < 0 || edge_index >= m) {
        throw std::out_of_range(
            "edge_index must be in [0, " + std::to_string(m - 1) +
            "], got " + std::to_string(edge_index));
    }

    // 全頂点が on_curve=false の場合はパラメータ範囲を決定できない
    bool any_on_curve = false;
    for (bool flag : on_curve) {
        if (flag) {
            any_on_curve = true;
            break;
        }
    }
    if (!any_on_curve) {
        throw std::invalid_argument(
            "All vertices have on_curve=false. "
            "Cannot determine curve parameter range.");
    }

    // 始点側: on_curveがtrueになるまで遡る
    int start = edge_index;
    while (!on_curve[start]) start = (start - 1 + m) % m;

    // 終点側: on_curveがtrueになるまで進む
    int end = (edge_index + 1) % m;
    while (!on_curve[end]) end = (end + 1) % m;

    return {start, end};
}

bool IsPointInPolygon(
        const Vector3d& point,
        const CurveContainmentPolygons& polygons) {
    // 外包多角形の外部 → 外部
    if (WindingNumber(point, polygons.circumscribed.vertices) == 0) {
        return false;
    }

    // 内包多角形の内部 → 内部
    if (WindingNumber(point, polygons.inscribed.vertices) != 0) {
        return true;
    }

    // 以降は詳細な判定
    // 内包・外包多角形の全辺から最近傍辺を探索
    const auto [circ_idx, circ_d] = FindNearestEdge(
        point, polygons.circumscribed);
    const auto [insc_idx, insc_d] = FindNearestEdge(
        point, polygons.inscribed);

    const bool use_inscribed = (insc_d <= circ_d);
    const PolygonData& nearest_poly = use_inscribed
        ? polygons.inscribed : polygons.circumscribed;
    const int nearest_edge = use_inscribed ? insc_idx : circ_idx;

    // パラメータ範囲の取得: i_start > i_end の場合に折り返しとする
    const auto [i_start, i_end] = nearest_poly.GetCurveParamIndex(nearest_edge);
    const bool is_wrap = (i_start > i_end);

    const std::vector<Vector3d> poly_verts = ExtractVerticesRange(
        nearest_poly, i_start, i_end, is_wrap);
    const double param_start = nearest_poly.curve_params[i_start];
    const double param_end   = nearest_poly.curve_params[i_end];

    // 近似多角形から対応するパラメータ範囲の頂点を取得
    const std::vector<Vector3d> approx_verts = ExtractApproxByParam(
        polygons.approximate, param_start, param_end, is_wrap);

    // 再構成多角形の構築
    // poly_verts (順方向, 末端除く) + approx_verts (逆方向, 末端除く) で閉多角形を形成する
    std::vector<Vector3d> reconstructed;
    reconstructed.insert(reconstructed.end(),
        poly_verts.begin(), poly_verts.end() - 1);
    reconstructed.insert(reconstructed.end(),
        approx_verts.rbegin(), approx_verts.rend() - 1);

    // 再構成多角形による内外判定
    // 内包多角形の辺を使用: 内部→内部、外部→外部
    // 外包多角形の辺を使用: 内部→外部 (反転)、外部→内部 (反転)
    const bool inside = (WindingNumber(point, reconstructed) != 0);
    return use_inscribed ? inside : !inside;
}

}  // namespace igesio::numerics
