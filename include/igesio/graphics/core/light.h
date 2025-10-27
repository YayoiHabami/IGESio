/**
 * @file graphics/core/light.h
 * @brief 光源クラス
 * @author Yayoi Habami
 * @date 2025-09-26
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_GRAPHICS_CORE_LIGHT_H_
#define IGESIO_GRAPHICS_CORE_LIGHT_H_

#include "igesio/numerics/matrix.h"



namespace igesio::graphics {

/// @brief 光源の種類
enum class LightType {
    /// @brief 面光源 (方向のみ指定)
    kDirectional,
    /// @brief 点光源 (位置を指定)
    kPoint
};

/// @brief 光源クラス
struct Light {
    /// @brief 光源の種類
    LightType type = LightType::kDirectional;

    /// @brief 光の向かう方向 (面光源) / 光源の位置 (点光源)
    /// @note 面光源の場合でも、単位ベクトルである必要はない.
    Vector3f position = {-1.0f, -1.0f, -1.0f};

    /// @brief 距離減衰係数 C, L, Q (点光源の場合に使用)
    /// @note 光源からの距離dに対して、光の強度は
    ///       1.0 / (C + L*d + Q*d^2)で減衰する.
    /// @note 面光源の場合は (0, 0, 0) に設定される.
    std::array<float, 3> attenuation = {0.0f, 0.0f, 0.0f};

    /// @brief 光源の色 (RGBA)
    /// @note 各成分は0.0fから1.0fの範囲.
    ///       デフォルトは白色光 (1.0f, 1.0f, 1.0f, 1.0f)
    Vector4f color = {1.0f, 1.0f, 1.0f, 1.0f};

    /// @brief 面光源として設定する
    /// @param direction 光の向かう方向 (単位ベクトル)
    void SetDirectional(const Vector3f& direction,
                        const Vector4f& light_color = {1.0f, 1.0f, 1.0f, 1.0f}) {
        type = LightType::kDirectional;
        position = direction;
        attenuation = {0.0f, 0.0f, 0.0f};
        color = light_color;
    }

    /// @brief 点光源として設定する
    /// @param pos 光源の位置
    /// @param attenuation_coeffs 距離減衰係数 C, L, Q
    /// @param light_color 光源の色 (RGBA)
    void SetPoint(const Vector3f& pos,
                  const std::array<float, 3>& attenuation_coeffs = {1.0f, 0.0f, 0.0f},
                  const Vector4f& light_color = {1.0f, 1.0f, 1.0f, 1.0f}) {
        type = LightType::kPoint;
        position = pos;
        attenuation = attenuation_coeffs;
        color = light_color;
    }

    /// @brief すべてのメンバ変数をリセットする
    void Reset() {
        SetDirectional({-1.0f, -1.0f, -1.0f});
    }
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CORE_LIGHT_H_
