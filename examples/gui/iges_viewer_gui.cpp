/**
 * @file examples/gui/iges_viewer_gui.cpp
 * @brief IGESのエンティティを表示するGUIクラス
 * @author Yayoi Habami
 * @date 2025-08-05
 * @copyright 2025 Yayoi Habami
 */
#include "./iges_viewer_gui.h"

#include <algorithm>
#include <string>

namespace {

using IgesViewerGUI = igesio::graphics::IgesViewerGUI;
using igesio::graphics::MouseButton;
using igesio::graphics::ModifierKey;

/// @brief クリックと判定する移動量の閾値 [px]
/// @note 押下から解放までの移動量がこの値未満であればクリック(ピッキング)、
///       以上であればドラッグ(回転)とみなす
constexpr double kClickThresholdPx = 5.0;

/// @brief GLFWのマウスボタン定数をMouseButtonへ変換する
/// @param glfw_button GLFW_MOUSE_BUTTON_*
/// @return 対応するMouseButton (未対応のボタンはkNone)
MouseButton ToMouseButton(const int glfw_button) {
    switch (glfw_button) {
        case GLFW_MOUSE_BUTTON_LEFT: return MouseButton::kLeft;
        case GLFW_MOUSE_BUTTON_MIDDLE: return MouseButton::kMiddle;
        case GLFW_MOUSE_BUTTON_RIGHT: return MouseButton::kRight;
        default: return MouseButton::kNone;
    }
}

/// @brief GLFWの修飾キービットをModifierKeyへ変換する
/// @param glfw_mods GLFW_MOD_* のOR
/// @return Ctrl/Shift/Alt/Superのみを反映したModifierKey
/// @note CapsLock等の無関係なビットは落とし、完全一致判定を可能にする
ModifierKey ToModifierKey(const int glfw_mods) {
    ModifierKey mods = ModifierKey::kNone;
    if (glfw_mods & GLFW_MOD_CONTROL) mods = mods | ModifierKey::kCtrl;
    if (glfw_mods & GLFW_MOD_SHIFT) mods = mods | ModifierKey::kShift;
    if (glfw_mods & GLFW_MOD_ALT) mods = mods | ModifierKey::kAlt;
    if (glfw_mods & GLFW_MOD_SUPER) mods = mods | ModifierKey::kSuper;
    return mods;
}

}  // namespace



IgesViewerGUI::IgesViewerGUI(
        const int width, const int height,
        const int msaa_samples)
        : renderer_(std::make_shared<OpenGL>(), width, height) {
    glfwSetErrorCallback(ErrorCallback);
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    // アンチエイリアス機能
    if (msaa_samples > 0) {
        glfwWindowHint(GLFW_SAMPLES, msaa_samples);
        msaa_samples_ = msaa_samples;
    }

    window_ = glfwCreateWindow(width, height, "IGES Viewer", NULL, NULL);
    if (!window_) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(window_);
    // thisポインタをウィンドウに保存
    glfwSetWindowUserPointer(window_, this);
    glfwSwapInterval(1);  // V-Syncを有効化

    if (!gladLoadGL(glfwGetProcAddress)) {
        glfwDestroyWindow(window_);
        glfwTerminate();
        throw std::runtime_error("Failed to initialize GLAD");
    }

    // コールバック関数を設定
    SetupCallbacks();

    // ImGuiの初期化
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();  // ダークテーマを使用
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 400 core");
}

IgesViewerGUI::~IgesViewerGUI() {
    // ImGuiの終了処理
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // ウィンドウを破棄
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }

    // GLFWを終了
    glfwTerminate();
}



/**
 * 描画関連の関数 (public)
 */

void IgesViewerGUI::Run(const bool vsync) {
    // レンダラーの初期化
    renderer_.Initialize();
    // V-Syncの設定
    glfwSwapInterval(vsync ? 1 : 0);
    // MSAAの設定
    if (msaa_samples_ > 0) {
        renderer_.EnableAntialiasing(true);
    }

    while (!glfwWindowShouldClose(window_)) {
        // イベントを待機
        glfwWaitEvents();

        // ImGuiのウィジェットがアクティブな場合はポーリングに切り替える
        if (ImGui::IsAnyItemActive()) {
            needs_redraw_ = true;
        }

        if (!needs_redraw_) {
            continue;
        }

        // ImGuiの新しいフレームを開始
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Controls GUIの描画
        RenderControls();

        // 範囲選択のラバーバンドを描画 (foregroundドローリスト)
        RenderBoxSelectionOverlay();

        // OpenGLの描画
        renderer_.Draw();

        // ImGuiの描画
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // バッファをスワップ
        glfwSwapBuffers(window_);
    }
}

