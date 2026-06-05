/**
 * @file examples/sample_curves.cpp
 * @brief This example demonstrates how to create various curve entities
 * @author Yayoi Habami
 * @date 2025-09-14
 * @copyright 2025 Yayoi Habami
 */

#include <math.h>

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <igesio/igesio.h>

namespace i_ent = igesio::entities;

using igesio::kPi;
using igesio::Matrix3Xd;
using igesio::Vector2d;
using igesio::Vector3d;
using ent_vec = std::vector<std::shared_ptr<igesio::EntityBase>>;



/// @brief Example for Circular Arc entity (Type 100)
/// @note 1. Circle: center (-0.75, 0), radius 1
///       2. Arc: center (0, 0), radius 1, start angle 4π/3, end angle 5π/2
ent_vec CreateCircularArc() {
    double x_diff = 1.25;
    auto circle = i_ent::MakeCircle(Vector2d{-x_diff, 0.0}, 1.0);

    auto arc = i_ent::MakeCircularArc(
            Vector2d{0.0, 0.0}, 1.0, 4.0 * kPi / 3.0, 5.0 * kPi / 2.0);
    auto arc_trans = i_ent::MakeTranslation(Vector3d{x_diff, 0.0, 0.0});
    arc->OverwriteTransformationMatrix(arc_trans);

    return {circle, arc_trans, arc};
}

/// @brief Example for Composite Curve entity (Type 102)
/// @note 1. Circular arc: center (0.5, -1), radius 1.5, start (-1, -1), end (2, -1) (CCW)
///       -> CircularArc is defined clockwise, so use transformation matrix to flip and translate
///       2. Line: start (-1, -1), end (1, 1)
///       3. Circular arc: center (-0.5, 1), radius 1.5, start (1, 1), end (-2, 1)
ent_vec CreateCompositeCurve() {
    // 1. circular arc with transformation
    auto rotation = igesio::AngleAxisd(kPi, Vector3d::UnitY());
    auto comp_1_trans = i_ent::MakeTransformationMatrix(
            rotation.toRotationMatrix(), Vector3d{0.5, -1.0, 0.0});
    auto comp_1 = i_ent::MakeCircularArc(
            Vector2d{0.0, 0.0}, Vector2d{-1.5, 0.0}, Vector2d{1.5, 0.0});
    comp_1->OverwriteTransformationMatrix(comp_1_trans);

    // 2. line
    auto comp_2 = i_ent::MakeLine(Vector3d{-1.0, -1.0, 0.0},
                                  Vector3d{1.0, 1.0, 0.0});

    // 3. circular arc
    auto comp_3 = i_ent::MakeCircularArc(
            Vector2d{-0.5, 1.0}, Vector2d{1.0, 1.0}, Vector2d{-2.0, 1.0});

    // Composite curve
    auto comp_curve = i_ent::MakeCompositeCurve({comp_1, comp_2, comp_3});

    return {comp_1_trans, comp_1, comp_2, comp_3, comp_curve};
}

/// @brief Example for Conic Arc entity (Type 104)
/// @note 1. Ellipse arc: center (0, 3), axis (x, y) = (3, 2),
///          start angle 7π/4, end angle 17π/6
ent_vec CreateConicArc() {
    // 1. ellipse arc
    auto ellipse_arc = i_ent::MakeEllipticArc(
            -3.0, 2.0, 7.0 * kPi / 4.0, 17.0 * kPi / 6.0);

    // Note: Since elliptical arc entities are defined with the origin
    // as their center, use a transformation matrix entity to move the origin.
    auto ellipse_trans = i_ent::MakeTranslation(Vector3d{0.0, 3.0, 0.0});
    ellipse_arc->OverwriteTransformationMatrix(ellipse_trans);

    return {ellipse_trans, ellipse_arc};
}

/// @brief Example for Copious Data entity (Type 106)
/// @note 1. Points: (3,0,1), (2,1,-1), (2,2,0), (0,3,1), (-1,2,0)
///       2. Polyline: (8,0,1), (7,1,-1), (7,2,0), (5,3,1), (4,2,0)
ent_vec CreateCopiousData() {
    // 1. Points
    const std::vector<Vector3d> copious_points{
            {3.0, 0.0, 1.0}, {2.0, 1.0, -1.0}, {2.0, 2.0, 0.0},
            {0.0, 3.0, 1.0}, {-1.0, 2.0, 0.0}};
    auto copious = i_ent::MakeCopiousData(copious_points);

    // 2. Polyline with transformation
    auto copious_trans = i_ent::MakeTranslation(Vector3d{5.0, 0.0, 0.0});
    auto linear_path = i_ent::MakeLinearPath(copious_points);
    linear_path->OverwriteTransformationMatrix(copious_trans);

    return {copious, copious_trans, linear_path};
}

