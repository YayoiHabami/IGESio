/**
 * @file entities/curves/circular_arc.cpp
 * @brief CircularArc (Type 100): 円弧エンティティの定義
 * @author Yayoi Habami
 * @date 2025-08-02
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/curves/circular_arc.h"

#include <utility>
#include <vector>

#include "igesio/numerics/tolerance.h"
#include "igesio/common/iges_parameter_vector.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using CircularArc = i_ent::CircularArc;
using Vector3d = igesio::Vector3d;

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
                         const Vector2d& terminate_point, const double z_t)
        : CircularArc(RawEntityDE::ByDefault(EntityType::kCircularArc),
                      IGESParameterVector{z_t, center[0], center[1],
                                         start_point[0], start_point[1],
                                         terminate_point[0], terminate_point[1]}) {
    // 値の検証: 中心から始点と終点までの距離が等しいことを確認
    double r1 = (start_point - center).norm();
    double r2 = (terminate_point - center).norm();
    if (!i_num::IsApproxEqual(r1, r2, i_num::kGeometryTolerance)) {
        throw igesio::DataFormatError(
            "Start and terminate points must be equidistant from the center.");
    }
    // 半径が0に近い場合はエラー
    if (i_num::IsApproxZero(r1, i_num::kGeometryTolerance)) {
        throw igesio::DataFormatError("Degenerate circular arc: radius is too small.");
    }
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
        throw igesio::DataFormatError("Degenerate circular arc: radius is too small.");
    }
    // 始点と終点の角度が不正な場合はエラー
    if (start_angle > end_angle) {
        throw igesio::DataFormatError(
            "Start angle must be less than end angle for a circular arc.");
    }
}

CircularArc::CircularArc(const Vector2d& center, const double radius,
                         const double z_t)
        : CircularArc(
            RawEntityDE::ByDefault(EntityType::kCircularArc),
            IGESParameterVector{z_t, center[0], center[1],
                                center[0] + radius, center[1],
                                center[0] + radius, center[1]}) {
    // 半径が0に近い場合はエラー
    if (i_num::IsApproxZero(radius, i_num::kGeometryTolerance)) {
        throw igesio::DataFormatError("Degenerate circular arc: radius is too small.");
    }
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
        throw igesio::DataFormatError("CircularArc requires at least 7 parameters");
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

    // 距離が等しいか確認する（許容範囲内で）
    if (!i_num::IsApproxEqual(r1, r2, i_num::kGeometryTolerance)) {
        // 点は中心から等距離でなければならない
        errors.push_back(ValidationError(
                "Start and terminate points must be equidistant from the center.")
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
    // 中心から始点・終点へのベクトルを計算
    const auto start_vec = start_point_ - center_;
    const auto end_vec = terminate_point_ - center_;

    // 始点と終点の角度を計算 (atan2は[-PI, PI]の範囲で返す)
    double start_angle = std::atan2(start_vec[1], start_vec[0]);
    double end_angle = std::atan2(end_vec[1], end_vec[0]);

    // 始点の角度を [0, 2*PI) の範囲に正規化
    if (start_angle < 0) start_angle += 2.0 * kPi;

    // 終点の角度を、始点から反時計回りに進んだ角度として計算
    if (end_angle <= start_angle) end_angle += 2.0 * kPi;

    // 閉じた円の場合 (始点と終点が同じ)
    if (IsClosed()) {
        end_angle = start_angle + 2.0 * kPi;
    }

    return {start_angle, end_angle};
}

bool CircularArc::IsClosed() const {
    // 始点と終点が一致するかどうかを確認
    return i_num::IsApproxEqual(start_point_, terminate_point_,
                                i_num::kGeometryTolerance);
}

std::optional<i_ent::CurveDerivatives>
CircularArc::TryGetDerivatives(const double t, const unsigned int n) const {
    const auto range = GetParameterRange();
    if (t < range[0] || t > range[1]) return std::nullopt;

    const double radius = Radius();

    CurveDerivatives result(n);
    // n階導関数を一般式で計算（位相は k * PI/2 で増える）
    for (unsigned int k = 0; k <= n; ++k) {
        double phase = t + static_cast<double>(k) * (kPi / 2.0);
        result[k] = Vector3d{
            radius * std::cos(phase),
            radius * std::sin(phase),
            0.0
        };
    }

    if (n >= 0) {
        // 0階導関数は位置ベクトルに変換
        result[0] += center_;
    }

    return result;
}



/**
 * 描画用
 */

double CircularArc::Radius() const {
    // 始点と中心の距離を計算
    return (start_point_ - center_).norm();
}

double CircularArc::StartAngle() const {
    return GetParameterRange()[0];
}

double CircularArc::EndAngle() const {
    return GetParameterRange()[1];
}
