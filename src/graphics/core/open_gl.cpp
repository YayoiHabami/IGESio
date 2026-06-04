/**
 * @file graphics/core/open_gl.cpp
 * @brief OpenGL関数をラップする具象クラスとファクトリの実装
 * @author Yayoi Habami
 * @date 2026-06-02
 * @copyright 2026 Yayoi Habami
 * @note gladをインクルードする「唯一の翻訳単位」.
 *       MX (マルチコンテキスト) 生成された`GladGLContext`をメンバに持ち、
 *       コンテキスト固有の関数ポインタ経由でGL関数を呼ぶ.
 *       gl_types.hのABI定数値とglad (`GL_*`) の一致を`static_assert`で検証する.
 */
#include "igesio/graphics/core/gl_backend.h"

#include <memory>
#include <string>
#include <utility>

#include <glad/gl.h>

#include "igesio/common/errors.h"
#include "igesio/graphics/core/gl_types.h"
#include "igesio/graphics/core/i_open_gl.h"



namespace igesio::graphics {

namespace {

/// @brief gl_types.hの定数値がglad (`GL_*`) と一致することの検証
/// @note 値がずれると本ファイルのコンパイルで露見する (ドリフト検出)
static_assert(gl::kFalse == GL_FALSE, "gl::kFalse mismatch");
static_assert(gl::kTrue == GL_TRUE, "gl::kTrue mismatch");
static_assert(gl::kNoError == GL_NO_ERROR, "gl::kNoError mismatch");
static_assert(gl::kPoints == GL_POINTS, "gl::kPoints mismatch");
static_assert(gl::kLines == GL_LINES, "gl::kLines mismatch");
static_assert(gl::kLineLoop == GL_LINE_LOOP, "gl::kLineLoop mismatch");
static_assert(gl::kLineStrip == GL_LINE_STRIP, "gl::kLineStrip mismatch");
static_assert(gl::kTriangles == GL_TRIANGLES, "gl::kTriangles mismatch");
static_assert(gl::kPatches == GL_PATCHES, "gl::kPatches mismatch");
static_assert(gl::kUnsignedByte == GL_UNSIGNED_BYTE, "gl::kUnsignedByte mismatch");
static_assert(gl::kUnsignedInt == GL_UNSIGNED_INT, "gl::kUnsignedInt mismatch");
static_assert(gl::kFloat == GL_FLOAT, "gl::kFloat mismatch");
static_assert(gl::kRgb == GL_RGB, "gl::kRgb mismatch");
static_assert(gl::kRgba == GL_RGBA, "gl::kRgba mismatch");
static_assert(gl::kTexture2D == GL_TEXTURE_2D, "gl::kTexture2D mismatch");
static_assert(gl::kTexture0 == GL_TEXTURE0, "gl::kTexture0 mismatch");
static_assert(gl::kLinear == GL_LINEAR, "gl::kLinear mismatch");
static_assert(gl::kTextureMagFilter == GL_TEXTURE_MAG_FILTER, "gl::kTextureMagFilter mismatch");
static_assert(gl::kTextureMinFilter == GL_TEXTURE_MIN_FILTER, "gl::kTextureMinFilter mismatch");
static_assert(gl::kTextureWrapS == GL_TEXTURE_WRAP_S, "gl::kTextureWrapS mismatch");
static_assert(gl::kTextureWrapT == GL_TEXTURE_WRAP_T, "gl::kTextureWrapT mismatch");
static_assert(gl::kClampToEdge == GL_CLAMP_TO_EDGE, "gl::kClampToEdge mismatch");
static_assert(gl::kArrayBuffer == GL_ARRAY_BUFFER, "gl::kArrayBuffer mismatch");
static_assert(gl::kElementArrayBuffer == GL_ELEMENT_ARRAY_BUFFER, "gl::kElementArrayBuffer mismatch");
static_assert(gl::kShaderStorageBuffer == GL_SHADER_STORAGE_BUFFER, "gl::kShaderStorageBuffer mismatch");
static_assert(gl::kStaticDraw == GL_STATIC_DRAW, "gl::kStaticDraw mismatch");
static_assert(gl::kDepthTest == GL_DEPTH_TEST, "gl::kDepthTest mismatch");
static_assert(gl::kBlend == GL_BLEND, "gl::kBlend mismatch");
static_assert(gl::kMultisample == GL_MULTISAMPLE, "gl::kMultisample mismatch");
static_assert(gl::kProgramPointSize == GL_PROGRAM_POINT_SIZE, "gl::kProgramPointSize mismatch");
static_assert(gl::kSrcAlpha == GL_SRC_ALPHA, "gl::kSrcAlpha mismatch");
static_assert(gl::kOneMinusSrcAlpha == GL_ONE_MINUS_SRC_ALPHA, "gl::kOneMinusSrcAlpha mismatch");
static_assert(gl::kColorBufferBit == GL_COLOR_BUFFER_BIT, "gl::kColorBufferBit mismatch");
static_assert(gl::kDepthBufferBit == GL_DEPTH_BUFFER_BIT, "gl::kDepthBufferBit mismatch");
static_assert(gl::kPatchVertices == GL_PATCH_VERTICES, "gl::kPatchVertices mismatch");
static_assert(gl::kVertexShader == GL_VERTEX_SHADER, "gl::kVertexShader mismatch");
static_assert(gl::kFragmentShader == GL_FRAGMENT_SHADER, "gl::kFragmentShader mismatch");
static_assert(gl::kGeometryShader == GL_GEOMETRY_SHADER, "gl::kGeometryShader mismatch");
static_assert(gl::kTessControlShader == GL_TESS_CONTROL_SHADER, "gl::kTessControlShader mismatch");
static_assert(gl::kTessEvaluationShader == GL_TESS_EVALUATION_SHADER, "gl::kTessEvaluationShader mismatch");
static_assert(gl::kCompileStatus == GL_COMPILE_STATUS, "gl::kCompileStatus mismatch");
static_assert(gl::kLinkStatus == GL_LINK_STATUS, "gl::kLinkStatus mismatch");
static_assert(gl::kFramebuffer == GL_FRAMEBUFFER, "gl::kFramebuffer mismatch");
static_assert(gl::kRenderbuffer == GL_RENDERBUFFER, "gl::kRenderbuffer mismatch");
static_assert(gl::kColorAttachment0 == GL_COLOR_ATTACHMENT0, "gl::kColorAttachment0 mismatch");
static_assert(gl::kDepthStencilAttachment == GL_DEPTH_STENCIL_ATTACHMENT, "gl::kDepthStencilAttachment mismatch");
static_assert(gl::kDepth24Stencil8 == GL_DEPTH24_STENCIL8, "gl::kDepth24Stencil8 mismatch");
static_assert(gl::kFramebufferComplete == GL_FRAMEBUFFER_COMPLETE, "gl::kFramebufferComplete mismatch");
static_assert(gl::kViewport == GL_VIEWPORT, "gl::kViewport mismatch");
static_assert(gl::kFramebufferBinding == GL_FRAMEBUFFER_BINDING, "gl::kFramebufferBinding mismatch");

// 型のABI互換性 (gl::型とglad型のサイズ一致)
static_assert(sizeof(gl::Uint) == sizeof(GLuint), "gl::Uint size mismatch");
static_assert(sizeof(gl::Int) == sizeof(GLint), "gl::Int size mismatch");
static_assert(sizeof(gl::Enum) == sizeof(GLenum), "gl::Enum size mismatch");
static_assert(sizeof(gl::Sizei) == sizeof(GLsizei), "gl::Sizei size mismatch");
static_assert(sizeof(gl::Float) == sizeof(GLfloat), "gl::Float size mismatch");
static_assert(sizeof(gl::Boolean) == sizeof(GLboolean), "gl::Boolean size mismatch");
static_assert(sizeof(gl::Bitfield) == sizeof(GLbitfield), "gl::Bitfield size mismatch");
static_assert(sizeof(gl::Sizeiptr) == sizeof(GLsizeiptr), "gl::Sizeiptr size mismatch");



/// @brief graphicsモジュールが要求する最低GLメジャーバージョン
/// @note 曲線描画のテッセレーション (GL 4.0) と、NURBS曲線・曲面の
///       パラメータ転送に用いるSSBO (GL 4.3) に依存するため、
///       4.3 core以上のコンテキストを必須とする.
constexpr gl::Int kMinGLMajorVersion = 4;
/// @brief graphicsモジュールが要求する最低GLマイナーバージョン
constexpr gl::Int kMinGLMinorVersion = 3;



/// @brief OpenGLの関数をラップする具象クラス
/// @note MX生成された`GladGLContext`をメンバに持ち、各メソッドは`ctx_.Xxx(...)`を呼ぶ.
///       グローバルな`glXxx`は使用しない (コンテキスト固有の関数ポインタを用いる).
class OpenGL : public IOpenGL {
 public:
    /// @brief デストラクタ
    ~OpenGL() override = default;

