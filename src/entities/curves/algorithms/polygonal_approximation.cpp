/**
 * @file entities/curves/algorithms/polygonal_approximation.cpp
 * @brief 曲線の折れ線近似アルゴリズムの実装
 * @author Yayoi Habami
 * @date 2026-04-10
 * @copyright 2026 Yayoi Habami
 */
#include "entities/curves/algorithms/polygonal_approximation.h"

#include <algorithm>
#include <array>
#include <optional>
#include <utility>
#include <vector>

#include "igesio/numerics/tolerance.h"
#include "igesio/entities/curves/algorithms.h"



namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using igesio::Vector3d;

/// @brief 区間 [u_a, u_b] が直線部に完全に含まれるか判定する
/// @param u_a 区間左端のパラメータ値
/// @param u_b 区間右端のパラメータ値
/// @param segments GetLinearSegments()が返す直線部区間のリスト
/// @return いずれかの直線部に完全に含まれればtrue
bool IsIntervalInLinearSegment(
        double u_a, double u_b,
        const std::vector<std::array<double, 2>>& segments) {
    for (const auto& seg : segments) {
        if (seg[0] <= u_a && u_b <= seg[1]) return true;
    }
    return false;
}

/// @brief パラメータ値と、その点における曲線上の評価済み座標
/// @note pointが値を持つ場合、細分中に評価済みのためPolygonDataの構築時に再評価しない.
///       区間先頭・直線部確定・範囲外端点ではnulloptとなり、
///       PolygonDataの構築時に評価する.
struct ParamPoint {
    /// @brief 曲線パラメータ値
    double param;
    /// @brief paramにおける曲線上の点 (範囲内). 未評価の場合はnullopt
    std::optional<Vector3d> point;
};

/// @brief 反復二分法のスタック要素 (区間と、引き継いだ端点評価値)
struct Interval {
    /// @brief 区間左端・右端のパラメータ値
    double ua, ub;
    /// @brief 端点ua, ubにおける評価済みの点. 親から引き継ぎ再評価を避ける.
    ///        未評価の場合はnullopt
    std::optional<Vector3d> pa, pb;
    /// @brief スタック深度
    int depth;
};

/// @brief スタックを用いた反復二分法で区間 [u_a, u_b] を折れ線近似する
///
/// 「辺と弧の中点との点-直線距離 <= eps」を満たすまで区間を再帰的に二分する.
/// 再帰の代わりに明示的なスタックを用い、左側の区間を優先して処理することで
/// 出力パラメータ列がu_aからu_bへと昇順に並ぶことを保証する.
///
/// 子区間の端点は親が評価済みの点 (外側端点と中点) をそのまま引き継ぐため、
/// 各区間で新規に評価するのは中点のみとなり、端点の再評価を避ける.
///
/// @param curve 対象の曲線
/// @param u_a 区間左端のパラメータ値
/// @param u_b 区間右端のパラメータ値
/// @param linear_segments GetLinearSegments()の結果
/// @param eps 許容距離
/// @param max_depth 最大スタック深度
/// @return u_aからu_bまでのパラメータ列 (両端を含む). 各要素は評価済みの点を
///         保持しうる (未評価はnullopt)
std::vector<ParamPoint> SubdivideWithParams(
        const i_ent::ICurve& curve,
        double u_a, double u_b,
        const std::vector<std::array<double, 2>>& linear_segments,
        double eps, int max_depth) {
    std::vector<ParamPoint> confirmed = {{u_a, std::nullopt}};
    // スタック: 右サブ区間を先に積む (LIFOなので左が先に処理される)
    std::vector<Interval> stack = {{u_a, u_b, std::nullopt, std::nullopt, 0}};

    while (!stack.empty()) {
        Interval iv = stack.back();
        stack.pop_back();

        // 終了条件1: 直線部に完全に含まれる
        if (IsIntervalInLinearSegment(iv.ua, iv.ub, linear_segments)) {
            confirmed.push_back({iv.ub, iv.pb});
            continue;
        }

        // 端点・中点を評価する (端点は親から引き継いだ値があれば再利用)
        const double u_mid = 0.5 * (iv.ua + iv.ub);
        if (!iv.pa) iv.pa = curve.TryGetPointAt(iv.ua);
        if (!iv.pb) iv.pb = curve.TryGetPointAt(iv.ub);
        const auto pm = curve.TryGetPointAt(u_mid);
        if (!iv.pa || !iv.pb || !pm) {
            // 計算失敗時は分割を打ち切る
            confirmed.push_back({iv.ub, iv.pb});
            continue;
        }
        const double dist = i_ent::PointLineDistance(*pm, *iv.pa, *iv.pb);

        // 終了条件2: 中点距離が許容値以下
        if (dist <= eps) {
            confirmed.push_back({iv.ub, iv.pb});
            continue;
        }

        // 終了条件3: 最大深度超過
        if (iv.depth >= max_depth) {
            confirmed.push_back({u_mid, pm});
            confirmed.push_back({iv.ub, iv.pb});
            continue;
        }

        // 右サブ区間を先に積む (端点の評価値を子へ引き継ぐ)
        stack.push_back({u_mid, iv.ub, pm, iv.pb, iv.depth + 1});
        stack.push_back({iv.ua, u_mid, iv.pa, pm, iv.depth + 1});
    }

    return confirmed;
}

}  // namespace



