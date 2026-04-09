/**
 * @file tests/entities/surfaces/surfaces_for_testing.h
 * @brief entities/surfaces配下のテストで使用する曲面エンティティの定義
 * @author Yayoi Habami
 * @date 2025-10-24
 * @copyright 2025 Yayoi Habami
 * @note 変換行列は使用しない. 適用する場合は別途エンティティを定義して上書きすること
 */
#ifndef IGESIO_TESTS_ENTITIES_SURFACES_SURFACES_FOR_TESTING_H_
#define IGESIO_TESTS_ENTITIES_SURFACES_SURFACES_FOR_TESTING_H_

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "igesio/entities/curves/rational_b_spline_curve.h"

#include "igesio/entities/surfaces/ruled_surface.h"
#include "igesio/entities/surfaces/surface_of_revolution.h"
#include "igesio/entities/surfaces/tabulated_cylinder.h"
#include "igesio/entities/surfaces/rational_b_spline_surface.h"

namespace igesio::tests {

/// @brief テスト用曲面エンティティの構造体
/// @note テストの都合上、エンティティはC2連続であることが期待される
struct TestSurface {
    /// @brief 曲面エンティティの名前
    std::string name;
    /// @brief 曲面エンティティの共有ポインタ
    std::shared_ptr<entities::ISurface> surface;

    /// @brief デフォルトコンストラクタ
    TestSurface() = default;
    /// @brief コンストラクタ
    TestSurface(const std::string& n,
                const std::shared_ptr<entities::ISurface>& s)
            : name(n), surface(s) {}
    /// @brief コンストラクタ (nameのみ)
    explicit TestSurface(const std::string& n)
            : name(n), surface(nullptr) {}
};

using surface_vec = std::vector<TestSurface>;



/// @brief Ruled Surfaceエンティティの作成
surface_vec CreateRuledSurfaces() {
    // curve1: Line
    auto curve1 = std::make_shared<entities::Line>(
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
    auto curve2 = std::make_shared<entities::RationalBSplineCurve>(param);

    // Ruled surface
    TestSurface ruled_surface("Ruled Surface");
    ruled_surface.surface = std::make_shared<entities::RuledSurface>(curve1, curve2);

    // Ruled surface (reversed)
    TestSurface ruled_surface_reversed("Ruled Surface (reversed)");
    ruled_surface_reversed.surface =
        std::make_shared<entities::RuledSurface>(curve1, curve2, true);

    return {ruled_surface, ruled_surface_reversed};
}

/// @brief Surface of Revolutionエンティティの作成
surface_vec CreateSurfaceOfRevolutions() {
    // Axis of revolution:
    auto axis_line = std::make_shared<entities::Line>(
            Vector3d{1., 1., 1.}, Vector3d{1., 2., 3.});

    // Generatrix: Rational B-Spline Curve
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
    auto generatrix = std::make_shared<entities::RationalBSplineCurve>(param);

    // Surface of Revolution
    TestSurface rev_full("Surface of Revolution (0 to 2π)");
    rev_full.surface = std::make_shared<entities::SurfaceOfRevolution>(
            axis_line, generatrix, 0.0, 2*kPi);

    TestSurface rev_half("Surface of Revolution (π/2 to 3π/2)");
    rev_half.surface = std::make_shared<entities::SurfaceOfRevolution>(
            axis_line, generatrix, kPi / 2, 3 * kPi / 2);

    return {rev_full, rev_half};
}

/// @brief Tabulated Cylinderエンティティの作成
surface_vec CreateTabulatedCylinders() {
    // Directrix curve
    auto param = igesio::IGESParameterVector{
        3,  // number of control points - 1
        3,  // degree
        false, false, false, false,   // non-periodic open NURBS curve
        0., 0., 0., 0., 1., 1., 1., 1.,  // knot vector
        1., 1., 1., 1.,               // weights
        0.0, -4.0, -4.0,              // control points P(0)
        0.0,  0.2, -1.1,              // control points P(1)
        0.0, -1.0,  4.5,              // control points P(2)
        0.0,  4.0,  4.0,              // control points P(3)
        0.0, 1.0,                     // parameter range V(0), V(1)
        1., 0., 0.                    // normal vector of the defining plane
    };
    auto directrix = std::make_shared<entities::RationalBSplineCurve>(param);

    // Axis direction
    Vector3d axis_dir{1., -1., 0.};
    double axis_length = 3.0;

    // Tabulated cylinder
    TestSurface tabulated_cylinder("Tabulated Cylinder");
    tabulated_cylinder.surface = std::make_shared<entities::TabulatedCylinder>(
            directrix, axis_dir, axis_length);

    return {tabulated_cylinder};
}

/// @brief Rational B-Spline Surfaceエンティティの作成
surface_vec CreateRationalBSplineSurfaces() {
    // Plane (Y = 5)
    TestSurface nurbs_plane("Rational B-Spline Surface: Plane");
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
    nurbs_plane.surface = std::make_shared<entities::RationalBSplineSurface>(param);

    // Freeform surface
    TestSurface nurbs_freeform("Rational B-Spline Surface: Freeform");
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
    nurbs_freeform.surface = std::make_shared<entities::RationalBSplineSurface>(param);

    return {nurbs_plane, nurbs_freeform};
}



surface_vec CreateAllTestSurfaces() {
    surface_vec all_surfaces;

    // Ruled Surfaces
    auto ruled_surfaces = CreateRuledSurfaces();
    all_surfaces.insert(all_surfaces.end(),
                        ruled_surfaces.begin(), ruled_surfaces.end());

    // Surface of Revolutions
    auto rev_surfaces = CreateSurfaceOfRevolutions();
    all_surfaces.insert(all_surfaces.end(),
                        rev_surfaces.begin(), rev_surfaces.end());

    // Tabulated Cylinders
    auto tabulated_cylinders = CreateTabulatedCylinders();
    all_surfaces.insert(all_surfaces.end(),
                        tabulated_cylinders.begin(), tabulated_cylinders.end());

    // Rational B-Spline Surfaces
    auto nurbs_surfaces = CreateRationalBSplineSurfaces();
    all_surfaces.insert(all_surfaces.end(),
                        nurbs_surfaces.begin(), nurbs_surfaces.end());

    return all_surfaces;
}

}  // namespace igesio::tests

#endif  // IGESIO_TESTS_ENTITIES_SURFACES_SURFACES_FOR_TESTING_H_
