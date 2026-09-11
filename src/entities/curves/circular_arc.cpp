/**
 * @file entities/curves/circular_arc.cpp
 * @brief CircularArc (Type 100): 円弧エンティティの定義
 * @author Yayoi Habami
 * @date 2025-08-02
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/curves/circular_arc.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/common/iges_parameter_vector.h"
#include "igesio/numerics/core/tolerance.h"
#include "igesio/entities/transformations/transformation_matrix.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using CircularArc = i_ent::CircularArc;
using Vector3d = igesio::Vector3d;
constexpr double kPi = igesio::kPi;

/// @brief 円弧の始点角と角度範囲
struct ArcAngles {
    /// @brief 始点角θs [rad] ([0, 2π))
    double start = 0.0;
    /// @brief 角度範囲Δ [rad] (向きによらず正. 閉じた円では2π)
    double sweep = 0.0;
};

/// @brief 始点角と角度範囲を計算する
/// @param center 中心座標
/// @param start_point 始点座標
/// @param terminate_point 終点座標
/// @param is_clockwise 時計回りの弧か
/// @param is_closed 閉じた円か (始点と終点が一致)
/// @return 始点角と角度範囲. 角度範囲は進行方向に沿って始点から終点まで測った
///         中心角で、(0, 2π]の範囲
ArcAngles ComputeArcAngles(const Vector3d& center, const Vector3d& start_point,
                           const Vector3d& terminate_point,
                           const bool is_clockwise, const bool is_closed) {
    const auto start_vec = start_point - center;
    const auto end_vec = terminate_point - center;

    // 始点の角度を [0, 2π) の範囲に正規化
    double start_angle = std::atan2(start_vec[1], start_vec[0]);
    if (start_angle < 0) start_angle += 2.0 * kPi;
    if (is_closed) return {start_angle, 2.0 * kPi};

    double end_angle = std::atan2(end_vec[1], end_vec[0]);
    if (end_angle < 0) end_angle += 2.0 * kPi;

    // 進行方向に沿って測った角度範囲を (0, 2π] にする
    double sweep = is_clockwise ? start_angle - end_angle
                                : end_angle - start_angle;
    if (sweep <= 0) sweep += 2.0 * kPi;
    return {start_angle, sweep};
}

}  // namespace



/**
 * コンストラクタ
 */

CircularArc::CircularArc(const RawEntityDE& de_record,
                         const IGESParameterVector& parameters,
                         const pointer2ID& de2id,
                         const ObjectID& iges_id)
    : EntityBase(de_record, parameters, de2id, iges_id) {
    InitializePD(de2id);
}

CircularArc::CircularArc(const Vector2d& center, const Vector2d& start_point,
                         const Vector2d& terminate_point, const double z_t,
                         const bool is_clockwise)
        : CircularArc(RawEntityDE::ByDefault(EntityType::kCircularArc),
                      IGESParameterVector{z_t, center[0], center[1],
                                         start_point[0], start_point[1],
                                         terminate_point[0], terminate_point[1]}) {
    // 値の検証: 中心から始点と終点までの距離が等しいことを確認
    double r1 = (start_point - center).norm();
    double r2 = (terminate_point - center).norm();
    if (!i_num::IsApproxEqual(r1, r2, i_num::kGeometryTolerance)) {
        throw igesio::EntityValueError(
            "Start and terminate points must be equidistant from the center.");
    }
    // 半径が0に近い場合はエラー
    if (i_num::IsApproxZero(r1, i_num::kGeometryTolerance)) {
        throw igesio::EntityValueError("Degenerate circular arc: radius is too small.");
    }
    is_clockwise_ = is_clockwise;
}

CircularArc::CircularArc(const Vector2d& center, const double radius,
                         const double start_angle, const double end_angle,
                         const double z_t)
        : CircularArc(RawEntityDE::ByDefault(EntityType::kCircularArc),
                      IGESParameterVector{
                            z_t, center[0], center[1],
                            center[0] + radius * std::cos(start_angle),
                            center[1] + radius * std::sin(start_angle),
                            center[0] + radius * std::cos(end_angle),
                            center[1] + radius * std::sin(end_angle)}) {
    // 半径が0に近い場合はエラー
    if (i_num::IsApproxZero(radius, i_num::kGeometryTolerance)) {
        throw igesio::EntityValueError("Degenerate circular arc: radius is too small.");
    }
    // 始点角が終点角より大きい場合は時計回りの弧と解釈する
    // (始終点の座標は各角度のcos/sinそのままで正しい)
    is_clockwise_ = start_angle > end_angle;
}

