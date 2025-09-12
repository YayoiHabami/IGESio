/**
 * @file graphics/core/camera.cpp
 * @brief 描画用のカメラクラス
 * @author Yayoi Habami
 * @date 2025-08-03
 * @copyright 2025 Yayoi Habami
 * @note カメラの位置、ターゲット、上方向を定義し、ビュー行列と投影行列を生成する
 */
#include "igesio/graphics/core/camera.h"

#include <algorithm>

namespace {

namespace i_graphic = igesio::graphics;
using Camera = i_graphic::Camera;

}  // namespace



Camera::Camera(const igesio::Vector3f& position,
               const igesio::Vector3f& target,
               const igesio::Vector3f& up)
        : position_(position), target_(target), up_(up.normalized()) {}

void Camera::SetPosition(const igesio::Vector3f& position) {
    position_ = position;
}

void Camera::SetTarget(const igesio::Vector3f& target) {
    target_ = target;
}

void Camera::SetFov(const float fov) {
    if (fov <= 0.0f || fov >= kPi) {
        throw std::invalid_argument("FOV must be in (0, π) radians");
    }
    fov_ = fov;
}

/// @brief 投影モードを設定する
void Camera::SetProjectionMode(const ProjectionMode mode) {
    projection_mode_ = mode;
}

/// @brief 投影モードを取得する
i_graphic::ProjectionMode Camera::GetProjectionMode() const {
    return projection_mode_;
}

void Camera::SetObliqueFactors(const float factor_x, const float factor_y) {
    oblique_factor_x_ = factor_x;
    oblique_factor_y_ = factor_y;
}

void Camera::Reset() {
    position_ = {0.0f, 0.0f, 5.0f};
    target_ = {0.0f, 0.0f, 0.0f};
    up_ = {0.0f, 1.0f, 0.0f};

    near_plane_ = kDefaultNearPlane;
    far_plane_ = kDefaultFarPlane;

    projection_mode_ = kDefaultProjectionMode;
    fov_ = kDefaultFov;
    ortho_scale_ = kDefaultOrthoScale;
    oblique_factor_x_ = kDefaultObliqueFactorX;
    oblique_factor_y_ = kDefaultObliqueFactorY;
}



/**
 * 画面操作関連
 */

void Camera::Rotate(const float yaw_angle, const float pitch_angle) {
    igesio::Vector3f view_dir = position_ - target_;

    // 現在のカメラの上方向を中心にヨー回転
    auto yaw = igesio::AngleAxisf(yaw_angle, up_);

    // 現在のカメラの右方向を計算
    auto right = up_.cross(view_dir).normalized();
    auto pitch = igesio::AngleAxisf(pitch_angle, right);

    // 回転を適用
    auto rotation = yaw * pitch;
    position_ = rotation * view_dir + target_;

    // カメラの上方向ベクトルも回転させる
    up_ = (rotation * up_).normalized();
}

void Camera::Pan(const float dx, const float dy) {
    auto right = up_.cross(position_ - target_).normalized();
    auto pan_up = right.cross((position_ - target_).normalized());

    // ターゲットからのカメラの距離に比例して感度を調整
    // 遠い場合は速くパン、近い場合は遅くパン
    auto distance = (position_ - target_).norm();
    auto translation = -right * dx * distance - pan_up * dy * distance;
    position_ += translation;
    target_ += translation;
}

void Camera::Zoom(const float zoom_factor) {
    if (projection_mode_ == ProjectionMode::kPerspective) {
        // 透視投影のズーム: カメラとターゲットの距離を変更
        // ターゲットとの距離が0.1fを下回らないようにする
        auto new_distance = (position_ - target_).norm() * zoom_factor;
        new_distance = std::max(new_distance, 0.1f);
        position_ = target_ + (position_ - target_).normalized() * new_distance;
    } else {  // Orthographic or Oblique
        // 平行投影・斜投影のズーム: ビューボリュームのスケールを変更
        ortho_scale_ *= zoom_factor;
        ortho_scale_ = std::max(ortho_scale_, 0.1f);
    }
}



/**
 * 描画用の行列の計算
 */

igesio::Matrix4f Camera::GetViewMatrix() const {
    // LookAt行列を生成
    igesio::Vector3f f = (target_ - position_).normalized();
    igesio::Vector3f s = f.cross(up_).normalized();
    igesio::Vector3f u = s.cross(f);

    igesio::Matrix4f res = {{ s(0),  s(1),  s(2), -s.dot(position_)},
                            { u(0),  u(1),  u(2), -u.dot(position_)},
                            {-f(0), -f(1), -f(2),  f.dot(position_)},
                            {    0,     0,     0,                 1}};
    return res;
}

igesio::Matrix4f
Camera::GetProjectionMatrix(const float aspect_ratio) const {
    switch (projection_mode_) {
        case ProjectionMode::kPerspective:
            return GetPerspectiveProjectionMatrix(aspect_ratio);
        case ProjectionMode::kOrthographic:
            return GetOrthographicProjectionMatrix(aspect_ratio);
        case ProjectionMode::kOblique:
            return GetObliqueProjectionMatrix(aspect_ratio);
        default:
            // 未知の投影モードの場合は単位行列を返す
            return igesio::Matrix4f::Identity();
    }
}

igesio::Matrix4f
Camera::GetPerspectiveProjectionMatrix(const float aspect_ratio) const {
    // 透視投影行列を計算
    float tan_half_fov_y = std::tan(fov_ * 0.5f);

    igesio::Matrix4f res = igesio::Matrix4f::Zero();
    res(0, 0) = 1.0f / (aspect_ratio * tan_half_fov_y);
    res(1, 1) = 1.0f / (tan_half_fov_y);
    res(2, 2) = -(far_plane_ + near_plane_) / (far_plane_ - near_plane_);
    res(2, 3) = -(2.0f * far_plane_ * near_plane_) / (far_plane_ - near_plane_);
    res(3, 2) = -1.0f;
    return res;
}

igesio::Matrix4f
Camera::GetOrthographicProjectionMatrix(const float aspect_ratio) const {
    // 平行投影行列を計算
    const float right = ortho_scale_ * aspect_ratio;
    const float left = -right;
    const float top = ortho_scale_;
    const float bottom = -top;

    igesio::Matrix4f res = igesio::Matrix4f::Identity();
    res(0, 0) = 2.0f / (right - left);
    res(1, 1) = 2.0f / (top - bottom);
    res(2, 2) = -2.0f / (far_plane_ - near_plane_);
    res(0, 3) = -(right + left) / (right - left);
    res(1, 3) = -(top + bottom) / (top - bottom);
    res(2, 3) = -(far_plane_ + near_plane_) / (far_plane_ - near_plane_);
    return res;
}

igesio::Matrix4f
Camera::GetObliqueProjectionMatrix(const float aspect_ratio) const {
    // 平行投影(Orthographic)行列を計算
    auto ortho_matrix = GetOrthographicProjectionMatrix(aspect_ratio);

    // せん断(Shear)行列を生成
    igesio::Matrix4f shear_matrix = igesio::Matrix4f::Identity();
    shear_matrix(0, 2) = oblique_factor_x_;  // X方向のせん断
    shear_matrix(1, 2) = oblique_factor_y_;  // Y方向のせん断

    // 平行投影行列にせん断行列を乗算して、斜投影行列を作成
    return shear_matrix * ortho_matrix;
}
