/**
 * @file models/flatten.cpp
 * @brief Assemblyの配置を永続化(フラット化)するための関数
 * @author Yayoi Habami
 * @date 2026-05-28
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/models/flatten.h"

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "igesio/entities/factory.h"



namespace {

namespace i_ent = igesio::entities;
namespace i_models = igesio::models;
using igesio::Matrix4d;
using igesio::ObjectID;
using i_models::Assembly;
using EntityPtr = std::shared_ptr<i_ent::EntityBase>;
using EntityIndex = std::unordered_map<ObjectID, EntityPtr>;

/// @brief Assembly以下の所有エンティティをID->ポインタの索引へ集約する
/// @param node 対象ノード
/// @param[out] index 集約先の索引
/// @note 索引は被参照closureの解決用のため、IGESエンティティ(EntityBase)のみを
///       集約する (EntityBaseが非IGESエンティティを参照することはない)
void CollectIndex(const Assembly& node, EntityIndex& index) {
    for (const auto& [id, entity] : node.GetEntities()) {
        if (auto eb = std::dynamic_pointer_cast<i_ent::EntityBase>(entity)) {
            index[id] = eb;
        }
    }
    for (const auto& child : node.GetChildAssemblies()) {
        if (child) CollectIndex(*child, index);
    }
}

/// @brief エンティティを一度だけ出力ルートへ追加する
/// @param entity 追加するエンティティ
/// @param[in,out] out_root 出力先ルートAssembly
/// @param[in,out] visited 追加済みIDの集合
void AddOnce(const EntityPtr& entity, Assembly& out_root,
             std::unordered_set<ObjectID>& visited) {
    if (!entity || visited.count(entity->GetID())) return;
    visited.insert(entity->GetID());
    out_root.AddEntity(entity);
}

/// @brief 複製元エンティティとその被参照closureを(共有のまま)出力ルートへ追加する
/// @param entity 追加するエンティティ(複製元)
/// @param[in,out] out_root 出力先ルートAssembly
/// @param index 複製元の全エンティティ索引
/// @param[in,out] visited 追加済みIDの集合
void AddOriginalClosure(const EntityPtr& entity, Assembly& out_root,
                        const EntityIndex& index,
                        std::unordered_set<ObjectID>& visited) {
    if (!entity || visited.count(entity->GetID())) return;
    visited.insert(entity->GetID());
    out_root.AddEntity(entity);
    for (const auto& rid : entity->GetReferencedEntityIDs()) {
        if (visited.count(rid)) continue;
        auto it = index.find(rid);
        if (it != index.end()) AddOriginalClosure(it->second, out_root, index, visited);
    }
}

/// @brief 複製エンティティとその被参照closureを出力ルートへ追加する
/// @param clone 追加する複製エンティティ(一意な新ID)
/// @param[in,out] out_root 出力先ルートAssembly
/// @param index 複製元の全エンティティ索引
/// @param[in,out] visited 追加済みIDの集合
/// @note cloneが参照する新124は事前にAddOnce済みのため、closureでは複製元のみ辿る
void AddCloneClosure(const EntityPtr& clone, Assembly& out_root,
                     const EntityIndex& index,
                     std::unordered_set<ObjectID>& visited) {
    visited.insert(clone->GetID());
    out_root.AddEntity(clone);
    for (const auto& rid : clone->GetReferencedEntityIDs()) {
        if (visited.count(rid)) continue;  // 新124(追加済み)や外部参照
        auto it = index.find(rid);
        if (it != index.end()) AddOriginalClosure(it->second, out_root, index, visited);
    }
}

/// @brief Assembly階層を走査し、独立エンティティをMaterializeして出力ルートへ集約する
/// @param node 対象ノード
/// @param node_world nodeのローカル空間からワールド空間への配置行列
/// @param[in,out] out_root 出力先ルートAssembly
/// @param index 複製元の全エンティティ索引
/// @param[in,out] visited 追加済みIDの集合
/// @param[in,out] skipped_non_iges スキップした非IGESエンティティのIDの格納先
///                (不要な場合はnullptr)
void FlattenAssembly(const Assembly& node, const Matrix4d& node_world,
                     Assembly& out_root, const EntityIndex& index,
                     std::unordered_set<ObjectID>& visited,
                     std::vector<ObjectID>* skipped_non_iges) {
    for (const auto& [id, entity] : node.GetEntities()) {
        if (!entity) continue;
        // フラット化はIGES出力(DE変換への畳み込み)前提のため、IGESエンティティ
        // (EntityBase)のみを対象とする. 非IGESエンティティはスキップし、IDを報告する
        const auto eb = std::dynamic_pointer_cast<i_ent::EntityBase>(entity);
        if (!eb) {
            if (skipped_non_iges != nullptr) skipped_non_iges->push_back(id);
            continue;
        }
        // 物理従属の子は親の畳み込んだ変換を介して追従する(被参照closureで追加される)
        if (eb->GetSubordinateEntitySwitch()
                == i_ent::SubordinateEntitySwitch::kPhysicallyDependent) {
            continue;
        }
        // 変換行列(124)自体は参照経由で扱う
        if (eb->GetType() == i_ent::EntityType::kTransformationMatrix) continue;

        auto res = i_models::Materialize(*eb, node_world);
        AddOnce(res.transform, out_root, visited);
        AddCloneClosure(res.entity, out_root, index, visited);
    }
    for (const auto& child : node.GetChildAssemblies()) {
        if (child) {
            FlattenAssembly(*child, node_world * child->GetGlobalTransform(),
                            out_root, index, visited, skipped_non_iges);
        }
    }
}

}  // namespace



i_models::MaterializeResult i_models::Materialize(
        const i_ent::EntityBase& entity, const Matrix4d& placement) {
    auto clone = i_ent::CloneEntity(entity);

    // 配置が単位行列なら124合成は不要(複製は元の124参照を共有する)
    if (placement.isApprox(Matrix4d::Identity())) {
        return {clone, nullptr};
    }

    // 新124 = placement · M_entity を生成し、複製の変換を差し替える
    const Matrix4d folded =
            placement * entity.GetTransformationMatrix().GetTransformation();
    auto transform = i_ent::MakeTransformationMatrix(folded);
    clone->OverwriteTransformationMatrix(transform);
    return {clone, transform};
}

i_models::IgesData i_models::Flatten(const IgesData& src) {
    IgesData out;
    out.description = src.description;
    out.global_section = src.global_section;

    // 複製時の被参照解決用に、src全エンティティのID->ポインタ索引を作る
    EntityIndex index;
    CollectIndex(src.Root(), index);

    std::unordered_set<ObjectID> visited;
    FlattenAssembly(src.Root(), src.Root().GetGlobalTransform(),
                    out.Root(), index, visited, nullptr);
    return out;
}

i_models::IgesData i_models::Flatten(const IgesData& src,
                                     std::vector<ObjectID>& skipped_non_iges) {
    IgesData out;
    out.description = src.description;
    out.global_section = src.global_section;

    EntityIndex index;
    CollectIndex(src.Root(), index);

    std::unordered_set<ObjectID> visited;
    FlattenAssembly(src.Root(), src.Root().GetGlobalTransform(),
                    out.Root(), index, visited, &skipped_non_iges);
    return out;
}
