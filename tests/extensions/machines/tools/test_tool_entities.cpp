/**
 * @file tests/extensions/machines/tools/test_tool_entities.cpp
 * @brief machines拡張の工具輪郭のエンティティ化 (tools/tool_entities) のテスト
 * @author Yayoi Habami
 * @date 2026-09-11
 * @copyright 2026 Yayoi Habami
 * @note 対象: MakeToolAssembly / FindToolPart / SetToolPartVisible /
 *       ToolAssemblyName / ToolElementName
 *       - Assembly構成: 根・部位容器・要素ノードの名前と数、部位ごとの複数要素
 *       - エンティティ: 種別ごとの数量 (直線のみ→Type 106、円弧を含む→100/110+102)、
 *         従属スイッチ、色定義 (既定色と指定色)、不透明度オーバーライド
 *       - 幾何: 要素ノードの変換 R_x(+90°)、回転体上の点が母線と一致すること、
 *         工具全体のワールドBBが全長・最大半径に一致すること
 *       - 時計回りの円弧 (ネック部): 1面のまま保たれ、IGES出力時に本体の展開機構が
 *         鏡映CCW弧+Type 124に置き換えること
 *       - 異常系: 検証違反の輪郭で例外
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/entities/entity_base.h"
#include "igesio/entities/entity_type.h"
#include "igesio/entities/curves/circular_arc.h"
#include "igesio/entities/curves/composite_curve.h"
#include "igesio/entities/de/raw_entity_de.h"
#include "igesio/entities/structures/color_definition.h"
#include "igesio/entities/surfaces/surface_of_revolution.h"
#include "igesio/models/assembly.h"
#include "igesio/models/iges_data.h"
#include "igesio/writer.h"
#include "igesio/extensions/machines/core/rotation.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/tools/tool_assembly.h"
#include "igesio/extensions/machines/tools/tool_entities.h"
#include "igesio/extensions/machines/tools/tool_profile.h"

namespace {

namespace mc = igesio::extensions::machines;
namespace i_ent = igesio::entities;
namespace i_mod = igesio::models;
using igesio::Vector2d;
using igesio::Vector3d;
using i_ent::EntityType;
using i_ent::SubordinateEntitySwitch;
using mc::ProfileSegment;
using mc::ToolPart;
using mc::ToolProfile;
using mc::ToolProfileElement;

/// @brief 数値比較の許容誤差
constexpr double kTol = 1e-9;

/// @brief 直径10・切れ刃長30・工具長80・突き出し60・ホルダφ40×60のボール工具 (工具番号1)
mc::ToolAssemblySpec BallToolSpec() {
    mc::SimpleToolSpec simple;
    simple.cutter = mc::SimpleToolSpec::Cutter::kBall;
    simple.diameter = 10.0;
    simple.cutting_length = 30.0;
    simple.tool_length = 80.0;
    simple.overhang = 60.0;
    simple.holder_diameter = 40.0;
    simple.holder_length = 60.0;
    mc::ToolAssemblySpec spec;
    spec.number = 1;
    spec.name = "ball";
    spec.profile = mc::MakeSimpleToolProfile(simple, nullptr);  // ゲージライン z = 120
    return spec;
}

/// @brief BallToolSpecと同寸のスクエア工具
mc::ToolAssemblySpec SquareToolSpec() {
    mc::SimpleToolSpec simple;
    simple.cutter = mc::SimpleToolSpec::Cutter::kSquare;
    simple.diameter = 10.0;
    simple.cutting_length = 30.0;
    simple.tool_length = 80.0;
    simple.overhang = 60.0;
    simple.holder_diameter = 40.0;
    simple.holder_length = 60.0;
    mc::ToolAssemblySpec spec = BallToolSpec();
    spec.profile = mc::MakeSimpleToolProfile(simple, nullptr);
    return spec;
}

/// @brief 直線列 (頂点列) の要素を作る
ToolProfileElement PolylineElement(const ToolPart part,
                                   const std::vector<Vector2d>& vertices) {
    ToolProfileElement element;
    element.part = part;
    for (std::size_t i = 0; i + 1 < vertices.size(); ++i) {
        element.segments.push_back(
                ProfileSegment::Line(vertices[i], vertices[i + 1]));
    }
    return element;
}

/// @brief ネック部 (時計回りの凹の円弧) を持つ工具 (工具番号2)
/// @note 切れ刃: スクエア形 半径5・長さ30 (直線のみ).
///       シャンク: (0,30)→(5,30)→(5,40)→[時計回り弧 中心(10,45)]→(5,50)→(5,60)→(0,60).
///       弧は軸側へ膨らみ、中点は (10 - 5√2, 45) で最小半径 ≈ 2.93
mc::ToolAssemblySpec NeckToolSpec() {
    ToolProfile profile;
    profile.elements.push_back(PolylineElement(ToolPart::kCutter, {
            Vector2d(0.0, 0.0), Vector2d(5.0, 0.0), Vector2d(5.0, 30.0),
            Vector2d(0.0, 30.0)}));
    ToolProfileElement shank;
    shank.part = ToolPart::kShank;
    shank.segments = {
            ProfileSegment::Line(Vector2d(0.0, 30.0), Vector2d(5.0, 30.0)),
            ProfileSegment::Line(Vector2d(5.0, 30.0), Vector2d(5.0, 40.0)),
            ProfileSegment::Arc(Vector2d(5.0, 40.0), Vector2d(5.0, 50.0),
                                Vector2d(10.0, 45.0), false),
            ProfileSegment::Line(Vector2d(5.0, 50.0), Vector2d(5.0, 60.0)),
            ProfileSegment::Line(Vector2d(5.0, 60.0), Vector2d(0.0, 60.0))};
    profile.elements.push_back(shank);
    profile.gauge_line_z = 60.0;  // ホルダ無し. シャンク上端をゲージラインとする
    mc::ToolAssemblySpec spec;
    spec.number = 2;
    spec.profile = profile;
    return spec;
}

/// @brief 名前で直接の子Assemblyを引く (無ければnullptr)
std::shared_ptr<i_mod::Assembly> FindChild(const i_mod::Assembly& parent,
                                          const std::string& name) {
    for (const auto& child : parent.GetChildAssemblies()) {
        if (child->Metadata().name == name) return child;
    }
    return nullptr;
}

/// @brief 自身を含むAssemblyの総数を数える
std::size_t CountAssemblies(const i_mod::Assembly& node) {
    std::size_t count = 1;
    for (const auto& child : node.GetChildAssemblies()) count += CountAssemblies(*child);
    return count;
}

/// @brief 指定タイプのエンティティ数 (再帰) を返す
std::size_t CountType(const i_mod::Assembly& node, const EntityType type) {
    return node.FindEntitiesByType(type, true).size();
}

/// @brief ノード直下の指定タイプのエンティティを1つ、指定型で取得する
template <class T>
std::shared_ptr<T> GetSingle(const i_mod::Assembly& node, const EntityType type) {
    const auto found = node.FindEntitiesByType(type, false);
    if (found.size() != 1) return nullptr;
    return std::dynamic_pointer_cast<T>(found.front());
}

/// @brief 根 → 部位容器 → 要素ノードをたどる
std::shared_ptr<i_mod::Assembly> ElementNode(const i_mod::Assembly& tool,
                                            const ToolPart part, const std::size_t index) {
    const auto container = mc::FindToolPart(tool, part);
    if (!container) return nullptr;
    return FindChild(*container, mc::ToolElementName(part, index));
}

/// @brief 回転体上の点を工具座標で評価し、半径 (xy距離) とzを返す
/// @param tool 工具Assemblyの根 (変換は単位行列)
/// @param surface 回転体
/// @param u 母線方向のパラメータ
/// @param v 回転角 [rad]
std::optional<std::array<double, 2>> RadiusAndZ(
        const i_mod::Assembly& tool, const i_ent::SurfaceOfRevolution& surface,
        const double u, const double v) {
    const auto placement = tool.ResolvePlacement(surface.GetID(), i_mod::CoordFrame::World());
    if (!placement.has_value()) return std::nullopt;
    const auto point = surface.TryGetPointAt(u, v, *placement);
    if (!point.has_value()) return std::nullopt;
    return std::array<double, 2>{std::hypot(point->x(), point->y()), point->z()};
}

}  // namespace



// ---- Assembly構成とエンティティ数量 ----

TEST(ToolEntitiesTest, Ball_EntityCountsAndAssemblies) {
    const auto tool = mc::MakeToolAssembly(BallToolSpec());
    ASSERT_NE(tool, nullptr);
    EXPECT_EQ(tool->Metadata().name, "tool:1");
    EXPECT_EQ(CountAssemblies(*tool), 7u);
    for (const ToolPart part : {ToolPart::kCutter, ToolPart::kShank, ToolPart::kHolder}) {
        const auto container = FindChild(*tool, std::string(mc::ToolPartName(part)));
        ASSERT_NE(container, nullptr) << mc::ToolPartName(part);
        EXPECT_EQ(container->GetEntityCount(), 0u);
        ASSERT_EQ(container->GetChildAssemblies().size(), 1u);
        EXPECT_EQ(container->GetChildAssemblies().front()->Metadata().name,
                  mc::ToolElementName(part, 0));
    }

    // Type 100 = 1 (ボール先端)、110 = 5 (切れ刃の直線2 + 軸線3)、102 = 1、
    // 106 = 2 (シャンク・ホルダ)、314 = 3、120 = 3
    EXPECT_EQ(CountType(*tool, EntityType::kCircularArc), 1u);
    EXPECT_EQ(CountType(*tool, EntityType::kLine), 5u);
    EXPECT_EQ(CountType(*tool, EntityType::kCompositeCurve), 1u);
    EXPECT_EQ(CountType(*tool, EntityType::kCopiousData), 2u);
    EXPECT_EQ(CountType(*tool, EntityType::kColorDefinition), 3u);
    EXPECT_EQ(CountType(*tool, EntityType::kSurfaceOfRevolution), 3u);
    EXPECT_EQ(tool->GetEntityIDs(true).size(), 15u);

    const auto cutter0 = ElementNode(*tool, ToolPart::kCutter, 0);
    ASSERT_NE(cutter0, nullptr);
    EXPECT_EQ(cutter0->GetEntityCount(), 7u);
    EXPECT_EQ(ElementNode(*tool, ToolPart::kShank, 0)->GetEntityCount(), 4u);
    EXPECT_EQ(ElementNode(*tool, ToolPart::kHolder, 0)->GetEntityCount(), 4u);
    EXPECT_TRUE(tool->IsReady());
}

TEST(ToolEntitiesTest, Square_CutterIsLinearPath) {
    const auto tool = mc::MakeToolAssembly(SquareToolSpec());
    const auto cutter0 = ElementNode(*tool, ToolPart::kCutter, 0);
    ASSERT_NE(cutter0, nullptr);
    EXPECT_EQ(cutter0->GetEntityCount(), 4u);
    EXPECT_EQ(CountType(*cutter0, EntityType::kCopiousData), 1u);
    EXPECT_EQ(CountType(*cutter0, EntityType::kLine), 1u);
    EXPECT_EQ(CountType(*cutter0, EntityType::kColorDefinition), 1u);
    EXPECT_EQ(CountType(*cutter0, EntityType::kSurfaceOfRevolution), 1u);
    EXPECT_EQ(CountType(*tool, EntityType::kCircularArc), 0u);
    EXPECT_EQ(CountType(*tool, EntityType::kCompositeCurve), 0u);

    // 母線はForm 11 (平面折れ線) で、頂点は先端から端面まで
    const auto path = GetSingle<i_ent::EntityBase>(*cutter0, EntityType::kCopiousData);
    ASSERT_NE(path, nullptr);
    EXPECT_EQ(path->GetFormNumber(), 11);
}

TEST(ToolEntitiesTest, Dependents_AreSubordinate) {
    const auto tool = mc::MakeToolAssembly(BallToolSpec());
    const auto cutter0 = ElementNode(*tool, ToolPart::kCutter, 0);
    ASSERT_NE(cutter0, nullptr);
    for (const auto& entity : cutter0->FindEntitiesByType(EntityType::kLine)) {
        const auto base = std::dynamic_pointer_cast<i_ent::EntityBase>(entity);
        ASSERT_NE(base, nullptr);
        EXPECT_EQ(base->GetSubordinateEntitySwitch(),
                  SubordinateEntitySwitch::kPhysicallyDependent);
    }
    const auto arc = GetSingle<i_ent::EntityBase>(*cutter0, EntityType::kCircularArc);
    const auto composite = GetSingle<i_ent::EntityBase>(*cutter0, EntityType::kCompositeCurve);
    const auto color = GetSingle<i_ent::EntityBase>(*cutter0, EntityType::kColorDefinition);
    const auto surface = GetSingle<i_ent::EntityBase>(
            *cutter0, EntityType::kSurfaceOfRevolution);
    ASSERT_NE(arc, nullptr);
    ASSERT_NE(composite, nullptr);
    ASSERT_NE(color, nullptr);
    ASSERT_NE(surface, nullptr);
    EXPECT_EQ(arc->GetSubordinateEntitySwitch(),
              SubordinateEntitySwitch::kPhysicallyDependent);
    EXPECT_EQ(composite->GetSubordinateEntitySwitch(),
              SubordinateEntitySwitch::kPhysicallyDependent);
    EXPECT_EQ(color->GetSubordinateEntitySwitch(), SubordinateEntitySwitch::kIndependent);
    EXPECT_EQ(surface->GetSubordinateEntitySwitch(), SubordinateEntitySwitch::kIndependent);

    // シャンク (Type 106) の母線と軸線も物理従属
    const auto shank0 = ElementNode(*tool, ToolPart::kShank, 0);
    ASSERT_NE(shank0, nullptr);
    const auto path = GetSingle<i_ent::EntityBase>(*shank0, EntityType::kCopiousData);
    const auto axis = GetSingle<i_ent::EntityBase>(*shank0, EntityType::kLine);
    ASSERT_NE(path, nullptr);
    ASSERT_NE(axis, nullptr);
    EXPECT_EQ(path->GetSubordinateEntitySwitch(),
              SubordinateEntitySwitch::kPhysicallyDependent);
    EXPECT_EQ(axis->GetSubordinateEntitySwitch(),
              SubordinateEntitySwitch::kPhysicallyDependent);
}

TEST(ToolEntitiesTest, Color_DefaultAndOverride) {
    mc::ToolAssemblySpec spec = BallToolSpec();
    {
        const auto tool = mc::MakeToolAssembly(spec);
        const std::array<std::array<int, 3>, 3> expected = {{
                {255, 217, 0}, {255, 255, 255}, {128, 144, 160}}};
        const std::array<ToolPart, 3> parts = {
                ToolPart::kCutter, ToolPart::kShank, ToolPart::kHolder};
        for (std::size_t i = 0; i < parts.size(); ++i) {
            const auto node = ElementNode(*tool, parts[i], 0);
            ASSERT_NE(node, nullptr);
            const auto color = GetSingle<i_ent::ColorDefinition>(
                    *node, EntityType::kColorDefinition);
            ASSERT_NE(color, nullptr);
            EXPECT_EQ(color->GetRGB255(), expected[i]) << mc::ToolPartName(parts[i]);
            // 回転体の色フィールドが色定義を参照する
            const auto surface = GetSingle<i_ent::EntityBase>(
                    *node, EntityType::kSurfaceOfRevolution);
            ASSERT_NE(surface, nullptr);
            EXPECT_EQ(surface->GetColor().GetPointer(), color);
            EXPECT_FALSE(node->Display().opacity_override.has_value());
        }
    }
    // 要素の色と不透明度を指定
    spec.profile.elements[0].color = std::array<float, 3>{0.2f, 0.4f, 0.6f};
    spec.profile.elements[0].opacity = 0.5f;
    const auto tool = mc::MakeToolAssembly(spec);
    const auto cutter0 = ElementNode(*tool, ToolPart::kCutter, 0);
    ASSERT_NE(cutter0, nullptr);
    const auto color = GetSingle<i_ent::ColorDefinition>(
            *cutter0, EntityType::kColorDefinition);
    ASSERT_NE(color, nullptr);
    EXPECT_EQ(color->GetRGB255(), (std::array<int, 3>{51, 102, 153}));
    ASSERT_TRUE(cutter0->Display().opacity_override.has_value());
    EXPECT_NEAR(*cutter0->Display().opacity_override, 0.5, 1e-6);
    // 他の要素は不透明のまま
    const auto shank0 = ElementNode(*tool, ToolPart::kShank, 0);
    EXPECT_FALSE(shank0->Display().opacity_override.has_value());
}



// ---- 幾何 ----

TEST(ToolEntitiesTest, Transform_ElementNodesCarryQuarterTurn) {
    const auto tool = mc::MakeToolAssembly(BallToolSpec());
    const igesio::Matrix4d quarter_turn = mc::MakeRigid(
            mc::RotationAboutAxis(Vector3d::UnitX(), mc::kQuarterTurn), Vector3d::Zero());
    EXPECT_TRUE(tool->GetGlobalTransform().isIdentity(kTol));
    for (const ToolPart part : {ToolPart::kCutter, ToolPart::kShank, ToolPart::kHolder}) {
        const auto container = mc::FindToolPart(*tool, part);
        ASSERT_NE(container, nullptr);
        EXPECT_TRUE(container->GetGlobalTransform().isIdentity(kTol));
        const auto node = ElementNode(*tool, part, 0);
        ASSERT_NE(node, nullptr);
        EXPECT_TRUE(node->GetGlobalTransform().isApprox(quarter_turn, kTol))
                << "part: " << mc::ToolPartName(part);
    }
    // R_x(+90°) は母線の軸 (+Y) を工具軸 (+Z) へ写す
    const auto node = ElementNode(*tool, ToolPart::kCutter, 0);
    EXPECT_TRUE(mc::ApplyDirection(node->GetGlobalTransform(), Vector3d::UnitY())
                        .isApprox(Vector3d::UnitZ(), kTol));
}

TEST(ToolEntitiesTest, Geometry_RevolvedPointsMatchProfile) {
    const auto tool = mc::MakeToolAssembly(BallToolSpec());
    const auto cutter0 = ElementNode(*tool, ToolPart::kCutter, 0);
    ASSERT_NE(cutter0, nullptr);
    const auto surface = GetSingle<i_ent::SurfaceOfRevolution>(
            *cutter0, EntityType::kSurfaceOfRevolution);
    ASSERT_NE(surface, nullptr);
    const auto range = surface->GetParameterRange();
    EXPECT_NEAR(range[2], 0.0, kTol);
    EXPECT_NEAR(range[3], mc::kFullTurn, kTol);

    // 母線 (Type 102) の先頭は1/4円弧 (Δ = π/2) なので、その中点は u = π/4.
    // 中心 (0, 5)・半径5 の角度 -π/4 の点: r = 5/√2、z = 5 - 5/√2
    const double radius = 5.0;
    const double u_mid = mc::kQuarterTurn / 2.0;
    const double r_mid = radius / std::sqrt(2.0);
    const double z_mid = radius - radius / std::sqrt(2.0);
    for (const double v : {0.0, mc::kQuarterTurn}) {
        const auto rz = RadiusAndZ(*tool, *surface, u_mid, v);
        ASSERT_TRUE(rz.has_value());
        EXPECT_NEAR((*rz)[0], r_mid, kTol) << "v = " << v;
        EXPECT_NEAR((*rz)[1], z_mid, kTol) << "v = " << v;
    }
    // 母線の終端は端面の軸上点 (0, 30)
    for (const double v : {0.0, mc::kQuarterTurn}) {
        const auto rz = RadiusAndZ(*tool, *surface, range[1], v);
        ASSERT_TRUE(rz.has_value());
        EXPECT_NEAR((*rz)[0], 0.0, kTol) << "v = " << v;
        EXPECT_NEAR((*rz)[1], 30.0, kTol) << "v = " << v;
    }
    // 母線の始端は先端 (0, 0)
    const auto tip = RadiusAndZ(*tool, *surface, range[0], 0.0);
    ASSERT_TRUE(tip.has_value());
    EXPECT_NEAR((*tip)[0], 0.0, kTol);
    EXPECT_NEAR((*tip)[1], 0.0, kTol);
}

TEST(ToolEntitiesTest, Geometry_WorldBoundingBoxMatchesReach) {
    const mc::ToolAssemblySpec spec = BallToolSpec();
    const auto tool = mc::MakeToolAssembly(spec);
    const auto bb = tool->GetWorldBoundingBox();
    ASSERT_TRUE(bb.has_value());
    const double max_radius = spec.profile.MaxRadius();
    const double reach = spec.profile.Reach();
    EXPECT_TRUE(bb->GetControl().isApprox(Vector3d(-max_radius, -max_radius, 0.0), kTol))
            << bb->GetControl().transpose();
    const auto sizes = bb->GetSizes();
    EXPECT_NEAR(sizes[0], 2.0 * max_radius, kTol);
    EXPECT_NEAR(sizes[1], 2.0 * max_radius, kTol);
    EXPECT_NEAR(sizes[2], reach, kTol);
}



// ---- 時計回りの円弧 (ネック部) ----

TEST(ToolEntitiesTest, Clockwise_ArcKeepsSingleSurface) {
    const auto tool = mc::MakeToolAssembly(NeckToolSpec());
    const auto shank0 = ElementNode(*tool, ToolPart::kShank, 0);
    ASSERT_NE(shank0, nullptr);
    EXPECT_EQ(CountType(*shank0, EntityType::kSurfaceOfRevolution), 1u);
    EXPECT_EQ(CountType(*shank0, EntityType::kCompositeCurve), 1u);
    EXPECT_EQ(CountType(*shank0, EntityType::kCircularArc), 1u);
    EXPECT_EQ(CountType(*shank0, EntityType::kLine), 5u);  // 直線4 + 軸線1

    // 弧は時計回りのまま、始終点が輪郭どおり (母線座標: x = r、y = z)
    const auto arc = GetSingle<i_ent::CircularArc>(*shank0, EntityType::kCircularArc);
    ASSERT_NE(arc, nullptr);
    EXPECT_TRUE(arc->IsClockwise());
    const auto arc_range = arc->GetParameterRange();
    const auto start = arc->TryGetPointAt(arc_range[0]);
    const auto end = arc->TryGetPointAt(arc_range[1]);
    ASSERT_TRUE(start.has_value());
    ASSERT_TRUE(end.has_value());
    EXPECT_TRUE(start->isApprox(Vector3d(5.0, 40.0, 0.0), kTol)) << start->transpose();
    EXPECT_TRUE(end->isApprox(Vector3d(5.0, 50.0, 0.0), kTol)) << end->transpose();

    // 母線 (Type 102) を弧の中点で評価すると、軸側へ膨らんだ点 (10 - 5√2, 45) になる
    const auto composite = GetSingle<i_ent::CompositeCurve>(
            *shank0, EntityType::kCompositeCurve);
    ASSERT_NE(composite, nullptr);
    ASSERT_EQ(composite->GetCurveCount(), 5u);
    double offset = 0.0;
    for (std::size_t i = 0; i < 2; ++i) {
        const auto range = composite->GetCurveAt(i)->GetParameterRange();
        offset += range[1] - range[0];
    }
    const double u_mid = offset + (arc_range[1] - arc_range[0]) / 2.0;
    const auto derivatives = composite->TryGetDerivatives(u_mid, 1);
    ASSERT_TRUE(derivatives.has_value());
    const Vector3d expected(10.0 - 5.0 * std::sqrt(2.0), 45.0, 0.0);
    EXPECT_TRUE(derivatives->derivatives[0].isApprox(expected, kTol))
            << derivatives->derivatives[0].transpose();
    // 進行方向は+z (母線座標では+y) 向き
    EXPECT_GT(derivatives->derivatives[1].y(), 0.0);
    EXPECT_NEAR(derivatives->derivatives[1].x(), 0.0, kTol);
}

TEST(ToolEntitiesTest, Clockwise_ExportsMirroredArcAndMatrix) {
    const auto tool = mc::MakeToolAssembly(NeckToolSpec());
    i_mod::IgesData data;
    data.Root().AddChildAssembly(tool);
    EXPECT_EQ(CountType(data.Root(), EntityType::kTransformationMatrix), 0u);

    const auto intermediate = igesio::ConvertToIntermediate(data);
    const auto& des = intermediate.directory_entry_section;
    const auto& pds = intermediate.parameter_data_section;
    // 展開でType 124が1件増える (モデル本体は不変)
    ASSERT_EQ(des.size(), tool->GetEntityIDs(true).size() + 1);
    EXPECT_EQ(CountType(data.Root(), EntityType::kTransformationMatrix), 0u);

    std::optional<std::size_t> arc_index;
    std::optional<std::size_t> composite_index;
    std::optional<std::size_t> matrix_index;
    std::size_t matrix_count = 0;
    for (std::size_t i = 0; i < des.size(); ++i) {
        if (des[i].entity_type == EntityType::kCircularArc) arc_index = i;
        if (des[i].entity_type == EntityType::kCompositeCurve) composite_index = i;
        if (des[i].entity_type == EntityType::kTransformationMatrix) {
            matrix_index = i;
            ++matrix_count;
        }
    }
    ASSERT_TRUE(arc_index.has_value());
    ASSERT_TRUE(composite_index.has_value());
    ASSERT_TRUE(matrix_index.has_value());
    EXPECT_EQ(matrix_count, 1u);

    // CW弧のDE枠にはCCW弧 (yを中心 45 について鏡映) が出力され、Type 124を参照する
    EXPECT_EQ(des[*arc_index].transformation_matrix,
              static_cast<int>(des[*matrix_index].sequence_number));
    const auto& arc_pd = pds[*arc_index].data;
    ASSERT_EQ(arc_pd.size(), 7u);
    const std::vector<double> expected{0.0, 10.0, 45.0, 5.0, 50.0, 5.0, 40.0};
    for (std::size_t i = 0; i < expected.size(); ++i) {
        EXPECT_NEAR(std::stod(arc_pd[i]), expected[i], kTol) << "i = " << i;
    }
    // Type 102の参照は同じDE枠を指したまま
    const std::string arc_pointer = std::to_string(des[*arc_index].sequence_number);
    const auto& composite_pd = pds[*composite_index].data;
    EXPECT_NE(std::find(composite_pd.begin(), composite_pd.end(), arc_pointer),
              composite_pd.end());
}



// ---- 部位の検索と表示切替 ----

TEST(ToolEntitiesTest, Parts_VisibilityAndLookup) {
    const auto tool = mc::MakeToolAssembly(BallToolSpec());
    mc::SetToolPartVisible(*tool, ToolPart::kHolder, false);
    const auto holder = mc::FindToolPart(*tool, ToolPart::kHolder);
    ASSERT_NE(holder, nullptr);
    EXPECT_FALSE(holder->Display().visible);
    EXPECT_TRUE(tool->Display().visible);
    EXPECT_TRUE(mc::FindToolPart(*tool, ToolPart::kCutter)->Display().visible);
    EXPECT_TRUE(mc::FindToolPart(*tool, ToolPart::kShank)->Display().visible);
    // 要素ノード自身の可視性は変えない (容器の可視性で切り替える)
    EXPECT_TRUE(holder->GetChildAssemblies().front()->Display().visible);
    mc::SetToolPartVisible(*tool, ToolPart::kHolder, true);
    EXPECT_TRUE(holder->Display().visible);

    // 無い部位は nullptr / 何もしない
    const auto neck = mc::MakeToolAssembly(NeckToolSpec());
    EXPECT_EQ(mc::FindToolPart(*neck, ToolPart::kHolder), nullptr);
    EXPECT_NO_THROW(mc::SetToolPartVisible(*neck, ToolPart::kHolder, false));
    EXPECT_EQ(CountAssemblies(*neck), 5u);
}

TEST(ToolEntitiesTest, Invalid_ProfileThrows) {
    mc::ToolAssemblySpec spec = BallToolSpec();
    spec.profile.elements.clear();
    EXPECT_THROW(mc::MakeToolAssembly(spec), std::invalid_argument);

    spec = BallToolSpec();
    spec.profile.elements[0].segments.pop_back();  // 端面を除くと軸上で終わらない
    EXPECT_THROW(mc::MakeToolAssembly(spec), std::invalid_argument);

    spec = BallToolSpec();
    spec.profile.command_point_z = 200.0;
    EXPECT_THROW(mc::MakeToolAssembly(spec), std::invalid_argument);
}

TEST(ToolEntitiesTest, Multiple_ElementsPerPart) {
    mc::ToolAssemblySpec spec;
    spec.number = 7;
    // 切れ刃2要素 (先端の小径部と、その上の大径部) とホルダ2要素 (段付き)
    spec.profile.elements.push_back(PolylineElement(ToolPart::kCutter, {
            Vector2d(0.0, 0.0), Vector2d(3.0, 0.0), Vector2d(3.0, 10.0),
            Vector2d(0.0, 10.0)}));
    spec.profile.elements.push_back(PolylineElement(ToolPart::kHolder, {
            Vector2d(0.0, 40.0), Vector2d(15.0, 40.0), Vector2d(15.0, 60.0),
            Vector2d(0.0, 60.0)}));
    spec.profile.elements.push_back(PolylineElement(ToolPart::kCutter, {
            Vector2d(0.0, 10.0), Vector2d(5.0, 10.0), Vector2d(5.0, 30.0),
            Vector2d(0.0, 30.0)}));
    spec.profile.elements.push_back(PolylineElement(ToolPart::kHolder, {
            Vector2d(0.0, 60.0), Vector2d(25.0, 60.0), Vector2d(25.0, 80.0),
            Vector2d(0.0, 80.0)}));

    const auto tool = mc::MakeToolAssembly(spec);
    EXPECT_EQ(tool->Metadata().name, "tool:7");
    EXPECT_EQ(CountAssemblies(*tool), 7u);  // 根 + 容器2 + 要素4
    EXPECT_EQ(mc::FindToolPart(*tool, ToolPart::kShank), nullptr);
    const auto cutter = mc::FindToolPart(*tool, ToolPart::kCutter);
    const auto holder = mc::FindToolPart(*tool, ToolPart::kHolder);
    ASSERT_NE(cutter, nullptr);
    ASSERT_NE(holder, nullptr);
    ASSERT_EQ(cutter->GetChildAssemblies().size(), 2u);
    ASSERT_EQ(holder->GetChildAssemblies().size(), 2u);
    EXPECT_EQ(cutter->GetChildAssemblies()[0]->Metadata().name, "cutter0");
    EXPECT_EQ(cutter->GetChildAssemblies()[1]->Metadata().name, "cutter1");
    EXPECT_EQ(holder->GetChildAssemblies()[0]->Metadata().name, "holder0");
    EXPECT_EQ(holder->GetChildAssemblies()[1]->Metadata().name, "holder1");
    EXPECT_EQ(CountType(*tool, EntityType::kSurfaceOfRevolution), 4u);
    EXPECT_EQ(mc::ToolElementName(ToolPart::kShank, 3), "shank3");
    EXPECT_EQ(mc::ToolAssemblyName(0), "tool:0");
}