    /// @brief コンテキストへ関数ポインタをロードする
    /// @param loader getProcAddress互換のローダ
    /// @return 成功した場合はtrue
    /// @note 呼び出し時にOpenGLコンテキストをカレントにしておくこと
    bool Load(GLProcLoader loader) {
        return gladLoadGLContext(
                &ctx_, reinterpret_cast<GLADloadfunc>(loader)) != 0;
    }

    /// @brief カレントコンテキストのGLバージョンを取得する
    /// @return {メジャーバージョン, マイナーバージョン}
    /// @note `Load`の成功後に呼ぶこと
    std::pair<gl::Int, gl::Int> GetContextVersion() {
        gl::Int major = 0;
        gl::Int minor = 0;
        ctx_.GetIntegerv(GL_MAJOR_VERSION, &major);
        ctx_.GetIntegerv(GL_MINOR_VERSION, &minor);
        return {major, minor};
    }



    /**
     * glUniform
     */

    void Uniform1i(gl::Int location, gl::Int v0) override {
        ctx_.Uniform1i(location, v0);
    }
    void Uniform2i(gl::Int location, gl::Int v0, gl::Int v1) override {
        ctx_.Uniform2i(location, v0, v1);
    }
    void Uniform3i(gl::Int location, gl::Int v0, gl::Int v1, gl::Int v2) override {
        ctx_.Uniform3i(location, v0, v1, v2);
    }
    void Uniform4i(gl::Int location, gl::Int v0, gl::Int v1, gl::Int v2, gl::Int v3) override {
        ctx_.Uniform4i(location, v0, v1, v2, v3);
    }

