/**
 * @file graphics/core/camera.h
 * @brief 描画用のカメラクラス
 * @author Yayoi Habami
 * @date 2025-08-03
 * @copyright 2025 Yayoi Habami
 * @note カメラの位置、ターゲット、上方向を定義し、ビュー行列と投影行列を生成する
 */
#ifndef IGESIO_GRAPHICS_CORE_CAMERA_H_
#define IGESIO_GRAPHICS_CORE_CAMERA_H_

#include <utility>

#include "igesio/common/matrix.h"



namespace igesio::graphics {

/// @brief 投影モードの列挙型
enum class ProjectionMode {
    /// @brief 透視投影
    /// @note いわゆる遠近法で、遠くのものが小さく見える投影方法.
    ///       物体の正確な寸法や形状が歪む点に注意が必要.
    kPerspective,
    /// @brief 正射影（等角投影を含む）
    /// @note 遠近感がなく、物体の大きさや形状が正確に表現される投影方法.
    ///       平行な線は投影後も平行に保たれる.
    kOrthographic,
    /// @brief 斜投影 (カバリエ投影やキャビネット投影など)
    /// @note 正射影の一種であり、投影方向が投影面に対して垂直ではない投影方法.
    ///       正面からの形状と奥行方向の関係を同時に表現したい場合に使用される.
    ///       GUI上での操作には向かないが、特定の用途では有用.
    kOblique
};

/// @brief 近接クリッピング面の距離
/// @note デフォルト値は1 (1mm).
///       kDefaultFarPlaneとの比が1000程度となるように設定すること
constexpr float kDefaultNearPlane = 1.0f;
/// @brief 遠方クリッピング面の距離
/// @note デフォルト値は10^3 (1m).
///       kDefaultNearPlaneとの比が1000程度となるように設定すること
constexpr float kDefaultFarPlane = 1000.0f;

/// @brief デフォルトの投影モード
constexpr ProjectionMode kDefaultProjectionMode = ProjectionMode::kPerspective;
/// @brief kPerspectiveの際のカメラの視野角 (FOV) のデフォルト値 [rad]
/// @note 50 degrees
constexpr double kDefaultFov = 50 * kPi / 180.0;
/// @brief kOrthographicの際のビューボリュームのスケールのデフォルト値
constexpr float kDefaultOrthoScale = 5.0f;
/// @brief kObliqueの際のX軸方向のせん断係数 (cot(alpha)) のデフォルト値
constexpr float kDefaultObliqueFactorX = -0.354f;
/// @brief kObliqueの際のY軸方向のせん断係数 (cot(beta)) のデフォルト値
constexpr float kDefaultObliqueFactorY = kDefaultObliqueFactorX;

/// @brief カメラクラス
/// @note カメラの位置、ターゲット、上方向を定義し、ビュー行列と投影行列を生成する
class Camera {
 private:
    /// @brief カメラの位置座標 (eye) (x, y, z)
    igesio::Vector3f position_ = {0.0f, 0.0f, -5.0f};
    /// @brief カメラのターゲット座標 (x, y, z)
    igesio::Vector3f target_ = {0.0f, 0.0f, 0.0f};
    /// @brief カメラの上方向座標 (x, y, z)
    igesio::Vector3f up_ = {0.0f, 1.0f, 0.0f};

    /// @brief 近接クリッピング面の距離
    float near_plane_ = kDefaultNearPlane;
    /// @brief 遠方クリッピング面の距離
    float far_plane_ = kDefaultFarPlane;

    /// @brief 投影モード
    ProjectionMode projection_mode_ = kDefaultProjectionMode;
    /// @brief kPerspectiveの際のカメラの視野角 (FOV) [rad]
    float fov_ = kDefaultFov;
    /// @brief kOrthographicの際のビューボリュームのスケール
    float ortho_scale_ = kDefaultOrthoScale;
    /// @brief kObliqueの際のX軸方向のせん断係数 (cot(alpha))
    float oblique_factor_x_ = kDefaultObliqueFactorX;
    /// @brief kObliqueの際のY軸方向のせん断係数 (cot(beta))
    float oblique_factor_y_ = kDefaultObliqueFactorY;


 public:
    /// @brief デフォルトコンストラクタ
    Camera() = default;

    /// @brief コンストラクタ
    /// @param position カメラの位置座標 (x, y, z)
    /// @param target カメラのターゲット座標 (x, y, z)
    /// @param up カメラの上方向座標 (x, y, z)
    Camera(const igesio::Vector3f&, const igesio::Vector3f&,
           const igesio::Vector3f&);