void IgesViewerGUI::CaptureScreenshot(const std::string& filename) {
    // OpenGLのフレームバッファからピクセルデータを取得
    auto texture = renderer_.CaptureScreenshot();
    igesio::graphics::SaveTextureToFile(filename, texture);
}



/**
 * 描画関連の関数 (protected)
 */

void IgesViewerGUI::RenderControls() {
    ImGui::Begin("Controls");
    ImGui::Text("Camera");
    ImGui::Text("  - Middle Drag: Rotate");
    ImGui::Text("  - Ctrl + Middle Drag: Pan");
    ImGui::Text("  - Wheel / Shift + Middle Drag: Zoom");
    ImGui::Text("Selection");
    ImGui::Text("  - Left Click: Select");
    ImGui::Text("  - Ctrl + Left Click: Multi-select");
    ImGui::Text("  - Left Drag L->R: Box select (window)");
    ImGui::Text("  - Left Drag R->L: Box select (crossing)");
    ImGui::Text("  - Ctrl + Left Drag: Add to selection");
    ImGui::Separator();

    // カメラの位置とターゲットを表示
    auto cam_pos = renderer_.Camera().GetPosition();
    auto cam_target = renderer_.Camera().GetTarget();
    ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)",
                cam_pos[0], cam_pos[1], cam_pos[2]);
    ImGui::Text("Camera Target: (%.2f, %.2f, %.2f)",
                cam_target[0], cam_target[1], cam_target[2]);

    // カメラの操作
    if (ImGui::Button("Reset Camera")) {
        renderer_.Camera().Reset();
        needs_redraw_ = true;
    }
    ImGui::Separator();

    if (ImGui::ColorEdit3("Background",
                      renderer_.GetBackgroundColorRef().data())) {
        needs_redraw_ = true;
    }
    ImGui::Separator();

    // 選択中エンティティの一覧 (型・ID・交差座標)
    const auto& selected = renderer_.GetSelectedIds();
    ImGui::Text("Selected: %d", static_cast<int>(selected.size()));
    for (const auto& id : selected) {
        const auto type_str = ToString(renderer_.GetEntityShaderType(id));
        auto it = selected_hit_positions_.find(id);
        if (it != selected_hit_positions_.end()) {
            const auto& p = it->second;
            ImGui::Text("  [%d] %s  (%.3f, %.3f, %.3f)",
                        id.ToInt(), type_str.c_str(), p.x(), p.y(), p.z());
        } else {
            ImGui::Text("  [%d] %s", id.ToInt(), type_str.c_str());
        }
    }
    if (ImGui::Button("Deselect All")) {
        renderer_.ClearSelection();
        selected_hit_positions_.clear();
        needs_redraw_ = true;
    }

    ImGui::End();
}



/**
 * コールバック関数
 */

void IgesViewerGUI::SetupCallbacks() {
    // マウスボタン操作時のコールバック
    glfwSetMouseButtonCallback(window_, [](GLFWwindow* window, int button,
                                            int action, int mods) {
        auto* viewer = static_cast<IgesViewerGUI*>(glfwGetWindowUserPointer(window));
        viewer->MouseButtonCallback(button, action, mods);
    });

    // マウスカーソル位置変化時のコールバック
    glfwSetCursorPosCallback(window_, [](GLFWwindow* window, double xpos, double ypos) {
        auto* viewer = static_cast<IgesViewerGUI*>(glfwGetWindowUserPointer(window));
        viewer->CursorPositionCallback(xpos, ypos);
    });

    // スクロールホイール操作時のコールバック
    glfwSetScrollCallback(window_, [](GLFWwindow* window, double x_offset, double y_offset) {
        auto* viewer = static_cast<IgesViewerGUI*>(glfwGetWindowUserPointer(window));
        viewer->ScrollCallback(x_offset, y_offset);
    });

    // ウィンドウリサイズ時のコールバック
    glfwSetFramebufferSizeCallback(window_, [](GLFWwindow* window, int width, int height) {
        auto* viewer = static_cast<IgesViewerGUI*>(glfwGetWindowUserPointer(window));
        viewer->FramebufferSizeCallback(width, height);
    });
}

