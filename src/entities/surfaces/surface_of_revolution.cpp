/**
 * @file entities/surfaces/surface_of_revolution.cpp
 * @brief Surface of Revolution (Type 120): 回転曲面エンティティの定義
 * @author Yayoi Habami
 * @date 2025-10-12
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/surfaces/surface_of_revolution.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "igesio/numerics/tolerance.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using i_ent::SurfaceOfRevolution;
using igesio::Vector3d;

}  // namespace



/**
 * コンストラクタ
 */

SurfaceOfRevolution::SurfaceOfRevolution(
        const RawEntityDE& de_record, const IGESParameterVector& parameters,
        const pointer2ID& de2id, const ObjectID& iges_id)
        : EntityBase(de_record, parameters, de2id, iges_id),
          axis_(IDGenerator::UnsetID()), generatrix_(IDGenerator::UnsetID()) {
    InitializePD(de2id);
}

SurfaceOfRevolution::SurfaceOfRevolution(
        const IGESParameterVector& parameters)
        : SurfaceOfRevolution(
            RawEntityDE::ByDefault(i_ent::EntityType::kSurfaceOfRevolution),
            parameters, {}, IDGenerator::UnsetID()) {}

SurfaceOfRevolution::SurfaceOfRevolution(
        const std::shared_ptr<Line>& axis,
        const std::shared_ptr<ICurve>& generatrix,
        const double start_angle, const double end_angle)
        : SurfaceOfRevolution({axis->GetID(), generatrix->GetID(),
                               start_angle, end_angle}) {
    if (axis == nullptr) {
        throw std::invalid_argument(
            "axis of SurfaceOfRevolution must not be nullptr");
    }
    if (generatrix == nullptr) {
        throw std::invalid_argument(
            "generatrix of SurfaceOfRevolution must not be nullptr");
    }

    // 参照の解決
    SetAxis(axis);
    SetGeneratrix(generatrix);
}



/**
 * EntityBase implementation
 */

igesio::IGESParameterVector
SurfaceOfRevolution::GetMainPDParameters() const {
    return {
        axis_.GetID(), generatrix_.GetID(),
        start_angle_, end_angle_
    };
}

size_t SurfaceOfRevolution::SetMainPDParameters(const pointer2ID& de2id) {
    auto& pd = pd_parameters_;

    if (pd.size() < 4) {
        throw igesio::DataFormatError(
            "SurfaceOfRevolution must have at least 4 parameters");
    }

    // 回転軸
    try {
        axis_ = PointerContainer<false, Line>(
            GetObjectIDFromParameters(pd, 0, de2id));
    } catch (const std::exception& e) {
        throw igesio::DataFormatError("Failed to get Axis (Line) ID "
            "from parameters: " + std::string(e.what()));
    }

    // 母線
    try {
        generatrix_ = PointerContainer<false, ICurve>(
            GetObjectIDFromParameters(pd, 1, de2id));
    } catch (const std::exception& e) {
        throw igesio::DataFormatError("Failed to get Generatrix (ICurve) ID "
            "from parameters: " + std::string(e.what()));
    }

    // 回転角度
    start_angle_ = pd.access_as<double>(2);
    end_angle_ = pd.access_as<double>(3);

    return 4;
}

std::unordered_set<igesio::ObjectID>
SurfaceOfRevolution::GetUnresolvedPDReferences() const {
    std::unordered_set<ObjectID> unresolved;

    if (!axis_.IsPointerSet()) {
        unresolved.insert(axis_.GetID());
    }
    if (!generatrix_.IsPointerSet()) {
        unresolved.insert(generatrix_.GetID());
    }
    return unresolved;
}

bool SurfaceOfRevolution::SetUnresolvedPDReferences(
        const std::shared_ptr<const EntityBase>& entity) {
    // 指定されたエンティティがnullptrの場合は失敗
    if (!entity) {
        return false;
    }

    if (entity->GetID() == axis_.GetID()) {
        if (!axis_.IsPointerSet()) {
            // ポインタが未設定の場合のみ設定
            if (auto ptr = std::dynamic_pointer_cast<const Line>(entity)) {
                return axis_.SetPointer(ptr);
            }
        }
    } else if (entity->GetID() == generatrix_.GetID()) {
        if (!generatrix_.IsPointerSet()) {
            // ポインタが未設定の場合のみ設定
            if (auto ptr = std::dynamic_pointer_cast<const ICurve>(entity)) {
                return generatrix_.SetPointer(ptr);
            }
        }
    }

    // 指定されたエンティティと同一のIDを持つ参照がない場合
    return false;
}

