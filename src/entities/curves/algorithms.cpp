/**
 * @file entities/curves/algorithms.cpp
 * @brief 曲線関連のアルゴリズム定義
 * @author Yayoi Habami
 * @date 2025-10-29
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/curves/algorithms.h"

#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/entities/curves/copious_data_base.h"
#include "igesio/entities/curves/line.h"

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
 * 曲線の折れ線近似
 */

namespace {

/// @brief 再帰的に曲線を分割する
/// @param curve 折れ線近似する曲線オブジェクト
/// @param t_range 分割するパラメータ範囲 (t_start, t_end)
/// @param tol 折れ線近似の許容誤差
/// @param depth 現在の再帰深度
/// @param max_depth 最大再帰深度
/// @return 分割された点列 (C(t_start+1), ..., C(t_end))
std::vector<Vector3d> SubdivideRecursive(
        const std::shared_ptr<const i_ent::ICurve>& curve,
        const std::array<double, 2>& t_range,
        const i_num::Tolerance& tol,
        const unsigned int depth,
        const unsigned int max_depth = 20) {
    // 終了条件の確認
    if (depth > max_depth) {
        return {curve->GetPointAt(t_range[1])};
    }
    auto p1 = curve->GetPointAt(t_range[0]);
    auto p2 = curve->GetPointAt(t_range[1]);

    auto t_mid = 0.5 * (t_range[0] + t_range[1]);
    auto p_mid_opt = curve->TryGetPointAt(t_mid);
    if (!p_mid_opt.has_value()) {
        // 中点の計算に失敗した場合、分割を終了
        return {curve->GetPointAt(t_range[1])};
    }
    const auto& p_mid = *p_mid_opt;

    // 弦と曲線の中点との距離を計算
    auto dist = i_ent::PointLineDistance(p_mid, p1, p2, {false, false});

    if (dist > tol.abs_tol) {
        // 許容誤差を超えている場合、さらに分割
        auto left_points = SubdivideRecursive(
                curve, {t_range[0], t_mid}, tol, depth + 1, max_depth);
        auto right_points = SubdivideRecursive(
                curve, {t_mid, t_range[1]}, tol, depth + 1, max_depth);
        left_points.insert(left_points.end(),
                           right_points.begin(), right_points.end());
        return left_points;
    }

    return {p2};
}


/// @brief CopiousData曲線の折れ線近似を行う
/// @throw std::invalid_argument 引数が不正な場合
std::vector<Vector3d> DiscretizeCopiousDataCurve(
        const std::shared_ptr<const i_ent::CopiousDataBase>& curve) {
    if (curve->GetFormNumber() <= 3) {
        // 点列は折れ線にできない
        throw std::invalid_argument("DiscretizeCurve: Cannot discretize CopiousData"
            " curve with form number <= 3.");
    } else if (curve->GetFormNumber() <= 13) {
        // 開いた折れ線の場合
        std::vector<Vector3d> points;
        auto vertices = curve->Coordinates();
        points.reserve(vertices.cols());
        for (int i = 0; i < vertices.cols(); ++i) {
            points.push_back(vertices.col(i));
        }
        return points;
    } else if (curve->GetDataType() == i_ent::CopiousDataType::kPlanarLoop) {
        // 閉じた折れ線の場合
        std::vector<Vector3d> points;
        auto vertices = curve->Coordinates();
        points.reserve(vertices.cols() + 1);
        for (int i = 0; i < vertices.cols(); ++i) {
            points.push_back(vertices.col(i));
        }
        // 閉ループのため、最初の点を最後に追加
        points.push_back(vertices.col(0));
        return points;
    } else {
        // 未対応
        throw igesio::NotImplementedError(
            "DiscretizeCurve: Discretization for this CopiousData form number is not implemented."
            " Form number: " + std::to_string(curve->GetFormNumber()));
    }
}

}  // namespace

std::vector<Vector3d>
i_ent::DiscretizeCurve(const std::shared_ptr<const i_ent::ICurve>& curve,
                       const i_num::Tolerance& tol,
                       const unsigned int initial_subdivisions) {
    // 入力された引数の妥当性をチェック
    if (!curve) {
        throw std::invalid_argument("DiscretizeCurve: curve is nullptr.");
    } else if (tol.abs_tol < 0.0 || tol.rel_tol < 0.0) {
        throw std::invalid_argument("DiscretizeCurve: tol has negative values."
            " abs_tol: " + std::to_string(tol.abs_tol) +
            ", rel_tol: " + std::to_string(tol.rel_tol));
    } else if (initial_subdivisions < 1) {
        throw std::invalid_argument("DiscretizeCurve: initial_subdivisions must be >= 1."
            " initial_subdivisions: " + std::to_string(initial_subdivisions));
    }

    // 曲線の初期分割数を決定
    auto init_divs = initial_subdivisions;
    if (curve->IsClosed() && init_divs < 3) {
        // 閉曲線の場合、最低でも3分割必要
        init_divs = 3;
    }

    if (curve->GetType() == i_ent::EntityType::kCopiousData) {
        // 点列・折れ線の場合の特殊化
        auto copious_data_curve =
            std::dynamic_pointer_cast<const i_ent::CopiousDataBase>(curve);
        if (copious_data_curve) {
            return DiscretizeCopiousDataCurve(copious_data_curve);
        } else {
            throw std::invalid_argument("DiscretizeCurve: Failed to cast to CopiousDataBase.");
        }
    } else if (curve->GetType() == i_ent::EntityType::kLine) {
        // 直線の場合、両端点のみで折れ線近似
        auto line = std::dynamic_pointer_cast<const i_ent::Line>(curve);
        if (!line) {
            throw std::invalid_argument("DiscretizeCurve: Failed to cast to Line.");
        } else if (line->GetLineType() != i_ent::LineType::kSegment) {
            // 半直線や直線は折れ線近似できない
            throw std::invalid_argument("DiscretizeCurve: Cannot discretize non-segment line.");
        }
        auto [start, end] = line->GetAnchorPoints();
        return {start, end};
    }

    // 一般的な曲線の場合、適応的折れ線近似を実行
    auto [t_min, t_max] = curve->GetParameterRange();
    std::vector<Vector3d> points;
    // 最初の点のみ追加
    points.push_back(curve->GetStartPoint());
    for (int i = 0; i < init_divs; ++i) {
        double t_start = t_min + (t_max - t_min) * i / init_divs;
        double t_end = t_min + (t_max - t_min) * (i + 1) / init_divs;

        // t_rangeの間を適応的に分割
        auto points_i = SubdivideRecursive(curve, {t_start, t_end}, tol, 1);
        points.insert(points.end(), points_i.begin(), points_i.end());
    }
    return points;
}
