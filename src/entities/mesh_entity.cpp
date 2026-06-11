/**
 * @file entities/mesh_entity.cpp
 * @brief 三角形メッシュを保持する非IGESエンティティ
 * @author Yayoi Habami
 * @date 2026-06-10
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/entities/mesh_entity.h"

#include <stdexcept>
#include <utility>

#include "igesio/numerics/mesh/algorithms.h"

namespace {

namespace i_ent = igesio::entities;
namespace i_num = igesio::numerics;

/// @brief メッシュの構造を検証し、不正な場合は例外を投げる
/// @param mesh 検証対象のメッシュ
/// @throw std::invalid_argument meshの構造が不正な場合
void ThrowIfInvalid(const i_num::TriangleMeshd& mesh) {
    const auto result = i_num::Validate(mesh);
    if (!result.is_valid) {
        throw std::invalid_argument(
                "Invalid triangle mesh: " + result.Message());
    }
}

}  // namespace



i_ent::MeshEntity::MeshEntity(numerics::TriangleMeshd mesh)
        : mesh_(std::move(mesh)) {
    ThrowIfInvalid(mesh_);
}

void i_ent::MeshEntity::SetMesh(numerics::TriangleMeshd mesh) {
    ThrowIfInvalid(mesh);
    mesh_ = std::move(mesh);
    // 形状編集としてリビジョンをバンプする (成功経路のみ)
    MarkGeometryModified();
}

igesio::numerics::BoundingBox i_ent::MeshEntity::GetDefinedBoundingBox() const {
    return numerics::ComputeBoundingBox(mesh_);
}

std::optional<igesio::Vector3d> i_ent::MeshEntity::Transform(
        const std::optional<Vector3d>& input,
        [[maybe_unused]] const bool is_point) const {
    // 変換行列を持たないため恒等 (配置はAssemblyの大域変換で行う)
    return input;
}
