/**
 * @file examples/gui/iges_viewer_gui.cpp
 * @brief IGESのエンティティを表示するGUIクラス
 * @author Yayoi Habami
 * @date 2025-08-05
 * @copyright 2025 Yayoi Habami
 */
#include "./iges_viewer_gui.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <string>

namespace {

using IgesViewerGUI = igesio::graphics::IgesViewerGUI;

}  // namespace



IgesViewerGUI::IgesViewerGUI(const int width, const int height)
        : renderer_(std::make_shared<OpenGL>(), width, height) {
    glfwSetErrorCallback(ErrorCallback);
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

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
    auto [width, height] = renderer_.GetDisplaySize();
    auto pixels = renderer_.CaptureScreenshot();
    if (pixels.empty()) {
        throw std::runtime_error("Failed to capture screenshot");
    }
    // 画像を保存 (stb_image_writeを使用)
    stbi_flip_vertically_on_write(true);  // OpenGLの座標系に合わせて上下反転
    if (!stbi_write_png(filename.c_str(), width, height, 3, pixels.data(), width * 3)) {
        throw std::runtime_error("Failed to save screenshot to " + filename);
    }
}



/**
 * 描画関連の関数 (protected)
 */

void IgesViewerGUI::RenderControls() {
    ImGui::Begin("Controls");
    ImGui::Text("Camera");
    ImGui::Text("  - Drag Left Mouse: Rotate");
    ImGui::Text("  - Drag Right Mouse: Pan");
    ImGui::Text("  - Mouse Wheel: Zoom");
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

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            glfwGetCursorPos(window_, &last_x_, &last_y_);
            is_dragging_ = true;
        } else if (action == GLFW_RELEASE) {
            is_dragging_ = false;
        }
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS) {
            glfwGetCursorPos(window_, &last_x_, &last_y_);
            is_panning_ = true;
        } else if (action == GLFW_RELEASE) {
            is_panning_ = false;
        }
    }
}

void IgesViewerGUI::CursorPositionCallback(
        const double xpos, const double ypos) {
    auto dx = static_cast<float>(xpos - last_x_);
    auto dy = static_cast<float>(ypos - last_y_);

    if (is_dragging_) {
        // カメラをターゲットの周りで回転
        float sensitivity = 0.006f;
        renderer_.Camera().Rotate(-dx * sensitivity, -dy * sensitivity);
        needs_redraw_ = true;
    }
    if (is_panning_) {
        // カメラをパン (平行移動)
        float sensitivity = 0.001f;
        renderer_.Camera().Pan(dx * sensitivity, dy * sensitivity);
        needs_redraw_ = true;
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
