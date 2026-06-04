/**
 * @file graphics/core/i_open_gl.h
 * @brief OpenGL関数用のインターフェース
 * @author Yayoi Habami
 * @date 2025-08-17
 * @copyright 2025 Yayoi Habami
 * @note GL型はglad非依存の`gl::`型エイリアス (gl_types.h) を用いる.
 *       具象実装 (gladを所有する`OpenGL`) はsrc/graphics/core/open_gl.cppに隠蔽される.
 */
#ifndef IGESIO_GRAPHICS_CORE_I_OPEN_GL_H_
#define IGESIO_GRAPHICS_CORE_I_OPEN_GL_H_

#include "igesio/graphics/core/gl_types.h"



namespace igesio::graphics {

/// @brief OpenGL関数のインターフェース
/// @note OpenGLの関数をラップするインターフェース.
///       例えば`glViewport`は、`IOpenGL::Viewport`として定義される.
///       また、OpenGL 2.0以降の関数については、`@note OpenGL 3.0~`のように、
///       要求するバージョンを明記する.
/// @note このインターフェースは、OpenGLの関数を抽象化し、
///       テストやモックを容易にするために使用される.
class IOpenGL {
 public:
    /// @brief デストラクタ
    virtual ~IOpenGL() = default;



    /**
     * glUniform
     */

    virtual void Uniform1i(gl::Int location, gl::Int v0) = 0;
    virtual void Uniform2i(gl::Int location, gl::Int v0, gl::Int v1) = 0;
    virtual void Uniform3i(gl::Int location, gl::Int v0, gl::Int v1, gl::Int v2) = 0;
    virtual void Uniform4i(gl::Int location, gl::Int v0,
                           gl::Int v1, gl::Int v2, gl::Int v3) = 0;

    // skip: glUniform1ui ~ glUniform4ui

    virtual void Uniform1f(gl::Int location, gl::Float v0) = 0;
    virtual void Uniform2f(gl::Int location, gl::Float v0, gl::Float v1) = 0;
    virtual void Uniform3f(gl::Int location, gl::Float v0, gl::Float v1, gl::Float v2) = 0;
    virtual void Uniform4f(gl::Int location, gl::Float v0,
                           gl::Float v1, gl::Float v2, gl::Float v3) = 0;

    virtual void Uniform1fv(gl::Int location, gl::Sizei count, const gl::Float *value) = 0;
    virtual void Uniform2fv(gl::Int location, gl::Sizei count, const gl::Float *value) = 0;
    virtual void Uniform3fv(gl::Int location, gl::Sizei count, const gl::Float *value) = 0;
    virtual void Uniform4fv(gl::Int location, gl::Sizei count, const gl::Float *value) = 0;

    // skip: glUniform1iv ~ glUniform4iv
    // skip : glUniform1uiv ~ glUniform4uiv

    virtual void UniformMatrix2fv(gl::Int location, gl::Sizei count,
                                  gl::Boolean transpose, const gl::Float *value) = 0;
    virtual void UniformMatrix3fv(gl::Int location, gl::Sizei count,
                                  gl::Boolean transpose, const gl::Float *value) = 0;
    virtual void UniformMatrix4fv(gl::Int location, gl::Sizei count,
                                  gl::Boolean transpose, const gl::Float *value) = 0;

    // skip: glUniformMatrix2x3fv ~ glUniformMatrix4x3fv

    virtual gl::Int GetUniformLocation(gl::Uint program, const gl::Char *name) = 0;



    /**
     * Shader
     */

    virtual void AttachShader(gl::Uint program, gl::Uint shader) = 0;
    virtual void CompileShader(gl::Uint shader) = 0;
    virtual gl::Uint CreateShader(gl::Enum shaderType) = 0;
    virtual void DeleteShader(gl::Uint shader) = 0;
    virtual void GetShaderInfoLog(gl::Uint shader, gl::Sizei maxLength,
                                  gl::Sizei *length, gl::Char *infoLog) = 0;
    virtual void GetShaderiv(gl::Uint shader, gl::Enum pname, gl::Int *params) = 0;
    virtual void ShaderSource(gl::Uint shader, gl::Sizei count,
                              const gl::Char **string, const gl::Int *length) = 0;



    /**
     * Program
     */

    virtual gl::Uint CreateProgram() = 0;
    virtual void DeleteProgram(gl::Uint program) = 0;
    virtual void GetProgramInfoLog(gl::Uint program, gl::Sizei maxLength,
                                   gl::Sizei *length, gl::Char *infoLog) = 0;
    virtual void GetProgramiv(gl::Uint program, gl::Enum pname, gl::Int *params) = 0;
    virtual void LinkProgram(gl::Uint program) = 0;
    virtual void UseProgram(gl::Uint program) = 0;



    /**
     * Vertex Arrays
     */

    virtual void DrawArrays(gl::Enum mode, gl::Int first, gl::Sizei count) = 0;
    virtual void EnableVertexAttribArray(gl::Uint index) = 0;
    virtual void DisableVertexAttribArray(gl::Uint index) = 0;
    virtual void VertexAttribPointer(gl::Uint index, gl::Int size, gl::Enum type,
                                     gl::Boolean normalized, gl::Sizei stride,
                                     const void *pointer) = 0;