/// @brief Example for Line entity (Type 110)
/// @note 1. Segment: start (0, -1, 0), end (1, 1, 0)
///       2. Semi-infinite line: start (-2, -1, 0), direction (1, 2, 0)
///       3. Infinite line: point (-4, -1, 0), direction (1, 2, 0)
ent_vec CreateLine() {
    // 1. segment
    auto start = Vector3d{0.0, -1.0, 0.0};
    auto direction = Vector3d{1.0, 2.0, 0.0};
    auto line_segment = i_ent::MakeLine(start, start + direction);

    // 2. semi-infinite line
    auto ray_trans = i_ent::MakeTranslation(Vector3d{2.0, 0, 0.0});
    auto ray = i_ent::MakeRay(start, direction);
    ray->OverwriteTransformationMatrix(ray_trans);

    // 3. infinite line
    auto line_trans = i_ent::MakeTranslation(Vector3d{4.0, 0, 0.0});
    auto line = i_ent::MakeUnboundedLine(start, direction);
    line->OverwriteTransformationMatrix(line_trans);

    return {line_segment, ray_trans, ray, line_trans, line};
}

/// @brief Example for Parametric Spline Curve entity (Type 112)
/// @note Creates a spline curve with specified parameters
ent_vec CreateParametricSplineCurve() {
    // Create spline curve (terminate-point values are computed automatically)
    const std::vector<double> breakpoints{0.0, 0.5, 1.0, 2.0, 2.25};
    std::vector<igesio::Matrix34d> coefficients(4);
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
    auto spline_c = i_ent::MakeParametricSplineCurve(
            i_ent::ParametricSplineCurveType::kBSpline, 3,
            breakpoints, coefficients);
    spline_c->OverwriteColor(i_ent::ColorNumber::kBlue);
    return {spline_c};
}

/// @brief Example for Point entity (Type 116)
/// @note Creates a point at (1, 2, 3)
ent_vec CreatePoint() {
    auto point = i_ent::MakePoint(Vector3d{1.0, 2.0, 3.0});
    point->OverwriteColor(i_ent::ColorNumber::kMagenta);
    return {point};
}

/// @brief Example for Rational B-Spline Curve entity (Type 126)
/// @note Creates a NURBS curve with specified parameters
ent_vec CreateRationalBSplineCurve() {
    // Create NURBS curve (4 control points, degree 3)
    Matrix3Xd cps(3, 4);
    cps.col(0) = Vector3d(-4.0, -4.0, 0.0);  // control point P(0)
    cps.col(1) = Vector3d(-1.5,  7.0, 3.5);  // control point P(1)
    cps.col(2) = Vector3d( 4.0, -3.0, 1.0);  // control point P(2)
    cps.col(3) = Vector3d( 4.0,  4.0, 0.0);  // control point P(3)
    // weights (all 1.0) and parameter range ([0,1]) are defaulted
    auto nurbs_c = i_ent::MakeRationalBSplineCurve(
        3,                                          // degree
        cps,
        {0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0});  // knot vector

    return {nurbs_c};
}