void IgesViewerGUI::MouseButtonCallback(
        const int button, const int action, const int mods) {
    if (ImGui::GetIO().WantCaptureMouse) return;

    const MouseButton btn = ToMouseButton(button);
    const ModifierKey mod = ToModifierKey(mods);

    if (action == GLFW_PRESS) {
        glfwGetCursorPos(window_, &last_x_, &last_y_);
        press_x_ = last_x_;
        press_y_ = last_y_;

        // 押下したボタン+修飾キーから、ドラッグ操作モードを確定する
        if (Matches(input_config_.rotate, btn, mod)) {
            drag_mode_ = DragMode::kRotate;
        } else if (Matches(input_config_.pan, btn, mod)) {
            drag_mode_ = DragMode::kPan;
        } else if (Matches(input_config_.zoom_drag, btn, mod)) {
            drag_mode_ = DragMode::kZoom;
        } else {
            drag_mode_ = DragMode::kNone;
        }

        // 選択ボタン押下時は範囲選択の候補とする (回転は中ボタンのため衝突しない)
        is_box_selecting_ = (btn == input_config_.select.button);
    } else if (action == GLFW_RELEASE) {
        drag_mode_ = DragMode::kNone;

        // 選択ボタン以外の解放では何もしない
        if (btn != input_config_.select.button) return;
        is_box_selecting_ = false;

        double x, y;
        glfwGetCursorPos(window_, &x, &y);
        const double dx = x - press_x_, dy = y - press_y_;
        const bool is_click =
                dx * dx + dy * dy < kClickThresholdPx * kClickThresholdPx;

        if (is_click) {
            // ウィンドウ座標 -> フレームバッファ座標へ換算
            // (HiDPI環境ではウィンドウサイズとフレームバッファサイズが異なる)
            int win_w = 0, win_h = 0;
            glfwGetWindowSize(window_, &win_w, &win_h);
            const auto [fb_w, fb_h] = renderer_.GetDisplaySize();
            const double sx = (win_w > 0)
                    ? static_cast<double>(fb_w) / win_w : 1.0;
            const double sy = (win_h > 0)
                    ? static_cast<double>(fb_h) / win_h : 1.0;

            HandleClickSelection(x * sx, y * sy, mods);
        } else {
            // 閾値以上の移動は範囲選択 (矩形ドラッグ)
            HandleBoxSelection(x, y, mods);
        }
        needs_redraw_ = true;
    }
}

void IgesViewerGUI::HandleClickSelection(
        const double x, const double y, const int mods) {
    const Ray ray = renderer_.GetRayFromScreen(x, y);
    const auto hits = renderer_.PickEntities(ray, x, y);
    // 複数選択 (トグル) 修飾キーが押下されているか
    const ModifierKey mod = ToModifierKey(mods);
    const ModifierKey multi_mod = input_config_.multi_select_mod;
    const bool ctrl = multi_mod != ModifierKey::kNone
            && (mod & multi_mod) == multi_mod;

    if (hits.empty()) {
        // 空クリック かつ 複数選択修飾なし のときのみ全解除
        if (!ctrl) {
            renderer_.ClearSelection();
            selected_hit_positions_.clear();
        }
        return;
    }

    const ObjectID id = hits.front().id;
    const Vector3d pos = hits.front().hit.position;
    if (ctrl) {
        // 複数選択修飾: 選択状態をトグル
        if (renderer_.IsSelected(id)) {
            renderer_.Deselect(id);
            selected_hit_positions_.erase(id);
        } else {
            renderer_.Select(id);
            selected_hit_positions_[id] = pos;
        }
    } else {
        // 複数選択修飾なし: 選択集合をid単独へ置換
        renderer_.ClearSelection();
        selected_hit_positions_.clear();
        renderer_.Select(id);
        selected_hit_positions_[id] = pos;
    }
}

