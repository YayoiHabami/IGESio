/**
 * @file tests/extensions/machines/scene/test_tool_trajectory.cpp
 * @brief 工具軌跡の表示オブジェクト (scene/tool_trajectory) のテスト
 * @author Yayoi Habami
 * @date 2026-09-16
 * @copyright 2026 Yayoi Habami
 * @note 対象: MakeToolTrajectoryGroup / ToolTrajectoryGroup::SetPlacements /
 *       ToolTrajectoryGroup::SetHolderVisible
 *       - 正常系: 切れ刃部+シャンク部とホルダ部の仕分け (回転曲面のみ)、局所配置の
 *         R_x(+90°) と雛形のルートの変換の除外、同次変換の列の差し替えと同じ列での
 *         無変更、ホルダ部の可視性
 *       - 正常系 (退化): ホルダの無い工具、工具軸線と制御点マーカーの除外、
 *         空の同次変換の列
 *       - 異常系: 切れ刃部もシャンク部も無い雛形 (`invalid_argument`)
 * @note 工具は`MinimalProject`の簡易ボール工具#1 (ホルダあり) を`MakeToolAssembly`
 *       で実体化したもの
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <unordered_set>
#include <vector>

#include "igesio/common/id_generator.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/entities/entity_type.h"
#include "igesio/entities/curves/line.h"
#include "igesio/models/assembly.h"
#include "igesio/extensions/inspection/instanced_entity.h"
#include "igesio/extensions/machines/core/rotation.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/scene/tool_trajectory.h"
#include "igesio/extensions/machines/tools/tool_assembly.h"
#include "igesio/extensions/machines/tools/tool_entities.h"
#include "igesio/extensions/machines/tools/tool_profile.h"
#include "../simulation/motion_for_testing.h"

namespace {

namespace mc = igesio::extensions::machines;
namespace i_ent = igesio::entities;
namespace i_mod = igesio::models;
namespace i_ins = igesio::extensions::inspection;
using igesio::Matrix4d;
using igesio::ObjectID;
using igesio::Vector3d;

/// @brief 数値比較の許容誤差
constexpr double kTol = 1e-9;

/// @brief 簡易ボール工具#1の定義
mc::ToolAssemblySpec ToolSpec() {
    return motion_test::MakeSetupWithoutDynamics().Tools().at(1);
}

/// @brief 指定した部位のアセンブリ配下の全エンティティのIDを集める
std::unordered_set<ObjectID> PartIds(const i_mod::Assembly& tool,
                                     const mc::ToolPart part) {
    std::unordered_set<ObjectID> ids;
    if (const auto container = mc::FindToolPart(tool, part)) {
        for (const ObjectID& id : container->GetEntityIDs(true)) ids.insert(id);
    }
    return ids;
}

/// @brief 全メンバの基準エンティティが指定した集合に属し、回転曲面であることを確かめる
void ExpectMembersIn(const std::vector<i_ins::InstancedMember>& members,
                     const std::unordered_set<ObjectID>& ids) {
    EXPECT_FALSE(members.empty());
    for (const i_ins::InstancedMember& member : members) {
        EXPECT_EQ(ids.count(member.source->GetID()), 1u);
        EXPECT_EQ(member.source->GetType(), i_ent::EntityType::kSurfaceOfRevolution);
    }
}

/// @brief 単位行列を`count`個並べた同次変換の列
std::vector<Matrix4d> Identities(const std::size_t count) {
    return std::vector<Matrix4d>(count, Matrix4d::Identity());
}

}  // namespace



TEST(ToolTrajectoryTest, Make_SplitsBodyAndHolder) {
    const auto tool = mc::MakeToolAssembly(ToolSpec());
    const mc::ToolTrajectoryGroup group =
            mc::MakeToolTrajectoryGroup(*tool, Identities(1), "run0");
    ASSERT_NE(group.assembly, nullptr);
    ASSERT_NE(group.body, nullptr);
    ASSERT_NE(group.holder, nullptr);
    EXPECT_EQ(group.assembly->Metadata().name, "run0");

    // 本体は切れ刃部+シャンク部の回転曲面、ホルダ部はホルダの回転曲面だけを持つ
    std::unordered_set<ObjectID> body_ids = PartIds(*tool, mc::ToolPart::kCutter);
    for (const ObjectID& id : PartIds(*tool, mc::ToolPart::kShank)) body_ids.insert(id);
    ExpectMembersIn(group.body->Members(), body_ids);
    ExpectMembersIn(group.holder->Members(), PartIds(*tool, mc::ToolPart::kHolder));

    // 本体はアセンブリが直接持ち、ホルダ部は子`holder`が持つ
    EXPECT_EQ(group.assembly->GetEntity(group.body->GetID()), group.body);
    ASSERT_EQ(group.assembly->GetChildAssemblies().size(), 1u);
    const auto holder = group.assembly->GetChildAssemblies()[0];
    EXPECT_EQ(holder->Metadata().name, mc::kTrajectoryHolderName);
    EXPECT_EQ(holder->GetEntity(group.holder->GetID()), group.holder);
    EXPECT_EQ(group.body->InstanceCount(), 1u);
    EXPECT_EQ(group.holder->InstanceCount(), 1u);
}

TEST(ToolTrajectoryTest, Make_WithoutHolder) {
    mc::ToolAssemblySpec spec = ToolSpec();
    auto& elements = spec.profile.elements;
    elements.erase(std::remove_if(elements.begin(), elements.end(),
                                  [](const mc::ToolProfileElement& element) {
                                      return element.part == mc::ToolPart::kHolder;
                                  }),
                   elements.end());
    // ホルダ上端のゲージラインは輪郭の外に出るため、既定 (最上端) に戻す
    spec.profile.gauge_line_z = std::nullopt;
    const auto tool = mc::MakeToolAssembly(spec);
    const mc::ToolTrajectoryGroup group =
            mc::MakeToolTrajectoryGroup(*tool, Identities(2), "run0", 0.5f);
    EXPECT_NE(group.body, nullptr);
    EXPECT_EQ(group.holder, nullptr);
    EXPECT_TRUE(group.assembly->GetChildAssemblies().empty());
    ASSERT_TRUE(group.assembly->Display().opacity_override.has_value());
    EXPECT_NEAR(*group.assembly->Display().opacity_override, 0.5f, 1e-6);
}

TEST(ToolTrajectoryTest, Make_LocalPlacementHasProfileRotation) {
    const auto tool = mc::MakeToolAssembly(ToolSpec());
    // 雛形のルートの変換は局所配置に含めない
    tool->SetGlobalTransform(mc::Translation(Vector3d(0.0, 0.0, -90.0)));
    const mc::ToolTrajectoryGroup group =
            mc::MakeToolTrajectoryGroup(*tool, Identities(1), "run0");

    const Matrix4d expected = mc::MakeRigid(
            mc::RotationAboutAxis(Vector3d::UnitX(), mc::kQuarterTurn), Vector3d::Zero());
    for (const auto& members : {group.body->Members(), group.holder->Members()}) {
        for (const i_ins::InstancedMember& member : members) {
            EXPECT_TRUE(member.local_placement.isApprox(expected, kTol))
                    << member.local_placement;
        }
    }
}

TEST(ToolTrajectoryTest, Make_ExcludesAxisAndControlPoint) {
    const auto tool = mc::MakeToolAssembly(ToolSpec());
    auto axis = i_mod::MakeAssembly("axis");
    axis->AddEntity(i_ent::MakeLine(Vector3d::Zero(), Vector3d(0.0, 0.0, 100.0)));
    tool->AddChildAssembly(axis);

    const mc::ToolTrajectoryGroup group =
            mc::MakeToolTrajectoryGroup(*tool, Identities(1), "run0");
    for (const i_ins::InstancedMember& member : group.body->Members()) {
        EXPECT_NE(member.source->GetType(), i_ent::EntityType::kLine);
    }
}

TEST(ToolTrajectoryTest, Placements_UpdateAndIdempotent) {
    const auto tool = mc::MakeToolAssembly(ToolSpec());
    mc::ToolTrajectoryGroup group = mc::MakeToolTrajectoryGroup(*tool, {}, "run0");
    EXPECT_EQ(group.body->InstanceCount(), 0u);

    const std::vector<Matrix4d> placements = {
            Matrix4d::Identity(), mc::Translation(Vector3d(10.0, 0.0, 0.0))};
    const auto revision = group.body->GeometryRevision();
    group.SetPlacements(placements);
    EXPECT_EQ(group.body->InstanceCount(), 2u);
    EXPECT_EQ(group.holder->InstanceCount(), 2u);
    EXPECT_TRUE(group.holder->Transforms()[1].isApprox(placements[1], kTol));
    EXPECT_GT(group.body->GeometryRevision(), revision);

    // 同じ列なら形状リビジョンを更新しない
    const auto updated = group.body->GeometryRevision();
    group.SetPlacements(placements);
    EXPECT_EQ(group.body->GeometryRevision(), updated);
}

TEST(ToolTrajectoryTest, HolderVisible_TogglesChild) {
    const auto tool = mc::MakeToolAssembly(ToolSpec());
    mc::ToolTrajectoryGroup group =
            mc::MakeToolTrajectoryGroup(*tool, Identities(1), "run0");
    const auto holder = group.assembly->GetChildAssemblies()[0];
    group.SetHolderVisible(false);
    EXPECT_FALSE(holder->Display().visible);
    group.SetHolderVisible(true);
    EXPECT_TRUE(holder->Display().visible);

    // ホルダの無いグループでは何もしない
    mc::ToolTrajectoryGroup empty;
    EXPECT_NO_THROW(empty.SetHolderVisible(false));
    EXPECT_NO_THROW(empty.SetPlacements(Identities(1)));
}

TEST(ToolTrajectoryTest, Make_ThrowsInvalidArgumentWhenNoBody) {
    const auto empty = i_mod::MakeAssembly("tool:9");
    EXPECT_THROW(mc::MakeToolTrajectoryGroup(*empty, Identities(1), "run0"),
                 std::invalid_argument);

    // ホルダだけの雛形も本体が無いので拒否する
    const auto holder_only = mc::MakeToolAssembly(ToolSpec());
    for (const mc::ToolPart part : {mc::ToolPart::kCutter, mc::ToolPart::kShank}) {
        if (const auto container = mc::FindToolPart(*holder_only, part)) {
            holder_only->RemoveChildAssembly(container->GetID(),
                                             i_mod::RemovalPolicy::kOrphan);
        }
    }
    EXPECT_THROW(mc::MakeToolTrajectoryGroup(*holder_only, Identities(1), "run0"),
                 std::invalid_argument);
}
