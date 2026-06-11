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
    BuildFromSegments(vertices);
}

void SurfaceEdgeBuffer::BuildFromSegments(
        const std::vector<float>& segment_vertices) {
    Cleanup();
    if (!gl_) return;
    if (segment_vertices.empty()) return;

    vertex_count_ = static_cast<gl::Sizei>(segment_vertices.size() / 3);

    gl_->GenVertexArrays(1, &vao_);
    gl_->GenBuffers(1, &vbo_);

    gl_->BindVertexArray(vao_);
    gl_->BindBuffer(gl::kArrayBuffer, vbo_);
    gl_->BufferData(gl::kArrayBuffer, segment_vertices.size() * sizeof(float),
                    segment_vertices.data(), gl::kStaticDraw);
    gl_->VertexAttribPointer(0, 3, gl::kFloat, gl::kFalse,
                             3 * sizeof(float), nullptr);
    gl_->EnableVertexAttribArray(0);

    gl_->BindBuffer(gl::kArrayBuffer, 0);
    gl_->BindVertexArray(0);
}

void SurfaceEdgeBuffer::DrawWithState(
        gl::Uint shader, const igesio::Matrix4f& model,
        const std::array<float, 4>& color, double line_width,
        const bool highlighted) const {
    if (vertex_count_ == 0 || !gl_) return;

    gl_->LineWidth(static_cast<gl::Float>(line_width));
    gl_->UniformMatrix4fv(gl_->GetUniformLocation(shader, "model"),
                          1, gl::kFalse, model.data());
    gl_->Uniform4fv(gl_->GetUniformLocation(shader, "mainColor"),
                    1, color.data());

    // ハイライト中は深度を僅かに手前へ圧縮し、隣接面側の同一エッジとの
    // Zファイト (選択色と通常色の縞) を描画順に依らず選択色側で確定させる
    if (highlighted) gl_->DepthRange(0.0, 1.0 - kHighlightDepthShrink);

    gl_->BindVertexArray(vao_);
    gl_->DrawArrays(gl::kLines, 0, vertex_count_);
    gl_->BindVertexArray(0);

    if (highlighted) gl_->DepthRange(0.0, 1.0);
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
