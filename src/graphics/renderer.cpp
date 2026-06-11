/**
 * @file graphics/renderer.cpp
 * @brief IGESエンティティの描画を行うレンダラークラスを定義する
 * @author Yayoi Habami
 * @date 2025-08-07
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/graphics/renderer.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "igesio/common/parallel.h"
#include "igesio/graphics/graphics_registry.h"

#include "./shaders.h"

namespace {

namespace i_graph = igesio::graphics;
namespace gl = igesio::graphics::gl;
using EntityRenderer = igesio::graphics::EntityRenderer;

/// @brief 表示モードに応じて当該シェーダー型を描画すべきか判定する
/// @param st シェーダー型
/// @param mode 表示モード
/// @return 描画すべき場合はtrue
/// @note 面塗り(kGeneralSurface/kRationalBSplineSurface)と面エッジ(kSurfaceEdge)を
///       モードで切り替える. それ以外 (独立曲線・点) は常に描画する.
bool ShouldDrawShaderType(const i_graph::ShaderType st,
                          const i_graph::DisplayMode mode) {
    const bool is_surface_fill = IsSurfaceFill(st);
    const bool is_surface_edge = st == i_graph::ShaderType::kSurfaceEdge;
    switch (mode) {
        case i_graph::DisplayMode::kShaded:    return true;
        case i_graph::DisplayMode::kNoEdge:    return !is_surface_edge;
        case i_graph::DisplayMode::kWireFrame: return !is_surface_fill;
    }
    return true;
}

/// @brief 頂点群の重心（平均座標）を計算する
/// @param vertices 頂点群（空でないこと）
/// @return 重心座標
igesio::Vector3d Centroid(const std::vector<igesio::Vector3d>& vertices) {
    igesio::Vector3d sum = igesio::Vector3d::Zero();
    for (const auto& v : vertices) sum += v;
    return sum / static_cast<double>(vertices.size());
}

/// @brief 点が矩形内（境界含む）にあるか
bool PointInRect(double x, double y, const i_graph::ScreenRect& r) {
    return x >= r.x_min && x <= r.x_max && y >= r.y_min && y <= r.y_max;
}

/// @brief 2次元ベクトル (ux,uy)x(vx,vy) の外積 (z成分)
double Cross2D(double ux, double uy, double vx, double vy) {
    return ux * vy - uy * vx;
}

/// @brief 線分 (a,b) と (c,d) が交差するか（端点接触・共線重なりを含む）
bool SegmentsCross(double ax, double ay, double bx, double by,
                   double cx, double cy, double dx, double dy) {
    const double d1 = Cross2D(bx - ax, by - ay, cx - ax, cy - ay);
    const double d2 = Cross2D(bx - ax, by - ay, dx - ax, dy - ay);
    const double d3 = Cross2D(dx - cx, dy - cy, ax - cx, ay - cy);
    const double d4 = Cross2D(dx - cx, dy - cy, bx - cx, by - cy);
    if (((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) &&
        ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0))) {
        return true;
    }
    // 共線で端点が相手の線分上に乗るケース
    auto on = [](double px, double py, double qx, double qy,
                 double rx, double ry) {
        return std::min(px, qx) <= rx && rx <= std::max(px, qx) &&
               std::min(py, qy) <= ry && ry <= std::max(py, qy);
    };
    if (d1 == 0.0 && on(ax, ay, bx, by, cx, cy)) return true;
    if (d2 == 0.0 && on(ax, ay, bx, by, dx, dy)) return true;
    if (d3 == 0.0 && on(cx, cy, dx, dy, ax, ay)) return true;
    if (d4 == 0.0 && on(cx, cy, dx, dy, bx, by)) return true;
    return false;
}

/// @brief 線分 (x0,y0)-(x1,y1) が矩形と交差するか
bool SegmentIntersectsRect(double x0, double y0, double x1, double y1,
                           const i_graph::ScreenRect& r) {
    if (PointInRect(x0, y0, r) || PointInRect(x1, y1, r)) return true;
    return SegmentsCross(x0, y0, x1, y1, r.x_min, r.y_min, r.x_max, r.y_min) ||
           SegmentsCross(x0, y0, x1, y1, r.x_max, r.y_min, r.x_max, r.y_max) ||
           SegmentsCross(x0, y0, x1, y1, r.x_max, r.y_max, r.x_min, r.y_max) ||
           SegmentsCross(x0, y0, x1, y1, r.x_min, r.y_max, r.x_min, r.y_min);
}

/// @brief 点 (px,py) が多角形 poly の内側にあるか（射線法・偶奇判定）
bool PointInPolygon(double px, double py,
                    const std::vector<std::array<double, 2>>& poly) {
    if (poly.size() < 3) return false;
    bool inside = false;
    const size_t n = poly.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const double xi = poly[i][0], yi = poly[i][1];
        const double xj = poly[j][0], yj = poly[j][1];
        if (((yi > py) != (yj > py)) &&
            (px < (xj - xi) * (py - yi) / (yj - yi) + xi)) {
            inside = !inside;
        }
    }
    return inside;
}

/// @brief クリップ空間の点を画面座標へ変換する
/// @return near面より手前 (clip.z + clip.w < 0) または透視特異点 (w<=0) は nullopt
/// @note near面クリッピングを考慮し、描画される範囲のみを画面座標化する
std::optional<std::array<double, 2>> ClipToPixel(
        const igesio::Vector4d& clip, int w, int h) {
    if (clip.w() <= 0.0) return std::nullopt;        // 透視投影でカメラ平面以遠
    if (clip.z() + clip.w() < 0.0) return std::nullopt;  // near面より手前
    const double inv_w = 1.0 / clip.w();
    const double x = (clip.x() * inv_w + 1.0) * 0.5 * static_cast<double>(w);
    const double y = (1.0 - clip.y() * inv_w) * 0.5 * static_cast<double>(h);
    return std::array<double, 2>{x, y};
}

/// @brief 2つのクリップ空間点を t で線形補間する
igesio::Vector4d LerpClip(const igesio::Vector4d& a,
                          const igesio::Vector4d& b, double t) {
    return igesio::Vector4d(a.x() + t * (b.x() - a.x()),
                            a.y() + t * (b.y() - a.y()),
                            a.z() + t * (b.z() - a.z()),
                            a.w() + t * (b.w() - a.w()));
}

/// @brief クリップ空間の線分をnear面でクリップし、画面座標の線分を返す
/// @param a, b 線分端点のクリップ空間座標
/// @return near面より手前で完全に切られる場合は nullopt
/// @note near面 (clip.z + clip.w = 0) をまたぐ線分は交点でクリップする
std::optional<std::pair<std::array<double, 2>, std::array<double, 2>>>
ClipSegmentNearAndProject(const igesio::Vector4d& a, const igesio::Vector4d& b,
                          int w, int h) {
    const double sa = a.z() + a.w();  // >=0 で near面の内側
    const double sb = b.z() + b.w();
    if (sa < 0.0 && sb < 0.0) return std::nullopt;  // 両端 near より手前

    igesio::Vector4d ca = a, cb = b;
    if (sa < 0.0 || sb < 0.0) {
        // near面との交点パラメータ (sa + t*(sb-sa) = 0)
        const double t = sa / (sa - sb);
        if (sa < 0.0) ca = LerpClip(a, b, t);
        else          cb = LerpClip(a, b, t);
    }
    const auto pa = ClipToPixel(ca, w, h);
    const auto pb = ClipToPixel(cb, w, h);
    if (!pa || !pb) return std::nullopt;
    return std::pair<std::array<double, 2>, std::array<double, 2>>{*pa, *pb};
}

/// @brief BBのスクリーンAABBがrectと重なる可能性があるか（粗カリング用）
/// @return BBが未定義/無限/一部がカメラ背面の場合は判定不能としtrueを返す
///         (カリングせずprecise判定に委ねる)
bool MayOverlapRect(const std::optional<igesio::numerics::BoundingBox>& bb,
                    const i_graph::ScreenRect& rect,
                    const igesio::Matrix4d& vp, int w, int h) {
    if (!bb || !bb->IsFinite()) return true;
    const auto vertices = bb->GetFiniteVertices();
    if (vertices.empty()) return true;

    double x_min = 0.0, y_min = 0.0, x_max = 0.0, y_max = 0.0;
    bool first = true;
    for (const auto& v : vertices) {
        const auto sp = ClipToPixel(i_graph::WorldToClip(vp, v), w, h);
        if (!sp) return true;  // 一部がカメラ背面/near手前 → カリングしない
        if (first) {
            x_min = x_max = (*sp)[0];
            y_min = y_max = (*sp)[1];
            first = false;
        } else {
            x_min = std::min(x_min, (*sp)[0]);
            x_max = std::max(x_max, (*sp)[0]);
            y_min = std::min(y_min, (*sp)[1]);
            y_max = std::max(y_max, (*sp)[1]);
        }
    }
    if (first) return true;
    return !(x_max < rect.x_min || x_min > rect.x_max ||
             y_max < rect.y_min || y_min > rect.y_max);
}

/// @brief サンプル全体が矩形内に収まるか（内包判定）
/// @return サンプルが存在し、かつ全点が射影可能で矩形内のときtrue
bool ContainedInRect(const i_graph::SelectionSamples& s,
                     const i_graph::ScreenRect& rect,
                     const igesio::Matrix4d& vp, int w, int h) {
    bool any = false;
    for (const auto& pt : s.points) {
        const auto sp = ClipToPixel(i_graph::WorldToClip(vp, pt), w, h);
        if (!sp || !PointInRect((*sp)[0], (*sp)[1], rect)) return false;
        any = true;
    }
    for (const auto& polyline : s.polylines) {
        for (const auto& v : polyline) {
            const auto sp = ClipToPixel(i_graph::WorldToClip(vp, v), w, h);
            if (!sp || !PointInRect((*sp)[0], (*sp)[1], rect)) return false;
            any = true;
        }
    }
    return any;
}

/// @brief サンプルが矩形と交差するか（交差判定）
bool CrossesRect(const i_graph::SelectionSamples& s,
                 const i_graph::ScreenRect& rect,
                 const igesio::Matrix4d& vp, int w, int h) {
    // 孤立点
    for (const auto& pt : s.points) {
        const auto sp = ClipToPixel(i_graph::WorldToClip(vp, pt), w, h);
        if (sp && PointInRect((*sp)[0], (*sp)[1], rect)) return true;
    }
    // 折れ線（各セグメントをnear面でクリップして判定する）
    for (const auto& polyline : s.polylines) {
        // 頂点ごとのクリップ空間座標を先に計算する (P*Vの再利用)
        std::vector<igesio::Vector4d> clips;
        clips.reserve(polyline.size());
        for (const auto& v : polyline) clips.push_back(i_graph::WorldToClip(vp, v));

        for (size_t i = 1; i < clips.size(); ++i) {
            const auto seg = ClipSegmentNearAndProject(clips[i - 1], clips[i], w, h);
            if (!seg) continue;
            const auto& [pa, pb] = *seg;
            if (PointInRect(pa[0], pa[1], rect)) return true;
            if (PointInRect(pb[0], pb[1], rect)) return true;
            if (SegmentIntersectsRect(pa[0], pa[1], pb[0], pb[1], rect)) {
                return true;
            }
        }
    }
    // 閉ループ: 矩形の角が射影境界ポリゴンの内側か（大面にズームインしたケース）。
    // 複数ループ (外周+穴) は偶奇規則で内外判定し、穴を正しく除外する
    if (s.polylines_closed && !s.polylines.empty()) {
        std::vector<std::vector<std::array<double, 2>>> loops;
        for (const auto& polyline : s.polylines) {
            std::vector<std::array<double, 2>> poly;
            poly.reserve(polyline.size());
            for (const auto& v : polyline) {
                const auto sp = ClipToPixel(i_graph::WorldToClip(vp, v), w, h);
                if (sp) poly.push_back(*sp);
            }
            if (poly.size() >= 3) loops.push_back(std::move(poly));
        }
        const double rx[4] = {rect.x_min, rect.x_max, rect.x_max, rect.x_min};
        const double ry[4] = {rect.y_min, rect.y_min, rect.y_max, rect.y_max};
        for (int i = 0; i < 4; ++i) {
            bool inside = false;
            for (const auto& loop : loops) {
                if (PointInPolygon(rx[i], ry[i], loop)) inside = !inside;
            }
            if (inside) return true;
        }
    }
    return false;
}

}  // namespace



EntityRenderer::EntityRenderer(std::shared_ptr<IOpenGL> gl,
                               const int width, const int height)
        : gl_(std::move(gl)), display_width_(width), display_height_(height) {
    default_global_param_ = std::make_shared<const models::GraphicsGlobalParam>(
            igesio::models::kDefaultModelSpaceScale,
            igesio::models::kDefaultLineWeightGradations,
            igesio::models::kDefaultMaxLineWeight);
}



void EntityRenderer::SetGLBackend(std::shared_ptr<IOpenGL> gl) {
    gl_ = std::move(gl);
}

bool EntityRenderer::IsInitialized() const {
    return gl_ != nullptr && !shader_programs_.empty();
}

void EntityRenderer::Initialize() {
    if (gl_ == nullptr) {
        throw igesio::ImplementationError(
                "GL backend is not set; call SetGLBackend() before Initialize()");
    }
    InitShaders();

    // 深度テストを有効化
    gl_->Enable(gl::kDepthTest);

    // アンチエイリアシングの初期化
    EnableAntialiasing(settings_.enable_antialiasing);
    // ブレンド設定
    EnableTransparency(settings_.enable_transparency);
}

void EntityRenderer::Cleanup() {
    // 描画オブジェクトのクリーンアップ (nullptrは負キャッシュのためスキップ)
    for (auto& [id, object] : graphics_cache_) {
        if (object) object->Cleanup();
    }
    graphics_cache_.clear();
    // キャッシュ描画/可視リストも破棄する (graphics_cache_の要素を指すため)
    draw_list_.clear();
    visible_list_.clear();
    local_dirty_ = true;

    // シェーダープログラムの削除
    for (auto& [shader_type, program_id] : shader_programs_) {
        gl_->DeleteProgram(program_id);
    }
    shader_programs_.clear();

    // OpenGLリソースの解放
    gl_->Disable(gl::kDepthTest);
}



/**
 * シーン同期 (Reconcile)
 */

