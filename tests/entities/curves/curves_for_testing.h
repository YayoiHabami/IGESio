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

#include <array>
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
#include "igesio/entities/curves/curve_on_a_parametric_surface.h"

#include "igesio/entities/surfaces/rational_b_spline_surface.h"  // for curve on surface


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
    /// @brief 2D曲線かどうか
    bool is_2d = false;
    /// @brief Z=0平面上に存在するかどうか
    bool is_on_xy_plane = false;

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

    /// @brief is_2dとis_on_xy_planeを設定する
    void Set2DInfo(const bool is_2d_in,
                   const bool is_on_xy_plane_in) {
        is_2d = is_2d_in;
        is_on_xy_plane = is_on_xy_plane_in;
    }
};

using curve_vec = std::vector<TestCurve>;



/// @brief Circular Arcエンティティの作成
inline curve_vec CreateCircularArcs() {
    TestCurve circle("R1.5 circle with center(-0.75,0)");
    circle.curve = entities::MakeCircle(Vector2d{-0.75, 0.0}, 1.5);
    circle.Set2DInfo(true, true);

    TestCurve arc("R2 arc with center(1,-1), start angle 4π/3, end angle 5π/2");
    arc.curve = entities::MakeCircularArc(
            Vector2d{1.0, -1.0}, 2.0, 4.0 * kPi / 3.0, 5.0 * kPi / 2.0);
    arc.Set2DInfo(true, true);

    return {circle, arc};
}

/// @brief Composite Curveエンティティの作成
inline curve_vec CreateCompositeCurves() {
    // 1. circular arc
    auto comp_1 = entities::MakeCircularArc(
            Vector2d{0.5, -1.0}, Vector2d{-1.0, -1.0}, Vector2d{2.0, -1.0});

    // 2. line
    auto comp_2 = entities::MakeLine(
            Vector3d{2.0, -1.0, 0.0}, Vector3d{1.0, 1.0, 0.0});

    // 3. polyline
    auto comp_3 = entities::MakeLinearPath(
            std::vector<Vector2d>{{1.0, 1.0}, {1.0, 0.0}, {-2.0, 1.0}}, false);

    // Composite curve
    auto comp_curve = entities::MakeCompositeCurve({comp_1, comp_2, comp_3});

    TestCurve composite_curve("composite curve (arc + line + polyline)", comp_curve, 0);
    composite_curve.Set2DInfo(true, true);

    TestCurve composite_curve_3d("3D composite curve (line + polyline)", 0);
    auto line_3d = entities::MakeLine(
            Vector3d{0.0, 0.0, 0.0}, Vector3d{1.0, 1.0, 1.0});
    auto polyline_3d = entities::MakeLinearPath(
            std::vector<Vector3d>{{1.0, 1.0, 1.0}, {2.0, 0.0, -1.0}, {3.0, 1.0, 0.0}});
    auto composite_curve_3d_ptr =
            entities::MakeCompositeCurve({line_3d, polyline_3d});
    composite_curve_3d.curve = composite_curve_3d_ptr;

    return {composite_curve};
}

/// @brief Conic Arcエンティティの作成
inline curve_vec CreateConicArcs() {
    TestCurve ellipse_arc("ellipse arc with center(0, 0), (rx, ry) = (2, 1), "
                          "angle ∈ [7π/4, 17π/6]");
    ellipse_arc.curve = entities::MakeEllipticArc(
            2.0, 1.0, 7.0 * kPi / 4.0, 17.0 * kPi / 6.0);
    ellipse_arc.Set2DInfo(true, true);

    TestCurve ellipse("full ellipse with rx 2, ry 1");
    ellipse.curve = entities::MakeEllipse(2.0, 1.0);
    ellipse.Set2DInfo(true, true);

    return {ellipse_arc, ellipse};
}

