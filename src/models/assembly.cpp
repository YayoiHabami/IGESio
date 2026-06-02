/**
 * @file models/assembly.cpp
 * @brief エンティティの集合を表すアセンブリクラス
 * @author Yayoi Habami
 * @date 2026-05-28
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/models/assembly.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <stdexcept>
#include <unordered_set>
#include <vector>

#include "igesio/common/parallel.h"
#include "igesio/entities/views/curve_view.h"
#include "igesio/entities/views/surface_view.h"

namespace {

namespace i_models = igesio::models;
namespace i_ent = igesio::entities;
using Assembly = i_models::Assembly;
using i_models::CoordFrame;
using igesio::Matrix4d;
using igesio::Vector3d;

/// @brief ノードのローカル空間からワールド空間への配置行列を計算する
/// @param node 対象ノード
/// @return ルートまでの大域変換の積 G_root·…·G_node (nodeのG_node自身を含む)
Matrix4d WorldPlacementOf(const Assembly* node) {
    Matrix4d acc = Matrix4d::Identity();
    while (node) {
        acc = node->GetGlobalTransform() * acc;
        node = node->GetParent().lock().get();
    }
    return acc;
}

/// @brief エンティティ所有Assemblyから指定フレームへの配置行列を計算する
/// @param owner エンティティを所有するAssembly
/// @param frame 取得したいフレーム
/// @return 配置行列. frameがkDefinitionの場合は配置概念がないため`std::nullopt`
/// @throw std::invalid_argument frameがRelativeToで、基準がownerの祖先でない場合
std::optional<Matrix4d> ComputePlacement(const Assembly* owner,
                                         const CoordFrame& frame) {
    if (frame.GetKind() == CoordFrame::Kind::kDefinition) return std::nullopt;
    if (frame.GetKind() == CoordFrame::Kind::kEntityLocal) {
        return Matrix4d::Identity();
    }

    const bool relative = (frame.GetKind() == CoordFrame::Kind::kRelative);
    const auto& base = frame.GetRelativeBase();  // kRelativeのときのみ意味を持つ

    Matrix4d acc = Matrix4d::Identity();
    const Assembly* node = owner;
    while (node != nullptr) {
        // RelativeTo: 基準ノードに到達したら、そのG_kは適用せず終了
        if (relative && node->GetID() == base) return acc;
        acc = node->GetGlobalTransform() * acc;
        node = node->GetParent().lock().get();
    }
    // ルートを越えた. RelativeToで基準に到達しなかった場合は祖先でない
    if (relative) {
        throw std::invalid_argument(
            "RelativeTo base is not an ancestor of the entity owner");
    }
    return acc;  // kWorld (G_rootまで積算済み)
}

/// @brief ノード以下の幾何メンバのワールド空間BB頂点を収集する
/// @param node 対象ノード
/// @param node_world nodeのローカル空間からワールド空間への配置行列
/// @param[out] out 収集した有限頂点の格納先
/// @note 幾何かつ物理従属でないメンバのみを対象とし、退化BB(点・直線状)は除外する.
void CollectWorldVertices(const Assembly& node, const Matrix4d& node_world,
                          std::vector<Vector3d>& out) {
    for (const auto& [id, entity] : node.GetEntities()) {
        if (!entity) continue;
        // 物理従属のメンバは親経由で辿るため、ここでは対象としない
        if (entity->GetSubordinateEntitySwitch()
                == i_ent::SubordinateEntitySwitch::kPhysicallyDependent) {
            continue;
        }
        const auto geom = std::dynamic_pointer_cast<const i_ent::IGeometry>(entity);
        if (!geom) continue;

        // 点・直線状(0/1次元)に退化したメンバはワールドBBに寄与しないため除外する
        const auto defined = geom->GetDefinedBoundingBox();
        if (defined.Dimension() < 2) continue;

        const auto bb = geom->GetBoundingBox(node_world);
        for (const auto& v : bb.GetFiniteVertices()) out.push_back(v);
    }

    // 子Assemblyへは、自ノードのworld配置に子の大域変換を後段で掛けて降ろす
    for (const auto& child : node.GetChildAssemblies()) {
        if (!child) continue;
        CollectWorldVertices(*child, node_world * child->GetGlobalTransform(), out);
    }
}

}  // namespace



/**
 * 内部ヘルパ (ルート探索・逆引きインデックス)
 */

