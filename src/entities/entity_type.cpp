/**
 * @file entities/entity_type.cpp
 * @brief IGESで定義されるエンティティタイプの列挙型
 * @author Yayoi Habami
 * @date 2025-04-10
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/entity_type.h"

#include <string>



std::optional<igesio::entities::EntityType>
igesio::entities::ToEntityType(const int n) {
    // 0の場合はEntityType::kNullを返す
    if (n == 0) return EntityType::kNull;

    // 有効な奇数の場合はEntityTypeを返す
    if (n == 123 || n == 125 || n == 141 || n == 143 || n == 213) return static_cast<EntityType>(n);
    // それ以外の奇数はstd::nulloptを返す
    if (n % 2 != 0) return std::nullopt;

    // 有効な値の場合はEntityTypeを返す
    if ((n >= 100 && n <= 168) || (n >= 180 && n <= 186) ||
        (n >= 190 && n <= 198) || (n >= 202 && n <= 222) ||
        (n >= 302 && n <= 316) || (n >= 402 && n <= 422) ||
        n == 228 || n == 230 || n == 320 || n == 322 || n == 430 ||
        n == 502 || n == 504 || n == 508 || n == 510 || n == 514) {
        return static_cast<EntityType>(n);
    }

    return std::nullopt;
}



std::string igesio::entities::ToString(const EntityType type) {
    switch (type) {
        case EntityType::kNull:
            return "Null";
        case EntityType::kCircularArc:
            return "Circular Arc";
        case EntityType::kCompositeCurve:
            return "Composite Curve";
        case EntityType::kConicArc:
            return "Conic Arc";
        case EntityType::kCopiousData:
            return "Copious Data";
        case EntityType::kPlane:
            return "Plane";
        case EntityType::kLine:
            return "Line";
        case EntityType::kParametricSplineCurve:
            return "Parametric Spline Curve";
        case EntityType::kParametricSplineSurface:
            return "Parametric Spline Surface";
        case EntityType::kPoint:
            return "Point";
        case EntityType::kRuledSurface:
            return "Ruled Surface";
        case EntityType::kSurfaceOfRevolution:
            return "Surface of Revolution";
        case EntityType::kTabulatedCylinder:
            return "Tabulated Cylinder";
        case EntityType::kDirection:
            return "Direction";
        case EntityType::kTransformationMatrix:
            return "Transformation Matrix";
        case EntityType::kFlash:
            return "Flash";
        case EntityType::kRationalBSplineCurve:
            return "Rational B-Spline Curve";
        case EntityType::kRationalBSplineSurface:
            return "Rational B-Spline Surface";
        case EntityType::kOffsetCurve:
            return "Offset Curve";
        case EntityType::kConnectPoint:
            return "Connect Point";
        case EntityType::kNode:
            return "Node";
        case EntityType::kFiniteElement:
            return "Finite Element";
        case EntityType::kNodalDisplacementAndRotation:
            return "Nodal Displacement and Rotation";
        case EntityType::kOffsetSurface:
            return "Offset Surface";
        case EntityType::kBoundary:
            return "Boundary";
        case EntityType::kCurveOnAParametricSurface:
            return "Curve on a Parametric Surface";
        case EntityType::kBoundedSurface:
            return "Bounded Surface";
        case EntityType::kTrimmedSurface:
            return "Trimmed Surface";
        case EntityType::kNodalResults:
            return "Nodal Results";
        case EntityType::kElementResults:
            return "Element Results";
        case EntityType::kBlock:
            return "Block";
        case EntityType::kRightAngularWedge:
            return "Right Angular Wedge";
        case EntityType::kRightCircularCylinder:
            return "Right Circular Cylinder";
        case EntityType::kRightCircularCone:
            return "Right Circular Cone";
        case EntityType::kSphere:
            return "Sphere";
        case EntityType::kTorus:
            return "Torus";
        case EntityType::kSolidOfRevolution:
            return "Solid of Revolution";
        case EntityType::kSolidOfLinearExtrusion:
            return "Solid of Linear Extrusion";
        case EntityType::kEllipsoid:
            return "Ellipsoid";
        case EntityType::kBooleanTree:
            return "Boolean Tree";
        case EntityType::kSelectedComponent:
            return "Selected Component";
        case EntityType::kSolidAssembly:
            return "Solid Assembly";
        case EntityType::kManifoldSolidBRepObject:
            return "Manifold Solid B-Rep Object";
        case EntityType::kPlaneSurface:
            return "Plane Surface";
        case EntityType::kRightCircularCylinderSurface:
            return "Right Circular Cylinder Surface";
        case EntityType::kRightCircularConicalSurface:
            return "Right Circular Conical Surface";
        case EntityType::kSphericalSurface:
            return "Spherical Surface";
        case EntityType::kToroidalSurface:
            return "Toroidal Surface";
        case EntityType::kAngularDimension:
            return "Angular Dimension";
        case EntityType::kCurveDimension:
            return "Curve Dimension";
        case EntityType::kDiameterDimension:
            return "Diameter Dimension";
        case EntityType::kFlagNote:
            return "Flag Note";
        case EntityType::kGeneralLabel:
            return "General Label";
        case EntityType::kGeneralNote:
            return "General Note";
        case EntityType::kNewGeneralNote:
            return "New General Note";
        case EntityType::kLeaderArrow:
            return "Leader Arrow";
        case EntityType::kLinearDimension:
            return "Linear Dimension";
        case EntityType::kOrdinateDimension:
            return "Ordinate Dimension";
        case EntityType::kPointDimension:
            return "Point Dimension";
        case EntityType::kRadiusDimension:
            return "Radius Dimension";
        case EntityType::kGeneralSymbol:
            return "General Symbol";
        case EntityType::kSectionedArea:
            return "Sectioned Area";
        case EntityType::kAssociativityDefinition:
            return "Associativity Definition";
        case EntityType::kLineFontDefinition:
            return "Line Font Definition";
        case EntityType::kMacroDefinition:
            return "Macro Definition";
        case EntityType::kSubfigureDefinition:
            return "Subfigure Definition";
        case EntityType::kTextFont:
            return "Text Font";
        case EntityType::kTextDisplayTemplate:
            return "Text Display Template";
        case EntityType::kColorDefinition:
            return "Color Definition";
        case EntityType::kUnitsData:
            return "Units Data";
        case EntityType::kNetworkSubfigureDefinition:
            return "Network Subfigure Definition";
        case EntityType::kAttributeTableDefinition:
            return "Attribute Table Definition";
        case EntityType::kAssociativityInstance:
            return "Associativity Instance";
        case EntityType::kDrawing:
            return "Drawing";
        case EntityType::kProperty:
            return "Property";
        case EntityType::kSingularSubfigureInstance:
            return "Singular Subfigure Instance";
        case EntityType::kView:
            return "View";
        case EntityType::kRectangularArraySubfigureInstance:
            return "Rectangular Array Subfigure Instance";
        case EntityType::kCircularArraySubfigureInstance:
            return "Circular Array Subfigure Instance";
        case EntityType::kExternalReference:
            return "External Reference";
        case EntityType::kNodalLoadConstraint:
            return "Nodal Load Constraint";
        case EntityType::kNetworkSubfigureInstance:
            return "Network Subfigure Instance";
        case EntityType::kAttributeTableInstance:
            return "Attribute Table Instance";
        case EntityType::kSolidInstance:
            return "Solid Instance";
        case EntityType::kVertex:
            return "Vertex";
        case EntityType::kEdge:
            return "Edge";
        case EntityType::kLoop:
            return "Loop";
        case EntityType::kFace:
            return "Face";
        case EntityType::kShell:
            return "Shell";
        default:
            return "Unknown";
    }
}
