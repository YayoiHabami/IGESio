/**
 * @file graphics/shaders.h
 * @brief シェーダープログラムをまとめる
 * @author Yayoi Habami
 * @date 2025-08-07
 * @copyright 2025 Yayoi Habami
 */
#ifndef SRC_GRAPHICS_SHADERS_H_
#define SRC_GRAPHICS_SHADERS_H_

#include "igesio/graphics/core/i_entity_graphics.h"
#include "./shaders/curves.h"

namespace igesio::graphics::shaders {

/// @brief シェーダーのソースコードを取得する
/// @param shader_type シェーダーのタイプ
/// @return シェーダーのソースコード、
///         指定されたタイプのシェーダーがない場合はnullopt
std::optional<ShaderCode> GetShaderCode(const ShaderType shader_type) {
    // 特定のシェーダーを持たないタイプの場合はnullopt
    if (!HasSpecificShaderCode(shader_type)) return std::nullopt;

    if (auto code = GetCurveShaderCode(shader_type)) {
        return *code;
    }

    // 対応するシェーダーコードが見つからない場合はnullopt
    return std::nullopt;
}

}  // namespace igesio::graphics::shaders

#endif  // SRC_GRAPHICS_SHADERS_H_
