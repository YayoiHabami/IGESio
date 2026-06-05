/**
 * @file examples/geometric_properties.cpp
 * @brief This example demonstrates how to compute geometric properties of curves and surfaces
 * @author Yayoi Habami
 * @date 2025-10-27
 * @copyright 2025 Yayoi Habami
 */
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <igesio/entities/curves/rational_b_spline_curve.h>
#include <igesio/entities/surfaces/rational_b_spline_surface.h>

namespace i_ent = igesio::entities;
using igesio::Matrix3Xd;
using igesio::Vector3d;



/**
 * Curves
 */

/// @brief Create a Rational B-Spline Curve for testing
/// @return Shared pointer to the created Rational B-Spline Curve
std::shared_ptr<i_ent::ICurve> CreateRationalBSplineCurve() {
    // Create NURBS curve (4 control points, degree 3)
    Matrix3Xd cps(3, 4);
    cps.col(0) = Vector3d(-4.0, -4.0, 0.0);  // control point P(0)
    cps.col(1) = Vector3d(-1.5,  7.0, 3.5);  // control point P(1)
    cps.col(2) = Vector3d( 4.0, -3.0, 1.0);  // control point P(2)
    cps.col(3) = Vector3d( 4.0,  4.0, 0.0);  // control point P(3)
    // weights (all 1.0) and parameter range ([0,1]) are defaulted
    return i_ent::MakeRationalBSplineCurve(
        3,                                          // degree
        cps,
        {0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0});  // knot vector
}

/// @brief Main function for curves
void TestCurveGeometricProperties() {
    auto curve = CreateRationalBSplineCurve();

    // Get parameter range
    auto [u_start, u_end] = curve->GetParameterRange();
    std::cout << "Parameter range: [" << u_start << ", " << u_end << "]" << std::endl;

    // Calculate point at midpoint
    double u = (u_start + u_end) / 2.0;
    auto point_opt = curve->TryGetPointAt(u);
    if (point_opt) {
        Vector3d point = *point_opt;
        std::cout << "Point at u = " << u << ": " << point << std::endl;
    } else {
        std::cout << "Failed to compute point at u = " << u << std::endl;
    }
    std::cout << std::endl;

    // Calculate derivatives at midpoint
    unsigned int n_deriv = 2;
    auto derivs_opt = curve->TryGetDerivatives(u, n_deriv);
    if (derivs_opt) {
        const auto& derivs = *derivs_opt;
        for (unsigned int i = 0; i <= n_deriv; ++i) {
            std::cout << "Derivative C^" << i << "(u): " << derivs[i] << std::endl;
        }
    } else {
        std::cout << "Failed to compute derivatives at u = " << u << std::endl;
    }
    std::cout << std::endl;

    // Calculate tangent at midpoint
    auto tangent_opt = curve->TryGetTangentAt(u);
    if (tangent_opt) {
        Vector3d tangent = *tangent_opt;
        std::cout << "Tangent T(u): " << tangent << std::endl;
    } else {
        std::cout << "Failed to compute tangent at u = " << u << std::endl;
    }
    std::cout << std::endl;

    // Calculate curvature at midpoint
    double curvature = curve->GetCurvature(u);
    if (curvature >= 0.0) {
        std::cout << "Curvature kappa(u): " << curvature << std::endl;
    } else {
        std::cout << "Failed to compute curvature at u = " << u << std::endl;
    }
    std::cout << std::endl;

    // Calculate curve length
    double length = curve->Length();
    std::cout << "Curve length: " << length << std::endl;
    length = curve->Length(0.25, 0.75);
    std::cout << "Curve length from u=0.25 to u=0.75: " << length << std::endl;
    std::cout << std::endl;

    // Calculate Frenet frame at midpoint
    auto normal_opt = curve->TryGetNormalAt(u);
    auto binormal_opt = curve->TryGetBinormalAt(u);
    if (normal_opt && binormal_opt) {
        Vector3d normal = *normal_opt;
        Vector3d binormal = *binormal_opt;
        std::cout << "Normal N(u): " << normal << std::endl;
        std::cout << "Binormal B(u): " << binormal << std::endl;
    } else {
        std::cout << "Failed to compute Frenet frame at u = " << u << std::endl;
    }
    std::cout << std::endl;
}



/**
 * Surfaces
 */

std::shared_ptr<i_ent::ISurface> CreateRationalBSplineSurface() {
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
    return i_ent::MakeRationalBSplineSurface(
        {3, 3},                                    // degrees {M1, M2}
        cp_grid,
        {0., 0., 0., 0., 1., 2., 3., 3., 3., 3.},  // knot vector in U
        {0., 0., 0., 0., 1., 2., 3., 3., 3., 3.});  // knot vector in V
}

