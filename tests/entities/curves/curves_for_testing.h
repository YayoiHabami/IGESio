/**
 * @file tests/entities/curves/curves_for_testing.h
 * @brief entities/curves配下のテストで使用する曲線エンティティの定義
 * @author Yayoi Habami
 * @date 2025-10-24
 * @copyright 2025 Yayoi Habami
 * @note 変換行列は使用しない. 適用する場合は別途エンティティを定義して上書きすること
 */
#ifndef IGESIO_TESTS_ENTITIES_CURVES_CURVES_FOR_TESTING_H_
#define IGESIO_TESTS_ENTITIES_CURVES_CURVES_FOR_TESTING_H_

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "igesio/entities/curves/circular_arc.h"
#include "igesio/entities/curves/composite_curve.h"
#include "igesio/entities/curves/conic_arc.h"
#include "igesio/entities/curves/copious_data.h"
#include "igesio/entities/curves/linear_path.h"
#include "igesio/entities/curves/line.h"
#include "igesio/entities/curves/parametric_spline_curve.h"
#include "igesio/entities/curves/point.h"
#include "igesio/entities/curves/rational_b_spline_curve.h"


namespace igesio::tests {

struct TestCurve {
    /// @brief 曲線エンティティの名前
    std::string name;
    /// @brief 曲線エンティティの共有ポインタ
    std::shared_ptr<entities::ICurve> curve;
    /// @brief 何階まで連続か
    /// @note 例えばC2連続なら2、C1連続なら1、C0連続なら0、連続でないなら-1.
    ///       C∞連続の場合はstd::numeric_limits<int>::max()を使用すること
    int continuity_order;

