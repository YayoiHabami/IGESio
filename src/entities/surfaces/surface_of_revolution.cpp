/**
 * @file entities/surfaces/surface_of_revolution.cpp
 * @brief Surface of Revolution (Type 120): 回転曲面エンティティの定義
 * @author Yayoi Habami
 * @date 2025-10-12
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/surfaces/surface_of_revolution.h"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "igesio/numerics/core/tolerance.h"
#include "igesio/entities/curves/algorithms.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using i_ent::SurfaceOfRevolution;
using igesio::Vector3d;

/// @brief 回転角度の制約 0 < TA-SA <= 2π を満たすか
/// @param start_angle 回転の開始角度SA [rad]
/// @param end_angle 回転の終了角度TA [rad]
/// @note 規格はSA・TAの絶対値ではなく差のみを制約する。上限は厳密比較では
///       なく許容誤差付き比較とする。全周回転(360°)ではCADが終了角を2πの
///       十進近似で出力し、double化で差が真の2πをわずかに超える
///       (例: 6.28318530717959 ≈ 2π+4e-15)ことがあるため(値はクランプせず
///       原値を保持)。
bool IsValidAngleRange(const double start_angle, const double end_angle) {
    return start_angle < end_angle
           && i_num::IsApproxLEQ(end_angle - start_angle, 2.0 * igesio::kPi);
}

/// @brief 回転角度の制約 0 < TA-SA <= 2π を検証する
/// @param start_angle 回転の開始角度SA [rad]
/// @param end_angle 回転の終了角度TA [rad]
/// @throw igesio::EntityValueError 制約を満たさない場合
void ValidateAngleRange(const double start_angle, const double end_angle) {
    if (!IsValidAngleRange(start_angle, end_angle)) {
        throw igesio::EntityValueError(
            "Invalid angles: Require 0 < θend - θstart <= 2*pi, but got "
            "θstart = " + std::to_string(start_angle) + "[rad] and "
            "θend = " + std::to_string(end_angle) + "[rad].");
    }
}

/// @brief 値コンストラクタ用のPDパラメータを構築する
/// @param axis 回転軸 (Lineエンティティへのポインタ)
/// @param generatrix 母線 (ICurveを継承したエンティティへのポインタ)
/// @return 構築されたIGESParameterVector
/// @throw std::invalid_argument axisまたはgeneratrixがnullptrの場合
/// @note 委譲コンストラクタの初期化式でnullptrを参照しないよう、
///       GetID()の呼び出し前にここでnull検証を行う
igesio::IGESParameterVector BuildSorParameters(
        const std::shared_ptr<i_ent::Line>& axis,
        const std::shared_ptr<i_ent::ICurve>& generatrix,
        const double start_angle, const double end_angle) {
    if (!axis) {
        throw std::invalid_argument(
            "axis of SurfaceOfRevolution must not be nullptr");
    }
    if (!generatrix) {
        throw std::invalid_argument(
            "generatrix of SurfaceOfRevolution must not be nullptr");
    }
    return {axis->GetID(), generatrix->GetID(), start_angle, end_angle};
}

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
        : SurfaceOfRevolution(BuildSorParameters(
              axis, generatrix, start_angle, end_angle)) {
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
        throw igesio::EntityParameterError(
            "SurfaceOfRevolution must have at least 4 parameters");
    }

    // 回転軸
    try {
        axis_ = PointerContainer<false, Line>(
            GetObjectIDFromParameters(pd, 0, de2id));
    } catch (const std::exception& e) {
        throw igesio::ReferenceError("Failed to get Axis (Line) ID "
            "from parameters: " + std::string(e.what()));
    }

    // 母線
    try {
        generatrix_ = PointerContainer<false, ICurve>(
            GetObjectIDFromParameters(pd, 1, de2id));
    } catch (const std::exception& e) {
        throw igesio::ReferenceError("Failed to get Generatrix (ICurve) ID "
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

    // 回転角度: 0 < θend - θstart <= 2*pi (許容誤差等はIsValidAngleRange参照)
    if (!IsValidAngleRange(start_angle_, end_angle_)) {
        // 角度は幾何的品質の指摘 (kWarning) とし描画はブロックしない
        // (過回転でも周期的に評価でき、退化(start>=end)は何も描かないだけ)。
        errors.emplace_back("Invalid angles: Require 0 < θend - θstart <= 2*pi, "
                            "but got θstart = " + std::to_string(start_angle_) + "[rad] and "
                            "θend = " + std::to_string(end_angle_) + "[rad].",
                            igesio::ValidationSeverity::kWarning);
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
    // v方向は回転角度がちょうど全周 (θend - θstart = 2π) の場合に閉じる
    return i_num::IsApproxEqual(end_angle_ - start_angle_, 2.0 * kPi);
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
SurfaceOfRevolution::TryGetDefinedDerivatives(
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

i_num::BoundingBox SurfaceOfRevolution::GetDefinedBoundingBox() const {
    // ポインタの確認
    if (!axis_.IsPointerSet() || !generatrix_.IsPointerSet()) {
        throw igesio::ReferenceError(
            "Cannot compute bounding box: Axis or Generatrix pointer is not set.");
    }

    // 回転軸の始点P0と方向ベクトルDを取得
    const auto& [P0, end_point] = GetAxis()->GetAnchorPoints();
    auto D = (end_point - P0).normalized();

    // バウンディングボックスの座標系を定義 (Dが第3軸)
    std::array<Vector3d, 3> dirs;
    dirs[2] = D;
    if (std::abs(D.z()) > 0.9) {
        dirs[1] = D.cross(Vector3d::UnitX()).normalized();
    } else {
        dirs[1] = D.cross(Vector3d::UnitZ()).normalized();
    }
    dirs[0] = dirs[1].cross(dirs[2]);

    // 母線のバウンディングボックスの頂点を取得
    auto curve_bbox = GetGeneratrix()->GetDefinedBoundingBox();
    auto vertices = curve_bbox.GetVertices();
    if (vertices.empty()) return i_num::BoundingBox();

    // 新しい座標系での最小/最大座標を計算
    Vector3d min_local = Vector3d::Constant(std::numeric_limits<double>::infinity());
    Vector3d max_local = Vector3d::Constant(-std::numeric_limits<double>::infinity());

    // 回転範囲を取得
    auto range = GetAngleRange();
    bool is_full_rotation =
            i_num::IsApproxEqual(std::fmod(range[1] - range[0] + 4*kPi, 2*kPi), 0.0);

    for (const auto& p : vertices) {
        const Vector3d p_P0 = p - P0;
        Vector3d local = {p_P0.dot(dirs[0]), p_P0.dot(dirs[1]), p_P0.dot(dirs[2])};

        // 頂点の軸からの距離を計算
        auto r = std::sqrt(local.x() * local.x() + local.y() * local.y());

        if (is_full_rotation) {
            // 360°回転した場合の最大/最小座標を計算
            min_local = min_local.cwiseMin(Vector3d{-r, -r, local.z()});
            max_local = max_local.cwiseMax(Vector3d{ r,  r, local.z()});
        } else {
            // 回転範囲内の主要角度での最大/最小座標を計算
            auto current_angle = std::atan2(local.y(), local.x());
            std::vector<double> angles_to_check = {
                    range[0], range[1],                             // 始点, 終点
                    0 - current_angle, kPi/2 - current_angle,       // 0°, 90°
                    kPi - current_angle, 3*kPi/2 - current_angle};  // 180°, 270°
            for (const auto& angle : angles_to_check) {
                // 回転角度が範囲内にある場合のみ計算
                if (!(i_num::IsApproxLEQ(range[0], angle) &&
                      i_num::IsApproxLEQ(angle, range[1]))) {
                    continue;
                }

                auto angle_c = current_angle + angle;
                Vector3d rotated = {r * std::cos(angle_c), r * std::sin(angle_c), local.z()};
                min_local = min_local.cwiseMin(rotated);
                max_local = max_local.cwiseMax(rotated);
            }
        }
    }

    // BoundingBoxの基点、方向、サイズを計算
    Matrix3d transform;
    transform.block<3, 1>(0, 0) = dirs[0];
    transform.block<3, 1>(0, 1) = dirs[1];
    transform.block<3, 1>(0, 2) = dirs[2];

    auto P0_bbox = P0 + transform * min_local;
    auto sizes = (max_local - min_local);
    std::array<double, 3> bbox_sizes = {sizes.x(), sizes.y(), sizes.z()};

    return i_num::BoundingBox(P0_bbox, dirs, bbox_sizes, {false, false, false});
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
    MarkGeometryModified();
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
    MarkGeometryModified();
}

void SurfaceOfRevolution::SetAngleRange(const double start_angle, const double end_angle) {
    ValidateAngleRange(start_angle, end_angle);
    start_angle_ = start_angle;
    end_angle_ = end_angle;
    MarkGeometryModified();
}

std::shared_ptr<const i_ent::Line> SurfaceOfRevolution::GetAxis() const {
    auto ptr = axis_.TryGetEntity<Line>();
    if (!ptr) {
        throw igesio::ReferenceError("Axis (Line) pointer is not set or invalid.");
    }
    return ptr.value();
}

std::shared_ptr<const i_ent::ICurve> SurfaceOfRevolution::GetGeneratrix() const {
    auto ptr = generatrix_.TryGetEntity<ICurve>();
    if (!ptr) {
        throw igesio::ReferenceError("Generatrix (ICurve) pointer is not set or invalid.");
    }
    return ptr.value();
}



/**
 * ファクトリ関数
 */

