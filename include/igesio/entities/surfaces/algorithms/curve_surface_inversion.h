/**
 * @file entities/surfaces/algorithms/curve_surface_inversion.h
 * @brief モデル空間曲線を曲面のパラメータ空間へ逆射影する
 * @author Yayoi Habami
 * @date 2026-05-31
 * @copyright 2026 Yayoi Habami
 *
 * @details
 * Type 142 (Curve on a Parametric Surface) や Type 141/143 (Boundary /
 * Bounded Surface) において、ベース曲線 B(t)=(u(t),v(t)) が省略され、
 * モデル空間曲線C(r)と曲面S(u,v)のみが与えられる場合がある
 * (CATIA等)。本モジュールは B = S^{-1} ∘ C を最近点射影により再構築する。
 *
 * ### アルゴリズム概要
 * 1. C(r)を折れ線近似して点列 P_0..P_m を得る (`ComputeApproximatePolygon`)。
 * 2. 各P_iをS上へ最近点射影 (ガウス・ニュートン法) して (u_i, v_i) を得る。
 *    直前点の解をウォームスタートに用い、先頭点のみ格子探索で初期値を決める。
 * 3. seam (閉方向のラップ) やpole (退化) で連続性が切れる箇所を検出し、
 *    `split_at_discontinuities` が真なら複数の連続弧 (collection) に分割する。
 *
 * `InvertPointOntoSurface` (単点射影) と`InvertCurveOntoSurface` (曲線射影) は
 * いずれも`ISurface`のTryGetDerivatives(u,v,1)`のみに依存する。
 *
 * @note Type 142は単一の連続弧を仮定するため`split_at_discontinuities=false`
 *       (既定) で1要素を取り出して用いる。Type 141/143 (TYPE=1) では
 *       1本のモデル曲線がseam/poleにより複数のパラメータ空間弧に分断され得るため
 *       (規格 4.31 D11/D12)、`split_at_discontinuities=true`を渡して
 *       戻り値のcollectionをそのまま用いる。
 */
#ifndef IGESIO_ENTITIES_SURFACES_ALGORITHMS_CURVE_SURFACE_INVERSION_H_
#define IGESIO_ENTITIES_SURFACES_ALGORITHMS_CURVE_SURFACE_INVERSION_H_

#include <array>
#include <optional>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/entities/interfaces/i_curve.h"
#include "igesio/entities/interfaces/i_surface.h"



namespace igesio::entities {

/// @brief 逆射影で得たパラメータ空間の連続した1弧
/// @note Type 141/143では1本のモデル曲線が複数弧に分断され得るため、
///       射影結果は本構造体の列 (collection) として表現する。
struct ParamSpaceArc {
    /// @brief パラメータ空間のサンプル列。各要素は (u, v, 0) (z=0平面)
    std::vector<Vector3d> uv_points;
    /// @brief この弧が対応するモデル空間曲線Cのパラメータ範囲 [r_start, r_end]
    std::array<double, 2> source_range;
};

/// @brief 曲線→曲面 逆射影の探索制御パラメータ
struct CurveInversionParams {
    /// @brief 先頭点の初期値探索に用いるu方向格子サンプル数
    int grid_u = 20;
    /// @brief 先頭点の初期値探索に用いるv方向格子サンプル数
    int grid_v = 20;
    /// @brief ガウス・ニュートン法の最大反復回数
    int max_iter = 50;
    /// @brief 収束判定の相対許容誤差 (曲面の代表寸法Lに対する残差 ‖r‖/L の閾値)
    double convergence_tol = 1e-9;
    /// @brief 「S上にある」とみなす受理許容誤差 (Lに対する相対値)。
    ///        反復が収束しきらなくても ‖r‖/L がこの値未満なら解として採用する
    double accept_tol = 1e-4;
    /// @brief Cを折れ線近似する際の許容距離
    double discretization_tol = 1e-4;
    /// @brief seam/poleで弧を分割するか。
    ///        false: 常に1弧 (Type 142)。true: 複数弧 (Type 141/143)
    bool split_at_discontinuities = false;
};

/// @brief 点Pを曲面S上へ最近点射影し、パラメータ (u, v) を求める
///
/// 残差 r(u,v) = S(u,v) - P を最小化するガウス・ニュートン反復で解く。
/// 近似ヘッセ行列には第一基本形式 [[E,F],[F,G]] を用いる (1階偏導関数のみ使用)。
/// PがS上にある場合 (残差→0) は2次収束する。
///
/// @param surface    射影先の曲面
/// @param model_point 射影する点P (モデル空間)
/// @param init_uv    初期パラメータ (u, v)。ウォームスタートに用いる
/// @param params     探索制御パラメータ
/// @return 収束した (u, v)。穴領域・退化・非収束で失敗した場合は `std::nullopt`
std::optional<std::array<double, 2>> InvertPointOntoSurface(
        const ISurface& surface, const Vector3d& model_point,
        const std::array<double, 2>& init_uv,
        const CurveInversionParams& params = {});

/// @brief モデル空間曲線Cを曲面Sのパラメータ空間へ逆射影する
///
/// Cを折れ線近似し、各点を`InvertPointOntoSurface`で (u,v) 化して連続弧を構成する。
/// `params.split_at_discontinuities` が真の場合、seam (閉方向のラップ跳び) や
/// pole (射影失敗) で弧を分割し、複数の`ParamSpaceArc`を返す。
///
/// @param surface     射影先の曲面S
/// @param model_curve 逆射影するモデル空間曲線C
/// @param params      探索制御パラメータ
/// @return パラメータ空間の連続弧の列。射影に全く成功しなかった場合は空。
///         `split_at_discontinuities=false`のときは最大1要素 (Type 142用)
/// @note CとSはいずれもモデル空間で評価する (前方変換と対称)。両者が同一の
///       モデル空間に存在すること (well-formedなIGES) を前提とする。
std::vector<ParamSpaceArc> InvertCurveOntoSurface(
        const ISurface& surface, const ICurve& model_curve,
        const CurveInversionParams& params = {});

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_SURFACES_ALGORITHMS_CURVE_SURFACE_INVERSION_H_
