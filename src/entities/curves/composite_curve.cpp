/**
 * @file entities/curves/compsite_curve.cpp
 * @brief CompositeCurve (Type 102): 複合曲線エンティティの定義
 * @author Yayoi Habami
 * @date 2025-08-02
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/curves/composite_curve.h"

#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "igesio/common/tolerance.h"

namespace {

namespace i_ent = igesio::entities;
using i_ent::CompositeCurve;
using igesio::Vector3d;

}  // namespace



/**
 * コンストラクタ
 */

CompositeCurve::CompositeCurve()
    : EntityBase(EntityType::kCompositeCurve, {0}) {
    InitializePD({});
}

CompositeCurve::CompositeCurve(const RawEntityDE& de_record,
                               const IGESParameterVector& parameters,
                               const pointer2ID& de2id,
                               const ObjectID& iges_id)
    : EntityBase(de_record, parameters, de2id, iges_id) {
    InitializePD(de2id);
}



/**
 * EntityBase implementation
 */

igesio::IGESParameterVector
CompositeCurve::GetMainPDParameters() const {
    // データをIGESParameterVectorに変換
    IGESParameterVector params;
    params.reserve(curves_.size() + 1);
    // 曲線の数
    params.push_back(static_cast<int>(curves_.size()));

    // 各曲線のIDを追加
    for (const auto& curve : curves_) {
        params.push_back(curve.GetID());
    }

    // pd_parameters_のフォーマットを適用
    // 要素数が同じ場合のみ適用する
    if (params.size() == pd_parameters_.size()) {
        for (size_t i = 0; i < pd_parameters_.size(); ++i) {
            try {
                params.set_format(i, pd_parameters_.get_format(i));
            } catch (const std::invalid_argument&) {
                // 変換元のフォーマットが正しくない場合は更新しない
            }
        }
    }
    return params;
}

size_t CompositeCurve::SetMainPDParameters(const pointer2ID& de2id) {
    // 参照する曲線の数を取得
    auto& pd = pd_parameters_;
    auto num_curves = static_cast<size_t>(pd.access_as<int>(0));
    if (num_curves <= 0) return 1;  // 曲線がない場合は1を返す
    if (pd_parameters_.size() <= num_curves) {
        // 参照するといわれた曲線の数がパラメータの数より多い場合はエラー
        throw igesio::DataFormatError(
            "CompositeCurve requires " + std::to_string(num_curves) +
            " curves, but only " + std::to_string(pd_parameters_.size() - 1) +
            " parameters provided.");
    }

    // 曲線のポインタを設定
    curves_.clear();
    curves_.reserve(num_curves);
    for (size_t i = 0; i < num_curves; ++i) {
        if (!de2id.empty()) {
            // de2idが空でない場合は、取得した数値をde2idでIDに変換
            auto de = static_cast<unsigned int>(pd.access_as<int>(i + 1));
            if (de2id.find(de) == de2id.end()) {
                // de2idが空でなく、かつ指定されたDEが存在しない場合はエラー
                throw std::out_of_range("Curve ID " + std::to_string(de) +
                                        " not found in DE to ID mapping.");
            }
            curves_.emplace_back(de2id.at(de));
        } else {
            // de2idが空の場合はIDを直接取得
            curves_.emplace_back(pd.access_as<ObjectID>(i + 1));
        }
    }

    return num_curves + 1;  // 成功した場合は曲線の数 + 1 を返す
}

std::unordered_set<igesio::ObjectID>
CompositeCurve::GetUnresolvedPDReferences() const {
    std::unordered_set<ObjectID> referenced_ids;
    // 曲線のIDを追加
    for (const auto& curve : curves_) {
        if (!curve.IsPointerSet()) {
            referenced_ids.insert(curve.GetID());
        }
    }
    return referenced_ids;
}

bool CompositeCurve::SetUnresolvedPDReferences(
        const std::shared_ptr<const EntityBase>& entity) {
    // 指定されたエンティティがnullptrの場合は失敗
    if (!entity) {
        return false;
    }

    // 曲線のポインタを設定
    for (auto& curve : curves_) {
        if (curve.GetID() == entity->GetID()) {
            if (curve.IsPointerSet()) continue;
            // ポインタが未設定の場合のみ設定
            if (auto ptr = std::dynamic_pointer_cast<const ICurve>(entity)) {
                return curve.SetPointer(ptr);
            }
        }
    }

    // どの曲線も指定されたエンティティと一致しなかった場合は失敗
    return false;
}

