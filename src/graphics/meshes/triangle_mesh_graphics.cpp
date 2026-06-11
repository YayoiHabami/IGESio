/**
 * @file graphics/meshes/triangle_mesh_graphics.cpp
 * @brief MeshEntity (三角形メッシュ) の描画クラス
 * @author Yayoi Habami
 * @date 2026-06-10
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/graphics/meshes/triangle_mesh_graphics.h"

#include <memory>
#include <utility>
#include <vector>

#include "igesio/numerics/mesh/algorithms.h"
#include "igesio/graphics/core/mesh_staging.h"

namespace {

namespace i_graph = igesio::graphics;
namespace i_num = igesio::numerics;
namespace gl = igesio::graphics::gl;

}  // namespace



i_graph::TriangleMeshGraphics::TriangleMeshGraphics(
        const std::shared_ptr<const entities::MeshEntity>& entity,
        const std::shared_ptr<IOpenGL>& gl)
        : EntityGraphics(entity, gl, ShaderType::kGeneralSurface, false) {
    // 同期 (CPU構築+GL転送) はレンダラのreconcile経路が駆動する (ctorでは行わない)
}

i_graph::TriangleMeshGraphics::~TriangleMeshGraphics() {
    Cleanup();
}

void i_graph::TriangleMeshGraphics::PrewarmCpu() {
    if (!entity_) return;
    // 冪等化: 形状が変わっていなければ作り直さない
    if (staged_ && staged_geometry_key_ == CurrentGeometryKey()) return;

    const auto& mesh = entity_->Mesh();

    // 法線が無い場合は面積重み平均で補う (コピーに対して計算する.
    // entity_は読み取り専用のため書き戻さない)
    i_num::TriangleMeshd recomputed;
    const i_num::TriangleMeshd* source = &mesh;
    if (!mesh.HasNormals()) {
        recomputed = mesh;
        i_num::RecomputeNormals(recomputed);
        source = &recomputed;
    }

    // 汎用曲面シェーダーのレイアウト (位置3+法線3+UV2) へinterleaveする.
    // GPU境界のためここで単精度へ変換する (CPU正準値はdoubleのまま)
    staging_vertices_ = BuildInterleavedVertices(*source);
    staging_indices_.assign(source->indices.begin(), source->indices.end());

    staged_geometry_key_ = CurrentGeometryKey();
    staged_ = true;
}

void i_graph::TriangleMeshGraphics::DoSynchronize() {
    // 既存のGLリソースを解放 (CPUステージングは保持される)
    Cleanup();

    SyncTexture();

    // 未準備ならCPUステージングを構築する
    if (!staged_) PrewarmCpu();
    if (staging_indices_.empty()) return;  // 空メッシュは描画なし

    // VAO, VBO, EBOを生成
    gl_->GenVertexArrays(1, &vao_);
    gl_->GenBuffers(1, &vbo_);
    gl_->GenBuffers(1, &ebo_);

    gl_->BindVertexArray(vao_);

    // VBOへinterleaved頂点 (位置3+法線3+UV2) を転送
    gl_->BindBuffer(gl::kArrayBuffer, vbo_);
    gl_->BufferData(gl::kArrayBuffer,
                    staging_vertices_.size() * sizeof(float),
                    staging_vertices_.data(), gl::kStaticDraw);
    gl_->VertexAttribPointer(0, 3, gl::kFloat, gl::kFalse,
                             8 * sizeof(float), (void*)0);
    gl_->EnableVertexAttribArray(0);  // 位置
    gl_->VertexAttribPointer(1, 3, gl::kFloat, gl::kFalse,
                             8 * sizeof(float), (void*)(3 * sizeof(float)));
    gl_->EnableVertexAttribArray(1);  // 法線
    gl_->VertexAttribPointer(2, 2, gl::kFloat, gl::kFalse,
                             8 * sizeof(float), (void*)(6 * sizeof(float)));
    gl_->EnableVertexAttribArray(2);  // テクスチャ座標

    // EBOへインデックスを転送
    gl_->BindBuffer(gl::kElementArrayBuffer, ebo_);
    gl_->BufferData(gl::kElementArrayBuffer,
                    staging_indices_.size() * sizeof(gl::Uint),
                    staging_indices_.data(), gl::kStaticDraw);

    gl_->BindVertexArray(0);
    index_count_ = static_cast<int>(staging_indices_.size());
}

void i_graph::TriangleMeshGraphics::DrawImpl(
        [[maybe_unused]] const gl::Uint shader,
        [[maybe_unused]] const std::pair<float, float>& viewport) const {
    if (vao_ == 0 || index_count_ <= 0) return;  // 未転送・空メッシュの自衛
    gl_->BindVertexArray(vao_);
    gl_->DrawElements(gl::kTriangles, index_count_, gl::kUnsignedInt, 0);
    gl_->BindVertexArray(0);
}

void i_graph::TriangleMeshGraphics::Cleanup() {
    EntityGraphics::Cleanup();

    // VBOとEBOを解放 (CPUステージングは破棄しない)
    if (vbo_ != 0) {
        gl_->DeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (ebo_ != 0) {
        gl_->DeleteBuffers(1, &ebo_);
        ebo_ = 0;
    }
    index_count_ = 0;
}