CircularArc::CircularArc(const Vector2d& center, const double radius,
                         const double z_t, const bool is_clockwise)
        : CircularArc(
            RawEntityDE::ByDefault(EntityType::kCircularArc),
            IGESParameterVector{z_t, center[0], center[1],
                                center[0] + radius, center[1],
                                center[0] + radius, center[1]}) {
    // 半径が0に近い場合はエラー
    if (i_num::IsApproxZero(radius, i_num::kGeometryTolerance)) {
        throw igesio::EntityValueError("Degenerate circular arc: radius is too small.");
    }
    is_clockwise_ = is_clockwise;
}



/**
 * EntityBase implementation
 */

igesio::IGESParameterVector CircularArc::GetMainPDParameters() const {
    // データをIGESParameterVectorに変換
    IGESParameterVector params{
        center_[2], center_[0], center_[1], start_point_[0], start_point_[1],
        terminate_point_[0], terminate_point_[1]};

    // pd_parameters_のフォーマットを適用
    // CircularArcの場合はPD部の要素数は常に同じためそのまま適用
    for (size_t i = 0; i < std::min(params.size(), pd_parameters_.size()); ++i) {
        try {
            params.set_format(i, pd_parameters_.get_format(i));
        } catch (const std::invalid_argument&) {
            // 変換元のフォーマットが正しくない場合は更新しない
        }
    }
    return params;
}

size_t CircularArc::SetMainPDParameters(const pointer2ID& de2id) {
    // パラメータの数が7以上であることを確認
    // CircularArcの7つのパラメータ + 追加のポインタ
    auto& pd = pd_parameters_;
    if (pd.size() < 7) {
        throw igesio::EntityParameterError("CircularArc requires at least 7 parameters");
    }

    // パラメータを設定
    auto z_t = pd.access_as<double>(0);
    center_ = {pd.access_as<double>(1), pd.access_as<double>(2), z_t};
    start_point_ = {pd.access_as<double>(3), pd.access_as<double>(4), z_t};
    terminate_point_ = {pd.access_as<double>(5), pd.access_as<double>(6), z_t};

    return 7;
}

igesio::ValidationResult CircularArc::ValidatePD() const {
    // 中心から始点と終点までの距離を計算
    double r1 = (start_point_ - center_).norm();
    double r2 = (terminate_point_ - center_).norm();

    std::vector<ValidationError> errors;

    // 距離が等しいか確認する（許容範囲内で）。等距離でなくても円弧は描画可能なため、
    // 幾何的品質の指摘 (kWarning) とし描画はブロックしない。
    if (!i_num::IsApproxEqual(r1, r2, i_num::kGeometryTolerance)) {
        // 点は中心から等距離でなければならない
        errors.push_back(ValidationError(
                "Start and terminate points must be equidistant from the center.",
                igesio::ValidationSeverity::kWarning)
                << " Start distance: " << r1 << ", Terminate distance: " << r2);
    }

    // 縮退した円弧でないか確認する（半径が小さすぎる）
    if (i_num::IsApproxZero(r1, i_num::kGeometryTolerance)) {
        // 半径が小さすぎる場合は縮退した円弧とみなす
        errors.emplace_back("Degenerate circular arc: radius is too small.");
    }

    return MakeValidationResult(std::move(errors));
}



/**
 * ICurve implementation
 */

std::array<double, 2> CircularArc::GetParameterRange() const {
    // 向きによらず {θs, θs + Δ}. 時計回りでは幾何角度が φ(t) = 2θs − t となる
    const auto angles = ComputeArcAngles(center_, start_point_, terminate_point_,
                                         is_clockwise_, IsClosed());
    return {angles.start, angles.start + angles.sweep};
}

