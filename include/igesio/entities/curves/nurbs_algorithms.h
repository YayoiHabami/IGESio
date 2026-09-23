/**
 * @file entities/curves/nurbs_algorithms.h
 * @brief 任意曲線の NURBS 近似・補間アルゴリズムの公開 API
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

#include "igesio/numerics/core/matrix.h"
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



/// @brief NURBS補間オプション
struct NurbsInterpOptions {
    /// @brief 角点とみなす折れ角の閾値 [rad]
    /// @note 0より大きくπ/2以下とする。前後の弦のなす角がこれを超える点は
    ///       角点とし、曲線をC⁰連続で折る
    double corner_angle = kPi / 2.0;
    /// @brief 与えられた接線を採用する、推定接線との角度差の上限 [rad]
    /// @note 0.0以上π/2以下とする
    double tangent_tolerance = kPi / 12.0;
    /// @brief 同一点とみなす連続点間の距離
    /// @note 0.0以上とする。点列の折れ線長の1e-12倍の方が大きい場合はそちらを使う
    double duplicate_tolerance = 1e-9;
};

/// @brief 点列の補間結果
struct NurbsInterpolation {
    /// @brief 補間曲線 (3次、パラメータ範囲0.0〜1.0)
    std::shared_ptr<RationalBSplineCurve> curve;
    /// @brief 各入力点に対応する曲線上のパラメータ
    /// @note 入力点と同数で単調非減少。同一点とみなした点は同じ値をもつ
    std::vector<double> parameters;
};

/// @brief 離散点列を通る3次NURBS曲線を局所エルミート補間で作成する
/// @param points   順序付き離散点列 Q_0, ..., Q_n
/// @param tangents 各点の接線方向 (空、または点列と同数)
/// @param options  補間オプション
/// @return 補間曲線と各入力点のパラメータ
/// @throw std::invalid_argument 相異なる点が2点未満の場合、点に非有限値を
///        含む場合、tangentsが空でなく点列と同数でない場合、
///        またはoptionsの値が不正な場合
/// @note (1) 曲線は全点を通り、角点以外ではC¹連続、角点ではC⁰連続となる
///       (2) 相異なる点の間の各区間は3次曲線であり、弦 (直線) からの距離は
///           弦長の1/3以下に収まる
///       (3) 各点のパラメータは、相異なる点の累積弦長を全弦長で割った値となる
///       (4) tangentsの各要素は方向のみを使う。std::nullopt、零ベクトル、
///           非有限値、推定接線との角度差がtangent_toleranceを超えるもの、
///           隣接する弦と鈍角をなすもの、および角点の接線は無視し、推定値を使う
NurbsInterpolation InterpolateWithNurbs(
    const std::vector<Vector3d>& points,
    const std::vector<std::optional<Vector3d>>& tangents = {},
    const NurbsInterpOptions& options = {});

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_CURVES_NURBS_ALGORITHMS_H_
