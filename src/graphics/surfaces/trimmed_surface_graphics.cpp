/**
 * @file graphics/surfaces/trimmed_surface_graphics.cpp
 * @brief TrimmedSurfaceの描画用クラスの実装
 * @author Yayoi Habami
 * @date 2026-04-13
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/graphics/surfaces/trimmed_surface_graphics.h"

#include <memory>
#include <utility>

#include "igesio/entities/surfaces/algorithms/restricted_surface_mesh.h"
#include "igesio/entities/surfaces/algorithms/surface_boundary_edges.h"



/**
 * constructor / destructor
 */

igesio::graphics::TrimmedSurfaceGraphics::TrimmedSurfaceGraphics(
        const std::shared_ptr<const entities::TrimmedSurface>& entity,
        const std::shared_ptr<IOpenGL>& gl)
        : EntityGraphics(entity, gl, ShaderType::kGeneralSurface, true),
          edge_buffer_(gl) {
    Synchronize();
}

igesio::graphics::TrimmedSurfaceGraphics::~TrimmedSurfaceGraphics() {
    Cleanup();
}



/**
 * protected
 */

void igesio::graphics::TrimmedSurfaceGraphics::DrawImpl(
        gl::Uint /*shader*/,
        const std::pair<float, float>& /*viewport*/) const {
    gl_->BindVertexArray(vao_);
    gl_->DrawElements(gl::kTriangles, indices_.size(), gl::kUnsignedInt, 0);
    gl_->BindVertexArray(0);
}



/**
 * public
 */

void igesio::graphics::TrimmedSurfaceGraphics::Synchronize() {
    Cleanup();
    SyncTexture();
    GenerateSurfaceData();

    // 境界エッジ (外周/内周トリム境界) を構築する
    const auto edges = entities::ComputeRestrictedSurfaceEdges(*entity_);
    edge_buffer_.Build(edges.loops);

    gl_->GenVertexArrays(1, &vao_);
    gl_->GenBuffers(1, &vbo_);
    gl_->GenBuffers(1, &ebo_);

    gl_->BindVertexArray(vao_);

    gl_->BindBuffer(gl::kArrayBuffer, vbo_);
    gl_->BufferData(gl::kArrayBuffer,
                    vertices_.size() * sizeof(float),
                    vertices_.data(), gl::kStaticDraw);
    // 位置 (location=0), 法線 (location=1), テクスチャ座標 (location=2)
    gl_->VertexAttribPointer(0, 3, gl::kFloat, gl::kFalse, 8 * sizeof(float),
                             nullptr);
    gl_->EnableVertexAttribArray(0);
    gl_->VertexAttribPointer(1, 3, gl::kFloat, gl::kFalse, 8 * sizeof(float),
                             reinterpret_cast<const void*>(3 * sizeof(float)));
    gl_->EnableVertexAttribArray(1);
    gl_->VertexAttribPointer(2, 2, gl::kFloat, gl::kFalse, 8 * sizeof(float),
                             reinterpret_cast<const void*>(6 * sizeof(float)));
    gl_->EnableVertexAttribArray(2);

    gl_->BindBuffer(gl::kElementArrayBuffer, ebo_);
    gl_->BufferData(gl::kElementArrayBuffer,
                    indices_.size() * sizeof(gl::Uint),
                    indices_.data(), gl::kStaticDraw);

    gl_->BindVertexArray(0);
}

void igesio::graphics::TrimmedSurfaceGraphics::Cleanup() {
    EntityGraphics::Cleanup();

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

    vertices_.resize(0, 0);
    indices_.clear();
}

igesio::graphics::SelectionSamples
igesio::graphics::TrimmedSurfaceGraphics::GetSelectionSamples(
        const SelectionSampleParams& params) const {
    if (!entity_) return {};

    const igesio::Matrix4d wt = world_transform_.cast<double>();
    SelectionSamples result;

    // 外周ループ: N1=0 ならパラメータ矩形、N1=1 ならトリム境界曲線(142)を使用
    if (entity_->IsOuterBoundaryOfD()) {
        SelectionSamples base = SampleSurfaceBoundary(*entity_, params, wt);
        for (auto& pl : base.polylines) {
            result.polylines.push_back(std::move(pl));
        }
    } else if (auto outer = entity_->GetOuterBoundary()) {
        // CurveOnAParametricSurface(142) は ICurve
        SelectionSamples outer_s = SampleCurve(*outer, params, wt);
        for (auto& pl : outer_s.polylines) {
            result.polylines.push_back(std::move(pl));
        }
    }

    // 内周 (穴) ループ
    const size_t n_inner = entity_->GetInnerBoundaryCount();
    for (size_t i = 0; i < n_inner; ++i) {
        auto inner = entity_->GetInnerBoundaryAt(i);
        if (!inner) continue;
        SelectionSamples inner_s = SampleCurve(*inner, params, wt);
        for (auto& pl : inner_s.polylines) {
            result.polylines.push_back(std::move(pl));
        }
    }

    // トリム領域内の内部グリッド点を追加する (IsInDomainで穴/外側を除外)
    SampleSurfaceInteriorGrid(*entity_, params, wt, result);

    // 各ループを閉じた境界とみなし偶奇規則で内外判定する (穴対応はrenderer側)
    result.polylines_closed = !result.polylines.empty();
    return result;
}



/**
 * private
 */

void igesio::graphics::TrimmedSurfaceGraphics::GenerateSurfaceData() {
    // テッセレーションは制限付き曲面(143/144)共通のアルゴリズムへ委譲する
    auto mesh = entities::TessellateRestrictedSurface(*entity_);
    vertices_ = std::move(mesh.vertices);
    indices_ = std::move(mesh.indices);
}