Assembly* Assembly::RootRaw() {
    Assembly* node = this;
    while (auto parent = node->parent_.lock()) {
        node = parent.get();
    }
    return node;
}

const Assembly* Assembly::RootRaw() const {
    const Assembly* node = this;
    while (auto parent = node->parent_.lock()) {
        node = parent.get();
    }
    return node;
}

void Assembly::RegisterInIndex(const ObjectID& id, Assembly* owner) {
    RootRaw()->entity_index_[id] = owner;
}

void Assembly::ReindexInto(Assembly* root) {
    for (const auto& [id, entity] : entities_) {
        root->entity_index_[id] = this;
    }
    for (const auto& child : children_) {
        child->ReindexInto(root);
    }
}



/**
 * エンティティの管理 (IgesDataから移設. 対象はこのノードのentities_のみ)
 */

void Assembly::SetPointerIfUnset(std::shared_ptr<entities::EntityBase> entity) {
    // nullptrチェック
    if (!entity) return;

    // entityが参照するエンティティのうち、ポインタが未設定のもののIDを取得
    for (auto& id : entity->GetUnresolvedReferences()) {
        auto it = entities_.find(id);
        if (it != entities_.end()) {
            // その未設定のエンティティを自身が持っている場合は、
            // entityにそのエンティティのポインタを設定する
            entity->SetUnresolvedReference(it->second);
        }
    }

    // entities_に登録されている各要素について、entityを参照していて
    // entityへのポインタが未設定の場合は設定
    for (const auto& [id, ent] : entities_) {
        for (auto& ptr : ent->GetUnresolvedReferences()) {
            if (ptr == entity->GetID()) {
                // entityへの参照ポインタを設定
                ent->SetUnresolvedReference(entity);
            }
        }
    }
}

void Assembly::AddEntities(
        const std::vector<std::shared_ptr<entities::EntityBase>>& entities) {
    // 事前にreserveしてリハッシュを抑える
    entities_.reserve(entities_.size() + entities.size());

    // まず全エンティティをマップとルート逆引きインデックスへ登録する
    for (const auto& entity : entities) {
        if (!entity) {
            throw std::invalid_argument("Entity pointer is null");
        }
        const auto id = entity->GetID();
        entities_[id] = entity;
        RegisterInIndex(id, this);
    }

    // 全件登録後に参照解決を1回だけ行う (O(エンティティ数+参照数)).
    // 各エンティティの未解決参照のうち、このノードが持つものを解決することで、
    // 前方参照・後方参照の双方が一括で解決される. 1件ずつAddEntityを呼ぶ場合に
    // 生じる挿入ごとの全件走査 (O(N^2)) を回避するための経路.
    for (const auto& [id, ent] : entities_) {
        for (const auto& rid : ent->GetUnresolvedReferences()) {
            auto it = entities_.find(rid);
            if (it != entities_.end()) {
                ent->SetUnresolvedReference(it->second);
            }
        }
    }
}

bool Assembly::AreAllReferencesSet() const {
    auto unresolved = GetUnresolvedReferences();
    return unresolved.empty();
}

std::unordered_set<igesio::ObjectID>
Assembly::GetUnresolvedReferences() const {
    std::unordered_set<ObjectID> unresolved_ids;

    for (const auto& [id, entity] : entities_) {
        auto unset = entity->GetUnresolvedReferences();
        unresolved_ids.insert(unset.begin(), unset.end());

        // entityは保持するが、このノードに登録されていないポインタがないか確認
        for (const auto& ptr_id : entity->GetReferencedEntityIDs()) {
            if (entities_.find(ptr_id) == entities_.end()) {
                // このノードに登録されていないポインタ
                unresolved_ids.insert(ptr_id);
            }
        }
    }
    return unresolved_ids;
}

std::shared_ptr<igesio::entities::EntityBase>
Assembly::GetEntity(const ObjectID& id) const {
    auto it = entities_.find(id);
    if (it != entities_.end()) {
        return it->second;
    }
    return nullptr;  // 存在しない場合はnullptrを返す
}

