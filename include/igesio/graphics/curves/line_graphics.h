/**
 * @file graphics/curves/line_graphics.h
 * @brief 線分/半直線/直線の描画用クラス
 * @author Yayoi Habami
 * @date 2025-08-10
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_GRAPHICS_CURVES_LINE_GRAPHICS_H_
#define IGESIO_GRAPHICS_CURVES_LINE_GRAPHICS_H_

#include <memory>
#include <utility>

#include "igesio/entities/curves/line.h"
#include "igesio/graphics/core/entity_graphics.h"



namespace igesio::graphics {

/// @brief 線分/半直線の描画用クラス
class SegmentGraphics
    : public EntityGraphics<entities::Line, ShaderType::kSegment> {
 private:
    /// @brief エンティティの描画用の頂点バッファオブジェクト (VBO) のID
    GLuint vbo_ = 0;
    /// @brief エンティティの頂点数
    GLsizei vertex_count_ = 0;
    /// @brief 無限長の端点の描画時の長さ
    double far_length_ = 1e10f;



 public:
    /// @brief コンストラクタ
    /// @param entity 描画するエンティティのポインタ
    /// @param gl OpenGL関数のラッパー
    /// @throw std::invalid_argument entityがnullptrの場合、
    ////       entityがICurveを継承していない場合
    explicit SegmentGraphics(const std::shared_ptr<const entities::Line>,
                          const std::shared_ptr<IOpenGL>);

    /// @brief デストラクタ
    ~SegmentGraphics();

    // コピーコンストラクタとコピー代入演算子を禁止
    SegmentGraphics(const SegmentGraphics&) = delete;
    SegmentGraphics& operator=(const SegmentGraphics&) = delete;

    /// @brief ムーブコンストラクタ
    /// @param other ムーブ元のEntityGraphics
    /// @note ムーブ元のリソース所有権を放棄
    SegmentGraphics(SegmentGraphics&&) noexcept;

    /// @brief ムーブ代入演算子
    /// @param other ムーブ元のEntityGraphics
    /// @return *this
    /// @note ムーブ元のリソース所有権を放棄
    SegmentGraphics& operator=(SegmentGraphics&&) noexcept;

    /// @brief OpenGLリソースを解放する
    void Cleanup() override;

    /// @brief 描画可能な状態かどうかを確認する
    bool IsDrawable() const override;

    /// @brief エンティティをセットアップする
    /// @note 内部で参照するエンティティの状態に基づいて、
    ///       描画用のリソースを再セットアップする
    void Synchronize() override;



 protected:
    /// @brief エンティティの描画を行う
    /// @param shader プログラムシェーダーのID
    /// @param viewport ビューポートのサイズ (width, height)
    void DrawImpl(
            GLuint, [[maybe_unused]] const std::pair<float, float>&) const override;
};



/// @brief 直線の描画用クラス
class LineGraphics
    : public EntityGraphics<entities::Line, ShaderType::kLine> {
 private:
    /// @brief エンティティの描画用の頂点バッファオブジェクト (VBO) のID
    GLuint vbo_ = 0;
    /// @brief エンティティの頂点数
    GLsizei vertex_count_ = 0;
    /// @brief 無限長の端点の描画時の長さ
    double far_length_ = 1e10f;



 public:
    /// @brief コンストラクタ
    /// @param entity 描画するエンティティのポインタ
    /// @param gl OpenGL関数のラッパー
    /// @throw std::invalid_argument entityがnullptrの場合、
    ////       entityがICurveを継承していない場合
    explicit LineGraphics(const std::shared_ptr<const entities::Line>,
                          const std::shared_ptr<IOpenGL>);

    /// @brief デストラクタ
    ~LineGraphics();

    // コピーコンストラクタとコピー代入演算子を禁止
    LineGraphics(const LineGraphics&) = delete;
    LineGraphics& operator=(const LineGraphics&) = delete;

    /// @brief ムーブコンストラクタ
    /// @param other ムーブ元のEntityGraphics
    /// @note ムーブ元のリソース所有権を放棄
    LineGraphics(LineGraphics&&) noexcept;

    /// @brief ムーブ代入演算子
    /// @param other ムーブ元のEntityGraphics
    /// @return *this
    /// @note ムーブ元のリソース所有権を放棄
    LineGraphics& operator=(LineGraphics&&) noexcept;

    /// @brief OpenGLリソースを解放する
    void Cleanup() override;

    /// @brief 描画可能な状態かどうかを確認する
    bool IsDrawable() const override;

    /// @brief エンティティをセットアップする
    /// @note 内部で参照するエンティティの状態に基づいて、
    ///       描画用のリソースを再セットアップする
    void Synchronize() override;



 protected:
    /// @brief エンティティの描画を行う
    /// @param shader プログラムシェーダーのID
    /// @param viewport ビューポートのサイズ (width, height)
    void DrawImpl(
            GLuint, [[maybe_unused]] const std::pair<float, float>&) const override;
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CURVES_LINE_GRAPHICS_H_
