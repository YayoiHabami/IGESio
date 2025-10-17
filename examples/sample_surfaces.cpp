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

#include <igesio/entities/surfaces/ruled_surface.h>
#include <igesio/entities/surfaces/surface_of_revolution.h>
#include <igesio/entities/surfaces/tabulated_cylinder.h>
#include <igesio/entities/surfaces/rational_b_spline_surface.h>

#include <igesio/entities/curves/rational_b_spline_curve.h>
#include <igesio/entities/transformations/transformation_matrix.h>
#include <igesio/writer.h>

namespace i_ent = igesio::entities;
namespace i_mod = igesio::models;
using igesio::kPi;
using igesio::Vector2d;
using igesio::Vector3d;
using ent_vec = std::vector<std::shared_ptr<igesio::entities::EntityBase>>;



/// @brief Example for Ruled Surface entity (Type 118)
ent_vec CreateRuledSurface() {
    // curve1: Line
    auto curve1 = std::make_shared<i_ent::Line>(
        Vector3d{-5., 0., 0.}, Vector3d{5., 0., 0.});

    // curve2: Rational B-Spline Curve
    auto param = igesio::IGESParameterVector{
        3,  // number of control points - 1
        3,  // degree
        false, false, false, false,  // non-periodic open NURBS curve
        0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0,  // knot vector
        1.0, 1.0, 1.0, 1.0,  // weights
        -5.0, 0.0, -6.0,     // control point P(0)
        -3.0, 4.0, -6.0,     // control point P(1)
         3.0, 4.0, -6.0,     // control point P(2)
         5.0, 0.0, -6.0,     // control point P(3)
        0.0, 1.0,            // parameter range V(0), V(1)
        0.0, 0.0, 1.0        // normal vector of the defining plane
    };
    auto curve2 = std::make_shared<i_ent::RationalBSplineCurve>(param);

    // Ruled surface
    auto ruled_surf = std::make_shared<i_ent::RuledSurface>(curve1, curve2);
    ruled_surf->OverwriteColor(i_ent::ColorNumber::kGreen);

    return {curve1, curve2, ruled_surf};
}

/// @brief Example for Surface of Revolution entity (Type 120)
ent_vec CreateSurfaceOfRevolution() {
    // Axis of revolution:
    auto axis_line = std::make_shared<i_ent::Line>(
        Vector3d{1., 1., 1.}, Vector3d{1., 2., 3.});

    // Generatrix curve
    auto param = igesio::IGESParameterVector{
        3,  // number of control points - 1
        3,  // degree
        false, false, false, false,  // non-periodic open NURBS curve
        0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0,  // knot vector
        1.0, 1.0, 1.0, 1.0,  // weights
        1.0, -4.0,  0.0,    // control point P(0)
        1.0, -5.0,  1.5,    // control point P(1)
        1.0, -3.0,  2.0,    // control point P(2)
        1.0,  0.0,  4.0,    // control point P(3)
        0.0, 1.0,            // parameter range V(0), V(1)
        1.0, 0.0, 0.0        // normal vector of the defining plane
    };
    auto generatrix = std::make_shared<i_ent::RationalBSplineCurve>(param);

    // Surface of revolution
    auto surf_rev = std::make_shared<i_ent::SurfaceOfRevolution>(
        axis_line, generatrix, 0.0, kPi);
    surf_rev->OverwriteColor(i_ent::ColorNumber::kYellow);

    return {axis_line, generatrix, surf_rev};
}

/// @brief Example for Tabulated Cylinder (Type 122)
ent_vec CreateTabulatedCylinder() {
    // Directrix curve
    auto param = igesio::IGESParameterVector{
        3,  // number of control points - 1
        2,  // degree
        false, false, false, false,   // non-periodic open NURBS curve
        0., 0., 0., 0.5, 1., 1., 1.,  // knot vector
        1., 1., 1., 1.,               // weights
        0.0, -4.0, -4.0,              // control points P(0)
        0.0,  0.2, -1.1,              // control points P(1)
        0.0, -1.0,  4.5,              // control points P(2)
        0.0,  4.0,  4.0,              // control points P(3)
        0.0, 1.0,                     // parameter range V(0), V(1)
        1., 0., 0.                    // normal vector of the defining plane
    };
    auto directrix = std::make_shared<i_ent::RationalBSplineCurve>(param);

    // Axis direction
    Vector3d axis_dir{1., -1., 0.};
    double axis_length = 3.0;

    // Tabulated cylinder
    auto tab_cyl = std::make_shared<i_ent::TabulatedCylinder>(
            directrix, axis_dir, axis_length);
    tab_cyl->OverwriteColor(i_ent::ColorNumber::kCyan);

    return {directrix, tab_cyl};
}

