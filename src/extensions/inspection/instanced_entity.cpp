/**
 * @file extensions/inspection/instanced_entity.cpp
 * @brief 基準メンバ列を変換行列で複製表示する非IGESエンティティ
 * @author Yayoi Habami
 * @date 2026-06-12
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/inspection/instanced_entity.h"

#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "igesio/entities/entity_base.h"
#include "igesio/models/assembly.h"

namespace igesio::extensions::inspection {

namespace {

/// @brief Assemblyサブツリーを再帰的に走査し、幾何メンバを平坦化する
/// @param node 走査中のノード
/// @param accum 基準フレーム→nodeフレームの累積変換 (ルート自身の大域変換は含めない)
/// @param[out] members 収集先のメンバ列
void FlattenAssembly(const models::Assembly& node,
                     const igesio::Matrix4d& accum,
                     std::vector<InstancedMember>& members) {
    // このノードが直接所有する幾何エンティティ (IGeometry) をメンバ化する.
    // 幾何でない (純粋な定義・変換) エンティティは複製対象外.
    for (const auto& [id, entity] : node.GetEntities()) {
        if (!entity) continue;
        // 物理従属 (トリム面の基底曲面・境界曲線等) は親 (トリム面・複合曲線等) の
        // 描画オブジェクトが子として描くため、独立メンバにしない.
        // レンダラのWalkAssemblyと同じ除外規則 (これを外すと未トリムの基底曲面が
        // 重畳してトリム描画が崩れる). 非EntityBase (純粋非IGES) は対象外とせず含める.
        const auto eb =
                std::dynamic_pointer_cast<const entities::EntityBase>(entity);
        if (eb && eb->GetSubordinateEntitySwitch()
                == entities::SubordinateEntitySwitch::kPhysicallyDependent) {
            continue;
        }
        if (!std::dynamic_pointer_cast<const entities::IGeometry>(entity)) {
            continue;
        }
        members.push_back(InstancedMember{entity, accum});
    }
    // 子Assemblyへ降りる (子の大域変換を累積へ後掛けする)
    for (const auto& child : node.GetChildAssemblies()) {
        if (!child) continue;
        FlattenAssembly(*child, accum * child->GetGlobalTransform(), members);
    }
}

}  // namespace



InstancedEntity::InstancedEntity(
        std::vector<InstancedMember> members,
        std::vector<igesio::Matrix4d> transforms)
        : members_(std::move(members)), transforms_(std::move(transforms)) {
    for (const auto& member : members_) {
        if (!member.source) {
            throw std::invalid_argument(
                    "Member source entity pointer cannot be null");
        }
    }
}

InstancedEntity::InstancedEntity(
        std::shared_ptr<const entities::IEntityIdentifier> source,
        std::vector<igesio::Matrix4d> transforms)
        : InstancedEntity(
              std::vector<InstancedMember>{
                  InstancedMember{std::move(source),
                                  igesio::Matrix4d::Identity()}},
              std::move(transforms)) {}

numerics::BoundingBox InstancedEntity::GetDefinedBoundingBox() const {
    if (members_.empty() || transforms_.empty()) return numerics::BoundingBox();

    bool initialized = false;
    Vector3d lo = Vector3d::Zero();
    Vector3d hi = Vector3d::Zero();
    for (const auto& member : members_) {
        // 基準形状のBB (メンバのDE変換適用済み・モデル空間) を取得する.
        // IGeometryでないメンバは包含領域を構成できないためスキップする.
        const auto geom = std::dynamic_pointer_cast<const entities::IGeometry>(
                member.source);
        if (!geom) continue;

        // 複製先行列・局所配置はスケール・せん断を含みうるため、BoundingBoxの
        // 直交基底制約を避けて頂点を直接変換する. 無限に伸びる基準 (無限平面等) は
        // 有限頂点が無く包含領域を構成できないためスキップする.
        const auto vertices = geom->GetBoundingBox().GetFiniteVertices();
        if (vertices.empty()) continue;

        for (const auto& transform : transforms_) {
            // 複製先行列 · 局所配置 を合成してから頂点を変換する
            const igesio::Matrix4d m = transform * member.local_placement;
            for (const auto& v : vertices) {
                const Vector3d w = numerics::ApplyTransform(m, v, true);
                if (!initialized) {
                    lo = w;
                    hi = w;
                    initialized = true;
                } else {
                    lo = lo.cwiseMin(w);
                    hi = hi.cwiseMax(w);
                }
            }
        }
    }
    if (!initialized) return numerics::BoundingBox();
    return numerics::BoundingBox(lo, hi);
}

std::optional<Vector3d> InstancedEntity::Transform(
        const std::optional<Vector3d>& input,
        [[maybe_unused]] const bool is_point) const {
    // 本エンティティ自身は変換行列を持たない (複製先行列・局所配置は描画側が適用する)
    return input;
}



/**
 * ファクトリ関数
 */

std::shared_ptr<InstancedEntity> MakeInstancedEntity(
        std::vector<InstancedMember> members,
        std::vector<igesio::Matrix4d> transforms) {
    return std::make_shared<InstancedEntity>(
            std::move(members), std::move(transforms));
}

std::shared_ptr<InstancedEntity> MakeInstancedEntity(
        std::shared_ptr<const entities::IEntityIdentifier> source,
        std::vector<igesio::Matrix4d> transforms) {
    return std::make_shared<InstancedEntity>(
            std::move(source), std::move(transforms));
}

std::shared_ptr<InstancedEntity> MakeInstancedEntity(
        std::shared_ptr<const entities::IEntityIdentifier> source,
        const igesio::Matrix4d& transform) {
    return std::make_shared<InstancedEntity>(
            std::move(source), std::vector<igesio::Matrix4d>{transform});
}

std::shared_ptr<InstancedEntity> MakeInstancedAssembly(
        const models::Assembly& template_root,
        std::vector<igesio::Matrix4d> transforms) {
    std::vector<InstancedMember> members;
    // ルート自身の大域変換は基準フレームの原点として扱い、累積へ含めない
    FlattenAssembly(template_root, igesio::Matrix4d::Identity(), members);
    return std::make_shared<InstancedEntity>(
            std::move(members), std::move(transforms));
}

}  // namespace igesio::extensions::inspection
