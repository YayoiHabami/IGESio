/**
 * @file entities/curves/algorithms/extremal_polygon.h
 * @brief 閉曲線の外包/内包多角形を構築するアルゴリズム（内部実装）
 * @author Yayoi Habami
 * @date 2026-04-10
 * @copyright 2026 Yayoi Habami
 * @note 本ファイルは内部実装用であり、公開APIには含めない.
 *       呼び出し側は以下を保証すること:
 *       - curveは自己交差のない閉曲線であること（近似チェックのみ実施）
 *       - reference_normalは曲線が乗る平面の法線ベクトルであること
 */
#ifndef SRC_ENTITIES_CURVES_ALGORITHMS_EXTREMAL_POLYGON_H_
#define SRC_ENTITIES_CURVES_ALGORITHMS_EXTREMAL_POLYGON_H_

#include <utility>

#include "igesio/numerics/core/matrix.h"
#include "igesio/numerics/geometric/polygon.h"
#include "igesio/entities/interfaces/i_curve.h"



namespace igesio::entities {

/// @brief 外包多角形（閉曲線を完全に囲む多角形）の頂点列を計算する
/// @param curve 対象の閉曲線（自己交差なし）. 角点を含んでもよい.
/// @param n_vert 初期分割数（実際の頂点数はこれより多くなる場合あり）
/// @param reference_normal 曲線が乗る平面の法線ベクトル（正規化不要）
/// @param eps 曲率判定の閾値
/// @param closure_rel_tol 閉性判定の相対許容. 始終点ギャップがこの値×曲線の広がり
///        （サンプル点のAABB対角長）以下なら閉曲線とみなす.
///        0なら`IsClosed()`による厳密判定のみ
/// @return 外包多角形の頂点データ
/// @throws std::invalid_argument curve が閉曲線でない場合（始点-終点ギャップが
///         closure_rel_tol×曲線の広がりを超える場合）、自己交差が検出された場合
/// @throws igesio::ComputationError 接線・曲率の計算に失敗した場合
/// @note 曲線の符号付き曲率の極値点をサンプリングの起点とし、凸性判定に基づいて
///       多角形の頂点と接点を分類し、頂点列を構築する.
numerics::PolygonData ComputeCircumscribedPolygon(
    const ICurve& curve, int n_vert, const Vector3d& reference_normal,
    double eps = 1e-9, double closure_rel_tol = 0.0);

/// @brief 内包多角形（閉曲線に完全に囲まれる多角形）の頂点列を計算する
/// @param curve 対象の閉曲線（自己交差なし）. 角点を含んでもよい.
/// @param n_vert 初期分割数（実際の頂点数はこれより多くなる場合あり）
/// @param reference_normal 曲線が乗る平面の法線ベクトル（正規化不要）
/// @param eps 曲率判定の閾値
/// @param closure_rel_tol 閉性判定の相対許容. 始終点ギャップがこの値×曲線の広がり
///        （サンプル点のAABB対角長）以下なら閉曲線とみなす.
///        0なら`IsClosed()`による厳密判定のみ
/// @return 内包多角形の頂点データ
/// @throws std::invalid_argument curve が閉曲線でない場合（始点-終点ギャップが
///         closure_rel_tol×曲線の広がりを超える場合）、自己交差が検出された場合
/// @throws igesio::ComputationError 接線・曲率の計算に失敗した場合
/// @note 曲線の符号付き曲率の極値点をサンプリングの起点とし、凸性判定に基づいて
///       多角形の頂点と接点を分類し、頂点列を構築する.
numerics::PolygonData ComputeInscribedPolygon(
    const ICurve& curve, int n_vert, const Vector3d& reference_normal,
    double eps = 1e-9, double closure_rel_tol = 0.0);

/// @brief 外包多角形と内包多角形を一括で計算する
/// @param curve 対象の閉曲線（自己交差なし）. 角点を含んでもよい。
/// @param n_vert 初期分割数（実際の頂点数はこれより多くなる場合あり）
/// @param reference_normal 曲線が乗る平面の法線ベクトル（正規化不要）
/// @param eps 曲率判定の閾値
/// @param closure_rel_tol 閉性判定の相対許容. 始終点ギャップがこの値×曲線の広がり
///        （サンプル点のAABB対角長）以下なら閉曲線とみなす.
///        0なら`IsClosed()`による厳密判定のみ
/// @return {外包多角形, 内包多角形}の頂点データ
/// @throws std::invalid_argument curve が閉曲線でない場合（始点-終点ギャップが
///         closure_rel_tol×曲線の広がりを超える場合）、自己交差が検出された場合
/// @throws igesio::ComputationError 接線・曲率の計算に失敗した場合
/// @note 外包/内包に依存しない前処理（均等サンプリング、曲線プロパティ計算,
///       初期サンプル点生成) は外包・内包の両多角形を構築時に共有する.
///       ComputeCircumscribedPolygonとComputeInscribedPolygonを個別に
///      呼んだ場合と結果は一致するが、前処理を共有するため計算コストは低減する.
std::pair<numerics::PolygonData, numerics::PolygonData>
ComputeExtremalPolygonPair(
    const ICurve& curve, int n_vert, const Vector3d& reference_normal,
    double eps = 1e-9, double closure_rel_tol = 0.0);

}  // namespace igesio::entities

#endif  // SRC_ENTITIES_CURVES_ALGORITHMS_EXTREMAL_POLYGON_H_
