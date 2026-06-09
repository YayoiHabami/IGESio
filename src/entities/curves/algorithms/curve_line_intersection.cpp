/**
 * @file entities/curves/algorithms/curve_line_intersection.cpp
 * @brief 曲線と直線（半直線・線分を含む）の近接判定の実装
 * @author Yayoi Habami
 * @date 2026-05-27
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/entities/curves/algorithms/curve_line_intersection.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "igesio/numerics/core/tolerance.h"
#include "igesio/entities/curves/algorithms.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using i_ent::ICurve;
using i_ent::CurveLineIntersection;
using i_ent::CurveLineIntersectionParams;
using igesio::Vector3d;
/// @brief BoundingBox::DirectionType の短縮名
using LineType = i_num::BoundingBox::DirectionType;



/// @brief 有効なパラメータ範囲（無限範囲をクランプ済み）
struct ParamRange {
    /// @brief tの最小値
    double t_min;
    /// @brief tの最大値
    double t_max;
};



/// @brief 線パラメータsが線種の有効範囲内かを判定する
bool IsValidS(const double s, const LineType line_type,
              const double eps = 1e-9) {
    switch (line_type) {
        case LineType::kSegment: return s >= -eps && s <= 1.0 + eps;
        case LineType::kRay:     return s >= -eps;
        default:                 return true;  // kLine: 範囲制約なし
    }
}

/// @brief 線種をPointLineDistance用のis_finiteペアへ変換する
std::pair<bool, bool> ToIsFinite(const LineType line_type) {
    switch (line_type) {
        case LineType::kSegment: return {true, true};
        case LineType::kRay:     return {true, false};
        default:                 return {false, false};  // kLine
    }
}

/// @brief 曲線のBBからパラメータ探索幅のフォールバック値を計算する
double ComputeFallbackExtent(const ICurve& curve,
                             const Vector3d& p0,
                             const Vector3d& p1) {
    const auto bb = curve.GetBoundingBox();
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
ParamRange GetEffectiveParamRange(const ICurve& curve,
                                  const double fallback_extent) {
    const auto r = curve.GetParameterRange();

    // 片側または両側が無限の区間を有限にクランプする
    const double lo = r[0];
    const double hi = r[1];
    const bool lo_inf = std::isinf(lo);
    const bool hi_inf = std::isinf(hi);
    if (!lo_inf && !hi_inf) return {lo, hi};
    if (!lo_inf &&  hi_inf) return {lo, lo + fallback_extent};
    if ( lo_inf && !hi_inf) return {hi - fallback_extent, hi};
    // 両側無限: 原点中心にfallback_extentの幅を設定
    return {-fallback_extent * 0.5, fallback_extent * 0.5};
}

/// @brief 点Pから L(s) = p0 + s*d への射影パラメータsを返す
double ProjectOntoLine(const Vector3d& point, const Vector3d& p0,
                       const Vector3d& d, const double d_sq) {
    return (point - p0).dot(d) / d_sq;
}

/// @brief バウンディングボックスの外接球による広域棄却を通過するか判定する
///
/// 曲線は必ずBB（≒外接球）内に含まれるため、線と球中心の距離から半径を
/// 引いた値が hit_tolerance を超える場合、最近接距離は必ずそれを超える
/// → 安全に棄却できる。無限BB（直線Line等）では球が作れないため通過させる。
///
/// @return 近接可能性がある（探索を行うべき）場合にtrue
bool PassesBroadPhase(const ICurve& curve,
                      const Vector3d& p0, const Vector3d& p1,
                      const LineType line_type,
                      const double hit_tolerance) {
    const auto bb = curve.GetBoundingBox();
    if (bb.IsEmpty() || !bb.IsFinite()) return true;

    const auto verts = bb.GetFiniteVertices();
    if (verts.empty()) return true;

    Vector3d center = Vector3d::Zero();
    for (const auto& v : verts) center += v;
    center /= static_cast<double>(verts.size());

    double radius = 0.0;
    for (const auto& v : verts) {
        radius = std::max(radius, (v - center).norm());
    }

    const double dist =
        i_ent::PointLineDistance(center, p0, p1, ToIsFinite(line_type));
    return (dist - radius) <= hit_tolerance;
}

/// @brief 1組の初期値tからニュートン法で最近傍点を求める
///
/// distance^2(C(t), L(s)) の最小点を求める。sは射影で消去され、
/// φ(t) = ½|w(t)|² (w = C(t) - L(s(t)), w⊥d) の1変数最小化に帰着する。
///   φ'(t)  = w・C'(t)
///   φ''(t) = |C'|² - (C'・d)²/|d|² + w・C''(t)
/// 2次項 w・C'' は、近接（gap≠0）の最近接点で接線がレイと平行になり
/// |C'|² - (C'・d)²/|d|² がゼロへ退化する場合に収束を支えるため必須。
/// 本項のみ有限差分（ワールド点の2階中心差分）で評価する。収束判定は解析的な
/// φ'=w・C' で行うため、2次項の差分誤差は最終精度に影響しない。
///
/// @note TryGetDerivativesはモデル空間(M_entity適用済み)を返す。p0/dは累積配置を
///       含むワールド空間のため、C'(t)_world = |C'(t)| × (ワールド単位接線) として
///       残差を評価する。IGES変換は直交行列+並進（スケールなし）なので |C'| はモデル空間
///       でもワールド空間でも不変であり、導関数からはノルムのみを用いる。
///
/// @return 収束した最近傍点情報（線種sを満たすもの）、失敗の場合はnullopt
std::optional<CurveLineIntersection> RunNewton(
        const ICurve& curve,
        const Vector3d& p0, const Vector3d& d, const double d_sq,
        double t,
        const ParamRange& pr,
        const CurveLineIntersectionParams& params,
        const LineType line_type) {
    // 2次項の有限差分ステップ（パラメータ範囲に比例）
    const double h = std::max((pr.t_max - pr.t_min) * 1e-4, 1e-7);
    for (int iter = 0; iter < params.max_iter; ++iter) {
        // 1階導関数のノルム（モデル空間/ワールドでノルム不変）
        const auto deriv = curve.TryGetDerivatives(t, 1);
        if (!deriv) return std::nullopt;
        const double cp_norm = (*deriv)[1].norm();
        // 速度ゼロ（カスプ等）の点では方向が定まらないため棄却
        if (cp_norm < i_num::kGeometryTolerance) return std::nullopt;

        // ワールド空間の点・単位接線を取得し C'(t)_world を構築
        const auto C_opt = curve.TryGetPointAt(t);
        if (!C_opt) return std::nullopt;
        const auto tan_opt = curve.TryGetTangentAt(t);
        if (!tan_opt) return std::nullopt;

        const Vector3d C  = *C_opt;
        const Vector3d Cp = cp_norm * (*tan_opt);
        const double s = ProjectOntoLine(C, p0, d, d_sq);
        const Vector3d w = C - (p0 + s * d);  // w⊥d

        // φ'(t) = w・C'
        const double phi_p = w.dot(Cp);
        const double cpd = Cp.dot(d);
        double phi_pp = Cp.squaredNorm() - cpd * cpd / d_sq;

        // 2次項 w・C'' を有限差分で加算（範囲内で評価できる場合のみ）。
        // 接線がレイと平行な最近接点ではこの項がφ''の主要項となる。
        const double tp = t + h;
        const double tm = t - h;
        if (tp <= pr.t_max && tm >= pr.t_min) {
            const auto Pp = curve.TryGetPointAt(tp);
            const auto Pm = curve.TryGetPointAt(tm);
            if (Pp && Pm) {
                const Vector3d Cpp = (*Pp - 2.0 * C + *Pm) / (h * h);
                phi_pp += w.dot(Cpp);
            }
        }

        // φ'' <= 0 は極小ではない（極大・変曲点）ため棄却
        if (phi_pp < i_num::kGeometryTolerance) return std::nullopt;

        const double dt = -phi_p / phi_pp;
        const double t_new = std::clamp(t + dt, pr.t_min, pr.t_max);
        const double step = t_new - t;  // クランプ後の実移動量
        t = t_new;

        if (std::abs(step) < params.convergence_tol) {
            // 収束: 最終tで点・線パラメータ・gapを再評価
            const auto Cf_opt = curve.TryGetPointAt(t);
            if (!Cf_opt) return std::nullopt;
            const Vector3d Cf = *Cf_opt;
            const double sf = ProjectOntoLine(Cf, p0, d, d_sq);
            if (!IsValidS(sf, line_type)) return std::nullopt;
            const double gap = (Cf - (p0 + sf * d)).norm();
            return CurveLineIntersection{Cf, t, sf, gap};
        }
    }
    return std::nullopt;  // 最大反復回数到達
}

/// @brief 3D距離がtol未満の重複解を除去する（インプレース）
void Deduplicate(std::vector<CurveLineIntersection>& results,
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
 * 曲線と直線（半直線・線分を含む）の最近傍点を求める
 */