bool CircularArc::IsClosed() const {
    // 始点と終点が一致するかどうかを確認
    return i_num::IsApproxEqual(start_point_, terminate_point_,
                                i_num::kGeometryTolerance);
}

std::optional<i_ent::CurveDerivatives>
CircularArc::TryGetDefinedDerivatives(const double t, const unsigned int n) const {
    const auto range = GetParameterRange();
    // 境界の浮動小数点誤差を許容し域内へ丸める
    auto tc = i_num::TryClampToRange(t, range[0], range[1]);
    if (!tc) return std::nullopt;

    const double radius = Radius();
    // 幾何角度 φ(t) = θs + σ(t − θs) (σ = CCWで+1、CWで−1)
    const double sigma = is_clockwise_ ? -1.0 : 1.0;
    const double phi = range[0] + sigma * (*tc - range[0]);

    CurveDerivatives result(n);
    // n階導関数を一般式で計算 (位相は k * PI/2 で増え、係数は σ^k)
    double coefficient = radius;
    for (unsigned int k = 0; k <= n; ++k) {
        double phase = phi + static_cast<double>(k) * (kPi / 2.0);
        result[k] = Vector3d{
            coefficient * std::cos(phase),
            coefficient * std::sin(phase),
            0.0
        };
        coefficient *= sigma;
    }

    // 0階導関数は位置ベクトルに変換
    result[0] += center_;

    return result;
}

i_num::BoundingBox CircularArc::GetDefinedBoundingBox() const {
    Vector3d min = Center();
    Vector3d max = Center();
    auto r = Radius();

    // 弧が通過する幾何角度の区間 (CCW: [θs, θs+Δ]、CW: [θs−Δ, θs])
    const auto angles = ComputeArcAngles(center_, start_point_, terminate_point_,
                                         is_clockwise_, IsClosed());
    const double start = is_clockwise_ ? angles.start - angles.sweep
                                       : angles.start;
    const double end = start + angles.sweep;
    // 始点と終点を考慮 (角度の往復変換誤差を避けるため保存済みの点を直接使用)
    min = min.cwiseMin(start_point_).cwiseMin(terminate_point_);
    max = max.cwiseMax(start_point_).cwiseMax(terminate_point_);

    // [start, end]の間にある主要な角度 (0, π/2, π, 3π/2) をチェック
    // (startが負でもfloorによるオフセットはそのまま機能する)
    auto offset = static_cast<int>(std::floor(start / (kPi / 2.0)));
    for (int i = 0; i < 4; ++i) {
        double angle = (offset + i) * (kPi / 2.0);
        if (i_num::IsApproxLEQ(start, angle) && i_num::IsApproxLEQ(angle, end)) {
            Vector3d point = center_ +
                Vector3d{r * std::cos(angle), r * std::sin(angle), 0.0};
            min = min.cwiseMin(point);
            max = max.cwiseMax(point);
        }
    }

    return i_num::BoundingBox(min, max);
}



/**
 * 出力時展開
 */

i_ent::EntityBase::ExportExpansion CircularArc::ExpandForExport() const {
    if (!is_clockwise_) return {};

    // 鏡映行列Fを計算する
    //    中心を通るXT軸平行線まわりのπ回転 (R = diag(1, −1, −1)、
    //    T = (0, 2yc, 2zt)). det = +1 なのでForm 0. 従属スイッチは既定の00のまま
    //    (DE第7欄からの参照は従属関係を作らず (IGES 5.3 §2.2.4.4.9.2)、
    //    Type 124の従属スイッチは規格上n.a.のため)
    auto flip = MakeRotation(kPi, Vector3d::UnitX(), center_);

    // 元の弧が変換行列M0を参照している場合はF → M0のチェーンにする (点はFと次にM0で変換)
    const auto& de7 = GetTransformationMatrix();
    if (de7.GetValueType() == DEFieldValueType::kPointer) {
        auto base = de7.GetPointer();
        if (!base) {
            throw igesio::ReferenceError(
                "CircularArc::ExpandForExport: the referenced transformation"
                " matrix (ID " + ToString(de7.GetID()) + ") is unresolved.");
        }
        flip->SetReference(base);
    }

    // 鏡映CCW弧を作成する
    //    y成分を中心について反転する (中心・z_tは不変).
    //    フォーマットはpd_parameters_から添字対応で複製する
    const double yc = center_[1];
    IGESParameterVector mirrored_params{
        center_[2], center_[0], yc,
        start_point_[0], 2.0 * yc - start_point_[1],
        terminate_point_[0], 2.0 * yc - terminate_point_[1]};
    for (size_t i = 0; i < std::min(mirrored_params.size(),
                                    pd_parameters_.size()); ++i) {
        try {
            mirrored_params.set_format(i, pd_parameters_.get_format(i));
        } catch (const std::invalid_argument&) {
            // 変換元のフォーマットが正しくない場合は更新しない
        }
    }
    auto mirrored = std::make_shared<CircularArc>(
            RawEntityDE::ByDefault(EntityType::kCircularArc), mirrored_params);
    mirrored->CopyCommonPropertiesFrom(*this);
    // DE第7欄をM0からFへ差し替える (Fは新規IDなので循環しない)
    mirrored->OverwriteTransformationMatrix(flip);

    return {mirrored, {flip}};
}