    void Uniform1f(gl::Int location, gl::Float v0) override {
        ctx_.Uniform1f(location, v0);
    }
    void Uniform2f(gl::Int location, gl::Float v0, gl::Float v1) override {
        ctx_.Uniform2f(location, v0, v1);
    }
    void Uniform3f(gl::Int location, gl::Float v0, gl::Float v1, gl::Float v2) override {
        ctx_.Uniform3f(location, v0, v1, v2);
    }
    void Uniform4f(gl::Int location, gl::Float v0,
                   gl::Float v1, gl::Float v2, gl::Float v3) override {
        ctx_.Uniform4f(location, v0, v1, v2, v3);
    }

    void Uniform1fv(gl::Int location, gl::Sizei count, const gl::Float *value) override {
        ctx_.Uniform1fv(location, count, value);
    }
    void Uniform2fv(gl::Int location, gl::Sizei count, const gl::Float *value) override {
        ctx_.Uniform2fv(location, count, value);
    }
    void Uniform3fv(gl::Int location, gl::Sizei count, const gl::Float *value) override {
        ctx_.Uniform3fv(location, count, value);
    }
    void Uniform4fv(gl::Int location, gl::Sizei count, const gl::Float *value) override {
        ctx_.Uniform4fv(location, count, value);
    }

    void UniformMatrix2fv(gl::Int location, gl::Sizei count,
                          gl::Boolean transpose, const gl::Float *value) override {
        ctx_.UniformMatrix2fv(location, count, transpose, value);
    }
    void UniformMatrix3fv(gl::Int location, gl::Sizei count,
                          gl::Boolean transpose, const gl::Float *value) override {
        ctx_.UniformMatrix3fv(location, count, transpose, value);
    }
    void UniformMatrix4fv(gl::Int location, gl::Sizei count,
                          gl::Boolean transpose, const gl::Float *value) override {
        ctx_.UniformMatrix4fv(location, count, transpose, value);
    }

    gl::Int GetUniformLocation(gl::Uint program, const gl::Char *name) override {
        return ctx_.GetUniformLocation(program, name);
    }



    /**
     * Shader
     */

    void AttachShader(gl::Uint program, gl::Uint shader) override {
        ctx_.AttachShader(program, shader);
    }
    void CompileShader(gl::Uint shader) override {
        ctx_.CompileShader(shader);
    }
    gl::Uint CreateShader(gl::Enum shaderType) override {
        return ctx_.CreateShader(shaderType);
    }
    void DeleteShader(gl::Uint shader) override {
        ctx_.DeleteShader(shader);
    }
    void GetShaderInfoLog(gl::Uint shader, gl::Sizei maxLength,
                          gl::Sizei *length, gl::Char *infoLog) override {
        ctx_.GetShaderInfoLog(shader, maxLength, length, infoLog);
    }
    void GetShaderiv(gl::Uint shader, gl::Enum pname, gl::Int *params) override {
        ctx_.GetShaderiv(shader, pname, params);
    }
    void ShaderSource(gl::Uint shader, gl::Sizei count,
                      const gl::Char **string, const gl::Int *length) override {
        ctx_.ShaderSource(shader, count, string, length);
    }



