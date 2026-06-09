/**
 * @file entities/surfaces/algorithms/surface_line_intersection.cpp
 * @brief 曲面と直線（半直線・線分を含む）の交点計算の実装
 * @author Yayoi Habami
 * @date 2026-04-13
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/entities/surfaces/algorithms/surface_line_intersection.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "igesio/numerics/core/tolerance.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using i_ent::ISurface;
using i_ent::SurfaceLineIntersection;
using i_ent::SurfaceLineIntersectionParams;
using igesio::Matrix3d;
using igesio::Vector3d;
/// @brief BoundingBox::DirectionType の短縮名
using LineType = i_num::BoundingBox::DirectionType;



/// @brief 有効なuvパラメータ範囲（無限範囲をクランプ済み）
struct ParamRange {
    /// @brief uの最小値
    double u_min;
    /// @brief uの最大値
    double u_max;
    /// @brief vの最小値
    double v_min;
    /// @brief vの最大値
    double v_max;
};



/// @brief 線パラメータtが線種の有効範囲内かを判定する
bool IsValidT(const double t, const LineType line_type,
              const double eps = 1e-9) {
    switch (line_type) {
        case LineType::kSegment: return t >= -eps && t <= 1.0 + eps;
        case LineType::kRay:     return t >= -eps;
        default:                 return true;  // kLine: 範囲制約なし
    }
}

/// @brief 曲面のBBからパラメータ探索幅のフォールバック値を計算する
double ComputeFallbackExtent(const ISurface& surface,
                              const Vector3d& p0,
                              const Vector3d& p1) {
    const auto bb = surface.GetBoundingBox();
    if (!bb.IsEmpty() && bb.IsFinite()) {
        // 有限BBの最大対角線長を探索幅とする
        const auto verts = bb.GetFiniteVertices();
        double max_dist = 0.0;
        for (size_t i = 0; i < verts.size(); ++i) {
            for (size_t j = i + 1; j < verts.size(); ++j) {
                max_dist = std::max(
                    max_dist, (verts[i] - verts[j]).norm());
            }
        }
        if (max_dist > i_num::kGeometryTolerance) return max_dist;
    }
    // BBが無限または空の場合: 線分長×100またはフォールバック定数
    const double line_len = (p1 - p0).norm();
    return (line_len > i_num::kGeometryTolerance)
        ? line_len * 100.0 : 1.0e6;
}

/// @brief 無限パラメータ範囲をfallback_extentでクランプした有効範囲を返す
ParamRange GetEffectiveParamRange(const ISurface& surface,
                                   const double fallback_extent) {
    const auto r = surface.GetParameterRange();

    // 片側または両側が無限の区間を有限にクランプする
    auto clamp_range = [&](const double lo,
                           const double hi) -> std::pair<double, double> {
        const bool lo_inf = std::isinf(lo);
        const bool hi_inf = std::isinf(hi);
        if (!lo_inf && !hi_inf) return {lo, hi};
        if (!lo_inf &&  hi_inf) return {lo, lo + fallback_extent};
        if ( lo_inf && !hi_inf) return {hi - fallback_extent, hi};
        // 両側無限: 原点中心にfallback_extentの幅を設定
        return {-fallback_extent * 0.5, fallback_extent * 0.5};
    };

    const auto [u_min, u_max] = clamp_range(r[0], r[1]);
    const auto [v_min, v_max] = clamp_range(r[2], r[3]);
    return {u_min, u_max, v_min, v_max};
}

/// @brief 曲面上の点Sから L(t) = p0 + t*d への射影パラメータtを返す
double ProjectOntoLine(const Vector3d& point, const Vector3d& p0,
                       const Vector3d& d, const double d_sq) {
    return (point - p0).dot(d) / d_sq;
}

/// @brief 1組の初期値 (u,v,t) からニュートン法で交点を求める
///
/// 残差 F(u,v,t) = S(u,v) - (p0 + t*d) = 0 をヤコビアン
/// J = [Su | Sv | -d] による反復で解く。
/// 穴領域 (TryGetDerivativesがnullopt) に到達した場合も失敗とする。
///
/// @note TryGetDerivativesはモデル空間(M_entity適用済み)を返す。p0/dは累積配置を
///       含むワールド空間のため、TryGetPointAt/TryGetTangentAtでワールド空間に統一して
///       残差を評価する。IGES変換は直交行列+並進（スケールなし）なので |Su| はモデル空間
///       でもワールド空間でも不変であり、偏導関数からはノルムのみを用いる。
///
/// @return 収束した交点情報、失敗の場合はnullopt
std::optional<SurfaceLineIntersection> RunNewton(
        const ISurface& surface,
        const Vector3d& p0, const Vector3d& d,
        double u, double v, double t,
        const ParamRange& pr,
        const SurfaceLineIntersectionParams& params,
        const LineType line_type) {
    for (int iter = 0; iter < params.max_iter; ++iter) {
        // 偏導関数を取得 (ノルムのみ使用; 穴領域ではnulloptが返る)
        const auto deriv = surface.TryGetDerivatives(u, v, 1);
        if (!deriv) return std::nullopt;

        // ワールド空間のS, Su, Svを構築
        // Su_world = |Su_def| × (ワールド空間の単位接線方向)
        const auto S_opt = surface.TryGetPointAt(u, v);
        if (!S_opt) return std::nullopt;
        const auto tan_opt = surface.TryGetTangentAt(u, v);
        if (!tan_opt) return std::nullopt;

        const Vector3d S  = *S_opt;
        const Vector3d Su = (*deriv)(1, 0).norm() * tan_opt->first;
        const Vector3d Sv = (*deriv)(0, 1).norm() * tan_opt->second;
        const Vector3d F  = S - (p0 + t * d);

        if (F.norm() < params.convergence_tol) {
            if (!IsValidT(t, line_type)) return std::nullopt;
            return SurfaceLineIntersection{S, u, v, t};
        }

        // ヤコビアン J = [Su | Sv | -d]
        Matrix3d J;
        J.col(0) = Su;
        J.col(1) = Sv;
        J.col(2) = -d;

        // 行列式でランク落ちを検出 (線が曲面の接線方向に平行な場合等)
        if (std::abs(J.determinant()) < 1e-12) return std::nullopt;

        const Vector3d delta = J.inverse() * (-F);
        // uvはパラメータ範囲にクランプしつつ更新
        u = std::clamp(u + delta(0), pr.u_min, pr.u_max);
        v = std::clamp(v + delta(1), pr.v_min, pr.v_max);
        t += delta(2);
    }
    return std::nullopt;  // 最大反復回数到達
}

/// @brief 3D距離がtol未満の重複解を除去する（インプレース）
void Deduplicate(std::vector<SurfaceLineIntersection>& results,
                  const double tol) {
    std::vector<bool> keep(results.size(), true);
    for (size_t i = 0; i < results.size(); ++i) {
        if (!keep[i]) continue;
        for (size_t j = i + 1; j < results.size(); ++j) {
            const double dist =
                (results[i].position - results[j].position).norm();
            if (dist < tol) keep[j] = false;
        }
    }
    // keepフラグが立っている要素のみ残す
    size_t write = 0;
    for (size_t i = 0; i < results.size(); ++i) {
        if (keep[i]) results[write++] = results[i];
    }
    results.resize(write);
}

}  // namespace



/**
 * 曲面と直線（半直線・線分を含む）の交点を計算する
 */

