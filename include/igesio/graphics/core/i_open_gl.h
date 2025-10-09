/**
 * @file graphics/core/i_open_gl.h
 * @brief OpenGL関数用のインターフェース
 * @author Yayoi Habami
 * @date 2025-08-17
 * @copyright 2025 Yayoi Habami
 * @note このファイルは'glad/gl.h'をインクルードしているため、
 *       このファイルをインクルードする際は、他のOpenGL/GLFWヘッダを
 *       インクルードする前にこのファイルをインクルードすること
 */
#ifndef IGESIO_GRAPHICS_CORE_I_OPEN_GL_H_
#define IGESIO_GRAPHICS_CORE_I_OPEN_GL_H_

#include <glad/gl.h>



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

    virtual void Uniform1i(GLint location, GLint v0) = 0;
    virtual void Uniform2i(GLint location, GLint v0, GLint v1) = 0;
    virtual void Uniform3i(GLint location, GLint v0, GLint v1, GLint v2) = 0;
    virtual void Uniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3) = 0;

    // skip: glUniform1ui ~ glUniform4ui

    virtual void Uniform1f(GLint location, GLfloat v0) = 0;
    virtual void Uniform2f(GLint location, GLfloat v0, GLfloat v1) = 0;
    virtual void Uniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) = 0;
    virtual void Uniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) = 0;

    virtual void Uniform1fv(GLint location, GLsizei count, const GLfloat *value) = 0;
    virtual void Uniform2fv(GLint location, GLsizei count, const GLfloat *value) = 0;
    virtual void Uniform3fv(GLint location, GLsizei count, const GLfloat *value) = 0;
    virtual void Uniform4fv(GLint location, GLsizei count, const GLfloat *value) = 0;

    // skip: glUniform1iv ~ glUniform4iv
    // skip : glUniform1uiv ~ glUniform4uiv

    virtual void UniformMatrix2fv(GLint location, GLsizei count,
                                  GLboolean transpose, const GLfloat *value) = 0;
    virtual void UniformMatrix3fv(GLint location, GLsizei count,
                                  GLboolean transpose, const GLfloat *value) = 0;
    virtual void UniformMatrix4fv(GLint location, GLsizei count,
                                  GLboolean transpose, const GLfloat *value) = 0;

    // skip: glUniformMatrix2x3fv ~ glUniformMatrix4x3fv

    virtual GLint GetUniformLocation(GLuint program, const GLchar *name) = 0;



    /**
     * Shader
     */

    virtual void AttachShader(GLuint program, GLuint shader) = 0;
    virtual void CompileShader(GLuint shader) = 0;
    virtual GLuint CreateShader(GLenum shaderType) = 0;
    virtual void DeleteShader(GLuint shader) = 0;
    virtual void GetShaderInfoLog(GLuint shader, GLsizei maxLength,
                                  GLsizei *length, GLchar *infoLog) = 0;
    virtual void GetShaderiv(GLuint shader, GLenum pname, GLint *params) = 0;
    virtual void ShaderSource(GLuint shader, GLsizei count,
                              const GLchar **string, const GLint *length) = 0;



    /**
     * Program
     */

    virtual GLuint CreateProgram() = 0;
    virtual void DeleteProgram(GLuint program) = 0;
    virtual void GetProgramInfoLog(GLuint program, GLsizei maxLength,
                                   GLsizei *length, GLchar *infoLog) = 0;
    virtual void GetProgramiv(GLuint program, GLenum pname, GLint *params) = 0;
    virtual void LinkProgram(GLuint program) = 0;
    virtual void UseProgram(GLuint program) = 0;



    /**
     * Vertex Arrays
     */

    virtual void DrawArrays(GLenum mode, GLint first, GLsizei count) = 0;
    virtual void EnableVertexAttribArray(GLuint index) = 0;
    virtual void DisableVertexAttribArray(GLuint index) = 0;
    virtual void VertexAttribPointer(GLuint index, GLint size, GLenum type,
                                     GLboolean normalized, GLsizei stride,
                                     const void *pointer) = 0;

    /// @note OpenGL 3.0~
    virtual void BindVertexArray(GLuint array) = 0;
    /// @note OpenGL 3.0~
    virtual void DeleteVertexArrays(GLsizei n, const GLuint *arrays) = 0;
    /// @note OpenGL 3.0~
    virtual void GenVertexArrays(GLsizei n, GLuint *arrays) = 0;

    /// @note OpenGL 4.0~
    virtual void PatchParameteri(GLenum pname, GLint value) = 0;



    /**
     * Buffers
     */

    virtual void BindBuffer(GLenum target, GLuint buffer) = 0;
    /// @note OpenGL 3.0~
    virtual void BindBufferBase(GLenum target, GLuint index, GLuint buffer) = 0;
    virtual void BufferData(GLenum target, GLsizeiptr size,
                            const void *data, GLenum usage) = 0;
    virtual void DeleteBuffers(GLsizei n, const GLuint *buffers) = 0;
    virtual void GenBuffers(GLsizei n, GLuint *buffers) = 0;



    /**
     * Textures
     */

    virtual void BindTexture(GLenum target, GLuint texture) = 0;
    virtual void DeleteTextures(GLsizei n, const GLuint *textures) = 0;
    virtual void GenTextures(GLsizei n, GLuint *textures) = 0;
    virtual void TexImage2D(GLenum target, GLint level, GLint internalFormat,
                            GLsizei width, GLsizei height, GLint border,
                            GLenum format, GLenum type, const void *data) = 0;
    virtual void TexParameteri(GLenum target, GLenum pname, GLint param) = 0;



    /**
     * For Offscreen Rendering
     */

    /// @note OpenGL 3.0~
    virtual void BindFramebuffer(GLenum target, GLuint framebuffer) = 0;
    /// @note OpenGL 3.0~
    virtual GLenum CheckFramebufferStatus(GLenum target) = 0;
    /// @note OpenGL 3.0~
    virtual void DeleteFramebuffers(GLsizei n, const GLuint *framebuffers) = 0;
    /// @note OpenGL 3.0~
    virtual void GenFramebuffers(GLsizei n, GLuint *framebuffers) = 0;

    /// @note OpenGL 3.0~
    virtual void FramebufferTexture2D(GLenum target, GLenum attachment,
                                      GLenum textarget, GLuint texture,
                                      GLint level) = 0;

    /// @note OpenGL 3.0~
    virtual GLenum BindRenderbuffer(GLenum target, GLuint renderbuffer) = 0;
    /// @note OpenGL 3.0~
    virtual void DeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers) = 0;
    /// @note OpenGL 3.0~
    virtual void GenRenderbuffers(GLsizei n, GLuint *renderbuffers) = 0;
    /// @note OpenGL 3.0~
    virtual void RenderbufferStorage(GLenum target, GLenum internalformat,
                                     GLsizei width, GLsizei height) = 0;

    /// @note OpenGL 3.0~
    virtual void FramebufferRenderbuffer(GLenum target, GLenum attachment,
                                         GLenum renderbuffertarget,
                                         GLuint renderbuffer) = 0;




    /**
     * Others
     */

    virtual void Enable(GLenum cap) = 0;
    virtual void Disable(GLenum cap) = 0;
    virtual void Clear(GLbitfield mask) = 0;
    virtual void ClearColor(GLfloat red, GLfloat green,
                            GLfloat blue, GLfloat alpha) = 0;
    virtual void Viewport(GLint x, GLint y,
                          GLsizei width, GLsizei height) = 0;
    virtual void GetIntegerv(GLenum pname, GLint *data) = 0;
    virtual void LineWidth(GLfloat width) = 0;
    virtual void PointSize(GLfloat size) = 0;
    virtual void ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                            GLenum format, GLenum type, void *data) = 0;
    virtual GLenum GetError() = 0;
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CORE_I_OPEN_GL_H_
