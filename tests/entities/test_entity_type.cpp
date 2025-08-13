/**
 * @file entities/test_entity_type.cpp
 * @brief entities/entity_type.hのテスト
 * @author Yayoi Habami
 * @date 2025-04-19
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <optional>

#include "igesio/entities/entity_type.h"

namespace {

/// @brief igesio::entities 名前空間のエイリアス
namespace ie = igesio::entities;

/// @brief 数値をエンティティタイプに変換する
/// @param value 数値
/// @return エンティティタイプ
/// @throw std::runtime_error 数値がエンティティタイプに変換できない場合
ie::EntityType N2ET(const int value) {
    auto e_type = ie::ToEntityType(value);

    if (!e_type.has_value()) {
        throw std::runtime_error("Invalid entity type: " + std::to_string(value));
    }
    return e_type.value();
}

}  // namespace


// 正常系
TEST(EntityTypeTest, ToEntityType) {
    EXPECT_EQ(N2ET(0), ie::EntityType::kNull);
    EXPECT_EQ(N2ET(100), ie::EntityType::kCircularArc);
    EXPECT_EQ(N2ET(102), ie::EntityType::kCompositeCurve);
    EXPECT_EQ(N2ET(104), ie::EntityType::kConicArc);
    EXPECT_EQ(N2ET(106), ie::EntityType::kCopiousData);
    EXPECT_EQ(N2ET(108), ie::EntityType::kPlane);
    EXPECT_EQ(N2ET(110), ie::EntityType::kLine);
    EXPECT_EQ(N2ET(112), ie::EntityType::kParametricSplineCurve);
    EXPECT_EQ(N2ET(114), ie::EntityType::kParametricSplineSurface);
    EXPECT_EQ(N2ET(116), ie::EntityType::kPoint);
    EXPECT_EQ(N2ET(118), ie::EntityType::kRuledSurface);
    EXPECT_EQ(N2ET(120), ie::EntityType::kSurfaceOfRevolution);
    EXPECT_EQ(N2ET(122), ie::EntityType::kTabulatedCylinder);
    EXPECT_EQ(N2ET(123), ie::EntityType::kDirection);
    EXPECT_EQ(N2ET(124), ie::EntityType::kTransformationMatrix);
    EXPECT_EQ(N2ET(125), ie::EntityType::kFlash);
    EXPECT_EQ(N2ET(126), ie::EntityType::kRationalBSplineCurve);
    EXPECT_EQ(N2ET(128), ie::EntityType::kRationalBSplineSurface);
    EXPECT_EQ(N2ET(130), ie::EntityType::kOffsetCurve);
    EXPECT_EQ(N2ET(132), ie::EntityType::kConnectPoint);
    EXPECT_EQ(N2ET(134), ie::EntityType::kNode);
    EXPECT_EQ(N2ET(136), ie::EntityType::kFiniteElement);
    EXPECT_EQ(N2ET(138), ie::EntityType::kNodalDisplacementAndRotation);
    EXPECT_EQ(N2ET(140), ie::EntityType::kOffsetSurface);
    EXPECT_EQ(N2ET(141), ie::EntityType::kBoundary);
    EXPECT_EQ(N2ET(142), ie::EntityType::kCurveOnAParametricSurface);
    EXPECT_EQ(N2ET(143), ie::EntityType::kBoundedSurface);
    EXPECT_EQ(N2ET(144), ie::EntityType::kTrimmedSurface);
    EXPECT_EQ(N2ET(146), ie::EntityType::kNodalResults);
    EXPECT_EQ(N2ET(148), ie::EntityType::kElementResults);
    EXPECT_EQ(N2ET(150), ie::EntityType::kBlock);
    EXPECT_EQ(N2ET(152), ie::EntityType::kRightAngularWedge);
    EXPECT_EQ(N2ET(154), ie::EntityType::kRightCircularCylinder);
    EXPECT_EQ(N2ET(156), ie::EntityType::kRightCircularCone);
    EXPECT_EQ(N2ET(158), ie::EntityType::kSphere);
    EXPECT_EQ(N2ET(160), ie::EntityType::kTorus);
    EXPECT_EQ(N2ET(162), ie::EntityType::kSolidOfRevolution);
    EXPECT_EQ(N2ET(164), ie::EntityType::kSolidOfLinearExtrusion);
    EXPECT_EQ(N2ET(168), ie::EntityType::kEllipsoid);
    EXPECT_EQ(N2ET(180), ie::EntityType::kBooleanTree);
    EXPECT_EQ(N2ET(182), ie::EntityType::kSelectedComponent);
    EXPECT_EQ(N2ET(184), ie::EntityType::kSolidAssembly);
    EXPECT_EQ(N2ET(186), ie::EntityType::kManifoldSolidBRepObject);
    EXPECT_EQ(N2ET(190), ie::EntityType::kPlaneSurface);
    EXPECT_EQ(N2ET(192), ie::EntityType::kRightCircularCylinderSurface);
    EXPECT_EQ(N2ET(194), ie::EntityType::kRightCircularConicalSurface);
    EXPECT_EQ(N2ET(196), ie::EntityType::kSphericalSurface);
    EXPECT_EQ(N2ET(198), ie::EntityType::kToroidalSurface);
    EXPECT_EQ(N2ET(202), ie::EntityType::kAngularDimension);
    EXPECT_EQ(N2ET(204), ie::EntityType::kCurveDimension);
    EXPECT_EQ(N2ET(206), ie::EntityType::kDiameterDimension);
    EXPECT_EQ(N2ET(208), ie::EntityType::kFlagNote);
    EXPECT_EQ(N2ET(210), ie::EntityType::kGeneralLabel);
    EXPECT_EQ(N2ET(212), ie::EntityType::kGeneralNote);
    EXPECT_EQ(N2ET(213), ie::EntityType::kNewGeneralNote);
    EXPECT_EQ(N2ET(214), ie::EntityType::kLeaderArrow);
    EXPECT_EQ(N2ET(216), ie::EntityType::kLinearDimension);
    EXPECT_EQ(N2ET(218), ie::EntityType::kOrdinateDimension);
    EXPECT_EQ(N2ET(220), ie::EntityType::kPointDimension);
    EXPECT_EQ(N2ET(222), ie::EntityType::kRadiusDimension);
    EXPECT_EQ(N2ET(228), ie::EntityType::kGeneralSymbol);
    EXPECT_EQ(N2ET(230), ie::EntityType::kSectionedArea);
    EXPECT_EQ(N2ET(302), ie::EntityType::kAssociativityDefinition);
    EXPECT_EQ(N2ET(304), ie::EntityType::kLineFontDefinition);
    EXPECT_EQ(N2ET(306), ie::EntityType::kMacroDefinition);
    EXPECT_EQ(N2ET(308), ie::EntityType::kSubfigureDefinition);
    EXPECT_EQ(N2ET(310), ie::EntityType::kTextFont);
    EXPECT_EQ(N2ET(312), ie::EntityType::kTextDisplayTemplate);
    EXPECT_EQ(N2ET(314), ie::EntityType::kColorDefinition);
    EXPECT_EQ(N2ET(316), ie::EntityType::kUnitsData);
    EXPECT_EQ(N2ET(320), ie::EntityType::kNetworkSubfigureDefinition);
    EXPECT_EQ(N2ET(322), ie::EntityType::kAttributeTableDefinition);
    EXPECT_EQ(N2ET(402), ie::EntityType::kAssociativityInstance);
    EXPECT_EQ(N2ET(404), ie::EntityType::kDrawing);
    EXPECT_EQ(N2ET(406), ie::EntityType::kProperty);
    EXPECT_EQ(N2ET(408), ie::EntityType::kSingularSubfigureInstance);
    EXPECT_EQ(N2ET(410), ie::EntityType::kView);
    EXPECT_EQ(N2ET(412), ie::EntityType::kRectangularArraySubfigureInstance);
    EXPECT_EQ(N2ET(414), ie::EntityType::kCircularArraySubfigureInstance);
    EXPECT_EQ(N2ET(416), ie::EntityType::kExternalReference);
    EXPECT_EQ(N2ET(418), ie::EntityType::kNodalLoadConstraint);
    EXPECT_EQ(N2ET(420), ie::EntityType::kNetworkSubfigureInstance);
    EXPECT_EQ(N2ET(422), ie::EntityType::kAttributeTableInstance);
    EXPECT_EQ(N2ET(430), ie::EntityType::kSolidInstance);
    EXPECT_EQ(N2ET(502), ie::EntityType::kVertex);
    EXPECT_EQ(N2ET(504), ie::EntityType::kEdge);
    EXPECT_EQ(N2ET(508), ie::EntityType::kLoop);
    EXPECT_EQ(N2ET(510), ie::EntityType::kFace);
    EXPECT_EQ(N2ET(514), ie::EntityType::kShell);
}

// 異常系
TEST(EntityTypeTest, ToEntityTypeInvalid) {
    EXPECT_FALSE(ie::ToEntityType(-1).has_value());
    EXPECT_FALSE(ie::ToEntityType(1).has_value());
    EXPECT_FALSE(ie::ToEntityType(350).has_value());
    EXPECT_FALSE(ie::ToEntityType(500).has_value());
    EXPECT_FALSE(ie::ToEntityType(515).has_value());
}



// ToStringのテスト
TEST(EntityTypeTest, ToString) {
    // すべてのEnum値に対して、先頭のkを除き、大文字の前にスペースを入れた
    // 文字列が返されることを確認する。ただし、以下のルールに従う
    //   - "Of", "On", "And"は小文字にする
    //   - "B Rep"は"B-Rep"に、"B Spline"は"B-Spline"に変換する
    auto TS = [](const ie::EntityType type) {
        return ie::ToString(type);
    };
    using ET = ie::EntityType;

    EXPECT_EQ(TS(ET::kNull), "Null");
    EXPECT_EQ(TS(ET::kCircularArc), "Circular Arc");
    EXPECT_EQ(TS(ET::kCompositeCurve), "Composite Curve");
    EXPECT_EQ(TS(ET::kConicArc), "Conic Arc");
    EXPECT_EQ(TS(ET::kCopiousData), "Copious Data");
    EXPECT_EQ(TS(ET::kPlane), "Plane");
    EXPECT_EQ(TS(ET::kLine), "Line");
    EXPECT_EQ(TS(ET::kParametricSplineCurve), "Parametric Spline Curve");
    EXPECT_EQ(TS(ET::kParametricSplineSurface), "Parametric Spline Surface");
    EXPECT_EQ(TS(ET::kPoint), "Point");
    EXPECT_EQ(TS(ET::kRuledSurface), "Ruled Surface");
    EXPECT_EQ(TS(ET::kSurfaceOfRevolution), "Surface of Revolution");
    EXPECT_EQ(TS(ET::kTabulatedCylinder), "Tabulated Cylinder");
    EXPECT_EQ(TS(ET::kDirection), "Direction");
    EXPECT_EQ(TS(ET::kTransformationMatrix), "Transformation Matrix");
    EXPECT_EQ(TS(ET::kFlash), "Flash");
    EXPECT_EQ(TS(ET::kRationalBSplineCurve), "Rational B-Spline Curve");
    EXPECT_EQ(TS(ET::kRationalBSplineSurface), "Rational B-Spline Surface");
    EXPECT_EQ(TS(ET::kOffsetCurve), "Offset Curve");
    EXPECT_EQ(TS(ET::kConnectPoint), "Connect Point");
    EXPECT_EQ(TS(ET::kNode), "Node");
    EXPECT_EQ(TS(ET::kFiniteElement), "Finite Element");
    EXPECT_EQ(TS(ET::kNodalDisplacementAndRotation), "Nodal Displacement and Rotation");
    EXPECT_EQ(TS(ET::kOffsetSurface), "Offset Surface");
    EXPECT_EQ(TS(ET::kBoundary), "Boundary");
    EXPECT_EQ(TS(ET::kCurveOnAParametricSurface), "Curve on a Parametric Surface");
    EXPECT_EQ(TS(ET::kBoundedSurface), "Bounded Surface");
    EXPECT_EQ(TS(ET::kTrimmedSurface), "Trimmed Surface");
    EXPECT_EQ(TS(ET::kNodalResults), "Nodal Results");
    EXPECT_EQ(TS(ET::kElementResults), "Element Results");
    EXPECT_EQ(TS(ET::kBlock), "Block");
    EXPECT_EQ(TS(ET::kRightAngularWedge), "Right Angular Wedge");
    EXPECT_EQ(TS(ET::kRightCircularCylinder), "Right Circular Cylinder");
    EXPECT_EQ(TS(ET::kRightCircularCone), "Right Circular Cone");
    EXPECT_EQ(TS(ET::kSphere), "Sphere");
    EXPECT_EQ(TS(ET::kTorus), "Torus");
    EXPECT_EQ(TS(ET::kSolidOfRevolution), "Solid of Revolution");
    EXPECT_EQ(TS(ET::kSolidOfLinearExtrusion), "Solid of Linear Extrusion");
    EXPECT_EQ(TS(ET::kEllipsoid), "Ellipsoid");
    EXPECT_EQ(TS(ET::kBooleanTree), "Boolean Tree");
    EXPECT_EQ(TS(ET::kSelectedComponent), "Selected Component");
    EXPECT_EQ(TS(ET::kSolidAssembly), "Solid Assembly");
    EXPECT_EQ(TS(ET::kManifoldSolidBRepObject), "Manifold Solid B-Rep Object");
    EXPECT_EQ(TS(ET::kPlaneSurface), "Plane Surface");
    EXPECT_EQ(TS(ET::kRightCircularCylinderSurface), "Right Circular Cylinder Surface");
    EXPECT_EQ(TS(ET::kRightCircularConicalSurface), "Right Circular Conical Surface");
    EXPECT_EQ(TS(ET::kSphericalSurface), "Spherical Surface");
    EXPECT_EQ(TS(ET::kToroidalSurface), "Toroidal Surface");
    EXPECT_EQ(TS(ET::kAngularDimension), "Angular Dimension");
    EXPECT_EQ(TS(ET::kCurveDimension), "Curve Dimension");
    EXPECT_EQ(TS(ET::kDiameterDimension), "Diameter Dimension");
    EXPECT_EQ(TS(ET::kFlagNote), "Flag Note");
    EXPECT_EQ(TS(ET::kGeneralLabel), "General Label");
    EXPECT_EQ(TS(ET::kGeneralNote), "General Note");
    EXPECT_EQ(TS(ET::kNewGeneralNote), "New General Note");
    EXPECT_EQ(TS(ET::kLeaderArrow), "Leader Arrow");
    EXPECT_EQ(TS(ET::kLinearDimension), "Linear Dimension");
    EXPECT_EQ(TS(ET::kOrdinateDimension), "Ordinate Dimension");
    EXPECT_EQ(TS(ET::kPointDimension), "Point Dimension");
    EXPECT_EQ(TS(ET::kRadiusDimension), "Radius Dimension");
    EXPECT_EQ(TS(ET::kGeneralSymbol), "General Symbol");
    EXPECT_EQ(TS(ET::kSectionedArea), "Sectioned Area");
    EXPECT_EQ(TS(ET::kAssociativityDefinition), "Associativity Definition");
    EXPECT_EQ(TS(ET::kLineFontDefinition), "Line Font Definition");
    EXPECT_EQ(TS(ET::kMacroDefinition), "Macro Definition");
    EXPECT_EQ(TS(ET::kSubfigureDefinition), "Subfigure Definition");
    EXPECT_EQ(TS(ET::kTextFont), "Text Font");
    EXPECT_EQ(TS(ET::kTextDisplayTemplate), "Text Display Template");
    EXPECT_EQ(TS(ET::kColorDefinition), "Color Definition");
    EXPECT_EQ(TS(ET::kUnitsData), "Units Data");
    EXPECT_EQ(TS(ET::kNetworkSubfigureDefinition), "Network Subfigure Definition");
    EXPECT_EQ(TS(ET::kAttributeTableDefinition), "Attribute Table Definition");
    EXPECT_EQ(TS(ET::kAssociativityInstance), "Associativity Instance");
    EXPECT_EQ(TS(ET::kDrawing), "Drawing");
    EXPECT_EQ(TS(ET::kProperty), "Property");
    EXPECT_EQ(TS(ET::kSingularSubfigureInstance), "Singular Subfigure Instance");
    EXPECT_EQ(TS(ET::kView), "View");
    EXPECT_EQ(TS(ET::kRectangularArraySubfigureInstance), "Rectangular Array Subfigure Instance");
    EXPECT_EQ(TS(ET::kCircularArraySubfigureInstance), "Circular Array Subfigure Instance");
    EXPECT_EQ(TS(ET::kExternalReference), "External Reference");
    EXPECT_EQ(TS(ET::kNodalLoadConstraint), "Nodal Load Constraint");
    EXPECT_EQ(TS(ET::kNetworkSubfigureInstance), "Network Subfigure Instance");
    EXPECT_EQ(TS(ET::kAttributeTableInstance), "Attribute Table Instance");
    EXPECT_EQ(TS(ET::kSolidInstance), "Solid Instance");
    EXPECT_EQ(TS(ET::kVertex), "Vertex");
    EXPECT_EQ(TS(ET::kEdge), "Edge");
    EXPECT_EQ(TS(ET::kLoop), "Loop");
    EXPECT_EQ(TS(ET::kFace), "Face");
    EXPECT_EQ(TS(ET::kShell), "Shell");
}