std::vector<igesio::ObjectID> CompositeCurve::GetChildIDs() const {
    std::vector<ObjectID> ids;
    ids.reserve(curves_.size());
    for (const auto& curve : curves_) {
        ids.push_back(curve.GetID());
    }
    return ids;
}

std::shared_ptr<const i_ent::EntityBase>
CompositeCurve::GetChildEntity(const ObjectID& id) const {
    for (const auto& curve : curves_) {
        if (curve.GetID() == id) {
            auto ptr = curve.TryGetEntity<EntityBase>();
            if (ptr) return ptr.value();
        }
    }
    // 指定されたIDのエンティティが見つからない場合はnullptrを返す
    return nullptr;
}

igesio::ValidationResult CompositeCurve::ValidatePD() const {
    std::vector<ValidationError> errors;

    // 曲線の数が0以下の場合はエラー
    if (curves_.empty()) {
        errors.emplace_back("CompositeCurve must contain at least one curve.");
    }

    // 各曲線の有効性を確認
    auto prev_end_point = Vector3d::Zero();
    for (size_t i = 0; i < curves_.size(); ++i) {
        auto str_i = "the curve at index " + std::to_string(i);
        auto curve_opt = curves_[i].TryGetEntity<EntityBase>();
        if (!curve_opt || !curve_opt.value()) {
            // 曲線のポインタが無効な場合はエラー
            errors.emplace_back("Invalid pointer for " + str_i + ".");
            continue;
        }
        // 曲線の有効性を確認
        auto curve_validation = curve_opt.value()->Validate();
        if (!curve_validation.is_valid) {
            // 曲線の検証結果が無効な場合はエラーを追加
            errors.emplace_back(curve_validation.Message() + " in " + str_i + ".");
            continue;
        }

        // 曲線の端点を取得
        auto start_point = curves_[i].GetEntity<ICurve>()->TryGetStartPoint();
        auto end_point = curves_[i].GetEntity<ICurve>()->TryGetEndPoint();
        if (i == 0) {
            if (!end_point) {
                errors.emplace_back("End point of the first curve is not defined.");
                continue;
            }
            prev_end_point = *end_point;
        } else if (i < curves_.size() - 1) {
            if (!start_point) {
                errors.emplace_back("Start point of " + str_i + " is not defined.");
                continue;
            }
            // 前の曲線の終点と現在の曲線の始点が一致するか確認
            if (!IsApproxEqual(prev_end_point, *start_point, kGeometryTolerance)) {
                errors.emplace_back("End point of the curve at index " + std::to_string(i - 1)
                                    + " does not match start point of " + str_i + ".");
                continue;
            }
            // 現在の曲線の終点を次の曲線の始点として設定
            if (i != curves_.size() - 1) {
                if (!end_point) {
                    errors.emplace_back("End point of " + str_i + " is not defined.");
                    continue;
                }
                prev_end_point = *end_point;
            }
        }
    }

    return MakeValidationResult(std::move(errors));
}



/**
 * ICurve implementation
 */

bool CompositeCurve::IsClosed() const {
    // 構成曲線から最初と最後のICurveを取得する
    const auto first_curve =
            curves_.empty() ? nullptr : curves_.front().GetEntity<ICurve>();
    const auto last_curve =
            curves_.empty() ? nullptr : curves_.back().GetEntity<ICurve>();

    // 曲線エンティティが存在しない場合は閉じないと判断
    if (!first_curve || !last_curve) return false;

    // 始点と終点を取得
    auto start_point = first_curve->TryGetStartPoint();
    auto end_point = last_curve->TryGetEndPoint();

    // 始点または終点が取得できない場合は閉じないと判断
    if (!start_point || !end_point) return false;

    // 始点と終点の座標を比較
    return IsApproxEqual(*start_point, *end_point, kGeometryTolerance);
}

std::array<double, 2> CompositeCurve::GetParameterRange() const {
    double total_parametric_length = 0.0;
    for (const auto& curve_container : curves_) {
        if (auto curve = curve_container.GetEntity<ICurve>()) {
            const auto range = curve->GetParameterRange();
            // 無限の長さを持つ曲線が含まれている場合は、結果も無限とする
            if (!std::isfinite(range[0]) || !std::isfinite(range[1])) {
                return {0.0, std::numeric_limits<double>::infinity()};
            }
            total_parametric_length += (range[1] - range[0]);
        }
    }
    return {0.0, total_parametric_length};
}

