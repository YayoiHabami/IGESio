/**
 * @file extensions/inspection/instanced_entity.cpp
 * @brief 基準エンティティを変換行列で複製表示する非IGESエンティティ
 * @author Yayoi Habami
 * @date 2026-06-12
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/inspection/instanced_entity.h"

#include <stdexcept>
#include <utility>

namespace igesio::extensions::inspection {

InstancedEntity::InstancedEntity(
        std::shared_ptr<const entities::IEntityIdentifier> source,
        std::vector<igesio::Matrix4d> transforms)
        : source_(std::move(source)), transforms_(std::move(transforms)) {
    if (!source_) {
        throw std::invalid_argument("Source entity pointer cannot be null");
    }
}

numerics::BoundingBox InstancedEntity::GetDefinedBoundingBox() const {
    // 基準形状のBB (基準のDE変換適用済み・モデル空間) を取得する.
    // IGeometryでない基準は包含領域を構成できないため空を返す.
    const auto* geom = dynamic_cast<const entities::IGeometry*>(source_.get());
    if (geom == nullptr || transforms_.empty()) return numerics::BoundingBox();

    // 複製先行列はスケール・せん断を含みうるため、BoundingBoxの直交基底制約を
    // 避けて頂点を直接変換する. 無限に伸びる基準 (無限平面等) は有限頂点が無く
    // 包含領域を構成できないためスキップする.
    const auto vertices = geom->GetBoundingBox().GetFiniteVertices();
    if (vertices.empty()) return numerics::BoundingBox();

    bool initialized = false;
    Vector3d lo = Vector3d::Zero();
    Vector3d hi = Vector3d::Zero();
    for (const auto& transform : transforms_) {
        for (const auto& v : vertices) {
            const Vector3d w = numerics::ApplyTransform(transform, v, true);
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
    if (!initialized) return numerics::BoundingBox();
    return numerics::BoundingBox(lo, hi);
}

std::optional<Vector3d> InstancedEntity::Transform(
        const std::optional<Vector3d>& input,
        [[maybe_unused]] const bool is_point) const {
    // 本エンティティ自身は変換行列を持たない (複製先行列は描画側が適用する)
    return input;
}



/**
 * ファクトリ関数
 */

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

}  // namespace igesio::extensions::inspection
