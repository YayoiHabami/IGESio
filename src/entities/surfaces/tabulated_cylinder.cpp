/**
 * @file entities/surfaces/tabulated_cylinder.cpp
 * @brief Tabulated Cylinder (Type 122): 平行曲面エンティティの定義
 * @author Yayoi Habami
 * @date 2025-10-13
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/surfaces/tabulated_cylinder.h"

#include <limits>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "igesio/common/tolerance.h"

namespace {

namespace i_ent = igesio::entities;
using i_ent::TabulatedCylinder;
using igesio::Vector3d;

}  // namespace



/**
 * コンストラクタ
 */

TabulatedCylinder::TabulatedCylinder(
        const RawEntityDE& de_record, const IGESParameterVector& parameters,
        const pointer2ID& de2id, const ObjectID& iges_id)
        : EntityBase(de_record, parameters, de2id, iges_id),
          directrix_(IDGenerator::UnsetID()),
          location_vector_(Vector3d::Zero()) {
    InitializePD(de2id);
}

TabulatedCylinder::TabulatedCylinder(
        const IGESParameterVector& parameters)
        : TabulatedCylinder(
            RawEntityDE::ByDefault(i_ent::EntityType::kTabulatedCylinder),
            parameters, {}, IDGenerator::UnsetID()) {}

TabulatedCylinder::TabulatedCylinder(const std::shared_ptr<ICurve>& directrix,
                                     const Vector3d& location_vector)
        : TabulatedCylinder({directrix->GetID(), location_vector(0),
                             location_vector(1), location_vector(2)}) {
    if (directrix == nullptr) {
        throw std::invalid_argument(
            "Directrix curve of TabulatedCylinder cannot be nullptr.");
    }
    // 曲線の始点とlocation_vectorが一致する場合はエラー
    auto start_pt_opt = directrix->TryGetDefinedStartPoint();
    if (start_pt_opt.has_value()) {
        if (IsApproxEqual(start_pt_opt.value(), location_vector)) {
            throw std::invalid_argument("Directrix curve's start point and "
                "location vector of TabulatedCylinder cannot be the same.");
        }
    }

    // 参照の解決
    SetDirectrix(directrix);
}

TabulatedCylinder::TabulatedCylinder(const std::shared_ptr<ICurve>& directrix,
                                     const Vector3d& direction,
                                     const double length)
        : TabulatedCylinder({directrix->GetID(), 0.0, 0.0, 0.0}) {
    if (directrix == nullptr) {
        throw std::invalid_argument(
            "Directrix curve of TabulatedCylinder cannot be nullptr.");
    }
    auto start_pt_opt = directrix->TryGetDefinedStartPoint();
    if (!start_pt_opt.has_value()) {
        throw std::invalid_argument(
            "Directrix curve of TabulatedCylinder must have a valid start point.");
    }
    if (IsApproxZero((length * direction).norm())) {
        throw std::invalid_argument(
            "Direction vector of TabulatedCylinder cannot be zero vector.");
    }

    // 位置ベクトルを計算
    location_vector_ = start_pt_opt.value() + direction.normalized() * length;
    // 参照の解決
    SetDirectrix(directrix);
}




/**
 * EntityBase implementation
 */

igesio::IGESParameterVector
TabulatedCylinder::GetMainPDParameters() const {
    return {directrix_.GetID(),
            location_vector_(0), location_vector_(1), location_vector_(2)};
}

size_t TabulatedCylinder::SetMainPDParameters(const pointer2ID& de2id) {
    auto& pd = pd_parameters_;

    if (pd.size() < 4) {
        throw igesio::DataFormatError(
            "TabulatedCylinder must have at least 4 parameters");
    }

    // 曲線
    try {
        directrix_ = PointerContainer<false, ICurve>(
                GetObjectIDFromParameters(pd, 0, de2id));
    } catch (const std::exception& e) {
        throw igesio::DataFormatError("Failed to set Directrix (Curve) reference"
            " in TabulatedCylinder: " + std::string(e.what()));
    }

    // 位置ベクトル
    location_vector_(0) = pd.access_as<double>(1);
    location_vector_(1) = pd.access_as<double>(2);
    location_vector_(2) = pd.access_as<double>(3);

    return 4;
}

std::unordered_set<igesio::ObjectID>
TabulatedCylinder::GetUnresolvedPDReferences() const {
    std::unordered_set<ObjectID> unresolved;

    if (!directrix_.IsPointerSet()) {
        unresolved.insert(directrix_.GetID());
    }
    return unresolved;
}

