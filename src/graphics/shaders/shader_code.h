/**
 * @file graphics/shaders/shader_code.h
 * @brief シェーダープログラムをまとめる構造体を定義する
 * @author Yayoi Habami
 * @date 2025-08-09
 * @copyright 2025 Yayoi Habami
 */
#ifndef SRC_GRAPHICS_SHADERS_SHADER_CODE_H_
#define SRC_GRAPHICS_SHADERS_SHADER_CODE_H_

#include <array>
#include <string>



/// @brief シェーダー関連のソースをまとめる名前空間
/// @note 各シェーダーのソースコードは`k<ShaderType>Shader`に定義される.
///       定数の型は`std::array<const char*, 2>`などで、
///       (1) 2要素の場合は vertex → fragment
///       (2) 3要素の場合は vertex → geometry → fragment
///       (3) 4要素の場合は vertex → tcs → tes → fragment
///       (4) 5要素の場合は vertex → tcs → tes → geometry → fragment
///       の順で定義・使用される
namespace igesio::graphics::shaders {

/// @brief シェーダーのコードをまとめた構造体
/// @note 組み合わせおよび順序は以下の通り:
///       (1) vertex → fragment
///       (2) vertex → geometry → fragment
///       (3) vertex → tcs → tes → fragment
///       (4) vertex → tcs → tes → geometry → fragment
/// @note computeシェーダーには非対応
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

}  // namespace igesio::graphics::shaders

#endif  // SRC_GRAPHICS_SHADERS_SHADER_CODE_H_