std::vector<CurveLineIntersection> i_ent::IntersectCurveWithLine(
        const ICurve& curve,
        const Vector3d& p0, const Vector3d& p1,
        const LineType line_type,
        const CurveLineIntersectionParams& params) {
    if (!p0.allFinite() || !p1.allFinite()) {
        throw std::invalid_argument(
            "IntersectCurveWithLine: p0 and p1 must be finite.");
    }

    // 方向ベクトルのゼロチェック
    const Vector3d d = p1 - p0;
    const double d_sq = d.squaredNorm();
    const double tol_sq =
        i_num::kGeometryTolerance * i_num::kGeometryTolerance;
    if (d_sq < tol_sq) return {};

    // 外接球による広域棄却
    if (!PassesBroadPhase(curve, p0, p1, line_type, params.hit_tolerance)) {
        return {};
    }

    // 有効パラメータ範囲の計算
    const double extent = ComputeFallbackExtent(curve, p0, p1);
    const ParamRange pr = GetEffectiveParamRange(curve, extent);
    if (pr.t_max <= pr.t_min) return {};

    // 格子サンプリングと多スタートニュートン法
    std::vector<CurveLineIntersection> candidates;
    const auto try_start = [&](const double t0) {
        const auto result =
            RunNewton(curve, p0, d, d_sq, t0, pr, params, line_type);
        if (result && result->gap < params.hit_tolerance) {
            candidates.push_back(*result);
        }
    };

    const double dt = (pr.t_max - pr.t_min) / (params.curve_samples + 1);
    for (int i = 1; i <= params.curve_samples; ++i) {
        try_start(pr.t_min + i * dt);
    }
    // 有限端点も初期値に加える（kSegment等で端点近傍が最近接の場合に対応）
    try_start(pr.t_min);
    try_start(pr.t_max);

    Deduplicate(candidates, params.dedup_tol);
    std::sort(candidates.begin(), candidates.end(),
              [](const CurveLineIntersection& a,
                 const CurveLineIntersection& b) {
                  return a.t_line < b.t_line;
              });
    return candidates;
}



/**
 * 点と直線（半直線・線分を含む）の最近接判定
 */

std::optional<CurveLineIntersection> i_ent::IntersectPointWithLine(
        const Vector3d& point,
        const Vector3d& p0, const Vector3d& p1,
        const LineType line_type,
        const double hit_tolerance) {
    if (!point.allFinite() || !p0.allFinite() || !p1.allFinite()) {
        throw std::invalid_argument(
            "IntersectPointWithLine: point, p0 and p1 must be finite.");
    }

    // 方向ベクトルのゼロチェック
    const Vector3d d = p1 - p0;
    const double d_sq = d.squaredNorm();
    const double tol_sq =
        i_num::kGeometryTolerance * i_num::kGeometryTolerance;
    if (d_sq < tol_sq) return std::nullopt;

    // 点を直線へ射影し、線種の有効範囲を確認
    const double s = ProjectOntoLine(point, p0, d, d_sq);
    if (!IsValidS(s, line_type)) return std::nullopt;

    // 最近接点との3D距離がhit_tolerance以下であればヒット
    const double gap = (point - (p0 + s * d)).norm();
    if (gap > hit_tolerance) return std::nullopt;

    return CurveLineIntersection{point, 0.0, s, gap};
}
