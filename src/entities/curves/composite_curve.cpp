/**
 * @file entities/curves/compsite_curve.cpp
 * @brief CompositeCurve (Type 102): 複合曲線エンティティの定義
 * @author Yayoi Habami
 * @date 2025-08-02
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/curves/composite_curve.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/numerics/core/tolerance.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using i_ent::CompositeCurve;
using i_ent::ICurve;
using igesio::Vector3d;

/// @brief 接合点の連続性判定に用いる許容差を計算する
/// @param a 接合点を構成する一方の端点
/// @param b 接合点を構成する他方の端点
/// @return 連続性判定の許容差
/// @note 絶対許容kGeometryToleranceはCADのモデリング公差由来の端点隙間に
///       対して厳しすぎるため、座標スケールに対する相対値とする
double ContinuityTolerance(const Vector3d& a, const Vector3d& b) {
    const double scale = std::max(a.norm(), b.norm());
    return std::max(i_num::kGeometryTolerance, 1e-5 * scale);
}

/// @brief 接合点の連続性を検証し、違反時は例外を投げる
/// @param prev_end 直前の曲線の終点
/// @param next_start 直後の曲線の始点
/// @param context エラーメッセージに含める呼び出し元の名前
/// @throw igesio::EntityValueError 接合点の隙間が許容差を超える場合
/// @note いずれかの端点が取得できない場合 (未解決参照・無限長等) は
///       チェック不能として許容する
void ThrowIfDiscontinuous(const std::optional<Vector3d>& prev_end,
                          const std::optional<Vector3d>& next_start,
                          const std::string& context) {
    if (!prev_end || !next_start) return;
    const double tol = ContinuityTolerance(*prev_end, *next_start);
    if (!i_num::IsApproxEqual(*prev_end, *next_start, tol)) {
        std::ostringstream oss;
        oss << context << ": curves must be contiguous, but the gap at the"
            << " junction (" << (*prev_end - *next_start).norm()
            << ") exceeds the tolerance (" << tol << ").";
        throw igesio::EntityValueError(oss.str());
    }
}

/// @brief コンテナに格納された曲線の終点を取得する
/// @param container 曲線を保持するコンテナ
/// @return 曲線の終点。曲線が未解決参照、または終点が取得できない場合はnullopt
std::optional<Vector3d> TryGetEndPointOf(
        const i_ent::PointerContainer<false, ICurve>& container) {
    auto curve = container.TryGetEntity<ICurve>();
    if (!curve || !*curve) return std::nullopt;
    return (*curve)->TryGetEndPoint();
}

/// @brief コンテナに格納された曲線の始点を取得する
/// @param container 曲線を保持するコンテナ
/// @return 曲線の始点。曲線が未解決参照、または始点が取得できない場合はnullopt
std::optional<Vector3d> TryGetStartPointOf(
        const i_ent::PointerContainer<false, ICurve>& container) {
    auto curve = container.TryGetEntity<ICurve>();
    if (!curve || !*curve) return std::nullopt;
    return (*curve)->TryGetStartPoint();
}

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
        throw igesio::EntityParameterError(
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
    Vector3d prev_end_point = Vector3d::Zero();
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
            const double continuity_tol =
                    ContinuityTolerance(prev_end_point, *start_point);
            // 連続性 (前曲線の終点=現曲線の始点) は幾何的品質の指摘でありkWarningとする。
            // 隙間があっても曲線は使用/描画可能なためis_valid (描画ゲート) はブロックしない。
            // 許容値は警告を出すか否かのみを左右する。CADは隙間を多用するため桁違いに大きい
            // 隙間 (真の不連続) のみ警告したい。
            if (!i_num::IsApproxEqual(prev_end_point, *start_point, continuity_tol)) {
                errors.emplace_back("End point of the curve at index " + std::to_string(i - 1)
                                    + " does not match start point of " + str_i + ".",
                                    igesio::ValidationSeverity::kWarning);
            }
            // 端点一致の可否に関わらず、現在の曲線の終点を次の始点基準として更新する
            // (接合点ごとに独立判定し、隙間が後続へ累積しないようにする)
            if (!end_point) {
                errors.emplace_back("End point of " + str_i + " is not defined.");
                continue;
            }
            prev_end_point = *end_point;
        }
    }

    return MakeValidationResult(std::move(errors));
}



/**
 * 直線部・角点サポート
 */

