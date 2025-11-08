/**
 * @file entities/surfaces/ruled_surface.cpp
 * @brief Ruled Surface (Type 118): ルールド曲面エンティティの定義
 * @author Yayoi Habami
 * @date 2025-10-14
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/surfaces/ruled_surface.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "igesio/numerics/tolerance.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using i_ent::RuledSurface;
using igesio::Vector3d;

}  // namespace



/**
 * コンストラクタ
 */

RuledSurface::RuledSurface(
        const RawEntityDE& de_record, const IGESParameterVector& parameters,
        const pointer2ID& de2id, const ObjectID& iges_id)
        : EntityBase(de_record, parameters, de2id, iges_id),
          curve1_(IDGenerator::UnsetID()), curve2_(IDGenerator::UnsetID()) {
    InitializePD(de2id);
}

RuledSurface::RuledSurface(
        const IGESParameterVector& parameters)
        : RuledSurface(
            RawEntityDE::ByDefault(i_ent::EntityType::kRuledSurface),
            parameters, {}, IDGenerator::UnsetID()) {}

RuledSurface::RuledSurface(const std::shared_ptr<ICurve>& curve1,
                           const std::shared_ptr<ICurve>& curve2,
                           const bool is_reversed, const bool is_developable)
        : RuledSurface({curve1->GetID(), curve2->GetID(),
                        is_reversed, is_developable}) {
    // nullptrの確認
    if (curve1 == nullptr || curve2 == nullptr) {
        throw std::invalid_argument("curve1 and curve2 of RuledSurface "
                                    "must not be nullptr.");
    }

    // 参照の解決
    SetCurve1(curve1);
    SetCurve2(curve2);
}



/**
 * EntityBase implementation
 */

igesio::IGESParameterVector
RuledSurface::GetMainPDParameters() const {
    return {
        curve1_.GetID(), curve2_.GetID(),
        is_reversed_, is_developable_
    };
}

size_t RuledSurface::SetMainPDParameters(const pointer2ID& de2id) {
    auto& pd = pd_parameters_;

    if (pd.size() < 4) {
        throw std::runtime_error(
            "Insufficient parameters for RuledSurface entity.");
    }

    // 曲線1,2のポインタを設定
    try {
        curve1_ = PointerContainer<false, ICurve>(
                GetObjectIDFromParameters(pd, 0, de2id));
        curve2_ = PointerContainer<false, ICurve>(
                GetObjectIDFromParameters(pd, 1, de2id));
    } catch (const std::exception& e) {
        throw igesio::DataFormatError("Failed to set curve pointers "
            "in RuledSurface entity: " + std::string(e.what()));
    }

    // 反転フラグ, 可展面フラグを設定
    is_reversed_ = pd.access_as<bool>(2);
    is_developable_ = pd.access_as<bool>(3);

    return 4;
}

std::unordered_set<igesio::ObjectID>
RuledSurface::GetUnresolvedPDReferences() const {
    std::unordered_set<ObjectID> unresolved;

    if (!curve1_.IsPointerSet()) {
        unresolved.insert(curve1_.GetID());
    }
    if (!curve2_.IsPointerSet()) {
        unresolved.insert(curve2_.GetID());
    }
    return unresolved;
}

bool RuledSurface::SetUnresolvedPDReferences(
        const std::shared_ptr<const EntityBase>& entity) {
    // 指定されたエンティティがnullptrの場合は失敗
    if (!entity) {
        return false;
    }

    if (entity->GetID() == curve1_.GetID()) {
        if (!curve1_.IsPointerSet()) {
            // ポインタが未設定の場合のみ設定
            if (auto ptr = std::dynamic_pointer_cast<const ICurve>(entity)) {
                return curve1_.SetPointer(ptr);
            }
        }
    } else if (entity->GetID() == curve2_.GetID()) {
        if (!curve2_.IsPointerSet()) {
            // ポインタが未設定の場合のみ設定
            if (auto ptr = std::dynamic_pointer_cast<const ICurve>(entity)) {
                return curve2_.SetPointer(ptr);
            }
        }
    }

    // 指定されたエンティティと同一のIDを持つ参照がない場合
    return false;
}