/// @brief Example for Rational B-Spline Surface entity (Type 128)
/// @note Creates a NURBS surface with specified parameters
ent_vec CreateRationalBSplineSurface() {
    // Plane (Y = 5)
    auto param = igesio::IGESParameterVector{
        1, 1,  // K1, K2 (Number of control points - 1 in U and V)
        1, 1,  // M1, M2 (Degree in U and V)
        false, false, true, false, false,  // PROP1-5
        0., 0., 1., 1.,   // Knot vector in U
        0., 0., 1., 1.,   // Knot vector in V
        1., 1., 1., 1.,   // Weights
        -5., 5.,  5.,     // Control point (0,0)
        -5., 5., -5.,     // Control point (0,1)
         5., 5.,  5.,     // Control point (1,0)
         5., 5., -5.,     // Control point (1,1)
        0., 1., 0., 1.    // Parameter range in U and V
    };
    auto nurbs_plane = std::make_shared<i_ent::RationalBSplineSurface>(param);
    nurbs_plane->OverwriteColor(i_ent::ColorNumber::kYellow);

    // Freeform surface
    param = igesio::IGESParameterVector{
        5, 5,  // K1, K2 (Number of control points - 1 in U and V)
        3, 3,  // M1, M2 (Degree in U and V)
        false, false, true, false, false,         // PROP1-5
        0., 0., 0., 0., 1., 2., 3., 3., 3., 3.,   // Knot vector in U
        0., 0., 0., 0., 1., 2., 3., 3., 3., 3.,   // Knot vector in V
        1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1.,   // Weights W(0,0) to W(1,5)
        1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1.,   // Weights W(2,0) to W(3,5)
        1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1.,   // Weights W(4,0) to W(5,5)
        // Control points (36 points, each with x, y, z)
        -25., -25., -10.,  // Control point (0,0)
        -25., -15., -5.,   // Control point (0,1)
        -25., -5., 0.,     // Control point (0,2)
        -25., 5., 0.,      // Control point (0,3)
        -25., 15., -5.,    // Control point (0,4)
        -25., 25., -10.,   // Control point (0,5)
        -15., -25., -8.,   // Control point (1,0)
        -15., -15., -4.,   // Control point (1,1)
        -15., -5., -4.,    // Control point (1,2)
        -15., 5., -4.,     // Control point (1,3)
        -15., 15., -4.,    // Control point (1,4)
        -15., 25., -8.,    // Control point (1,5)
        -5., -25., -5.,    // Control point (2,0)
        -5., -15., -3.,    // Control point (2,1)
        -5., -5., -8.,     // Control point (2,2)
        -5., 5., -8.,      // Control point (2,3)
        -5., 15., -3.,     // Control point (2,4)
        -5., 25., -5.,     // Control point (2,5)
        5., -25., -3.,     // Control point (3,0)
        5., -15., -2.,     // Control point (3,1)
        5., -5., -8.,      // Control point (3,2)
        5., 5., -8.,       // Control point (3,3)
        5., 15., -2.,      // Control point (3,4)
        5., 25., -3.,      // Control point (3,5)
        15., -25., -8.,    // Control point (4,0)
        15., -15., -4.,    // Control point (4,1)
        15., -5., -4.,     // Control point (4,2)
        15., 5., -4.,      // Control point (4,3)
        15., 15., -4.,     // Control point (4,4)
        15., 25., -8.,     // Control point (4,5)
        25., -25., -10.,   // Control point (5,0)
        25., -15., -5.,    // Control point (5,1)
        25., -5., 2.,      // Control point (5,2)
        25., 5., 2.,       // Control point (5,3)
        25., 15., -5.,     // Control point (5,4)
        25., 25., -10.,    // Control point (5,5)
        0., 3., 0., 3.     // Parameter range in U and V
    };
    auto nurbs_freeform = std::make_shared<i_ent::RationalBSplineSurface>(param);
    nurbs_freeform->OverwriteColor(i_ent::ColorNumber::kCyan);

    return {nurbs_plane, nurbs_freeform};
}



/// @brief Main function (creates IGES data and writes to file)
int main() {
    i_mod::IgesData iges_data;

    for (const auto& entity : CreateRuledSurface()) {
        iges_data.AddEntity(entity);
    }
    for (const auto& entity : CreateSurfaceOfRevolution()) {
        iges_data.AddEntity(entity);
    }
    for (const auto& entity : CreateTabulatedCylinder()) {
        iges_data.AddEntity(entity);
    }
    for (const auto& entity : CreateRationalBSplineSurface()) {
        iges_data.AddEntity(entity);
    }

    // Write to IGES file
    auto success = igesio::WriteIges(iges_data, "sample_surfaces.igs");
    if (!success) {
        std::cerr << "Failed to write IGES file." << std::endl;
        return 1;
    }

    return 0;
}
