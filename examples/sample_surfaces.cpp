/**
 * @file examples/sample_surfaces.cpp
 * @brief This example demonstrates how to create various surface entities
 * @author Yayoi Habami
 * @date 2025-10-08
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



/// @brief Example for Ruled Surface entity (Type 118)
ent_vec CreateRuledSurface() {
    // curve1: Line
    auto curve1 = i_ent::MakeLine(Vector3d{-5., 0., 0.}, Vector3d{5., 0., 0.});

    // curve2: Rational B-Spline Curve (4 control points, degree 3)
    Matrix3Xd cps(3, 4);
    cps.col(0) = Vector3d(-5.0, 0.0, -6.0);  // control point P(0)
    cps.col(1) = Vector3d(-3.0, 4.0, -6.0);  // control point P(1)
    cps.col(2) = Vector3d( 3.0, 4.0, -6.0);  // control point P(2)
    cps.col(3) = Vector3d( 5.0, 0.0, -6.0);  // control point P(3)
    auto curve2 = i_ent::MakeRationalBSplineCurve(
        3,                                          // degree
        cps,
        {0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0});  // knot vector

    // Ruled surface
    auto ruled_surf = i_ent::MakeRuledSurface(curve1, curve2);
    ruled_surf->OverwriteColor(i_ent::ColorNumber::kGreen);

    return {curve1, curve2, ruled_surf};
}

/// @brief Example for Surface of Revolution entity (Type 120)
ent_vec CreateSurfaceOfRevolution() {
    // Axis of revolution:
    auto axis_line = i_ent::MakeLine(Vector3d{1., 1., 1.}, Vector3d{1., 2., 3.});

    // Generatrix curve (4 control points, degree 3)
    Matrix3Xd cps(3, 4);
    cps.col(0) = Vector3d(1.0, -4.0, 0.0);  // control point P(0)
    cps.col(1) = Vector3d(1.0, -5.0, 1.5);  // control point P(1)
    cps.col(2) = Vector3d(1.0, -3.0, 2.0);  // control point P(2)
    cps.col(3) = Vector3d(1.0,  0.0, 4.0);  // control point P(3)
    auto generatrix = i_ent::MakeRationalBSplineCurve(
        3,                                          // degree
        cps,
        {0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0});  // knot vector

    // Surface of revolution
    auto surf_rev = i_ent::MakeSurfaceOfRevolution(axis_line, generatrix, 0.0, kPi);
    surf_rev->OverwriteColor(i_ent::ColorNumber::kYellow);

    return {axis_line, generatrix, surf_rev};
}

/// @brief Example for Tabulated Cylinder (Type 122)
ent_vec CreateTabulatedCylinder() {
    // Directrix curve (4 control points, degree 2)
    Matrix3Xd cps(3, 4);
    cps.col(0) = Vector3d(0.0, -4.0, -4.0);  // control point P(0)
    cps.col(1) = Vector3d(0.0,  0.2, -1.1);  // control point P(1)
    cps.col(2) = Vector3d(0.0, -1.0,  4.5);  // control point P(2)
    cps.col(3) = Vector3d(0.0,  4.0,  4.0);  // control point P(3)
    auto directrix = i_ent::MakeRationalBSplineCurve(
        2,                                  // degree
        cps,
        {0., 0., 0., 0.5, 1., 1., 1.});     // knot vector

    // Axis direction
    Vector3d axis_dir{1., -1., 0.};
    double axis_length = 3.0;

    // Tabulated cylinder
    auto tab_cyl = i_ent::MakeExtrudedSurface(
            directrix, axis_dir.normalized() * axis_length);
    tab_cyl->OverwriteColor(i_ent::ColorNumber::kCyan);

    return {directrix, tab_cyl};
}

/// @brief Example for Rational B-Spline Surface entity (Type 128)
/// @note Creates a NURBS surface with specified parameters
ent_vec CreateRationalBSplineSurface() {
    // Plane (Y = 5)
    auto nurbs_plane = i_ent::MakeRationalBSplineSurface(
        {1, 1},                                              // degrees {M1, M2}
        {{Vector3d(-5., 5.,  5.), Vector3d(-5., 5., -5.)},   // P(0,j)
         {Vector3d( 5., 5.,  5.), Vector3d( 5., 5., -5.)}},  // P(1,j)
        {0., 0., 1., 1.},                                    // knot vector in U
        {0., 0., 1., 1.});                                   // knot vector in V
    nurbs_plane->OverwriteColor(i_ent::ColorNumber::kYellow);

    // Freeform surface (6x6 control points, degree 3 in U and V)
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
    auto nurbs_freeform = i_ent::MakeRationalBSplineSurface(
        {3, 3},                                    // degrees {M1, M2}
        cp_grid,
        {0., 0., 0., 0., 1., 2., 3., 3., 3., 3.},  // knot vector in U
        {0., 0., 0., 0., 1., 2., 3., 3., 3., 3.});  // knot vector in V
    nurbs_freeform->OverwriteColor(i_ent::ColorNumber::kCyan);

    return {nurbs_plane, nurbs_freeform};
}

/// @brief Example for Trimmed Surface entity (Type 144)
ent_vec CreateTrimmedSurface() {
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

    // Create closed curve on a parametric surface
    // (the curve B(t) is defined in the parameter space of the surface)
    Matrix3Xd cps(3, 5);
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

    // Trimmed surface
    auto trimmed_surf = i_ent::MakeTrimmedSurface(nurbs_s, nullptr, {closed_curve});
    trimmed_surf->OverwriteColor(i_ent::ColorNumber::kGreen);

    return {nurbs_s, nurbs_cc, closed_curve, closed_cons_bs, trimmed_surf};
}

/// @brief Example for Plane entity (Type 108)
/// @note Form 1/-1 is a planar region bounded by a closed curve lying in the
///       plane; Form 0 is an unbounded plane.
ent_vec CreatePlane() {
    // Bounded plane (Form 1): a circular planar face in the plane Z = 8.
    // The boundary is a full circle lying in that plane (z_t = 8), so it is
    // co-planar with the surface it trims.
    auto boundary = i_ent::MakeCircle(Vector2d(-40.0, 0.0), 8.0, 8.0);
    auto bounded_plane = i_ent::MakeBoundedPlane(
            0.0, 0.0, 1.0, 8.0,  // plane: 0*X + 0*Y + 1*Z = 8  (Z = 8)
            boundary);
    bounded_plane->OverwriteColor(i_ent::ColorNumber::kRed);

    // Unbounded plane (Form 0): the plane Z = -8 (no bounding curve).
    auto unbounded_plane = i_ent::MakePlane(0.0, 0.0, 1.0, -8.0);
    unbounded_plane->OverwriteColor(i_ent::ColorNumber::kBlue);

    return {boundary, bounded_plane, unbounded_plane};
}



/// @brief Main function (creates IGES data and writes to file)
int main() {
    igesio::IgesData iges_data;

    for (const auto& entity : CreateRuledSurface()) {
        iges_data.Root().AddEntity(entity);
    }
    for (const auto& entity : CreateSurfaceOfRevolution()) {
        iges_data.Root().AddEntity(entity);
    }
    for (const auto& entity : CreateTabulatedCylinder()) {
        iges_data.Root().AddEntity(entity);
    }
    for (const auto& entity : CreateRationalBSplineSurface()) {
        iges_data.Root().AddEntity(entity);
    }
    for (const auto& entity : CreateTrimmedSurface()) {
        iges_data.Root().AddEntity(entity);
    }
    for (const auto& entity : CreatePlane()) {
        iges_data.Root().AddEntity(entity);
    }

    // Write to IGES file
    auto success = igesio::WriteIges(iges_data, "sample_surfaces.igs");
    if (!success) {
        std::cerr << "Failed to write IGES file." << std::endl;
        return 1;
    }

    return 0;
}
