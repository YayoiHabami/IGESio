/**
 * @file tests/graphics/mock_open_gl.h
 * @brief IOpenGLの記録用モック (GL文脈なしで描画ロジックを検証する)
 * @author Yayoi Habami
 * @date 2026-05-29
 * @copyright 2026 Yayoi Habami
 * @note 実際のGL呼び出しは行わず、要所 (uniform設定/draw呼び出し/プログラム使用) を記録する.
 *       CreateXx/GenXxは非ゼロのIDを返し、コンパイル/リンク状態は常に成功 (GL_TRUE) とすることで、
 *       GL文脈なしで`EntityRenderer::Initialize()`や`AddEntity()`を通せる.
 *       本ファイルは`i_open_gl.h`(glad/gl.hを含む)を最初にインクルードすること.
 */
#ifndef TESTS_GRAPHICS_MOCK_OPEN_GL_H_
#define TESTS_GRAPHICS_MOCK_OPEN_GL_H_

#include <algorithm>
#include <array>
#include <string>
#include <unordered_map>
#include <vector>

#include "igesio/graphics/core/i_open_gl.h"



namespace igesio::graphics::test {

/// @brief 描画ロジック検証用のIOpenGLモック
/// @note GL状態は変更せず、検証に使う呼び出しのみ公開メンバへ記録する.
class MockOpenGL : public IOpenGL {
 public:
    /// @brief uniform名 -> 直近のUniformMatrix4fv値 (16要素・列優先)
    std::unordered_map<std::string, std::array<float, 16>> last_matrix_by_name;
    /// @brief uniform名 -> 直近のUniformNfv値
    std::unordered_map<std::string, std::vector<float>> last_vec_by_name;
    /// @brief UseProgramで使用されたプログラムIDの列
    std::vector<GLuint> used_programs;
    /// @brief LineWidthに渡された値の列
    std::vector<GLfloat> line_widths;
    /// @brief DrawArraysの呼び出し回数
    int draw_arrays_calls = 0;
    /// @brief DrawElementsの呼び出し回数
    int draw_elements_calls = 0;



    /**
     * glUniform (fv/matrixのみ記録. 他はno-op)
     */

    void Uniform1i(GLint, GLint) override {}
    void Uniform2i(GLint, GLint, GLint) override {}
    void Uniform3i(GLint, GLint, GLint, GLint) override {}
    void Uniform4i(GLint, GLint, GLint, GLint, GLint) override {}
    void Uniform1f(GLint, GLfloat) override {}
    void Uniform2f(GLint, GLfloat, GLfloat) override {}
    void Uniform3f(GLint, GLfloat, GLfloat, GLfloat) override {}
    void Uniform4f(GLint, GLfloat, GLfloat, GLfloat, GLfloat) override {}

    void Uniform1fv(GLint location, GLsizei count, const GLfloat* value) override {
        RecordVec(location, value, count * 1);
    }
    void Uniform2fv(GLint location, GLsizei count, const GLfloat* value) override {
        RecordVec(location, value, count * 2);
    }
    void Uniform3fv(GLint location, GLsizei count, const GLfloat* value) override {
        RecordVec(location, value, count * 3);
    }
    void Uniform4fv(GLint location, GLsizei count, const GLfloat* value) override {
        RecordVec(location, value, count * 4);
    }

    void UniformMatrix2fv(GLint, GLsizei, GLboolean, const GLfloat*) override {}
    void UniformMatrix3fv(GLint, GLsizei, GLboolean, const GLfloat*) override {}
    void UniformMatrix4fv(GLint location, GLsizei, GLboolean,
                          const GLfloat* value) override {
        RecordMatrix(location, value);
    }

    GLint GetUniformLocation(GLuint, const GLchar* name) override {
        return LocationOf(name);
    }



    /**
     * Shader (コンパイルは常に成功扱い)
     */

    void AttachShader(GLuint, GLuint) override {}
    void CompileShader(GLuint) override {}
    GLuint CreateShader(GLenum) override { return next_id_++; }
    void DeleteShader(GLuint) override {}
    void GetShaderInfoLog(GLuint, GLsizei, GLsizei* length, GLchar*) override {
        if (length) *length = 0;
    }
    void GetShaderiv(GLuint, GLenum, GLint* params) override {
        if (params) *params = GL_TRUE;
    }
    void ShaderSource(GLuint, GLsizei, const GLchar**, const GLint*) override {}



    /**
     * Program (リンクは常に成功扱い)
     */

    GLuint CreateProgram() override { return next_id_++; }
    void DeleteProgram(GLuint) override {}
    void GetProgramInfoLog(GLuint, GLsizei, GLsizei* length, GLchar*) override {
        if (length) *length = 0;
    }
    void GetProgramiv(GLuint, GLenum, GLint* params) override {
        if (params) *params = GL_TRUE;
    }
    void LinkProgram(GLuint) override {}
    void UseProgram(GLuint program) override { used_programs.push_back(program); }



    /**
     * Vertex Arrays
     */

    void DrawArrays(GLenum, GLint, GLsizei) override { ++draw_arrays_calls; }
    void EnableVertexAttribArray(GLuint) override {}
    void DisableVertexAttribArray(GLuint) override {}
    void VertexAttribPointer(GLuint, GLint, GLenum, GLboolean, GLsizei,
                             const void*) override {}
    void BindVertexArray(GLuint) override {}
    void DeleteVertexArrays(GLsizei, const GLuint*) override {}
    void GenVertexArrays(GLsizei n, GLuint* arrays) override { FillIds(n, arrays); }
    void PatchParameteri(GLenum, GLint) override {}



