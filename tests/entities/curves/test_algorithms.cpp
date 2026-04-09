/**
 * @file entities/test_algorithms.cpp
 * @brief 曲線関連のアルゴリズムのテスト
 * @author Yayoi Habami
 * @date 2025-10-29
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <cstdlib>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "igesio/entities/curves/algorithms.h"
#include "./curves_for_testing.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
namespace i_test = igesio::tests;
using Vector2d = igesio::Vector2d;
using Vector3d = igesio::Vector3d;
using i_ent::ICurve;
constexpr double kPi = igesio::kPi;

/// @brief ランダムな0~1の値を取得する
double rand_0to1() {
    return static_cast<double>(rand()) / RAND_MAX;
}

/// @brief 点と折れ線の（最近傍セグメントにおける）距離を計算する
/// @param point 点
/// @param polyline 折れ線の頂点(l(1), ..., l(n))
/// @return 最短距離
double PointPolylineDistance(const Vector3d& point,
                             const std::vector<Vector3d>& polyline) {
    // 各セグメントについて距離を計算
    const std::pair<bool, bool> is_finite = {true, true};
    double min_dist = std::numeric_limits<double>::infinity();
    for (size_t i = 0; i < polyline.size() - 1; ++i) {
        auto start = polyline[i];
        auto end = polyline[i + 1];
        auto dist = i_ent::PointLineDistance(point, start, end, is_finite);
        min_dist = (dist < min_dist) ? dist : min_dist;
    }
    return min_dist;
}

}  // namespace



/**
 * PointLineDistanceのテスト
 */

// is_finite = {true, true} (線分) のテスト
TEST(PointLineDistanceTest, LineSegment) {
    const Vector3d a1(0.0, 0.0, 0.0);
    const Vector3d a2(10.0, 0.0, 0.0);
    const std::pair<bool, bool> is_finite = {true, true};

    // Case 1: 垂線の足が線分内にある場合
    const Vector3d p1(5.0, 5.0, 0.0);
    EXPECT_DOUBLE_EQ(i_ent::PointLineDistance(p1, a1, a2, is_finite),
                     5.0);

    // Case 2: 垂線の足が線分の外 (a1側) にある場合
    // 点(-5,5,0)から最も近いのは端点a1(0,0,0)
    const Vector3d p2(-5.0, 5.0, 0.0);
    EXPECT_DOUBLE_EQ(i_ent::PointLineDistance(p2, a1, a2, is_finite),
                     std::sqrt(5.0*5.0 + 5.0*5.0));

    // Case 3: 垂線の足が線分の外 (a2側) にある場合
    // 点(15,5,0)から最も近いのは端点a2(10,0,0)
    const Vector3d p3(15.0, 5.0, 0.0);
    EXPECT_DOUBLE_EQ(i_ent::PointLineDistance(p3, a1, a2, is_finite),
                     std::sqrt(5.0*5.0 + 5.0*5.0));

    // Case 4: 点が線分上にある場合
    const Vector3d p4(3.0, 0.0, 0.0);
    EXPECT_DOUBLE_EQ(i_ent::PointLineDistance(p4, a1, a2, is_finite),
                     0.0);

    // Case 5: anchor1とanchor2が同じ点の場合
    const Vector3d a_same(1.0, 1.0, 1.0);
    const Vector3d p5(4.0, 5.0, 6.0);
    EXPECT_DOUBLE_EQ(i_ent::PointLineDistance(p5, a_same, a_same, is_finite),
                     std::sqrt(3.0*3.0 + 4.0*4.0 + 5.0*5.0));

    // Case 6: 垂線の足がanchor1またはanchor2に一致する場合
    const Vector3d p6a(0.0, 5.0, 0.0);  // anchor1に一致
    const Vector3d p6b(10.0, -3.0, 0.0);  // anchor2に一致
    EXPECT_DOUBLE_EQ(i_ent::PointLineDistance(p6a, a1, a2, is_finite),
                     5.0);
    EXPECT_DOUBLE_EQ(i_ent::PointLineDistance(p6b, a1, a2, is_finite),
                     3.0);
}

