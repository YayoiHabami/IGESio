/**
 * @file entities/surfaces/algorithms/surface_line_intersection.h
 * @brief 曲面と直線（半直線・線分を含む）の交点計算
 * @author Yayoi Habami
 * @date 2026-04-13
 * @copyright 2026 Yayoi Habami
 *
 * @details
 * ISurfaceの具象クラスに依存せず、以下の2つのAPIのみを使用する:
 *   - TryGetDerivatives(u, v, 1) : 点座標S(u,v)と偏導関数Su, Svの取得
 *   - GetBoundingBox().Intersects() : バウンディングボックスによる事前棄却
 *
 * 穴のある曲面（TryGetDerivativesがnulloptを返す領域）との交点は
 * 結果に含まれない。
 *
 * ### アルゴリズム概要
 * 直線を L(t) = p0 + t*(p1-p0)、曲面をS(u,v)とするとき、
 * 交点条件 F(u,v,t) = S(u,v) - L(t) = 0 を多変数ニュートン法で解く。
 * ヤコビアン J = [Su | Sv | -(p1-p0)] (3×3) はTryGetDerivativesから構築する。
 *
 * 収束への多スタートのため、uvパラメータ空間をN×N格子でサンプリングし、
 * 各格子点から線上への射影を初期tとして使用する。
 */
#ifndef IGESIO_ENTITIES_SURFACES_ALGORITHMS_SURFACE_LINE_INTERSECTION_H_
#define IGESIO_ENTITIES_SURFACES_ALGORITHMS_SURFACE_LINE_INTERSECTION_H_

#include <vector>

#include "igesio/numerics/geometric/bounding_box.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/entities/interfaces/i_surface.h"



namespace igesio::entities {

/// @brief 曲面と直線（半直線・線分を含む）の交点情報
struct SurfaceLineIntersection {
    /// @brief 3D交点座標
    Vector3d position;
    /// @brief 交点のuパラメータ値
    double u;
    /// @brief 交点のvパラメータ値
    double v;
    /// @brief 線パラメータt (L(t) = p0 + t*(p1-p0) に対応)
    /// @note kSegmentの場合t∈[0,1]、kRayの場合 t≥0、kLineの場合は任意
    double t;
};

/// @brief IntersectSurfaceWithLineの探索制御パラメータ
struct SurfaceLineIntersectionParams {
    /// @brief u方向の初期格子サンプル数
    /// @note 増やすと多重解の検出精度が上がるが計算コストが増加する
    int u_samples = 20;
    /// @brief v方向の初期格子サンプル数
    int v_samples = 20;
    /// @brief ニュートン法の最大反復回数
    int max_iter = 50;
    /// @brief 収束判定の許容誤差 (残差ベクトルのノルム)
    double convergence_tol = 1e-9;
    /// @brief 重複解の除去に使用する3D空間距離の許容誤差
    double dedup_tol = 1e-6;
};

/// @brief 曲面と直線（半直線・線分を含む）の交点を計算する
///
/// 交点条件 S(u,v) = L(t) を多変数ニュートン法で求解する。
/// 初期値として uvパラメータ空間を格子サンプリングし、各サンプル点を
/// 線分への射影によってtの初期値を生成する多スタート戦略を採用する。
///
/// 穴のある曲面（TrimmedSurface等）では、ニュートン法の各反復で
/// TryGetDerivativesがnulloptを返した場合にその探索を打ち切る。
///
/// @param surface    交点を求める曲面
/// @param p0         線の始点（kLineの場合は通過点1）
/// @param p1         線の終点（kLine/kRayの場合は通過方向を定める第2点）
/// @param line_type  線の種類（kSegment: 線分, kRay: 半直線, kLine: 無限直線）
/// @param params     探索制御パラメータ
/// @return           有効な交点のリスト（重複除去済み・t昇順ソート済み）
/// @throw std::invalid_argument p0またはp1に無限大・NaNの成分がある場合
/// @note 線方向ベクトル (p1-p0) のノルムがゼロの場合は空リストを返す
/// @note uvパラメータ範囲が無限大の場合、バウンディングボックスの
///       対角線長を基準に探索範囲をクランプする
std::vector<SurfaceLineIntersection> IntersectSurfaceWithLine(
        const ISurface& surface,
        const Vector3d& p0, const Vector3d& p1,
        numerics::BoundingBox::DirectionType line_type,
        const SurfaceLineIntersectionParams& params = {});

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_SURFACES_ALGORITHMS_SURFACE_LINE_INTERSECTION_H_
