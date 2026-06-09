/**
 * @file examples/special_swept_surfaces.cpp
 * @brief Generates IGES files of edge-case swept surfaces (Types 120/118/122)
 *        for rendering verification (creases / degenerate apex etc.)
 * @author Yayoi Habami
 * @date 2026-06-08
 * @copyright 2026 Yayoi Habami
 * @note Each case is laid out along the world X axis so they can be inspected
 *       side by side. Three files are written, one per surface family.
 *
 *       Surface of Revolution (Type 120): axis = +Y, profile in the XY plane
 *         1. Cone whose generatrix end lies on the axis (apex; no hole)
 *         2. Sphere from a semicircle (both poles are apexes; v-closed)
 *         3. Stepped profile (sharp circular ribs; corners)
 *         4. Cup: degenerate disk center on axis + sharp rim (apex + corner)
 *         5. Partial revolution of 270 deg (open in v; seam boundary)
 *         6. Torus from an off-axis full circle (u-closed, v-closed, no apex)
 *
 *       Ruled Surface (Type 118): S(u,v) = (1-v)C1(u) + vC2(u)
 *         1. Tent collapsing to a shared end point (degenerate ruling = apex)
 *         2. V-shaped C1 vs straight C2 (single crease)
 *         3. Both curves cornered + DIRFLG reversed (multiple creases)
 *         4. Two smooth curves (regression baseline; should stay smooth)
 *
 *       Tabulated Cylinder (Type 122): directrix extruded along +Z
 *         1. Closed square directrix (4 sharp edges + u-closed)
 *         2. L-shaped open directrix (single sharp edge)
 *         3. Line + arc composite directrix (corner mixed with a smooth part)
 *         4. Smooth NURBS directrix (regression baseline)
 */

#include <math.h>

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <igesio/igesio.h>

namespace i_ent = igesio::entities;

using igesio::kPi;
using igesio::Matrix3Xd;
using igesio::Vector2d;
using igesio::Vector3d;
using ent_vec = std::vector<std::shared_ptr<igesio::EntityBase>>;
using Color = i_ent::ColorNumber;



/**
 * Surface of Revolution (Type 120)
 */

/// @brief 1. Cone whose generatrix start point sits on the axis (apex test)
ent_vec SorCone(double cx) {
    auto axis = i_ent::MakeLine(Vector3d{cx, 0., 0.}, Vector3d{cx, 1., 0.});
    // The first end (cx, 4) is on the axis -> becomes the cone apex
    auto gen = i_ent::MakeLine(Vector3d{cx, 4., 0.}, Vector3d{cx + 3., 0., 0.});
    auto surf = i_ent::MakeSurfaceOfRevolution(axis, gen, 0., 2. * kPi);
    surf->OverwriteColor(Color::kRed);
    return {axis, gen, surf};
}

/// @brief 2. Sphere from a semicircle (both poles are apexes; v-closed)
ent_vec SorSphere(double cx) {
    auto axis = i_ent::MakeLine(Vector3d{cx, -3., 0.}, Vector3d{cx, 3., 0.});
    // Semicircle: endpoints (cx, -3) and (cx, 3) lie on the axis
    auto gen = i_ent::MakeCircularArc(Vector2d{cx, 0.}, 3.0, -kPi / 2., kPi / 2.);
    auto surf = i_ent::MakeSurfaceOfRevolution(axis, gen, 0., 2. * kPi);
    surf->OverwriteColor(Color::kGreen);
    return {axis, gen, surf};
}

/// @brief 3. Stepped polyline generatrix (sharp circular ribs)
ent_vec SorStepped(double cx) {
    auto axis = i_ent::MakeLine(Vector3d{cx, 0., 0.}, Vector3d{cx, 1., 0.});
    auto gen = i_ent::MakeLinearPath(std::vector<Vector3d>{
            Vector3d{cx + 1., 0., 0.}, Vector3d{cx + 1., 2., 0.},
            Vector3d{cx + 2., 2., 0.}, Vector3d{cx + 2., 4., 0.}});
    auto surf = i_ent::MakeSurfaceOfRevolution(axis, gen, 0., 2. * kPi);
    surf->OverwriteColor(Color::kBlue);
    return {axis, gen, surf};
}

