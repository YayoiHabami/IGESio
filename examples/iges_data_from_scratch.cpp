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
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

#include <igesio/igesio.h>

namespace {

using IgesData = igesio::models::IgesData;
namespace i_ent = igesio::entities;

}  // namespace



int main() {
    // Create a Composite Curve entity with:
    // - Arc with center (0, 0), start point (1, 0), and end point (0, 1)
    // - Arc with center (0, 1.5), start point (0, 1), and end point (-0.5, 1.5)
    // - Arc with center (1.5, 1.5), start point (-0.5, 1.5), and end point (3.5, 1.5)
    auto curve1 = i_ent::MakeCircularArc(
            igesio::Vector2d{0.0, 0.0}, igesio::Vector2d{1.0, 0.0},
            igesio::Vector2d{0.0, 1.0});
    auto curve2 = i_ent::MakeCircularArc(
            igesio::Vector2d{0.0, 1.5}, igesio::Vector2d{0.0, 1.0},
            igesio::Vector2d{-0.5, 1.5});
    auto curve3 = i_ent::MakeCircularArc(
            igesio::Vector2d{1.5, 1.5}, igesio::Vector2d{-0.5, 1.5},
            igesio::Vector2d{3.5, 1.5});
    auto composite_curve = i_ent::MakeCompositeCurve({curve1, curve2, curve3});
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

    // Create a Color Definition entity (#7FFF4C)
    // and overwrite the color of the Composite Curve
    auto color_def = i_ent::MakeColorDefinitionFromHex("#7FFF4C", "Light Green");
    composite_curve->OverwriteColor(color_def);
    std::cout << "  The 2nd curve ID (from TryGet): "
              << composite_curve->GetChildEntity(curve2->GetID())->GetID() << std::endl;



    // Create Circular Arc entities and demonstrate their normal and tangent calculations
    // - Arc with center (0, 0), start point (2, 0), end point (0, 2)
    // - Circle with center (1, 0) and radius 1 (z = -1)
    auto arc = i_ent::MakeCircularArc(
            igesio::Vector2d{0.0, 0.0}, igesio::Vector2d{2.0, 0.0},
            igesio::Vector2d{0.0, 2.0}, 0.0);
    auto circle = i_ent::MakeCircle(igesio::Vector2d{1.0, 0.0}, 1.0, -1.0);
    auto arc_norm = arc->GetNormalAt(1.5);
    auto arc_tangent = arc->GetTangentAt(1.5);
    auto dot = arc_norm.dot(arc_tangent);
    std::cout << "Arc Parameters" << std::endl
              << "  Normal at t=1.5: " << arc_norm << std::endl
              << "  Tangent at t=1.5: " << arc_tangent << std::endl
              << "  Dot product: " << dot << std::endl;

    // Create a Transformation Matrix entity
    // (90-degree rotation around Y-axis and translation (0, 0, 1))
    auto rotation = igesio::AngleAxisd(igesio::kPi/2, igesio::Vector3d::UnitY());
    auto transformation = i_ent::MakeTransformationMatrix(
            rotation.toRotationMatrix(), igesio::Vector3d{0, 0, 1});
    auto trans_params = transformation->GetParameters();
    std::cout << std::endl << "TransformationMatrix parameters: " << trans_params << std::endl;
    // Transform arc
    arc->OverwriteTransformationMatrix(transformation);


    // Create an IgesData object and add all entities
    igesio::IgesData iges_data;
    iges_data.Root().AddEntity(transformation);
    iges_data.Root().AddEntity(arc);
    iges_data.Root().AddEntity(circle);
    iges_data.Root().AddEntity(curve1);
    iges_data.Root().AddEntity(curve2);
    iges_data.Root().AddEntity(curve3);
    iges_data.Root().AddEntity(composite_curve);
    iges_data.Root().AddEntity(color_def);

    std::cout << std::endl << "Total entities added: "
              << iges_data.Root().GetEntities().size() << std::endl;
    auto is_ready = iges_data.Root().IsReady();
    std::cout << "iges_data is ready: " << (is_ready ? "true" : "false") << std::endl;
    if (!is_ready) {
        auto result = iges_data.Root().Validate();
        std::cout << "Validation errors: " << result.Message() << std::endl;
    } else {
        // Write the IGES data to a file
        auto cwd = std::filesystem::current_path();
        auto output_path = cwd / "from_scratch.iges";
        std::cout << "Writing IGES file to: " << output_path << std::endl;
        auto success = igesio::WriteIges(iges_data, output_path.string());
        std::cout << "Write success: " << (success ? "true" : "false") << std::endl;
    }

    return 0;
}
