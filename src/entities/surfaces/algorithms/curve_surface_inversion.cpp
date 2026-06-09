/**
 * @file entities/surfaces/algorithms/curve_surface_inversion.cpp
 * @brief モデル空間曲線を曲面のパラメータ空間へ逆射影する実装
 * @author Yayoi Habami
 * @date 2026-05-31
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/entities/surfaces/algorithms/curve_surface_inversion.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "igesio/numerics/core/tolerance.h"
#include "entities/curves/algorithms/polygonal_approximation.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using i_ent::ISurface;
using i_ent::CurveInversionParams;
using igesio::Vector3d;

/// @brief 無限範囲をクランプ済みの有効パラメータ範囲
struct ParamRange {
    double u_min, u_max, v_min, v_max;
};

/// @brief 曲面の代表寸法 L (BBの最大対角線長) を返す
/// @return 有限BBが得られればその対角長、得られなければ 1.0
double CharacteristicLength(const ISurface& surface) {
    const auto bb = surface.GetBoundingBox();
    if (!bb.IsEmpty() && bb.IsFinite()) {
        const auto verts = bb.GetFiniteVertices();
        double max_dist = 0.0;
        for (size_t i = 0; i < verts.size(); ++i) {
            for (size_t j = i + 1; j < verts.size(); ++j) {
                max_dist = std::max(max_dist, (verts[i] - verts[j]).norm());
            }
        }
        if (max_dist > i_num::kGeometryTolerance) return max_dist;
    }
    return 1.0;
}

/// @brief 無限パラメータ範囲を代表寸法Lでクランプした有効範囲を返す
ParamRange GetEffectiveRange(const ISurface& surface, const double extent) {
    const auto r = surface.GetParameterRange();
    auto clamp_one = [&](const double lo,
                         const double hi) -> std::pair<double, double> {
        const bool lo_inf = std::isinf(lo);
        const bool hi_inf = std::isinf(hi);
        if (!lo_inf && !hi_inf) return {lo, hi};
        if (!lo_inf &&  hi_inf) return {lo, lo + extent};
        if ( lo_inf && !hi_inf) return {hi - extent, hi};
        return {-extent * 0.5, extent * 0.5};
    };
    const auto [u_min, u_max] = clamp_one(r[0], r[1]);
    const auto [v_min, v_max] = clamp_one(r[2], r[3]);
    return {u_min, u_max, v_min, v_max};
}

/// @brief 1点の最近点射影をガウス・ニュートン法で解く
///
/// 残差 r = S(u,v) - P を最小化する。近似ヘッセに第一基本形式 [[E,F],[F,G]] を
/// 用い、1階偏導関数のみで更新する。穴領域・退化(pole)・非収束では失敗を返す。
///
/// @return 収束または受理許容内に達した (u,v)。失敗時は `std::nullopt`
std::optional<std::array<double, 2>> RunInversion(
        const ISurface& surface, const Vector3d& p,
        double u, double v, const ParamRange& pr,
        const double L, const CurveInversionParams& params) {
    const double tol_conv = params.convergence_tol * L;
    const double tol_accept = params.accept_tol * L;

    double best_u = u, best_v = v;
    double best_res = std::numeric_limits<double>::infinity();

    for (int iter = 0; iter < params.max_iter; ++iter) {
        const auto d = surface.TryGetDerivatives(u, v, 1);
        if (!d) break;  // 穴領域 (TrimmedSurface 等)
        const Vector3d s  = (*d)(0, 0);
        const Vector3d su = (*d)(1, 0);
        const Vector3d sv = (*d)(0, 1);
        const Vector3d r  = s - p;

        const double res = r.norm();
        if (res < best_res) { best_res = res; best_u = u; best_v = v; }
        if (res < tol_conv) return std::array<double, 2>{u, v};

        // 第一基本形式 (近似ヘッセ)
        const double E = su.squaredNorm();
        const double F = su.dot(sv);
        const double G = sv.squaredNorm();
        const double det = E * G - F * F;
        // det = EG sin^2θ. pole (E~0 or G~0) や Su∥Sv で退化
        if (E <= 0.0 || G <= 0.0 || det < 1e-12 * E * G) break;

        // δ = -H^{-1} g,  g = [r·Su, r·Sv]
        const double gu = r.dot(su);
        const double gv = r.dot(sv);
        const double du = -( G * gu - F * gv) / det;
        const double dv = -(-F * gu + E * gv) / det;

        const double nu = std::clamp(u + du, pr.u_min, pr.u_max);
        const double nv = std::clamp(v + dv, pr.v_min, pr.v_max);
        const bool stationary = (std::abs(nu - u) <= 0.0 && std::abs(nv - v) <= 0.0);
        u = nu;
        v = nv;
        if (stationary) break;  // 境界で停留: 以降は受理判定に委ねる
    }

    // 反復終了: 受理許容内なら最良解を採用する
    if (best_res < tol_accept) return std::array<double, 2>{best_u, best_v};
    return std::nullopt;
}

/// @brief 格子サンプリングでPに最も近い初期パラメータ (u,v) を探す
std::array<double, 2> GridSearchInit(
        const ISurface& surface, const Vector3d& p,
        const ParamRange& pr, const CurveInversionParams& params) {
    const double du = (pr.u_max - pr.u_min) / (params.grid_u + 1);
    const double dv = (pr.v_max - pr.v_min) / (params.grid_v + 1);

    std::array<double, 2> best = {0.5 * (pr.u_min + pr.u_max),
                                  0.5 * (pr.v_min + pr.v_max)};
    double best_dist = std::numeric_limits<double>::infinity();
    for (int iu = 1; iu <= params.grid_u; ++iu) {
        const double u = pr.u_min + iu * du;
        for (int iv = 1; iv <= params.grid_v; ++iv) {
            const double v = pr.v_min + iv * dv;
            const auto pt = surface.TryGetPointAt(u, v);
            if (!pt) continue;
            const double dist = (*pt - p).norm();
            if (dist < best_dist) { best_dist = dist; best = {u, v}; }
        }
    }
    return best;
}

/// @brief 連続する2点間がseam (閉方向のラップ跳び) かどうかを判定する
bool IsSeamJump(const ISurface& surface,
                const std::array<double, 2>& prev,
                const std::array<double, 2>& cur,
                const ParamRange& pr) {
    if (surface.IsUClosed() &&
        std::abs(cur[0] - prev[0]) > 0.5 * (pr.u_max - pr.u_min)) {
        return true;
    }
    if (surface.IsVClosed() &&
        std::abs(cur[1] - prev[1]) > 0.5 * (pr.v_max - pr.v_min)) {
        return true;
    }
    return false;
}

}  // namespace



std::optional<std::array<double, 2>> i_ent::InvertPointOntoSurface(
        const ISurface& surface, const Vector3d& model_point,
        const std::array<double, 2>& init_uv,
        const CurveInversionParams& params) {
    const double L = CharacteristicLength(surface);
    const ParamRange pr = GetEffectiveRange(surface, L);
    return RunInversion(surface, model_point, init_uv[0], init_uv[1],
                        pr, L, params);
}

std::vector<i_ent::ParamSpaceArc> i_ent::InvertCurveOntoSurface(
        const ISurface& surface, const ICurve& model_curve,
        const CurveInversionParams& params) {
    const double L = CharacteristicLength(surface);
    const ParamRange pr = GetEffectiveRange(surface, L);

    // Cを折れ線近似して点列 (順序付き) を得る
    const auto poly = i_ent::ComputeApproximatePolygon(
            model_curve, {}, {}, params.discretization_tol);
    const size_t n = poly.vertices.size();

    std::vector<i_ent::ParamSpaceArc> arcs;
    std::vector<Vector3d> cur_pts;     // 構築中の弧のuv点列
    double arc_r_start = 0.0;          // 構築中の弧の開始r
    double prev_r = 0.0;               // 直前に採用した点のr
    std::optional<std::array<double, 2>> prev_uv;

    // 構築中の弧を確定しarcsに積む (点数2未満は破棄)
    auto flush = [&](const double r_end) {
        if (cur_pts.size() >= 2) {
            arcs.push_back({cur_pts, {arc_r_start, r_end}});
        }
        cur_pts.clear();
    };

    for (size_t i = 0; i < n; ++i) {
        const Vector3d& p = poly.vertices[i];
        const double r_i = poly.curve_params[i];

        // 初期値: 直前解をウォームスタート、無ければ格子探索
        const std::array<double, 2> init =
                prev_uv ? *prev_uv : GridSearchInit(surface, p, pr, params);
        const auto sol = RunInversion(surface, p, init[0], init[1],
                                      pr, L, params);

        // 射影失敗 (pole等)
        if (!sol) {
            if (params.split_at_discontinuities) {
                flush(prev_r);       // 現在の弧をここで打ち切る
                prev_uv = std::nullopt;  // 次点は格子探索からやり直す
            }
            // 単一弧モードでは当該点をスキップ (ウォームスタートは維持)
            continue;
        }

        // seam 跨ぎ: 分割モードでは弧を切り替える
        if (prev_uv && params.split_at_discontinuities &&
            IsSeamJump(surface, *prev_uv, *sol, pr)) {
            flush(prev_r);
        }

        if (cur_pts.empty()) arc_r_start = r_i;
        cur_pts.emplace_back((*sol)[0], (*sol)[1], 0.0);
        prev_uv = sol;
        prev_r = r_i;
    }
    flush(prev_r);

    return arcs;
}