bool TabulatedCylinder::SetUnresolvedPDReferences(
        const std::shared_ptr<const EntityBase>& entity) {
    // 指定されたエンティティがnullptrの場合は失敗
    if (!entity) {
        return false;
    }

    if (entity->GetID() == directrix_.GetID()) {
        if (!directrix_.IsPointerSet()) {
            // ポインタが未設定の場合のみ設定
            if (auto ptr = std::dynamic_pointer_cast<const ICurve>(entity)) {
                return directrix_.SetPointer(ptr);
            }
        }
    }

    // 指定されたエンティティと同一のIDを持つ参照がない場合
    return false;
}

std::vector<igesio::ObjectID> TabulatedCylinder::GetChildIDs() const {
    std::vector<ObjectID> ids;
    ids.push_back(directrix_.GetID());
    return ids;
}

std::shared_ptr<const i_ent::EntityBase>
TabulatedCylinder::GetChildEntity(const ObjectID& id) const {
    if (directrix_.GetID() == id) {
        auto ptr = directrix_.TryGetEntity<EntityBase>();
        if (ptr) return ptr.value();
    }

    // 指定されたIDを持つ子エンティティが存在しない場合
    return nullptr;
}

igesio::ValidationResult TabulatedCylinder::ValidatePD() const {
    std::vector<ValidationError> errors;

    // 準線
    bool has_valid_start = false;
    Vector3d directrix_start = Vector3d::Zero();
    if (!directrix_.IsPointerSet()) {
        errors.emplace_back("Directrix (Curve) reference is not set.");
    } else {
        auto directrix_curve = directrix_.TryGetEntity<EntityBase>();
        if (!directrix_curve) {
            errors.emplace_back("Directrix (Curve) reference is not set.");
        } else {
            // 曲線の検証
            auto result = directrix_curve.value()->Validate();
            if (!result.is_valid) {
                errors.emplace_back("Directrix (Curve) is invalid: " + result.Message());
            } else {
                // 曲線の始点を取得
                auto start_pt_opt = GetDirectrix()->TryGetStartPoint();
                if (start_pt_opt.has_value()) {
                    has_valid_start = true;
                    directrix_start = start_pt_opt.value();
                }
            }
        }
    }

    // 位置ベクトル
    if (!has_valid_start) {
        errors.emplace_back(
            "Cannot validate location vector because directrix start point is invalid.");
    } else {
        // 曲線の始点とlocation_vectorが一致する場合はエラー
        if (IsApproxEqual(directrix_start, location_vector_)) {
            errors.emplace_back("Directrix curve's start point and "
                "location vector of TabulatedCylinder cannot be the same.");
        }
    }

    return MakeValidationResult(std::move(errors));
}



/**
 * ISurface implementation
 */

bool TabulatedCylinder::IsUClosed() const {
    // U方向: 準線が閉じているか
    auto directrix_curve = directrix_.TryGetEntity<ICurve>();
    if (!directrix_curve) return false;
    return directrix_curve.value()->IsClosed();
}

bool TabulatedCylinder::IsVClosed() const {
    // V方向: 母線は閉じていない
    return false;
}

std::array<double, 4> TabulatedCylinder::GetParameterRange() const {
    auto directrix_curve = directrix_.TryGetEntity<ICurve>();
    if (!directrix_curve) return {0.0, 1.0, 0.0, 1.0};

    // u = (t - t_start)/(t_end - t_start)
    // tの範囲が無限を含む場合には特殊処理
    auto [t_start, t_end] = directrix_curve.value()->GetParameterRange();
    double u_start = (t_start == -std::numeric_limits<double>::infinity()) ?
                     -std::numeric_limits<double>::infinity() : 0.0;
    double u_end = (t_end == std::numeric_limits<double>::infinity()) ?
                   std::numeric_limits<double>::infinity() : 1.0;
    return {u_start, u_end, 0.0, 1.0};
}

std::optional<Vector3d>
TabulatedCylinder::TryGetDefinedPointAt(const double u, const double v) const {
    // ポインタの確認
    if (!directrix_.IsPointerSet()) return std::nullopt;

    // パラメータ範囲の確認
    auto [umin, umax, vmin, vmax] = GetParameterRange();
    if (u < umin || u > umax || v < vmin || v > vmax) {
        return std::nullopt;
    }

    // 計算: S(u, v) = C(t) + v * direction
    double t = GetDirectrixParameterAtU(u);
    auto c_t = GetDirectrix()->TryGetPointAt(t);
    if (!c_t.has_value()) return std::nullopt;

    return c_t.value() + v * GetDirection();
}

