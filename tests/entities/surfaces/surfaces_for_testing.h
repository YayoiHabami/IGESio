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

#include <array>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "igesio/entities/curves/rational_b_spline_curve.h"

#include "igesio/entities/surfaces/plane.h"
#include "igesio/entities/surfaces/ruled_surface.h"
#include "igesio/entities/surfaces/surface_of_revolution.h"
#include "igesio/entities/surfaces/tabulated_cylinder.h"
#include "igesio/entities/surfaces/rational_b_spline_surface.h"
#include "igesio/entities/surfaces/trimmed_surface.h"

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
inline surface_vec CreateRuledSurfaces() {
    // curve1: Line
    auto curve1 = entities::MakeLine(
            Vector3d{-5., 0., 0.}, Vector3d{5., 0., 0.});
    // curve2: Rational B-Spline Curve
    Matrix3Xd cps(3, 4);
    cps.col(0) = Vector3d(-5.0, 0.0, -6.0);  // control point P(0)
    cps.col(1) = Vector3d(-3.0, 4.0, -6.0);  // control point P(1)
    cps.col(2) = Vector3d(3.0,  4.0, -6.0);  // control point P(2)
    cps.col(3) = Vector3d(5.0,  0.0, -6.0);  // control point P(3)
    auto curve2 = entities::MakeRationalBSplineCurve(
        3,                                         // degree
        cps,
        {0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0},  // knot vector
        {},                                        // weights (all 1.0)
        std::array<double, 2>{0.0, 1.0});          // parameter range V(0), V(1)

    // Ruled surface
    TestSurface ruled_surface("Ruled Surface");
    ruled_surface.surface = entities::MakeRuledSurface(curve1, curve2);

    // Ruled surface (reversed)
    TestSurface ruled_surface_reversed("Ruled Surface (reversed)");
    ruled_surface_reversed.surface =
        entities::MakeRuledSurface(curve1, curve2, true);

    return {ruled_surface, ruled_surface_reversed};
}

/// @brief Surface of Revolutionエンティティの作成
inline surface_vec CreateSurfaceOfRevolutions() {
    // Axis of revolution:
    auto axis_line = entities::MakeLine(
            Vector3d{1., 1., 1.}, Vector3d{1., 2., 3.});

    // Generatrix: Rational B-Spline Curve
    Matrix3Xd cps(3, 4);
    cps.col(0) = Vector3d(1.0, -4.0, 0.0);   // control point P(0)
    cps.col(1) = Vector3d(1.0, -5.0, 1.5);   // control point P(1)
    cps.col(2) = Vector3d(1.0, -3.0, 2.0);   // control point P(2)
    cps.col(3) = Vector3d(1.0,  0.0, 4.0);   // control point P(3)
    auto generatrix = entities::MakeRationalBSplineCurve(
        3,                                         // degree
        cps,
        {0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0},  // knot vector
        {},                                        // weights (all 1.0)
        std::array<double, 2>{0.0, 1.0});          // parameter range V(0), V(1)

    // Surface of Revolution
    TestSurface rev_full("Surface of Revolution (0 to 2π)");
    rev_full.surface = entities::MakeSurfaceOfRevolution(
            axis_line, generatrix, 0.0, 2*kPi);

    TestSurface rev_half("Surface of Revolution (π/2 to 3π/2)");
    rev_half.surface = entities::MakeSurfaceOfRevolution(
            axis_line, generatrix, kPi / 2, 3 * kPi / 2);

    return {rev_full, rev_half};
}