void EntityRenderer::EnsureSynced() const {
    if (scene_ == nullptr || gl_ == nullptr) return;
    const auto& root = scene_->Root();

    // 描画レジストリの変化を検知したら負キャッシュ (生成失敗のnullptrエントリ)
    // のみ破棄し、新規登録された型の生成を再試行させる (生成済みは温存)
    const auto registry_revision = GraphicsRegistry::Revision();
    if (registry_revision != synced_graphics_registry_revision_) {
        for (auto it = graphics_cache_.begin(); it != graphics_cache_.end();) {
            if (it->second == nullptr) {
                it = graphics_cache_.erase(it);
            } else {
                ++it;
            }
        }
        synced_graphics_registry_revision_ = registry_revision;
        local_dirty_ = true;
    }

    // リビジョン値は同一rootに対してのみ比較可能なため、同一性とペアで比較する
    if (!local_dirty_ && synced_root_ == root.GetID()
            && synced_revision_ == root.Revision()) {
        return;
    }

    SweepStaleGraphics(root);
    RebuildDrawList();
    UpdateAutoClipSphere();
    // 可視集合が変わったためバケットの再構築が必要
    draw_buckets_dirty_ = true;

    synced_revision_ = root.Revision();
    synced_root_ = root.GetID();
    local_dirty_ = false;
}