std::vector<std::array<double, 2>> CompositeCurve::GetLinearSegments() const {
    std::vector<std::array<double, 2>> result;
    const auto offsets = GetCurveBreakParameters();
    for (size_t i = 0; i < curves_.size(); ++i) {
        auto curve = curves_[i].GetEntity<ICurve>();
        if (!curve) continue;
        const double offset = offsets[i];
        const double local_start = curve->GetParameterRange()[0];
        for (const auto& seg : curve->GetLinearSegments()) {
            // 構成曲線が直線部を持てば、その区間をグローバルパラメータ空間に
            // マッピングして追加
            result.push_back({offset + (seg[0] - local_start),
                              offset + (seg[1] - local_start)});
        }
    }
    return result;
}

std::vector<double> CompositeCurve::GetCornerParams() const {
    std::vector<double> result;
    const auto offsets = GetCurveBreakParameters();
    for (size_t i = 0; i < curves_.size(); ++i) {
        auto curve = curves_[i].GetEntity<ICurve>();
        if (!curve) continue;
        const double offset = offsets[i];
        const double local_start = curve->GetParameterRange()[0];
        // 接合点を角点として追加 (最初の曲線は除く)
        if (i > 0) result.push_back(offset);
        // 構成曲線の角点をグローバルパラメータ空間にマッピング
        for (const double tc : curve->GetCornerParams()) {
            result.push_back(offset + (tc - local_start));
        }
    }
    return result;
}

std::optional<Vector3d> CompositeCurve::TryGetDefinedLeftTangentAt(const double t) const {
    const auto offsets = GetCurveBreakParameters();
    // 接合点かチェック: offsets[i] (i >= 1) に一致する場合
    for (size_t i = 1; i < curves_.size(); ++i) {
        if (std::abs(t - offsets[i]) < 1e-9) {
            // 直前の構成曲線 (i-1) の終端における左接線
            auto curve = curves_[i - 1].GetEntity<ICurve>();
            if (!curve) break;
            return curve->TryGetDefinedLeftTangentAt(curve->GetParameterRange()[1]);
        }
    }
    // 非接合点: 該当する構成曲線に委譲
    auto [curve, t_local] = GetCurveAtParameter(t);
    if (!curve) return std::nullopt;
    return curve->TryGetDefinedLeftTangentAt(t_local);
}

