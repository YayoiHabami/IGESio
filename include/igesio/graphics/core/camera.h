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

#include <optional>
#include <utility>

#include "igesio/numerics/geometric/bounding_box.h"
#include "igesio/numerics/core/matrix.h"



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

/// @brief 標準軸ビューの種別
/// @note カメラの視線方向と上方向を、各座標軸に沿った定型の向きへ設定する際に使用する.
enum class StandardView {
    /// @brief 正面 (+Z側から-Z方向を見る. 上方向は+Y)
    kFront,
    /// @brief 背面 (-Z側から+Z方向を見る. 上方向は+Y)
    kBack,
    /// @brief 上面 (+Y側から-Y方向を見下ろす. 上方向は-Z)
    kTop,
    /// @brief 底面 (-Y側から+Y方向を見上げる. 上方向は+Z)
    kBottom,
    /// @brief 右面 (+X側から-X方向を見る. 上方向は+Y)
    kRight,
    /// @brief 左面 (-X側から+X方向を見る. 上方向は+Y)
    kLeft,
    /// @brief 等角 (+X+Y+Z方向から見る. 上方向は+Y)
    kIso
};

/// @brief 近接クリッピング面の距離 (自動クリッピング無効時のフォールバック値)
/// @note デフォルト値は1 (1mm).
///       kDefaultFarPlaneとの比が1000程度となるように設定すること
constexpr float kDefaultNearPlane = 1.0f;
/// @brief 遠方クリッピング面の距離 (自動クリッピング無効時のフォールバック値)
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

/// @brief FitToBoundingBox時の余白係数のデフォルト値
/// @note 1.0で対象に密着、>1.0で周囲に余白を設ける
constexpr float kDefaultFitMargin = 1.1f;

/// @brief 自動クリッピング時に外接球半径へ掛ける安全マージン
/// @note 面がクリップ面上へ正確に乗ることによる描画抜けを防ぐ
constexpr float kAutoClipRadiusMargin = 1.05f;
/// @brief 自動クリッピング時のnear/far比の下限 (near >= far * この値)
/// @note カメラが外接球内へ入った際の、手前の見切れと深度精度の
///       トレードオフを決める. 透視投影の深度分解能はnearにほぼ反比例して
///       劣化するため、小さくしすぎると奥のエッジ線が面を透過して見える
constexpr float kAutoClipMinNearRatio = 1e-3f;

/// @brief バウンディングボックスの外接球(中心と半径)を計算する
/// @param bbox 対象のバウンディングボックス
/// @return (中心, 半径)のペア. 有限頂点を持たない場合はstd::nullopt
std::optional<std::pair<igesio::Vector3f, float>>
ComputeBoundingSphere(const numerics::BoundingBox&);

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

    /// @brief 自動クリッピングが有効か
    /// @note 有効な場合、near/farはシーン外接球と現在のカメラ位置から
    ///       毎回導出される (near_plane_/far_plane_は使用されない)
    bool auto_clip_enabled_ = false;
    /// @brief 自動クリッピング用のシーン外接球の中心 (ワールド座標系)
    igesio::Vector3f auto_clip_center_ = {0.0f, 0.0f, 0.0f};
    /// @brief 自動クリッピング用のシーン外接球の半径
    float auto_clip_radius_ = 0.0f;


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

    /// @brief カメラの上方向を取得する
    /// @return カメラの上方向ベクトル (x, y, z)
    const igesio::Vector3f& GetUp() const { return up_; }

    /// @brief カメラの視野角を取得する
    /// @return カメラの視野角 [rad]
    float GetFov() const { return fov_; }

    /// @brief 近接/遠方クリッピング面の距離を取得する
    /// @return 1つ目が近接クリッピング面、2つ目が遠方クリッピング面の距離
    /// @note 自動クリッピングが有効な場合、シーン外接球と現在のカメラ位置から
    ///       導出した値を返す. 無効な場合はSetClippingPlanesで設定した値を返す
    std::pair<float, float> GetClippingPlanes() const;

    /// @brief カメラの位置を設定する
    /// @param position カメラの位置座標 (x, y, z)
    void SetPosition(const igesio::Vector3f&);

    /// @brief カメラのターゲット位置を設定する
    /// @param target カメラのターゲット座標 (x, y, z)
    void SetTarget(const igesio::Vector3f&);

    /// @brief カメラの上方向を設定する
    /// @param up カメラの上方向ベクトル (x, y, z)
    /// @note 正規化して格納する
    void SetUp(const igesio::Vector3f&);

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
    /// @note 呼び出すと自動クリッピングを解除し、設定した手動値に固定する
    void SetClippingPlanes(const float, const float);

    /// @brief 自動クリッピング用のシーン外接球を設定する
    /// @param center 外接球の中心 (ワールド座標系)
    /// @param radius 外接球の半径 (負値は0として扱う)
    /// @note 設定後は、クリッピング面がカメラ位置と外接球から毎回導出される
    ///       (Zoom/Pan等でカメラが動いてもシーンがクリップされなくなる)
    void SetAutoClipSphere(const igesio::Vector3f&, const float);

    /// @brief 自動クリッピングを解除する
    /// @note 以降はSetClippingPlanesで設定した値が使用される
    void ClearAutoClipSphere();

    /// @brief 自動クリッピングが有効か
    /// @return 有効な場合はtrue
    bool IsAutoClipEnabled() const { return auto_clip_enabled_; }

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
     * ビュー設定
     */

    /// @brief 標準軸ビューへカメラの向きを設定する
    /// @param view 標準軸ビューの種別
    /// @note 現在のターゲットおよびターゲットまでの距離を維持したまま、位置と上方向を
    ///       標準軸方向へ再配置する. 物体全体を画面に収めるには別途FitToBoundingBoxを
    ///       呼ぶこと.
    void SetStandardView(const StandardView);

    /// @brief バウンディングボックス全体が収まるようにカメラを設定する
    /// @param bbox 対象のバウンディングボックス (ワールド座標系)
    /// @param aspect_ratio アスペクト比 (width / height)
    /// @param margin 余白係数 (1.0で密着, デフォルトはkDefaultFitMargin)
    /// @note 現在の視線方向と上方向を維持したまま、ターゲットをbboxの中心へ移し、
    ///       カメラ距離 (透視) またはビューボリュームのスケール (平行・斜) を、
    ///       bboxの外接球が収まるように調整する. また、外接球を自動クリッピング用に
    ///       登録する (SetAutoClipSphere).
    /// @note bboxが有限な頂点を持たない場合、またはaspect_ratioが非正の場合は何もしない.
    void FitToBoundingBox(const numerics::BoundingBox&, const float,
                          const float = kDefaultFitMargin);



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
