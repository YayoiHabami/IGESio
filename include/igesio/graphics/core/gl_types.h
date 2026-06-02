/**
 * @file graphics/core/gl_types.h
 * @brief OpenGLに依存しない型エイリアスとABI定数の定義
 * @author Yayoi Habami
 * @date 2026-06-02
 * @copyright 2026 Yayoi Habami
 * @note glad (OpenGLローダ) を公開APIから排除するためのヘッダ.
 *       `IOpenGL`等のインターフェースおよびライブラリ内部はこのヘッダの
 *       `gl::`型エイリアスと`gl::k*`定数のみを参照する.
 *       定数値はOpenGL仕様で固定されたABI値であり、`src/graphics/core/open_gl.cpp`
 *       (gladをインクルードする唯一の翻訳単位) の`static_assert`で`GL_*`との一致を検証する.
 */
#ifndef IGESIO_GRAPHICS_CORE_GL_TYPES_H_
#define IGESIO_GRAPHICS_CORE_GL_TYPES_H_

#include <cstddef>
#include <cstdint>



namespace igesio::graphics {

/// @brief `getProcAddress`互換のローダ関数ポインタ型
/// @note 利用側は`reinterpret_cast<GLProcLoader>(glfwGetProcAddress)`等で渡す
using GLProcLoader = void* (*)(const char*);

/// @brief OpenGLの型・定数をglad非依存で表現する名前空間
/// @note 型エイリアスはglad (khrplatform) の定義とビット互換.
namespace gl {

/// @brief OpenGLのGLuint相当の型エイリアス
using Uint = std::uint32_t;
/// @brief OpenGLのGLint相当の型エイリアス
using Int = std::int32_t;
/// @brief OpenGLのGLenum相当の型エイリアス
using Enum = std::uint32_t;
/// @brief OpenGLのGLsizei相当の型エイリアス
using Sizei = std::int32_t;
/// @brief OpenGLのGLfloat相当の型エイリアス
using Float = float;
/// @brief OpenGLのGLboolean相当の型エイリアス
using Boolean = std::uint8_t;
/// @brief OpenGLのGLbitfield相当の型エイリアス
using Bitfield = std::uint32_t;
/// @brief OpenGLのGLchar相当の型エイリアス
using Char = char;
/// @brief OpenGLのGLsizeiptr相当の型エイリアス
using Sizeiptr = std::ptrdiff_t;

// 真偽・エラー
constexpr Enum kFalse   = 0;  // GL_FALSE
constexpr Enum kTrue    = 1;  // GL_TRUE
constexpr Enum kNoError = 0;  // GL_NO_ERROR
// 描画モード
constexpr Enum kPoints    = 0x0000;  // GL_POINTS
constexpr Enum kLines     = 0x0001;  // GL_LINES
constexpr Enum kLineLoop  = 0x0002;  // GL_LINE_LOOP
constexpr Enum kLineStrip = 0x0003;  // GL_LINE_STRIP
constexpr Enum kTriangles = 0x0004;  // GL_TRIANGLES
constexpr Enum kPatches   = 0x000E;  // GL_PATCHES
// データ型
constexpr Enum kUnsignedByte = 0x1401;  // GL_UNSIGNED_BYTE
constexpr Enum kUnsignedInt  = 0x1405;  // GL_UNSIGNED_INT
constexpr Enum kFloat        = 0x1406;  // GL_FLOAT
// ピクセルフォーマット
constexpr Enum kRgb  = 0x1907;  // GL_RGB
constexpr Enum kRgba = 0x1908;  // GL_RGBA
// テクスチャ
constexpr Enum kTexture2D        = 0x0DE1;  // GL_TEXTURE_2D
constexpr Enum kTexture0         = 0x84C0;  // GL_TEXTURE0
constexpr Enum kLinear           = 0x2601;  // GL_LINEAR
constexpr Enum kTextureMagFilter = 0x2800;  // GL_TEXTURE_MAG_FILTER
constexpr Enum kTextureMinFilter = 0x2801;  // GL_TEXTURE_MIN_FILTER
constexpr Enum kTextureWrapS     = 0x2802;  // GL_TEXTURE_WRAP_S
constexpr Enum kTextureWrapT     = 0x2803;  // GL_TEXTURE_WRAP_T
constexpr Enum kClampToEdge      = 0x812F;  // GL_CLAMP_TO_EDGE
// バッファ
constexpr Enum kArrayBuffer         = 0x8892;  // GL_ARRAY_BUFFER
constexpr Enum kElementArrayBuffer  = 0x8893;  // GL_ELEMENT_ARRAY_BUFFER
constexpr Enum kShaderStorageBuffer = 0x90D2;  // GL_SHADER_STORAGE_BUFFER
constexpr Enum kStaticDraw          = 0x88E4;  // GL_STATIC_DRAW
// 状態・ケイパビリティ
constexpr Enum kDepthTest        = 0x0B71;  // GL_DEPTH_TEST
constexpr Enum kBlend            = 0x0BE2;  // GL_BLEND
constexpr Enum kMultisample      = 0x809D;  // GL_MULTISAMPLE
constexpr Enum kProgramPointSize = 0x8642;  // GL_PROGRAM_POINT_SIZE
// ブレンドファクタ
constexpr Enum kSrcAlpha         = 0x0302;  // GL_SRC_ALPHA
constexpr Enum kOneMinusSrcAlpha = 0x0303;  // GL_ONE_MINUS_SRC_ALPHA
// クリアビット
constexpr Bitfield kColorBufferBit = 0x00004000;  // GL_COLOR_BUFFER_BIT
constexpr Bitfield kDepthBufferBit = 0x00000100;  // GL_DEPTH_BUFFER_BIT
// テッセレーション
constexpr Enum kPatchVertices = 0x8E72;  // GL_PATCH_VERTICES
// シェーダ
constexpr Enum kVertexShader         = 0x8B31;  // GL_VERTEX_SHADER
constexpr Enum kFragmentShader       = 0x8B30;  // GL_FRAGMENT_SHADER
constexpr Enum kGeometryShader       = 0x8DD9;  // GL_GEOMETRY_SHADER
constexpr Enum kTessControlShader    = 0x8E88;  // GL_TESS_CONTROL_SHADER
constexpr Enum kTessEvaluationShader = 0x8E87;  // GL_TESS_EVALUATION_SHADER
constexpr Enum kCompileStatus        = 0x8B81;  // GL_COMPILE_STATUS
constexpr Enum kLinkStatus           = 0x8B82;  // GL_LINK_STATUS
// フレームバッファ・レンダーバッファ
constexpr Enum kFramebuffer            = 0x8D40;  // GL_FRAMEBUFFER
constexpr Enum kRenderbuffer           = 0x8D41;  // GL_RENDERBUFFER
constexpr Enum kColorAttachment0       = 0x8CE0;  // GL_COLOR_ATTACHMENT0
constexpr Enum kDepthStencilAttachment = 0x821A;  // GL_DEPTH_STENCIL_ATTACHMENT
constexpr Enum kDepth24Stencil8        = 0x88F0;  // GL_DEPTH24_STENCIL8
constexpr Enum kFramebufferComplete    = 0x8CD5;  // GL_FRAMEBUFFER_COMPLETE
// クエリ
constexpr Enum kViewport = 0x0BA2;  // GL_VIEWPORT

}  // namespace gl

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CORE_GL_TYPES_H_