// is_finite = {false, true} or {true, false} (半直線) のテスト
TEST(PointLineDistanceTest, HalfLine) {
    const Vector3d a1(0.0, 0.0, 0.0);
    const Vector3d a2(10.0, 0.0, 0.0);
    const std::pair<bool, bool> is_finite1 = {false, true};
    const std::pair<bool, bool> is_finite2 = {true, false};

    // Case 1: 垂線の足が半直線内にある場合
    const Vector3d p1(5.0, 5.0, 0.0);
    EXPECT_DOUBLE_EQ(i_ent::PointLineDistance(p1, a1, a2, is_finite1),
                     5.0);
    EXPECT_DOUBLE_EQ(i_ent::PointLineDistance(p1, a1, a2, is_finite2),
                     5.0);

    // Case 2: 垂線の足が半直線の外にある場合
    const Vector3d p2(-5.0, 5.0, 0.0);
    const Vector3d p3(15.0, 5.0, 0.0);
    EXPECT_DOUBLE_EQ(i_ent::PointLineDistance(p3, a1, a2, is_finite1),
                     std::sqrt(5.0*5.0 + 5.0*5.0));
    EXPECT_DOUBLE_EQ(i_ent::PointLineDistance(p2, a1, a2, is_finite2),
                     std::sqrt(5.0*5.0 + 5.0*5.0));

    // Case 3: 垂線の足が直線側にある場合
    EXPECT_DOUBLE_EQ(i_ent::PointLineDistance(p2, a1, a2, is_finite1),
                     5.0);
    EXPECT_DOUBLE_EQ(i_ent::PointLineDistance(p3, a1, a2, is_finite2),
                     5.0);

    // Case 4: 点が直線上にある場合
    const Vector3d p4(3.0, 0.0, 0.0);
    EXPECT_DOUBLE_EQ(i_ent::PointLineDistance(p4, a1, a2, is_finite1),
                     0.0);
    EXPECT_DOUBLE_EQ(i_ent::PointLineDistance(p4, a1, a2, is_finite2),
                     0.0);

    // Case 5: anchor1とanchor2が同じ点の場合 -> anchorとの距離を返す
    const Vector3d a_same(1.0, 1.0, 1.0);
    const Vector3d p5(4.0, 5.0, 6.0);
    EXPECT_DOUBLE_EQ(i_ent::PointLineDistance(p5, a_same, a_same, is_finite1),
                     std::sqrt(3.0*3.0 + 4.0*4.0 + 5.0*5.0));
    EXPECT_DOUBLE_EQ(i_ent::PointLineDistance(p5, a_same, a_same, is_finite2),
                     std::sqrt(3.0*3.0 + 4.0*4.0 + 5.0*5.0));

    // Case 6: 垂線の足がanchor1またはanchor2に一致する場合
    const Vector3d p6a(0.0, 5.0, 0.0);  // anchor1に一致
    const Vector3d p6b(10.0, -3.0, 0.0);  // anchor2に一致
    EXPECT_DOUBLE_EQ(i_ent::PointLineDistance(p6a, a1, a2, is_finite1),
                     5.0);
    EXPECT_DOUBLE_EQ(i_ent::PointLineDistance(p6b, a1, a2, is_finite2),
                     3.0);
}

