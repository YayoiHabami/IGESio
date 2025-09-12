/**
 * @file graphics/curves/circular_arc_graphics.h
 * @brief CircularArcの描画用クラス
 * @author Yayoi Habami
 * @date 2025-08-08
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_GRAPHICS_CURVES_CIRCULAR_ARC_GRAPHICS_H_
#define IGESIO_GRAPHICS_CURVES_CIRCULAR_ARC_GRAPHICS_H_

#include <memory>
#include <utility>

#include "igesio/entities/curves/circular_arc.h"
#include "igesio/graphics/core/entity_graphics.h"

namespace igesio::graphics {

/// @brief CircularArcエンティティの描画情報の管理クラス
class CircularArcGraphics
    : public EntityGraphics<entities::CircularArc, ShaderType::kCircularArc> {
 public:
    /// @brief コンストラクタ
    /// @param entity 描画するエンティティのポインタ
    /// @param gl OpenGL関数のラッパー
    /// @throw std::invalid_argument entityがnullptrの場合、
    ////       entityがICurveを継承していない場合
    explicit CircularArcGraphics(
            const std::shared_ptr<const entities::CircularArc>,
            const std::shared_ptr<IOpenGL>);

    /// @brief デストラクタ
    ~CircularArcGraphics();

    // コピーコンストラクタとコピー代入演算子を禁止
    CircularArcGraphics(const CircularArcGraphics&) = delete;
    CircularArcGraphics& operator=(const CircularArcGraphics&) = delete;

    /// @brief エンティティをセットアップする
    /// @note 内部で参照するエンティティの状態に基づいて、
    ///       描画用のリソースを再セットアップする
    void Synchronize() override;



 protected:
    /// @brief エンティティの描画を行う
    /// @param shader プログラムシェーダーのID
    /// @param viewport ビューポートのサイズ (width, height)
    void DrawImpl(GLuint, const std::pair<float, float>&) const override;
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CURVES_CIRCULAR_ARC_GRAPHICS_H_
