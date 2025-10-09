/**
 * @file graphics/core/surface_property.h
 * @brief 面を持つエンティティの描画プロパティを定義する
 * @author Yayoi Habami
 * @date 2025-09-26
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_GRAPHICS_CORE_SURFACE_PROPERTY_H_
#define IGESIO_GRAPHICS_CORE_SURFACE_PROPERTY_H_

#include "igesio/common/matrix.h"



namespace igesio::graphics {

/// @brief サーフェスプロパティクラス
/// @note 面を持つエンティティの描画プロパティを管理する
class SurfaceProperty {
    /// @brief 環境光の強さ
    /// @note 0.0fから1.0fの範囲. デフォルトは0.1f
    float ambient_strength_ = 0.1f;

    /// @brief 鏡面反射光の強さ
    /// @note 0.0fから1.0fの範囲. デフォルトは0.5f
    float specular_strength_ = 0.5f;

    /// @brief 輝き (shininess)
    /// @note デフォルトは32
    int shininess_ = 32;

 public:
    /// @brief デフォルトコンストラクタ
    SurfaceProperty() = default;

    /// @brief コンストラクタ
    /// @param object_color オブジェクトの色 (RGBA)
    /// @param ambient_strength 環境光の強さ
    /// @param specular_strength 鏡面反射光の強さ
    /// @param shininess 輝き
    SurfaceProperty(float ambient_strength,
                    float specular_strength, int shininess)
        : ambient_strength_(ambient_strength),
          specular_strength_(specular_strength),
          shininess_(shininess) {}

    /// @brief 環境光の強さを取得する
    float GetAmbientStrength() const { return ambient_strength_; }

    /// @brief 環境光の強さを設定する
    void SetAmbientStrength(float ambient_strength) {
        ambient_strength_ = ambient_strength;
    }

    /// @brief 鏡面反射光の強さを取得する
    float GetSpecularStrength() const { return specular_strength_; }

    /// @brief 鏡面反射光の強さを設定する
    void SetSpecularStrength(float specular_strength) {
        specular_strength_ = specular_strength;
    }

    /// @brief 輝きを取得する
    int GetShininess() const { return shininess_; }

    /// @brief 輝きを設定する
    void SetShininess(int shininess) { shininess_ = shininess; }

    /// @brief すべてのメンバ変数をリセットする
    void Reset() {
        ambient_strength_ = 0.1f;
        specular_strength_ = 0.5f;
        shininess_ = 32;
    }
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CORE_SURFACE_PROPERTY_H_