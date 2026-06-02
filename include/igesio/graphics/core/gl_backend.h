/**
 * @file graphics/core/gl_backend.h
 * @brief GLバックエンド (IOpenGL実装) を生成するファクトリの宣言
 * @author Yayoi Habami
 * @date 2026-06-02
 * @copyright 2026 Yayoi Habami
 * @note 具象実装 (gladを所有する`OpenGL`) はsrc/graphics/core/open_gl.cppに隠蔽される.
 *       利用側はこのヘッダと`gl_types.h`のみを参照し、gladをinclude/link/loadしない.
 */
#ifndef IGESIO_GRAPHICS_CORE_GL_BACKEND_H_
#define IGESIO_GRAPHICS_CORE_GL_BACKEND_H_

#include <memory>

#include "igesio/graphics/core/gl_types.h"
#include "igesio/graphics/core/i_open_gl.h"



namespace igesio::graphics {

/// @brief GLバックエンド (IOpenGL実装) を生成する
/// @param loader getProcAddress互換のローダ関数ポインタ
///        (例: `reinterpret_cast<GLProcLoader>(glfwGetProcAddress)`)
/// @return IOpenGL実装の共有ポインタ
/// @throw igesio::ImplementationError OpenGL関数のロードに失敗した場合
/// @note 呼び出し時にOpenGLコンテキストをカレントにしておくこと.
///       内部で`gladLoadGLContext`を呼び、コンテキスト固有の関数ポインタを取得する.
std::shared_ptr<IOpenGL> CreateGLBackend(GLProcLoader loader);

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CORE_GL_BACKEND_H_
