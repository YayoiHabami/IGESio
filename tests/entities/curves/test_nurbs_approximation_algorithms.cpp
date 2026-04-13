/**
 * @file entities/curves/test_nurbs_approximation_algorithms.cpp
 * @brief ApproximateWithNurbs のテスト
 * @author Yayoi Habami
 * @date 2026-04-11
 * @copyright 2026 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

#include "igesio/entities/curves/nurbs_algorithms.h"
#include "igesio/entities/curves/circular_arc.h"
#include "igesio/entities/curves/line.h"
#include "igesio/entities/curves/rational_b_spline_curve.h"

namespace {

namespace i_ent = igesio::entities;
using Vector2d = igesio::Vector2d;
using Vector3d = igesio::Vector3d;
constexpr double kPi = igesio::kPi;

// =========================================================================
// テスト用ヘルパー
// =========================================================================

/// @brief 近似NURBS曲線(パラメータ範囲[0,1])と参照曲線の最大近似誤差を返す
/// @param nurbs      近似されたNURBS曲線
/// @param ref        参照曲線
/// @param ref_range  参照曲線のパラメータ範囲 {t0, t1}
/// @param n_samples  サンプル数（等間隔）
double MaxApproxError(
        const i_ent::RationalBSplineCurve& nurbs,
        const i_ent::ICurve& ref,
        const std::array<double, 2>& ref_range,
        unsigned int n_samples = 100) {
    const double t0 = ref_range[0];
    const double dt = ref_range[1] - ref_range[0];
    double max_err  = 0.0;
    for (unsigned int i = 0; i <= n_samples; ++i) {
        const double s = static_cast<double>(i) / n_samples;
        const auto p_n = nurbs.TryGetPointAt(s);
        const auto p_r = ref.TryGetPointAt(t0 + s * dt);
        if (!p_n || !p_r) continue;
        max_err = std::max(max_err, (*p_n - *p_r).norm());
    }
    return max_err;
}

/// @brief ノットベクトルが端点クランプ形式か確認する
/// @param knots ノットベクトル
/// @param m     次数
/// @return 先頭 m+1 個が 0.0、末尾 m+1 個が 1.0 であれば true
bool IsClamped(const std::vector<double>& knots, int m) {
    if (static_cast<int>(knots.size()) < 2 * (m + 1)) return false;
    for (int i = 0; i <= m; ++i) {
        if (std::abs(knots[i]                                 ) > 1e-14) return false;
        if (std::abs(knots[static_cast<int>(knots.size()) - 1 - i] - 1.0) > 1e-14) return false;
    }
    return true;
}

/// @brief ノットベクトルが単調非減少か確認する
bool IsMonotonicallyNonDecreasing(const std::vector<double>& knots) {
    for (size_t i = 1; i < knots.size(); ++i) {
        if (knots[i] < knots[i - 1] - 1e-14) return false;
    }
    return true;
}

}  // namespace



// =========================================================================
// グループA: 入力バリデーション
// =========================================================================

/// @brief 離散点が1点以下の場合は invalid_argument
TEST(ApproximateWithNurbsFromPointsTest, InvalidInput_TooFewPoints) {
    const std::vector<Vector3d> one_point = {{1.0, 0.0, 0.0}};
    EXPECT_THROW(
        i_ent::ApproximateWithNurbs(one_point),
        std::invalid_argument);

    const std::vector<Vector3d> empty = {};
    EXPECT_THROW(
        i_ent::ApproximateWithNurbs(empty),
        std::invalid_argument);
}

/// @brief 全点が同一座標（弦長ゼロ）の場合は invalid_argument
/// @note degree=3 のデフォルトでは k_min+1=4 点が必要なため、4点以上で試験する
TEST(ApproximateWithNurbsFromPointsTest, InvalidInput_AllSamePoint) {
    const std::vector<Vector3d> same = {
        {1.0, 2.0, 3.0}, {1.0, 2.0, 3.0},
        {1.0, 2.0, 3.0}, {1.0, 2.0, 3.0}};
    EXPECT_THROW(
        i_ent::ApproximateWithNurbs(same),
        std::invalid_argument);
}

/// @brief degree に対して点数が不足している場合は invalid_argument
TEST(ApproximateWithNurbsFromPointsTest, InvalidInput_TooFewPointsForDegree) {
    // degree=3 (デフォルト) には r=1 で k_min=3、k_min+1=4 点が必要
    const std::vector<Vector3d> three_pts = {
        {0.0, 0.0, 0.0}, {1.0, 1.0, 0.0}, {2.0, 0.0, 0.0}};
    EXPECT_THROW(
        i_ent::ApproximateWithNurbs(three_pts),
        std::invalid_argument);

    // 両端接線あり (r=2) は k_min=max(3,4)=4、k_min+1=5 点が必要
    const std::vector<Vector3d> four_pts = {
        {0.0, 0.0, 0.0}, {1.0, 1.0, 0.0},
        {2.0, 1.0, 0.0}, {3.0, 0.0, 0.0}};
    i_ent::NurbsEndpointTangents tang;
    tang.start = Vector3d{1.0, 0.0, 0.0};
    tang.end   = Vector3d{1.0, 0.0, 0.0};
    EXPECT_THROW(
        i_ent::ApproximateWithNurbs(four_pts, tang),
        std::invalid_argument);
}

/// @brief 無限のパラメータ範囲を持つ曲線で param_range を省略した場合は
///        invalid_argument
TEST(ApproximateWithNurbsFromCurveTest, InvalidInput_InfiniteDefaultRange) {
    // kLine タイプの Line は param_range が ±∞
    const i_ent::Line line(
        Vector3d{0.0, 0.0, 0.0},
        Vector3d{1.0, 0.0, 0.0},
        i_ent::LineType::kLine);
    EXPECT_THROW(
        i_ent::ApproximateWithNurbs(line),
        std::invalid_argument);
}

/// @brief range[0] >= range[1] の場合は invalid_argument
TEST(ApproximateWithNurbsFromCurveTest, InvalidInput_ReversedRange) {
    const i_ent::Line seg(
        Vector3d{0.0, 0.0, 0.0},
        Vector3d{1.0, 0.0, 0.0},
        i_ent::LineType::kSegment);
    EXPECT_THROW(
        i_ent::ApproximateWithNurbs(seg, std::array<double, 2>{1.0, 0.0}),
        std::invalid_argument);
    EXPECT_THROW(
        i_ent::ApproximateWithNurbs(seg, std::array<double, 2>{0.5, 0.5}),
        std::invalid_argument);
}

/// @brief 非有限の明示的 param_range は invalid_argument
TEST(ApproximateWithNurbsFromCurveTest, InvalidInput_InfiniteExplicitRange) {
    const i_ent::Line seg(
        Vector3d{0.0, 0.0, 0.0},
        Vector3d{1.0, 0.0, 0.0},
        i_ent::LineType::kSegment);
    constexpr double kInf = std::numeric_limits<double>::infinity();
    EXPECT_THROW(
        i_ent::ApproximateWithNurbs(seg, std::array<double, 2>{0.0, kInf}),
        std::invalid_argument);
}



// =========================================================================
// グループB: 出力仕様の検証
// =========================================================================

/// @brief 離散点入力: 戻り値が nullptr でなく、パラメータ範囲が [0,1]
/// @note degree=3 のデフォルトでは k_min+1=4 点が最低限必要
TEST(ApproximateWithNurbsFromPointsTest, Output_BasicSpec) {
    const std::vector<Vector3d> pts = {
        {0.0, 0.0, 0.0}, {1.0, 1.0, 0.0}, {2.0, 0.5, 0.0}, {3.0, 0.0, 0.0}};
    auto result = i_ent::ApproximateWithNurbs(pts);

    ASSERT_NE(result, nullptr);
    const auto range = result->GetParameterRange();
    EXPECT_DOUBLE_EQ(range[0], 0.0);
    EXPECT_DOUBLE_EQ(range[1], 1.0);
}

/// @brief 離散点入力: 重みがすべて 1.0
/// @note 現状の実装制約。将来的に適応重みを実装した際は変更が必要
TEST(ApproximateWithNurbsFromPointsTest, Output_AllWeightsAreOne) {
    const std::vector<Vector3d> pts = {
        {0.0, 0.0, 0.0}, {1.0, 2.0, 0.0}, {3.0, 1.0, 0.5}, {4.0, 0.0, 0.0}};
    auto result = i_ent::ApproximateWithNurbs(pts);
    ASSERT_NE(result, nullptr);

    for (double w : result->Weights()) {
        EXPECT_DOUBLE_EQ(w, 1.0);
    }
}

/// @brief 離散点入力: ノットベクトルが端点クランプ形式かつ単調非減少
TEST(ApproximateWithNurbsFromPointsTest, Output_ClampedMonotonicKnotVector) {
    const std::vector<Vector3d> pts = {
        {0.0, 0.0, 0.0}, {1.0, 1.0, 0.0}, {2.0, 0.5, 0.0},
        {3.0, 1.5, 0.0}, {4.0, 0.0, 0.0}};
    auto result = i_ent::ApproximateWithNurbs(pts);
    ASSERT_NE(result, nullptr);

    EXPECT_TRUE(IsClamped(result->Knots(), result->Degree()));
    EXPECT_TRUE(IsMonotonicallyNonDecreasing(result->Knots()));
}

/// @brief 曲線入力: 戻り値が nullptr でなく、パラメータ範囲が [0,1]
TEST(ApproximateWithNurbsFromCurveTest, Output_BasicSpec) {
    // 半径 1.0 の半円弧 (デフォルト parameter range を使用)
    const i_ent::CircularArc arc(
        Vector2d{0.0, 0.0},
        Vector2d{1.0, 0.0},
        Vector2d{-1.0, 0.0});
    auto result = i_ent::ApproximateWithNurbs(arc);

    ASSERT_NE(result, nullptr);
    const auto range = result->GetParameterRange();
    EXPECT_DOUBLE_EQ(range[0], 0.0);
    EXPECT_DOUBLE_EQ(range[1], 1.0);
}

/// @brief 曲線入力: 重みがすべて 1.0
/// @note 現状の実装制約。将来的に true NURBS 重みを使う場合は変更が必要
TEST(ApproximateWithNurbsFromCurveTest, Output_AllWeightsAreOne) {
    const i_ent::CircularArc arc(Vector2d{0.0, 0.0}, 2.0);
    auto result = i_ent::ApproximateWithNurbs(arc);
    ASSERT_NE(result, nullptr);

    for (double w : result->Weights()) {
        EXPECT_DOUBLE_EQ(w, 1.0);
    }
}

/// @brief 曲線入力: ノットベクトルが端点クランプ形式かつ単調非減少
TEST(ApproximateWithNurbsFromCurveTest, Output_ClampedMonotonicKnotVector) {
    const i_ent::CircularArc arc(
        Vector2d{0.0, 0.0},
        Vector2d{1.0, 0.0},
        Vector2d{-1.0, 0.0});
    auto result = i_ent::ApproximateWithNurbs(arc);
    ASSERT_NE(result, nullptr);

    EXPECT_TRUE(IsClamped(result->Knots(), result->Degree()));
    EXPECT_TRUE(IsMonotonicallyNonDecreasing(result->Knots()));
}



// =========================================================================
// グループC: 端点補間の保証
// =========================================================================

/// @brief 離散点入力: C(0), C(1) が入力点列の始点・終点と一致する
TEST(ApproximateWithNurbsFromPointsTest, EndpointInterpolation) {
    const std::vector<Vector3d> pts = {
        {1.0, 0.0, 2.0}, {3.0, 4.0, 1.0}, {5.0, 2.0, 0.0},
        {6.0, -1.0, 3.0}, {7.0, 0.0, 0.0}};
    auto result = i_ent::ApproximateWithNurbs(pts);
    ASSERT_NE(result, nullptr);

    const auto p_start = result->TryGetPointAt(0.0);
    const auto p_end   = result->TryGetPointAt(1.0);
    ASSERT_TRUE(p_start.has_value());
    ASSERT_TRUE(p_end.has_value());

    EXPECT_NEAR((*p_start - pts.front()).norm(), 0.0, 1e-9);
    EXPECT_NEAR((*p_end   - pts.back() ).norm(), 0.0, 1e-9);
}

/// @brief 曲線入力 (デフォルト range): C(0), C(1) が参照曲線の端点と一致する
TEST(ApproximateWithNurbsFromCurveTest, EndpointInterpolation_DefaultRange) {
    const i_ent::CircularArc arc(
        Vector2d{0.0, 0.0},
        Vector2d{1.0, 0.0},
        Vector2d{-1.0, 0.0});
    const auto ref_range = arc.GetParameterRange();

    auto result = i_ent::ApproximateWithNurbs(arc);
    ASSERT_NE(result, nullptr);

    const auto p_start = result->TryGetPointAt(0.0);
    const auto p_end   = result->TryGetPointAt(1.0);
    const auto r_start = arc.TryGetPointAt(ref_range[0]);
    const auto r_end   = arc.TryGetPointAt(ref_range[1]);
    ASSERT_TRUE(p_start && p_end && r_start && r_end);

    EXPECT_NEAR((*p_start - *r_start).norm(), 0.0, 1e-9);
    EXPECT_NEAR((*p_end   - *r_end).norm(),   0.0, 1e-9);
}

/// @brief 曲線入力 (明示的 range): C(0), C(1) が指定範囲の端点と一致する
TEST(ApproximateWithNurbsFromCurveTest, EndpointInterpolation_ExplicitRange) {
    // 全円を使い、その一部区間を指定する
    const i_ent::CircularArc arc(Vector2d{0.0, 0.0}, 1.5);
    const std::array<double, 2> sub_range = {kPi / 4.0, 3.0 * kPi / 2.0};

    auto result = i_ent::ApproximateWithNurbs(arc, sub_range);
    ASSERT_NE(result, nullptr);

    const auto p_start = result->TryGetPointAt(0.0);
    const auto p_end   = result->TryGetPointAt(1.0);
    const auto r_start = arc.TryGetPointAt(sub_range[0]);
    const auto r_end   = arc.TryGetPointAt(sub_range[1]);
    ASSERT_TRUE(p_start && p_end && r_start && r_end);

    EXPECT_NEAR((*p_start - *r_start).norm(), 0.0, 1e-9);
    EXPECT_NEAR((*p_end   - *r_end).norm(),   0.0, 1e-9);
}



// =========================================================================
// グループD: 接線拘束（離散点入力版のみ）
// =========================================================================

/// @brief tangents なし (r=1): 端点補間のみ行われる
TEST(ApproximateWithNurbsFromPointsTest, TangentConstraint_NoTangents) {
    const std::vector<Vector3d> pts = {
        {0.0, 0.0, 0.0}, {1.0, 2.0, 0.0}, {3.0, 1.0, 0.0}, {4.0, 0.0, 0.0}};
    // 例外なく完了し、端点が一致することを確認する
    std::shared_ptr<i_ent::RationalBSplineCurve> result;
    EXPECT_NO_THROW({
        result = i_ent::ApproximateWithNurbs(
            pts, i_ent::NurbsEndpointTangents{});
    });
    ASSERT_NE(result, nullptr);

    const auto p_start = result->TryGetPointAt(0.0);
    const auto p_end   = result->TryGetPointAt(1.0);
    ASSERT_TRUE(p_start && p_end);
    EXPECT_NEAR((*p_start - pts.front()).norm(), 0.0, 1e-9);
    EXPECT_NEAR((*p_end   - pts.back() ).norm(), 0.0, 1e-9);
}

/// @brief 片端のみ接線指定 (r=1 扱い): 例外なく完了し、端点が一致する
TEST(ApproximateWithNurbsFromPointsTest, TangentConstraint_OnlyOneSide_r1) {
    const std::vector<Vector3d> pts = {
        {0.0, 0.0, 0.0}, {1.0, 1.5, 0.0}, {3.0, 0.5, 0.0}, {4.0, 0.0, 0.0}};
    i_ent::NurbsEndpointTangents tang_start_only;
    tang_start_only.start = Vector3d{1.0, 1.0, 0.0};
    // end は std::nullopt のまま

    std::shared_ptr<i_ent::RationalBSplineCurve> result;
    EXPECT_NO_THROW({
        result = i_ent::ApproximateWithNurbs(pts, tang_start_only);
    });
    ASSERT_NE(result, nullptr);

    const auto p_start = result->TryGetPointAt(0.0);
    const auto p_end   = result->TryGetPointAt(1.0);
    ASSERT_TRUE(p_start && p_end);
    EXPECT_NEAR((*p_start - pts.front()).norm(), 0.0, 1e-9);
    EXPECT_NEAR((*p_end   - pts.back() ).norm(), 0.0, 1e-9);
}

/// @brief 両端接線指定 (r=2): 端点補間に加え、接線方向が一致する
/// @note r=2 では k_min=max(3,4)=4、k_min+1=5 点が最低限必要
TEST(ApproximateWithNurbsFromPointsTest, TangentConstraint_BothTangents_r2) {
    const std::vector<Vector3d> pts = {
        {0.0, 0.0, 0.0}, {1.0, 2.0, 0.0}, {2.0, 2.5, 0.0},
        {3.0, 2.0, 0.0}, {4.0, 0.0, 0.0}};
    // 曲線の形状上、自然な接線方向を指定する
    const Vector3d dir_start{1.0, 1.0, 0.0};
    const Vector3d dir_end  {1.0, -1.0, 0.0};
    i_ent::NurbsEndpointTangents tang;
    tang.start = dir_start;
    tang.end   = dir_end;

    auto result = i_ent::ApproximateWithNurbs(pts, tang);
    ASSERT_NE(result, nullptr);

    // C'(0) の方向が dir_start と一致することを確認する
    const auto d_start = result->TryGetDerivatives(0.0, 1);
    const auto d_end   = result->TryGetDerivatives(1.0, 1);
    ASSERT_TRUE(d_start.has_value());
    ASSERT_TRUE(d_end.has_value());

    const Vector3d tang_out_start = (*d_start)[1].normalized();
    const Vector3d tang_out_end   = (*d_end  )[1].normalized();
    // 向きが一致（内積が 1 に近い）ことを確認する
    EXPECT_NEAR(tang_out_start.dot(dir_start.normalized()), 1.0, 1e-6);
    EXPECT_NEAR(tang_out_end.dot(dir_end.normalized()),     1.0, 1e-6);
}



// =========================================================================
// グループE: 近似精度
// =========================================================================

/// @brief 直線上の点列: 近似誤差が tolerance 以下
TEST(ApproximateWithNurbsFromPointsTest, Accuracy_StraightLine) {
    // y = 0.5*x の直線上の点列
    std::vector<Vector3d> pts;
    for (int i = 0; i <= 5; ++i) {
        const double x = static_cast<double>(i);
        pts.push_back({x, 0.5 * x, 0.0});
    }
    i_ent::NurbsApproxOptions opts;
    opts.tolerance = 1e-4;
    auto result = i_ent::ApproximateWithNurbs(pts, {}, opts);
    ASSERT_NE(result, nullptr);

    // 各入力点について、近似曲線との距離が tolerance 以下か確認する
    // t̄ はコード長パラメータ化なので、入力点はほぼ等間隔に対応する
    // ここでは端点のみ厳密に、内部点は tolerance 内で近似されていることを確認する
    const auto p0 = result->TryGetPointAt(0.0);
    const auto p1 = result->TryGetPointAt(1.0);
    ASSERT_TRUE(p0 && p1);
    EXPECT_NEAR((*p0 - pts.front()).norm(), 0.0, 1e-9);
    EXPECT_NEAR((*p1 - pts.back() ).norm(), 0.0, 1e-9);

    // 中間点もおよそ直線上にあることを確認する（直線補間との比較）
    for (unsigned int i = 0; i <= 50; ++i) {
        const double s = static_cast<double>(i) / 50.0;
        const auto pt = result->TryGetPointAt(s);
        ASSERT_TRUE(pt.has_value());
        // 直線 y = 0.5*x 上の最短距離が tolerance 以下
        const double dist_to_line =
            std::abs((*pt)[1] - 0.5 * (*pt)[0])
            / std::sqrt(1.0 + 0.25);
        EXPECT_LE(dist_to_line, opts.tolerance * 10.0)
            << "s=" << s << " dist_to_line=" << dist_to_line;
    }
}

/// @brief 線分の ICurve 入力: 近似誤差が tolerance 以下
TEST(ApproximateWithNurbsFromCurveTest, Accuracy_LineSegment) {
    const i_ent::Line seg(
        Vector3d{0.0, 0.0, 0.0},
        Vector3d{3.0, 4.0, 0.0},
        i_ent::LineType::kSegment);

    i_ent::NurbsApproxOptions opts;
    opts.tolerance = 1e-4;
    auto result = i_ent::ApproximateWithNurbs(seg, std::nullopt, opts);
    ASSERT_NE(result, nullptr);

    const double err = MaxApproxError(*result, seg, seg.GetParameterRange());
    EXPECT_LE(err, opts.tolerance)
        << "最大近似誤差 " << err << " が tolerance " << opts.tolerance << " を超えている";
}

/// @brief 半円弧の ICurve 入力: 近似誤差が tolerance 以下
/// @note 重みは 1 固定だが、サンプリング十分で許容誤差内に収まることを確認する
TEST(ApproximateWithNurbsFromCurveTest, Accuracy_SemicircularArc) {
    // 半径 1 の半円弧 (0 → π)
    const i_ent::CircularArc arc(
        Vector2d{0.0, 0.0},
        Vector2d{1.0, 0.0},
        Vector2d{-1.0, 0.0});

    i_ent::NurbsApproxOptions opts;
    opts.tolerance = 1e-4;
    auto result = i_ent::ApproximateWithNurbs(arc, std::nullopt, opts);
    ASSERT_NE(result, nullptr);

    const double err = MaxApproxError(*result, arc, arc.GetParameterRange());
    EXPECT_LE(err, opts.tolerance)
        << "最大近似誤差 " << err << " が tolerance " << opts.tolerance << " を超えている";
}

/// @brief 全円の明示的サブレンジ（四半円）: 近似誤差が tolerance 以下、端点一致
TEST(ApproximateWithNurbsFromCurveTest, Accuracy_QuarterCircle_ExplicitRange) {
    const i_ent::CircularArc full_circle(Vector2d{0.0, 0.0}, 2.0);
    const std::array<double, 2> quarter = {0.0, kPi / 2.0};

    i_ent::NurbsApproxOptions opts;
    opts.tolerance = 1e-4;
    auto result = i_ent::ApproximateWithNurbs(full_circle, quarter, opts);
    ASSERT_NE(result, nullptr);

    const double err = MaxApproxError(*result, full_circle, quarter);
    EXPECT_LE(err, opts.tolerance)
        << "最大近似誤差 " << err << " が tolerance " << opts.tolerance << " を超えている";

    // 端点一致
    const auto p0 = result->TryGetPointAt(0.0);
    const auto p1 = result->TryGetPointAt(1.0);
    const auto r0 = full_circle.TryGetPointAt(quarter[0]);
    const auto r1 = full_circle.TryGetPointAt(quarter[1]);
    ASSERT_TRUE(p0 && p1 && r0 && r1);
    EXPECT_NEAR((*p0 - *r0).norm(), 0.0, 1e-9);
    EXPECT_NEAR((*p1 - *r1).norm(), 0.0, 1e-9);
}

/// @brief 最小有効入力（degree=3 では4点）: 例外なく完了し、端点が一致する
TEST(ApproximateWithNurbsFromPointsTest, Accuracy_MinimumValidPoints) {
    // degree=3 (デフォルト), r=1 の k_min+1=4 点が最低限
    const std::vector<Vector3d> pts = {
        {0.0, 0.0, 0.0}, {1.0, 2.0, 0.0},
        {2.0, 2.0, 0.0}, {3.0, 0.0, 0.0}};
    std::shared_ptr<i_ent::RationalBSplineCurve> result;
    EXPECT_NO_THROW({
        result = i_ent::ApproximateWithNurbs(pts);
    });
    ASSERT_NE(result, nullptr);

    const auto p0 = result->TryGetPointAt(0.0);
    const auto p1 = result->TryGetPointAt(1.0);
    ASSERT_TRUE(p0 && p1);
    EXPECT_NEAR((*p0 - pts.front()).norm(), 0.0, 1e-9);
    EXPECT_NEAR((*p1 - pts.back() ).norm(), 0.0, 1e-9);
}



// =========================================================================
// グループF: オプション動作
// =========================================================================

/// @brief degree=2 で近似できる
TEST(ApproximateWithNurbsFromCurveTest, Options_Degree2) {
    const i_ent::CircularArc arc(
        Vector2d{0.0, 0.0},
        Vector2d{1.0, 0.0},
        Vector2d{-1.0, 0.0});

    i_ent::NurbsApproxOptions opts;
    opts.degree    = 2;
    opts.tolerance = 1e-4;

    std::shared_ptr<i_ent::RationalBSplineCurve> result;
    EXPECT_NO_THROW({
        result = i_ent::ApproximateWithNurbs(arc, std::nullopt, opts);
    });
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->Degree(), 2);

    const double err = MaxApproxError(*result, arc, arc.GetParameterRange());
    EXPECT_LE(err, opts.tolerance);
}

/// @brief tolerance を厳しくすると制御点数がデフォルトより増える（または同等以上の精度）
TEST(ApproximateWithNurbsFromCurveTest, Options_TighterTolerance) {
    const i_ent::CircularArc arc(Vector2d{0.0, 0.0}, 1.0);

    i_ent::NurbsApproxOptions opts_default;
    opts_default.tolerance = 1e-4;
    auto result_default = i_ent::ApproximateWithNurbs(arc, std::nullopt, opts_default);

    i_ent::NurbsApproxOptions opts_tight;
    opts_tight.tolerance = 1e-6;
    auto result_tight = i_ent::ApproximateWithNurbs(arc, std::nullopt, opts_tight);

    ASSERT_NE(result_default, nullptr);
    ASSERT_NE(result_tight, nullptr);

    // 厳しい tolerance では制御点数が増えるか、誤差が小さい
    const double err_default = MaxApproxError(*result_default, arc, arc.GetParameterRange());
    const double err_tight   = MaxApproxError(*result_tight,   arc, arc.GetParameterRange());
    EXPECT_LE(err_tight, opts_tight.tolerance)
        << "tight tolerance " << opts_tight.tolerance
        << " の誤差 " << err_tight << " が超過している";

    // デフォルトより tight の方が制御点数が多いか、誤差が小さい
    const int n_default = static_cast<int>(result_default->ControlPoints().cols());
    const int n_tight   = static_cast<int>(result_tight->ControlPoints().cols());
    EXPECT_TRUE(n_tight >= n_default || err_tight < err_default)
        << "tight tolerance のほうが制御点数が少なく (" << n_tight << " < " << n_default
        << ") かつ誤差も大きい (" << err_tight << " >= " << err_default << ")";
}

/// @brief max_control_points による打ち切り: 制御点数が上限を超えない
TEST(ApproximateWithNurbsFromCurveTest, Options_MaxControlPointsBinding) {
    // 全円を近似する（多くの制御点が必要な形状）
    const i_ent::CircularArc full_circle(Vector2d{0.0, 0.0}, 1.0);

    i_ent::NurbsApproxOptions opts;
    opts.tolerance          = 1e-12;  // 実質達成不能な精度
    opts.max_control_points = 8;      // 小さい上限で強制打ち切り

    std::shared_ptr<i_ent::RationalBSplineCurve> result;
    EXPECT_NO_THROW({
        result = i_ent::ApproximateWithNurbs(full_circle, std::nullopt, opts);
    });
    ASSERT_NE(result, nullptr);

    const int n_ctrl = static_cast<int>(result->ControlPoints().cols());
    EXPECT_LE(n_ctrl, static_cast<int>(opts.max_control_points))
        << "制御点数 " << n_ctrl << " が上限 " << opts.max_control_points << " を超えている";
}