void IgesViewerGUI::HandleBoxSelection(
        const double x, const double y, const int mods) {
    // ウィンドウ座標 -> フレームバッファ座標へ換算 (HiDPI対応)
    int win_w = 0, win_h = 0;
    glfwGetWindowSize(window_, &win_w, &win_h);
    const auto [fb_w, fb_h] = renderer_.GetDisplaySize();
    const double sx = (win_w > 0) ? static_cast<double>(fb_w) / win_w : 1.0;
    const double sy = (win_h > 0) ? static_cast<double>(fb_h) / win_h : 1.0;

    ScreenRect rect;
    rect.x_min = std::min(press_x_, x) * sx;
    rect.x_max = std::max(press_x_, x) * sx;
    rect.y_min = std::min(press_y_, y) * sy;
    rect.y_max = std::max(press_y_, y) * sy;

    // 左→右ドラッグで内包、右→左で交差 (AutoCAD流)
    const BoxSelectionMode mode = (x - press_x_ >= 0.0)
            ? BoxSelectionMode::kContained : BoxSelectionMode::kCrossing;
    const auto ids = renderer_.PickEntitiesInRect(rect, mode);

    // 複数選択 (追加) 修飾キーが押下されているか
    const ModifierKey mod = ToModifierKey(mods);
    const ModifierKey multi_mod = input_config_.multi_select_mod;
    const bool additive = multi_mod != ModifierKey::kNone
            && (mod & multi_mod) == multi_mod;

    // 追加でなければ既存の選択を置換する
    if (!additive) {
        renderer_.ClearSelection();
        selected_hit_positions_.clear();
    }
    // 範囲選択は単一の交点座標を持たないため hit_positions には登録しない
    for (const auto& id : ids) {
        renderer_.Select(id);  // 既に選択済みの場合はSelect側で無視される
    }
}

void IgesViewerGUI::RenderBoxSelectionOverlay() {
    if (!is_box_selecting_) return;

    // 閾値未満の移動はクリック扱いとなるためラバーバンドを描画しない
    const double dx = last_x_ - press_x_, dy = last_y_ - press_y_;
    if (dx * dx + dy * dy < kClickThresholdPx * kClickThresholdPx) return;

    // ウィンドウ座標でラバーバンドを描画する (ImGui座標系に一致)
    const float x0 = static_cast<float>(std::min(press_x_, last_x_));
    const float y0 = static_cast<float>(std::min(press_y_, last_y_));
    const float x1 = static_cast<float>(std::max(press_x_, last_x_));
    const float y1 = static_cast<float>(std::max(press_y_, last_y_));

    // 左→右で内包 (青)、右→左で交差 (緑) — AutoCAD流の色分け
    const bool contained = (last_x_ - press_x_) >= 0.0;
    const ImU32 fill = contained
            ? IM_COL32(50, 120, 255, 40) : IM_COL32(50, 200, 80, 40);
    const ImU32 border = contained
            ? IM_COL32(50, 120, 255, 200) : IM_COL32(50, 200, 80, 200);

    ImDrawList* draw_list = ImGui::GetForegroundDrawList();
    draw_list->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), fill);
    draw_list->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), border);
}

void IgesViewerGUI::CursorPositionCallback(
        const double xpos, const double ypos) {
    const auto dx = static_cast<float>(xpos - last_x_);
    const auto dy = static_cast<float>(ypos - last_y_);

    // 範囲選択中はカメラ操作を行わず、ラバーバンド更新のため再描画する
    if (is_box_selecting_) {
        needs_redraw_ = true;
    }

    switch (drag_mode_) {
        case DragMode::kRotate: {
            // カメラをターゲットの周りで回転
            constexpr float kSensitivity = 0.006f;
            renderer_.Camera().Rotate(-dx * kSensitivity, -dy * kSensitivity);
            needs_redraw_ = true;
            break;
        }
        case DragMode::kPan: {
            // カメラをパン (平行移動)
            constexpr float kSensitivity = 0.001f;
            renderer_.Camera().Pan(dx * kSensitivity, dy * kSensitivity);
            needs_redraw_ = true;
            break;
        }
        case DragMode::kZoom: {
            // 上方向のドラッグでズームイン (Zoomは<1.0fでズームイン)
            constexpr float kSensitivity = 0.01f;
            renderer_.Camera().Zoom(1.0f + dy * kSensitivity);
            needs_redraw_ = true;
            break;
        }
        case DragMode::kNone:
            break;
    }

    last_x_ = xpos;
    last_y_ = ypos;
}

void IgesViewerGUI::ScrollCallback(
        const double x_offset, const double y_offset) {
    if (ImGui::GetIO().WantCaptureMouse) return;
    float sensitivity = 0.1f;
    float zoom_factor = 1.0f - static_cast<float>(y_offset) * sensitivity;

    renderer_.Camera().Zoom(zoom_factor);
    needs_redraw_ = true;
}

void IgesViewerGUI::FramebufferSizeCallback(const int width, const int height) {
    renderer_.SetDisplaySize(width, height);
    needs_redraw_ = true;
}
