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
R"(#version 400 core
layout (location = 0) in vec3 aPos;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)", R"(
#version 400 core
out vec4 FragColor;
uniform vec4 mainColor;
void main() {
    FragColor = mainColor;
})"};



/// @brief kCircularArc用のシェーダー
/// @note Vertex, TCS, TES, Fragment
constexpr std::array<const char*, 4> kCircularArcShader = {
R"(#version 400 core
void main() { }
)", R"(
#version 400 core
layout (vertices = 1) out;

// parameters
uniform vec3 center;
uniform float radius;
uniform float startAngle;  // [rad]
uniform float endAngle;    // [rad], endAngle > startAngle
const int pxPerSegment = 2;
const float maxSegments = 100.0;

// transformation
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// viewport
uniform vec2 viewportSize;

void main() {
    gl_InvocationID;  // to avoid compiler warnings

    vec4 startPosW = vec4(center.x + radius * cos(startAngle),
                          center.y + radius * sin(startAngle), center.z, 1.0);
    vec4 endPosW = vec4(center.x + radius * cos(endAngle),
                        center.y + radius * sin(endAngle), center.z, 1.0);
    float midAngle = (startAngle + endAngle) / 2.0;
    vec4 midPosW = vec4(center.x + radius * cos(midAngle),
                        center.y + radius * sin(midAngle), center.z, 1.0);

    // World -> Clip coordinates
    mat4 pvm = projection * view * model;
    vec4 startPosClip = pvm * startPosW;
    vec4 endPosClip = pvm * endPosW;
    vec4 midPosClip = pvm * midPosW;

    // Clip -> Screen coordinates
    vec2 startPosScreen = (startPosClip.xy / startPosClip.w + 1.0) / 2.0 * viewportSize;
    vec2 endPosScreen = (endPosClip.xy / endPosClip.w + 1.0) / 2.0 * viewportSize;
    vec2 midPosScreen = (midPosClip.xy / midPosClip.w + 1.0) / 2.0 * viewportSize;

    // Calculate the number of segments
    float dist = distance(startPosScreen, midPosScreen)
               + distance(midPosScreen, endPosScreen);
    float segments = clamp(dist / pxPerSegment, 1.0, maxSegments);

    // Set tessellation level
    gl_TessLevelOuter[0] = 1.0;
    gl_TessLevelOuter[1] = segments;
}
)", R"(
#version 400 core
layout (isolines, equal_spacing, ccw) in;

// parameters
uniform vec3 center;
uniform float radius;
uniform float startAngle;  // [rad]
uniform float endAngle;    // [rad], endAngle > startAngle

// transformation
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    float t = gl_TessCoord.x;  // [0.0, 1.0]
    float angle = startAngle + t * (endAngle - startAngle);

    vec4 pos = vec4(center.x + radius * cos(angle),
                    center.y + radius * sin(angle),
                    center.z, 1.0);
    gl_Position = projection * view * model * pos;
}
)", kGeneralCurveShader[1]};



/// @brief kEllipse用のシェーダー
constexpr std::array<const char*, 2> kEllipseShader = {
R"(#version 400 core
// parameters
uniform vec3 center;
uniform float radiusX;
uniform float radiusY;
uniform float startAngle;  // [rad]
uniform float endAngle;    // [rad], endAngle > startAngle

// transformation
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// segmentation
uniform int segments;
void main() {
    float t = float(gl_VertexID) / float(segments);
    float angle = startAngle + t * (endAngle - startAngle);

    vec4 pos = vec4(center.x + radiusX * cos(angle),
                    center.y + radiusY * sin(angle),
                    center.z, 1.0);
    gl_Position = projection * view * model * pos;
}
)", kGeneralCurveShader[1]};

