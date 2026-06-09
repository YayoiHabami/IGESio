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

#include "igesio/numerics/geometric/polygon.h"
#include "igesio/entities/interfaces/i_curve.h"

// curves/algorithms下の関数を取得
#include "igesio/entities/curves/algorithms/curve_line_intersection.h"



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

/// @brief 閉性判定の相対許容誤差のデフォルト値
/// @note 始終点ギャップ <= この値×曲線の広がり (サンプル点のAABB対角長) なら
///       閉曲線とみなす。実CADが出力するトリム境界ループの継ぎ目には1e-6程度の
///       ギャップが普通に存在するため、トリム用途では厳密判定にしない
constexpr double kCurveClosureRelTolerance = 1e-4;

/// @brief 閉曲線の内包・外包・近似多角形を計算する
/// @param curve 対象の閉曲線 (自己交差なし)
/// @param n_vert 内包・外包多角形の初期分割数
/// @param reference_normal 曲線が乗る平面の法線ベクトル
/// @param curvature_eps 曲率判定の閾値
/// @param distance_eps 近似多角形の許容距離
/// @param closure_rel_tol 閉性判定の相対許容. 始終点ギャップがこの値×曲線の広がり
///        以下なら閉曲線とみなす. 0なら`IsClosed()`による厳密判定のみ
/// @return 内包・外包・近似多角形を保持するデータ構造
/// @throws std::invalid_argument curve が閉曲線でない場合 (始終点ギャップが
///         closure_rel_tol×曲線の広がりを超える場合)、自己交差が検出された場合
/// @throws igesio::ComputationError 接線・曲率の計算に失敗した場合
numerics::CurveContainmentPolygons ComputeContainmentPolygons(
    const ICurve& curve,
    int n_vert,
    const Vector3d& reference_normal,
    double curvature_eps = 1e-3,
    double distance_eps = 1e-3,
    double closure_rel_tol = kCurveClosureRelTolerance);

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_CURVES_ALGORITHMS_H_
