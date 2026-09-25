/**
 * @file tests/graphics/test_shader_include_expand.cpp
 * @brief 埋め込みGLSLのインクルード展開 (入れ子を含む) の検証
 * @author Yayoi Habami
 * @date 2026-09-25
 * @copyright 2026 Yayoi Habami
 * @note テスト対象:
 *       - `shaders::GetIncludeShaderCodes`の展開済みコードに`#include`が残らないこと
 *         (128 frag → general frag → pbr_shading の入れ子)
 *       - `shaders::ExpandShaderCodeIncludes`でパス参照とソース内`#include`の
 *         いずれからもPBRの関数が展開されること
 */
#include <gtest/gtest.h>

#include <array>
#include <string>

#include "igesio/graphics/core/shader_code.h"

#include "graphics/shaders.h"

namespace {

namespace shaders = igesio::graphics::shaders;
using igesio::graphics::ShaderCode;

/// @brief PBR照明のスニペットが展開済みであるか判定する
/// @param code 展開後のGLSLコード
/// @return PBRの関数を含み、かつ`#include`が残っていない場合はtrue
bool HasExpandedPbr(const std::string& code) {
    return code.find("ShadePBR") != std::string::npos &&
           code.find("DistributionGGX") != std::string::npos &&
           code.find("#include") == std::string::npos;
}

}  // namespace



// 埋め込み表の段階で入れ子のインクルードまで展開される
TEST(ShaderIncludeExpandTest, GetIncludeShaderCodes_ExpandsNestedIncludes) {
    const auto& codes = shaders::GetIncludeShaderCodes();
    for (const char* path : {"glsl/surfaces/general_surface.frag",
                             "glsl/surfaces/128_nurbs_surface.frag"}) {
        const auto it = codes.find(path);
        ASSERT_NE(it, codes.end()) << path;
        EXPECT_TRUE(HasExpandedPbr(it->second.glsl_code)) << path;
    }
}

// パス参照のShaderCodeを展開すると、両方の面シェーダーでPBRが展開される
TEST(ShaderIncludeExpandTest, ExpandShaderCodeIncludes_PathReferencedSurface) {
    const ShaderCode general("glsl/surfaces/general_surface.vert",
                             "glsl/surfaces/general_surface.frag");
    EXPECT_TRUE(HasExpandedPbr(
            shaders::ExpandShaderCodeIncludes(general).fragment));

    const ShaderCode nurbs(std::array<const char*, 4>{
            "glsl/surfaces/128_nurbs_surface.vert",
            "glsl/surfaces/128_nurbs_surface.tesc",
            "glsl/surfaces/128_nurbs_surface.tese",
            "glsl/surfaces/128_nurbs_surface.frag"});
    EXPECT_TRUE(HasExpandedPbr(
            shaders::ExpandShaderCodeIncludes(nurbs).fragment));
}

// ソース文字列内の#includeからもPBRのスニペットを展開できる (拡張の解析シェーダー向け)
TEST(ShaderIncludeExpandTest, ExpandShaderIncludes_InlineSourceInclude) {
    const std::string source =
            "#version 430 core\n"
            "#include \"glsl/surfaces/pbr_shading.glsl\"\n"
            "void main() {}\n";
    EXPECT_TRUE(HasExpandedPbr(shaders::ExpandShaderIncludes(source)));
}
