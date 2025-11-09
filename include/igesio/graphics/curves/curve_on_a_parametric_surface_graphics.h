/**
 * @file graphics/curves/curve_on_a_parametric_surface_graphics.h
 * @brief CurveOnAParametricSurfaceの描画用クラス
 * @author Yayoi Habami
 * @date 2025-11-09
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_GRAPHICS_CURVES_CURVE_ON_A_PARAMETRIC_SURFACE_GRAPHICS_H_
#define IGESIO_GRAPHICS_CURVES_CURVE_ON_A_PARAMETRIC_SURFACE_GRAPHICS_H_

#include <memory>
#include <unordered_set>
#include <utility>

#include "igesio/entities/curves/curve_on_a_parametric_surface.h"
#include "igesio/graphics/core/entity_graphics.h"



namespace igesio::graphics {

/// @brief CurveOnAParametricSurfaceエンティティの描画情報の管理クラス
class CurveOnAParametricSurfaceGraphics
    : public EntityGraphics<entities::CurveOnAParametricSurface,
                            ShaderType::kComposite> {
 private:
    /// @brief C(t) 用の描画オブジェクト
    std::unique_ptr<IEntityGraphics> curve_graphics_;

 public:
    /// @brief コンストラクタ
    /// @param entity 描画するエンティティのポインタ
    /// @param gl OpenGL関数のラッパー
    /// @throw std::invalid_argument entityがnullptrの場合、
    ////       entityがICurveを継承していない場合
    CurveOnAParametricSurfaceGraphics(
            const std::shared_ptr<const entities::CurveOnAParametricSurface>&,
            const std::shared_ptr<IOpenGL>);

    /// @brief デストラクタ
    ~CurveOnAParametricSurfaceGraphics();

    // コピーコンストラクタとコピー代入演算子を禁止
    CurveOnAParametricSurfaceGraphics(
            const CurveOnAParametricSurfaceGraphics&) = delete;
    CurveOnAParametricSurfaceGraphics& operator=(
            const CurveOnAParametricSurfaceGraphics&) = delete;

    /// @brief エンティティをセットアップする
    /// @note 内部で参照するエンティティの状態に基づいて、
    ///       描画用のリソースを再セットアップする
    void Synchronize() override;

    /// @brief OpenGLリソースを解放する
    void Cleanup() override;

    /// @brief 描画可能な状態かどうかを確認する
    bool IsDrawable() const override;

    /// @brief 全ての可能なシェーダータイプを取得する
    /// @return 全ての可能なシェーダータイプのリスト
    /// @note 例えば子要素がある場合、`GetShaderType`のShaderTypeに加えて、
    ///       各子要素のShaderTypeも含まれる.
    std::unordered_set<ShaderType> GetShaderTypes() const override;

    /// @brief エンティティの描画を行う
    /// @param shader プログラムシェーダーのID
    /// @param shader_type 描画に使用するシェーダーのタイプ
    /// @param viewport ビューポートのサイズ (width, height)
    /// @note shader_typeの子要素のみを描画する. 子要素の描画は
    ///       子要素に移譲する.
    void Draw(GLuint, const ShaderType, const std::pair<float, float>&) const override;



 protected:
    /// @brief エンティティの描画を行う
    /// @param shader プログラムシェーダーのID
    /// @param viewport ビューポートのサイズ (width, height)
    void DrawImpl(GLuint, const std::pair<float, float>&) const override {};
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CURVES_CURVE_ON_A_PARAMETRIC_SURFACE_GRAPHICS_H_
