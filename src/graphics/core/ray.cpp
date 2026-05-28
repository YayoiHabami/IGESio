/**
 * @file graphics/core/ray.cpp
 * @brief スクリーン座標からワールド空間のレイを生成する機能を実装する
 * @author Yayoi Habami
 * @date 2026-05-27
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/graphics/core/ray.h"



namespace igesio::graphics {

namespace {

/// @brief 正規化デバイス座標(NDC)をワールド座標へ逆射影する
/// @param inv_vp (投影行列P)*(ビュー行列V)の逆行列
/// @param ndc_x NDCのx成分
/// @param ndc_y NDCのy成分
/// @param ndc_z NDCのz成分（nearクリップ面:-1, farクリップ面:+1）
/// @return ワールド座標（w除算済み）
/// @note Eigen非使用ビルドでも動作するよう、Vector4dの成分単位でw除算する
igesio::Vector3d UnprojectNdc(const igesio::Matrix4d& inv_vp,
                              double ndc_x, double ndc_y, double ndc_z) {
    const igesio::Vector4d clip(ndc_x, ndc_y, ndc_z, 1.0);
    const igesio::Vector4d world = inv_vp * clip;
    return igesio::Vector3d(world.x() / world.w(),
                            world.y() / world.w(),
                            world.z() / world.w());
}

}  // namespace

Ray GetRayFromScreen(const Camera& camera, int w, int h,
                     double x, double y) {
    if (w <= 0 || h <= 0) {
        Ray ray;
        ray.origin = igesio::Vector3d::Zero();
        ray.direction = igesio::Vector3d::Zero();
        return ray;
    }

    const float aspect = static_cast<float>(w) / static_cast<float>(h);
    // P・Vをdoubleへ昇格してから逆行列を取る。far/near比が1000程度の
    // 投影行列はfloatでは数値的に悪条件となりやすいため
    const igesio::Matrix4d v = camera.GetViewMatrix().cast<double>();
    const igesio::Matrix4d p = camera.GetProjectionMatrix(aspect).cast<double>();
    const igesio::Matrix4d inv_vp = (p * v).inverse();

    const double ndc_x = 2.0 * x / static_cast<double>(w) - 1.0;
    const double ndc_y = 1.0 - 2.0 * y / static_cast<double>(h);

    const igesio::Vector3d near_world = UnprojectNdc(inv_vp, ndc_x, ndc_y, -1.0);
    const igesio::Vector3d far_world = UnprojectNdc(inv_vp, ndc_x, ndc_y, 1.0);

    Ray ray;
    ray.origin = near_world;
    ray.direction = (far_world - near_world).normalized();
    return ray;
}

double PixelToWorldSize(const Camera& camera, int w, int h,
                        double x, double y, double depth, double pixel_tol) {
    if (w <= 0 || h <= 0) return 0.0;

    const Ray r1 = GetRayFromScreen(camera, w, h, x, y);
    const Ray r2 = GetRayFromScreen(camera, w, h, x + pixel_tol, y);

    const igesio::Vector3d p1 = r1.origin + r1.direction * depth;
    const igesio::Vector3d p2 = r2.origin + r2.direction * depth;
    return (p2 - p1).norm();
}

igesio::Vector4d WorldToClip(
        const igesio::Matrix4d& view_proj, const igesio::Vector3d& world) {
    return view_proj * igesio::Vector4d(world.x(), world.y(), world.z(), 1.0);
}

std::optional<igesio::Vector3d> WorldToScreen(
        const igesio::Matrix4d& view_proj, int w, int h,
        const igesio::Vector3d& world) {
    if (w <= 0 || h <= 0) return std::nullopt;

    const igesio::Vector4d clip = WorldToClip(view_proj, world);

    // clip.w<=0は透視投影でカメラ平面以遠の点に生じる射影特異点
    // (w除算で符号反転・破綻する)。正射影/斜投影ではwが常に1のため発火しない
    if (clip.w() <= 0.0) return std::nullopt;

    const double inv_w = 1.0 / clip.w();
    const double ndc_x = clip.x() * inv_w;
    const double ndc_y = clip.y() * inv_w;
    const double ndc_z = clip.z() * inv_w;

    const double x = (ndc_x + 1.0) * 0.5 * static_cast<double>(w);
    const double y = (1.0 - ndc_y) * 0.5 * static_cast<double>(h);
    return igesio::Vector3d(x, y, ndc_z);
}

std::optional<igesio::Vector3d> WorldToScreen(
        const Camera& camera, int w, int h, const igesio::Vector3d& world) {
    if (w <= 0 || h <= 0) return std::nullopt;

    const float aspect = static_cast<float>(w) / static_cast<float>(h);
    // P・Vをdoubleへ昇格してから合成する (GetRayFromScreenと同じ理由)
    const igesio::Matrix4d v = camera.GetViewMatrix().cast<double>();
    const igesio::Matrix4d p = camera.GetProjectionMatrix(aspect).cast<double>();
    return WorldToScreen(p * v, w, h, world);
}

}  // namespace igesio::graphics
