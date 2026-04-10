/**
 * @file entities/curves/algorithms/extremal_polygon.h
 * @brief 閉曲線の外包/内包多角形を構築するアルゴリズム (内部実装)
 * @author Yayoi Habami
 * @date 2026-04-10
 * @copyright 2026 Yayoi Habami
 * @note 本ファイルは内部実装用であり、公開APIには含めない.
 *       呼び出し側は以下を保証すること:
 *       - curveは自己交差のない閉曲線であること (近似チェックのみ実施)
 *       - reference_normalは曲線が乗る平面の法線ベクトルであること
 */
#ifndef SRC_ENTITIES_CURVES_ALGORITHMS_EXTREMAL_POLYGON_H_
#define SRC_ENTITIES_CURVES_ALGORITHMS_EXTREMAL_POLYGON_H_

#include <memory>

#include "igesio/numerics/matrix.h"
#include "igesio/numerics/polygon.h"
#include "igesio/entities/interfaces/i_curve.h"



namespace igesio::entities {

/// @brief 外包多角形 (閉曲線を完全に囲む多角形) の頂点列を計算する
///
/// 曲線の符号付き曲率の極値点をサンプリング起点とし、凸性判定に基づいて
/// 多角形の頂点 (vertex) と接点 (contact point) を分類し、頂点列を構築する.
///
/// @param curve 対象の閉曲線 (自己交差なし). 角点を含んでもよい.
/// @param n_vert 初期分割数 (実際の頂点数はこれより多くなる場合あり)
/// @param reference_normal 曲線が乗る平面の法線ベクトル (正規化不要)
/// @param eps 曲率判定の閾値
/// @return 外包多角形の頂点データ
/// @throws std::invalid_argument curve が閉曲線でない場合、自己交差が検出された場合
/// @throws std::runtime_error 接線・曲率の計算に失敗した場合
numerics::PolygonData ComputeCircumscribedPolygon(
    const std::shared_ptr<const ICurve>& curve,
    int n_vert,
    const Vector3d& reference_normal,
    double eps = 1e-9);

/// @brief 内包多角形 (閉曲線に完全に囲まれる多角形) の頂点列を計算する
///
/// 曲線の符号付き曲率の極値点をサンプリング起点とし、凸性判定に基づいて
/// 多角形の頂点 (vertex) と接点 (contact point) を分類し、頂点列を構築する.
///
/// @param curve 対象の閉曲線 (自己交差なし). 角点を含んでもよい.
/// @param n_vert 初期分割数 (実際の頂点数はこれより多くなる場合あり)
/// @param reference_normal 曲線が乗る平面の法線ベクトル (正規化不要)
/// @param eps 曲率判定の閾値
/// @return 内包多角形の頂点データ
/// @throws std::invalid_argument curve が閉曲線でない場合、自己交差が検出された場合
/// @throws std::runtime_error 接線・曲率の計算に失敗した場合
numerics::PolygonData ComputeInscribedPolygon(
    const std::shared_ptr<const ICurve>& curve,
    int n_vert,
    const Vector3d& reference_normal,
    double eps = 1e-9);

}  // namespace igesio::entities

#endif  // SRC_ENTITIES_CURVES_ALGORITHMS_EXTREMAL_POLYGON_H_
