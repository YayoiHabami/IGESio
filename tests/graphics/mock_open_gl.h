/**
 * @file tests/graphics/mock_open_gl.h
 * @brief IOpenGLの記録用モック (GL文脈なしで描画ロジックを検証する)
 * @author Yayoi Habami
 * @date 2026-05-29
 * @copyright 2026 Yayoi Habami
 * @note 実際のGL呼び出しは行わず、要所 (uniform設定/draw呼び出し/プログラム使用) を記録する.
 *       CreateXx/GenXxは非ゼロのIDを返し、コンパイル/リンク状態は常に成功 (gl::kTrue) とすることで、
 *       GL文脈なしで`EntityRenderer::Initialize()`や`AddEntity()`を通せる.
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
    std::vector<gl::Uint> used_programs;
    /// @brief LineWidthに渡された値の列
    std::vector<gl::Float> line_widths;
    /// @brief DrawArraysの呼び出し回数
    int draw_arrays_calls = 0;
    /// @brief DrawElementsの呼び出し回数
    int draw_elements_calls = 0;



    /**
     * glUniform (fv/matrixのみ記録. 他はno-op)
     */

    void Uniform1i(gl::Int, gl::Int) override {}
    void Uniform2i(gl::Int, gl::Int, gl::Int) override {}
    void Uniform3i(gl::Int, gl::Int, gl::Int, gl::Int) override {}
    void Uniform4i(gl::Int, gl::Int, gl::Int, gl::Int, gl::Int) override {}
    void Uniform1f(gl::Int, gl::Float) override {}
    void Uniform2f(gl::Int, gl::Float, gl::Float) override {}
    void Uniform3f(gl::Int, gl::Float, gl::Float, gl::Float) override {}
    void Uniform4f(gl::Int, gl::Float, gl::Float, gl::Float, gl::Float) override {}

    void Uniform1fv(gl::Int location, gl::Sizei count, const gl::Float* value) override {
        RecordVec(location, value, count * 1);
    }
    void Uniform2fv(gl::Int location, gl::Sizei count, const gl::Float* value) override {
        RecordVec(location, value, count * 2);
    }
    void Uniform3fv(gl::Int location, gl::Sizei count, const gl::Float* value) override {
        RecordVec(location, value, count * 3);
    }
    void Uniform4fv(gl::Int location, gl::Sizei count, const gl::Float* value) override {
        RecordVec(location, value, count * 4);
    }

    void UniformMatrix2fv(gl::Int, gl::Sizei, gl::Boolean, const gl::Float*) override {}
    void UniformMatrix3fv(gl::Int, gl::Sizei, gl::Boolean, const gl::Float*) override {}
    void UniformMatrix4fv(gl::Int location, gl::Sizei, gl::Boolean,
                          const gl::Float* value) override {
        RecordMatrix(location, value);
    }

    gl::Int GetUniformLocation(gl::Uint, const gl::Char* name) override {
        return LocationOf(name);
    }



    /**
     * Shader (コンパイルは常に成功扱い)
     */

    void AttachShader(gl::Uint, gl::Uint) override {}
    void CompileShader(gl::Uint) override {}
    gl::Uint CreateShader(gl::Enum) override { return next_id_++; }
    void DeleteShader(gl::Uint) override {}
    void GetShaderInfoLog(gl::Uint, gl::Sizei, gl::Sizei* length, gl::Char*) override {
        if (length) *length = 0;
    }
    void GetShaderiv(gl::Uint, gl::Enum, gl::Int* params) override {
        if (params) *params = gl::kTrue;
    }
    void ShaderSource(gl::Uint, gl::Sizei, const gl::Char**, const gl::Int*) override {}



    /**
     * Program (リンクは常に成功扱い)
     */

    gl::Uint CreateProgram() override { return next_id_++; }
    void DeleteProgram(gl::Uint) override {}
    void GetProgramInfoLog(gl::Uint, gl::Sizei, gl::Sizei* length, gl::Char*) override {
        if (length) *length = 0;
    }
    void GetProgramiv(gl::Uint, gl::Enum, gl::Int* params) override {
        if (params) *params = gl::kTrue;
    }
    void LinkProgram(gl::Uint) override {}
    void UseProgram(gl::Uint program) override { used_programs.push_back(program); }



    /**
     * Vertex Arrays
     */

    void DrawArrays(gl::Enum, gl::Int, gl::Sizei) override { ++draw_arrays_calls; }
    void EnableVertexAttribArray(gl::Uint) override {}
    void DisableVertexAttribArray(gl::Uint) override {}
    void VertexAttribPointer(gl::Uint, gl::Int, gl::Enum, gl::Boolean, gl::Sizei,
                             const void*) override {}
    void BindVertexArray(gl::Uint) override {}
    void DeleteVertexArrays(gl::Sizei, const gl::Uint*) override {}
    void GenVertexArrays(gl::Sizei n, gl::Uint* arrays) override { FillIds(n, arrays); }
    void PatchParameteri(gl::Enum, gl::Int) override {}



    /**
     * Buffers
     */

    void BindBuffer(gl::Enum, gl::Uint) override {}
    void BindBufferBase(gl::Enum, gl::Uint, gl::Uint) override {}
    void BufferData(gl::Enum, gl::Sizeiptr, const void*, gl::Enum) override {}
    void DeleteBuffers(gl::Sizei, const gl::Uint*) override {}
    void GenBuffers(gl::Sizei n, gl::Uint* buffers) override { FillIds(n, buffers); }



    /**
     * Textures
     */

    void ActiveTexture(gl::Enum) override {}
    void BindTexture(gl::Enum, gl::Uint) override {}
    void DeleteTextures(gl::Sizei, const gl::Uint*) override {}
    void GenerateMipmap(gl::Enum) override {}
    void GenTextures(gl::Sizei n, gl::Uint* textures) override { FillIds(n, textures); }
    void TexImage2D(gl::Enum, gl::Int, gl::Int, gl::Sizei, gl::Sizei, gl::Int,
                    gl::Enum, gl::Enum, const void*) override {}
    void TexParameteri(gl::Enum, gl::Enum, gl::Int) override {}



    /**
     * Offscreen Rendering
     */

    void BindFramebuffer(gl::Enum, gl::Uint) override {}
    gl::Enum CheckFramebufferStatus(gl::Enum) override {
        return gl::kFramebufferComplete;
    }
    void DeleteFramebuffers(gl::Sizei, const gl::Uint*) override {}
    void GenFramebuffers(gl::Sizei n, gl::Uint* framebuffers) override {
        FillIds(n, framebuffers);
    }
    void FramebufferTexture2D(gl::Enum, gl::Enum, gl::Enum, gl::Uint, gl::Int) override {}
    gl::Enum BindRenderbuffer(gl::Enum, gl::Uint) override { return gl::kNoError; }
    void DeleteRenderbuffers(gl::Sizei, const gl::Uint*) override {}
    void GenRenderbuffers(gl::Sizei n, gl::Uint* renderbuffers) override {
        FillIds(n, renderbuffers);
    }
    void RenderbufferStorage(gl::Enum, gl::Enum, gl::Sizei, gl::Sizei) override {}
    void FramebufferRenderbuffer(gl::Enum, gl::Enum, gl::Enum, gl::Uint) override {}



    /**
     * Others
     */

    void Enable(gl::Enum) override {}
    void Disable(gl::Enum) override {}
    void BlendFunc(gl::Enum, gl::Enum) override {}
    void Clear(gl::Bitfield) override {}
    void ClearColor(gl::Float, gl::Float, gl::Float, gl::Float) override {}
    void DrawElements(gl::Enum, gl::Sizei, gl::Enum, const void*) override {
        ++draw_elements_calls;
    }
    void Viewport(gl::Int, gl::Int, gl::Sizei, gl::Sizei) override {}
    void GetIntegerv(gl::Enum pname, gl::Int* data) override {
        if (!data) return;
        // GetCurrentViewport (gl::kViewport) は4要素を要求する
        if (pname == gl::kViewport) {
            data[0] = data[1] = data[2] = data[3] = 0;
        } else {
            data[0] = 0;
        }
    }
    void LineWidth(gl::Float width) override { line_widths.push_back(width); }
    void PointSize(gl::Float) override {}
    void PolygonOffset(gl::Float, gl::Float) override {}
    void DepthRange(gl::Double, gl::Double) override {}
    void ReadPixels(gl::Int, gl::Int, gl::Sizei, gl::Sizei, gl::Enum, gl::Enum,
                    void*) override {}
    gl::Enum GetError() override { return gl::kNoError; }



 private:
    /// @brief 次に払い出すオブジェクトID (0は無効IDのため1から開始)
    gl::Uint next_id_ = 1;
    /// @brief 次に払い出すuniform location
    gl::Int next_loc_ = 0;
    /// @brief uniform名 -> location
    std::unordered_map<std::string, gl::Int> name_to_loc_;
    /// @brief location -> uniform名 (記録時の逆引き)
    std::unordered_map<gl::Int, std::string> loc_to_name_;

    /// @brief uniform名にlocationを採番する (既出なら同じ値を返す)
    gl::Int LocationOf(const gl::Char* name) {
        const std::string key = (name != nullptr) ? std::string(name) : std::string();
        auto it = name_to_loc_.find(key);
        if (it != name_to_loc_.end()) return it->second;
        const gl::Int loc = next_loc_++;
        name_to_loc_[key] = loc;
        loc_to_name_[loc] = key;
        return loc;
    }

    /// @brief n個のIDを非0で充填する
    void FillIds(gl::Sizei n, gl::Uint* out) {
        if (!out) return;
        for (gl::Sizei i = 0; i < n; ++i) out[i] = next_id_++;
    }

    /// @brief location -> uniform名を引いて、ベクトル値を記録する
    void RecordVec(gl::Int location, const gl::Float* value, int count) {
        if (value == nullptr || count <= 0) return;
        auto it = loc_to_name_.find(location);
        if (it == loc_to_name_.end()) return;
        last_vec_by_name[it->second] = std::vector<float>(value, value + count);
    }

    /// @brief location -> uniform名を引いて、4x4行列値を記録する
    void RecordMatrix(gl::Int location, const gl::Float* value) {
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