/// @brief Tabulated Cylinderエンティティの作成
inline surface_vec CreateTabulatedCylinders() {
    // Directrix curve
    Matrix3Xd cps(3, 4);
    cps.col(0) = Vector3d(0.0, -4.0, -4.0);  // control point P(0)
    cps.col(1) = Vector3d(0.0,  0.2, -1.1);  // control point P(1)
    cps.col(2) = Vector3d(0.0, -1.0,  4.5);  // control point P(2)
    cps.col(3) = Vector3d(0.0,  4.0,  4.0);  // control point P(3)
    auto directrix = entities::MakeRationalBSplineCurve(
        3,                                         // degree
        cps,
        {0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0},  // knot vector
        {},                                        // weights (all 1.0)
        std::array<double, 2>{0.0, 1.0});          // parameter range V(0), V(1)

    // Axis direction
    Vector3d axis_dir{1., -1., 0.};
    double axis_length = 3.0;

    // Tabulated cylinder
    TestSurface tabulated_cylinder("Tabulated Cylinder");
    tabulated_cylinder.surface = entities::MakeExtrudedSurface(
            directrix, axis_dir.normalized() * axis_length);

    return {tabulated_cylinder};
}

/// @brief Rational B-Spline Surfaceエンティティの作成
inline surface_vec CreateRationalBSplineSurfaces() {
    // Plane (Y = 5)
    TestSurface nurbs_plane("Rational B-Spline Surface: Plane");
    nurbs_plane.surface = entities::MakeRationalBSplineSurface(
        {1, 1},                                              // degrees {M1, M2}
        {{Vector3d(-5., 5.,  5.), Vector3d(-5., 5., -5.)},   // P(0,j)
         {Vector3d( 5., 5.,  5.), Vector3d( 5., 5., -5.)}},  // P(1,j)
        {0., 0., 1., 1.},                                    // knot vector in U
        {0., 0., 1., 1.});                                   // knot vector in V

    // Freeform surface (6x6 control points, degree 3 in U and V)
    // grid[i][j] = P(i,j) = (-25+10i, -25+10j, z_ij)
    TestSurface nurbs_freeform("Rational B-Spline Surface: Freeform");
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
    nurbs_freeform.surface = entities::MakeRationalBSplineSurface(
        {3, 3},                                    // degrees {M1, M2}
        cp_grid,
        {0., 0., 0., 0., 1., 2., 3., 3., 3., 3.},  // knot vector in U
        {0., 0., 0., 0., 1., 2., 3., 3., 3., 3.});  // knot vector in V

    return {nurbs_plane, nurbs_freeform};
}

/// @brief Trimmed Surfaceエンティティの作成 (未トリム; N1=0)
/// @note 未トリムのトリム面は基底曲面へ全委譲し、IsInDomainは常にtrueとなるため、
///       汎用ISurfaceテストで委譲経路 (パラメータ範囲・偏導関数・面積・BBox) を
///       検証できる。基底には非自明な偏導関数を持つフリーフォームNURBSを用いる。
inline surface_vec CreateTrimmedSurfaces() {
    auto base = CreateRationalBSplineSurfaces();

    TestSurface untrimmed("Trimmed Surface (untrimmed over freeform NURBS)");
    untrimmed.surface = entities::MakeTrimmedSurface(base[1].surface, nullptr, {});

    return {untrimmed};
}



/// @brief Plane (Type 108, Form 0) エンティティの作成
/// @note 無限平面のため、汎用ISurfaceテストの面積系 (全範囲面積が無限) は
///       IsFinite()でスキップされる。偏導関数の数値微分は±1e8までクランプ
///       されて評価されるため、桁落ちを避けられるよう軸平行平面 (Y=5; フレームの
///       各成分が厳密表現可能) を用いる。
inline surface_vec CreatePlanes() {
    TestSurface plane("Plane (unbounded, Y = 5)");
    plane.surface = entities::MakePlane(0.0, 1.0, 0.0, 5.0);
    return {plane};
}



inline surface_vec CreateAllTestSurfaces() {
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

    // Trimmed Surfaces (untrimmed; delegates to base)
    auto trimmed_surfaces = CreateTrimmedSurfaces();
    all_surfaces.insert(all_surfaces.end(),
                        trimmed_surfaces.begin(), trimmed_surfaces.end());

    // Planes (unbounded; infinite extent — area checks skip via IsFinite())
    auto planes = CreatePlanes();
    all_surfaces.insert(all_surfaces.end(), planes.begin(), planes.end());

    return all_surfaces;
}

}  // namespace igesio::tests

#endif  // IGESIO_TESTS_ENTITIES_SURFACES_SURFACES_FOR_TESTING_H_
