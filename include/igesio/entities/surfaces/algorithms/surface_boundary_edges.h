/**
 * @file entities/surfaces/algorithms/surface_boundary_edges.h
 * @brief 曲面の境界エッジ(折れ線群)の生成
 * @author Yayoi Habami
 * @date 2026-06-03
 * @copyright 2026 Yayoi Habami
 *
 * @details
 * 曲面の境界を、描画 (ワイヤフレーム/エッジ表示) 用の折れ線群として生成する。
 * GLに依存せず、`ISurface`/`IRestrictedSurface` のAPIのみを使用する。
 *
 * - 制限付き曲面 (Type 144等): 内側境界(穴)と、明示された外側境界、または
 *   パラメータ矩形の外周を生成する。境界曲線はUV空間のB(t)として与えられ、
 *   基底曲面Sで評価したS(B(t))を採用する (描画メッシュと同一空間に乗るため)。
 * - 一般曲面: パラメータ矩形の4本のアイソパラ辺を生成する。閉じた方向の継ぎ目辺は
 *   既定で除外する (人工的な継ぎ目線を描かないため)。
 */
#ifndef IGESIO_ENTITIES_SURFACES_ALGORITHMS_SURFACE_BOUNDARY_EDGES_H_
#define IGESIO_ENTITIES_SURFACES_ALGORITHMS_SURFACE_BOUNDARY_EDGES_H_

#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/entities/interfaces/i_curve.h"
#include "igesio/entities/interfaces/i_surface.h"
#include "igesio/entities/interfaces/i_restricted_surface.h"



namespace igesio::entities {

/// @brief 曲面の境界エッジ (折れ線群)
struct SurfaceBoundaryEdges {
    /// @brief 境界ループ群。各ループは定義空間 (M_base適用済み) の連続点列。
    /// @note 1つの境界でも、曲面/境界曲線が評価不能となる点で分割され、複数の
    ///       折れ線になり得る (欠落点をまたいで偽の線分を作らないため)。
    std::vector<std::vector<Vector3d>> loops;
};

/// @brief 境界エッジ生成の制御パラメータ
struct SurfaceBoundaryEdgeParams {
    /// @brief 1ループ (1本の境界曲線/アイソ辺) あたりの分割数
    int divisions = 64;
    /// @brief 閉じた方向の継ぎ目辺を含めるか
    /// @note falseの場合、IsUClosed()/IsVClosed()がtrueの方向の両端アイソ辺を除外する
    bool include_seams = false;
};

/// @brief 曲線の等間隔サンプルに、開区間内の角点パラメータを併合した列を返す
/// @param curve サンプリング対象の曲線 (角点取得にGetCornerParamsを使用)
/// @param t0 サンプリング範囲の開始値 (有限値; 無限端は呼び出し側でクランプ済み)
/// @param t1 サンプリング範囲の終了値 (有限値; t1 > t0 であること)
/// @param divisions 等間隔分割数 (1以上)
/// @return [t0, t1] の等間隔点と (t0, t1) 内の角点を併合し、昇順ソート・近接重複
///         除去したパラメータ列。t1 <= t0 または divisions < 1 のときは空
/// @note 角点 (GetCornerParams) を評価点に含めることで、折れ線が曲線の角を
///       丸めず正確に通過する。角点を持たない曲線では等間隔点のみを返す
std::vector<double> BuildCornerAwareSampleParams(
        const ICurve& curve, double t0, double t1, int divisions);

/// @brief 制限付き曲面 (Type 144等) の境界エッジを生成する
/// @param surface 制限付き曲面
/// @param params 生成パラメータ
/// @return 外側境界 (明示 or パラメータ矩形) と内側境界 (穴) の折れ線群
/// @note 外側境界は IsOuterBoundaryOfD()==true (制限なし) の場合は基底曲面の
///       パラメータ矩形 (ComputeParametricSurfaceEdges) を、falseの場合は
///       明示されたUV外側境界をS(B(t))で評価する。内側境界は常にS(B(t))。
/// @note 基底曲面が未解決、または境界曲線が評価不能の場合、その境界はスキップされる。
SurfaceBoundaryEdges ComputeRestrictedSurfaceEdges(
        const IRestrictedSurface&, const SurfaceBoundaryEdgeParams& = {});

/// @brief 一般曲面のパラメータ矩形の境界エッジ (4本のアイソ辺) を生成する
/// @param surface 曲面
/// @param params 生成パラメータ
/// @return パラメータ矩形の周囲を構成するアイソパラ辺の折れ線群
/// @note u=u0,u=u1,v=v0,v=v1 の各アイソ辺をTryGetPointAtで評価する。
///       include_seams=falseかつ IsUClosed()/IsVClosed() の方向は両端辺を除外する。
/// @note 無限のパラメータ範囲は有限値にクランプされる。
SurfaceBoundaryEdges ComputeParametricSurfaceEdges(
        const ISurface&, const SurfaceBoundaryEdgeParams& = {});

/// @brief スイープ系曲面の折り目に沿う内部稜線エッジを生成する
/// @param surface 曲面
/// @param params 生成パラメータ
/// @return 各u方向折り目u_cにおけるv方向アイソ曲線 S(u_c, v) の折れ線群
/// @note ISurface::GetUCreaseParameters() が返す角パラメータのうち、パラメータ
///       矩形の内部 (u0 < u_c < u1) のもののみを対象とする (端は境界辺と重複)。
///       角を持たない曲面 (既定で空) では空を返す。境界辺とは別概念のため、
///       描画側で境界エッジへ連結して用いる。
SurfaceBoundaryEdges ComputeSurfaceCreaseEdges(
        const ISurface&, const SurfaceBoundaryEdgeParams& = {});

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_SURFACES_ALGORITHMS_SURFACE_BOUNDARY_EDGES_H_