std::optional<Vector3d> CompositeCurve::TryGetDefinedRightTangentAt(const double t) const {
    const auto offsets = GetCurveBreakParameters();
    // 接合点かチェック: offsets[i] (i >= 1) に一致する場合
    for (size_t i = 1; i < curves_.size(); ++i) {
        if (std::abs(t - offsets[i]) < 1e-9) {
            // 直後の構成曲線 (i) の始端における右接線
            auto curve = curves_[i].GetEntity<ICurve>();
            if (!curve) break;
            return curve->TryGetDefinedRightTangentAt(curve->GetParameterRange()[0]);
        }
    }
    // 非接合点: 該当する構成曲線に委譲
    auto [curve, t_local] = GetCurveAtParameter(t);
    if (!curve) return std::nullopt;
    return curve->TryGetDefinedRightTangentAt(t_local);
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
    return i_num::IsApproxEqual(*start_point, *end_point,
                                i_num::kGeometryTolerance);
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

std::optional<i_ent::CurveDerivatives>
CompositeCurve::TryGetDefinedDerivatives(const double t, const unsigned int n) const {
    auto [curve, t_local] = GetCurveAtParameter(t);
    if (curve) {
        // 合成の定義空間は、構成曲線をそれぞれのモデル空間(M_sub適用済み)に
        // 配置したもの. よって構成曲線のモデル空間導関数を集約する
        return curve->TryGetDerivatives(t_local, n);
    }
    // パラメータtが範囲外の場合
    return std::nullopt;
}



/**
 * CompositeCurve: 構成要素 (曲線) の操作
 */

std::shared_ptr<const i_ent::ICurve>
CompositeCurve::GetCurveAt(const size_t index) const {
    if (index >= curves_.size()) {
        throw std::out_of_range("Index " + std::to_string(index) +
                                " is out of range for CompositeCurve.");
    }
    return curves_[index].GetEntity<ICurve>();
}

bool CompositeCurve::AddCurve(const std::shared_ptr<ICurve>& curve) {
    if (!curve) {
        return false;  // 無効なポインタは追加できない
    }

    // NOTE: プログラム構築経路では連続性 (直前の曲線の終点=新曲線の始点) を
    // 強制する。許容差は座標スケールに対する相対値 (ContinuityTolerance) であり、
    // CADのモデリング公差由来の微小隙間は許容し、桁違いの真の不連続のみ拒否する。
    // ファイル読み込み経路 (SetMainPDParameters) はチェックを行わず、連続性は
    // ValidatePDがkWarningとして報告する (描画はブロックしない)。
    if (!curves_.empty()) {
        ThrowIfDiscontinuous(TryGetEndPointOf(curves_.back()),
                             curve->TryGetStartPoint(), "AddCurve");
    }

    // SubordinateEntitySwitchをkPhysicallyDependentに設定
    if (auto entity_base = std::dynamic_pointer_cast<EntityBase>(curve)) {
        entity_base->SetSubordinateEntitySwitch(
            SubordinateEntitySwitch::kPhysicallyDependent);
    }

    curves_.emplace_back(curve);
    MarkGeometryModified();
    return true;
}

std::vector<std::shared_ptr<const i_ent::ICurve>>
CompositeCurve::ReplaceCurves(const size_t first, const size_t last,
                              const std::vector<std::shared_ptr<ICurve>>& new_curves) {
    // 引数検証 (検証がすべて通るまでcurves_は変更しない)
    if (first > last) {
        throw std::invalid_argument(
                "ReplaceCurves: first (" + std::to_string(first) +
                ") must not exceed last (" + std::to_string(last) + ").");
    }
    if (last >= curves_.size()) {
        throw std::out_of_range(
                "ReplaceCurves: last (" + std::to_string(last) +
                ") is out of range (curve count: " +
                std::to_string(curves_.size()) + ").");
    }
    for (size_t i = 0; i < new_curves.size(); ++i) {
        if (!new_curves[i]) {
            throw std::invalid_argument("ReplaceCurves: new_curves[" +
                                        std::to_string(i) + "] is null.");
        }
    }

    // 置換後の隣接接合 [前隣接, new_curves..., 後隣接] の連続性を検証
    // (first == 0の場合、先頭の新曲線に前隣接はなくprev_endはnulloptのまま)
    std::optional<Vector3d> prev_end;
    if (first > 0) prev_end = TryGetEndPointOf(curves_[first - 1]);
    for (const auto& curve : new_curves) {
        ThrowIfDiscontinuous(prev_end, curve->TryGetStartPoint(), "ReplaceCurves");
        prev_end = curve->TryGetEndPoint();
    }
    if (last + 1 < curves_.size()) {
        ThrowIfDiscontinuous(prev_end, TryGetStartPointOf(curves_[last + 1]),
                             "ReplaceCurves");
    }

    // 取り外す曲線を収集 (未解決参照はnullptr)
    std::vector<std::shared_ptr<const ICurve>> removed;
    removed.reserve(last - first + 1);
    for (size_t i = first; i <= last; ++i) {
        auto curve = curves_[i].TryGetEntity<ICurve>();
        removed.push_back((curve && *curve) ? *curve : nullptr);
    }

    // 置換後のリストを構築してから一括で差し替える (強い例外保証)
    std::vector<PointerContainer<false, ICurve>> replaced;
    replaced.reserve(curves_.size() - removed.size() + new_curves.size());
    for (size_t i = 0; i < first; ++i) replaced.push_back(curves_[i]);
    for (const auto& curve : new_curves) replaced.emplace_back(curve);
    for (size_t i = last + 1; i < curves_.size(); ++i) replaced.push_back(curves_[i]);
    curves_.swap(replaced);

    // 新規曲線のSubordinateEntitySwitchをkPhysicallyDependentに設定
    // (取り外した曲線のスイッチは、他エンティティからの参照有無を本クラスが
    //  知り得ないため変更しない)
    for (const auto& curve : new_curves) {
        if (auto entity_base = std::dynamic_pointer_cast<EntityBase>(curve)) {
            entity_base->SetSubordinateEntitySwitch(
                SubordinateEntitySwitch::kPhysicallyDependent);
        }
    }
    // SetCurveAt/RemoveFirstCurve/RemoveLastCurveも本メソッド経由でバンプされる
    MarkGeometryModified();
    return removed;
}

std::shared_ptr<const i_ent::ICurve>
CompositeCurve::SetCurveAt(const size_t index,
                           const std::shared_ptr<ICurve>& curve) {
    if (!curve) {
        throw std::invalid_argument("SetCurveAt: curve is null.");
    }
    return ReplaceCurves(index, index, {curve}).front();
}

std::shared_ptr<const i_ent::ICurve> CompositeCurve::RemoveFirstCurve() {
    if (curves_.empty()) {
        throw std::out_of_range("RemoveFirstCurve: the composite curve is empty.");
    }
    return ReplaceCurves(0, 0, {}).front();
}

std::shared_ptr<const i_ent::ICurve> CompositeCurve::RemoveLastCurve() {
    if (curves_.empty()) {
        throw std::out_of_range("RemoveLastCurve: the composite curve is empty.");
    }
    return ReplaceCurves(curves_.size() - 1, curves_.size() - 1, {}).front();
}

void CompositeCurve::ClearCurves() {
    curves_.clear();
    MarkGeometryModified();
}

double CompositeCurve::Length(const double start, const double end) const {
    // パラメータ開始/終了値に対応する曲線とローカルパラメータを取得
    auto start_result = TryGetCurveIndexAtParameter(start);
    auto end_result = TryGetCurveIndexAtParameter(end);
    if (!start_result || !end_result) {
        throw std::invalid_argument("Parameter out of range in Length calculation.");
    }

    // パラメータ範囲の妥当性を確認
    size_t start_index = start_result->first;
    double t_start_local = start_result->second;
    size_t end_index = end_result->first;
    double t_end_local = end_result->second;
    if (start_index > end_index ||
        (start_index == end_index && t_start_local >= t_end_local)) {
        throw std::invalid_argument("Invalid parameter range in Length calculation.");
    }

    double total_length = 0.0;
    for (size_t i = start_index; i <= end_index; ++i) {
        auto curve = curves_[i].GetEntity<ICurve>();

        double t_curve_start = 0.0;
        double t_curve_end = 0.0;
        if (i == start_index) {
            t_curve_start = t_start_local;
        } else {
            t_curve_start = curve->GetParameterRange()[0];
        }

        if (i == end_index) {
            t_curve_end = t_end_local;
        } else {
            t_curve_end = curve->GetParameterRange()[1];
        }

        total_length += curve->Length(t_curve_start, t_curve_end);
    }

    return total_length;
}

i_num::BoundingBox CompositeCurve::GetDefinedBoundingBox() const {
    auto bbox = i_num::BoundingBox();

    for (const auto& curve_container : curves_) {
        if (auto curve = curve_container.GetEntity<ICurve>()) {
            auto curve_bbox = curve->GetDefinedBoundingBox();
            bbox.ExpandToInclude(curve_bbox);
        }
    }
    return bbox;
}



/**
 * CompositeCurve: パラメータ写像・接合診断
 */

std::vector<double> CompositeCurve::GetCurveBreakParameters() const {
    std::vector<double> breaks;
    breaks.reserve(curves_.size() + 1);
    double acc = 0.0;
    breaks.push_back(0.0);
    for (const auto& container : curves_) {
        auto curve = container.TryGetEntity<ICurve>();
        if (!curve || !*curve) {
            // 未解決参照の曲線はパラメータ長0として扱う
            breaks.push_back(acc);
            continue;
        }
        const auto range = (*curve)->GetParameterRange();
        if (!std::isfinite(range[0]) || !std::isfinite(range[1])) {
            // 無限長の曲線はパラメータ長0として扱う
            breaks.push_back(acc);
            continue;
        }
        acc += range[1] - range[0];
        breaks.push_back(acc);
    }
    return breaks;
}

std::optional<double>
CompositeCurve::TryGetGlobalParameter(const size_t index,
                                      const double t_local) const {
    if (index >= curves_.size()) {
        throw std::out_of_range("TryGetGlobalParameter: index " +
                                std::to_string(index) +
                                " is out of range (curve count: " +
                                std::to_string(curves_.size()) + ").");
    }
    auto curve = curves_[index].TryGetEntity<ICurve>();
    if (!curve || !*curve) return std::nullopt;
    const auto range = (*curve)->GetParameterRange();
    if (!std::isfinite(range[0]) || !std::isfinite(range[1])) return std::nullopt;
    // t_localが当該曲線のパラメータ範囲内であることを確認
    // (浮動小数点数の比較のため、わずかな誤差を許容する)
    if (t_local < range[0] - i_num::kGeometryTolerance ||
        t_local > range[1] + i_num::kGeometryTolerance) {
        return std::nullopt;
    }
    return GetCurveBreakParameters()[index] + (t_local - range[0]);
}

std::vector<std::optional<double>> CompositeCurve::GetJunctionGaps() const {
    std::vector<std::optional<double>> gaps;
    if (curves_.size() < 2) return gaps;
    gaps.reserve(curves_.size() - 1);
    for (size_t i = 0; i + 1 < curves_.size(); ++i) {
        const auto end_point = TryGetEndPointOf(curves_[i]);
        const auto start_point = TryGetStartPointOf(curves_[i + 1]);
        if (end_point && start_point) {
            gaps.push_back((*end_point - *start_point).norm());
        } else {
            // 端点が取得できない接合点は隙間を定義できない
            gaps.push_back(std::nullopt);
        }
    }
    return gaps;
}

std::optional<std::pair<size_t, double>>
CompositeCurve::TryGetCurveIndexAtParameter(const double t) const {
    double accumulated_length = 0.0;
    for (size_t i = 0; i < curves_.size(); ++i) {
        auto curve_container = curves_[i];
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
                t <= accumulated_length + current_length + i_num::kGeometryTolerance) {
                // ローカルパラメータを計算
                // t_local = t_start + (t_global - accumulated_length)
                const double t_local = range[0] + (t - accumulated_length);
                return std::make_pair(i, t_local);
            }
            accumulated_length += current_length;
        }
    }
    // パラメータtが範囲外の場合
    return std::nullopt;
}



