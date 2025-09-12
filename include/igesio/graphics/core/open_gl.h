/**
 * @file graphics/core/open_gl.h
 * @brief OpenGLの関数をラップするクラス
 * @author Yayoi Habami
 * @date 2025-08-17
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_GRAPHICS_CORE_OPEN_GL_H_
#define IGESIO_GRAPHICS_CORE_OPEN_GL_H_

#include "igesio/graphics/core/i_open_gl.h"



namespace igesio::graphics {

/// @brief OpenGLの関数をラップするクラス
/// @note 例えば`glViewport`は、`IOpenGL::Viewport`として定義される.
///       また、OpenGL 2.0以降の関数については、`@note OpenGL 3.0~`のように、
///       要求するバージョンを明記する.
class OpenGL : public IOpenGL {
 public:
    /// @brief デストラクタ
    ~OpenGL() override = default;



    /**
     * glUniform
     */

    void Uniform1i(GLint location, GLint v0) override {
        glUniform1i(location, v0);
    }
    void Uniform2i(GLint location, GLint v0, GLint v1) override {
        glUniform2i(location, v0, v1);
    }
    void Uniform3i(GLint location, GLint v0, GLint v1, GLint v2) override {
        glUniform3i(location, v0, v1, v2);
    }
    void Uniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3) override {
        glUniform4i(location, v0, v1, v2, v3);
    }

    // skip: glUniform1ui ~ glUniform4ui

    void Uniform1f(GLint location, GLfloat v0) override {
        glUniform1f(location, v0);
    }
    void Uniform2f(GLint location, GLfloat v0, GLfloat v1) override {
        glUniform2f(location, v0, v1);
    }
    void Uniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) override {
        glUniform3f(location, v0, v1, v2);
    }
    void Uniform4f(GLint location, GLfloat v0,
                   GLfloat v1, GLfloat v2, GLfloat v3) override {
        glUniform4f(location, v0, v1, v2, v3);
    }

    // skip: glUniform1iv ~ glUniform4iv
    // skip : glUniform1uiv ~ glUniform4uiv

    void Uniform1fv(GLint location, GLsizei count, const GLfloat *value) override {
        glUniform1fv(location, count, value);
    }
    void Uniform2fv(GLint location, GLsizei count, const GLfloat *value) override {
        glUniform2fv(location, count, value);
    }
    void Uniform3fv(GLint location, GLsizei count, const GLfloat *value) override {
        glUniform3fv(location, count, value);
    }
    void Uniform4fv(GLint location, GLsizei count, const GLfloat *value) override {
        glUniform4fv(location, count, value);
    }

    void UniformMatrix2fv(GLint location, GLsizei count,
                          GLboolean transpose, const GLfloat *value) override {
        glUniformMatrix2fv(location, count, transpose, value);
    }
    void UniformMatrix3fv(GLint location, GLsizei count,
                          GLboolean transpose, const GLfloat *value) override {
        glUniformMatrix3fv(location, count, transpose, value);
    }
    void UniformMatrix4fv(GLint location, GLsizei count,
                          GLboolean transpose, const GLfloat *value) override {
        glUniformMatrix4fv(location, count, transpose, value);
    }

    // skip: glUniformMatrix2x3fv ~ glUniformMatrix4x3fv

    GLint GetUniformLocation(GLuint program, const GLchar *name) override {
        return glGetUniformLocation(program, name);
    }



    /**
      * Shader
      */

     void AttachShader(GLuint program, GLuint shader) override {
          glAttachShader(program, shader);
     }
     void CompileShader(GLuint shader) override {
          glCompileShader(shader);
     }
     GLuint CreateShader(GLenum shaderType) override {
          return glCreateShader(shaderType);
     }
     void DeleteShader(GLuint shader) override {
          glDeleteShader(shader);
     }
     void GetShaderInfoLog(GLuint shader, GLsizei maxLength,
                           GLsizei *length, GLchar *infoLog) override {
          glGetShaderInfoLog(shader, maxLength, length, infoLog);
     }
     void GetShaderiv(GLuint shader, GLenum pname, GLint *params) override {
          glGetShaderiv(shader, pname, params);
     }
     void ShaderSource(GLuint shader, GLsizei count,
                       const GLchar **string, const GLint *length) override {
          glShaderSource(shader, count, string, length);
     }



    /**
     * Program
     */

    GLuint CreateProgram() override {
        return glCreateProgram();
    }
    void DeleteProgram(GLuint program) override {
        glDeleteProgram(program);
    }
    void GetProgramInfoLog(GLuint program, GLsizei maxLength,
                           GLsizei *length, GLchar *infoLog) override {
        glGetProgramInfoLog(program, maxLength, length, infoLog);
    }
    void GetProgramiv(GLuint program, GLenum pname, GLint *params) override {
        glGetProgramiv(program, pname, params);
    }
    void LinkProgram(GLuint program) override {
        glLinkProgram(program);
    }
    void UseProgram(GLuint program) override {
        glUseProgram(program);
    }



    /**
      * Vertex Arrays
      */

    void DrawArrays(GLenum mode, GLint first, GLsizei count) override {
        glDrawArrays(mode, first, count);
    }
    void EnableVertexAttribArray(GLuint index) override {
        glEnableVertexAttribArray(index);
    }
    void VertexAttribPointer(GLuint index, GLint size, GLenum type,
                                                GLboolean normalized, GLsizei stride,
                                                const void *pointer) override {
        glVertexAttribPointer(index, size, type, normalized, stride, pointer);
    }

    /// @note OpenGL 3.0~
    void BindVertexArray(GLuint array) override {
        glBindVertexArray(array);
    }
    /// @note OpenGL 3.0~
    void DeleteVertexArrays(GLsizei n, const GLuint *arrays) override {
        glDeleteVertexArrays(n, arrays);
    }
    /// @note OpenGL 3.0~
    void GenVertexArrays(GLsizei n, GLuint *arrays) override {
        glGenVertexArrays(n, arrays);
    }

    /// @note OpenGL 4.0~
    void PatchParameteri(GLenum pname, GLint value) override {
        glPatchParameteri(pname, value);
    }



    /**
     * Buffers
     */

    void BindBuffer(GLenum target, GLuint buffer) override {
        glBindBuffer(target, buffer);
    }
    void BufferData(GLenum target, GLsizeiptr size,
                    const void *data, GLenum usage) override {
        glBufferData(target, size, data, usage);
    }
    void DeleteBuffers(GLsizei n, const GLuint *buffers) override {
        glDeleteBuffers(n, buffers);
    }
    void GenBuffers(GLsizei n, GLuint *buffers) override {
        glGenBuffers(n, buffers);
    }



    /**
     * Others
     */

    void Enable(GLenum cap) override {
        glEnable(cap);
    }
    void Disable(GLenum cap) override {
        glDisable(cap);
    }
    void Clear(GLbitfield mask) override {
        glClear(mask);
    }
    void ClearColor(GLfloat red, GLfloat green,
                    GLfloat blue, GLfloat alpha) override {
        glClearColor(red, green, blue, alpha);
    }
    void Viewport(GLint x, GLint y,
                  GLsizei width, GLsizei height) override {
        glViewport(x, y, width, height);
    }
    void LineWidth(GLfloat width) override {
        glLineWidth(width);
    }
    void PointSize(GLfloat size) override {
        glPointSize(size);
    }
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CORE_OPEN_GL_H_