    /// @brief デフォルトコンストラクタ
    TestCurve() = default;
    /// @brief コンストラクタ
    TestCurve(const std::string& n,
              const std::shared_ptr<entities::ICurve>& c,
              const int order)
            : name(n), curve(c), continuity_order(order) {}
    /// @brief コンストラクタ (nameとorderのみ)
    explicit TestCurve(const std::string& n,
                       const int order = std::numeric_limits<int>::max())
            : name(n), curve(nullptr), continuity_order(order) {}
};

using curve_vec = std::vector<TestCurve>;



/// @brief Circular Arcエンティティの作成
inline curve_vec CreateCircularArcs() {
    TestCurve circle("R1.5 circle with center(-0.75,0)");
    circle.curve = std::make_shared<entities::CircularArc>(
            Vector2d{-0.75, 0.0}, 1.5);

    TestCurve arc("R2 arc with center(1,-1), start angle 4π/3, end angle 5π/2");
    auto center = Vector2d{1.0, -1.0};
    auto arc_start = Vector2d{cos(4.0 * kPi / 3.0), sin(4.0 * kPi / 3.0)};
    auto arc_end   = Vector2d{cos(5.0 * kPi / 2.0), sin(5.0 * kPi / 2.0)};
    arc.curve = std::make_shared<entities::CircularArc>(
            center, 2.0 * arc_start + center, 2.0 * arc_end + center);

    return {circle, arc};
}

/// @brief Composite Curveエンティティの作成
inline curve_vec CreateCompositeCurves() {
    // 1. circular arc
    auto comp_1 = std::make_shared<entities::CircularArc>(
            Vector2d{0.5, -1.0}, Vector2d{-1.0, -1.0}, Vector2d{2.0, -1.0});

    // 2. line
    auto comp_2 = std::make_shared<entities::Line>(
            Vector3d{2.0, -1.0, 0.0}, Vector3d{1.0, 1.0, 0.0});

    // 3. polyline
    auto comp_3 = std::make_shared<entities::LinearPath>(
            std::vector<Vector2d>{{1.0, 1.0}, {1.0, 0.0}, {-2.0, 1.0}}, false);

    // Composite curve
    auto comp_curve = std::make_shared<entities::CompositeCurve>();
    comp_curve->AddCurve(comp_1);
    comp_curve->AddCurve(comp_2);
    comp_curve->AddCurve(comp_3);

    TestCurve composite_curve("composite curve (arc + line + polyline)", comp_curve, 0);

    return {composite_curve};
}

/// @brief Conic Arcエンティティの作成
inline curve_vec CreateConicArcs() {
    TestCurve ellipse_arc("ellipse arc with center(0, 0), (rx, ry) = (2, 1), "
                          "angle ∈ [7π/4, 17π/6]");
    ellipse_arc.curve = std::make_shared<entities::ConicArc>(
            std::pair<double, double>{2.0, 1.0}, 7.0 * kPi / 4.0, 17.0 * kPi / 6.0);

    TestCurve ellipse("full ellipse with rx 2, ry 1");
    ellipse.curve = std::make_shared<entities::ConicArc>(
            std::pair<double, double>{2.0, 1.0}, 0.0, 2.0 * kPi);

    return {ellipse_arc, ellipse};
}

/// @brief Copious Dataエンティティの作成
inline curve_vec CreateCopiousData() {
    TestCurve points("3D copious points (5 points)", -1);
    igesio::Matrix3Xd copious_coords(3, 5);
    copious_coords << 3.0,  2.0, 2.0, 0.0, -1.0,
                      0.0,  1.0, 2.0, 3.0,  2.0,
                      1.0, -1.0, 0.0, 1.0,  0.0;
    points.curve = std::make_shared<entities::CopiousData>(
        entities::CopiousDataType::kPoints3D, copious_coords);

    TestCurve polyline("3D polyline (5 points)", 0);
    polyline.curve = std::make_shared<entities::LinearPath>(
        entities::CopiousDataType::kPolyline3D, copious_coords);

    return {points, polyline};
}

/// @brief Lineエンティティの作成
inline curve_vec CreateLines() {
    using LT = entities::LineType;

    TestCurve segment("segment from (0,-1,0) to (1,1,0)", 1);
    segment.curve = std::make_shared<entities::Line>(
            Vector3d{0.0, -1.0, 0.0}, Vector3d{1.0, 1.0, 0.0}, LT::kSegment);

    TestCurve ray("ray from (0,-1,0) through (1,1,0)", 1);
    ray.curve = std::make_shared<entities::Line>(
            Vector3d{0.0, -1.0, 0.0}, Vector3d{1.0, 1.0, 0.0}, LT::kRay);

    TestCurve line("line through (0,-1,0) and (1,1,0)", 1);
    line.curve = std::make_shared<entities::Line>(
            Vector3d{0.0, -1.0, 0.0}, Vector3d{1.0, 1.0, 0.0}, LT::kLine);

    return {segment, ray, line};
}

/// @brief Parametric Spline Curveエンティティの作成
inline curve_vec CreateParametricSplineCurve() {
    TestCurve spline_c("3D parametric spline curve", 3);
    auto param = igesio::IGESParameterVector{
        6,     // CTYPE: B-Spline
        3, 3,  // degree, NDIM (3D)
        4,     // number of segments
        0., .5, 1., 2., 2.25,  // Break Points T(1), ..., T(5)
         1.,     2.,   -5.,    1.,  // Ax(1) ~ Dx(1)
         0.,     2.,    3.,   -1.,  // Ay(1) ~ Dy(1)
         5.,     0.,    3.,   -2.,  // Az(1) ~ Dz(1)
         0.875, -2.25, -3.5,   2.,  // Ax(2) ~ Dx(2)
         1.625,  4.25,  1.5,  -1.,  // Ay(2) ~ Dy(2)
         5.5,    1.5,   0.0,   2.,  // Az(2) ~ Dz(2)
        -0.875, -4.25, -0.5,   1.,  // Ax(3) ~ Dx(3)
         4.0,    5.0,   0.0,  -1.,  // Ay(3) ~ Dy(3)
         6.5,    3.0,   3.0,  -1.,  // Az(3) ~ Dz(3)
        -4.625, -2.25,  2.5,   8.,  // Ax(4) ~ Dx(4)
         8.0,    2.0,  -3.0,   0.,  // Ay(4) ~ Dy(4)
        11.5,    6.0,   0.0,   0.,  // Az(4) ~ Dz(4),
        -4.90625, 0.5, 17.,  48.,   // TPX0 ~ TPX3
         8.3125,  0.5, -6.,   0.,   // TPY0 ~ TPY3
        13.0,     6.0,  0.,   0.    // TPZ0 ~ TPZ3
    };
    spline_c.curve = std::make_shared<entities::ParametricSplineCurve>(param);
    return {spline_c};
}

/// @brief Rational B-Spline Curveエンティティの作成
inline curve_vec CreateRationalBSplineCurve() {
    TestCurve nurbs_c("3D rational B-spline curve", 3);
    auto param = igesio::IGESParameterVector{
        3,  // number of control points - 1
        3,  // degree
        false, false, false, false,  // non-periodic open NURBS curve
        0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0,  // knot vector
        1.0, 1.0, 1.0, 1.0,  // weights
        -4.0, -4.0,  0.0,    // control point P(0)
        -1.5,  7.0,  3.5,    // control point P(1)
         4.0, -3.0,  1.0,    // control point P(2)
         4.0,  4.0,  0.0,    // control point P(3)
        0.0, 1.0,            // parameter range V(0), V(1)
        0.0, 0.0, 1.0        // normal vector of the defining plane
    };
    nurbs_c.curve = std::make_shared<entities::RationalBSplineCurve>(param);

    return {nurbs_c};
}

/// @brief すべてのテスト用曲線エンティティを作成
inline curve_vec CreateAllTestCurves() {
    curve_vec all_curves;

    auto circular_arcs = CreateCircularArcs();
    all_curves.insert(all_curves.end(), circular_arcs.begin(), circular_arcs.end());

    auto composite_curves = CreateCompositeCurves();
    all_curves.insert(all_curves.end(), composite_curves.begin(), composite_curves.end());

    auto conic_arcs = CreateConicArcs();
    all_curves.insert(all_curves.end(), conic_arcs.begin(), conic_arcs.end());

    auto copious_data = CreateCopiousData();
    all_curves.insert(all_curves.end(), copious_data.begin(), copious_data.end());

    auto lines = CreateLines();
    all_curves.insert(all_curves.end(), lines.begin(), lines.end());

    auto parametric_spline_curves = CreateParametricSplineCurve();
    all_curves.insert(all_curves.end(),
                      parametric_spline_curves.begin(), parametric_spline_curves.end());

    auto rational_bspline_curves = CreateRationalBSplineCurve();
    all_curves.insert(all_curves.end(),
                      rational_bspline_curves.begin(), rational_bspline_curves.end());

    return all_curves;
}

}  // namespace igesio::tests

#endif  // IGESIO_TESTS_ENTITIES_CURVES_CURVES_FOR_TESTING_H_
