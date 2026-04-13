/**
 * @file entities/curves/nurbs_algorithms.h
 * @brief 任意曲線の NURBS 近似アルゴリズムの公開 API
 * @author Yayoi Habami
 * @date 2026-04-11
 * @copyright 2026 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_CURVES_NURBS_ALGORITHMS_H_
#define IGESIO_ENTITIES_CURVES_NURBS_ALGORITHMS_H_

#include <array>
#include <memory>
#include <optional>
#include <vector>

#include "igesio/numerics/matrix.h"
#include "igesio/entities/interfaces/i_curve.h"
#include "igesio/entities/curves/rational_b_spline_curve.h"



namespace igesio::entities {

/// @brief NURBS近似オプション
struct NurbsApproxOptions {
    /// @brief B-スプラインの次数m
    unsigned int degree = 3;
    /// @brief 許容最大近似誤差ε
    double tolerance = 1e-4;
    /// @brief 制御点数の上限（反復打ち切り条件）
    unsigned int max_control_points = 200;
    /// @brief 制御点初期数推定の方向変化角閾値 θ_tol [rad]
    double angle_per_segment = kPi/4.0;
};

/// @brief 端点接線拘束
/// @note std::nulloptの場合、その端点は拘束なし（r = 1 扱い）
struct NurbsEndpointTangents {
    /// @brief 始点の接線方向（正規化不要、方向のみ使用）
    std::optional<Vector3d> start;
    /// @brief 終点の接線方向
    std::optional<Vector3d> end;
};



/// @brief 離散点列からNURBS曲線を近似する（コア実装）
/// @param points   順序付き離散点列 Q_0, ..., Q_L
/// @param tangents 端点接線拘束（r ≤ 2; 両端に接線が提供された場合のみr=2）
/// @param options  近似オプション
/// @return 近似された RationalBSplineCurve（パラメータ範囲 [0, 1]）
/// @throw std::invalid_argument pointsが2点未満の場合、
///        または options の値が不正な場合
std::shared_ptr<RationalBSplineCurve> ApproximateWithNurbs(
    const std::vector<Vector3d>& points,
    const NurbsEndpointTangents& tangents = {},
    const NurbsApproxOptions& options = {});

/// @brief 連続曲線からNURBS曲線を近似する（ICurve ラッパー）
/// @param curve       近似対象の曲線（指定範囲内で連続かつ角点なし）
/// @param param_range 近似するパラメータ範囲
///                    （std::nulloptの場合curve.GetParameterRange()を使用）
/// @param options     近似オプション
/// @return 近似された RationalBSplineCurve（パラメータ範囲 [0, 1]）
/// @throw std::invalid_argument param_rangeが有限でない場合、
///        またはcurveの評価に失敗した場合
std::shared_ptr<RationalBSplineCurve> ApproximateWithNurbs(
    const ICurve& curve,
    std::optional<std::array<double, 2>> param_range = std::nullopt,
    const NurbsApproxOptions& options = {});

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_CURVES_NURBS_ALGORITHMS_H_
