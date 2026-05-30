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

#include "./shaders.h"

namespace {

namespace i_graph = igesio::graphics;
using EntityRenderer = igesio::graphics::EntityRenderer;

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



bool EntityRenderer::IsInitialized() const {
    return !shader_programs_.empty();
}

void EntityRenderer::Initialize() {
    InitShaders();

    // 深度テストを有効化
    gl_->Enable(GL_DEPTH_TEST);

    // アンチエイリアシングの初期化
    EnableAntialiasing(settings_.enable_antialiasing);
    // ブレンド設定
    EnableTransparency(settings_.enable_transparency);
}

void EntityRenderer::Cleanup() {
    // 描画オブジェクトのクリーンアップ
    for (auto& [shader_type, objects] : draw_objects_) {
        for (auto& [id, object] : objects) {
            object->Cleanup();
        }
        objects.clear();
    }
    draw_objects_.clear();
    // キャッシュ描画リストも破棄する (draw_objects_の要素を指すため)
    draw_list_.clear();
    scene_dirty_ = true;

    // シェーダープログラムの削除
    for (auto& [shader_type, program_id] : shader_programs_) {
        gl_->DeleteProgram(program_id);
    }
    shader_programs_.clear();

    // OpenGLリソースの解放
    gl_->Disable(GL_DEPTH_TEST);
}



/**
 * エンティティの取得/設定
 */

void EntityRenderer::AddGraphicsObject(
        std::unique_ptr<IEntityGraphics>&& graphics) {
    if (!graphics) return;

    if (HasEntity(graphics->GetEntityID())) {
        // すでに同じIDのエンティティが存在する場合は何もしない
        return;
    }

    // 新しい描画オブジェクトを追加
    draw_objects_[graphics->GetShaderType()][graphics->GetEntityID()]
            = std::move(graphics);
    // 走査対象が増えたので次回描画で描画リストを再構築する
    scene_dirty_ = true;
}

void EntityRenderer::RemoveEntity(const ObjectID& id) {
    for (auto& [shader_type, objects] : draw_objects_) {
        auto it = objects.find(id);
        if (it != objects.end()) {
            it->second->Cleanup();  // OpenGLリソースを解放
            objects.erase(it);  // 描画オブジェクトを削除
            // draw_list_は当該ポインタを保持し得るため破棄し、次回再構築する
            draw_list_.clear();
            scene_dirty_ = true;
            return;  // 削除が完了したら終了
        }
    }
}

bool EntityRenderer::HasEntity(const ObjectID& id) const {
    // draw_objects_を走査
    for (const auto& [shader_type, objects] : draw_objects_) {
        if (objects.find(id) != objects.end()) {
            return true;
        }
    }
    return false;
}

i_graph::ShaderType
EntityRenderer::GetEntityShaderType(const ObjectID& id) const {
    for (const auto& [shader_type, objects] : draw_objects_) {
        if (objects.find(id) != objects.end()) {
            return shader_type;
        }
    }
    return ShaderType::kNone;
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

void EntityRenderer::SetSettings(const GraphicsSettings& settings) {
    // アンチエイリアシング
    if (settings.enable_antialiasing != settings_.enable_antialiasing) {
        EnableAntialiasing(settings.enable_antialiasing);
    }

    // 透明度
    if (settings.enable_transparency != settings_.enable_transparency) {
        EnableTransparency(settings.enable_transparency);
    }
}

void EntityRenderer::EnableAntialiasing(const bool enable) {
    settings_.enable_antialiasing = enable;
    if (settings_.enable_antialiasing) {
        gl_->Enable(GL_MULTISAMPLE);
    } else {
        gl_->Disable(GL_MULTISAMPLE);
    }
}

bool EntityRenderer::IsAntialiasingEnabled() const {
    return settings_.enable_antialiasing;
}

void EntityRenderer::EnableTransparency(const bool enable) {
    settings_.enable_transparency = enable;
    if (settings_.enable_transparency) {
        gl_->Enable(GL_BLEND);
        gl_->BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        gl_->Disable(GL_BLEND);
    }
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
    gl_->Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 描画するエンティティが1つもない場合は何もしない
    if (IsEmpty()) return;

    // 現在の表示サイズと、このクラスが保持している表示サイズが異なる場合は更新
    auto [x, y, width, height] = GetCurrentViewport();
    if (width != display_width_ || height != display_height_) {
        gl_->Viewport(0, 0, display_width_, display_height_);
    }

    // シーン(権威ツリー)が未設定なら描画しない (描画はScene走査に一本化)
    if (scene_ == nullptr) return;

    // 選択ハイライトをPULLするための表示コンテキスト (全シェーダーで共通)
    const DrawContext ctx{&scene_->ActiveSelection(), kSelectionColor};

    // dirty時のみ描画リストを再構築し (Assemblyツリー走査)、それを実行する.
    // カメラ操作・選択変更だけの再描画では走査せずキャッシュを再利用する
    // (選択ハイライトは毎フレームctxからPULLされるため再walkは不要)
    if (scene_dirty_) {
        RebuildDrawList();
        scene_dirty_ = false;
    }
    ExecuteDrawList(ctx);
}

void EntityRenderer::SetScene(const models::Scene* scene) {
    scene_ = scene;
    scene_dirty_ = true;
}

void EntityRenderer::MarkSceneDirty() {
    scene_dirty_ = true;
}

void EntityRenderer::RebuildDrawList() const {
    draw_list_.clear();
    if (scene_ == nullptr) return;
    WalkAssembly(scene_->Root(), igesio::Matrix4d::Identity(),
                 std::nullopt, std::nullopt);
}

void EntityRenderer::WalkAssembly(
        const models::Assembly& node, const igesio::Matrix4d& parent_accum,
        const std::optional<std::array<float, 3>>& inherited_color,
        const std::optional<float>& inherited_opacity) const {
    const auto& meta = node.Metadata();
    // 非表示・抑制のサブツリーは描画対象から除外する
    if (!meta.visible || meta.suppressed) return;

    const igesio::Matrix4d accum = parent_accum * node.GetGlobalTransform();
    const igesio::Matrix4f accum_f = accum.cast<float>();

    // 色/不透明度のオーバーライドは最近接が優先 (子の設定が祖先を上書きする)
    const auto color_ovr = meta.color_override ? meta.color_override : inherited_color;
    const auto opacity_ovr =
            meta.opacity_override ? meta.opacity_override : inherited_opacity;

    for (const auto& [id, entity] : node.GetEntities()) {
        // キャッシュに在席する = 投入時フィルタを通過した描画対象
        // (ここではShouldRenderEntity/Validateを呼ばない)
        auto* graphics = FindGraphics(id);
        if (graphics == nullptr) continue;

        // ピッキング/描画の整合のためワールド変換をリフレッシュする.
        // M_entityは含めない (各描画オブジェクトが内部で処理するため二重適用を避ける)
        graphics->SetWorldTransform(accum_f);

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

        // バッチ描画のためシェーダー別に収集する (複合は子の各型へ収集される)
        for (const auto st : graphics->GetShaderTypes()) {
            if (HasSpecificShaderCode(st) && shader_programs_.count(st) > 0) {
                draw_list_[st].push_back(graphics);
            }
        }
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

    for (const auto& [shader_type, program_id] : shader_programs_) {
        auto it = draw_list_.find(shader_type);
        if (it == draw_list_.end() || it->second.empty()) continue;

        gl_->UseProgram(program_id);
        gl_->UniformMatrix4fv(gl_->GetUniformLocation(program_id, "view"),
                              1, GL_FALSE, view_matrix.data());
        gl_->UniformMatrix4fv(gl_->GetUniformLocation(program_id, "projection"),
                              1, GL_FALSE, projection_matrix.data());

        // 光源のパラメータを設定
        if (UsesLighting(shader_type)) {
            gl_->Uniform3fv(gl_->GetUniformLocation(program_id, "lightPos_WorldSpace"),
                            1, light_.position.data());
            gl_->Uniform3fv(gl_->GetUniformLocation(program_id, "lightAttenuation"),
                            1, light_.attenuation.data());
            gl_->Uniform4fv(gl_->GetUniformLocation(program_id, "lightColor"),
                            1, light_.color.data());
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

    // フレームバッファオブジェクトの準備
    GLuint fbo;
    gl_->GenFramebuffers(1, &fbo);
    gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo);

    // テクスチャを作成 (カラーバッファ用)
    GLuint color_tex;
    gl_->GenTextures(1, &color_tex);
    gl_->BindTexture(GL_TEXTURE_2D, color_tex);
    gl_->TexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height,
                    0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // レンダーバッファオブジェクトの準備 (深度バッファ用)
    GLuint rbo;
    gl_->GenRenderbuffers(1, &rbo);
    gl_->BindRenderbuffer(GL_RENDERBUFFER, rbo);
    gl_->RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

    // テクスチャ/RBOをFBOにアタッチ
    gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D, color_tex, 0);
    gl_->FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                  GL_RENDERBUFFER, rbo);

    // FBOの準備に成功した場合のみ描画と読み取りを行う
    std::vector<unsigned char> pixels;
    if (gl_->CheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
        Draw();
        pixels.resize(width * height * 3);  // RGB形式
        gl_->ReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    }

    gl_->BindFramebuffer(GL_FRAMEBUFFER, 0);
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

    // フォールバック深度: カメラのターゲットをレイへ射影した距離
    const igesio::Vector3d target = camera_.GetTarget().cast<double>();
    const igesio::Vector3d position = camera_.GetPosition().cast<double>();
    double fallback_depth = (target - ray.origin).dot(ray.direction);
    if (fallback_depth <= 0.0) fallback_depth = (target - position).norm();
    if (fallback_depth <= 0.0) fallback_depth = 1.0;  // 最終フォールバック

    for (const auto& [shader_type, objects] : draw_objects_) {
        for (const auto& [id, object] : objects) {
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

    for (const auto& [shader_type, objects] : draw_objects_) {
        for (const auto& [id, object] : objects) {
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
    }
    return result;
}

i_graph::IEntityGraphics* EntityRenderer::FindGraphics(const ObjectID& id) const {
    for (const auto& [shader_type, objects] : draw_objects_) {
        auto it = objects.find(id);
        if (it != objects.end()) {
            return it->second.get();
        }
    }
    return nullptr;
}




/**
 * protected member functions
 */

std::array<int, 4> EntityRenderer::GetCurrentViewport() const {
    GLint viewport[4];
    gl_->GetIntegerv(GL_VIEWPORT, viewport);
    return {viewport[0], viewport[1], viewport[2], viewport[3]};
}



/**
 * private member functions
 */

GLuint EntityRenderer::CompileVertexShader(const std::string& vertex_source) {
    if (vertex_source.empty()) {
        throw igesio::ImplementationError("Vertex shader source is empty");
    }

    GLuint vertex_shader = gl_->CreateShader(GL_VERTEX_SHADER);
    const char* source_cstr = vertex_source.c_str();
    gl_->ShaderSource(vertex_shader, 1, &source_cstr, nullptr);
    gl_->CompileShader(vertex_shader);

    // エラーチェック
    int success;
    char info_log[512];
    gl_->GetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        gl_->GetShaderInfoLog(vertex_shader, 512, nullptr, info_log);
        gl_->DeleteShader(vertex_shader);
        throw igesio::ImplementationError(
            "Failed to compile vertex shader: " + std::string(info_log));
    }
    return vertex_shader;
}

GLuint EntityRenderer::CompileGeometryShader(const std::string& geometry_source) {
    if (geometry_source.empty()) {
        return 0;  // ジオメトリシェーダーが空の場合は0を返す
    }

    GLuint geometry_shader = gl_->CreateShader(GL_GEOMETRY_SHADER);
    const char* source_cstr = geometry_source.c_str();
    gl_->ShaderSource(geometry_shader, 1, &source_cstr, nullptr);
    gl_->CompileShader(geometry_shader);

    // エラーチェック
    int success;
    char info_log[512];
    gl_->GetShaderiv(geometry_shader, GL_COMPILE_STATUS, &success);
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

    GLuint tcs_shader = gl_->CreateShader(GL_TESS_CONTROL_SHADER);
    const char* tcs_cstr = tcs_source.c_str();
    gl_->ShaderSource(tcs_shader, 1, &tcs_cstr, nullptr);
    gl_->CompileShader(tcs_shader);

    // エラーチェック
    int success;
    char info_log[512];
    gl_->GetShaderiv(tcs_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        gl_->GetShaderInfoLog(tcs_shader, 512, nullptr, info_log);
        gl_->DeleteShader(tcs_shader);
        throw igesio::ImplementationError(
            "Failed to compile tessellation control shader: " + std::string(info_log));
    }

    GLuint tes_shader = gl_->CreateShader(GL_TESS_EVALUATION_SHADER);
    const char* tes_cstr = tes_source.c_str();
    gl_->ShaderSource(tes_shader, 1, &tes_cstr, nullptr);
    gl_->CompileShader(tes_shader);

    // エラーチェック
    gl_->GetShaderiv(tes_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        gl_->GetShaderInfoLog(tes_shader, 512, nullptr, info_log);
        gl_->DeleteShader(tes_shader);
        throw igesio::ImplementationError(
            "Failed to compile tessellation evaluation shader: " + std::string(info_log));
    }

    return {tcs_shader, tes_shader};
}

GLuint EntityRenderer::CompileFragmentShader(const std::string& fragment_source) {
    if (fragment_source.empty()) {
        throw igesio::ImplementationError("Fragment shader source is empty");
    }

    GLuint fragment_shader = gl_->CreateShader(GL_FRAGMENT_SHADER);
    const char* source_cstr = fragment_source.c_str();
    gl_->ShaderSource(fragment_shader, 1, &source_cstr, nullptr);
    gl_->CompileShader(fragment_shader);

    // エラーチェック
    int success;
    char info_log[512];
    gl_->GetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        gl_->GetShaderInfoLog(fragment_shader, 512, nullptr, info_log);
        gl_->DeleteShader(fragment_shader);
        throw igesio::ImplementationError(
            "Failed to compile fragment shader: " + std::string(info_log));
    }
    return fragment_shader;
}

GLuint EntityRenderer::CreateShaderProgram(
        GLuint vertex_shader, GLuint fragment_shader,
        GLuint geometry_shader, GLuint tcs_shader, GLuint tes_shader) {
    GLuint program_id = gl_->CreateProgram();

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
    gl_->GetProgramiv(program_id, GL_LINK_STATUS, &success);
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

        GLuint vertex_shader = 0, geometry_shader = 0,
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

bool EntityRenderer::IsEmpty() const {
    if (draw_objects_.empty()) return true;

    // 1つもエンティティ（描画オブジェクト）が設定されていない場合は空
    for (const auto& [shader_type, objects] : draw_objects_) {
        if (!objects.empty()) return false;
    }
    return true;
}
