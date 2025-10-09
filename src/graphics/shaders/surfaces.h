/**
 * @file graphics/shaders/surfaces.h
 * @brief 曲面用のGLSLシェーダープログラムを定義する
 * @author Yayoi Habami
 * @date 2025-09-26
 * @copyright 2025 Yayoi Habami
 */
#ifndef SRC_GRAPHICS_SHADERS_SURFACES_H_
#define SRC_GRAPHICS_SHADERS_SURFACES_H_

#include <array>
#include <optional>

#include "igesio/graphics/core/i_entity_graphics.h"
#include "./shader_code.h"



namespace igesio::graphics::shaders {

/// @brief kRationalBSplineSurface用のシェーダー
/// @note Vertex, TCS, TES, Fragment
constexpr std::array<const char*, 4> kRationalBSplineSurfaceShader = {
    "glsl/surfaces/128_nurbs_surface.vert",
    "glsl/surfaces/128_nurbs_surface.tesc",
    "glsl/surfaces/128_nurbs_surface.tese",
    "glsl/surfaces/128_nurbs_surface.frag"
};



/// @brief 曲面シェーダーのソースコードを取得する
/// @param shader_type シェーダーの種類
/// @return シェーダーのソースコード、
///         指定された種類のシェーダーがない場合はnullopt
std::optional<ShaderCode>
GetSurfaceShaderCode(const ShaderType shader_type) {
    switch (shader_type) {
        case ShaderType::kRationalBSplineSurface:
            return ShaderCode(kRationalBSplineSurfaceShader);
        default:
            return std::nullopt;
    }
}

}  // namespace igesio::graphics::shaders

#endif  // SRC_GRAPHICS_SHADERS_SURFACES_H_
