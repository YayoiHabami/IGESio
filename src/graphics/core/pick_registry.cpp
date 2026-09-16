/**
 * @file graphics/core/pick_registry.cpp
 * @brief エンティティ型とピック関数 (レイ交差・範囲選択サンプル) のレジストリ
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/graphics/core/pick_registry.h"

#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "igesio/numerics/meshes/algorithms/edges.h"
#include "igesio/numerics/meshes/algorithms/mesh_line_intersection.h"
#include "igesio/entities/meshes/mesh_entity.h"

namespace {

namespace i_graph = igesio::graphics;
namespace i_ent = igesio::entities;
namespace i_num = igesio::numerics;
using igesio::Matrix4d;
using igesio::Vector3d;

/// @brief メッシュの全頂点をワールド座標へ変換する
/// @param mesh 対象のメッシュ
/// @param world_transform ローカル→ワールドの変換行列
/// @return ワールド座標の頂点列 (メッシュの頂点列と同じ順序)
std::vector<Vector3d> TransformMeshVertices(
        const i_num::TriangleMeshd& mesh, const Matrix4d& world_transform) {
    std::vector<Vector3d> world_positions;
    world_positions.reserve(mesh.VertexCount());
    for (Eigen::Index c = 0; c < mesh.positions.cols(); ++c) {
        const Vector3d local = mesh.positions.col(c);
        world_positions.push_back(
                (world_transform * local.homogeneous()).hnormalized());
    }
    return world_positions;
}

/// @brief MeshEntityとレイの交差点を求める (PickRegistryの組み込みseed)
/// @param entity 対象のメッシュエンティティ
/// @param world_transform ローカル→ワールドの変換行列
///        (MeshEntityは自身の変換を持たないため、親空間=ローカル空間)
/// @param ray ワールド空間のレイ (direction正規化済み)
/// @param params 探索制御パラメータ (dedup_tolのみ使用する)
/// @return 交差点のリスト (distance昇順). 変換行列が非可逆の場合は空リスト
/// @note レイをローカル空間へ逆変換して全三角形と判定し、交点をワールドへ戻す
///       (TransformRayToLocal/TransformHitToWorld). 距離はワールド空間で再計算
///       するため、非一様スケールを含む変換でも正しい距離を返す
std::vector<i_graph::RayHit> IntersectMeshEntity(
        const i_ent::MeshEntity& entity, const Matrix4d& world_transform,
        const i_graph::Ray& ray, const i_graph::RayIntersectionParams& params) {
    const auto local = i_graph::TransformRayToLocal(ray, world_transform);
    if (!local) return {};
    const auto& [origin_local, p1_local] = *local;

    i_num::MeshIntersectionParams mesh_params;
    mesh_params.dedup_tol = params.dedup_tol;
    const auto local_hits = i_num::IntersectMeshWithLine(
            entity.Mesh(), origin_local, p1_local,
            i_num::BoundingBox::DirectionType::kRay, mesh_params);

    // ローカルのt昇順はワールドの距離昇順と一致する (同一レイ上の単調変換のため)
    std::vector<i_graph::RayHit> hits;
    hits.reserve(local_hits.size());
    for (const auto& hit : local_hits) {
        hits.push_back(
                i_graph::TransformHitToWorld(hit.position, world_transform, ray));
    }
    return hits;
}

/// @brief MeshEntityの範囲選択サンプルを返す (PickRegistryの組み込みseed)
/// @param entity 対象のメッシュエンティティ
/// @param world_transform ローカル→ワールドの変換行列
/// @return 全頂点を共有頂点プール、全ユニークエッジをインデックス線分とした
///         サンプル (segment_vertices/segmentsチャンネル)
/// @note 三角形は頂点の凸結合のため、内包判定 (全サンプルが矩形内) は
///       頂点のみで正確に判定できる. 交差判定はエッジ線分が担う
///       (矩形が1枚の三角形の内部に完全に収まるケースのみ取りこぼす)
/// @note 頂点ごとの射影が1回で済むインデックス線分表現を使い、エッジ列挙も
///       分類なしの軽量版 (ExtractUniqueEdges) を使う (大規模メッシュ対策)
i_graph::SelectionSamples SampleMeshEntity(
        const i_ent::MeshEntity& entity, const Matrix4d& world_transform,
        const i_graph::SelectionSampleParams&) {
    const auto& mesh = entity.Mesh();

    i_graph::SelectionSamples samples;
    samples.segment_vertices = TransformMeshVertices(mesh, world_transform);
    samples.segments = i_num::ExtractUniqueEdges(mesh);
    return samples;
}

/// @brief レジストリのストレージ
struct PickRegistryStorage {
    /// @brief 動的型とピック関数ペアのマッピング
    std::unordered_map<std::type_index,
                       i_graph::PickRegistry::PickFunctions> functions;
};

/// @brief ストレージを取得する
/// @note 関数内static (Meyersシングルトン). 静的初期化子からの登録
///       (PickAutoRegistrar) でも初期化順に依存せず安全に参照できる
PickRegistryStorage& GetStorage() {
    static PickRegistryStorage storage = [] {
        PickRegistryStorage s;
        // 組み込みのピック対応はここでseedする (コアの組み込み分は静的レジストラに
        // 頼らず、遅延初期化で確実に登録する).
        // NOTE: ここでTryRegister等を呼ぶとGetStorage()の再帰初期化になるため、
        //       必ずローカルのs.functionsへ直接挿入すること
        i_graph::PickRegistry::PickFunctions mesh_picks;
        mesh_picks.intersect =
                [](const i_ent::IEntityIdentifier& entity,
                   const Matrix4d& world_transform,
                   const i_graph::Ray& ray,
                   const i_graph::RayIntersectionParams& params) {
                    return IntersectMeshEntity(
                            dynamic_cast<const i_ent::MeshEntity&>(entity),
                            world_transform, ray, params);
                };
        mesh_picks.selection_samples =
                [](const i_ent::IEntityIdentifier& entity,
                   const Matrix4d& world_transform,
                   const i_graph::SelectionSampleParams& params) {
                    return SampleMeshEntity(
                            dynamic_cast<const i_ent::MeshEntity&>(entity),
                            world_transform, params);
                };
        s.functions.emplace(std::type_index(typeid(i_ent::MeshEntity)),
                            std::move(mesh_picks));
        return s;
    }();
    return storage;
}

}  // namespace



bool i_graph::PickRegistry::RegisterImpl(
        const std::type_index type, PickFunctions functions) {
    auto& storage = GetStorage();
    if (storage.functions.find(type) != storage.functions.end()) {
        // 登録済み: TryRegisterはfalse、Registerは呼び出し側で例外にする
        return false;
    }
    storage.functions.emplace(type, std::move(functions));
    return true;
}

bool i_graph::PickRegistry::UnregisterImpl(const std::type_index type) {
    return GetStorage().functions.erase(type) != 0;
}

bool i_graph::PickRegistry::IsRegisteredImpl(const std::type_index type) {
    auto& storage = GetStorage();
    return storage.functions.find(type) != storage.functions.end();
}

i_graph::PickRegistry::PickFunctions
i_graph::PickRegistry::Find(const std::type_index type) {
    const auto& storage = GetStorage();
    auto it = storage.functions.find(type);
    if (it == storage.functions.end()) return {};
    return it->second;
}