std::vector<igesio::ObjectID> SurfaceOfRevolution::GetChildIDs() const {
    std::vector<ObjectID> ids;
    ids.push_back(axis_.GetID());
    ids.push_back(generatrix_.GetID());
    return ids;
}

std::shared_ptr<const i_ent::EntityBase>
SurfaceOfRevolution::GetChildEntity(const ObjectID& id) const {
    if (axis_.GetID() == id) {
        auto ptr = axis_.TryGetEntity<EntityBase>();
        if (ptr) return ptr.value();
    } else if (generatrix_.GetID() == id) {
        auto ptr = generatrix_.TryGetEntity<EntityBase>();
        if (ptr) return ptr.value();
    }

    // 指定されたIDを持つ子エンティティが存在しない場合
    return nullptr;
}

igesio::ValidationResult SurfaceOfRevolution::ValidatePD() const {
    std::vector<ValidationError> errors;

    // 回転軸
    if (!axis_.IsPointerSet()) {
        errors.emplace_back("Axis (Line) pointer is not set.");
    } else {
        auto axis_line = GetAxis();
        auto result = axis_line->Validate();
        if (!result.is_valid) {
            errors.emplace_back("Axis (Line) is invalid: " + result.Message());
        }
    }

    // 母線
    if (!generatrix_.IsPointerSet()) {
        errors.emplace_back("Generatrix (ICurve) pointer is not set.");
    } else {
        auto generatrix_curve = generatrix_.TryGetEntity<EntityBase>();
        if (!generatrix_curve) {
            errors.emplace_back("Generatrix (ICurve) is not an EntityBase.");
        } else {
            auto result = (*generatrix_curve)->Validate();
            if (!result.is_valid) {
                errors.emplace_back("Generatrix (ICurve) is invalid: " + result.Message());
            }
        }
    }

    // 回転角度
    // 0 <= start_angle < end_angle <= 2*pi
    if (!(0.0 <= start_angle_ && start_angle_ < end_angle_ && end_angle_ <= 2.0 * kPi)) {
        errors.emplace_back("Invalid angles: Require 0 <= θstart < θend <= 2*pi, "
                            "but got θstart = " + std::to_string(start_angle_) + "[rad] and "
                            "θend = " + std::to_string(end_angle_) + "[rad].");
    }

    return MakeValidationResult(errors);
}



/**
 * ISurface implementation
 */

bool SurfaceOfRevolution::IsUClosed() const {
    // u方向は母線のパラメータに依存する
    auto generatrix_curve = generatrix_.TryGetEntity<ICurve>();
    if (!generatrix_curve) return false;
    return generatrix_curve.value()->IsClosed();
}

bool SurfaceOfRevolution::IsVClosed() const {
    // v方向は回転角度に依存する
    return i_num::IsApproxEqual(start_angle_, 0.0)
           && i_num::IsApproxEqual(end_angle_, 2.0 * kPi);
}

std::array<double, 4> SurfaceOfRevolution::GetParameterRange() const {
    auto generatrix_curve = generatrix_.TryGetEntity<ICurve>();
    if (!generatrix_curve) {
        // 母線のポインタが未設定の場合、パラメータ範囲は不定
        return {std::numeric_limits<double>::infinity(),
                -std::numeric_limits<double>::infinity(),
                start_angle_, end_angle_};
    }
    auto [umin, umax] = generatrix_curve.value()->GetParameterRange();
    return {umin, umax, start_angle_, end_angle_};
}

