// Geometry Shader: 線分をスクリーン空間でlineWidth [px] 幅のクアッドへ展開する
//
// Core Profileでは glLineWidth による太線が無効 (幅1pxに固定) なため、各線分
// (GL_LINES / LINE_STRIP / LINE_LOOP / テッセレーションのisolines) を、深度に依らず
// px幅一定の三角形ストリップへ展開して太線を生成する.
//
// THIS CODE REQUIRES THE FOLLOWING UNIFORMS TO BE BOUND:
//   - viewportSize (uniform vec2)  描画領域のサイズ [px] (レンダラが設定)
//   - lineWidth    (uniform float) 線幅 [px] (描画クラスが設定)
#version 400 core
layout (lines) in;
layout (triangle_strip, max_vertices = 4) out;

uniform vec2 viewportSize;
uniform float lineWidth;

void main() {
    vec4 p0 = gl_in[0].gl_Position;
    vec4 p1 = gl_in[1].gl_Position;
    vec2 ndc0 = p0.xy / p0.w;
    vec2 ndc1 = p1.xy / p1.w;

    // スクリーン空間(px)での進行方向と単位法線
    vec2 dirPx = (ndc1 - ndc0) * viewportSize;
    float len = length(dirPx);
    if (len < 1e-6) return;  // 退化線分 (長さ0) は描かない
    vec2 normalPx = vec2(-dirPx.y, dirPx.x) / len;

    // 半幅(lineWidth*0.5)px を NDC へ. NDC[-1,1]=viewportSize px のため係数2/viewportSize.
    // → (lineWidth*0.5)*2/viewportSize = lineWidth/viewportSize.
    // 各端点でwを乗じクリップ空間へ戻す (z/wは元のまま→深度は不変).
    vec2 offset = normalPx * lineWidth / viewportSize;
    gl_Position = vec4((ndc0 + offset) * p0.w, p0.z, p0.w); EmitVertex();
    gl_Position = vec4((ndc0 - offset) * p0.w, p0.z, p0.w); EmitVertex();
    gl_Position = vec4((ndc1 + offset) * p1.w, p1.z, p1.w); EmitVertex();
    gl_Position = vec4((ndc1 - offset) * p1.w, p1.z, p1.w); EmitVertex();
    EndPrimitive();
}