bool Assembly::IsReady() const {
    // すべての参照が設定されているか確認
    if (!AreAllReferencesSet()) {
        return false;
    }

    // すべてのエンティティが有効であることを確認
    for (const auto& [id, entity] : entities_) {
        if (!entity->IsValid()) {
            return false;  // 無効なエンティティがある場合は`false`
        }
    }
    return true;  // すべての条件を満たしている場合は`true`
}

igesio::ValidationResult Assembly::Validate() const {
    ValidationResult result;

    // 参照が未設定のエンティティがないか確認
    for (const auto& id : GetUnresolvedReferences()) {
        result.AddError(ValidationError(
            "Reference to the entity with ID " + ToString(id) +
            " is unresolved. Please add a pointer to this entity "
            "from `AddEntity` function."));
    }

    // すべてのエンティティの検証を実行
    for (const auto& [id, entity] : entities_) {
        auto validation = entity->Validate();
        if (!validation.is_valid) {
            result.Merge(validation);
        }
    }
    return result;
}



/**
 * ツリー・変換
 */

void Assembly::AddChildAssembly(const std::shared_ptr<Assembly>& child) {
    if (!child) {
        throw std::invalid_argument("Child assembly pointer is null");
    }

    // 親を自身に設定し、子として登録する
    child->parent_ = weak_from_this();
    children_.push_back(child);

    // 子とその子孫のエンティティをルートの逆引きインデックスへ登録する
    child->ReindexInto(RootRaw());
}

std::shared_ptr<Assembly> Assembly::Root() {
    auto self = shared_from_this();
    while (auto parent = self->parent_.lock()) {
        self = parent;
    }
    return self;
}



/**
 * 子要素のクエリ
 */

Assembly* Assembly::FindOwner(const ObjectID& id) const {
    const Assembly* root = RootRaw();
    auto it = root->entity_index_.find(id);
    if (it != root->entity_index_.end()) {
        return it->second;
    }
    return nullptr;
}

bool Assembly::IsEffectivelySelectable(const ObjectID& id) const {
    const Assembly* node = FindOwner(id);
    // ツリーに属さないIDは制限しない (選択可)
    if (node == nullptr) return true;
    // 所有ノードからrootまで lock.selectable をANDで畳む
    for (const Assembly* n = node; n != nullptr; n = n->GetParent().lock().get()) {
        if (!n->Metadata().lock.selectable) return false;
    }
    return true;
}

bool Assembly::IsEffectivelyEditable(const ObjectID& id) const {
    // ツリーに属さないIDは制限しない (編集可)
    return IsChainEditable(FindOwner(id));
}

std::vector<igesio::ObjectID>
Assembly::GetEntityIDs(const bool recursive) const {
    std::vector<ObjectID> ids;
    ids.reserve(entities_.size());
    for (const auto& [id, entity] : entities_) {
        ids.push_back(id);
    }
    if (recursive) {
        for (const auto& child : children_) {
            auto child_ids = child->GetEntityIDs(true);
            ids.insert(ids.end(), child_ids.begin(), child_ids.end());
        }
    }
    return ids;
}

std::vector<std::shared_ptr<igesio::entities::EntityBase>>
Assembly::FindEntities(
        const std::function<bool(const entities::EntityBase&)>& predicate,
        const bool recursive) const {
    std::vector<std::shared_ptr<entities::EntityBase>> result;
    for (const auto& [id, entity] : entities_) {
        if (predicate(*entity)) {
            result.push_back(entity);
        }
    }
    if (recursive) {
        for (const auto& child : children_) {
            auto sub = child->FindEntities(predicate, true);
            result.insert(result.end(), sub.begin(), sub.end());
        }
    }
    return result;
}

std::vector<std::shared_ptr<igesio::entities::EntityBase>>
Assembly::FindEntitiesByType(const entities::EntityType type,
                             const bool recursive) const {
    return FindEntities([type](const entities::EntityBase& entity) {
        return entity.GetType() == type;
    }, recursive);
}