/// @brief 4. Cup: disk center on axis (apex) + sharp rim (corner)
ent_vec SorCup(double cx) {
    auto axis = i_ent::MakeLine(Vector3d{cx, 0., 0.}, Vector3d{cx, 1., 0.});
    auto gen = i_ent::MakeLinearPath(std::vector<Vector3d>{
            Vector3d{cx, 0., 0.},        // on the axis -> degenerate disk center
            Vector3d{cx + 2., 0., 0.},   // rim corner
            Vector3d{cx + 2., 3., 0.}});  // top corner
    auto surf = i_ent::MakeSurfaceOfRevolution(axis, gen, 0., 2. * kPi);
    surf->OverwriteColor(Color::kYellow);
    return {axis, gen, surf};
}

/// @brief 5. Partial revolution of 270 deg (open in v; seam boundary)
ent_vec SorPartial(double cx) {
    auto axis = i_ent::MakeLine(Vector3d{cx, 0., 0.}, Vector3d{cx, 1., 0.});
    auto gen = i_ent::MakeLine(Vector3d{cx + 1., 0., 0.}, Vector3d{cx + 2.5, 4., 0.});
    auto surf = i_ent::MakeSurfaceOfRevolution(axis, gen, 0., 1.5 * kPi);
    surf->OverwriteColor(Color::kCyan);
    return {axis, gen, surf};
}

/// @brief 6. Torus from an off-axis full circle (u-closed, v-closed)
ent_vec SorTorus(double cx) {
    auto axis = i_ent::MakeLine(Vector3d{cx, 0., 0.}, Vector3d{cx, 1., 0.});
    auto gen = i_ent::MakeCircle(Vector2d{cx + 3., 2.}, 1.0);  // does not touch axis
    auto surf = i_ent::MakeSurfaceOfRevolution(axis, gen, 0., 2. * kPi);
    surf->OverwriteColor(Color::kMagenta);
    return {axis, gen, surf};
}



/**
 * Ruled Surface (Type 118)
 */

/// @brief 1. Tent collapsing to a shared end point (degenerate ruling = apex)
ent_vec RuledTent(double cx) {
    auto c1 = i_ent::MakeLine(Vector3d{cx - 3., 0., 0.}, Vector3d{cx, 5., 0.});
    // C2 ends at the same point (cx, 5, 0); its base is out of plane (z = 2)
    auto c2 = i_ent::MakeLine(Vector3d{cx + 3., 0., 2.}, Vector3d{cx, 5., 0.});
    auto surf = i_ent::MakeRuledSurface(c1, c2);
    surf->OverwriteColor(Color::kRed);
    return {c1, c2, surf};
}

/// @brief 2. V-shaped C1 vs straight C2 (single crease)
ent_vec RuledCrease(double cx) {
    auto c1 = i_ent::MakeLinearPath(std::vector<Vector3d>{
            Vector3d{cx - 4., 0., 0.}, Vector3d{cx, 2.5, 0.},
            Vector3d{cx + 4., 0., 0.}});
    auto c2 = i_ent::MakeLine(Vector3d{cx - 4., 0., 5.}, Vector3d{cx + 4., 0., 5.});
    auto surf = i_ent::MakeRuledSurface(c1, c2);
    surf->OverwriteColor(Color::kGreen);
    return {c1, c2, surf};
}

/// @brief 3. Both curves cornered + DIRFLG reversed (multiple creases)
ent_vec RuledBothCornersReversed(double cx) {
    auto c1 = i_ent::MakeLinearPath(std::vector<Vector3d>{
            Vector3d{cx - 4., 0., 0.}, Vector3d{cx - 1., 3., 0.},
            Vector3d{cx + 4., 0., 0.}});
    auto c2 = i_ent::MakeLinearPath(std::vector<Vector3d>{
            Vector3d{cx - 4., 0., 5.}, Vector3d{cx + 1., 3., 5.},
            Vector3d{cx + 4., 0., 5.}});
    auto surf = i_ent::MakeRuledSurface(c1, c2, /*is_reversed=*/true);
    surf->OverwriteColor(Color::kYellow);
    return {c1, c2, surf};
}

/// @brief 4. Two smooth curves (regression baseline; should stay smooth)
ent_vec RuledSmooth(double cx) {
    auto c1 = i_ent::MakeLine(Vector3d{cx - 4., 0., 0.}, Vector3d{cx + 4., 0., 0.});
    Matrix3Xd cps(3, 4);
    cps.col(0) = Vector3d{cx - 4., 0., 5.};
    cps.col(1) = Vector3d{cx - 1.5, 4., 5.};
    cps.col(2) = Vector3d{cx + 1.5, 4., 5.};
    cps.col(3) = Vector3d{cx + 4., 0., 5.};
    auto c2 = i_ent::MakeRationalBSplineCurve(
            3, cps, {0., 0., 0., 0., 1., 1., 1., 1.});
    auto surf = i_ent::MakeRuledSurface(c1, c2);
    surf->OverwriteColor(Color::kCyan);
    return {c1, c2, surf};
}