std::vector<igesio::ObjectID> RuledSurface::GetChildIDs() const {
    std::vector<ObjectID> ids;
    ids.push_back(curve1_.GetID());
    ids.push_back(curve2_.GetID());
    return ids;
}

std::shared_ptr<const i_ent::EntityBase>
RuledSurface::GetChildEntity(const ObjectID& id) const {
    if (curve1_.GetID() == id) {
        auto ptr = curve1_.TryGetEntity<EntityBase>();
        if (ptr) return ptr.value();
    } else if (curve2_.GetID() == id) {
        auto ptr = curve2_.TryGetEntity<EntityBase>();
        if (ptr) return ptr.value();
    }

    // 指定されたIDを持つ子エンティティが存在しない場合
    return nullptr;
}

igesio::ValidationResult RuledSurface::ValidatePD() const {
    std::vector<ValidationError> errors;

    // 曲線1
    if (!curve1_.IsPointerSet()) {
        errors.emplace_back("Curve1 pointer is not set.");
    } else {
        auto curve1_opt = curve1_.TryGetEntity<EntityBase>();
        if (!curve1_opt) {
            errors.emplace_back("Curve1 entity (ID: ")
                << ToString(curve1_.GetID()) << ") is not an EntityBase.";
        } else {
            auto result = curve1_opt.value()->Validate();
            if (!result.is_valid) {
                errors.emplace_back("Curve1 entity (ID: ")
                    << ToString(curve1_.GetID()) << ") is invalid: " << result.Message();
            }
        }
    }

    // 曲線2
    if (!curve2_.IsPointerSet()) {
        errors.emplace_back("Curve2 pointer is not set.");
    } else {
        auto curve2_opt = curve2_.TryGetEntity<EntityBase>();
        if (!curve2_opt) {
            errors.emplace_back("Curve2 entity (ID: ")
                << ToString(curve2_.GetID()) << ") is not an EntityBase.";
        } else {
            auto result = curve2_opt.value()->Validate();
            if (!result.is_valid) {
                errors.emplace_back("Curve2 entity (ID: ")
                    << ToString(curve2_.GetID()) << ") is invalid: " << result.Message();
            }
        }
    }

    // 曲線1と曲線2が同じエンティティであってはならない
    if (curve1_.GetID() == curve2_.GetID()) {
        errors.emplace_back("Curve1 and Curve2 must be different entities.")
            << " (Both are ID: " << ToString(curve1_.GetID()) << ")";
    }

    return MakeValidationResult(errors);
}



/**
 * ISurface implementation
 */

std::optional<i_ent::SurfaceDerivatives>
RuledSurface::TryGetDerivatives(
        const double u, const double v, const unsigned int order) const {
    // ポインタの確認
    if (!curve1_.IsPointerSet() || !curve2_.IsPointerSet()) {
        return std::nullopt;
    }

    // パラメータの範囲チェック
    auto [umin, umax, vmin, vmax] = GetParameterRange();
    if (!(umin <= u && u <= umax && vmin <= v && v <= vmax)) {
        return std::nullopt;
    }

    // 曲線の導関数を取得
    auto [t, s] = GetParametersTS(u);
    auto deriv1_opt = GetCurve1()->TryGetDerivatives(t, order);
    auto deriv2_opt = GetCurve2()->TryGetDerivatives(s, order);
    if (!deriv1_opt || !deriv2_opt)  return std::nullopt;
    const auto& deriv1 = deriv1_opt.value();
    const auto& deriv2 = deriv2_opt.value();

    // 曲線の範囲を取得 : Δt, Δs
    auto [tmin, tmax] = GetCurve1()->GetParameterRange();
    auto [smin, smax] = GetCurve2()->GetParameterRange();
    auto dt = tmax - tmin;
    auto ds = smax - smin;

    // 反転フラグを取得
    auto sigma = is_reversed_ ? -1.0 : 1.0;

    // サーフェスの導関数を計算
    // 計算式についてはdocs/entities/surfaces/118_ruled_surface_ja.mdを参照
    SurfaceDerivatives s_deriv(order);
    for (unsigned int n_u = 0; n_u <= order; ++n_u) {
        auto max_n_v = order - n_u;
        if (max_n_v >= 0) {
            s_deriv(n_u, 0) = (1 - v) * deriv1[n_u] * std::pow(dt, n_u)
                            + v * deriv2[n_u] * std::pow(sigma * ds, n_u);
        }
        if (max_n_v >= 1) {
            s_deriv(n_u, 1) = -1.0 * deriv1[n_u] * std::pow(dt, n_u)
                            + deriv2[n_u] * std::pow(sigma * ds, n_u);
        }
        // n_v >= 2については、S^(n_u, n_v) = 0となるため計算不要
    }

    return s_deriv;
}