/// @brief Copious Dataエンティティの作成
inline curve_vec CreateCopiousData() {
    TestCurve points("3D copious points (5 points)", -1);
    const std::vector<Vector3d> copious_points{
        {3.0, 0.0, 1.0}, {2.0, 1.0, -1.0}, {2.0, 2.0, 0.0},
        {0.0, 3.0, 1.0}, {-1.0, 2.0, 0.0}};
    points.curve = entities::MakeCopiousData(copious_points);

    TestCurve polyline("3D polyline (5 points)", 0);
    polyline.curve = entities::MakeLinearPath(copious_points);

    TestCurve points_2d("2D copious points (5 points)", -1);
    const std::vector<Vector2d> copious_points_2d{
        {3.0, 0.0}, {2.0, 1.0}, {2.0, 2.0}, {0.0, 3.0}, {-1.0, 2.0}};
    points_2d.curve = entities::MakeCopiousData(copious_points_2d);
    points_2d.Set2DInfo(true, true);

    TestCurve polyline_2d("2D polyline (5 points)", 0);
    polyline_2d.curve = entities::MakeLinearPath(copious_points_2d);
    polyline_2d.Set2DInfo(true, true);

    TestCurve closed_2d_loop("2D closed loop (5 points)", 0);
    closed_2d_loop.curve = entities::MakeLinearPath(copious_points_2d, true);
    closed_2d_loop.Set2DInfo(true, true);

    return {points, polyline, points_2d, polyline_2d, closed_2d_loop};
}

/// @brief Lineエンティティの作成
inline curve_vec CreateLines() {
    using LT = entities::LineType;

    TestCurve segment("segment from (0,-1,0) to (1,1,0)", 1);
    segment.curve = entities::MakeLine(
            Vector3d{0.0, -1.0, 0.0}, Vector3d{1.0, 1.0, 0.0}, LT::kSegment);
    segment.Set2DInfo(true, true);

    TestCurve ray("ray from (0,-1,0) through (1,1,0)", 1);
    ray.curve = entities::MakeLine(
            Vector3d{0.0, -1.0, 0.0}, Vector3d{1.0, 1.0, 0.0}, LT::kRay);
    ray.Set2DInfo(true, true);

    TestCurve line("line through (0,-1,0) and (1,1,0)", 1);
    line.curve = entities::MakeLine(
            Vector3d{0.0, -1.0, 0.0}, Vector3d{1.0, 1.0, 0.0}, LT::kLine);
    line.Set2DInfo(true, true);

    TestCurve segment_3d("3D segment from (0,0,0) to (1,1,1)", 1);
    segment_3d.curve = entities::MakeLine(
            Vector3d{0.0, 0.0, 0.0}, Vector3d{1.0, 1.0, 1.0}, LT::kSegment);

    TestCurve ray_3d("3D ray from (0,0,0) through (1,1,1)", 1);
    ray_3d.curve = entities::MakeLine(
            Vector3d{0.0, 0.0, 0.0}, Vector3d{1.0, 1.0, 1.0}, LT::kRay);

    TestCurve line_3d("3D line through (0,0,0) and (1,1,1)", 1);
    line_3d.curve = entities::MakeLine(
            Vector3d{0.0, 0.0, 0.0}, Vector3d{1.0, 1.0, 1.0}, LT::kLine);

    return {segment, ray, line, segment_3d, ray_3d, line_3d};
}

/// @brief Parametric Spline Curveエンティティの作成
inline curve_vec CreateParametricSplineCurve() {
    TestCurve spline_c("3D parametric spline curve", 3);
    const std::vector<double> breakpoints{0.0, 0.5, 1.0, 2.0, 2.25};
    std::vector<Matrix34d> coefficients(4);
    coefficients[0] <<  1.,     2.,   -5.,    1.,   // x: A(1) ~ D(1)
                        0.,     2.,    3.,   -1.,   // y
                        5.,     0.,    3.,   -2.;   // z
    coefficients[1] <<  0.875, -2.25, -3.5,   2.,
                        1.625,  4.25,  1.5,  -1.,
                        5.5,    1.5,   0.0,   2.;
    coefficients[2] << -0.875, -4.25, -0.5,   1.,
                        4.0,    5.0,   0.0,  -1.,
                        6.5,    3.0,   3.0,  -1.;
    coefficients[3] << -4.625, -2.25,  2.5,   8.,
                        8.0,    2.0,  -3.0,   0.,
                       11.5,    6.0,   0.0,   0.;
    spline_c.curve = entities::MakeParametricSplineCurve(
        entities::ParametricSplineCurveType::kBSpline, 3,
        breakpoints, coefficients);

    TestCurve spline_c_2d("2D parametric spline curve", 3);
    // x, y係数は3D版と同一で、z行をゼロにした平面 (NDIM=2) 版
    auto coefficients_2d = coefficients;
    for (auto& segment : coefficients_2d) {
        segment.row(2).setZero();
    }
    spline_c_2d.curve = entities::MakeParametricSplineCurve(
        entities::ParametricSplineCurveType::kBSpline, 3,
        breakpoints, coefficients_2d, 2);
    spline_c_2d.Set2DInfo(true, true);

    return {spline_c, spline_c_2d};
}