/**
 * 描画用
 */

double CircularArc::Radius() const {
    // 始点と中心の距離を計算
    return (start_point_ - center_).norm();
}

double CircularArc::SweepAngle() const {
    return ComputeArcAngles(center_, start_point_, terminate_point_,
                            is_clockwise_, IsClosed()).sweep;
}

double CircularArc::StartAngle() const {
    return ComputeArcAngles(center_, start_point_, terminate_point_,
                            is_clockwise_, IsClosed()).start;
}

double CircularArc::EndAngle() const {
    const auto angles = ComputeArcAngles(center_, start_point_, terminate_point_,
                                         is_clockwise_, IsClosed());
    return is_clockwise_ ? angles.start - angles.sweep
                         : angles.start + angles.sweep;
}



/**
 * ファクトリ関数
 */

std::shared_ptr<CircularArc> i_ent::MakeCircularArc(
        const Vector2d& center, const Vector2d& start_point,
        const Vector2d& terminate_point, const double z_t,
        const bool is_clockwise) {
    return std::make_shared<CircularArc>(
            center, start_point, terminate_point, z_t, is_clockwise);
}

std::shared_ptr<CircularArc> i_ent::MakeCircularArc(
        const Vector2d& center, const double radius,
        const double start_angle, const double end_angle, const double z_t) {
    return std::make_shared<CircularArc>(
            center, radius, start_angle, end_angle, z_t);
}

std::shared_ptr<CircularArc> i_ent::MakeCircle(
        const Vector2d& center, const double radius, const double z_t,
        const bool is_clockwise) {
    return std::make_shared<CircularArc>(center, radius, z_t, is_clockwise);
}

std::shared_ptr<CircularArc> i_ent::MakeCircularArcThroughPoints(
        const Vector2d& start_point, const Vector2d& mid_point,
        const Vector2d& terminate_point, const double z_t) {
    const Vector2d u = mid_point - start_point;
    const Vector2d v = terminate_point - start_point;

    // 外積を辺長の積で正規化した相対判定により、共線と点の一致を一括検出する
    const double cross = u.x() * v.y() - u.y() * v.x();
    if (std::abs(cross) <= i_num::kGeometryTolerance * u.norm() * v.norm()) {
        throw igesio::EntityValueError(
                "MakeCircularArcThroughPoints: The three points must not be"
                " collinear or coincident.");
    }

    // 垂直二等分線の交点として外心を計算する (構成上3点と等距離になるため、
    // ラップ先コンストラクタの等距離検証は常に通過する)
    const double u2 = u.squaredNorm(), v2 = v.squaredNorm();
    const Vector2d center = start_point +
            Vector2d{v.y() * u2 - u.y() * v2,
                     u.x() * v2 - v.x() * u2} / (2.0 * cross);

    // 3点が時計回り (cross < 0) の場合は入力順のまま時計回りの弧にする
    // (通過点が弧上に乗り、パラメータの進行方向も入力どおりになる)
    return std::make_shared<CircularArc>(
            center, start_point, terminate_point, z_t, cross < 0.0);
}
