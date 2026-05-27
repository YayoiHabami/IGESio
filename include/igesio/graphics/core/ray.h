/**
 * @file graphics/core/ray.h
 * @brief スクリーン座標からワールド空間のレイを生成する機能を提供する
 * @author Yayoi Habami
 * @date 2026-05-27
 * @copyright 2026 Yayoi Habami
 *
 * @details
 * 画面クリック位置を通るワールド空間のレイ（半直線）を生成し、
 * エンティティとのピッキング判定の起点とする。あわせて、曲線ヒット判定の
 * ピクセル許容量を、指定深度におけるワールド距離へ換算する機能を提供する。
 *
 * いずれの関数もkOblique投影モードでは正しい結果を返さない（投影方向が
 * 投影面に垂直でないため、逆射影したレイが本来の視線方向と一致しない）。
 */
#ifndef IGESIO_GRAPHICS_CORE_RAY_H_
#define IGESIO_GRAPHICS_CORE_RAY_H_

#include <optional>

#include "igesio/numerics/matrix.h"
#include "igesio/graphics/core/camera.h"



namespace igesio::graphics {

/// @brief ワールド座標系のレイ（半直線）
struct Ray {
    /// @brief 始点（nearクリップ面上の点）
    Vector3d origin;
    /// @brief 正規化済みの方向ベクトル
    Vector3d direction;
};

/// @brief レイとエンティティの交差結果（エンティティ層に依存しない）
struct RayHit {
    /// @brief 3D交点座標
    Vector3d position;
    /// @brief ray.originからの距離
    /// @note ray.directionが単位ベクトルのため、線パラメータtに等しい
    double distance;
};

/// @brief IEntityGraphics::Intersectの探索制御パラメータ
struct RayIntersectionParams {
    /// @brief 曲面のu方向の初期格子サンプル数
    int surface_u_samples = 10;
    /// @brief 曲面のv方向の初期格子サンプル数
    int surface_v_samples = 10;
    /// @brief 曲線の初期格子サンプル数
    int curve_samples = 20;
    /// @brief ニュートン法の収束判定の許容誤差
    double convergence_tol = 1e-9;
    /// @brief 重複解の除去に使用する3D空間距離の許容誤差
    double dedup_tol = 1e-6;
    /// @brief 曲線ヒット判定のピクセル許容量 [px]
    /// @note PickEntitiesが深度に応じてワールド距離へ換算し、
    ///       curve_hit_toleranceに設定する
    double curve_hit_pixels = 5.0;
    /// @brief 曲線ヒット判定距離 [モデル単位]
    /// @note Intersectが消費する。ピッキング経路ではPickEntitiesが
    ///       エンティティ毎に上書きするため、手設定は基本不要
    double curve_hit_tolerance = 1.0;
};

/// @brief スクリーン空間の矩形領域 [px]（左上原点・GLFW準拠）
struct ScreenRect {
    /// @brief x方向の最小値 [px]
    double x_min = 0.0;
    /// @brief y方向の最小値 [px]
    double y_min = 0.0;
    /// @brief x方向の最大値 [px]
    double x_max = 0.0;
    /// @brief y方向の最大値 [px]
    double y_max = 0.0;
};

/// @brief 範囲選択用のサンプリング制御パラメータ
struct SelectionSampleParams {
    /// @brief 曲線の折れ線分割数
    int curve_samples = 32;
    /// @brief 曲面境界/グリッドのu方向サンプル数
    int surface_u_samples = 16;
    /// @brief 曲面境界/グリッドのv方向サンプル数
    int surface_v_samples = 16;
};

/// @brief スクリーン座標からワールド空間のレイを生成する
/// @param camera 現在のカメラ
/// @param w ビューポートの幅 [px]
/// @param h ビューポートの高さ [px]
/// @param x スクリーンx座標 [px]（左上原点・GLFW準拠）
/// @param y スクリーンy座標 [px]（左上原点・GLFW準拠）
/// @return ワールド空間のレイ（originはnearクリップ面上の点、
///         directionは正規化済み）
/// @note wまたはhが0以下の場合、origin/directionともにゼロベクトルの
///       レイを返す
/// @note kOblique投影モードでは正しいレイが生成されない (TODO: 未対応)
Ray GetRayFromScreen(const Camera& camera, int w, int h,
                     double x, double y);

/// @brief ピクセル許容量を、指定深度におけるワールド距離へ換算する
/// @param camera 現在のカメラ
/// @param w ビューポートの幅 [px]
/// @param h ビューポートの高さ [px]
/// @param x 基準スクリーンx座標 [px]
/// @param y 基準スクリーンy座標 [px]
/// @param depth ray.originからの距離 [モデル単位]
/// @param pixel_tol ピクセル許容量 [px]
/// @return ワールド空間での許容距離 [モデル単位]
/// @note (x,y)と(x+pixel_tol,y)の2本のレイを同じdepthで評価し、
///       その世界座標間距離を返す。透視投影ではdepthに比例し、
///       正射影ではdepth非依存となる
/// @note wまたはhが0以下の場合は0を返す
/// @note kOblique投影モードでは正しい値が返らない (TODO: 未対応)
double PixelToWorldSize(const Camera& camera, int w, int h,
                        double x, double y, double depth, double pixel_tol);

/// @brief ワールド座標をスクリーン座標へ順射影する（GetRayFromScreenの双対）
/// @param camera 現在のカメラ
/// @param w ビューポートの幅 [px]
/// @param h ビューポートの高さ [px]
/// @param world ワールド座標
/// @return {x, y, ndc_z}（x, yはピクセル座標・左上原点、ndc_zは奥行き -1..1）.
///         w<=0 / h<=0、または透視投影でカメラ平面より手前（clip.w<=0）の場合は
///         std::nullopt
/// @note 内部でP*Vをdoubleへ昇格して計算する。多数の点を射影する場合は、
///       VP行列版オーバーロードでP*Vを使い回すこと
/// @note 可視 (near/far) 判定はndc_zで行う (ndc_z<-1: near面より手前,
///       >1: far面より奥). 正射影/斜投影では特異点が無いため、near面より手前の点も
///       射影値を返す
/// @note kOblique投影モードでは正しい結果を返さない (TODO: 未対応)
std::optional<Vector3d> WorldToScreen(const Camera& camera, int w, int h,
                                      const Vector3d& world);

/// @brief ワールド座標をスクリーン座標へ順射影する（VP行列を直接受ける版）
/// @param view_proj (投影行列P)*(ビュー行列V)（double）
/// @param w ビューポートの幅 [px]
/// @param h ビューポートの高さ [px]
/// @param world ワールド座標
/// @return カメラ版と同じ. 多数の点を射影する際にP*Vの再計算を避けるための版
/// @note 戻り値の意味・特異点の扱いはカメラ版と同一
std::optional<Vector3d> WorldToScreen(const Matrix4d& view_proj, int w, int h,
                                      const Vector3d& world);

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CORE_RAY_H_
