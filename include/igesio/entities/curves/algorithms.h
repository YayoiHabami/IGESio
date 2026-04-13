/**
 * @file entities/curves/algorithms.h
 * @brief 曲線関連のアルゴリズム定義
 * @author Yayoi Habami
 * @date 2025-10-29
 * @copyright 2025 Yayoi Habami
 * @note ここではICurveおよびその派生クラスに対するアルゴリズムを定義する.
 *       例えば、曲線の折れ線近似など. 曲面などが関わる場合（例: 曲線と曲面の交線計算）は
 *       entities/surfaces/algorithms.hに定義するなど、より高位のモジュールに定義すること.
 */
#ifndef IGESIO_ENTITIES_CURVES_ALGORITHMS_H_
#define IGESIO_ENTITIES_CURVES_ALGORITHMS_H_

#include <utility>

#include "igesio/numerics/polygon.h"
#include "igesio/entities/interfaces/i_curve.h"



namespace igesio::entities {

/// @brief 点と直線の距離を計算する
/// @param point 点の座標
/// @param anchor1 直線上の点1の座標
/// @param anchor2 直線上の点2の座標
/// @param is_finite anchor1側とanchor2側が有限かどうか
/// @return 点と直線の距離
/// @note is_finite = {true, true} の場合は線分として扱い、点から直線への垂線の足が
///       線分の外に出る場合は端点までの距離を返す. is_finite = {true, false} または
///       {false, true} の場合は半直線として扱う. is_finite = {false, false} の場合は
///       無限直線として扱う.
/// @note anchor1とanchor2が同じ点の場合、is_finiteの値に関わらず、
///       anchor1までの距離を返す.
double PointLineDistance(
        const Vector3d&, const Vector3d&, const Vector3d&,
        const std::pair<bool, bool>& = {false, false});

/// @brief 閉曲線の内包・外包・近似多角形を計算する
/// @param curve 対象の閉曲線 (自己交差なし)
/// @param n_vert 内包・外包多角形の初期分割数
/// @param reference_normal 曲線が乗る平面の法線ベクトル
/// @param curvature_eps 曲率判定の閾値
/// @param distance_eps 近似多角形の許容距離
/// @return 内包・外包・近似多角形を保持するデータ構造
/// @throws std::invalid_argument curve が閉曲線でない場合、自己交差が検出された場合
/// @throws std::runtime_error 接線・曲率の計算に失敗した場合
numerics::CurveContainmentPolygons ComputeContainmentPolygons(
    const ICurve& curve,
    int n_vert,
    const Vector3d& reference_normal,
    double curvature_eps = 1e-3,
    double distance_eps = 1e-3);

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_CURVES_ALGORITHMS_H_
