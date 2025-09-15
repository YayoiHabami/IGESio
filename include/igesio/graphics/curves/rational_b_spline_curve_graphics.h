/**
 * @file graphics/curves/rational_b_spline_curve_graphics.h
 * @brief RationalBSplineCurveの描画用クラス
 * @author Yayoi Habami
 * @date 2025-08-15
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_GRAPHICS_CURVES_RATIONAL_B_SPLINE_CURVE_GRAPHICS_H_
#define IGESIO_GRAPHICS_CURVES_RATIONAL_B_SPLINE_CURVE_GRAPHICS_H_

#include <memory>
#include <utility>

#include "igesio/common/matrix.h"
#include "igesio/entities/curves/rational_b_spline_curve.h"
#include "igesio/graphics/core/entity_graphics.h"



namespace igesio::graphics {

/// @brief RationalBSplineCurveエンティティの描画情報の管理クラス
class RationalBSplineCurveGraphics
    : public EntityGraphics<entities::RationalBSplineCurve,
                            ShaderType::kRationalBSplineCurve> {
 public:
    /// @brief コンストラクタ
    /// @param entity 描画するエンティティのポインタ
    /// @param gl OpenGL関数のラッパー
    /// @throw std::invalid_argument entityがnullptrの場合、
    ////       entityがICurveを継承していない場合
    explicit RationalBSplineCurveGraphics(
            const std::shared_ptr<const entities::RationalBSplineCurve>,
                   const std::shared_ptr<IOpenGL>);

    /// @brief デストラクタ
    ~RationalBSplineCurveGraphics();

    // コピーコンストラクタとコピー代入演算子を禁止
    RationalBSplineCurveGraphics(const RationalBSplineCurveGraphics&) = delete;
    RationalBSplineCurveGraphics& operator=(const RationalBSplineCurveGraphics&) = delete;

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


 protected:
    /// @brief エンティティの描画を行う
    /// @param shader プログラムシェーダーのID
    /// @param viewport ビューポートのサイズ (width, height)
    void DrawImpl(GLuint, const std::pair<float, float>&) const override;

    /// @brief 参照点
    /// @note テッセレーション数決定のために使用する、曲線上のいくつかの点.
    ///       これで曲線を折れ線に近似し、その画面上での長さからテッセレーション数を決定する.
    /// @note このサイズが0の場合、描画は行わない
    Matrix3Xf reference_points_;
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CURVES_RATIONAL_B_SPLINE_CURVE_GRAPHICS_H_