    /// @brief カメラの位置を取得する
    /// @return カメラの位置座標 (x, y, z)
    const igesio::Vector3f& GetPosition() const { return position_; }

    /// @brief カメラのターゲット位置
    /// @return カメラのターゲット座標 (x, y, z)
    const igesio::Vector3f& GetTarget() const { return target_; }

    /// @brief カメラの視野角を取得する
    /// @return カメラの視野角 [rad]
    float GetFov() const { return fov_; }

    /// @brief 近接/遠方クリッピング面の距離を取得する
    /// @return 1つ目が近接クリッピング面、2つ目が遠方クリッピング面の距離
    std::pair<float, float> GetClippingPlanes() const {
        return {near_plane_, far_plane_};
    }

    /// @brief カメラの位置を設定する
    /// @param position カメラの位置座標 (x, y, z)
    void SetPosition(const igesio::Vector3f&);

    /// @brief カメラのターゲット位置を設定する
    /// @param target カメラのターゲット座標 (x, y, z)
    void SetTarget(const igesio::Vector3f&);

    /// @brief カメラの視野角を設定する
    /// @param fov カメラの視野角 [rad]
    /// @throw std::invalid_argument fovが0以下またはπ以上の場合
    void SetFov(const float);

    /// @brief 近接/遠方クリッピング面の距離を設定する
    /// @param near_plane 近接クリッピング面の距離
    /// @param far_plane 遠方クリッピング面の距離
    /// @throw std::invalid_argument near_planeが0以下、
    ///        far_planeがnear_plane以下の場合
    /// @note Zファイティングを避けるため、far_plane / near_plane
    ///       の比は1000程度（例: near=1.0, far=1000.0）を推奨
    /// @note 実際の描画の際は、近平面、遠平面、および上下左右の面で
    ///       囲まれた立体 (視錐台) の内側のみが描画される. 設定する
    ///       値によっては、近い物体が見えなくなったり、遠い物体が
    ///       見えなくなったりする (カリング) ので注意.
    void SetClippingPlanes(const float, const float);

    /// @brief 投影モードを設定する
    /// @param mode 設定する投影モード
    void SetProjectionMode(const ProjectionMode);

    /// @brief 投影モードを取得する
    /// @return 現在の投影モード
    ProjectionMode GetProjectionMode() const;

    /// @brief 斜投影の係数を設定する
    /// @param factor_x X軸方向のせん断係数
    /// @param factor_y Y軸方向のせん断係数
    void SetObliqueFactors(const float, const float);

    /// @brief すべてのメンバ変数をリセットする
    void Reset();



    /**
     * 画面操作関連
     */

    /// @brief カメラをターゲットの周りで回転させる
    /// @param yaw_angle ヨー角 [rad]
    /// @param pitch_angle ピッチ角 [rad]
    void Rotate(const float, const float);

    /// @brief カメラをパン (平行移動) させる
    /// @param dx 水平方向の移動量
    /// @param dy 垂直方向の移動量
    void Pan(const float, const float);

    /// @brief カメラをズームする
    /// @param zoom_factor ズーム係数
    ///        (1.0fで変化なし, <1.0fでズームイン, >1.0fでズームアウト)
    /// @note ターゲットとの距離は0.1fを下限とする
    void Zoom(const float);



    /**
     * 描画用の行列の計算
     */

    /// @brief ビュー行列を取得する
    /// @return ビュー行列 (LookAt行列)
    Matrix4f GetViewMatrix() const;

    /// @brief 投影行列を取得する
    /// @param aspect_ratio アスペクト比 (width / height)
    /// @return 投影行列 (Perspective行列)
    Matrix4f GetProjectionMatrix(const float) const;

 private:
    /// @brief kPerspectiveの際の投影行列を計算する
    /// @param aspect_ratio アスペクト比 (width / height)
    /// @return 投影行列 (Perspective行列)
    Matrix4f GetPerspectiveProjectionMatrix(const float) const;

    /// @brief kOrthographicの際の投影行列を計算する
    /// @param aspect_ratio アスペクト比 (width / height)
    /// @return 投影行列 (Orthographic行列)
    Matrix4f GetOrthographicProjectionMatrix(const float) const;

    /// @brief kObliqueの際の投影行列を計算する
    /// @param aspect_ratio アスペクト比 (width / height)
    /// @return 投影行列 (Oblique行列)
    Matrix4f GetObliqueProjectionMatrix(const float) const;
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CORE_CAMERA_H_
