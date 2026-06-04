/**
 * @file graphics/core/material_property.h
 * @brief エンティティの描画プロパティを定義する
 * @author Yayoi Habami
 * @date 2025-09-26
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_GRAPHICS_CORE_MATERIAL_PROPERTY_H_
#define IGESIO_GRAPHICS_CORE_MATERIAL_PROPERTY_H_

#include "igesio/numerics/core/matrix.h"
#include "igesio/graphics/core/texture.h"



namespace igesio::graphics {

/// @brief サーフェスプロパティクラス
/// @note IGESで保持されない、エンティティの描画プロパティを管理する
struct MaterialProperty {
    /// @brief 表面粗さ (roughness; for Surface)
    /// @note 0.0f(鏡面)から1.0f(完全拡散)の範囲. デフォルトは0.5f
    float roughness = 0.5f;

    /// @brief 金属度 (metallic; for Surface)
    /// @note 0.0f(非金属/誘電体)から1.0f(金属)の範囲. デフォルトは0.0f
    float metallic = 0.0f;

    /// @brief アンビエントオクルージョン (ao; for Surface)
    /// @note 0.0f(完全遮蔽)から1.0f(遮蔽なし)の範囲. デフォルトは1.0f
    float ao = 1.0f;

    /// @brief 透明度
    /// @note 0.0f (完全に透明) から 1.0f (完全に不透明)
    ///       の範囲. デフォルトは1.0f (完全に不透明)
    float opacity = 1.0f;

    /// @brief テクスチャ (for Surface)
    /// @note この変数にテクスチャを設定しただけでは描画に使用されない
    ///       ことに注意. `use_texture`をtrueに設定した後、
    ///       `EntityGraphics::SyncTexture()`を呼び出す必要がある.
    ///       一度テクスチャを設定した後にテクスチャの使用を中止/再開する
    ///       場合は、`use_texture`をfalse/trueに設定するだけでよい.
    Texture texture;
    /// @brief テクスチャを使用するか
    bool use_texture = false;
    /// @brief テクスチャが使用可能であるか
    /// @note texture.IsValid() && use_textureがtrueの場合にtrue
    bool IsTextureUsable() const {
        return texture.IsValid() && use_texture;
    }

    /// @brief すべてのメンバ変数をリセットする
    void Reset() {
        roughness = 0.5f;
        metallic = 0.0f;
        ao = 1.0f;
        opacity = 1.0f;
        texture.Clear();
        use_texture = false;
    }
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CORE_MATERIAL_PROPERTY_H_
