/**
 * @file graphics/curves/composite_curve_graphics.h
 * @brief CompositeCurveの描画用クラス
 * @author Yayoi Habami
 * @date 2025-08-10
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_GRAPHICS_CURVES_COMPOSITE_CURVE_GRAPHICS_H_
#define IGESIO_GRAPHICS_CURVES_COMPOSITE_CURVE_GRAPHICS_H_

#include <memory>
#include <utility>

#include "igesio/entities/curves/composite_curve.h"
#include "igesio/graphics/core/composite_entity_graphics.h"



namespace igesio::graphics {

/// @brief CompositeCurveの描画用クラス
class CompositeCurveGraphics
    : public CompositeEntityGraphics<entities::CompositeCurve> {
 public:
    /// @brief コンストラクタ
    /// @param entity 描画するエンティティのポインタ
    /// @param gl OpenGL関数のラッパー
    /// @throw std::invalid_argument entityがnullptrの場合
    CompositeCurveGraphics(
            const std::shared_ptr<const entities::CompositeCurve>,
            const std::shared_ptr<IOpenGL>);

    /// @brief エンティティをセットアップする
    /// @note 内部で参照するエンティティの状態に基づいて、
    ///       描画用のリソースを再セットアップする
    void Synchronize() override;
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CURVES_COMPOSITE_CURVE_GRAPHICS_H_
