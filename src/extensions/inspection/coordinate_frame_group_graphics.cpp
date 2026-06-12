/**
 * @file extensions/inspection/coordinate_frame_group_graphics.cpp
 * @brief CoordinateFrameGroup (座標系群) の描画クラス
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/inspection/coordinate_frame_group_graphics.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_set>
#include <utility>
#include <vector>

#include "igesio/graphics/core/shader_code.h"
#include "igesio/graphics/graphics_registry.h"
#include "igesio/graphics/shader_registry.h"

namespace igesio::extensions::inspection {

namespace {

namespace gl = igesio::graphics::gl;

/// @brief 点描画用カスタムシェーダーの頂点シェーダー
/// @note gl_PointSizeをuniform (uPointSize) で指定する. view/projectionは
///       レンダラが設定し、model/mainColor/uPointSizeは描画クラスが設定する.
const char* kFramePointVert = R"(#version 430 core
layout (location = 0) in vec3 aPos;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float uPointSize;
void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    gl_PointSize = uPointSize;
}
)";

/// @brief 点描画用カスタムシェーダーのフラグメントシェーダー
const char* kFramePointFrag = R"(#version 430 core
out vec4 FragColor;
uniform vec4 mainColor;
void main() {
    FragColor = mainColor;
}
)";

/// @brief 軸描画用カスタムシェーダーの頂点シェーダー
/// @note クリップ空間へ変換するのみ (太線化はジオメトリシェーダーで行う).
///       view/projectionはレンダラが、model等は描画クラスが設定する.
const char* kFrameLineVert = R"(#version 430 core
layout (location = 0) in vec3 aPos;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

/// @brief 軸描画用カスタムシェーダーのジオメトリシェーダー
/// @note 各線分をスクリーン空間でuLineWidth [px] 幅のクアッド (三角形ストリップ)
///       へ展開する. Core Profileでは glLineWidth による太線が無効なため、
///       深度に依らずpx幅一定の太線をここで生成する.
const char* kFrameLineGeom = R"(#version 430 core
layout (lines) in;
layout (triangle_strip, max_vertices = 4) out;
uniform vec2 viewportSize;
uniform float uLineWidth;
void main() {
    vec4 p0 = gl_in[0].gl_Position;
    vec4 p1 = gl_in[1].gl_Position;
    vec2 ndc0 = p0.xy / p0.w;
    vec2 ndc1 = p1.xy / p1.w;
    // スクリーン空間(px)での進行方向と法線
    vec2 dirPx = (ndc1 - ndc0) * viewportSize;
    float len = length(dirPx);
    vec2 dir = (len > 0.0) ? dirPx / len : vec2(1.0, 0.0);
    vec2 normalPx = vec2(-dir.y, dir.x);
    // 半幅(uLineWidth*0.5)px を NDC へ. NDC[-1,1]=viewport px のため係数2/viewport.
    // → (uLineWidth*0.5)*2/viewport = uLineWidth/viewport
    vec2 offset = normalPx * uLineWidth / viewportSize;
    gl_Position = vec4((ndc0 + offset) * p0.w, p0.z, p0.w); EmitVertex();
    gl_Position = vec4((ndc0 - offset) * p0.w, p0.z, p0.w); EmitVertex();
    gl_Position = vec4((ndc1 + offset) * p1.w, p1.z, p1.w); EmitVertex();
    gl_Position = vec4((ndc1 - offset) * p1.w, p1.z, p1.w); EmitVertex();
    EndPrimitive();
}
)";

/// @brief 点描画用カスタムシェーダーのShaderIdを取得する (未登録なら登録する)
/// @return CamFramePointシェーダーのShaderId
/// @note 初回呼び出し時に一度だけShaderRegistryへ登録し、以後はキャッシュを返す
graphics::ShaderId GetFramePointShaderId() {
    static const graphics::ShaderId id = [] {
        if (const auto found = graphics::ShaderRegistry::Find("CamFramePoint")) {
            return *found;
        }
        graphics::ShaderInfo info;
        info.name = "CamFramePoint";
        info.code = graphics::ShaderCode(kFramePointVert, kFramePointFrag);
        info.uses_lighting = false;
        info.category = graphics::ShaderDrawCategory::kAlways;
        return graphics::ShaderRegistry::Register(std::move(info));
    }();
    return id;
}

/// @brief 軸 (太線) 描画用カスタムシェーダーのShaderIdを取得する (未登録なら登録する)
/// @return CamFrameLineシェーダーのShaderId
/// @note 初回呼び出し時に一度だけShaderRegistryへ登録し、以後はキャッシュを返す.
///       フラグメントは点と同一 (mainColorを出力するのみ) のため再利用する.
///       ShaderCodeの3文字列引数順は (vertex, fragment, geometry) である点に注意.
graphics::ShaderId GetFrameLineShaderId() {
    static const graphics::ShaderId id = [] {
        if (const auto found = graphics::ShaderRegistry::Find("CamFrameLine")) {
            return *found;
        }
        graphics::ShaderInfo info;
        info.name = "CamFrameLine";
        info.code = graphics::ShaderCode(
                kFrameLineVert, kFramePointFrag, kFrameLineGeom);
        info.uses_lighting = false;
        info.category = graphics::ShaderDrawCategory::kAlways;
        return graphics::ShaderRegistry::Register(std::move(info));
    }();
    return id;
}

/// @brief Vector3dを単精度の3要素としてfloat列へ追加する
/// @param out 追加先のfloat列
/// @param v 追加するベクトル
void AppendVec(std::vector<float>& out, const igesio::Vector3d& v) {
    out.push_back(static_cast<float>(v.x()));
    out.push_back(static_cast<float>(v.y()));
    out.push_back(static_cast<float>(v.z()));
}

/// @brief 2点を結ぶ線分を線分頂点列 (x,y,z×2) としてfloat列へ追加する
/// @param out 追加先のfloat列
/// @param a 線分の始点
/// @param b 線分の終点
void AppendSegment(std::vector<float>& out,
                   const igesio::Vector3d& a, const igesio::Vector3d& b) {
    AppendVec(out, a);
    AppendVec(out, b);
}

/// @brief GraphicsRegistry用の描画オブジェクト作成関数
/// @param entity 描画する座標系群エンティティ
/// @param gl OpenGL関数のラッパー
/// @return 生成した描画オブジェクト
std::unique_ptr<graphics::IEntityGraphics> CreateGraphics(
        const std::shared_ptr<const CoordinateFrameGroup>& entity,
        const std::shared_ptr<graphics::IOpenGL>& gl) {
    return std::make_unique<CoordinateFrameGroupGraphics>(entity, gl);
}

}  // namespace



CoordinateFrameGroupGraphics::CoordinateFrameGroupGraphics(
        const std::shared_ptr<const CoordinateFrameGroup>& entity,
        const std::shared_ptr<graphics::IOpenGL>& gl)
        : EntityGraphics(entity, gl, GetFrameLineShaderId(), false),
          x_axis_buffer_(gl), y_axis_buffer_(gl), z_axis_buffer_(gl) {
    // 同期 (CPU構築+GL転送) はレンダラのreconcile経路が駆動する (ctorでは行わない)
}

CoordinateFrameGroupGraphics::~CoordinateFrameGroupGraphics() {
    Cleanup();
}

void CoordinateFrameGroupGraphics::Draw(
        gl::Uint shader, const graphics::ShaderId shader_id,
        const std::pair<float, float>& viewport,
        [[maybe_unused]] const graphics::DrawContext& ctx) const {
    // 軸 (色分けした3本の太線) をCamFrameLineプログラムで描画する
    if (shader_id == GetFrameLineShaderId()) {
        const igesio::Matrix4f model = GetWorldTransform();
        const double width = GetLineWidth();
        // GSで太線(px幅)を生成するため、画素サイズと線幅を渡す
        gl_->Uniform2f(gl_->GetUniformLocation(shader, "viewportSize"),
                       viewport.first, viewport.second);
        gl_->Uniform1f(gl_->GetUniformLocation(shader, "uLineWidth"),
                       static_cast<float>(width));
        if (!x_axis_buffer_.IsEmpty()) {
            x_axis_buffer_.DrawWithState(shader, model, entity_->XColor(), width);
        }
        if (!y_axis_buffer_.IsEmpty()) {
            y_axis_buffer_.DrawWithState(shader, model, entity_->YColor(), width);
        }
        if (!z_axis_buffer_.IsEmpty()) {
            z_axis_buffer_.DrawWithState(shader, model, entity_->ZColor(), width);
        }
        return;
    }
    // 点 (原点群) を専用シェーダーで描画する
    if (shader_id == GetFramePointShaderId()) {
        DrawPoints(shader);
    }
}

std::unordered_set<igesio::graphics::ShaderId>
CoordinateFrameGroupGraphics::GetShaderIds() const {
    return {GetFrameLineShaderId(), GetFramePointShaderId()};
}

void CoordinateFrameGroupGraphics::PrewarmCpu() {
    if (!entity_) return;
    // 冪等化: 形状が変わっていなければ作り直さない
    if (staged_ && staged_geometry_key_ == CurrentGeometryKey()) return;

    const auto& frames = entity_->Frames();
    const double axis_size = entity_->AxisSize();

    staging_x_.clear();
    staging_y_.clear();
    staging_z_.clear();
    staging_points_.clear();
    staging_x_.reserve(frames.size() * 6);
    staging_y_.reserve(frames.size() * 6);
    staging_z_.reserve(frames.size() * 6);
    staging_points_.reserve(frames.size() * 3);

    for (const auto& f : frames) {
        AppendSegment(staging_x_, f.origin, f.origin + f.x_axis * axis_size);
        AppendSegment(staging_y_, f.origin, f.origin + f.y_axis * axis_size);
        AppendSegment(staging_z_, f.origin, f.origin + f.z_axis * axis_size);
        AppendVec(staging_points_, f.origin);
    }

    staged_geometry_key_ = CurrentGeometryKey();
    staged_ = true;
}

bool CoordinateFrameGroupGraphics::IsDrawable() const {
    if (!entity_) return false;
    return point_count_ > 0 || !x_axis_buffer_.IsEmpty()
           || !y_axis_buffer_.IsEmpty() || !z_axis_buffer_.IsEmpty();
}

double CoordinateFrameGroupGraphics::GetLineWidth() const {
    // 非IGESエンティティのためIGES線幅番号は持たない. スタイルの軸線幅を用いる
    if (!entity_) return EntityGraphics::GetLineWidth();
    return entity_->AxisWidth();
}

void CoordinateFrameGroupGraphics::DoSynchronize() {
    // 既存のGLリソースを解放 (CPUステージングは保持される)
    Cleanup();

    // 未準備ならCPUステージングを構築する
    if (!staged_) PrewarmCpu();

    // 軸の線分バッファを転送 (空ならIsEmptyのままで描画されない)
    x_axis_buffer_.BuildFromSegments(staging_x_);
    y_axis_buffer_.BuildFromSegments(staging_y_);
    z_axis_buffer_.BuildFromSegments(staging_z_);

    // 点群のVAO/VBOを転送
    point_count_ = static_cast<int>(staging_points_.size() / 3);
    if (point_count_ > 0) {
        gl_->GenVertexArrays(1, &point_vao_);
        gl_->GenBuffers(1, &point_vbo_);
        gl_->BindVertexArray(point_vao_);
        gl_->BindBuffer(gl::kArrayBuffer, point_vbo_);
        gl_->BufferData(gl::kArrayBuffer,
                        staging_points_.size() * sizeof(float),
                        staging_points_.data(), gl::kStaticDraw);
        gl_->VertexAttribPointer(0, 3, gl::kFloat, gl::kFalse,
                                 3 * sizeof(float), nullptr);
        gl_->EnableVertexAttribArray(0);
        gl_->BindVertexArray(0);
    }
}

void CoordinateFrameGroupGraphics::DrawPoints(gl::Uint shader) const {
    if (point_vao_ == 0 || point_count_ <= 0) return;

    // 点径はgl_PointSizeで指定するため、プログラム制御の点径を有効化する
    gl_->Enable(gl::kProgramPointSize);

    const igesio::Matrix4f model = GetWorldTransform();
    gl_->UniformMatrix4fv(gl_->GetUniformLocation(shader, "model"),
                          1, gl::kFalse, model.data());
    const auto& color = entity_->PointColor();
    gl_->Uniform4fv(gl_->GetUniformLocation(shader, "mainColor"),
                    1, color.data());
    gl_->Uniform1f(gl_->GetUniformLocation(shader, "uPointSize"),
                   static_cast<float>(entity_->PointSize()));

    gl_->BindVertexArray(point_vao_);
    gl_->DrawArrays(gl::kPoints, 0, point_count_);
    gl_->BindVertexArray(0);
}

void CoordinateFrameGroupGraphics::Cleanup() {
    EntityGraphics::Cleanup();

    if (point_vbo_ != 0) {
        gl_->DeleteBuffers(1, &point_vbo_);
        point_vbo_ = 0;
    }
    if (point_vao_ != 0) {
        gl_->DeleteVertexArrays(1, &point_vao_);
        point_vao_ = 0;
    }
    point_count_ = 0;

    // 軸の線分バッファのGPUリソースを解放 (CPUステージングは破棄しない)
    x_axis_buffer_.Cleanup();
    y_axis_buffer_.Cleanup();
    z_axis_buffer_.Cleanup();
}

void RegisterCoordinateFrameGroupGraphics() {
    // 軸 (太線) ・点描画用カスタムシェーダーを登録する (冪等)
    GetFrameLineShaderId();
    GetFramePointShaderId();
    // 描画オブジェクト作成関数を登録する (冪等; 二重登録は無害)
    graphics::GraphicsRegistry::TryRegister<CoordinateFrameGroup>(&CreateGraphics);
}

}  // namespace igesio::extensions::inspection