std::optional<Vector3d>
TabulatedCylinder::TryGetDefinedNormalAt(const double u, const double v) const {
    // ポインタの確認
    if (!directrix_.IsPointerSet()) return std::nullopt;

    // パラメータ範囲の確認
    auto [umin, umax, vmin, vmax] = GetParameterRange();
    if (u < umin || u > umax || v < vmin || v > vmax) {
        return std::nullopt;
    }

    // 計算: N(u, v) = T_C(t) × direction; T_C(t): 準線の接線ベクトル
    double t = GetDirectrixParameterAtU(u);
    auto t_c = GetDirectrix()->TryGetTangentAt(t);
    if (!t_c.has_value()) return std::nullopt;

    auto normal = t_c.value().cross(GetDirection());
    if (IsApproxZero(normal.norm())) {
        // 法線ベクトルがゼロベクトルになる場合は、法線ベクトルを計算できない
        return std::nullopt;
    }
    return normal.normalized();
}

std::optional<igesio::Vector3d>
TabulatedCylinder::TryGetPointAt(const double u, const double v) const {
    return TransformImpl(TryGetDefinedPointAt(u, v), true);
}

std::optional<igesio::Vector3d>
TabulatedCylinder::TryGetNormalAt(const double u, const double v) const {
    return TransformImpl(TryGetDefinedNormalAt(u, v), false);
}



/**
 * 要素の変更・取得
 */

void TabulatedCylinder::SetDirectrix(const std::shared_ptr<ICurve>& directrix) {
    if (directrix == nullptr) {
        throw std::invalid_argument(
            "Directrix curve of TabulatedCylinder cannot be nullptr.");
    }
    if (directrix->GetID() == directrix_.GetID()) {
        directrix_.SetPointer(directrix);
    } else {
        directrix_.OverwritePointer(directrix);
    }

    // SubordinateEntitySwitchをkPhysicallyDependentに設定
    if (auto entity_base = std::dynamic_pointer_cast<EntityBase>(directrix)) {
        entity_base->SetSubordinateEntitySwitch(
            SubordinateEntitySwitch::kPhysicallyDependent);
    }
}

std::shared_ptr<const i_ent::ICurve> TabulatedCylinder::GetDirectrix() const {
    auto ptr = directrix_.TryGetEntity<ICurve>();
    if (!ptr) {
        throw std::runtime_error("Directrix (Curve) reference is not set.");
    }
    return ptr.value();
}

void TabulatedCylinder::SetLocationVector(const Vector3d& location_vector) {
    // 曲線の始点とlocation_vectorが一致する場合はエラー
    auto start_pt_opt = GetDirectrix()->TryGetDefinedStartPoint();
    if (start_pt_opt.has_value()) {
        if (IsApproxEqual(start_pt_opt.value(), location_vector)) {
            throw std::invalid_argument("Directrix curve's start point and "
                "location vector of TabulatedCylinder cannot be the same.");
        }
    }

    location_vector_ = location_vector;
}

Vector3d TabulatedCylinder::GetLocationVector() const {
    return location_vector_;
}

void TabulatedCylinder::SetDirection(const Vector3d& direction, const double length) {
    if (IsApproxZero((length * direction).norm())) {
        throw std::invalid_argument("Direction vector of TabulatedCylinder cannot be zero vector.");
    }

    // 曲線の始点とlocation_vectorが一致する場合はエラー
    auto start_pt_opt = GetDirectrix()->TryGetDefinedStartPoint();
    if (start_pt_opt.has_value()) {
        Vector3d new_location_vector = start_pt_opt.value() + direction * length;
        if (IsApproxEqual(start_pt_opt.value(), new_location_vector)) {
            throw std::invalid_argument("Directrix curve's start point and "
                "location vector of TabulatedCylinder cannot be the same.");
        }
        location_vector_ = new_location_vector;
    } else {
        // 曲線の始点が不明な場合は、位置ベクトルを更新しない
        throw std::runtime_error("Cannot set direction because directrix start point is invalid.");
    }
}

Vector3d TabulatedCylinder::GetDirection() const {
    auto start_pt_opt = GetDirectrix()->TryGetDefinedStartPoint();
    if (start_pt_opt.has_value()) {
        return location_vector_ - start_pt_opt.value();
    } else {
        throw std::runtime_error("Cannot get direction because directrix start point is invalid.");
    }
}



/**
 * private methods
 */

double TabulatedCylinder::GetDirectrixParameterAtU(const double u) const {
    auto directrix_curve = directrix_.TryGetEntity<ICurve>();
    if (!directrix_curve) return u;

    // u = (t - t_start)/(t_end - t_start)
    auto [t_start, t_end] = directrix_curve.value()->GetParameterRange();
    if (t_start == -std::numeric_limits<double>::infinity() &&
        t_end == std::numeric_limits<double>::infinity()) {
        return u;  // tの範囲が無限の場合
    } else if (t_start == -std::numeric_limits<double>::infinity()) {
        return u * t_end;  // tの下限が無限の場合
    } else if (t_end == std::numeric_limits<double>::infinity()) {
        return t_start + u;  // tの上限が無限の場合
    }

    return t_start + u * (t_end - t_start);  // tの範囲が有限の場合
}