std::vector<SurfaceLineIntersection> i_ent::IntersectSurfaceWithLine(
        const ISurface& surface,
        const Vector3d& p0, const Vector3d& p1,
        const LineType line_type,
        const SurfaceLineIntersectionParams& params) {
    if (!p0.allFinite() || !p1.allFinite()) {
        throw std::invalid_argument(
            "IntersectSurfaceWithLine: p0 and p1 must be finite.");
    }

    // 方向ベクトルのゼロチェック
    const Vector3d d = p1 - p0;
    const double d_sq = d.squaredNorm();
    const double tol_sq =
        i_num::kGeometryTolerance * i_num::kGeometryTolerance;
    if (d_sq < tol_sq) return {};

    // バウンディングボックスによる事前棄却
    if (!surface.GetBoundingBox().Intersects(p0, p1, line_type)) {
        return {};
    }

    // 有効パラメータ範囲の計算
    const double extent = ComputeFallbackExtent(surface, p0, p1);
    const ParamRange pr = GetEffectiveParamRange(surface, extent);
    if (pr.u_max <= pr.u_min || pr.v_max <= pr.v_min) return {};

    // 格子サンプリングと多スタートニュートン法
    std::vector<SurfaceLineIntersection> candidates;
    const double du =
        (pr.u_max - pr.u_min) / (params.u_samples + 1);
    const double dv =
        (pr.v_max - pr.v_min) / (params.v_samples + 1);

    for (int iu = 1; iu <= params.u_samples; ++iu) {
        const double u = pr.u_min + iu * du;
        for (int iv = 1; iv <= params.v_samples; ++iv) {
            const double v = pr.v_min + iv * dv;

            // 穴領域のサンプルはスキップ
            const auto pt = surface.TryGetPointAt(u, v);
            if (!pt) continue;

            const double t_init =
                ProjectOntoLine(*pt, p0, d, d_sq);
            const auto result = RunNewton(
                surface, p0, d, u, v, t_init,
                pr, params, line_type);
            if (result) candidates.push_back(*result);
        }
    }

    Deduplicate(candidates, params.dedup_tol);
    std::sort(candidates.begin(), candidates.end(),
              [](const SurfaceLineIntersection& a,
                 const SurfaceLineIntersection& b) { return a.t < b.t; });
    return candidates;
}
