/**
 * @file entities/curves/algorithms/curve_line_intersection.h
 * @brief 曲線と直線（半直線・線分を含む）の近接判定
 * @author Yayoi Habami
 * @date 2026-05-27
 * @copyright 2026 Yayoi Habami
 *
 * @details
 * ICurveの具象クラスに依存せず、以下のAPIのみを使用する:
 *   - TryGetDerivatives(t, 1) : 定義空間における |C'(t)| の取得
 *   - TryGetPointAt(t) / TryGetTangentAt(t) : ワールド空間の点・単位接線
 *   - GetParameterRange() : パラメータ範囲（無限範囲はクランプする）
 *   - GetBoundingBox() : バウンディングボックス（広域棄却の外接球に使用）
 *
 * ### アルゴリズム概要
 * 曲線はレイと厳密交差しないため、最近接距離gapが許容値hit_tolerance
 * 以下の点を「ヒット」とみなす。直線を L(s) = p0 + s*(p1-p0)、曲線をC(t)
 * とするとき、distance^2(C(t), L(s))を最小化する。線パラメータsは
 * s(t) = (C(t)-p0)・d / |d|^2 で解析的に消去でき、1変数tの
 * ニュートン反復に帰着する（曲面版IntersectSurfaceWithLineと対称）。
 *
 * 収束への多スタートのため、パラメータ範囲をN分割して各サンプル点と
 * 有限端点を初期値とする。
 */
#ifndef IGESIO_ENTITIES_CURVES_ALGORITHMS_CURVE_LINE_INTERSECTION_H_
#define IGESIO_ENTITIES_CURVES_ALGORITHMS_CURVE_LINE_INTERSECTION_H_

#include <vector>

#include "igesio/numerics/bounding_box.h"
#include "igesio/numerics/matrix.h"
#include "igesio/entities/interfaces/i_curve.h"



namespace igesio::entities {

/// @brief 曲線と直線（半直線・線分を含む）の最近傍情報
struct CurveLineIntersection {
    /// @brief 曲線側の最近傍点C(t)の3D座標
    Vector3d position;
    /// @brief 曲線パラメータt
    double t_curve;
    /// @brief 線パラメータs (L(s) = p0 + s*(p1-p0) に対応)
    /// @note kSegmentの場合s∈[0,1]、kRayの場合s≥0、kLineの場合は任意
    double t_line;
    /// @brief 曲線と線の3D距離 (C(t)とL(s)の距離)
    double gap;
};

/// @brief IntersectCurveWithLineの探索制御パラメータ
struct CurveLineIntersectionParams {
    /// @brief パラメータ範囲の初期格子サンプル数
    /// @note 増やすと多重解の検出精度が上がるが計算コストが増加する
    int curve_samples = 20;
    /// @brief ニュートン法の最大反復回数
    int max_iter = 50;
    /// @brief 収束判定の許容誤差 (パラメータtの更新幅)
    double convergence_tol = 1e-9;
    /// @brief 重複解の除去に使用する3D空間距離の許容誤差
    double dedup_tol = 1e-6;
    /// @brief [モデル単位] gapがこの値以下の最近傍点のみを返す
    /// @note スタンドアロン利用時のデフォルト値。ピッキング経路では
    ///       PickEntitiesがピクセル換算したワールド距離を毎回上書きする。
    double hit_tolerance = 1.0;
};

/// @brief 曲線と直線（半直線・線分を含む）の最近傍点を求める
///
/// distance^2(C(t), L(s))を最小化する。線パラメータsは射影により
/// 解析的に消去され、1変数tのガウス・ニュートン反復に帰着する。
/// 初期値としてパラメータ範囲を格子サンプリングし、各サンプルと有限端点を
/// 起点とする多スタート戦略を採用する。
///
/// @param curve      最近傍点を求める曲線
/// @param p0         線の始点（kLineの場合は通過点1）
/// @param p1         線の終点（kLine/kRayの場合は通過方向を定める第2点）
/// @param line_type  線の種類（kSegment: 線分, kRay: 半直線, kLine: 無限直線）
/// @param params     探索制御パラメータ
/// @return           gap < params.hit_toleranceの最近傍点のリスト
///                   （重複除去済み・t_line昇順ソート済み）
/// @throw std::invalid_argument p0またはp1に無限大・NaNの成分がある場合
/// @note 線方向ベクトル (p1-p0) のノルムがゼロの場合は空リストを返す
/// @note パラメータ範囲が無限大の場合、バウンディングボックスの
///       対角線長を基準に探索範囲をクランプする
std::vector<CurveLineIntersection> IntersectCurveWithLine(
        const ICurve& curve,
        const Vector3d& p0, const Vector3d& p1,
        numerics::BoundingBox::DirectionType line_type,
        const CurveLineIntersectionParams& params = {});

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_CURVES_ALGORITHMS_CURVE_LINE_INTERSECTION_H_