// is_finite = {false, false} (無限直線) のテスト
TEST(PointLineDistanceTest, InfiniteLine) {
    const Vector3d a1(0.0, 0.0, 0.0);
    const Vector3d a2(10.0, 0.0, 0.0);
    const std::pair<bool, bool> is_finite = {false, false};

    // Case 1: 垂線の足が直線上にある場合
    const Vector3d p1(5.0, 5.0, 0.0);
    EXPECT_DOUBLE_EQ(i_ent::PointLineDistance(p1, a1, a2, is_finite),
                     5.0);

    // Case 2: 点が直線上にある場合
    const Vector3d p2(3.0, 0.0, 0.0);
    EXPECT_DOUBLE_EQ(i_ent::PointLineDistance(p2, a1, a2, is_finite),
                     0.0);

    // Case 3: anchor1とanchor2が同じ点の場合 -> anchorとの距離を返す
    const Vector3d a_same(1.0, 1.0, 1.0);
    const Vector3d p3(4.0, 5.0, 6.0);
    EXPECT_DOUBLE_EQ(i_ent::PointLineDistance(p3, a_same, a_same, is_finite),
                     std::sqrt(3.0*3.0 + 4.0*4.0 + 5.0*5.0));
}



/**
 * DiscretizeCurveのテスト
 */

// エラーケース
// 一般の曲線（C1連続以上）のテスト
// CopiousData, Line以外
TEST(DiscretizeCurveTest, ErrorCases) {
    auto curve = i_test::CreateCircularArcs()[0].curve;
    ASSERT_TRUE(curve != nullptr);

    // nullptrを指定した場合はエラー
    EXPECT_THROW(i_ent::DiscretizeCurve(nullptr),
                 std::invalid_argument);

    // 負のトレランスは不許可
    EXPECT_THROW({i_ent::DiscretizeCurve(nullptr, i_num::Tolerance(-1));},
                 std::invalid_argument);

    // 分割数1未満はNG
    EXPECT_THROW({i_ent::DiscretizeCurve(nullptr, i_num::Tolerance(), 0);},
                 std::invalid_argument);

    // 閉じた曲線について、3以下の分割数は不許可
    ASSERT_TRUE(curve->IsClosed());
    EXPECT_THROW({i_ent::DiscretizeCurve(nullptr, i_num::Tolerance(), 2);},
                 std::invalid_argument);
}

// 一般の曲線（C1連続以上）のテスト
// CopiousData, Line以外
TEST(DiscretizeCurveTest, GeneralCurves) {
    auto curves = i_test::CreateAllTestCurves();

    for (const auto& curve : curves) {
        SCOPED_TRACE("Curve: " + curve.name);
        ASSERT_TRUE(curve.curve != nullptr)
                << "Curve '" << curve.name << "' is nullptr.";
        if (curve.curve->GetType() == i_ent::EntityType::kCopiousData ||
            curve.curve->GetType() == i_ent::EntityType::kLine) {
            // CopiousData, Lineの場合はスキップ
            continue;
        }

        // 折れ線近似の実行
        unsigned int init_div = 10;
        auto tol = i_num::Tolerance();
        auto points = i_ent::DiscretizeCurve(curve.curve, tol, init_div);

        // 点の数がinit_div以上になっていることを確認
        EXPECT_GE(points.size(), init_div) << "The number of vertices must be greater "
            "than initial_subdivisions " << init_div << ", but got " << points.size();

        // 隣り合った頂点が同じ点ではないことを確認
        auto prev_point = points[0];
        for (size_t i = 1; i < points.size(); ++i) {
            EXPECT_TRUE(!i_num::IsApproxEqual(prev_point, points[i])) <<
                "The coordinate of adjacent vertices must be different";
            prev_point = points[i];
        }

        // ランダムな点について、許容誤差を満たしていることを確認
        auto [t_min, t_max] = curve.curve->GetParameterRange();
        t_min = (t_min == -std::numeric_limits<double>::infinity()) ? -1e8 : t_min;
        t_max = (t_max ==  std::numeric_limits<double>::infinity()) ?  1e8 : t_max;
        for (int i = 0; i <= 100; ++i) {
            // C(t_i) を計算 (t_i ∈ [tmin, tmax] はランダムな値)
            auto t_i = rand_0to1() * (t_max - t_min) + t_min;
            auto point_i = curve.curve->TryGetPointAt(t_i);
            ASSERT_TRUE(point_i.has_value()) << "Cannot evaluate C(" << t_i << ").";

            // 折れ線との距離が許容誤差以下であることを確認
            auto dist = PointPolylineDistance(point_i.value(), points);
            EXPECT_LE(dist, tol.abs_tol) << "The distance " << dist << " between "
                    << "polyline and C(" << t_i << ") must be less than tolerance "
                    << tol.abs_tol << ".";
        }
    }
}

