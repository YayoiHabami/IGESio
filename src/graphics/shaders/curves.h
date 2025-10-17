/**
 * @file graphics/shaders/curves.h
 * @brief 曲線用のGLSLシェーダープログラムを定義する
 * @author Yayoi Habami
 * @date 2025-08-06
 * @copyright 2025 Yayoi Habami
 */
#ifndef SRC_GRAPHICS_SHADERS_CURVES_H_
#define SRC_GRAPHICS_SHADERS_CURVES_H_

#include <array>

// 定数の取得のためのインクルード
#include "igesio/graphics/curves/rational_b_spline_curve_graphics.h"

#include "./shader_code.h"

namespace igesio::graphics::shaders {

/// @brief kGeneralCurve用のシェーダー
constexpr std::array<const char*, 2> kGeneralCurveShader = {
    "glsl/curves/general_curve.vert",
    "glsl/curves/general_curve.frag"
};



/// @brief kCircularArc用のシェーダー (Type 100)
/// @note Vertex, TCS, TES, Fragment
constexpr std::array<const char*, 4> kCircularArcShader = {
    "glsl/curves/100_circular_arc.vert",
    "glsl/curves/100_circular_arc.tesc",
    "glsl/curves/100_circular_arc.tese",
    "glsl/curves/100_circular_arc.frag"
};



/// @brief kEllipse用のシェーダー (Type 104, Form 0)
constexpr std::array<const char*, 2> kEllipseShader = {
    "glsl/curves/104_form0_ellipse.vert",
    "glsl/curves/104_form0_ellipse.frag"
};



/// @brief kCopiousData用のシェーダー (Type 106, Forms 1-13)
constexpr std::array<const char*, 2> kCopiousDataShader = {
kGeneralCurveShader[0],
kGeneralCurveShader[1]};



/// @brief kSegment用のシェーダー (Type 110, Form 0-1)
constexpr std::array<const char*, 2> kSegmentShader = {
kGeneralCurveShader[0],
kGeneralCurveShader[1]};

/// @brief kLine用のシェーダー (Type 110, Forms 2)
/// @note Vertex, Geometry, Fragment
constexpr std::array<const char*, 3> kLineShader = {
R"(
#version 400 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aDir;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform int lineType; // 0: Segment, 1: Ray, 2: Line

out VS_OUT {
    vec3 pos; // position in model coordinates
    vec3 dir; // direction in model coordinates
} vs_out;

void main() {
    // Pass the position and direction in model coordinates to the geometry shader
    vs_out.pos = aPos;
    vs_out.dir = aDir;
    // Dummy write to gl_Position to satisfy the vertex shader requirement
    gl_Position = vec4(aPos, 1.0);
}
)", R"(
#version 400 core
// input: start point
layout (points) in;
// output: line segments
layout (line_strip, max_vertices = 4) out;

in VS_OUT {
    vec3 pos;
    vec3 dir;
} gs_in[];

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float farLength;
uniform int lineType;

void main() {
    // Apply model transformation
    vec3 startPos = (model * vec4(gs_in[0].pos, 1.0)).xyz;
    vec3 startDir = mat3(model) * gs_in[0].dir;

    if (lineType == 1) {  // kRay
        vec3 endPos = startPos + startDir * farLength;
        gl_Position = projection * view * vec4(startPos, 1.0);
        EmitVertex();
        gl_Position = projection * view * vec4(endPos, 1.0);
        EmitVertex();
        EndPrimitive();
    } else if (lineType == 2) {  // kLine
        // Semi-infinite line (forward direction)
        vec3 endPos1 = startPos + startDir * farLength;
        gl_Position = projection * view * vec4(startPos, 1.0);
        EmitVertex();
        gl_Position = projection * view * vec4(endPos1, 1.0);
        EmitVertex();
        EndPrimitive();  // first line segment completed

        // Semi-infinite line (backward direction)
        vec3 endPos2 = startPos - startDir * farLength;
        gl_Position = projection * view * vec4(startPos, 1.0);
        EmitVertex();
        gl_Position = projection * view * vec4(endPos2, 1.0);
        EmitVertex();
        EndPrimitive();  // second line segment completed
    }
}
)",
kGeneralCurveShader[1]};



/// @brief kPoint用のシェーダー (Type 116)
constexpr std::array<const char*, 2> kPointShader = {
    "glsl/curves/116_point.vert",
    "glsl/curves/116_point.frag"
};



/// @brief kRationalBSplineCurve用のシェーダー (Type 126)
/// @note Vertex, TCS, TES, Fragment
constexpr std::array<const char*, 4> kRationalBSplineCurveShader = {
    "glsl/curves/126_nurbs_curve.vert",
    "glsl/curves/126_nurbs_curve.tesc",
    "glsl/curves/126_nurbs_curve.tese",
    "glsl/curves/126_nurbs_curve.frag"
};



/// @brief 曲線シェーダーのソースコードを取得する
/// @param shader_type シェーダーのタイプ
/// @return シェーダーのソースコード、
///         指定されたタイプのシェーダーがない場合はnullopt
std::optional<ShaderCode>
GetCurveShaderCode(const ShaderType shader_type) {
    switch (shader_type) {
        case ShaderType::kGeneralCurve:  // Type ---
            return ShaderCode(kGeneralCurveShader);
        case ShaderType::kCircularArc:   // Type 100
            return ShaderCode(kCircularArcShader);
        case ShaderType::kEllipse:       // Type 104, Form 1
            return ShaderCode(kEllipseShader);
        case ShaderType::kCopiousData:   // Type 106, Forms 1-13
            return ShaderCode(kCopiousDataShader);
        case ShaderType::kSegment:       // Type 110, Form 0
            return ShaderCode(kSegmentShader);
        case ShaderType::kLine:          // Type 110, Forms 1-2
            return ShaderCode(kLineShader);
        case ShaderType::kPoint:         // Type 116
            return ShaderCode(kPointShader);
        case ShaderType::kRationalBSplineCurve:  // Type 126
            return ShaderCode(kRationalBSplineCurveShader);
        default:
            return std::nullopt;  // 他のシェーダータイプは未実装
    }
}

}  // namespace igesio::graphics::shaders

#endif  // SRC_GRAPHICS_SHADERS_CURVES_H_