    /**
     * Buffers
     */

    void BindBuffer(GLenum, GLuint) override {}
    void BindBufferBase(GLenum, GLuint, GLuint) override {}
    void BufferData(GLenum, GLsizeiptr, const void*, GLenum) override {}
    void DeleteBuffers(GLsizei, const GLuint*) override {}
    void GenBuffers(GLsizei n, GLuint* buffers) override { FillIds(n, buffers); }



    /**
     * Textures
     */

    void ActiveTexture(GLenum) override {}
    void BindTexture(GLenum, GLuint) override {}
    void DeleteTextures(GLsizei, const GLuint*) override {}
    void GenerateMipmap(GLenum) override {}
    void GenTextures(GLsizei n, GLuint* textures) override { FillIds(n, textures); }
    void TexImage2D(GLenum, GLint, GLint, GLsizei, GLsizei, GLint,
                    GLenum, GLenum, const void*) override {}
    void TexParameteri(GLenum, GLenum, GLint) override {}



    /**
     * Offscreen Rendering
     */

    void BindFramebuffer(GLenum, GLuint) override {}
    GLenum CheckFramebufferStatus(GLenum) override {
        return GL_FRAMEBUFFER_COMPLETE;
    }
    void DeleteFramebuffers(GLsizei, const GLuint*) override {}
    void GenFramebuffers(GLsizei n, GLuint* framebuffers) override {
        FillIds(n, framebuffers);
    }
    void FramebufferTexture2D(GLenum, GLenum, GLenum, GLuint, GLint) override {}
    GLenum BindRenderbuffer(GLenum, GLuint) override { return GL_NO_ERROR; }
    void DeleteRenderbuffers(GLsizei, const GLuint*) override {}
    void GenRenderbuffers(GLsizei n, GLuint* renderbuffers) override {
        FillIds(n, renderbuffers);
    }
    void RenderbufferStorage(GLenum, GLenum, GLsizei, GLsizei) override {}
    void FramebufferRenderbuffer(GLenum, GLenum, GLenum, GLuint) override {}



    /**
     * Others
     */

    void Enable(GLenum) override {}
    void Disable(GLenum) override {}
    void BlendFunc(GLenum, GLenum) override {}
    void Clear(GLbitfield) override {}
    void ClearColor(GLfloat, GLfloat, GLfloat, GLfloat) override {}
    void DrawElements(GLenum, GLsizei, GLenum, const void*) override {
        ++draw_elements_calls;
    }
    void Viewport(GLint, GLint, GLsizei, GLsizei) override {}
    void GetIntegerv(GLenum pname, GLint* data) override {
        if (!data) return;
        // GetCurrentViewport (GL_VIEWPORT) は4要素を要求する
        if (pname == GL_VIEWPORT) {
            data[0] = data[1] = data[2] = data[3] = 0;
        } else {
            data[0] = 0;
        }
    }
    void LineWidth(GLfloat width) override { line_widths.push_back(width); }
    void PointSize(GLfloat) override {}
    void ReadPixels(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum,
                    void*) override {}
    GLenum GetError() override { return GL_NO_ERROR; }



 private:
    /// @brief 次に払い出すオブジェクトID (0は無効IDのため1から開始)
    GLuint next_id_ = 1;
    /// @brief 次に払い出すuniform location
    GLint next_loc_ = 0;
    /// @brief uniform名 -> location
    std::unordered_map<std::string, GLint> name_to_loc_;
    /// @brief location -> uniform名 (記録時の逆引き)
    std::unordered_map<GLint, std::string> loc_to_name_;

    /// @brief uniform名にlocationを採番する (既出なら同じ値を返す)
    GLint LocationOf(const GLchar* name) {
        const std::string key = (name != nullptr) ? std::string(name) : std::string();
        auto it = name_to_loc_.find(key);
        if (it != name_to_loc_.end()) return it->second;
        const GLint loc = next_loc_++;
        name_to_loc_[key] = loc;
        loc_to_name_[loc] = key;
        return loc;
    }

    /// @brief n個のIDを非0で充填する
    void FillIds(GLsizei n, GLuint* out) {
        if (!out) return;
        for (GLsizei i = 0; i < n; ++i) out[i] = next_id_++;
    }

    /// @brief location -> uniform名を引いて、ベクトル値を記録する
    void RecordVec(GLint location, const GLfloat* value, int count) {
        if (value == nullptr || count <= 0) return;
        auto it = loc_to_name_.find(location);
        if (it == loc_to_name_.end()) return;
        last_vec_by_name[it->second] = std::vector<float>(value, value + count);
    }

    /// @brief location -> uniform名を引いて、4x4行列値を記録する
    void RecordMatrix(GLint location, const GLfloat* value) {
        if (value == nullptr) return;
        auto it = loc_to_name_.find(location);
        if (it == loc_to_name_.end()) return;
        std::array<float, 16> m{};
        std::copy(value, value + 16, m.begin());
        last_matrix_by_name[it->second] = m;
    }
};

}  // namespace igesio::graphics::test

#endif  // TESTS_GRAPHICS_MOCK_OPEN_GL_H_
