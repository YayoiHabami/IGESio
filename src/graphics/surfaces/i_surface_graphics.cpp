/**
 * @file graphics/surfaces/i_surface_graphics.cpp
 * @brief ISurfaceの描画用クラス
 * @author Yayoi Habami
 * @date 2025-10-10
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/graphics/surfaces/i_surface_graphics.h"

#include <memory>
#include <utility>
#include <vector>

#include "igesio/entities/surfaces/algorithms/surface_boundary_edges.h"
#include "igesio/graphics/core/mesh_staging.h"
#include "igesio/graphics/surfaces/surface_mesh.h"

namespace {

using igesio::graphics::ISurfaceGraphics;

/// @brief u/vパラメータのデフォルト分解能数
constexpr int kDefaultDiv = 20;

}  // namespace



/**
 * constructor / destructor
 */

ISurfaceGraphics::ISurfaceGraphics(
        const std::shared_ptr<const entities::ISurface>& entity,
        const std::shared_ptr<IOpenGL>& gl)
        : EntityGraphics(entity, gl, ShaderType::kGeneralSurface, true),
          edge_buffer_(gl) {
    // 同期 (CPU構築+GL転送) はレンダラのreconcile経路が駆動する (ctorでは行わない)
}

ISurfaceGraphics::~ISurfaceGraphics() {
    Cleanup();
}



/**
 * protected
 */

void ISurfaceGraphics::DrawImpl(
        gl::Uint shader, const std::pair<float, float>& viewport) const {
    // VAOをバインドして描画
    gl_->BindVertexArray(vao_);
    // glDrawElementsでインデックスバッファを用いた描画を行う
    gl_->DrawElements(gl::kTriangles, indices_.size(), gl::kUnsignedInt, 0);
    gl_->BindVertexArray(0);
}

void ISurfaceGraphics::DoSynchronize() {
    // 既存のリソースを解放
    Cleanup();

    SyncTexture();

    // 頂点・法線データとインデックスデータを生成
    GenerateSurfaceData();

    // 境界エッジ (パラメータ矩形の4アイソ辺) と、折り目の内部稜線を構築する
    auto edges = entities::ComputeParametricSurfaceEdges(*entity_);
    auto creases = entities::ComputeSurfaceCreaseEdges(*entity_);
    for (auto& loop : creases.loops) edges.loops.push_back(std::move(loop));
    edge_buffer_.Build(edges.loops);

    // VAO, VBO, EBOを生成
    gl_->GenVertexArrays(1, &vao_);
    gl_->GenBuffers(1, &vbo_);
    gl_->GenBuffers(1, &ebo_);

    // VAOをバインド
    gl_->BindVertexArray(vao_);

    // VBOに頂点・法線データを転送
    gl_->BindBuffer(gl::kArrayBuffer, vbo_);
    gl_->BufferData(gl::kArrayBuffer, vertices_.size() * sizeof(float),
                    vertices_.data(), gl::kStaticDraw);
    // 頂点属性を設定
    gl_->VertexAttribPointer(0, 3, gl::kFloat, gl::kFalse, 8 * sizeof(float), (void*)0);
    gl_->EnableVertexAttribArray(0);  // 位置
    gl_->VertexAttribPointer(1, 3, gl::kFloat, gl::kFalse, 8 * sizeof(float),
                             (void*)(3 * sizeof(float)));
    gl_->EnableVertexAttribArray(1);  // 法線
    gl_->VertexAttribPointer(2, 2, gl::kFloat, gl::kFalse, 8 * sizeof(float),
                             (void*)(6 * sizeof(float)));
    gl_->EnableVertexAttribArray(2);  // テクスチャ座標

    // EBOにインデックスデータを転送
    gl_->BindBuffer(gl::kElementArrayBuffer, ebo_);
    gl_->BufferData(gl::kElementArrayBuffer, indices_.size() * sizeof(gl::Uint),
                    indices_.data(), gl::kStaticDraw);

    // VAOのバインドを解除
    gl_->BindVertexArray(0);
}

void ISurfaceGraphics::Cleanup() {
    EntityGraphics::Cleanup();

    // VBOとEBOを解放
    if (vbo_ != 0) {
        gl_->DeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (ebo_ != 0) {
        gl_->DeleteBuffers(1, &ebo_);
        ebo_ = 0;
    }

    // 境界エッジのバッファを解放
    edge_buffer_.Cleanup();

    // 頂点・法線データとインデックスデータをクリア
    vertices_.clear();
    indices_.clear();
}

void ISurfaceGraphics::GenerateSurfaceData() {
    // メッシュ生成本体はGL非依存の自由関数へ委譲する (単体テスト可能)
    auto mesh = BuildGeneralSurfaceMesh(*entity_, kDefaultDiv, kDefaultDiv);
    // SoAメッシュをシェーダーレイアウトへinterleaveする (GPU転送用ステージング)
    vertices_ = BuildInterleavedVertices(mesh.mesh);
    indices_ = std::move(mesh.mesh.indices);
}
