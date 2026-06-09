/**
 * @file entities/surfaces/algorithms/surface_boundary_edges.cpp
 * @brief 曲面の境界エッジ(折れ線群)の生成の実装
 * @author Yayoi Habami
 * @date 2026-06-03
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/entities/surfaces/algorithms/surface_boundary_edges.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include "igesio/entities/interfaces/i_curve.h"



namespace igesio::entities {

namespace {

/// @brief 値が無限の場合に代替値へクランプする
double ClampInfinite(const double x, const double fallback) {
    return std::isinf(x) ? fallback : x;
}

/// @brief 構築中の折れ線を確定し、2点以上ならloopsへ追加してクリアする
/// @param current 構築中の折れ線 (確定後は空になる)
/// @param loops 追加先のループ群
void FlushPolyline(std::vector<Vector3d>& current,
                   std::vector<std::vector<Vector3d>>& loops) {
    if (current.size() >= 2) loops.push_back(std::move(current));
    current.clear();
}

/// @brief UV境界曲線B(t)を基底曲面Sで評価し、S(B(t))の折れ線群を生成する
/// @param base 基底曲面S
/// @param uv_curve UV空間の境界曲線B(t) (TryGetPointAtが(u,v,0)を返す)
/// @param divisions 分割数
/// @param loops 追加先のループ群
/// @note Bの評価不能点、または基底曲面のドメイン外で折れ線を分割する
/// @note 等間隔点に加えて境界曲線の角点も評価点に含め、S(B(t))が曲面の角を
///       丸めず正確に通過するようにする
void AppendSurfaceCurveLoop(const ISurface& base, const ICurve& uv_curve,
                            const int divisions,
                            std::vector<std::vector<Vector3d>>& loops) {
    auto range = uv_curve.GetParameterRange();
    const double t0 = ClampInfinite(range[0], -kInfiniteParamClamp);
    const double t1 = ClampInfinite(range[1], kInfiniteParamClamp);
    const auto params =
            BuildCornerAwareSampleParams(uv_curve, t0, t1, divisions);

    std::vector<Vector3d> current;
    for (const double t : params) {
        const auto uv = uv_curve.TryGetPointAt(t);
        std::optional<Vector3d> p;
        if (uv) p = base.TryGetPointAt(uv->x(), uv->y());

        if (p) {
            current.push_back(*p);
        } else {
            FlushPolyline(current, loops);
        }
    }
    FlushPolyline(current, loops);
}

/// @brief 1本のアイソパラ辺をサンプリングして折れ線群へ追加する
/// @param surf 曲面
/// @param vary_u trueならuを動かしvを固定、falseならvを動かしuを固定
/// @param fixed 固定する側のパラメータ値
/// @param s0 動かす側のパラメータ開始値
/// @param s1 動かす側のパラメータ終了値
/// @param divisions 分割数
/// @param loops 追加先のループ群
void AppendIsoEdge(const ISurface& surf, const bool vary_u, const double fixed,
                   const double s0, const double s1, const int divisions,
                   std::vector<std::vector<Vector3d>>& loops) {
    if (!(s1 > s0) || divisions < 1) return;

    std::vector<Vector3d> current;
    for (int k = 0; k <= divisions; ++k) {
        const double s = s0 + (s1 - s0) * k / static_cast<double>(divisions);
        const double u = vary_u ? s : fixed;
        const double v = vary_u ? fixed : s;
        const auto p = surf.TryGetPointAt(u, v);

        if (p) {
            current.push_back(*p);
        } else {
            FlushPolyline(current, loops);
        }
    }
    FlushPolyline(current, loops);
}

}  // namespace



std::vector<double> BuildCornerAwareSampleParams(
        const ICurve& curve, const double t0, const double t1,
        const int divisions) {
    std::vector<double> params;
    if (!(t1 > t0) || divisions < 1) return params;

    params.reserve(divisions + 1);
    for (int k = 0; k <= divisions; ++k) {
        params.push_back(t0 + (t1 - t0) * k / static_cast<double>(divisions));
    }

    // 開区間内の角点を併合する (端点t0/t1は等間隔点に既に含まれる)。
    // 近接重複判定の幅はパラメータスパンに対する相対値とする
    const double eps = (t1 - t0) * 1e-9;
    for (const double tc : curve.GetCornerParams()) {
        if (tc > t0 + eps && tc < t1 - eps) params.push_back(tc);
    }

    std::sort(params.begin(), params.end());
    params.erase(
        std::unique(params.begin(), params.end(),
                    [eps](const double a, const double b) {
                        return std::abs(a - b) <= eps;
                    }),
        params.end());
    return params;
}

SurfaceBoundaryEdges ComputeParametricSurfaceEdges(
        const ISurface& surf, const SurfaceBoundaryEdgeParams& params) {
    const auto range = surf.GetParameterRange();  // {u0, u1, v0, v1}
    const double u0 = ClampInfinite(range[0], -kInfiniteParamClamp);
    const double u1 = ClampInfinite(range[1], kInfiniteParamClamp);
    const double v0 = ClampInfinite(range[2], -kInfiniteParamClamp);
    const double v1 = ClampInfinite(range[3], kInfiniteParamClamp);

    SurfaceBoundaryEdges edges;
    // 閉じた方向のアイソ辺は両端が一致する継ぎ目のため、既定では除外する
    const bool skip_u_iso = surf.IsUClosed() && !params.include_seams;
    const bool skip_v_iso = surf.IsVClosed() && !params.include_seams;

    // v一定 (uを動かす) のアイソ辺: v=v0, v=v1
    if (!skip_v_iso) {
        AppendIsoEdge(surf, true, v0, u0, u1, params.divisions, edges.loops);
        AppendIsoEdge(surf, true, v1, u0, u1, params.divisions, edges.loops);
    }
    // u一定 (vを動かす) のアイソ辺: u=u0, u=u1
    if (!skip_u_iso) {
        AppendIsoEdge(surf, false, u0, v0, v1, params.divisions, edges.loops);
        AppendIsoEdge(surf, false, u1, v0, v1, params.divisions, edges.loops);
    }
    return edges;
}

SurfaceBoundaryEdges ComputeSurfaceCreaseEdges(
        const ISurface& surf, const SurfaceBoundaryEdgeParams& params) {
    const auto range = surf.GetParameterRange();  // {u0, u1, v0, v1}
    const double u0 = ClampInfinite(range[0], -kInfiniteParamClamp);
    const double u1 = ClampInfinite(range[1], kInfiniteParamClamp);
    const double v0 = ClampInfinite(range[2], -kInfiniteParamClamp);
    const double v1 = ClampInfinite(range[3], kInfiniteParamClamp);

    SurfaceBoundaryEdges edges;
    // 折り目位置u_cのv方向アイソ曲線 S(u_c, v) を内部稜線として追加する
    for (const double uc : surf.GetUCreaseParameters()) {
        // 開区間内のもののみ (端は境界アイソ辺と重複するため除外)
        if (uc <= u0 || uc >= u1) continue;
        AppendIsoEdge(surf, /*vary_u=*/false, uc, v0, v1,
                      params.divisions, edges.loops);
    }
    return edges;
}