/**
 * CompositeCurve: private methods
 */

std::pair<std::shared_ptr<const i_ent::ICurve>, double>
CompositeCurve::GetCurveAtParameter(const double t) const {
    auto result = TryGetCurveIndexAtParameter(t);
    if (result) {
        size_t curve_index = result->first;
        double t_local = result->second;
        auto curve = curves_[curve_index].GetEntity<ICurve>();
        return std::make_pair(curve, t_local);
    }
    // パラメータtが範囲外の場合
    return {nullptr, 0.0};
}



/**
 * ファクトリ関数
 */

std::shared_ptr<CompositeCurve>
i_ent::MakeCompositeCurve(const std::vector<std::shared_ptr<ICurve>>& curves) {
    if (curves.empty()) {
        throw std::invalid_argument("MakeCompositeCurve: curves is empty.");
    }
    for (size_t i = 0; i < curves.size(); ++i) {
        if (!curves[i]) {
            throw std::invalid_argument("MakeCompositeCurve: curves[" +
                                        std::to_string(i) + "] is null.");
        }
    }
    // 連続性を事前検証する (AddCurve途中での失敗により、一部の入力曲線にのみ
    // SubordinateEntitySwitch設定の副作用が残ることを防ぐ)
    for (size_t i = 0; i + 1 < curves.size(); ++i) {
        ThrowIfDiscontinuous(curves[i]->TryGetEndPoint(),
                             curves[i + 1]->TryGetStartPoint(),
                             "MakeCompositeCurve");
    }

    auto composite = std::make_shared<CompositeCurve>();
    for (const auto& curve : curves) {
        composite->AddCurve(curve);
    }
    return composite;
}
