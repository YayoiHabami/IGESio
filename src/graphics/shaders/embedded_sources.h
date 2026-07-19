/**
 * @file graphics/shaders/embedded_sources.h
 * @brief ビルド時に埋め込まれたGLSLシェーダーソースへのアクセスを提供する
 * @author Yayoi Habami
 * @date 2026-07-19
 * @copyright 2026 Yayoi Habami
 * @note 実体はビルド時に生成される shader_sources_generated.cpp が定義する
 *       (生成元: cmake/embed_shaders.cmake)。
 */
#ifndef SRC_GRAPHICS_SHADERS_EMBEDDED_SOURCES_H_
#define SRC_GRAPHICS_SHADERS_EMBEDDED_SOURCES_H_

#include <string>
#include <unordered_map>

namespace igesio::graphics::shaders {

/// @brief ビルド時に埋め込まれたGLSLシェーダーソースを取得する
/// @return キー (glslフォルダをルートとした`glsl/...`形式の相対パス) から
///         ソーステキストへのマップ. インクルード展開前の生の内容を持つ
/// @note プロセス内で一度だけ構築される静的マップへの参照を返す
const std::unordered_map<std::string, std::string>& GetEmbeddedShaderSources();

}  // namespace igesio::graphics::shaders

#endif  // SRC_GRAPHICS_SHADERS_EMBEDDED_SOURCES_H_
