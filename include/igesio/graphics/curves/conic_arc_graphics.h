/**
 * @file graphics/curves/conic_arc_graphics.h
 * @brief ConicArcの描画用クラス
 * @author Yayoi Habami
 * @date 2025-08-09
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_GRAPHICS_CURVES_CONIC_ARC_GRAPHICS_H_
#define IGESIO_GRAPHICS_CURVES_CONIC_ARC_GRAPHICS_H_

#include <memory>
#include <utility>

#include "igesio/entities/curves/conic_arc.h"
#include "igesio/graphics/core/entity_graphics.h"

namespace igesio::graphics {

/// @brief 楕円エンティティの描画情報の管理クラス
/// @note このクラスは、ConicArcのうち、ConicType::kEllipseに対応する
///       描画用クラスである。
class EllipseGraphics
    : public EntityGraphics<entities::ConicArc, ShaderType::kEllipse> {
 public:
    /// @brief コンストラクタ
    /// @param entity 描画するエンティティのポインタ
    /// @param gl OpenGL関数のラッパー
    /// @throw std::invalid_argument entityがnullptrの場合、
    ///       entityがICurveを継承していない場合、
    ///       entityがConicType::kEllipseでない場合
    explicit EllipseGraphics(const std::shared_ptr<const entities::ConicArc>,
                             const std::shared_ptr<IOpenGL>);

    /// @brief デストラクタ
    ~EllipseGraphics();

    // コピーコンストラクタとコピー代入演算子を禁止
    EllipseGraphics(const EllipseGraphics&) = delete;
    EllipseGraphics& operator=(const EllipseGraphics&) = delete;

    /// @brief エンティティをセットアップする
    /// @note 内部で参照するエンティティの状態に基づいて、
    ///       描画用のリソースを再セットアップする
    void Synchronize() override;



 protected:
    /// @brief エンティティの描画を行う
    /// @param shader プログラムシェーダーのID
    /// @param viewport ビューポートのサイズ (width, height)
    void DrawImpl(GLuint, [[maybe_unused]] const std::pair<float, float>&) const override;
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CURVES_CONIC_ARC_GRAPHICS_H_
