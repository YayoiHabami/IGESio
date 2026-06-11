/**
 * @file graphics/core/shader_code.h
 * @brief シェーダープログラムのソースコードをまとめる構造体を定義する
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 */
#ifndef IGESIO_GRAPHICS_CORE_SHADER_CODE_H_
#define IGESIO_GRAPHICS_CORE_SHADER_CODE_H_

#include <array>
#include <string>
#include <utility>

namespace igesio::graphics {

/// @brief シェーダーのソースコードをまとめた構造体
/// @note 組み合わせおよび順序は以下の通り:
///       (1) vertex → fragment
///       (2) vertex → geometry → fragment
///       (3) vertex → tcs → tes → fragment
///       (4) vertex → tcs → tes → geometry → fragment
/// @note computeシェーダーには非対応
/// @note 各ステージは完結したGLSL文字列のほか、組み込みGLSLスニペットの
///       `#include "glsl/..."`参照を含められる (コンパイル直前にレンダラが
///       展開する)。展開はライブラリのソースディレクトリを実行時に読むため、
///       ソースを配置しない環境では完結したGLSL文字列のみ使用できる
struct ShaderCode {
    /// @brief 頂点シェーダーのソースコード
    std::string vertex;
    /// @brief フラグメントシェーダーのソースコード
    std::string fragment;
    /// @brief ジオメトリシェーダーのソースコード
    std::string geometry;
    /// @brief TCS (Tessellation Control Shader) のソースコード
    std::string tcs;
    /// @brief TES (Tessellation Evaluation Shader) のソースコード
    std::string tes;

    /// @brief シェーダーコードが不完全か
    /// @return 頂点シェーダーとフラグメントシェーダーの
    ///         どちらかが空文字列の場合はtrue
    bool IsIncomplete() const {
        if (vertex.empty() || fragment.empty()) return true;
        // TCSとTESは、どちらも空文字列 or どちらも非空文字列でなければならない
        if (tcs.empty() != tes.empty()) return true;
        return false;
    }

    /// @brief デフォルトコンストラクタ (全ステージ空)
    ShaderCode() = default;

    /// @brief コンストラクタ (各ステージを文字列で指定)
    /// @param vertex 頂点シェーダーのソースコード
    /// @param fragment フラグメントシェーダーのソースコード
    /// @param geometry ジオメトリシェーダーのソースコード (省略可)
    /// @param tcs TCSのソースコード (省略可; tesとセットで指定すること)
    /// @param tes TESのソースコード (省略可; tcsとセットで指定すること)
    ShaderCode(std::string vertex, std::string fragment,
               std::string geometry = "",
               std::string tcs = "", std::string tes = "")
        : vertex(std::move(vertex)), fragment(std::move(fragment)),
          geometry(std::move(geometry)),
          tcs(std::move(tcs)), tes(std::move(tes)) {}

    /// @brief コンストラクタ (2要素)
    /// @param shaders 頂点/フラグメントシェーダーのソースコード
    explicit ShaderCode(const std::array<const char*, 2>& shaders)
        : vertex(shaders[0]), fragment(shaders[1]) {}

    /// @brief コンストラクタ (3要素)
    /// @param shaders 頂点/ジオメトリ/フラグメントシェーダーのソースコード
    explicit ShaderCode(const std::array<const char*, 3>& shaders)
        : vertex(shaders[0]), geometry(shaders[1]), fragment(shaders[2]) {}

    /// @brief コンストラクタ (4要素)
    /// @param shaders 頂点/TCS/TES/フラグメントシェーダーのソースコード
    explicit ShaderCode(const std::array<const char*, 4>& shaders)
        : vertex(shaders[0]), tcs(shaders[1]), tes(shaders[2]),
          fragment(shaders[3]) {}

    /// @brief コンストラクタ (5要素)
    /// @param shaders 頂点/TCS/TES/ジオメトリ/フラグメントシェーダーのソースコード
    explicit ShaderCode(const std::array<const char*, 5>& shaders)
        : vertex(shaders[0]), tcs(shaders[1]), tes(shaders[2]),
          geometry(shaders[3]), fragment(shaders[4]) {}
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CORE_SHADER_CODE_H_