    /**
     * Program
     */

    gl::Uint CreateProgram() override {
        return ctx_.CreateProgram();
    }
    void DeleteProgram(gl::Uint program) override {
        ctx_.DeleteProgram(program);
    }
    void GetProgramInfoLog(gl::Uint program, gl::Sizei maxLength,
                           gl::Sizei *length, gl::Char *infoLog) override {
        ctx_.GetProgramInfoLog(program, maxLength, length, infoLog);
    }
    void GetProgramiv(gl::Uint program, gl::Enum pname, gl::Int *params) override {
        ctx_.GetProgramiv(program, pname, params);
    }
    void LinkProgram(gl::Uint program) override {
        ctx_.LinkProgram(program);
    }
    void UseProgram(gl::Uint program) override {
        ctx_.UseProgram(program);
    }



    /**
     * Vertex Arrays
     */

    void DrawArrays(gl::Enum mode, gl::Int first, gl::Sizei count) override {
        ctx_.DrawArrays(mode, first, count);
    }
    void EnableVertexAttribArray(gl::Uint index) override {
        ctx_.EnableVertexAttribArray(index);
    }
    void DisableVertexAttribArray(gl::Uint index) override {
        ctx_.DisableVertexAttribArray(index);
    }
    void VertexAttribPointer(gl::Uint index, gl::Int size, gl::Enum type,
                             gl::Boolean normalized, gl::Sizei stride,
                             const void *pointer) override {
        ctx_.VertexAttribPointer(index, size, type, normalized, stride, pointer);
    }

    void BindVertexArray(gl::Uint array) override {
        ctx_.BindVertexArray(array);
    }
    void DeleteVertexArrays(gl::Sizei n, const gl::Uint *arrays) override {
        ctx_.DeleteVertexArrays(n, arrays);
    }
    void GenVertexArrays(gl::Sizei n, gl::Uint *arrays) override {
        ctx_.GenVertexArrays(n, arrays);
    }

    void PatchParameteri(gl::Enum pname, gl::Int value) override {
        ctx_.PatchParameteri(pname, value);
    }



    /**
     * Buffers
     */

    void BindBuffer(gl::Enum target, gl::Uint buffer) override {
        ctx_.BindBuffer(target, buffer);
    }
    void BindBufferBase(gl::Enum target, gl::Uint index, gl::Uint buffer) override {
        ctx_.BindBufferBase(target, index, buffer);
    }
    void BufferData(gl::Enum target, gl::Sizeiptr size,
                    const void *data, gl::Enum usage) override {
        ctx_.BufferData(target, size, data, usage);
    }
    void DeleteBuffers(gl::Sizei n, const gl::Uint *buffers) override {
        ctx_.DeleteBuffers(n, buffers);
    }
    void GenBuffers(gl::Sizei n, gl::Uint *buffers) override {
        ctx_.GenBuffers(n, buffers);
    }



    /**
     * Textures
     */

    void ActiveTexture(gl::Enum texture) override {
        ctx_.ActiveTexture(texture);
    }
    void BindTexture(gl::Enum target, gl::Uint texture) override {
        ctx_.BindTexture(target, texture);
    }
    void DeleteTextures(gl::Sizei n, const gl::Uint *textures) override {
        ctx_.DeleteTextures(n, textures);
    }
    void GenerateMipmap(gl::Enum target) override {
        ctx_.GenerateMipmap(target);
    }
    void GenTextures(gl::Sizei n, gl::Uint *textures) override {
        ctx_.GenTextures(n, textures);
    }
    void TexImage2D(gl::Enum target, gl::Int level, gl::Int internalFormat,
                    gl::Sizei width, gl::Sizei height, gl::Int border,
                    gl::Enum format, gl::Enum type, const void *data) override {
        ctx_.TexImage2D(target, level, internalFormat, width, height, border,
                        format, type, data);
    }
    void TexParameteri(gl::Enum target, gl::Enum pname, gl::Int param) override {
        ctx_.TexParameteri(target, pname, param);
    }



    /**
     * For Offscreen Rendering
     */

    void BindFramebuffer(gl::Enum target, gl::Uint framebuffer) override {
        ctx_.BindFramebuffer(target, framebuffer);
    }
    gl::Enum CheckFramebufferStatus(gl::Enum target) override {
        return ctx_.CheckFramebufferStatus(target);
    }
    void DeleteFramebuffers(gl::Sizei n, const gl::Uint *framebuffers) override {
        ctx_.DeleteFramebuffers(n, framebuffers);
    }
    void GenFramebuffers(gl::Sizei n, gl::Uint *framebuffers) override {
        ctx_.GenFramebuffers(n, framebuffers);
    }