    /// @note OpenGL 3.0~
    virtual void BindVertexArray(gl::Uint array) = 0;
    /// @note OpenGL 3.0~
    virtual void DeleteVertexArrays(gl::Sizei n, const gl::Uint *arrays) = 0;
    /// @note OpenGL 3.0~
    virtual void GenVertexArrays(gl::Sizei n, gl::Uint *arrays) = 0;

    /// @note OpenGL 4.0~
    virtual void PatchParameteri(gl::Enum pname, gl::Int value) = 0;



    /**
     * Buffers
     */

    virtual void BindBuffer(gl::Enum target, gl::Uint buffer) = 0;
    /// @note OpenGL 3.0~
    virtual void BindBufferBase(gl::Enum target, gl::Uint index, gl::Uint buffer) = 0;
    virtual void BufferData(gl::Enum target, gl::Sizeiptr size,
                            const void *data, gl::Enum usage) = 0;
    virtual void DeleteBuffers(gl::Sizei n, const gl::Uint *buffers) = 0;
    virtual void GenBuffers(gl::Sizei n, gl::Uint *buffers) = 0;



    /**
     * Textures
     */

    virtual void ActiveTexture(gl::Enum texture) = 0;
    virtual void BindTexture(gl::Enum target, gl::Uint texture) = 0;
    virtual void DeleteTextures(gl::Sizei n, const gl::Uint *textures) = 0;
    virtual void GenerateMipmap(gl::Enum target) = 0;
    virtual void GenTextures(gl::Sizei n, gl::Uint *textures) = 0;
    virtual void TexImage2D(gl::Enum target, gl::Int level, gl::Int internalFormat,
                            gl::Sizei width, gl::Sizei height, gl::Int border,
                            gl::Enum format, gl::Enum type, const void *data) = 0;
    virtual void TexParameteri(gl::Enum target, gl::Enum pname, gl::Int param) = 0;



    /**
     * For Offscreen Rendering
     */

    /// @note OpenGL 3.0~
    virtual void BindFramebuffer(gl::Enum target, gl::Uint framebuffer) = 0;
    /// @note OpenGL 3.0~
    virtual gl::Enum CheckFramebufferStatus(gl::Enum target) = 0;
    /// @note OpenGL 3.0~
    virtual void DeleteFramebuffers(gl::Sizei n, const gl::Uint *framebuffers) = 0;
    /// @note OpenGL 3.0~
    virtual void GenFramebuffers(gl::Sizei n, gl::Uint *framebuffers) = 0;

    /// @note OpenGL 3.0~
    virtual void FramebufferTexture2D(gl::Enum target, gl::Enum attachment,
                                      gl::Enum textarget, gl::Uint texture,
                                      gl::Int level) = 0;

    /// @note OpenGL 3.0~
    virtual gl::Enum BindRenderbuffer(gl::Enum target, gl::Uint renderbuffer) = 0;
    /// @note OpenGL 3.0~
    virtual void DeleteRenderbuffers(gl::Sizei n, const gl::Uint *renderbuffers) = 0;
    /// @note OpenGL 3.0~
    virtual void GenRenderbuffers(gl::Sizei n, gl::Uint *renderbuffers) = 0;
    /// @note OpenGL 3.0~
    virtual void RenderbufferStorage(gl::Enum target, gl::Enum internalformat,
                                     gl::Sizei width, gl::Sizei height) = 0;

    /// @note OpenGL 3.0~
    virtual void FramebufferRenderbuffer(gl::Enum target, gl::Enum attachment,
                                         gl::Enum renderbuffertarget,
                                         gl::Uint renderbuffer) = 0;




    /**
     * Others
     */

    virtual void Enable(gl::Enum cap) = 0;
    virtual void Disable(gl::Enum cap) = 0;
    virtual void BlendFunc(gl::Enum sfactor, gl::Enum dfactor) = 0;
    virtual void Clear(gl::Bitfield mask) = 0;
    virtual void ClearColor(gl::Float red, gl::Float green,
                            gl::Float blue, gl::Float alpha) = 0;
    virtual void DrawElements(gl::Enum mode, gl::Sizei count,
                              gl::Enum type, const void *indices) = 0;
    virtual void Viewport(gl::Int x, gl::Int y,
                          gl::Sizei width, gl::Sizei height) = 0;
    virtual void GetIntegerv(gl::Enum pname, gl::Int *data) = 0;
    virtual void LineWidth(gl::Float width) = 0;
    virtual void PointSize(gl::Float size) = 0;
    virtual void PolygonOffset(gl::Float factor, gl::Float units) = 0;
    virtual void ReadPixels(gl::Int x, gl::Int y, gl::Sizei width, gl::Sizei height,
                            gl::Enum format, gl::Enum type, void *data) = 0;
    virtual gl::Enum GetError() = 0;
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CORE_I_OPEN_GL_H_