// CopiousDataの場合のテスト
TEST(DiscretizeCurveTest, CopiousData) {
    auto curves = i_test::CreateCopiousData();
    unsigned int init_div = 10;
    auto tol = i_num::Tolerance();

    for (const auto& curve : curves) {
        SCOPED_TRACE("Curve: " + curve.name);
        ASSERT_TRUE(curve.curve != nullptr)
                << "Curve '" << curve.name << "' is nullptr.";

        auto copious = std::dynamic_pointer_cast<const i_ent::CopiousDataBase>(curve.curve);
        if (copious->GetFormNumber() <= 3) {
            // 点列の場合 -> 折れ線化不可能
            EXPECT_THROW(i_ent::DiscretizeCurve(curve.curve),
                         std::invalid_argument);
        } else if (copious->GetFormNumber() <= 13) {
            // 折れ線の場合 -> 折れ線化可能
            std::vector<Vector3d> points;
            EXPECT_NO_THROW({
                points = i_ent::DiscretizeCurve(curve.curve);
            });
            // pointsの要素数がcopiousの点数と同じはず
            EXPECT_EQ(copious->GetCount(), points.size());

            // 点列の各要素が一致すること
            for (size_t i = 0; i < points.size(); ++i) {
                EXPECT_TRUE(i_num::IsApproxEqual(points[i], copious->Coordinate(i)));
            }
        } else if (copious->GetDataType() == i_ent::CopiousDataType::kPlanarLoop) {
            // 折れ線の場合 -> 折れ線化可能
            std::vector<Vector3d> points;
            EXPECT_NO_THROW({
                points = i_ent::DiscretizeCurve(curve.curve);
            });
            // pointsの要素数はcopiousの点数+1と同じはず
            EXPECT_EQ(copious->GetCount() + 1, points.size());

            // 点列の各要素が一致すること
            // ただしpointsの末端はcopiousの先頭に一致すること
            for (size_t i = 0; i < points.size(); ++i) {
                auto coord_idx = i % copious->GetCount();
                EXPECT_TRUE(i_num::IsApproxEqual(points[i], copious->Coordinate(coord_idx)));
            }
        }
    }
}

// Lineの場合のテスト
TEST(DiscretizeCurveTest, Line) {
    auto curves = i_test::CreateLines();

    for (const auto& curve : curves) {
        SCOPED_TRACE("Curve: " + curve.name);
        ASSERT_TRUE(curve.curve != nullptr)
                << "Curve '" << curve.name << "' is nullptr.";

        auto line = std::dynamic_pointer_cast<const i_ent::Line>(curve.curve);

        if (line->GetLineType() != i_ent::LineType::kSegment) {
            // 線分以外では折れ線近似不可
            EXPECT_THROW(i_ent::DiscretizeCurve(curve.curve), std::invalid_argument);
            continue;
        }

        // 2点のみ取得されること
        std::vector<Vector3d> points;
        EXPECT_NO_THROW({
            points = i_ent::DiscretizeCurve(curve.curve);
        });
        EXPECT_EQ(points.size(), 2);

        // 始点/終点に一致すること
        auto [start, end] = line->GetAnchorPoints();
        EXPECT_TRUE(i_num::IsApproxEqual(start, points[0]));
        EXPECT_TRUE(i_num::IsApproxEqual(end,   points[1]));
    }
}