/// @brief Rational B-Spline Curveエンティティの作成
inline curve_vec CreateRationalBSplineCurve() {
    TestCurve nurbs_c_2d("2D rational B-spline curve", 3);
    Matrix3Xd cps(3, 4);
    cps.col(0) = Vector3d(-2.5, -5.5, 0.0);  // control point P(0)
    cps.col(1) = Vector3d(-2.0,  4.0, 0.0);  // control point P(1)
    cps.col(2) = Vector3d(8.5,   2.5, 0.0);  // control point P(2)
    cps.col(3) = Vector3d(5.5,  -2.0, 0.0);  // control point P(3)
    nurbs_c_2d.curve = entities::MakeRationalBSplineCurve(
        3,                                         // degree
        cps,
        {0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0},  // knot vector
        {},                                        // weights (all 1.0)
        std::array<double, 2>{0.0, 1.0});          // parameter range V(0), V(1)
    nurbs_c_2d.Set2DInfo(true, true);

    TestCurve nurbs_closed_2d("2D closed rational B-spline curve", 0);
    cps.col(0) = Vector3d(0.0,   5.0, 0.0);  // control point P(0)
    cps.col(1) = Vector3d(-4.0,  0.0, 0.0);  // control point P(1)
    cps.col(2) = Vector3d(4.0,   0.0, 0.0);  // control point P(2)
    cps.col(3) = Vector3d(0.0,   5.0, 0.0);  // control point P(3)
    nurbs_closed_2d.curve = entities::MakeRationalBSplineCurve(
        3,                                         // degree
        cps,
        {0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0},  // knot vector
        {},                                        // weights (all 1.0)
        std::array<double, 2>{0.0, 1.0});          // parameter range V(0), V(1)
    nurbs_closed_2d.Set2DInfo(true, true);

    TestCurve nurbs_c("3D rational B-spline curve", 3);
    cps.col(0) = Vector3d(-4.0, -4.0, 0.0);  // control point P(0)
    cps.col(1) = Vector3d(-1.5,  7.0, 3.5);  // control point P(1)
    cps.col(2) = Vector3d(4.0,  -3.0, 1.0);  // control point P(2)
    cps.col(3) = Vector3d(4.0,   4.0, 0.0);  // control point P(3)
    nurbs_c.curve = entities::MakeRationalBSplineCurve(
        3,                                         // degree
        cps,
        {0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0},  // knot vector
        {},                                        // weights (all 1.0)
        std::array<double, 2>{0.0, 1.0});          // parameter range V(0), V(1)

    return {nurbs_c_2d, nurbs_closed_2d, nurbs_c};
}