    void FramebufferTexture2D(gl::Enum target, gl::Enum attachment,
                              gl::Enum textarget, gl::Uint texture,
                              gl::Int level) override {
        ctx_.FramebufferTexture2D(target, attachment, textarget, texture, level);
    }

    gl::Enum BindRenderbuffer(gl::Enum target, gl::Uint renderbuffer) override {
        ctx_.BindRenderbuffer(target, renderbuffer);
        // glBindRenderbufferはvoidを返すが、インターフェースはgl::Enumを要求する.
        // 互換性のためgl::kNoError相当を返す.
        return gl::kNoError;
    }
    void DeleteRenderbuffers(gl::Sizei n, const gl::Uint *renderbuffers) override {
        ctx_.DeleteRenderbuffers(n, renderbuffers);
    }
    void GenRenderbuffers(gl::Sizei n, gl::Uint *renderbuffers) override {
        ctx_.GenRenderbuffers(n, renderbuffers);
    }
    void RenderbufferStorage(gl::Enum target, gl::Enum internalformat,
                             gl::Sizei width, gl::Sizei height) override {
        ctx_.RenderbufferStorage(target, internalformat, width, height);
    }

    void FramebufferRenderbuffer(gl::Enum target, gl::Enum attachment,
                                 gl::Enum renderbuffertarget,
                                 gl::Uint renderbuffer) override {
        ctx_.FramebufferRenderbuffer(target, attachment, renderbuffertarget,
                                     renderbuffer);
    }



    /**
     * Others
     */

    void Enable(gl::Enum cap) override {
        ctx_.Enable(cap);
    }
    void Disable(gl::Enum cap) override {
        ctx_.Disable(cap);
    }
    void BlendFunc(gl::Enum sfactor, gl::Enum dfactor) override {
        ctx_.BlendFunc(sfactor, dfactor);
    }
    void Clear(gl::Bitfield mask) override {
        ctx_.Clear(mask);
    }
    void ClearColor(gl::Float red, gl::Float green,
                    gl::Float blue, gl::Float alpha) override {
        ctx_.ClearColor(red, green, blue, alpha);
    }
    void DrawElements(gl::Enum mode, gl::Sizei count,
                      gl::Enum type, const void *indices) override {
        ctx_.DrawElements(mode, count, type, indices);
    }
    void Viewport(gl::Int x, gl::Int y,
                  gl::Sizei width, gl::Sizei height) override {
        ctx_.Viewport(x, y, width, height);
    }
    void GetIntegerv(gl::Enum pname, gl::Int *data) override {
        ctx_.GetIntegerv(pname, data);
    }
    void LineWidth(gl::Float width) override {
        ctx_.LineWidth(width);
    }
    void PointSize(gl::Float size) override {
        ctx_.PointSize(size);
    }
    void ReadPixels(gl::Int x, gl::Int y, gl::Sizei width, gl::Sizei height,
                    gl::Enum format, gl::Enum type, void *data) override {
        ctx_.ReadPixels(x, y, width, height, format, type, data);
    }
    gl::Enum GetError() override { return ctx_.GetError(); }

 private:
    /// @brief MX生成されたコンテキスト固有の関数ポインタ群
    GladGLContext ctx_ = {};
};

}  // namespace



std::shared_ptr<IOpenGL> CreateGLBackend(GLProcLoader loader) {
    auto impl = std::make_shared<OpenGL>();
    if (!impl->Load(loader)) {
        throw igesio::ImplementationError(
                "Failed to load OpenGL functions via the provided loader");
    }
    // 4.3未満のコンテキストではテッセレーション (GL 4.0) やSSBO (GL 4.3) の
    // 関数ポインタがnullのままロードに成功してしまい、描画時のクラッシュに
    // つながる. ここで検出し、明確なエラーとして報告する.
    const auto [major, minor] = impl->GetContextVersion();
    if (major * 10 + minor < kMinGLMajorVersion * 10 + kMinGLMinorVersion) {
        throw igesio::ImplementationError(
                "OpenGL " + std::to_string(kMinGLMajorVersion) + "." +
                std::to_string(kMinGLMinorVersion) +
                " (core profile) or later is required for the graphics"
                " module, but the current context is OpenGL " +
                std::to_string(major) + "." + std::to_string(minor));
    }
    return impl;
}

}  // namespace igesio::graphics