std::vector<std::shared_ptr<igesio::entities::EntityBase>>
Assembly::FindEntitiesByUseFlag(const entities::EntityUseFlag flag,
                                const bool recursive) const {
    return FindEntities([flag](const entities::EntityBase& entity) {
        return entity.GetEntityUseFlag() == flag;
    }, recursive);
}



/**
 * 編集・ライフサイクル (TODO 3-3)
 */

bool Assembly::IsInSubtree(const Assembly* node) const {
    for (; node != nullptr; node = node->GetParent().lock().get()) {
        if (node == this) return true;
    }
    return false;
}

bool Assembly::IsChainEditable(const Assembly* node) const {
    for (; node != nullptr; node = node->GetParent().lock().get()) {
        if (!node->Metadata().lock.editable) return false;
    }
    return true;
}

void Assembly::EraseEntity(Assembly* owner, const ObjectID& id) {
    owner->entities_.erase(id);
    RootRaw()->entity_index_.erase(id);
}

std::vector<igesio::ObjectID>
Assembly::FindReferrers(const ObjectID& id) const {
    std::vector<ObjectID> referrers;
    // ルートの逆引きインデックスを使い、全子孫エンティティを走査する
    const Assembly* root = RootRaw();
    for (const auto& [eid, owner] : root->entity_index_) {
        if (eid == id) continue;  // 自己参照は対象外
        const auto entity = owner->GetEntity(eid);
        if (!entity) continue;
        for (const auto& ref : entity->GetReferencedEntityIDs()) {
            if (ref == id) {
                referrers.push_back(eid);
                break;
            }
        }
    }
    return referrers;
}

void Assembly::RemoveCascade(const ObjectID& start) {
    std::unordered_set<ObjectID> visited;
    std::vector<ObjectID> stack{start};
    while (!stack.empty()) {
        const ObjectID cur = stack.back();
        stack.pop_back();
        if (!visited.insert(cur).second) continue;  // 既処理

        Assembly* owner = FindOwner(cur);
        // サブツリー外/未存在は触らない
        if (owner == nullptr || !IsInSubtree(owner)) continue;
        if (const auto entity = owner->GetEntity(cur)) {
            // 物理従属子を閉包に追加
            for (const auto& cid : entity->GetChildIDs()) stack.push_back(cid);
        }
        // 参照元も閉包に追加 (除去前に収集)
        for (const auto& rid : FindReferrers(cur)) stack.push_back(rid);
        EraseEntity(owner, cur);
    }
}

bool Assembly::RemoveEntity(const ObjectID& id, const RemovalPolicy policy) {
    Assembly* owner = FindOwner(id);
    if (owner == nullptr) return false;        // ツリーに存在しない
    if (!IsInSubtree(owner)) return false;     // このサブツリー外
    if (!IsEffectivelyEditable(id)) return false;  // ロック(編集不可)

    switch (policy) {
        case RemovalPolicy::kReject:
            // 参照が残るなら無変更で拒否する
            if (!FindReferrers(id).empty()) return false;
            EraseEntity(owner, id);
            return true;
        case RemovalPolicy::kOrphan:
            // 参照は未解決のまま(被参照weak_ptrは自然失効)
            EraseEntity(owner, id);
            return true;
        case RemovalPolicy::kCascade:
            RemoveCascade(id);
            return true;
    }
    return false;
}

bool Assembly::RemoveChildAssembly(const ObjectID& child_id,
                                   const RemovalPolicy policy) {
    // 直接の子から該当を探す
    auto it = std::find_if(children_.begin(), children_.end(),
            [&](const std::shared_ptr<Assembly>& c) {
                return c && c->GetID() == child_id;
            });
    if (it == children_.end()) return false;
    Assembly* child = it->get();
    if (!IsChainEditable(child)) return false;  // ロック(編集不可)

    // サブツリー内の全エンティティと、外部(サブツリー外)からの参照(inbound)を収集する
    const auto inside = child->GetEntityIDs(/*recursive=*/true);
    const std::unordered_set<ObjectID> inside_set(inside.begin(), inside.end());
    std::vector<ObjectID> outside_referrers;
    for (const auto& eid : inside) {
        for (const auto& rid : FindReferrers(eid)) {
            if (inside_set.find(rid) == inside_set.end()) {
                outside_referrers.push_back(rid);
            }
        }
    }

    // kReject: 境界跨ぎ参照(外部→サブツリー内)が残るなら無変更で拒否する
    if (policy == RemovalPolicy::kReject && !outside_referrers.empty()) {
        return false;
    }

    // サブツリーのエンティティを逆引きから除去し、子をchildren_から外す
    Assembly* root = RootRaw();
    for (const auto& eid : inside) root->entity_index_.erase(eid);
    children_.erase(it);  // shared_ptrが落ち、サブツリーが破棄される

    // kCascade: 外部の参照元も連鎖削除する
    if (policy == RemovalPolicy::kCascade) {
        for (const auto& rid : outside_referrers) {
            RemoveEntity(rid, RemovalPolicy::kCascade);
        }
    }
    return true;
}

