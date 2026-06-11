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
#include <utility>
#include <vector>

#include "igesio/graphics/shader_registry.h"



namespace igesio::graphics::shaders {

/// @brief kGeneralSurface用のシェーダー
/// @note Vertex, Fragment
constexpr std::array<const char*, 2> kGeneralSurfaceShader = {
    "glsl/surfaces/general_surface.vert",
    "glsl/surfaces/general_surface.frag"
};

/// @brief kRationalBSplineSurface用のシェーダー
/// @note Vertex, TCS, TES, Fragment
constexpr std::array<const char*, 4> kRationalBSplineSurfaceShader = {
    "glsl/surfaces/128_nurbs_surface.vert",
    "glsl/surfaces/128_nurbs_surface.tesc",
    "glsl/surfaces/128_nurbs_surface.tese",
    "glsl/surfaces/128_nurbs_surface.frag"
};



/// @brief 組み込み曲面シェーダーの定義一覧を取得する
/// @return (組み込みShaderId, ShaderInfo) のリスト
/// @note ShaderRegistryの組み込み設定用. 曲面系はいずれも光源を使用する
///       面塗りであり、kWireFrameでは描画されない
inline std::vector<std::pair<ShaderId, ShaderInfo>>
GetBuiltinSurfaceShaderInfos() {
    constexpr auto kFill = ShaderDrawCategory::kSurfaceFill;
    return {
        {ShaderId::kGeneralSurface,
         {"GeneralSurface", ShaderCode(kGeneralSurfaceShader), true, kFill}},
        {ShaderId::kRationalBSplineSurface,    // Type 128
         {"RationalBSplineSurface", ShaderCode(kRationalBSplineSurfaceShader),
          true, kFill}},
    };
}

}  // namespace igesio::graphics::shaders

#endif  // SRC_GRAPHICS_SHADERS_SURFACES_H_
