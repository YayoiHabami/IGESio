/**
 * @file examples/gui/machining_view_pane.cpp
 * @brief 簡易CAM GUIの1ビュー (レンダラ、カメラ、FBO、マウス操作) の実装
 * @author Yayoi Habami
 * @date 2026-09-17
 * @copyright 2026 Yayoi Habami
 */
#include "./machining_view_pane.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

#include <igesio/graphics/core/texture.h>

namespace igesio::graphics {

namespace {

/// @brief クリックと判定する移動量の閾値 [px]
constexpr float kClickThresholdPx = 5.0f;
/// @brief ドラッグ回転の係数 [rad/px]
constexpr float kRotateSensitivity = 0.006f;
/// @brief ドラッグ平行移動の係数
constexpr float kPanSensitivity = 0.001f;
/// @brief ドラッグズームの係数
constexpr float kZoomDragSensitivity = 0.01f;
/// @brief ホイール1目盛りのズーム率
constexpr float kWheelZoomStep = 0.1f;
/// @brief オーバーレイの余白 [px]
constexpr float kOverlayPadding = 6.0f;
/// @brief 標準ビューの表示名 (`ZUpView`の順)
constexpr const char* kZUpViewNames[] = {
        "Top", "Bottom", "Front", "Back", "Right", "Left", "Iso"};

/// @brief ImGuiの修飾キーの状態を`ModifierKey`にする
ModifierKey CurrentModifiers() {
    const ImGuiIO& io = ImGui::GetIO();
    ModifierKey mods = ModifierKey::kNone;
    if (io.KeyCtrl) mods = mods | ModifierKey::kCtrl;
    if (io.KeyShift) mods = mods | ModifierKey::kShift;
    if (io.KeyAlt) mods = mods | ModifierKey::kAlt;
    if (io.KeySuper) mods = mods | ModifierKey::kSuper;
    return mods;
}

/// @brief `MouseButton`をImGuiのボタン番号にする
/// @return `kNone`なら-1
int ToImGuiButton(const MouseButton button) {
    switch (button) {
        case MouseButton::kLeft: return ImGuiMouseButton_Left;
        case MouseButton::kMiddle: return ImGuiMouseButton_Middle;
        case MouseButton::kRight: return ImGuiMouseButton_Right;
        default: return -1;
    }
}

/// @brief 判定対象のボタン (押下順の検出に用いる)
constexpr MouseButton kButtons[] = {
        MouseButton::kLeft, MouseButton::kMiddle, MouseButton::kRight};

/// @brief 標準ビューの視線 (ターゲットから見たカメラの方向) と上方向
/// @param view 標準ビュー
/// @return {視線, 上方向} (いずれも正規化済み)
std::pair<igesio::Vector3f, igesio::Vector3f> ViewDirections(const ZUpView view) {
    using V = igesio::Vector3f;
    const V y_up(0.0f, 1.0f, 0.0f);
    const V z_up(0.0f, 0.0f, 1.0f);
    switch (view) {
        case ZUpView::kTop:    return {V(0.0f, 0.0f, 1.0f), y_up};
        case ZUpView::kBottom: return {V(0.0f, 0.0f, -1.0f), y_up};
        case ZUpView::kFront:  return {V(0.0f, -1.0f, 0.0f), z_up};
        case ZUpView::kBack:   return {V(0.0f, 1.0f, 0.0f), z_up};
        case ZUpView::kRight:  return {V(1.0f, 0.0f, 0.0f), z_up};
        case ZUpView::kLeft:   return {V(-1.0f, 0.0f, 0.0f), z_up};
        case ZUpView::kIso:
        default: {
            const float s = 1.0f / std::sqrt(3.0f);
            return {V(s, -s, s), z_up};
        }
    }
}

}  // namespace



const char* ZUpViewName(const ZUpView view) {
    return kZUpViewNames[static_cast<int>(view)];
}



/**
 * 初期化・終了
 */

ViewPane::ViewPane(std::string name, std::shared_ptr<IOpenGL> gl,
                   const GLProcLoader loader, const int msaa_samples)
        : name_(std::move(name)), renderer_(std::move(gl), 1, 1),
          msaa_samples_(std::max(msaa_samples, 0)) {
    if (gladLoadGLContext(&gl_, reinterpret_cast<GLADloadfunc>(loader)) == 0) {
        throw std::runtime_error("Failed to load OpenGL functions for the view '"
                                 + name_ + "'");
    }
    renderer_.Initialize();
    renderer_.EnableTransparency(true);
    if (msaa_samples_ > 0) renderer_.EnableAntialiasing(true);
    // 機械はZ-upなので、初期の向きを等角のZ-upにする
    renderer_.Camera().SetTarget(igesio::Vector3f(0.0f, 0.0f, 0.0f));
    renderer_.Camera().SetPosition(igesio::Vector3f(0.0f, 0.0f, 1000.0f));
    renderer_.Camera().SetProjectionMode(ProjectionMode::kOrthographic);
    SetStandardView(ZUpView::kIso);
}

ViewPane::~ViewPane() {
    DestroyFramebuffer();
}



/**
 * FBO
 */

void ViewPane::EnsureFramebuffer(const int width, const int height) {
    if (width == fb_width_ && height == fb_height_ && resolve_fbo_ != 0) return;
    DestroyFramebuffer();
    fb_width_ = width;
    fb_height_ = height;

    // 表示用の色テクスチャと、それを持つFBO
    gl_.GenTextures(1, &color_texture_);
    gl_.BindTexture(GL_TEXTURE_2D, color_texture_);
    gl_.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
                   GL_UNSIGNED_BYTE, nullptr);
    gl_.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl_.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl_.BindTexture(GL_TEXTURE_2D, 0);
    gl_.GenFramebuffers(1, &resolve_fbo_);
    gl_.BindFramebuffer(GL_FRAMEBUFFER, resolve_fbo_);
    gl_.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                             color_texture_, 0);

    gl_.GenRenderbuffers(1, &depth_renderbuffer_);
    gl_.BindRenderbuffer(GL_RENDERBUFFER, depth_renderbuffer_);
    if (msaa_samples_ > 0) {
        // マルチサンプルの描画先 (色と深度のレンダーバッファ) を別のFBOに持ち,
        // 描画後に表示用テクスチャへ解決する
        gl_.RenderbufferStorageMultisample(GL_RENDERBUFFER, msaa_samples_,
                                           GL_DEPTH24_STENCIL8, width, height);
        gl_.GenRenderbuffers(1, &color_renderbuffer_);
        gl_.BindRenderbuffer(GL_RENDERBUFFER, color_renderbuffer_);
        gl_.RenderbufferStorageMultisample(GL_RENDERBUFFER, msaa_samples_,
                                           GL_RGBA8, width, height);
        gl_.GenFramebuffers(1, &draw_fbo_);
        gl_.BindFramebuffer(GL_FRAMEBUFFER, draw_fbo_);
        gl_.FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                    GL_RENDERBUFFER, color_renderbuffer_);
        gl_.FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                    GL_RENDERBUFFER, depth_renderbuffer_);
    } else {
        gl_.RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width,
                                height);
        gl_.FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                    GL_RENDERBUFFER, depth_renderbuffer_);
        draw_fbo_ = resolve_fbo_;
    }
    gl_.BindRenderbuffer(GL_RENDERBUFFER, 0);
    if (gl_.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        gl_.BindFramebuffer(GL_FRAMEBUFFER, 0);
        DestroyFramebuffer();
        throw std::runtime_error("Failed to create the framebuffer for the view '"
                                 + name_ + "'");
    }
    gl_.BindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ViewPane::DestroyFramebuffer() {
    if (draw_fbo_ != 0 && draw_fbo_ != resolve_fbo_) {
        gl_.DeleteFramebuffers(1, &draw_fbo_);
    }
    if (resolve_fbo_ != 0) gl_.DeleteFramebuffers(1, &resolve_fbo_);
    if (color_renderbuffer_ != 0) gl_.DeleteRenderbuffers(1, &color_renderbuffer_);
    if (depth_renderbuffer_ != 0) gl_.DeleteRenderbuffers(1, &depth_renderbuffer_);
    if (color_texture_ != 0) gl_.DeleteTextures(1, &color_texture_);
    draw_fbo_ = resolve_fbo_ = color_renderbuffer_ = 0;
    depth_renderbuffer_ = color_texture_ = 0;
    fb_width_ = fb_height_ = 0;
}