inline curve_vec CreateCurveOnAParametricSurface() {
    // Create NURBS surface (6x6 control points, degree 3 in U and V)
    // grid[i][j] = P(i,j) = (-25+10i, -25+10j, z_ij)
    const std::vector<std::vector<Vector3d>> cp_grid{
        {Vector3d(-25., -25., -10.), Vector3d(-25., -15., -5.),
         Vector3d(-25., -5., 0.), Vector3d(-25., 5., 0.),
         Vector3d(-25., 15., -5.), Vector3d(-25., 25., -10.)},  // P(0,j)
        {Vector3d(-15., -25., -8.), Vector3d(-15., -15., -4.),
         Vector3d(-15., -5., -4.), Vector3d(-15., 5., -4.),
         Vector3d(-15., 15., -4.), Vector3d(-15., 25., -8.)},   // P(1,j)
        {Vector3d(-5., -25., -5.), Vector3d(-5., -15., -3.),
         Vector3d(-5., -5., -8.), Vector3d(-5., 5., -8.),
         Vector3d(-5., 15., -3.), Vector3d(-5., 25., -5.)},     // P(2,j)
        {Vector3d(5., -25., -3.), Vector3d(5., -15., -2.),
         Vector3d(5., -5., -8.), Vector3d(5., 5., -8.),
         Vector3d(5., 15., -2.), Vector3d(5., 25., -3.)},       // P(3,j)
        {Vector3d(15., -25., -8.), Vector3d(15., -15., -4.),
         Vector3d(15., -5., -4.), Vector3d(15., 5., -4.),
         Vector3d(15., 15., -4.), Vector3d(15., 25., -8.)},     // P(4,j)
        {Vector3d(25., -25., -10.), Vector3d(25., -15., -5.),
         Vector3d(25., -5., 2.), Vector3d(25., 5., 2.),
         Vector3d(25., 15., -5.), Vector3d(25., 25., -10.)}};   // P(5,j)
    // weights (all 1.0) and parameter range ([0,3]x[0,3]) are defaulted
    auto nurbs_s = entities::MakeRationalBSplineSurface(
        {3, 3},                                    // degrees {M1, M2}
        cp_grid,
        {0., 0., 0., 0., 1., 2., 3., 3., 3., 3.},  // knot vector in U
        {0., 0., 0., 0., 1., 2., 3., 3., 3., 3.});  // knot vector in V

    // Create curve on the surface
    TestCurve curve_on_surface("open curve on a parametric surface", 2);
    Matrix3Xd cps(3, 5);
    cps.col(0) = Vector3d(0.0,  0.0, 0.0);   // control point P(0)
    cps.col(1) = Vector3d(0.0,  4.0, 0.0);   // control point P(1)
    cps.col(2) = Vector3d(2.0, -2.0, 0.0);   // control point P(2)
    cps.col(3) = Vector3d(1.5,  2.0, 0.0);   // control point P(3)
    cps.col(4) = Vector3d(3.0,  3.0, 0.0);   // control point P(4)
    auto nurbs_c = entities::MakeRationalBSplineCurve(
        3,                                              // degree
        cps,
        {0.0, 0.0, 0.0, 0.0, 0.5, 1.0, 1.0, 1.0, 1.0},  // knot vector
        {},                                             // weights (all 1.0)
        std::array<double, 2>{0.0, 1.0});               // parameter range V(0), V(1)
    auto [open_curve, _] = entities::MakeCurveOnAParametricSurface(nurbs_s, nurbs_c);
    curve_on_surface.curve = open_curve;

    TestCurve closed_curve_on_surface("closed curve on a parametric surface", 2);
    cps.col(0) = Vector3d(1.5, 0.5, 0.0);    // control point P(0)
    cps.col(1) = Vector3d(0.0, 0.5, 0.0);    // control point P(1)
    cps.col(2) = Vector3d(2.0, 4.0, 0.0);    // control point P(2)
    cps.col(3) = Vector3d(3.0, 0.5, 0.0);    // control point P(3)
    cps.col(4) = Vector3d(1.5, 0.5, 0.0);    // control point P(4)
    auto nurbs_cc = entities::MakeRationalBSplineCurve(
        3,                                              // degree
        cps,
        {0.0, 0.0, 0.0, 0.0, 0.5, 1.0, 1.0, 1.0, 1.0},  // knot vector
        {},                                             // weights (all 1.0)
        std::array<double, 2>{0.0, 1.0});               // parameter range V(0), V(1)
    auto [closed_curve, __] = entities::MakeCurveOnAParametricSurface(nurbs_s, nurbs_cc);
    closed_curve_on_surface.curve = closed_curve;

    return {curve_on_surface, closed_curve_on_surface};
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

    auto curves_on_parametric_surface = CreateCurveOnAParametricSurface();
    all_curves.insert(all_curves.end(),
                      curves_on_parametric_surface.begin(),
                      curves_on_parametric_surface.end());

    return all_curves;
}

}  // namespace igesio::tests

#endif  // IGESIO_TESTS_ENTITIES_CURVES_CURVES_FOR_TESTING_H_
