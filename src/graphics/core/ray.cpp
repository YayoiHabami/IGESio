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

}  // namespace igesio::graphics