void Assembly::Clear() {
    // 自ノード+全子孫のエンティティをルート逆引きから除去する
    Assembly* root = RootRaw();
    for (const auto& eid : GetEntityIDs(/*recursive=*/true)) {
        root->entity_index_.erase(eid);
    }
    entities_.clear();
    children_.clear();
}

void Assembly::MoveEntityTo(const ObjectID& id, Assembly& dest) {
    if (RootRaw() != dest.RootRaw()) {
        throw std::invalid_argument(
            "MoveEntityTo: dest must share the same root");
    }
    Assembly* owner = FindOwner(id);
    if (owner == nullptr) {
        throw std::invalid_argument("MoveEntityTo: entity not found");
    }
    if (owner == &dest) return;  // 既にdest所属

    auto entity = owner->GetEntity(id);
    owner->entities_.erase(id);
    dest.entities_[id] = entity;
    dest.SetPointerIfUnset(entity);        // dest内で参照を張り直す
    RootRaw()->entity_index_[id] = &dest;  // 逆引きownerを更新
}

void Assembly::MoveChildAssemblyTo(const ObjectID& child_id, Assembly& dest) {
    if (RootRaw() != dest.RootRaw()) {
        throw std::invalid_argument(
            "MoveChildAssemblyTo: dest must share the same root");
    }
    auto it = std::find_if(children_.begin(), children_.end(),
            [&](const std::shared_ptr<Assembly>& c) {
                return c && c->GetID() == child_id;
            });
    if (it == children_.end()) {
        throw std::invalid_argument(
            "MoveChildAssemblyTo: not a direct child");
    }
    std::shared_ptr<Assembly> child = *it;

    // 循環防止: destがchild自身またはその子孫である場合は不可
    for (const Assembly* n = &dest; n != nullptr;
         n = n->GetParent().lock().get()) {
        if (n == child.get()) {
            throw std::invalid_argument(
                "MoveChildAssemblyTo: dest is within the moved subtree");
        }
    }

    // children_から外し、destへ付け替える (同一ルートのため逆引きは不変)
    children_.erase(it);
    child->parent_ = dest.weak_from_this();
    dest.children_.push_back(child);
}

void Assembly::SetVisibleRecursive(const bool visible) {
    metadata_.visible = visible;
    for (const auto& child : children_) {
        if (child) child->SetVisibleRecursive(visible);
    }
}

void Assembly::SetSuppressedRecursive(const bool suppressed) {
    metadata_.suppressed = suppressed;
    for (const auto& child : children_) {
        if (child) child->SetSuppressedRecursive(suppressed);
    }
}

void Assembly::SetColorOverrideRecursive(
        const std::optional<std::array<float, 3>>& color) {
    metadata_.color_override = color;
    for (const auto& child : children_) {
        if (child) child->SetColorOverrideRecursive(color);
    }
}

void Assembly::SetOpacityOverrideRecursive(
        const std::optional<float>& opacity) {
    metadata_.opacity_override = opacity;
    for (const auto& child : children_) {
        if (child) child->SetOpacityOverrideRecursive(opacity);
    }
}

void Assembly::ComposeGlobalTransform(const igesio::Matrix4d& transform) {
    // 親フレームでの後付け合成 (左から掛ける)
    global_transform_ = transform * global_transform_;
}

