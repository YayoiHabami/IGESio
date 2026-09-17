/**
 * @file extensions/machines/scene/tool_trajectory.cpp
 * @brief 工具軌跡の表示オブジェクト
 * @author Yayoi Habami
 * @date 2026-09-16
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/machines/scene/tool_trajectory.h"

#include <initializer_list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "igesio/common/id_generator.h"
#include "igesio/extensions/machines/tools/tool_entities.h"
#include "igesio/extensions/machines/tools/tool_profile.h"

namespace igesio::extensions::machines {

namespace {

namespace i_mod = igesio::models;
namespace i_ins = igesio::extensions::inspection;

/// @brief 指定した部位のアセンブリ配下の全エンティティのIDを集める
/// @param tool 工具のアセンブリ
/// @param parts 部位
/// @return IDの集合 (該当する部位が無ければ空)
std::unordered_set<ObjectID> PartEntityIds(
        const i_mod::Assembly& tool, const std::initializer_list<ToolPart> parts) {
    std::unordered_set<ObjectID> ids;
    for (const ToolPart part : parts) {
        const auto container = FindToolPart(tool, part);
        if (!container) continue;
        for (const ObjectID& id : container->GetEntityIDs(true)) ids.insert(id);
    }
    return ids;
}

/// @brief 平坦化したテンプレートのメンバを、切れ刃部+シャンク部とホルダ部に分ける
/// @param tool_template テンプレート
/// @param[out] body 切れ刃部+シャンク部のメンバ
/// @param[out] holder ホルダ部のメンバ
/// @note どちらの部位にも属さないメンバ (工具軸線、制御点マーカー等) は除外する
void SplitMembers(const i_mod::Assembly& tool_template,
                  std::vector<i_ins::InstancedMember>& body,
                  std::vector<i_ins::InstancedMember>& holder) {
    const std::unordered_set<ObjectID> body_ids =
            PartEntityIds(tool_template, {ToolPart::kCutter, ToolPart::kShank});
    const std::unordered_set<ObjectID> holder_ids =
            PartEntityIds(tool_template, {ToolPart::kHolder});
    const auto flattened = i_ins::MakeInstancedAssembly(tool_template);
    for (const i_ins::InstancedMember& member : flattened->Members()) {
        const ObjectID& id = member.source->GetID();
        if (body_ids.count(id) > 0) {
            body.push_back(member);
        } else if (holder_ids.count(id) > 0) {
            holder.push_back(member);
        }
    }
}

}  // namespace



void ToolTrajectoryGroup::SetPlacements(std::vector<igesio::Matrix4d> placements) {
    // 成分の完全一致なら形状リビジョンを更新しない
    if (!body || body->Transforms() == placements) return;

    if (holder) holder->SetTransforms(placements);
    body->SetTransforms(std::move(placements));
}

void ToolTrajectoryGroup::SetHolderVisible(const bool visible) {
    if (!assembly || !holder) return;

    for (const auto& child : assembly->GetChildAssemblies()) {
        if (child && child->Metadata().name == kTrajectoryHolderName) {
            child->SetVisible(visible);
        }
    }
}



ToolTrajectoryGroup MakeToolTrajectoryGroup(
        const i_mod::Assembly& tool_template,
        std::vector<igesio::Matrix4d> placements, const std::string_view name,
        const std::optional<float> opacity) {
    std::vector<i_ins::InstancedMember> body_members;
    std::vector<i_ins::InstancedMember> holder_members;
    SplitMembers(tool_template, body_members, holder_members);
    if (body_members.empty()) {
        throw std::invalid_argument(
                "tool template has no cutter or shank geometry to instance");
    }

    ToolTrajectoryGroup group;
    group.assembly = i_mod::MakeAssembly(std::string(name));
    if (opacity.has_value()) group.assembly->SetOpacityOverride(opacity);
    group.body = i_ins::MakeInstancedEntity(std::move(body_members), placements);
    group.assembly->AddEntity(group.body);
    if (!holder_members.empty()) {
        auto holder = i_mod::MakeAssembly(std::string(kTrajectoryHolderName));
        group.holder = i_ins::MakeInstancedEntity(std::move(holder_members),
                                                  std::move(placements));
        holder->AddEntity(group.holder);
        group.assembly->AddChildAssembly(holder);
    }
    return group;
}

}  // namespace igesio::extensions::machines