std::optional<i_ent::SurfaceDerivatives>
SurfaceOfRevolution::TryGetDerivatives(
        const double u, const double v, const unsigned int order) const {
    // ポインタの確認
    if (!axis_.IsPointerSet() || !generatrix_.IsPointerSet()) {
        return std::nullopt;
    }

    // パラメータ範囲のチェック
    auto [umin, umax, vmin, vmax] = GetParameterRange();
    if (!(umin <= u && u <= umax && vmin <= v && v <= vmax)) {
        return std::nullopt;
    }

    // 回転軸の始点P0と方向ベクトルDを取得
    const auto& [P0, end_point] = GetAxis()->GetAnchorPoints();
    auto D = (end_point - P0).normalized();

    // 母線の偏導関数を取得
    auto curve_deriv_opt = GetGeneratrix()->TryGetDerivatives(u, order);
    if (!curve_deriv_opt) return std::nullopt;
    auto c_deriv = curve_deriv_opt.value();

    // サーフェスの偏導関数を計算
    // 計算式の詳細については[docs/entities/surfaces/120_surface_of_revolution_ja.md]を参照
    SurfaceDerivatives s_deriv(order);
    const double cos_v = std::cos(v);
    const double sin_v = std::sin(v);
    for (unsigned int nu = 0; nu <= order; ++nu) {
        // C(u) - P0 または C^(nu)(u) を計算
        const auto& c_n = c_deriv[nu];
        const auto cp_n = (nu == 0) ? (c_n - P0) : c_n;

        // D との内積・外積
        const double dot_d_cp_n = D.dot(cp_n);
        const auto cross_d_cp_n = D.cross(cp_n);

        for (unsigned int nv = 0; nv <= order - nu; ++nv) {
            if (nu == 0 && nv == 0) {
                // S(u, v)
                s_deriv(0, 0) = P0 + cp_n * cos_v + cross_d_cp_n * sin_v
                              + D * dot_d_cp_n * (1.0 - cos_v);
            } else if (nv == 0) {
                // S^(nu, 0)(u, v)
                s_deriv(nu, 0) = cp_n * cos_v + cross_d_cp_n * sin_v
                               + D * dot_d_cp_n * (1.0 - cos_v);
            } else {
                // S^(nu, nv)(u, v)
                const double angle = v + nv * kPi / 2.0;
                const auto term1 = cp_n - D * dot_d_cp_n;
                s_deriv(nu, nv) = term1 * std::cos(angle) + cross_d_cp_n * std::sin(angle);
            }
        }
    }

    return s_deriv;
}



/**
 * 要素の変更・取得
 */

void SurfaceOfRevolution::SetAxis(const std::shared_ptr<Line>& axis) {
    if (axis == nullptr) {
        throw std::invalid_argument(
            "axis of SurfaceOfRevolution must not be nullptr");
    }
    if (axis->GetID() == axis_.GetID()) {
        axis_.SetPointer(axis);
    } else {
        axis_.OverwritePointer(axis);
    }

    // SubordinateEntitySwitchをkPhysicallyDependentに設定
    if (auto entity_base = std::dynamic_pointer_cast<EntityBase>(axis)) {
        entity_base->SetSubordinateEntitySwitch(
            SubordinateEntitySwitch::kPhysicallyDependent);
    }
}

void SurfaceOfRevolution::SetGeneratrix(const std::shared_ptr<ICurve>& generatrix) {
    if (generatrix == nullptr) {
        throw std::invalid_argument(
            "generatrix of SurfaceOfRevolution must not be nullptr");
    }
    if (generatrix->GetID() == generatrix_.GetID()) {
        generatrix_.SetPointer(generatrix);
    } else {
        generatrix_.OverwritePointer(generatrix);
    }

    // SubordinateEntitySwitchをkPhysicallyDependentに設定
    if (auto entity_base = std::dynamic_pointer_cast<EntityBase>(generatrix)) {
        entity_base->SetSubordinateEntitySwitch(
            SubordinateEntitySwitch::kPhysicallyDependent);
    }
}

void SurfaceOfRevolution::SetAngleRange(const double start_angle, const double end_angle) {
    if (!(0.0 <= start_angle && start_angle < end_angle && end_angle <= 2.0 * kPi)) {
        throw std::invalid_argument("Invalid angles: Require 0 <= θstart < θend <= 2*pi");
    }
    start_angle_ = start_angle;
    end_angle_ = end_angle;
}

std::shared_ptr<const i_ent::Line> SurfaceOfRevolution::GetAxis() const {
    auto ptr = axis_.TryGetEntity<Line>();
    if (!ptr) {
        throw std::runtime_error("Axis (Line) pointer is not set or invalid.");
    }
    return ptr.value();
}

std::shared_ptr<const i_ent::ICurve> SurfaceOfRevolution::GetGeneratrix() const {
    auto ptr = generatrix_.TryGetEntity<ICurve>();
    if (!ptr) {
        throw std::runtime_error("Generatrix (ICurve) pointer is not set or invalid.");
    }
    return ptr.value();
}
