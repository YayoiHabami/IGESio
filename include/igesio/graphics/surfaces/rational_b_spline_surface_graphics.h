/**
 * @file graphics/curves/rational_b_spline_surface_graphics.h
 * @brief RationalBSplineSurfaceの描画用クラス
 * @author Yayoi Habami
 * @date 2025-09-26
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_GRAPHICS_SURFACES_RATIONAL_B_SPLINE_SURFACE_GRAPHICS_H_
#define IGESIO_GRAPHICS_SURFACES_RATIONAL_B_SPLINE_SURFACE_GRAPHICS_H_

#include <memory>
#include <utility>

#include "igesio/common/matrix.h"
#include "igesio/entities/surfaces/rational_b_spline_surface.h"
#include "igesio/graphics/core/entity_graphics.h"



namespace igesio::graphics {

/// @brief RationalBSplineSurfaceエンティティの描画情報の管理クラス
class RationalBSplineSurfaceGraphics
    : public EntityGraphics<entities::RationalBSplineSurface,
                            ShaderType::kRationalBSplineSurface,
                            true> {
    /// @brief u方向のノットベクトルのSSBO
    GLuint knots_u_ssbo_ = 0;
    /// @brief v方向のノットベクトルのSSBO
    GLuint knots_v_ssbo_ = 0;
    /// @brief 制御点と重みのSSBO
    /// @note vec4のx~zが制御点、wが重み
    GLuint control_with_weights_ssbo_ = 0;
    /// @brief 参照点のSSBO
    GLuint reference_points_ssbo_ = 0;

 public:
    /// @brief コンストラクタ
    /// @param entity 描画するエンティティのポインタ
    /// @param gl OpenGL関数のラッパー
    /// @throw std::invalid_argument entityがnullptrの場合、
    ////       entityがISurfaceを継承していない場合
    explicit RationalBSplineSurfaceGraphics(
            const std::shared_ptr<const entities::RationalBSplineSurface>,
            const std::shared_ptr<IOpenGL>);

    /// @brief デストラクタ
    ~RationalBSplineSurfaceGraphics();

    // コピーコンストラクタとコピー代入演算子を禁止
    RationalBSplineSurfaceGraphics(const RationalBSplineSurfaceGraphics&) = delete;
    RationalBSplineSurfaceGraphics& operator=(const RationalBSplineSurfaceGraphics&) = delete;

    /// @brief 描画可能な状態かどうかを確認する
    /// @note 参照点が0の場合は描画できない
    bool IsDrawable() const override {
        return EntityGraphics::IsDrawable() &&
               (reference_points_.cols() > 0);
    }

    /// @brief エンティティをセットアップする
    /// @note 内部で参照するエンティティの状態に基づいて、
    ///       描画用のリソースを再セットアップする
    void Synchronize() override;

    /// @brief OpenGLリソースを解放する
    /// @note SSBO等のOpenGLリソースを解放する
    void Cleanup() override;

 protected:
    /// @brief エンティティの描画を行う
    /// @param shader プログラムシェーダーのID
    /// @param viewport ビューポートのサイズ (width, height)
    void DrawImpl(GLuint, const std::pair<float, float>&) const override;

    /// @brief 参照点
    /// @note テッセレーション数決定のために使用する、曲面上のいくつかの点
    ///       曲面のu,v方向の長さを計算し、その画面上での長さから
    ///       テッセレーション数を決定する. std430のSSBOによるデータ転送を行うため、
    ///       4行N列の行列として保持する (4行目はダミー).
    MatrixXf reference_points_;
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_SURFACES_RATIONAL_B_SPLINE_SURFACE_GRAPHICS_H_
