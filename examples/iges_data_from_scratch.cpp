/**
 * @file examples/iges_data_from_scratch.cpp
 * @brief This example demonstrates how to create an IGES data structure
 *        from scratch by manually constructing various IGES entities,
 *        including circular arcs (simple curves), composite curves (entity with pointers),
 *        transformation matrices, and color definitions (Structures).
 *        It showcases the process of adding these entities to an IgesData
 *        object and performing basic operations such as calculating parameter ranges,
 *        overwriting colors, and validating the data structure.
 *        The example also illustrates how to access child entities
 *        and perform calculations (normal and tangent calculations on circular arcs).
 * @author Yayoi Habami
 * @date 2025-08-03
 * @copyright 2025 Yayoi Habami
 */
#include <iostream>
#include <memory>

#include <igesio/entities/curves/circular_arc.h>
#include <igesio/entities/curves/composite_curve.h>
#include <igesio/entities/transformations/transformation_matrix.h>
#include <igesio/entities/structures/color_definition.h>
#include <igesio/models/iges_data.h>

namespace {

using IgesData = igesio::models::IgesData;
using Vector2d = igesio::Vector2d;
using CircularArc = igesio::entities::CircularArc;
using CompositeCurve = igesio::entities::CompositeCurve;
using TransMatrix = igesio::entities::TransformationMatrix;

}  // namespace



int main() {
    // Create a Composite Curve entity with:
    // - Arc with center (0, 0), start point (1, 0), and end point (0, 1)
    // - Arc with center (0, 1.5), start point (0, 1), and end point (-0.5, 1.5)
    // - Arc with center (1.5, 1.5), start point (-0.5, 1.5), and end point (3.5, 1.5)
    auto curve1 = std::make_shared<CircularArc>(
            Vector2d{0.0, 0.0}, Vector2d{1.0, 0.0}, Vector2d{0.0, 1.0});
    auto curve2 = std::make_shared<CircularArc>(
            Vector2d{0.0, 1.5}, Vector2d{0.0, 1.0}, Vector2d{-0.5, 1.5});
    auto curve3 = std::make_shared<CircularArc>(
            Vector2d{1.5, 1.5}, Vector2d{-0.5, 1.5}, Vector2d{3.5, 1.5});
    auto composite_curve = std::make_shared<CompositeCurve>();
    composite_curve->AddCurve(curve1);
    composite_curve->AddCurve(curve2);
    composite_curve->AddCurve(curve3);
    auto [start_1, end_1] = curve1->GetParameterRange();
    auto [start_2, end_2] = curve2->GetParameterRange();
    auto [start_3, end_3] = curve3->GetParameterRange();
    auto [start_comp, end_comp] = composite_curve->GetParameterRange();
    std::cout << "Composite Curve:" << std::endl << "  Parameter ranges: " << std::endl
              << "    Curve1 range: [" << start_1 << ", " << end_1 << "], " << std::endl
              << "    Curve2 range: [" << start_2 << ", " << end_2 << "], " << std::endl
              << "    Curve3 range: [" << start_3 << ", " << end_3 << "], " << std::endl
              << "    CompositeCurve range: [" << start_comp << ", " << end_comp
                                               << "]" << std::endl;

    // Create a Color Definition entity (≈ #7FFF4C)
    // and overwrite the color of the Composite Curve
    auto color_def = std::make_shared<igesio::entities::ColorDefinition>(
            std::array<double, 3>{50.0, 100.0, 30.0}, "Light Green");
    composite_curve->OverwriteColor(color_def);
    std::cout << "  The 2nd curve ID (from TryGet): "
              << composite_curve->GetChildEntity(curve2->GetID())->GetID() << std::endl;


    // Create a Transformation Matrix entity
    // (zero rotation and zero translation)
    auto transformation = std::make_shared<TransMatrix>(
            igesio::Matrix3d::Identity(), igesio::Vector3d::Zero(), 0);
    auto trans_params = transformation->GetParameters();
    std::cout << std::endl << "TransformationMatrix parameters: " << trans_params << std::endl;

    // Create Circular Arc entities and demonstrate their normal and tangent calculations
    // - Arc with center (0, 0), start point (1, 0), end point (0, 1)
    // - Circle with center (0, 0) and radius 1
    auto arc = std::make_shared<CircularArc>(
            Vector2d{0.0, 0.0}, Vector2d{1.0, 0.0}, Vector2d{0.0, 1.0}, 0.0);
    auto circle = std::make_shared<CircularArc>(
            Vector2d{0.0, 0.0}, 1.0, 0.0);
    auto arc_norm = arc->GetNormalAt(1.5);
    auto arc_tangent = arc->GetTangentAt(1.5);
    auto dot = arc_norm.dot(arc_tangent);
    std::cout << "Arc Parameters" << std::endl
              << "  Normal at t=1.5: " << arc_norm << std::endl
              << "  Tangent at t=1.5: " << arc_tangent << std::endl
              << "  Dot product: " << dot << std::endl;



    // Create an IgesData object and add all entities
    IgesData iges_data;
    iges_data.AddEntity(transformation);
    iges_data.AddEntity(arc);
    iges_data.AddEntity(circle);
    iges_data.AddEntity(curve1);
    iges_data.AddEntity(curve2);
    iges_data.AddEntity(curve3);
    iges_data.AddEntity(composite_curve);
    iges_data.AddEntity(color_def);

    std::cout << std::endl << "Total entities added: "
              << iges_data.GetEntities().size() << std::endl;
    auto is_ready = iges_data.IsReady();
    std::cout << "iges_data is ready: " << (is_ready ? "true" : "false") << std::endl;
    if (!is_ready) {
        auto result = iges_data.Validate();
        std::cout << "Validation errors: " << result.Message() << std::endl;
    }

    return 0;
}
