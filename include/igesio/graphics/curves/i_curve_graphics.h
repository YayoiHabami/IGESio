/**
 * @file graphics/curves/i_curve_graphics.h
 * @brief 曲線全般の描画用クラス
 * @author Yayoi Habami
 * @date 2025-08-06
 * @copyright 2025 Yayoi Habami
 * @brief このクラスは、ICurveを継承した全エンティティの描画を行う。
 *        曲線のパラメータ範囲で離散化を行い、OpenGLのVAOとVBOを使用して描画する。
 */
#ifndef IGESIO_GRAPHICS_CURVES_I_CURVE_GRAPHICS_H_
#define IGESIO_GRAPHICS_CURVES_I_CURVE_GRAPHICS_H_

#include <memory>
#include <utility>

#include "igesio/entities/interfaces/i_curve.h"
#include "igesio/graphics/core/entity_graphics.h"



namespace igesio::graphics {

/// @brief ICurveエンティティの描画情報の管理クラス
class ICurveGraphics
    : public EntityGraphics<entities::ICurve, ShaderType::kGeneralCurve> {
 private:
    /// @brief エンティティの描画用の頂点バッファオブジェクト (VBO) のID
    GLuint vbo_ = 0;
    /// @brief エンティティの頂点数
    GLsizei vertex_count_ = 0;



 public:
    /// @brief コンストラクタ
    /// @param entity 描画するエンティティのポインタ
    /// @param gl OpenGL関数のラッパー
    /// @throw std::invalid_argument entityがnullptrの場合、
    ///       entityがICurveを継承していない場合
    explicit ICurveGraphics(const std::shared_ptr<const entities::ICurve>,
                            const std::shared_ptr<IOpenGL>);

    /// @brief デストラクタ
    ~ICurveGraphics();

    // コピーコンストラクタとコピー代入演算子を禁止
    ICurveGraphics(const ICurveGraphics&) = delete;
    ICurveGraphics& operator=(const ICurveGraphics&) = delete;

    /// @brief ムーブコンストラクタ
    /// @param other ムーブ元のEntityGraphics
    /// @note ムーブ元のリソース所有権を放棄
    ICurveGraphics(ICurveGraphics&&) noexcept;

    /// @brief ムーブ代入演算子
    /// @param other ムーブ元のEntityGraphics
    /// @return *this
    /// @note ムーブ元のリソース所有権を放棄
    ICurveGraphics& operator=(ICurveGraphics&&) noexcept;

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

#endif  // IGESIO_GRAPHICS_CURVES_I_CURVE_GRAPHICS_H_