/**
 * Tabulated Cylinder (Type 122)
 */

/// @brief 1. Closed square directrix (4 sharp edges + u-closed)
ent_vec TabSquare(double cx) {
    auto dir = i_ent::MakeLinearPath(std::vector<Vector2d>{
            Vector2d{cx - 2., -2.}, Vector2d{cx + 2., -2.},
            Vector2d{cx + 2., 2.}, Vector2d{cx - 2., 2.}},
            /*is_closed=*/true, /*z_t=*/0.);
    auto surf = i_ent::MakeExtrudedSurface(dir, Vector3d{0., 0., 5.});
    surf->OverwriteColor(Color::kRed);
    return {dir, surf};
}

/// @brief 2. L-shaped open directrix (single sharp edge)
ent_vec TabL(double cx) {
    auto dir = i_ent::MakeLinearPath(std::vector<Vector3d>{
            Vector3d{cx - 3., 0., 0.}, Vector3d{cx, 0., 0.},
            Vector3d{cx, 3., 0.}});
    auto surf = i_ent::MakeExtrudedSurface(dir, Vector3d{0., 0., 5.});
    surf->OverwriteColor(Color::kGreen);
    return {dir, surf};
}

/// @brief 3. Line + arc composite directrix (corner mixed with a smooth part)
ent_vec TabComposite(double cx) {
    auto line = i_ent::MakeLine(Vector3d{cx - 4., 0., 0.}, Vector3d{cx, 0., 0.});
    // Quarter arc starting vertically at (cx, 0) -> a corner against the line
    auto arc = i_ent::MakeCircularArc(
            Vector2d{cx - 2., 0.}, Vector2d{cx, 0.}, Vector2d{cx - 2., 2.});
    auto dir = i_ent::MakeCompositeCurve({line, arc});
    auto surf = i_ent::MakeExtrudedSurface(dir, Vector3d{0., 0., 5.});
    surf->OverwriteColor(Color::kYellow);
    return {line, arc, dir, surf};
}

/// @brief 4. Smooth NURBS directrix (regression baseline)
ent_vec TabSmooth(double cx) {
    Matrix3Xd cps(3, 4);
    cps.col(0) = Vector3d{cx - 4., -2., 0.};
    cps.col(1) = Vector3d{cx + 0.2, 1., 0.};
    cps.col(2) = Vector3d{cx - 1., 4.5, 0.};
    cps.col(3) = Vector3d{cx + 4., 2., 0.};
    auto dir = i_ent::MakeRationalBSplineCurve(
            2, cps, {0., 0., 0., 0.5, 1., 1., 1.});
    auto surf = i_ent::MakeExtrudedSurface(dir, Vector3d{0., 0., 5.});
    surf->OverwriteColor(Color::kCyan);
    return {dir, surf};
}



/// @brief Builds the cases (each offset along X) and writes one IGES file
/// @param path output IGES file path
/// @param builders per-case builder functions (called with the X offset)
void WriteCases(const std::string& path,
                const std::vector<std::function<ent_vec(double)>>& builders) {
    igesio::IgesData iges_data;
    double cx = 0.;
    for (const auto& build : builders) {
        for (const auto& entity : build(cx)) {
            iges_data.Root().AddEntity(entity);
        }
        cx += 15.;  // spacing between cases
    }

    if (!igesio::WriteIges(iges_data, path)) {
        std::cerr << "Failed to write IGES file: " << path << std::endl;
    } else {
        std::cout << "Wrote " << path << std::endl;
    }
}

/// @brief Main function (creates the edge-case IGES files)
int main() {
    WriteCases("special_surface_of_revolution.igs",
               {SorCone, SorSphere, SorStepped, SorCup, SorPartial, SorTorus});
    WriteCases("special_ruled_surface.igs",
               {RuledTent, RuledCrease, RuledBothCornersReversed, RuledSmooth});
    WriteCases("special_tabulated_cylinder.igs",
               {TabSquare, TabL, TabComposite, TabSmooth});
    return 0;
}