i_num::PolygonData i_ent::ComputeApproximatePolygon(
        const ICurve& curve,
        const i_num::PolygonData& inscribed,
        const i_num::PolygonData& circumscribed,
        double eps,
        int max_depth) {
    auto [t_min, t_max] = curve.GetParameterRange();
    auto linear_segments = curve.GetLinearSegments();

    // 固定点パラメータの収集・ソート・重複除去
    // inscribed/circumscribedのon_curve=trueの頂点とt_min/t_maxを固定点とする
    std::vector<double> fixed_params;
    for (const auto* pd : {&inscribed, &circumscribed}) {
        for (int i = 0; i < pd->Count(); ++i) {
            if (pd->on_curve[static_cast<size_t>(i)]) {
                fixed_params.push_back(
                    pd->curve_params[static_cast<size_t>(i)]);
            }
        }
    }
    fixed_params.push_back(t_min);
    fixed_params.push_back(t_max);

    std::sort(fixed_params.begin(), fixed_params.end());
    fixed_params.erase(
        std::unique(fixed_params.begin(), fixed_params.end()),
        fixed_params.end());

    // 各固定点間の区間を折れ線近似
    std::vector<ParamPoint> result;
    for (size_t idx = 0; idx + 1 < fixed_params.size(); ++idx) {
        auto interval = SubdivideWithParams(
            curve, fixed_params[idx], fixed_params[idx + 1],
            linear_segments, eps, max_depth);

        if (idx == 0) {
            result.insert(result.end(), interval.begin(), interval.end());
        } else {
            // u_aは前区間の末尾と重複するため除く
            result.insert(result.end(), interval.begin() + 1, interval.end());
        }
    }

    // 閉曲線の場合、末尾のt_max (= t_min と同一点) を除去する
    if (curve.IsClosed() && result.size() > 1 &&
            i_num::IsApproxEqual(result.back().param, t_max)) {
        result.pop_back();
    }

    // PolygonDataの構築
    // 細分中に評価済みの点はそのまま再利用し、
    // 未評価の点 (区間先頭・直線部確定・範囲外端点) のみGetPointAtで評価する
    // 固定点 (inscribed/circumscribedの曲率パラメータ等) は
    // 閉曲線の周期正規化などにより [t_min, t_max] を僅かに外れる
    // ことがあるため、評価時は範囲へクランプしてGetPointAtの範囲外例外を避ける
    // (C層: 数値誤差でも例外を投げない)。
    std::vector<Vector3d> vertices;
    std::vector<double> result_params;
    vertices.reserve(result.size());
    result_params.reserve(result.size());
    for (const auto& pp : result) {
        result_params.push_back(pp.param);
        if (pp.point) {
            vertices.push_back(*pp.point);
        } else {
            vertices.push_back(
                curve.GetPointAt(std::clamp(pp.param, t_min, t_max)));
        }
    }
    const std::vector<bool> on_curve(result_params.size(), true);

    return {vertices, on_curve, result_params};
}
