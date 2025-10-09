/**
 * @file graphics/core/light.h
 * @brief 光源クラス
 * @author Yayoi Habami
 * @date 2025-09-26
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_GRAPHICS_CORE_LIGHT_H_
#define IGESIO_GRAPHICS_CORE_LIGHT_H_

#include "igesio/common/matrix.h"



namespace igesio::graphics {

/// @brief 光源クラス
class Light {
    /// @brief 光源の位置
    Vector3f position_ = {10.0f, 10.0f, 10.0f};

    /// @brief 光源の色 (RGBA)
    /// @note 各成分は0.0fから1.0fの範囲.
    ///       デフォルトは白色光 (1.0f, 1.0f, 1.0f, 1.0f)
    Vector4f color_ = {1.0f, 1.0f, 1.0f, 1.0f};

 public:
    /// @brief デフォルトコンストラクタ
    Light() = default;

    /// @brief コンストラクタ
    /// @param position 光源の位置
    /// @param color 光源の色 (RGBA)
    Light(const Vector3f& position, const Vector4f& color)
        : position_(position), color_(color) {}



    /// @brief 光源の位置を取得する
    Vector3f GetPosition() const { return position_; }

    /// @brief 光源の位置を設定する
    void SetPosition(const Vector3f& position) { position_ = position; }

    /// @brief 光源の色を取得する
    Vector4f GetColor() const { return color_; }

    /// @brief 光源の色を設定する
    void SetColor(const Vector4f& color) { color_ = color; }

    /// @brief すべてのメンバ変数をリセットする
    void Reset() {
        position_ = {10.0f, 10.0f, 10.0f};
        color_ = {1.0f, 1.0f, 1.0f, 1.0f};
    }
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CORE_LIGHT_H_