std::optional<Vector3d>
CompositeCurve::TryGetDefinedPointAt(const double t) const {
    auto [curve, t_local] = GetCurveAtParameter(t);
    if (curve) {
        // 個別の要素の定義空間の座標ではなく、本エンティティの定義空間における
        // 曲線を取得するため、`TryGetPointAt`を使用する
        return curve->TryGetPointAt(t_local);
    }
    // パラメータtが範囲外の場合
    return std::nullopt;
}

std::optional<Vector3d>
CompositeCurve::TryGetDefinedTangentAt(const double t) const {
    auto [curve, t_local] = GetCurveAtParameter(t);
    if (curve) {
        // 個別の要素の定義空間の接線ベクトルではなく、本エンティティの定義空間における
        // 曲線接線を取得するため、`TryGetTangentAt`を使用する
        return curve->TryGetTangentAt(t_local);
    }
    // パラメータtが範囲外の場合
    return std::nullopt;
}

std::optional<Vector3d>
CompositeCurve::TryGetDefinedNormalAt(const double t) const {
    auto [curve, t_local] = GetCurveAtParameter(t);
    if (curve) {
        // 個別の要素の定義空間の法線ベクトルではなく、本エンティティの定義空間における
        // 曲線法線を取得するため、`TryGetNormalAt`を使用
        return curve->TryGetNormalAt(t_local);
    }
    // パラメータtが範囲外の場合
    return std::nullopt;
}

std::optional<Vector3d> CompositeCurve::TryGetPointAt(const double t) const {
    return TransformPoint(TryGetDefinedPointAt(t));
}

std::optional<Vector3d> CompositeCurve::TryGetTangentAt(const double t) const {
    return TransformVector(TryGetDefinedTangentAt(t));
}

std::optional<Vector3d> CompositeCurve::TryGetNormalAt(const double t) const {
    return TransformVector(TryGetDefinedNormalAt(t));
}



/**
 * CompositeCurve: 構成要素 (曲線) の操作
 */

bool CompositeCurve::AddCurve(const std::shared_ptr<ICurve>& curve) {
    if (!curve) {
        return false;  // 無効なポインタは追加できない
    }

    // 終点が前の曲線の始点と一致するか確認
    if (!curves_.empty()) {
        auto last_curve = curves_.back().GetEntity<ICurve>();
        if (!last_curve) {
            return false;  // 最後の曲線が無効な場合は追加できない
        }
        auto last_end_point = last_curve->TryGetEndPoint();
        auto new_start_point = curve->TryGetStartPoint();
        if (!last_end_point || !new_start_point ||
            !IsApproxEqual(*last_end_point, *new_start_point, kGeometryTolerance)) {
            return false;  // 終点と始点が一致しない場合は追加できない
        }
    }

    // SubordinateEntitySwitchをkPhysicallyDependentに設定
    if (auto entity_base = std::dynamic_pointer_cast<EntityBase>(curve)) {
        entity_base->SetSubordinateEntitySwitch(
            SubordinateEntitySwitch::kPhysicallyDependent);
    }

    curves_.emplace_back(curve);
    return true;
}



/**
 * CompositeCurve: private methods
 */

std::pair<std::shared_ptr<const i_ent::ICurve>, double>
CompositeCurve::GetCurveAtParameter(const double t) const {
    double accumulated_length = 0.0;
    for (const auto& curve_container : curves_) {
        if (auto curve = curve_container.GetEntity<ICurve>()) {
            const auto range = curve->GetParameterRange();
            // 無限の長さを持つ曲線はパラメータ範囲の計算から除外
            if (!std::isfinite(range[0]) || !std::isfinite(range[1])) {
                continue;
            }
            const double current_length = range[1] - range[0];

            // パラメータtが現在の曲線の範囲内にあるかチェック
            // 浮動小数点数の比較のため、わずかな誤差を許容する
            if (t >= accumulated_length &&
                t <= accumulated_length + current_length + kGeometryTolerance) {
                // ローカルパラメータを計算
                // t_local = t_start + (t_global - accumulated_length)
                const double t_local = range[0] + (t - accumulated_length);
                return {curve, t_local};
            }
            accumulated_length += current_length;
        }
    }
    // パラメータtが範囲外の場合
    return {nullptr, 0.0};
}