void ViewPane::DrawToFramebuffer() {
    GLint prev_fbo = 0;
    gl_.GetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
    gl_.BindFramebuffer(GL_FRAMEBUFFER, draw_fbo_);
    renderer_.SetDisplaySize(fb_width_, fb_height_);
    renderer_.Draw();
    if (draw_fbo_ != resolve_fbo_) {
        gl_.BindFramebuffer(GL_READ_FRAMEBUFFER, draw_fbo_);
        gl_.BindFramebuffer(GL_DRAW_FRAMEBUFFER, resolve_fbo_);
        gl_.BlitFramebuffer(0, 0, fb_width_, fb_height_, 0, 0, fb_width_,
                            fb_height_, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
    gl_.BindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prev_fbo));
}



/**
 * 描画と操作
 */

std::optional<ClickInfo> ViewPane::Render(const ImVec2& size,
                                          const InputConfig& input) {
    if (size.x < 1.0f || size.y < 1.0f) {
        hovered_ = false;
        return std::nullopt;
    }
    const ImGuiIO& io = ImGui::GetIO();
    pixel_scale_ = std::max(io.DisplayFramebufferScale.x, 0.01f);
    const int width = std::max(1, static_cast<int>(size.x * pixel_scale_));
    const int height = std::max(1, static_cast<int>(size.y * pixel_scale_));
    EnsureFramebuffer(width, height);
    DrawToFramebuffer();
    if (fit_pending_) {
        // 走査後 (Draw後) にFitViewを行い、次のフレームで反映する
        fit_pending_ = false;
        renderer_.FitView();
    }

    // 画像 (FBOは下が原点なので上下を反転する)、オーバーレイ、入力用の透明ボタン
    // の順に置く. オーバーレイを先に置くことで、そのボタンの上ではビューの
    // ホバーが成立しない (ImGuiは先に置いた項目にホバーを与える)
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddImage(
            reinterpret_cast<ImTextureID>(static_cast<std::intptr_t>(color_texture_)),
            origin, ImVec2(origin.x + size.x, origin.y + size.y),
            ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
    RenderOverlay(origin);
    ImGui::InvisibleButton(("##view_" + name_).c_str(), size,
                           ImGuiButtonFlags_MouseButtonLeft
                           | ImGuiButtonFlags_MouseButtonMiddle
                           | ImGuiButtonFlags_MouseButtonRight);
    hovered_ = ImGui::IsItemHovered();
    return HandleMouse(origin, size, input);
}

std::optional<ClickInfo> ViewPane::HandleMouse(const ImVec2& origin,
                                               const ImVec2& size,
                                               const InputConfig& input) {
    const ImGuiIO& io = ImGui::GetIO();
    std::optional<ClickInfo> click;

    // 押下: 画像の上で押されたボタンから操作を決める (1操作ずつ)
    if (drag_mode_ == DragMode::kNone && hovered_) {
        for (const MouseButton button : kButtons) {
            if (!ImGui::IsMouseClicked(ToImGuiButton(button))) continue;
            press_mods_ = CurrentModifiers();
            drag_mode_ = ResolveDragMode(button, press_mods_, input);
            if (drag_mode_ == DragMode::kNone) continue;
            drag_button_ = button;
            press_pos_ = io.MousePos;
            break;
        }
    }
    if (drag_mode_ == DragMode::kNone) {
        if (hovered_ && io.MouseWheel != 0.0f) {
            renderer_.Camera().Zoom(1.0f - kWheelZoomStep * io.MouseWheel);
        }
        return click;
    }

    // ドラッグ中: 移動量をカメラ操作にする
    const int button = ToImGuiButton(drag_button_);
    const ImVec2 delta = io.MouseDelta;
    switch (drag_mode_) {
        case DragMode::kRotate:
            renderer_.Camera().Rotate(-delta.x * kRotateSensitivity,
                                      -delta.y * kRotateSensitivity);
            break;
        case DragMode::kPan:
            renderer_.Camera().Pan(delta.x * kPanSensitivity,
                                   delta.y * kPanSensitivity);
            break;
        case DragMode::kZoom:
            renderer_.Camera().Zoom(1.0f + delta.y * kZoomDragSensitivity);
            break;
        default:
            break;
    }

    // 解放: 選択のクリックは移動量が閾値未満のときだけ
    if (ImGui::IsMouseReleased(button)) {
        if (drag_mode_ == DragMode::kSelect) {
            const float dx = io.MousePos.x - press_pos_.x;
            const float dy = io.MousePos.y - press_pos_.y;
            if (std::sqrt(dx * dx + dy * dy) < kClickThresholdPx) {
                const float x = std::clamp(press_pos_.x - origin.x, 0.0f, size.x);
                const float y = std::clamp(press_pos_.y - origin.y, 0.0f, size.y);
                click = ClickInfo{static_cast<double>(x * pixel_scale_),
                                  static_cast<double>(y * pixel_scale_),
                                  press_mods_};
            }
        }
        drag_mode_ = DragMode::kNone;
        drag_button_ = MouseButton::kNone;
    }
    return click;
}

ViewPane::DragMode ViewPane::ResolveDragMode(const MouseButton button,
                                             const ModifierKey mods,
                                             const InputConfig& input) const {
    if (Matches(input.rotate, button, mods)) return DragMode::kRotate;
    if (Matches(input.pan, button, mods)) return DragMode::kPan;
    if (Matches(input.zoom_drag, button, mods)) return DragMode::kZoom;
    // 選択は割り当てのボタンに、複数選択の修飾キーが付いてもよい
    const ModifierKey without_multi = static_cast<ModifierKey>(
            static_cast<int>(mods) & ~static_cast<int>(input.multi_select_mod));
    if (Matches(input.select, button, mods)
        || Matches(input.select, button, without_multi)) {
        return DragMode::kSelect;
    }
    return DragMode::kNone;
}

void ViewPane::RenderOverlay(const ImVec2& origin) {
    const ImVec2 saved = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(ImVec2(origin.x + kOverlayPadding,
                                     origin.y + kOverlayPadding));
    ImGui::PushID(name_.c_str());
    ImGui::BeginGroup();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(name_.c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("Fit")) renderer_.FitView();
    ImGui::SameLine();
    if (ImGui::SmallButton("Iso")) {
        overlay_view_index_ = static_cast<int>(ZUpView::kIso);
        SetStandardView(ZUpView::kIso);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    if (ImGui::Combo("##view", &overlay_view_index_, kZUpViewNames,
                     IM_ARRAYSIZE(kZUpViewNames))) {
        SetStandardView(static_cast<ZUpView>(overlay_view_index_));
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Shot")) screenshot_requested_ = true;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save a screenshot of this view");
    ImGui::EndGroup();
    ImGui::PopID();
    ImGui::SetCursorScreenPos(saved);
}

bool ViewPane::TakeScreenshotRequest() {
    const bool requested = screenshot_requested_;
    screenshot_requested_ = false;
    return requested;
}



/**
 * カメラ
 */

void ViewPane::SetStandardView(const ZUpView view) {
    graphics::Camera& camera = renderer_.Camera();
    const igesio::Vector3f target = camera.GetTarget();
    const float distance =
            std::max((camera.GetPosition() - target).norm(), 0.1f);
    const auto [direction, up] = ViewDirections(view);
    camera.SetPosition(target + direction * distance);
    camera.SetUp(up);
}

void ViewPane::CaptureScreenshot(const std::string& path) {
    const Texture texture = renderer_.CaptureScreenshot();
    SaveTextureToFile(path, texture);
}

}  // namespace igesio::graphics