/// @brief Example for Curve on a Parametric Surface entity (Type 142)
/// @note Creates NURBS curves on NURBS surface
ent_vec CreateCurveOnParametricSurface() {
    // Create NURBS surface (6x6 control points, degree 3 in U and V)
    // grid[i][j] = P(i,j) = (5+10i, -25+10j, z_ij)
    const std::vector<std::vector<Vector3d>> cp_grid{
        {Vector3d(5., -25., -10.), Vector3d(5., -15., -5.),
         Vector3d(5., -5., 0.), Vector3d(5., 5., 0.),
         Vector3d(5., 15., -5.), Vector3d(5., 25., -10.)},      // P(0,j)
        {Vector3d(15., -25., -8.), Vector3d(15., -15., -4.),
         Vector3d(15., -5., -4.), Vector3d(15., 5., -4.),
         Vector3d(15., 15., -4.), Vector3d(15., 25., -8.)},     // P(1,j)
        {Vector3d(25., -25., -5.), Vector3d(25., -15., -3.),
         Vector3d(25., -5., -8.), Vector3d(25., 5., -8.),
         Vector3d(25., 15., -3.), Vector3d(25., 25., -5.)},     // P(2,j)
        {Vector3d(35., -25., -3.), Vector3d(35., -15., -2.),
         Vector3d(35., -5., -8.), Vector3d(35., 5., -8.),
         Vector3d(35., 15., -2.), Vector3d(35., 25., -3.)},     // P(3,j)
        {Vector3d(45., -25., -8.), Vector3d(45., -15., -4.),
         Vector3d(45., -5., -4.), Vector3d(45., 5., -4.),
         Vector3d(45., 15., -4.), Vector3d(45., 25., -8.)},     // P(4,j)
        {Vector3d(55., -25., -10.), Vector3d(55., -15., -5.),
         Vector3d(55., -5., 2.), Vector3d(55., 5., 2.),
         Vector3d(55., 15., -5.), Vector3d(55., 25., -10.)}};   // P(5,j)
    // weights (all 1.0) and parameter range ([0,3]x[0,3]) are defaulted
    auto nurbs_s = i_ent::MakeRationalBSplineSurface(
        {3, 3},                                    // degrees {M1, M2}
        cp_grid,
        {0., 0., 0., 0., 1., 2., 3., 3., 3., 3.},  // knot vector in U
        {0., 0., 0., 0., 1., 2., 3., 3., 3., 3.});  // knot vector in V
    nurbs_s->OverwriteColor(i_ent::ColorNumber::kGreen);

    // Create open curve on a parametric surface
    // (the curve B(t) is defined in the parameter space of the surface)
    Matrix3Xd cps(3, 5);
    cps.col(0) = Vector3d(0.0,  0.0, 0.0);   // control point P(0)
    cps.col(1) = Vector3d(0.0,  4.0, 0.0);   // control point P(1)
    cps.col(2) = Vector3d(2.0, -2.0, 0.0);   // control point P(2)
    cps.col(3) = Vector3d(1.5,  2.0, 0.0);   // control point P(3)
    cps.col(4) = Vector3d(3.0,  3.0, 0.0);   // control point P(4)
    auto nurbs_c = i_ent::MakeRationalBSplineCurve(
        3,                                               // degree
        cps,
        {0.0, 0.0, 0.0, 0.0, 0.5, 1.0, 1.0, 1.0, 1.0});  // knot vector
    auto [open_curve, open_cons] = i_ent::MakeCurveOnAParametricSurface(nurbs_s, nurbs_c);
    open_curve->SetLineWeightNumber(2);
    auto open_cons_bs = std::dynamic_pointer_cast<i_ent::EntityBase>(open_cons);

    // Create closed curve on a parametric surface
    cps.col(0) = Vector3d(1.5, 0.5, 0.0);    // control point P(0)
    cps.col(1) = Vector3d(0.0, 0.5, 0.0);    // control point P(1)
    cps.col(2) = Vector3d(2.0, 4.0, 0.0);    // control point P(2)
    cps.col(3) = Vector3d(3.0, 0.5, 0.0);    // control point P(3)
    cps.col(4) = Vector3d(1.5, 0.5, 0.0);    // control point P(4)
    auto nurbs_cc = i_ent::MakeRationalBSplineCurve(
        3,                                               // degree
        cps,
        {0.0, 0.0, 0.0, 0.0, 0.5, 1.0, 1.0, 1.0, 1.0});  // knot vector
    auto [closed_curve, closed_cons] = i_ent::MakeCurveOnAParametricSurface(nurbs_s, nurbs_cc);
    closed_curve->SetLineWeightNumber(2);
    auto closed_cons_bs = std::dynamic_pointer_cast<i_ent::EntityBase>(closed_cons);

    return {nurbs_s, nurbs_c, open_curve, open_cons_bs, nurbs_cc, closed_curve, closed_cons_bs};
}



/// @brief Main function (creates IGES data and writes to file)
int main() {
    igesio::IgesData iges_data;

    for (const auto& entity : CreateCircularArc()) {
        iges_data.Root().AddEntity(entity);
    }
    for (const auto& entity : CreateCompositeCurve()) {
        iges_data.Root().AddEntity(entity);
    }
    for (const auto& entity : CreateConicArc()) {
        iges_data.Root().AddEntity(entity);
    }
    for (const auto& entity : CreateCopiousData()) {
        iges_data.Root().AddEntity(entity);
    }
    for (const auto& entity : CreateLine()) {
        iges_data.Root().AddEntity(entity);
    }
    for (const auto& entity : CreateParametricSplineCurve()) {
        iges_data.Root().AddEntity(entity);
    }
    for (const auto& entity : CreatePoint()) {
        iges_data.Root().AddEntity(entity);
    }
    for (const auto& entity : CreateRationalBSplineCurve()) {
        iges_data.Root().AddEntity(entity);
    }
    for (const auto& entity : CreateCurveOnParametricSurface()) {
        auto id = iges_data.Root().AddEntity(entity);
    }

    // Write to IGES file
    bool success = false;
    try {
        success = igesio::WriteIges(iges_data, "sample_curves.igs");
    } catch (const igesio::IGESioError& e) {
        std::cerr << "IGES error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
        return 1;
    }

    if (!success) {
        std::cerr << "Failed to write IGES file." << std::endl;
        return 1;
    }

    return 0;
}
