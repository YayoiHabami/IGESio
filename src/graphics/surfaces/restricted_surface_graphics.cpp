/**
 * @file graphics/surfaces/restricted_surface_graphics.cpp
 * @brief 制限付き曲面 (IRestrictedSurface) の描画用クラスの実装
 * @author Yayoi Habami
 * @date 2026-06-09
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/graphics/surfaces/restricted_surface_graphics.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "igesio/entities/surfaces/algorithms/restricted_surface_mesh.h"
#include "igesio/entities/surfaces/algorithms/surface_boundary_edges.h"



/**
 * constructor / destructor
 */

igesio::graphics::RestrictedSurfaceGraphics::RestrictedSurfaceGraphics(
        const std::shared_ptr<const entities::IRestrictedSurface>& entity,
        const std::shared_ptr<IOpenGL>& gl)
        : EntityGraphics(entity, gl, ShaderType::kGeneralSurface, true),
          edge_buffer_(gl) {
    // 同期 (テッセレーション+GP転送) はレンダラのreconcile経路が駆動する
    // (PrewarmCpuで並列前倒し → DoSynchronizeでGL転送)。ctorでは行わない。
}

igesio::graphics::RestrictedSurfaceGraphics::~RestrictedSurfaceGraphics() {
    Cleanup();
}



/**
 * protected
 */

void igesio::graphics::RestrictedSurfaceGraphics::DrawImpl(
        gl::Uint /*shader*/,
        const std::pair<float, float>& /*viewport*/) const {
    gl_->BindVertexArray(vao_);
    gl_->DrawElements(gl::kTriangles, indices_.size(), gl::kUnsignedInt, 0);
    gl_->BindVertexArray(0);
}



/**
 * public
 */

void igesio::graphics::RestrictedSurfaceGraphics::PrewarmCpu() {
    if (!entity_) return;
    // GL転送前のCPU相. 同一同期キーでは再構築しない (冪等)。自身のメンバのみ
    // 書き込むため、レンダラから並列に呼んでも安全。
    const std::uint64_t key = CurrentGeometryKey();
    if (cpu_ready_ && cpu_key_ == key) return;

    // テッセレーションは制限付き曲面(143/144/108有界)共通のアルゴリズムへ委譲する
    pending_mesh_ = entities::TessellateRestrictedSurface(*entity_);
    // 境界エッジ (外周/内周トリム境界) をモデル空間の折れ線として計算する
    pending_edge_loops_ = entities::ComputeRestrictedSurfaceEdges(*entity_).loops;
    cpu_ready_ = true;
    cpu_key_ = key;
}

void igesio::graphics::RestrictedSurfaceGraphics::DoSynchronize() {
    Cleanup();
    SyncTexture();

    // CPU相 (テッセレーション・境界エッジ). 通常はレンダラが並列に前倒し済み。
    // 未構築なら自前で構築する (直列フォールバック)。
    PrewarmCpu();

    // ステージングを消費してGPUへ転送する
    vertices_ = std::move(pending_mesh_.vertices);
    indices_ = std::move(pending_mesh_.indices);
    edge_buffer_.Build(pending_edge_loops_);

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

    // ステージングは消費済み。次回 (編集による再同期) は再度PrewarmCpuで構築する。
    cpu_ready_ = false;
    pending_edge_loops_.clear();
}

void igesio::graphics::RestrictedSurfaceGraphics::Cleanup() {
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
igesio::graphics::RestrictedSurfaceGraphics::GetSelectionSamples(
        const SelectionSampleParams& params) const {
    if (!entity_) return {};

    const igesio::Matrix4d& wt = world_transform_;
    SelectionSamples result;

    // 外周ループ: N1=0ならパラメータ矩形、N1=1ならトリム境界曲線をS(B(t))で評価
    if (entity_->IsOuterBoundaryOfD()) {
        SelectionSamples base = SampleSurfaceBoundary(*entity_, params, wt);
        for (auto& pl : base.polylines) {
            result.polylines.push_back(std::move(pl));
        }
    } else if (auto outer = entity_->GetOuterUVBoundary()) {
        AppendBoundaryWorldPolyline(*outer, params.curve_samples, result);
    }

    // 内周 (穴) ループ
    const size_t n_inner = entity_->GetInnerBoundaryCount();
    for (size_t i = 0; i < n_inner; ++i) {
        auto inner = entity_->GetInnerUVBoundaryAt(i);
        if (!inner) continue;
        AppendBoundaryWorldPolyline(*inner, params.curve_samples, result);
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

void igesio::graphics::RestrictedSurfaceGraphics::AppendBoundaryWorldPolyline(
        const entities::ICurve& uv_boundary, int n_samples,
        SelectionSamples& result) const {
    auto base = entity_->GetBaseSurface();
    if (!base) return;

    // GetWorldTransformD() = world_transform_ · M_entity。基底S(u,v)はM_entity未適用の
    // ため、これを掛けるとワールド座標になる (entity_->TryGetPointAtと一致)
    const igesio::Matrix4d wtd = GetWorldTransformD();

    auto range = uv_boundary.GetParameterRange();
    double t0 = range[0], t1 = range[1];
    if (std::isinf(t0)) t0 = -kInfiniteParamClamp;
    if (std::isinf(t1)) t1 =  kInfiniteParamClamp;

    // 角点を評価点に含め、選択用折れ線が境界の角を丸めず通るようにする
    // (描画エッジComputeRestrictedSurfaceEdgesと同一のサンプル方針)
    const int n = std::max(1, n_samples);
    const auto params =
            entities::BuildCornerAwareSampleParams(uv_boundary, t0, t1, n);
    std::vector<Vector3d> poly;
    poly.reserve(params.size());
    for (const double t : params) {
        const auto uv = uv_boundary.TryGetPointAt(t);  // (u, v, 0)
        if (!uv) continue;
        const auto p = base->TryGetPointAt(uv->x(), uv->y());
        if (!p) continue;
        poly.push_back(ToWorld(wtd, *p));
    }
    if (poly.size() >= 2) result.polylines.push_back(std::move(poly));
}
