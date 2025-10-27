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

#include <igesio/common/versions.h>
#include <igesio/entities/curves/circular_arc.h>
#include <igesio/entities/curves/composite_curve.h>
#include <igesio/entities/curves/conic_arc.h>
#include <igesio/entities/curves/copious_data.h>
#include <igesio/entities/curves/linear_path.h>
#include <igesio/entities/curves/line.h>
#include <igesio/entities/curves/parametric_spline_curve.h>
#include <igesio/entities/curves/point.h>
#include <igesio/entities/curves/rational_b_spline_curve.h>
#include <igesio/entities/structures/color_definition.h>
#include <igesio/entities/transformations/transformation_matrix.h>
#include <igesio/writer.h>

namespace i_ent = igesio::entities;
namespace i_mod = igesio::models;
using igesio::kPi;
using igesio::Vector2d;
using igesio::Vector3d;
using ent_vec = std::vector<std::shared_ptr<igesio::entities::EntityBase>>;



/// @brief Example for Circular Arc entity (Type 100)
/// @note 1. Circle: center (-0.75, 0), radius 1
///       2. Arc: center (0, 0), radius 1, start angle 4π/3, end angle 5π/2
ent_vec CreateCircularArc() {
    double x_diff = 1.25;
    auto circle = std::make_shared<i_ent::CircularArc>(
            Vector2d{-x_diff, 0.0}, 1.0);

    auto arc_start = Vector2d{cos(4.0 * kPi / 3.0), sin(4.0 * kPi / 3.0)};
    auto arc_end = Vector2d{cos(5.0 * kPi / 2.0), sin(5.0 * kPi / 2.0)};
    auto arc = std::make_shared<i_ent::CircularArc>(
            Vector2d{0.0, 0.0}, arc_start, arc_end);
    auto arc_trans = std::make_shared<i_ent::TransformationMatrix>(
            igesio::Matrix3d::Identity(), Vector3d{x_diff, 0.0, 0.0});
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
    auto comp_1_trans = std::make_shared<i_ent::TransformationMatrix>(
            igesio::AngleAxisd(kPi, Vector3d::UnitY()), Vector3d{0.5, -1.0, 0.0});
    auto comp_1 = std::make_shared<i_ent::CircularArc>(
            Vector2d{0.0, 0.0}, Vector2d{-1.5, 0.0}, Vector2d{1.5, 0.0});
    comp_1->OverwriteTransformationMatrix(comp_1_trans);

    // 2. line
    auto comp_2 = std::make_shared<i_ent::Line>(
            Vector3d{-1.0, -1.0, 0.0}, Vector3d{1.0, 1.0, 0.0});

    // 3. circular arc
    auto comp_3 = std::make_shared<i_ent::CircularArc>(
            Vector2d{-0.5, 1.0}, Vector2d{1.0, 1.0}, Vector2d{-2.0, 1.0});

    // Composite curve
    auto comp_curve = std::make_shared<i_ent::CompositeCurve>();
    comp_curve->AddCurve(comp_1);
    comp_curve->AddCurve(comp_2);
    comp_curve->AddCurve(comp_3);

    return {comp_1_trans, comp_1, comp_2, comp_3, comp_curve};
}

/// @brief Example for Conic Arc entity (Type 104)
/// @note 1. Ellipse arc: center (0, 3), axis (x, y) = (3, 2),
///          start angle 7π/4, end angle 17π/6
ent_vec CreateConicArc() {
    // 1. ellipse arc
    auto ellipse_arc = std::make_shared<i_ent::ConicArc>(
            std::pair<double, double>{-3.0, 2.0}, 7.0 * kPi / 4.0, 17.0 * kPi / 6.0);

    // Note: Since elliptical arc entities are defined with the origin
    // as their center, use a transformation matrix entity to move the origin.
    auto ellipse_trans = std::make_shared<i_ent::TransformationMatrix>(
            igesio::Matrix3d::Identity(), Vector3d{0.0, 3.0, 0.0});
    ellipse_arc->OverwriteTransformationMatrix(ellipse_trans);

    return {ellipse_trans, ellipse_arc};
}