void EntityRenderer::SweepStaleGraphics(const models::Assembly& root) const {
    // 逆引きインデックスでO(1)/件. 追加のツリー走査は不要
    for (auto it = graphics_cache_.begin(); it != graphics_cache_.end();) {
        if (root.FindOwner(it->first) == nullptr) {
            if (it->second) it->second->Cleanup();  // OpenGLリソースを解放
            it = graphics_cache_.erase(it);
        } else {
            ++it;
        }
    }
    // 所有者を失ったマテリアル設定も破棄する
    for (auto it = material_overrides_.begin();
         it != material_overrides_.end();) {
        if (root.FindOwner(it->first) == nullptr) {
            it = material_overrides_.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = pending_material_ids_.begin();
         it != pending_material_ids_.end();) {
        if (root.FindOwner(*it) == nullptr) {
            it = pending_material_ids_.erase(it);
        } else {
            ++it;
        }
    }
}

void EntityRenderer::PrepareCpuGeometries() const {
    // 再同期が必要な描画オブジェクトを集める
    std::vector<i_graph::IEntityGraphics*> dirty;
    for (const auto& [id, graphics] : visible_list_) {
        if (graphics && graphics->NeedsResync()) dirty.push_back(graphics);
    }
    if (dirty.empty()) return;

    // CPU相 (テッセレーション・遅延生成等) を並列に前倒しする. PrewarmCpuはGL呼び出しを
    // 含まず、各オブジェクトが自身のステージングのみ書き込むためロックなしで並列実行できる.
    igesio::ParallelForEach(
            dirty, [](i_graph::IEntityGraphics* g) { g->PrewarmCpu(); });

    // 子の遅延生成・型確定でシェーダー型集合が変わりうるためバケットを作り直す
    draw_buckets_dirty_ = true;
}

void EntityRenderer::ResyncGeometries() const {
    // GPUへの転送 (Synchronize→DoSynchronize). PrepareCpuGeometriesで前倒し済みなら
    // 即転送される. 単一GLコンテキスト前提でこのスレッドで直列に行う.
    bool resynced = false;
    for (const auto& [id, graphics] : visible_list_) {
        if (graphics && graphics->NeedsResync()) {
            graphics->Synchronize();
            resynced = true;
        }
    }
    // 形状変更はBBoxも変えるため、自動クリップ球を追従させる
    if (resynced) UpdateAutoClipSphere();
}

void EntityRenderer::RebuildDrawBuckets() const {
    draw_list_.clear();
    for (const auto& [id, graphics] : visible_list_) {
        if (!graphics) continue;
        // バッチ描画のためシェーダー別に収集する (複合は子の各型へ収集される)
        for (const auto st : graphics->GetShaderTypes()) {
            if (HasSpecificShaderCode(st) && shader_programs_.count(st) > 0) {
                draw_list_[st].push_back(graphics);
            }
        }
    }
}

i_graph::IEntityGraphics* EntityRenderer::FindOrCreateGraphics(
        const ObjectID& id,
        const std::shared_ptr<entities::IEntityIdentifier>& entity) const {
    auto it = graphics_cache_.find(id);
    if (it != graphics_cache_.end()) {
        return it->second.get();  // nullptrは負キャッシュ (再試行しない)
    }

    // 遅延生成. 生成失敗 (型起因の恒久的失敗) はnullptrを負キャッシュする.
    // 同期はReconcile経路 (ResyncGeometries) で並列前倒し+直列GL転送するため、
    // ここでは生成時の同期を省く (synchronize=false)。
    auto graphics = CreateEntityGraphics(entity, gl_, /*synchronize=*/false);
    if (graphics) {
        graphics->SetGlobalParam(*default_global_param_);
        // ビュー層のマテリアルオーバーライドを生成時に適用する
        auto mat_it = material_overrides_.find(id);
        if (mat_it != material_overrides_.end()) {
            graphics->MaterialProperty() = mat_it->second;
        }
        graphics->SyncTexture();
        pending_material_ids_.erase(id);  // 生成時に適用済み
    }
    const auto result = graphics_cache_.emplace(id, std::move(graphics));
    return result.first->second.get();
}



/**
 * エンティティ以外の要素の取得/設定
 */

std::pair<int, int> EntityRenderer::GetDisplaySize() const {
    return {display_width_, display_height_};
}

void EntityRenderer::SetDisplaySize(const int width, const int height) {
    display_width_ = width;
    display_height_ = height;
}

std::array<float, 4> EntityRenderer::GetBackgroundColor() const {
    return background_color_;
}

std::array<float, 4>& EntityRenderer::GetBackgroundColorRef() {
    return background_color_;
}

void EntityRenderer::SetBackgroundColor(
        const float red, const float green,
        const float blue, const float alpha) {
    background_color_ = {red, green, blue, alpha};
}

std::array<float, 3> EntityRenderer::GetAmbientColor() const {
    return ambient_color_;
}

void EntityRenderer::SetAmbientColor(
        const float red, const float green, const float blue) {
    ambient_color_ = {red, green, blue};
}

void EntityRenderer::SetSettings(const GraphicsSettings& settings) {
    // アンチエイリアシング
    if (settings.enable_antialiasing != settings_.enable_antialiasing) {
        EnableAntialiasing(settings.enable_antialiasing);
    }

    // 透明度
    if (settings.enable_transparency != settings_.enable_transparency) {
        EnableTransparency(settings.enable_transparency);
    }

    // 表示モード (即時のGL状態変更は不要; 次回Draw時のシェーダー型フィルタで反映)
    settings_.display_mode = settings.display_mode;
}

void EntityRenderer::EnableAntialiasing(const bool enable) {
    settings_.enable_antialiasing = enable;
    if (settings_.enable_antialiasing) {
        gl_->Enable(gl::kMultisample);
    } else {
        gl_->Disable(gl::kMultisample);
    }
}

bool EntityRenderer::IsAntialiasingEnabled() const {
    return settings_.enable_antialiasing;
}

void EntityRenderer::EnableTransparency(const bool enable) {
    settings_.enable_transparency = enable;
    if (settings_.enable_transparency) {
        gl_->Enable(gl::kBlend);
        gl_->BlendFunc(gl::kSrcAlpha, gl::kOneMinusSrcAlpha);
    } else {
        gl_->Disable(gl::kBlend);
    }
}

bool EntityRenderer::IsTransparencyEnabled() const {
    return settings_.enable_transparency;
}

void EntityRenderer::SetDisplayMode(const DisplayMode mode) {
    settings_.display_mode = mode;
}

i_graph::DisplayMode EntityRenderer::GetDisplayMode() const {
    return settings_.display_mode;
}



/**
 * 描画
 */

void EntityRenderer::Draw() const {
    // 描画対象のサイズが0なら何もしない
    if (display_width_ <= 0 || display_height_ <= 0) return;

    // 背景色と深度バッファをクリア
    gl_->ClearColor(background_color_[0], background_color_[1],
                 background_color_[2], background_color_[3]);
    gl_->Clear(gl::kColorBufferBit | gl::kDepthBufferBit);

    // シーン(描画の基準ツリー)が未設定なら描画しない (描画はScene走査に一本化)
    if (scene_ == nullptr) return;

    // 現在の表示サイズと、このクラスが保持している表示サイズが異なる場合は更新
    auto [x, y, width, height] = GetCurrentViewport();
    if (width != display_width_ || height != display_height_) {
        gl_->Viewport(0, 0, display_width_, display_height_);
    }

    // 3フェーズ整流:
    // (1) EnsureSynced: モデルリビジョンと突き合わせ、可視リストを再構築 (構造)
    // (2) PrepareCpuGeometries: 形状のCPU準備を並列前倒し (遅延生成・テッセレーション)
    // (3) RebuildDrawBuckets: CPU状態確定後にシェーダー別バケットを構築 (変化時のみ)
    // (4) ResyncGeometries: GPUへ直列転送
    // カメラ操作・選択変更だけの再描画では走査・準備・再バケットを省きキャッシュを再利用する
    // (選択ハイライトは毎フレームctxからPULLされるため再walkは不要)
    EnsureSynced();
    PrepareCpuGeometries();
    if (draw_buckets_dirty_) {
        RebuildDrawBuckets();
        draw_buckets_dirty_ = false;
    }
    ResyncGeometries();

    // 描画するエンティティが1つもない場合は何もしない
    if (visible_list_.empty()) return;

    // 選択ハイライトをPULLするための表示コンテキスト (全シェーダーで共通)
    const DrawContext ctx{&scene_->ActiveSelection(), kSelectionColor};
    ExecuteDrawList(ctx);
}

void EntityRenderer::SetScene(const models::Scene* scene) {
    scene_ = scene;
    local_dirty_ = true;
    UpdateAutoClipSphere();
}

void EntityRenderer::UpdateAutoClipSphere() const {
    if (scene_ != nullptr) {
        if (const auto bbox = scene_->Root().GetWorldBoundingBox()) {
            if (const auto sphere = ComputeBoundingSphere(*bbox)) {
                camera_.SetAutoClipSphere(sphere->first, sphere->second);
                return;
            }
        }
    }
    camera_.ClearAutoClipSphere();
}

void EntityRenderer::FitView() {
    if (scene_ == nullptr) return;
    if (display_width_ <= 0 || display_height_ <= 0) return;

    const auto bbox = scene_->Root().GetWorldBoundingBox();
    if (!bbox.has_value()) return;

    const float aspect =
            static_cast<float>(display_width_) / display_height_;
    camera_.FitToBoundingBox(*bbox, aspect);
}

void EntityRenderer::RebuildDrawList() const {
    // draw_list_ (シェーダー別バケット) はRebuildDrawBucketsが管理する.
    // ここでは可視リストのみ再構築する
    visible_list_.clear();
    if (scene_ == nullptr) return;
    WalkAssembly(scene_->Root(), igesio::Matrix4d::Identity(),
                 std::nullopt, std::nullopt);
}

void EntityRenderer::WalkAssembly(
        const models::Assembly& node, const igesio::Matrix4d& parent_accum,
        const std::optional<std::array<float, 3>>& inherited_color,
        const std::optional<float>& inherited_opacity) const {
    const auto& disp = node.Display();
    // 非表示・抑制のサブツリーは描画対象から除外する
    if (!disp.visible || disp.suppressed) return;

    const igesio::Matrix4d accum = parent_accum * node.GetGlobalTransform();

    // 色/不透明度のオーバーライドは最近接が優先 (子の設定が祖先を上書きする)
    const auto color_ovr = disp.color_override ? disp.color_override : inherited_color;
    const auto opacity_ovr =
            disp.opacity_override ? disp.opacity_override : inherited_opacity;

    for (const auto& [id, entity] : node.GetEntities()) {
        if (!entity) continue;
        // 物理従属エンティティは親 (複合曲線・トリム面等) の描画オブジェクトが
        // 子として描画するため、独立した描画対象としない
        // (二重描画と重複ピックの防止)
        // (従属スイッチはDEステータス由来のためEntityBaseのみが持つ.
        //  非IGESエンティティは常に独立扱い=描画対象)
        const auto eb = std::dynamic_pointer_cast<entities::EntityBase>(entity);
        if (eb && eb->GetSubordinateEntitySwitch()
                == entities::SubordinateEntitySwitch::kPhysicallyDependent) {
            continue;
        }
        // ビュー状態の型フィルタ (キャッシュは温存し、収集のみ抑止する)
        if (!display_filter_.ShouldRender(entity->GetType())) continue;

        // キャッシュ未在席なら遅延生成する (負キャッシュのnullptrはスキップ)
        auto* graphics = FindOrCreateGraphics(id, entity);
        if (graphics == nullptr) continue;

        // マテリアルの遅延適用 (setterはGLを触らないため、GL前提のここで行う)
        if (pending_material_ids_.erase(id) > 0) {
            auto mat_it = material_overrides_.find(id);
            graphics->MaterialProperty() =
                    (mat_it != material_overrides_.end())
                    ? mat_it->second : i_graph::MaterialProperty();
            graphics->SyncTexture();
        }

        // ピッキング/描画の整合のためワールド変換をリフレッシュする.
        // M_entityは含めない (各描画オブジェクトが内部で処理するため二重適用を避ける).
        // 正準値はdoubleで保持し、単精度化はGPUアップロード時のみ行う
        graphics->SetWorldTransform(accum);

        // 解決したオーバーライドをフレーム毎にPUSHする (world_transform_と同じ派生キャッシュ).
        // まずentity固有色へ戻し、指定があるRGB/不透明度のみ差し替える
        // (選択ハイライトは描画時にPULLされ、これより優先される)
        graphics->ResetColor();
        if (color_ovr || opacity_ovr) {
            const auto natural = graphics->GetColor();  // {base_rgb, material_opacity}
            graphics->SetColor({
                color_ovr ? (*color_ovr)[0] : natural[0],
                color_ovr ? (*color_ovr)[1] : natural[1],
                color_ovr ? (*color_ovr)[2] : natural[2],
                opacity_ovr ? *opacity_ovr : natural[3]});
        }

        // ピック用の平坦リストへエンティティ毎に一意に収集する.
        // シェーダー別バケット (draw_list_) はCPU準備フェーズ後にRebuildDrawBucketsで
        // 構築する (遅延生成の子・テッセレーション結果で型集合が確定してから振り分けるため)
        visible_list_.emplace_back(id, graphics);
    }

    for (const auto& child : node.GetChildAssemblies()) {
        if (child) WalkAssembly(*child, accum, color_ovr, opacity_ovr);
    }
}

void EntityRenderer::ExecuteDrawList(const DrawContext& ctx) const {
    const auto view_matrix = camera_.GetViewMatrix();
    const auto projection_matrix = camera_.GetProjectionMatrix(
        static_cast<float>(display_width_) / display_height_);
    const std::pair<float, float> viewport{
        static_cast<float>(display_width_), static_cast<float>(display_height_)};

    // 光源パラメータを平坦バッファへ詰める (シェーダー間で共有のため一度だけ構築)
    const int num_lights = std::min(static_cast<int>(lights_.size()), kMaxLights);
    std::vector<float> light_pos(3 * num_lights);
    std::vector<float> light_att(3 * num_lights);
    std::vector<float> light_col(4 * num_lights);
    for (int i = 0; i < num_lights; ++i) {
        const auto& l = lights_[i];
        std::copy_n(l.position.data(), 3, light_pos.begin() + 3 * i);
        std::copy_n(l.attenuation.data(), 3, light_att.begin() + 3 * i);
        std::copy_n(l.color.data(), 4, light_col.begin() + 4 * i);
    }

    for (const auto& [shader_type, program_id] : shader_programs_) {
        // 表示モードに応じて面塗り/面エッジのシェーダー型を取捨する
        if (!ShouldDrawShaderType(shader_type, settings_.display_mode)) continue;

        auto it = draw_list_.find(shader_type);
        if (it == draw_list_.end() || it->second.empty()) continue;

        gl_->UseProgram(program_id);
        gl_->UniformMatrix4fv(gl_->GetUniformLocation(program_id, "view"),
                              1, gl::kFalse, view_matrix.data());
        gl_->UniformMatrix4fv(gl_->GetUniformLocation(program_id, "projection"),
                              1, gl::kFalse, projection_matrix.data());

        // 面塗りは真の深度のまま描く (奥へオフセットしない). 面上に乗る曲線・
        // エッジは曲線フラグメントシェーダ側で局所深度勾配に比例した分だけ手前へ
        // 出すため、面の奥にある曲線は真の前面深度で正しく遮蔽される.

        // 光源のパラメータを設定 (配列uniformとして送信)
        if (UsesLighting(shader_type)) {
            // 視点位置 (鏡面・アンビエントFresnelで使用) と環境光色を送信
            gl_->Uniform3fv(gl_->GetUniformLocation(program_id, "viewPos_WorldSpace"),
                            1, camera_.GetPosition().data());
            gl_->Uniform3fv(gl_->GetUniformLocation(program_id, "ambientColor"),
                            1, ambient_color_.data());
            gl_->Uniform1i(gl_->GetUniformLocation(program_id, "numLights"),
                           num_lights);
            if (num_lights > 0) {
                gl_->Uniform3fv(gl_->GetUniformLocation(program_id, "lightPositions"),
                                num_lights, light_pos.data());
                gl_->Uniform3fv(gl_->GetUniformLocation(program_id, "lightAttenuations"),
                                num_lights, light_att.data());
                gl_->Uniform4fv(gl_->GetUniformLocation(program_id, "lightColors"),
                                num_lights, light_col.data());
            }
        }

        for (auto* graphics : it->second) {
            if (graphics && graphics->IsDrawable()) {
                graphics->Draw(program_id, shader_type, viewport, ctx);
            }
        }
    }
}

i_graph::Texture EntityRenderer::CaptureScreenshot() const {
    auto [width, height] = GetDisplaySize();
    if (width <= 0 || height <= 0) {
        return {};  // サイズが無効な場合は空のベクターを返す
    }

    // 呼び出し前にバインドされていたFBOを退避する.
    // QtのQOpenGLWidget等、既定のFBOが0でない環境でも末尾で正しく復元するため
    gl::Int prev_fbo = 0;
    gl_->GetIntegerv(gl::kFramebufferBinding, &prev_fbo);

    // フレームバッファオブジェクトの準備
    gl::Uint fbo;
    gl_->GenFramebuffers(1, &fbo);
    gl_->BindFramebuffer(gl::kFramebuffer, fbo);

    // テクスチャを作成 (カラーバッファ用)
    gl::Uint color_tex;
    gl_->GenTextures(1, &color_tex);
    gl_->BindTexture(gl::kTexture2D, color_tex);
    gl_->TexImage2D(gl::kTexture2D, 0, gl::kRgb, width, height,
                    0, gl::kRgb, gl::kUnsignedByte, nullptr);
    gl_->TexParameteri(gl::kTexture2D, gl::kTextureMinFilter, gl::kLinear);
    gl_->TexParameteri(gl::kTexture2D, gl::kTextureMagFilter, gl::kLinear);

    // レンダーバッファオブジェクトの準備 (深度バッファ用)
    gl::Uint rbo;
    gl_->GenRenderbuffers(1, &rbo);
    gl_->BindRenderbuffer(gl::kRenderbuffer, rbo);
    gl_->RenderbufferStorage(gl::kRenderbuffer, gl::kDepth24Stencil8, width, height);

    // テクスチャ/RBOをFBOにアタッチ
    gl_->FramebufferTexture2D(gl::kFramebuffer, gl::kColorAttachment0,
                              gl::kTexture2D, color_tex, 0);
    gl_->FramebufferRenderbuffer(gl::kFramebuffer, gl::kDepthStencilAttachment,
                                  gl::kRenderbuffer, rbo);

    // FBOの準備に成功した場合のみ描画と読み取りを行う
    std::vector<unsigned char> pixels;
    if (gl_->CheckFramebufferStatus(gl::kFramebuffer) == gl::kFramebufferComplete) {
        Draw();
        pixels.resize(width * height * 3);  // RGB形式
        gl_->ReadPixels(0, 0, width, height, gl::kRgb, gl::kUnsignedByte, pixels.data());
    }

    // FBOを0固定ではなく、呼び出し前にバインドされていた状態へ復元する
    gl_->BindFramebuffer(gl::kFramebuffer, static_cast<gl::Uint>(prev_fbo));
    gl_->DeleteFramebuffers(1, &fbo);
    gl_->DeleteTextures(1, &color_tex);
    gl_->DeleteRenderbuffers(1, &rbo);

    Texture tex;
    tex.SetData(width, height, false, pixels.data(), false, true);
    return tex;
}



/**
 * ピッキング・選択
 */

i_graph::Ray EntityRenderer::GetRayFromScreen(
        double screen_x, double screen_y) const {
    return i_graph::GetRayFromScreen(
            camera_, display_width_, display_height_, screen_x, screen_y);
}

std::vector<i_graph::EntityHit> EntityRenderer::PickEntities(
        const Ray& ray, double screen_x, double screen_y,
        const RayIntersectionParams& params) const {
    std::vector<EntityHit> hits;
    const int w = display_width_, h = display_height_;
    if (w <= 0 || h <= 0) return hits;

    // 描画キャッシュをツリーと突き合わせる. これにより削除済み・非表示・抑制中・
    // フィルタ除外のエンティティは構造的にヒット不能になる
    EnsureSynced();
    // 遅延生成 (142の子C(t)) 等のCPU状態を確定させる (CanIntersect/Intersectが成立する).
    // GPU転送は不要なためResyncGeometriesは呼ばない
    PrepareCpuGeometries();

    // フォールバック深度: カメラのターゲットをレイへ射影した距離
    const igesio::Vector3d target = camera_.GetTarget().cast<double>();
    const igesio::Vector3d position = camera_.GetPosition().cast<double>();
    double fallback_depth = (target - ray.origin).dot(ray.direction);
    if (fallback_depth <= 0.0) fallback_depth = (target - position).norm();
    if (fallback_depth <= 0.0) fallback_depth = 1.0;  // 最終フォールバック

    // 可視リストを走査する (draw_list_は複合を複数シェーダーへ重複登録するため、
    // エンティティ毎に一意なこちらで二重判定を避ける)
    for (const auto& [id, object] : visible_list_) {
        if (!object || !object->CanIntersect()) continue;

        // 代表深度: BB中心をレイへ射影。BBが無限/未定義ならフォールバック
        double depth = fallback_depth;
        const auto bb = object->GetWorldBoundingBox();
        if (bb && bb->IsFinite()) {
            const auto vertices = bb->GetFiniteVertices();
            if (!vertices.empty()) {
                const igesio::Vector3d center = Centroid(vertices);
                depth = (center - ray.origin).dot(ray.direction);
            }
        }
        if (depth <= 0.0) depth = fallback_depth;

        // 曲線ヒット許容量をワールド距離へ換算してパラメータを上書き
        RayIntersectionParams p = params;
        p.curve_hit_tolerance = i_graph::PixelToWorldSize(
                camera_, w, h, screen_x, screen_y, depth,
                params.curve_hit_pixels);

        for (const auto& rh : object->Intersect(ray, p)) {
            hits.push_back({id, rh});
        }
    }

    // distance昇順にソート
    std::sort(hits.begin(), hits.end(),
              [](const EntityHit& a, const EntityHit& b) {
                  return a.hit.distance < b.hit.distance;
              });

    // 同一IDかつposition近接 (dedup_tol以内) のヒットは近い方のみ残す
    std::vector<EntityHit> deduped;
    for (const auto& hit : hits) {
        bool duplicate = false;
        for (const auto& kept : deduped) {
            if (kept.id == hit.id &&
                (kept.hit.position - hit.hit.position).norm() < params.dedup_tol) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) deduped.push_back(hit);
    }
    return deduped;
}

std::vector<igesio::ObjectID> EntityRenderer::PickEntitiesInRect(
        const ScreenRect& rect, BoxSelectionMode mode,
        const SelectionSampleParams& params) const {
    std::vector<igesio::ObjectID> result;
    const int w = display_width_, h = display_height_;
    if (w <= 0 || h <= 0) return result;

    // 描画キャッシュをツリーと突き合わせる (PickEntitiesと同様)
    EnsureSynced();
    // 遅延生成等のCPU状態を確定させる (GetSelectionSamples/境界が成立する)
    PrepareCpuGeometries();

    // VP行列を一度だけ計算し、各サンプルの射影で使い回す
    const float aspect = static_cast<float>(w) / static_cast<float>(h);
    const igesio::Matrix4d vp =
            camera_.GetProjectionMatrix(aspect).cast<double>()
            * camera_.GetViewMatrix().cast<double>();

    // 曲線サンプリングを画面弦長基準で適応的に細分する設定
    SelectionSampleParams sp = params;
    sp.adaptive_refine = true;
    sp.adaptive_view_proj = vp;
    sp.adaptive_width = w;
    sp.adaptive_height = h;

    for (const auto& [id, object] : visible_list_) {
        if (!object) continue;

        // 粗カリングを先に行い、範囲外エンティティのサンプリングを省く
        if (!MayOverlapRect(object->GetWorldBoundingBox(), rect, vp, w, h)) {
            continue;
        }

        const auto samples = object->GetSelectionSamples(sp);
        if (samples.polylines.empty() && samples.points.empty()) continue;

        const bool hit = (mode == BoxSelectionMode::kContained)
                ? ContainedInRect(samples, rect, vp, w, h)
                : CrossesRect(samples, rect, vp, w, h);
        if (hit) result.push_back(id);
    }
    return result;
}

i_graph::IEntityGraphics* EntityRenderer::FindGraphics(const ObjectID& id) const {
    auto it = graphics_cache_.find(id);
    if (it != graphics_cache_.end()) {
        return it->second.get();
    }
    return nullptr;
}




/**
 * protected member functions
 */

std::array<int, 4> EntityRenderer::GetCurrentViewport() const {
    gl::Int viewport[4];
    gl_->GetIntegerv(gl::kViewport, viewport);
    return {viewport[0], viewport[1], viewport[2], viewport[3]};
}



/**
 * private member functions
 */

gl::Uint EntityRenderer::CompileVertexShader(const std::string& vertex_source) {
    if (vertex_source.empty()) {
        throw igesio::ImplementationError("Vertex shader source is empty");
    }

    gl::Uint vertex_shader = gl_->CreateShader(gl::kVertexShader);
    const char* source_cstr = vertex_source.c_str();
    gl_->ShaderSource(vertex_shader, 1, &source_cstr, nullptr);
    gl_->CompileShader(vertex_shader);

    // エラーチェック
    int success;
    char info_log[512];
    gl_->GetShaderiv(vertex_shader, gl::kCompileStatus, &success);
    if (!success) {
        gl_->GetShaderInfoLog(vertex_shader, 512, nullptr, info_log);
        gl_->DeleteShader(vertex_shader);
        throw igesio::ImplementationError(
            "Failed to compile vertex shader: " + std::string(info_log));
    }
    return vertex_shader;
}

gl::Uint EntityRenderer::CompileGeometryShader(const std::string& geometry_source) {
    if (geometry_source.empty()) {
        return 0;  // ジオメトリシェーダーが空の場合は0を返す
    }

    gl::Uint geometry_shader = gl_->CreateShader(gl::kGeometryShader);
    const char* source_cstr = geometry_source.c_str();
    gl_->ShaderSource(geometry_shader, 1, &source_cstr, nullptr);
    gl_->CompileShader(geometry_shader);

    // エラーチェック
    int success;
    char info_log[512];
    gl_->GetShaderiv(geometry_shader, gl::kCompileStatus, &success);
    if (!success) {
        gl_->GetShaderInfoLog(geometry_shader, 512, nullptr, info_log);
        gl_->DeleteShader(geometry_shader);
        throw igesio::ImplementationError(
            "Failed to compile geometry shader: " + std::string(info_log));
    }
    return geometry_shader;
}

std::pair<float, float> EntityRenderer::CompileTCSAndTES(
        const std::string& tcs_source, const std::string& tes_source) {
    if (tcs_source.empty() || tes_source.empty()) {
        return {0, 0};  // TCSまたはTESが空の場合は0を返す
    }

    gl::Uint tcs_shader = gl_->CreateShader(gl::kTessControlShader);
    const char* tcs_cstr = tcs_source.c_str();
    gl_->ShaderSource(tcs_shader, 1, &tcs_cstr, nullptr);
    gl_->CompileShader(tcs_shader);

    // エラーチェック
    int success;
    char info_log[512];
    gl_->GetShaderiv(tcs_shader, gl::kCompileStatus, &success);
    if (!success) {
        gl_->GetShaderInfoLog(tcs_shader, 512, nullptr, info_log);
        gl_->DeleteShader(tcs_shader);
        throw igesio::ImplementationError(
            "Failed to compile tessellation control shader: " + std::string(info_log));
    }

    gl::Uint tes_shader = gl_->CreateShader(gl::kTessEvaluationShader);
    const char* tes_cstr = tes_source.c_str();
    gl_->ShaderSource(tes_shader, 1, &tes_cstr, nullptr);
    gl_->CompileShader(tes_shader);

    // エラーチェック
    gl_->GetShaderiv(tes_shader, gl::kCompileStatus, &success);
    if (!success) {
        gl_->GetShaderInfoLog(tes_shader, 512, nullptr, info_log);
        gl_->DeleteShader(tes_shader);
        throw igesio::ImplementationError(
            "Failed to compile tessellation evaluation shader: " + std::string(info_log));
    }

    return {tcs_shader, tes_shader};
}

gl::Uint EntityRenderer::CompileFragmentShader(const std::string& fragment_source) {
    if (fragment_source.empty()) {
        throw igesio::ImplementationError("Fragment shader source is empty");
    }

    gl::Uint fragment_shader = gl_->CreateShader(gl::kFragmentShader);
    const char* source_cstr = fragment_source.c_str();
    gl_->ShaderSource(fragment_shader, 1, &source_cstr, nullptr);
    gl_->CompileShader(fragment_shader);

    // エラーチェック
    int success;
    char info_log[512];
    gl_->GetShaderiv(fragment_shader, gl::kCompileStatus, &success);
    if (!success) {
        gl_->GetShaderInfoLog(fragment_shader, 512, nullptr, info_log);
        gl_->DeleteShader(fragment_shader);
        throw igesio::ImplementationError(
            "Failed to compile fragment shader: " + std::string(info_log));
    }
    return fragment_shader;
}

gl::Uint EntityRenderer::CreateShaderProgram(
        gl::Uint vertex_shader, gl::Uint fragment_shader,
        gl::Uint geometry_shader, gl::Uint tcs_shader, gl::Uint tes_shader) {
    gl::Uint program_id = gl_->CreateProgram();

    if (vertex_shader == 0 || fragment_shader == 0) {
        throw igesio::ImplementationError("Vertex or Fragment shader is not provided.");
    }

    // 存在する各シェーダーをプログラムにリンクする
    gl_->AttachShader(program_id, vertex_shader);
    if (geometry_shader != 0) gl_->AttachShader(program_id, geometry_shader);
    if (tcs_shader != 0 && tes_shader != 0) {
        gl_->AttachShader(program_id, tcs_shader);
        gl_->AttachShader(program_id, tes_shader);
    }
    gl_->AttachShader(program_id, fragment_shader);
    gl_->LinkProgram(program_id);

    // リンクのエラーチェック
    int success;
    char info_log[512];
    gl_->GetProgramiv(program_id, gl::kLinkStatus, &success);
    if (!success) {
        gl_->GetProgramInfoLog(program_id, 512, nullptr, info_log);
        throw igesio::ImplementationError(
            "Failed to link shader program: " + std::string(info_log));
    }

    return program_id;
}

void EntityRenderer::InitShaders() {
    for (int i = 0; i < static_cast<int>(ShaderType::kNone); ++i) {
        auto shader_type = static_cast<ShaderType>(i);
        std::optional<shaders::ShaderCode> shader_opt;
        try {
            shader_opt = shaders::GetShaderCode(shader_type);
        } catch (const igesio::FileError& e) {
            // #includeするファイルが見つからない場合はエラーをスロー
            throw igesio::ImplementationError(std::string(e.what()));
        }
        if (!shader_opt || shader_opt->IsIncomplete()) {
            continue;  // シェーダーが未実装の場合はスキップ
        }
        auto code = *shader_opt;

        gl::Uint vertex_shader = 0, geometry_shader = 0,
               tcs_shader = 0, tes_shader = 0, fragment_shader = 0,
               program_id = 0;

        try {
            // 各シェーダーをコンパイルする (存在しない場合は0が返る)
            vertex_shader = CompileVertexShader(code.vertex);
            geometry_shader = CompileGeometryShader(code.geometry);
            std::tie(tcs_shader, tes_shader) = CompileTCSAndTES(code.tcs, code.tes);
            fragment_shader = CompileFragmentShader(code.fragment);

            // 各シェーダーをリンクしてプログラムを作成
            program_id = CreateShaderProgram(
                vertex_shader, fragment_shader,
                geometry_shader, tcs_shader, tes_shader);

            // リンク後、各シェーダーオブジェクトを削除
            gl_->DeleteShader(vertex_shader);
            if (geometry_shader != 0) gl_->DeleteShader(geometry_shader);
            if (tcs_shader != 0) gl_->DeleteShader(tcs_shader);
            if (tes_shader != 0) gl_->DeleteShader(tes_shader);
            gl_->DeleteShader(fragment_shader);
        } catch (const igesio::ImplementationError& e) {
            // エラーが発生した場合はリソースを解放
            if (vertex_shader != 0) gl_->DeleteShader(vertex_shader);
            if (geometry_shader != 0) gl_->DeleteShader(geometry_shader);
            if (tcs_shader != 0) gl_->DeleteShader(tcs_shader);
            if (tes_shader != 0) gl_->DeleteShader(tes_shader);
            if (fragment_shader != 0) gl_->DeleteShader(fragment_shader);
            if (program_id != 0) gl_->DeleteProgram(program_id);
            throw e;  // エラーを再スロー
        }

        shader_programs_[shader_type] = program_id;
    }
}