i_num::BoundingBox RuledSurface::GetDefinedBoundingBox() const {
    // ポインタの確認
    if (!curve1_.IsPointerSet() || !curve2_.IsPointerSet()) {
        return i_num::BoundingBox();
    }

    // 曲線1,2のバウンディングボックスを取得
    auto bbox1 = GetCurve1()->GetBoundingBox();
    auto bbox2 = GetCurve2()->GetBoundingBox();
    bbox1.ExpandToInclude(bbox2);
    return bbox1;
}



/**
 * 要素の変更・取得
 */

std::shared_ptr<const i_ent::ICurve> RuledSurface::GetCurve1() const {
    auto ptr = curve1_.TryGetEntity<ICurve>();
    if (!ptr) {
        throw std::runtime_error("Curve1 pointer is not set or not an ICurve.");
    }
    return ptr.value();
}

void RuledSurface::SetCurve1(const std::shared_ptr<ICurve>& curve) {
    if (curve == nullptr) {
        throw std::invalid_argument("curve1 of RuledSurface must not be nullptr.");
    }
    if (curve1_.GetID() == curve->GetID()) {
        curve1_.SetPointer(curve);
    } else {
        curve1_.OverwritePointer(curve);
    }

    // SubordinateEntitySwitchをkPhysicallyDependentに設定
    if (auto entity_base = std::dynamic_pointer_cast<EntityBase>(curve)) {
        entity_base->SetSubordinateEntitySwitch(
            SubordinateEntitySwitch::kPhysicallyDependent);
    }
}

std::shared_ptr<const i_ent::ICurve> RuledSurface::GetCurve2() const {
    auto ptr = curve2_.TryGetEntity<ICurve>();
    if (!ptr) {
        throw std::runtime_error("Curve2 pointer is not set or not an ICurve.");
    }
    return ptr.value();
}

void RuledSurface::SetCurve2(const std::shared_ptr<ICurve>& curve) {
    if (curve == nullptr) {
        throw std::invalid_argument("curve2 of RuledSurface must not be nullptr.");
    }
    if (curve2_.GetID() == curve->GetID()) {
        curve2_.SetPointer(curve);
    } else {
        curve2_.OverwritePointer(curve);
    }

    // SubordinateEntitySwitchをkPhysicallyDependentに設定
    if (auto entity_base = std::dynamic_pointer_cast<EntityBase>(curve)) {
        entity_base->SetSubordinateEntitySwitch(
            SubordinateEntitySwitch::kPhysicallyDependent);
    }
}



/**
 * private methods
 */

std::pair<double, double> RuledSurface::GetParametersTS(const double u) const {
    // TODO: フォーム番号が0の際に、弧長さに基づいたパラメータ変換を行う (see Section 4.17)
    auto curve1_opt = curve1_.TryGetEntity<ICurve>();
    auto curve2_opt = curve2_.TryGetEntity<ICurve>();
    // ポインタが未設定の場合
    if (!curve1_opt || !curve2_opt) return {0.0, 0.0};

    auto range1 = curve1_opt.value()->GetParameterRange();
    auto range2 = curve2_opt.value()->GetParameterRange();

    if (is_reversed_) {
        // sの範囲を反転
        return {range1[0] + u * (range1[1] - range1[0]),
                range2[1] - u * (range2[1] - range2[0])};
    }
    return {range1[0] + u * (range1[1] - range1[0]),
            range2[0] + u * (range2[1] - range2[0])};
}
