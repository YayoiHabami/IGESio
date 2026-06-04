/**
 * @file graphics/core/surface_edge_buffer.cpp
 * @brief サーフェス境界エッジを線分として描画するGPUバッファの実装
 * @author Yayoi Habami
 * @date 2026-06-03
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/graphics/core/surface_edge_buffer.h"

#include <array>
#include <cstddef>
#include <vector>



namespace igesio::graphics {

void SurfaceEdgeBuffer::Build(
        const std::vector<std::vector<Vector3d>>& loops) {
    Cleanup();
    if (!gl_) return;

    // 各ループを隣接点ペア (線分) へ平坦化する
    std::vector<float> vertices;
    for (const auto& loop : loops) {
        for (std::size_t i = 1; i < loop.size(); ++i) {
            const auto& a = loop[i - 1];
            const auto& b = loop[i];
            vertices.push_back(static_cast<float>(a.x()));
            vertices.push_back(static_cast<float>(a.y()));
            vertices.push_back(static_cast<float>(a.z()));
            vertices.push_back(static_cast<float>(b.x()));
            vertices.push_back(static_cast<float>(b.y()));
            vertices.push_back(static_cast<float>(b.z()));
        }
    }
    if (vertices.empty()) return;

    vertex_count_ = static_cast<gl::Sizei>(vertices.size() / 3);

    gl_->GenVertexArrays(1, &vao_);
    gl_->GenBuffers(1, &vbo_);

    gl_->BindVertexArray(vao_);
    gl_->BindBuffer(gl::kArrayBuffer, vbo_);
    gl_->BufferData(gl::kArrayBuffer, vertices.size() * sizeof(float),
                    vertices.data(), gl::kStaticDraw);
    gl_->VertexAttribPointer(0, 3, gl::kFloat, gl::kFalse,
                             3 * sizeof(float), nullptr);
    gl_->EnableVertexAttribArray(0);

    gl_->BindBuffer(gl::kArrayBuffer, 0);
    gl_->BindVertexArray(0);
}

void SurfaceEdgeBuffer::DrawWithState(
        gl::Uint shader, const igesio::Matrix4f& model,
        const std::array<float, 4>& color, double line_width) const {
    if (vertex_count_ == 0 || !gl_) return;

    gl_->LineWidth(static_cast<gl::Float>(line_width));
    gl_->UniformMatrix4fv(gl_->GetUniformLocation(shader, "model"),
                          1, gl::kFalse, model.data());
    gl_->Uniform4fv(gl_->GetUniformLocation(shader, "mainColor"),
                    1, color.data());

    gl_->BindVertexArray(vao_);
    gl_->DrawArrays(gl::kLines, 0, vertex_count_);
    gl_->BindVertexArray(0);
}

void SurfaceEdgeBuffer::Cleanup() {
    if (!gl_) return;
    if (vbo_ != 0) {
        gl_->DeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (vao_ != 0) {
        gl_->DeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    vertex_count_ = 0;
}

}  // namespace igesio::graphics