SurfaceBoundaryEdges ComputeRestrictedSurfaceEdges(
        const IRestrictedSurface& surface,
        const SurfaceBoundaryEdgeParams& params) {
    SurfaceBoundaryEdges edges;
    const auto base = surface.GetBaseSurface();
    if (!base) return edges;

    // 外側境界: 制限なし (N1=0相当) は基底曲面のパラメータ矩形、
    //           明示指定 (N1=1相当) はUV外側境界をS(B(t))で評価する
    if (surface.IsOuterBoundaryOfD()) {
        auto rect = ComputeParametricSurfaceEdges(*base, params);
        for (auto& loop : rect.loops) edges.loops.push_back(std::move(loop));
    } else if (const auto outer = surface.GetOuterUVBoundary()) {
        AppendSurfaceCurveLoop(*base, *outer, params.divisions, edges.loops);
    }

    // 内側境界 (穴): 常にUV境界をS(B(t))で評価する
    const std::size_t n_inner = surface.GetInnerBoundaryCount();
    for (std::size_t i = 0; i < n_inner; ++i) {
        if (const auto inner = surface.GetInnerUVBoundaryAt(i)) {
            AppendSurfaceCurveLoop(*base, *inner, params.divisions, edges.loops);
        }
    }
    return edges;
}

}  // namespace igesio::entities