/// @brief Main function for surfaces
void TestSurfaceGeometricProperties() {
    auto surface = CreateRationalBSplineSurface();

    // Get parameter range
    auto [u_start, u_end] = surface->GetURange();
    auto [v_start, v_end] = surface->GetVRange();
    std::cout << "Parameter range U: [" << u_start << ", " << u_end << "]" << std::endl;
    std::cout << "Parameter range V: [" << v_start << ", " << v_end << "]" << std::endl;

    // Calculate point at midpoint
    double u = (u_start + u_end) / 2.0;
    double v = (v_start + v_end) / 2.0;
    auto point_opt = surface->TryGetPointAt(u, v);
    if (point_opt) {
        Vector3d point = *point_opt;
        std::cout << "Point at (u, v) = (" << u << ", " << v << "): " << point << std::endl;
    } else {
        std::cout << "Failed to compute point at (u, v) = (" << u << ", " << v << ")" << std::endl;
    }
    std::cout << std::endl;

    // Calculate derivatives at midpoint
    unsigned int n_deriv = 2;
    auto derivs_opt = surface->TryGetDerivatives(u, v, n_deriv);
    if (derivs_opt) {
        const auto& derivs = *derivs_opt;
        for (unsigned int i = 0; i <= n_deriv; ++i) {
            for (unsigned int j = 0; j <= n_deriv - i; ++j) {
                std::cout << "Derivative S^(" << i << "," << j << ")(u,v): "
                          << derivs(i, j) << std::endl;
            }
        }
    } else {
        std::cout << "Failed to compute derivatives at (u, v) = ("
                  << u << ", " << v << ")" << std::endl;
    }
    std::cout << std::endl;

    // Calculate tangent and normal vectors at midpoint
    auto tangent_opt = surface->TryGetTangentAt(u, v);
    auto normal_opt = surface->TryGetNormalAt(u, v);
    if (tangent_opt && normal_opt) {
        auto [tangent_u, tangent_v] = *tangent_opt;
        Vector3d normal = *normal_opt;
        std::cout << "Tangent T_u(u,v): " << tangent_u << std::endl;
        std::cout << "Tangent T_v(u,v): " << tangent_v << std::endl;
        std::cout << "Normal N(u,v): " << normal << std::endl;
    } else {
        std::cout << "Failed to compute tangent/normal at (u, v) = ("
                  << u << ", " << v << ")" << std::endl;
    }
    std::cout << std::endl;

    // Calculate first and second fundamental forms at midpoint
    auto first_form_opt = surface->TryGetFirstFundamentalForm(u, v);
    auto second_form_opt = surface->TryGetSecondFundamentalForm(u, v);
    if (first_form_opt && second_form_opt) {
        auto [E, F, G] = *first_form_opt;
        auto [L, M, N] = *second_form_opt;
        std::cout << "First Fundamental Form (E, F, G): ("
                  << E << ", " << F << ", " << G << ")" << std::endl;
        std::cout << "Second Fundamental Form (L, M, N): ("
                  << L << ", " << M << ", " << N << ")" << std::endl;
    } else {
        std::cout << "Failed to compute fundamental forms at (u, v) = ("
                  << u << ", " << v << ")" << std::endl;
    }
    std::cout << std::endl;

    // Calculate area of the entire surface
    double area = surface->Area();
    std::cout << "Surface area: " << area << std::endl;
    // Calculate area of a subregion
    auto u_mid = (u_start + u_end) / 2.0;
    auto v_mid = (v_start + v_end) / 2.0;
    area = surface->Area(u_start, u_mid, v_start, v_mid);
    std::cout << "Surface area in subregion: " << area << std::endl;
    std::cout << std::endl;

    // Calculate Curvatures at midpoint
    auto gaussian_curvature_opt = surface->TryGetGaussianCurvature(u, v);
    auto mean_curvature_opt = surface->TryGetMeanCurvature(u, v);
    auto principal_curvatures_opt = surface->TryGetPrincipalCurvatures(u, v);
    if (gaussian_curvature_opt && mean_curvature_opt && principal_curvatures_opt) {
        double K = *gaussian_curvature_opt;
        double H = *mean_curvature_opt;
        auto [k1, k2] = *principal_curvatures_opt;
        std::cout << "Gaussian Curvature K(u,v): " << K << std::endl;
        std::cout << "Mean Curvature H(u,v): " << H << std::endl;
        std::cout << "Principal Curvatures (k1, k2): ("
                  << k1 << ", " << k2 << ")" << std::endl;
    } else {
        std::cout << "Failed to compute curvatures at (u, v) = ("
                  << u << ", " << v << ")" << std::endl;
    }
    std::cout << std::endl;
}




/**
 * Main function
 */

int main() {
    std::cout << "Testing geometric properties of curves..." << std::endl;
    TestCurveGeometricProperties();

    std::cout << "Testing geometric properties of surfaces..." << std::endl;
    TestSurfaceGeometricProperties();

    std::cout << "Done." << std::endl;
    return 0;
}