/// @brief Example for Copious Data entity (Type 106)
/// @note 1. Points: (3,0,1), (2,1,-1), (2,2,0), (0,3,1), (-1,2,0)
///       2. Polyline: (8,0,1), (7,1,-1), (7,2,0), (5,3,1), (4,2,0)
ent_vec CreateCopiousData() {
    // 1. Points
    igesio::Matrix3Xd copious_coords(3, 5);
    copious_coords << 3.0,  2.0, 2.0, 0.0, -1.0,
                      0.0,  1.0, 2.0, 3.0,  2.0,
                      1.0, -1.0, 0.0, 1.0,  0.0;
    auto copious = std::make_shared<i_ent::CopiousData>(
            i_ent::CopiousDataType::kPoints3D, copious_coords);

    // 2. Polyline with transformation
    auto copious_trans = std::make_shared<i_ent::TransformationMatrix>(
            igesio::Matrix3d::Identity(), Vector3d{5.0, 0.0, 0.0});
    auto linear_path = std::make_shared<i_ent::LinearPath>(
            i_ent::CopiousDataType::kPolyline3D, copious_coords);
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
    auto end = Vector3d{1.0, 1.0, 0.0};
    auto line_segment = std::make_shared<i_ent::Line>(
            start, end, i_ent::LineType::kSegment);

    // 2. semi-infinite line
    auto ray_trans = std::make_shared<i_ent::TransformationMatrix>(
            igesio::Matrix3d::Identity(), Vector3d{2.0, 0, 0.0});
    auto ray = std::make_shared<i_ent::Line>(
            start, end, i_ent::LineType::kRay);
    ray->OverwriteTransformationMatrix(ray_trans);

    // 3. infinite line
    auto line_trans = std::make_shared<i_ent::TransformationMatrix>(
            igesio::Matrix3d::Identity(), Vector3d{4.0, 0, 0.0});
    auto line = std::make_shared<i_ent::Line>(
            start, end, i_ent::LineType::kLine);
    line->OverwriteTransformationMatrix(line_trans);

    return {line_segment, ray_trans, ray, line_trans, line};
}

/// @brief Example for Parametric Spline Curve entity (Type 112)
/// @note Creates a spline curve with specified parameters
ent_vec CreateParametricSplineCurve() {
    // Create spline curve
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
    auto spline_c = std::make_shared<i_ent::ParametricSplineCurve>(param);
    spline_c->OverwriteColor(i_ent::ColorNumber::kBlue);
    return {spline_c};
}

/// @brief Example for Point entity (Type 116)
/// @note Creates a point at (1, 2, 3)
ent_vec CreatePoint() {
    auto point = std::make_shared<i_ent::Point>(Vector3d{1.0, 2.0, 3.0});
    point->OverwriteColor(i_ent::ColorNumber::kMagenta);
    return {point};
}

/// @brief Example for Rational B-Spline Curve entity (Type 126)
/// @note Creates a NURBS curve with specified parameters
ent_vec CreateRationalBSplineCurve() {
    // Create NURBS curve
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
    auto nurbs_c = std::make_shared<i_ent::RationalBSplineCurve>(param);

    return {nurbs_c};
}



/// @brief Main function (creates IGES data and writes to file)
int main() {
    i_mod::IgesData iges_data;

    for (const auto& entity : CreateCircularArc()) {
        iges_data.AddEntity(entity);
    }
    for (const auto& entity : CreateCompositeCurve()) {
        iges_data.AddEntity(entity);
    }
    for (const auto& entity : CreateConicArc()) {
        iges_data.AddEntity(entity);
    }
    for (const auto& entity : CreateCopiousData()) {
        iges_data.AddEntity(entity);
    }
    for (const auto& entity : CreateLine()) {
        iges_data.AddEntity(entity);
    }
    for (const auto& entity : CreateParametricSplineCurve()) {
        iges_data.AddEntity(entity);
    }
    for (const auto& entity : CreatePoint()) {
        iges_data.AddEntity(entity);
    }
    for (const auto& entity : CreateRationalBSplineCurve()) {
        iges_data.AddEntity(entity);
    }

    // Write to IGES file
    auto success = igesio::WriteIges(iges_data, "sample_curves.igs");
    if (!success) {
        std::cerr << "Failed to write IGES file." << std::endl;
        return 1;
    }

    return 0;
}
