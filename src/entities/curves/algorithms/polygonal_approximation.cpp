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
#include <tuple>
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

/// @brief スタックを用いた反復二分法で区間 [u_a, u_b] を折れ線近似する
///
/// 「辺と弧の中点との点-直線距離 <= eps」を満たすまで区間を再帰的に二分する.
/// 再帰の代わりに明示的なスタックを用い、左側の区間を優先して処理することで
/// 出力パラメータ列がu_aからu_bへと昇順に並ぶことを保証する.
///
/// @param curve 対象の曲線
/// @param u_a 区間左端のパラメータ値
/// @param u_b 区間右端のパラメータ値
/// @param linear_segments GetLinearSegments()の結果
/// @param eps 許容距離
/// @param max_depth 最大スタック深度
/// @return u_aからu_bまでのパラメータ列 (両端を含む)
std::vector<double> SubdivideWithParams(
        const std::shared_ptr<const i_ent::ICurve>& curve,
        double u_a, double u_b,
        const std::vector<std::array<double, 2>>& linear_segments,
        double eps, int max_depth) {
    std::vector<double> confirmed = {u_a};
    // スタック: (u_left, u_right, depth)
    // 右サブ区間を先に積む (LIFOなので左が先に処理される)
    std::vector<std::tuple<double, double, int>> stack = {{u_a, u_b, 0}};

    while (!stack.empty()) {
        auto [ua, ub, depth] = stack.back();
        stack.pop_back();

        // 終了条件1: 直線部に完全に含まれる
        if (IsIntervalInLinearSegment(ua, ub, linear_segments)) {
            confirmed.push_back(ub);
            continue;
        }

        // 終了条件2: 中点距離が許容値以下
        double u_mid = 0.5 * (ua + ub);
        auto pa_opt = curve->TryGetPointAt(ua);
        auto pb_opt = curve->TryGetPointAt(ub);
        auto pm_opt = curve->TryGetPointAt(u_mid);
        if (!pa_opt || !pb_opt || !pm_opt) {
            // 計算失敗時は分割を打ち切る
            confirmed.push_back(ub);
            continue;
        }
        double dist = i_ent::PointLineDistance(*pm_opt, *pa_opt, *pb_opt);

        if (dist <= eps) {
            confirmed.push_back(ub);
            continue;
        }

        // 終了条件3: 最大深度超過
        if (depth >= max_depth) {
            confirmed.push_back(u_mid);
            confirmed.push_back(ub);
            continue;
        }

        // 右サブ区間を先に積む
        stack.push_back({u_mid, ub, depth + 1});
        stack.push_back({ua, u_mid, depth + 1});
    }

    return confirmed;
}

}  // namespace



i_num::PolygonData i_ent::ComputeApproximatePolygon(
        const std::shared_ptr<const ICurve>& curve,
        const i_num::PolygonData& inscribed,
        const i_num::PolygonData& circumscribed,
        double eps,
        int max_depth) {
    auto [t_min, t_max] = curve->GetParameterRange();
    auto linear_segments = curve->GetLinearSegments();

    // Step 1: 固定点パラメータの収集・ソート・重複除去
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

    // Step 2: 各固定点間の区間を折れ線近似
    std::vector<double> result_params;
    for (size_t idx = 0; idx + 1 < fixed_params.size(); ++idx) {
        auto interval = SubdivideWithParams(
            curve, fixed_params[idx], fixed_params[idx + 1],
            linear_segments, eps, max_depth);

        if (idx == 0) {
            result_params.insert(result_params.end(),
                interval.begin(), interval.end());
        } else {
            // u_aは前区間の末尾と重複するため除く
            result_params.insert(result_params.end(),
                interval.begin() + 1, interval.end());
        }
    }

    // 閉曲線の場合、末尾のt_max (= t_min と同一点) を除去する
    if (curve->IsClosed() && result_params.size() > 1 &&
            i_num::IsApproxEqual(result_params.back(), t_max)) {
        result_params.pop_back();
    }

    // Step 3: PolygonDataの構築
    std::vector<Vector3d> vertices;
    vertices.reserve(result_params.size());
    for (double u : result_params) {
        vertices.push_back(curve->GetPointAt(u));
    }
    const std::vector<bool> on_curve(result_params.size(), true);

    return {vertices, on_curve, result_params};
}