/// @brief kRationalBSplineCurve用のシェーダー
/// @note Vertex, TCS, TES, Fragment
constexpr std::array<const char*, 4> kRationalBSplineCurveShader = {
R"(#version 400 core
void main() { }
)", R"(
#version 400 core
layout (vertices = 1) out;

// constants
const int pxPerSegment = 2;
const float maxSegments = 100.0;
const int MAX_REF_POINTS = 10;

// transformation
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// viewport
uniform vec2 viewportSize;

// reference points (points on the curve)
uniform int numRefPoints;
uniform vec3 refPoints[MAX_REF_POINTS];

void main() {
    gl_InvocationID;  // to avoid compiler warnings

    // Approximate the curve length by summing the distances between consecutive points
    mat4 pvm = projection * view * model;
    float totalScreenDistance = 0.0;

    // Convert the first point to screen coordinates
    vec4 clipPos = pvm * vec4(refPoints[0], 1.0);
    vec2 prevScreenPos = (clipPos.xy / clipPos.w + 1.0) / 2.0 * viewportSize;

    // Process subsequent points and accumulate distances between each point
    for (int i = 1; i < numRefPoints; ++i) {
        clipPos = pvm * vec4(refPoints[i], 1.0);
        vec2 currentScreenPos = (clipPos.xy / clipPos.w + 1.0) / 2.0 * viewportSize;
        totalScreenDistance += distance(prevScreenPos, currentScreenPos);
        prevScreenPos = currentScreenPos;
    }

    // Determine the number of segments based on the distance
    float segments = clamp(totalScreenDistance / pxPerSegment, 1.0, maxSegments);

    // Set tessellation level
    gl_TessLevelOuter[0] = 1.0;
    gl_TessLevelOuter[1] = segments;
}
)", R"(
#version 400 core
layout (isolines, equal_spacing, ccw) in;

// constants
const int MAX_DEGREE = 5;
const int MAX_CTRL_POINTS = 128;
const int MAX_KNOTS = MAX_CTRL_POINTS + MAX_DEGREE + 1;

// parameters
uniform int degree;
uniform int numCtrlPoints;
uniform float knots[MAX_KNOTS];
uniform float weights[MAX_CTRL_POINTS];
uniform vec3 ctrlPoints[MAX_CTRL_POINTS];
uniform vec2 paramRange;  // [start, end]

// transformation
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

/// @brief Calculate the NURBS curve point at parameter u
/// @param u parameter value
vec3 computeNURBSPoint(float u) {
    // clamp u within the defined range
    u = clamp(u, knots[degree], knots[numCtrlPoints]);

    // 1. Find the knot span [knot[j], knot[j+1])
    // This logic mimics the behavior of std::upper_bound in the C++ code.
    int knotSpan;
    if (u >= knots[numCtrlPoints]) {
        // Handle the case where the parameter is at the end of the domain
        knotSpan = numCtrlPoints - 1;
    } else {
        // Find the index of the first knot value strictly greater than u
        int upper_bound_idx = 0;
        for (int i = 0; i < MAX_KNOTS; ++i) {
            if (i >= numCtrlPoints + degree + 1) break; // Avoid reading out of bounds
            if (knots[i] > u) {
                upper_bound_idx = i;
                break;
            }
        }
        knotSpan = upper_bound_idx - 1;
    }

    // 2. Calculate B-spline basis functions (The NURBS Book, Algorithm A2.1)
    float N[MAX_DEGREE + 1];
    float left[MAX_DEGREE + 1];
    float right[MAX_DEGREE + 1];

    N[0] = 1.0;
    for (int j = 1; j <= degree; ++j) {
        left[j] = u - knots[knotSpan + 1 - j];
        right[j] = knots[knotSpan + j] - u;
        float saved = 0.0;
        for (int r = 0; r < j; ++r) {
            float temp = N[r] / (right[r + 1] + left[j - r]);
            N[r] = saved + right[r + 1] * temp;
            saved = left[j - r] * temp;
        }
        N[j] = saved;
    }

    // 3. Calculate the point coordinates according to the NURBS definition
    vec3 numerator = vec3(0.0);
    float denominator = 0.0;

    for (int i = 0; i <= degree; ++i) {
        int cp_idx = knotSpan - degree + i;
        if (cp_idx < 0 || cp_idx >= numCtrlPoints) continue;

        float temp = N[i] * weights[cp_idx];
        numerator += ctrlPoints[cp_idx] * temp;
        denominator += temp;
    }

    // Avoid division by zero
    if (abs(denominator) < 1e-6) {
        return ctrlPoints[knotSpan - degree];
    }

    return numerator / denominator;
}

void main() {
    // Map gl_TessCoord.x (range [0, 1]) to the curve's parameter range [start, end]
    float t = paramRange.x + gl_TessCoord.x * (paramRange.y - paramRange.x);

    // Calculate the coordinates on the curve
    vec3 pos = computeNURBSPoint(t);

    // Perform coordinate transformation and set the final vertex position
    gl_Position = projection * view * model * vec4(pos, 1.0);
}
)", kGeneralCurveShader[1]};

/// @brief kLine用のシェーダー
constexpr std::array<const char*, 2> kLineShader = {
kGeneralCurveShader[0],
kGeneralCurveShader[1]};

/// @brief CopiousData用のシェーダー
constexpr std::array<const char*, 2> kCopiousDataShader = {
kGeneralCurveShader[0],
kGeneralCurveShader[1]};


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
        case ShaderType::kCopiousData:  // Type 106, Forms 1-13
            return ShaderCode(kCopiousDataShader);
        case ShaderType::kLine:          // Type 110
            return ShaderCode(kLineShader);
        case ShaderType::kRationalBSplineCurve:  // Type 126
            return ShaderCode(kRationalBSplineCurveShader);
        default:
            return std::nullopt;  // 他のシェーダータイプは未実装
    }
}

}  // namespace igesio::graphics::shaders

#endif  // SRC_GRAPHICS_SHADERS_CURVES_H_