std::shared_ptr<SurfaceOfRevolution> i_ent::MakeSurfaceOfRevolution(
        const std::shared_ptr<Line>& axis,
        const std::shared_ptr<ICurve>& generatrix,
        const double start_angle, const double end_angle) {
    // 角度は値コンストラクタでは検証されないため、構築前に検証する
    ValidateAngleRange(start_angle, end_angle);
    return std::make_shared<SurfaceOfRevolution>(
        axis, generatrix, start_angle, end_angle);
}

std::pair<std::shared_ptr<SurfaceOfRevolution>, std::shared_ptr<i_ent::Line>>
i_ent::MakeSurfaceOfRevolution(
        const Vector3d& axis_point, const Vector3d& axis_direction,
        const std::shared_ptr<ICurve>& generatrix,
        const double start_angle, const double end_angle) {
    ValidateAngleRange(start_angle, end_angle);

    // 軸の方向が定まらないため、ゼロベクトルは不可
    if (i_num::IsApproxZero(axis_direction.norm(), i_num::kGeometryTolerance)) {
        throw igesio::EntityValueError(
            "MakeSurfaceOfRevolution: Axis direction must not be a zero vector.");
    }
    // 孤立した軸Lineが生成されるのを防ぐため、軸の生成前に検証する
    if (!generatrix) {
        throw std::invalid_argument(
            "generatrix of SurfaceOfRevolution must not be nullptr");
    }

    auto axis = MakeLine(axis_point, axis_point + axis_direction);
    auto surface = std::make_shared<SurfaceOfRevolution>(
        axis, generatrix, start_angle, end_angle);
    return {surface, axis};
}
