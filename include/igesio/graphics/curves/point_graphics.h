/**
 * @file graphics/curves/point_graphics.h
 * @brief 点の描画用クラス
 * @author Yayoi Habami
 * @date 2025-10-15
 * @copyright 2025 Yayoi Habami
 * @note Pointエンティティ (Type 116) の描画を行う. Pointエンティティは
 *       Subfigure Definition (Type 308) への参照を持つことがあり、この場合
 *       点をSubfigure Definitionで定義された図形で描画する必要があるため、
 *       独立した描画クラスを用意する.
 */
#ifndef IGESIO_GRAPHICS_CURVES_POINT_GRAPHICS_H_
#define IGESIO_GRAPHICS_CURVES_POINT_GRAPHICS_H_

#include <memory>
#include <utility>

#include "igesio/entities/curves/point.h"
#include "igesio/graphics/core/entity_graphics.h"



namespace igesio::graphics {

/// @brief 点の描画用クラス
/// @note Pointエンティティ (Type 116) の描画を行う. Pointエンティティは
///       Subfigure Definition (Type 308) への参照を持つことがあり、この場合
///       点をSubfigure Definitionで定義された図形で描画する必要があるため、
///       独立した描画クラスを用意する.
class PointGraphics
    : public EntityGraphics<entities::Point, ShaderType::kPoint> {
 private:
    /// @brief エンティティの描画用の頂点バッファオブジェクト (VBO) のID
    GLuint vbo_ = 0;

 public:
    /// @brief コンストラクタ
    /// @param entity 描画するエンティティのポインタ
    /// @param gl OpenGL関数のラッパー
    /// @throw std::invalid_argument entityがnullptrの場合
    explicit PointGraphics(const std::shared_ptr<const entities::Point>,
                           const std::shared_ptr<IOpenGL>);

    /// @brief デストラクタ
    ~PointGraphics();

    // コピーコンストラクタとコピー代入演算子を禁止
    PointGraphics(const PointGraphics&) = delete;
    PointGraphics& operator=(const PointGraphics&) = delete;

    /// @brief ムーブコンストラクタ
    /// @param other ムーブ元のEntityGraphics
    PointGraphics(PointGraphics&&) noexcept;

    /// @brief ムーブ代入演算子
    /// @param other ムーブ元のEntityGraphics
    /// @return *this
    PointGraphics& operator=(PointGraphics&&) noexcept;

    /// @brief OpenGLリソースを解放する
    void Cleanup() override;

    /// @brief 描画可能な状態かどうかを確認する
    bool IsDrawable() const override;

    /// @brief エンティティをセットアップする
    void Synchronize() override;

 protected:
    /// @brief エンティティの描画を行う
    /// @param shader プログラムシェーダーのID
    /// @param viewport ビューポートのサイズ (width, height)
    void DrawImpl(GLuint, const std::pair<float, float>&) const override;
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CURVES_POINT_GRAPHICS_H_