igesio::ValidationResult Assembly::ValidateSelfContainedRecursive() const {
    ValidationResult result;
    // 自ノードの未解決参照(同一ノード内で解決できない参照)を検査する
    for (const auto& ref_id : GetUnresolvedReferences()) {
        result.AddError(ValidationError(
            "Reference to entity " + ToString(ref_id) +
            " is not resolved within its owning Assembly node (ID " +
            ToString(GetID()) + ")."));
    }
    // 全子孫へ再帰
    for (const auto& child : children_) {
        if (child) result.Merge(child->ValidateSelfContainedRecursive());
    }
    return result;
}



/**
 * 座標系・ビュー・空間検索
 */

std::optional<igesio::Matrix4d>
Assembly::ResolvePlacement(const ObjectID& id, const CoordFrame& frame) const {
    const Assembly* owner = FindOwner(id);
    if (!owner) return std::nullopt;
    return ComputePlacement(owner, frame);
}

std::shared_ptr<igesio::entities::CurveView>
Assembly::GetCurveView(const ObjectID& id, const CoordFrame& frame) const {
    if (frame.GetKind() == CoordFrame::Kind::kDefinition) {
        throw std::invalid_argument(
            "kDefinition cannot be used for view generation");
    }

    const Assembly* owner = FindOwner(id);
    if (!owner) return nullptr;
    auto curve = std::dynamic_pointer_cast<const entities::ICurve>(
            owner->GetEntity(id));
    if (!curve) return nullptr;

    const auto placement = ComputePlacement(owner, frame);
    if (!placement) return nullptr;
    return std::make_shared<entities::CurveView>(curve, *placement);
}

std::shared_ptr<igesio::entities::SurfaceView>
Assembly::GetSurfaceView(const ObjectID& id, const CoordFrame& frame) const {
    if (frame.GetKind() == CoordFrame::Kind::kDefinition) {
        throw std::invalid_argument(
            "kDefinition cannot be used for view generation");
    }

    const Assembly* owner = FindOwner(id);
    if (!owner) return nullptr;
    auto surface = std::dynamic_pointer_cast<const entities::ISurface>(
            owner->GetEntity(id));
    if (!surface) return nullptr;

    const auto placement = ComputePlacement(owner, frame);
    if (!placement) return nullptr;
    return std::make_shared<entities::SurfaceView>(surface, *placement);
}

std::optional<igesio::numerics::BoundingBox>
Assembly::GetWorldBoundingBox() const {
    // 自ノードのローカル空間からワールド空間への配置を求める
    const Matrix4d this_world = WorldPlacementOf(this);

    // 子孫の幾何メンバのワールド空間BB頂点を収集する
    std::vector<Vector3d> pts;
    CollectWorldVertices(*this, this_world, pts);
    if (pts.empty()) return std::nullopt;

    // 軸平行AABBへ合成する
    Vector3d lo = pts.front();
    Vector3d hi = pts.front();
    for (const auto& p : pts) {
        lo = lo.cwiseMin(p);
        hi = hi.cwiseMax(p);
    }

    // 幅が0の軸の数を数える. 2軸以上が0(点・直線状)はBBを構成できないため除外する
    // (退化BBはnulloptとする既存ガードに倣う). 1軸のみ0の場合は2次元BBとなる
    const Vector3d sizes = hi - lo;
    int zero_axes = 0;
    if (sizes.x() <= 0.0) ++zero_axes;
    if (sizes.y() <= 0.0) ++zero_axes;
    if (sizes.z() <= 0.0) ++zero_axes;
    if (zero_axes >= 2) return std::nullopt;

    return numerics::BoundingBox(lo, hi);
}

void Assembly::PrepareGeometryCaches(const bool recursive) const {
    // 自ノード(必要なら全子孫)のエンティティをフラットに収集する.
    // 各PrepareGeometryCacheは互いに独立 (それぞれ自身のキャッシュのみ書き込む) なので、
    // ロックなしで並列実行できる. ParallelForEachが戻る前に全ワーカーを待ち合わせる.
    const auto entities = FindEntities(
            [](const i_ent::EntityBase&) { return true; }, recursive);
    igesio::ParallelForEach(
            entities, [](const std::shared_ptr<i_ent::EntityBase>& e) {
        e->PrepareGeometryCache();
    });
}
