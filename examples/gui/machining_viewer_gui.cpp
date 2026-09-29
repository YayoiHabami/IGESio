/**
 * @file examples/gui/machining_viewer_gui.cpp
 * @brief 簡易CAM GUIの実装 (初期化、描画ループ、メニュー、中央領域、モーダル,
 *        セッションの読込と解除)
 * @author Yayoi Habami
 * @date 2026-09-17
 * @copyright 2026 Yayoi Habami
 * @note 各パネルの描画は`machining_viewer_panels.cpp`に置く.
 * @note ImGuiのポップアップは`OpenPopup`を呼んだ時点のIDスタックに属するため,
 *       モーダルを開く要求は`OpenModal`でフラグにし、ウィンドウの外 (最上位)
 *       で描画する`RenderModals`で`OpenPopup`を呼ぶ.
 */
#include "./machining_viewer_gui.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <exception>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <igesio/graphics/core/gl_backend.h>
#include <igesio/graphics/core/gl_types.h>
#include <igesio/graphics/core/material_property.h>
#include <igesio/extensions/inspection/instanced_entity_graphics.h>

namespace igesio::graphics {

namespace {

namespace m = igesio::extensions::machines;
namespace anim = igesio::extensions::animation;

/// @brief 左パネルの幅 [px]
constexpr float kLeftPanelWidth = 280.0f;
/// @brief 右パネルの幅 [px]
constexpr float kRightPanelWidth = 340.0f;
/// @brief 再生バーの高さ [px]
constexpr float kPlaybackBarHeight = 92.0f;
/// @brief ステータスバーの高さ [px]
constexpr float kStatusBarHeight = 28.0f;
/// @brief 2ビューの分割バーの幅 [px]
constexpr float kSplitterWidth = 6.0f;
/// @brief 分割比の下限
constexpr float kMinSplitRatio = 0.2f;
/// @brief 分割比の上限
constexpr float kMaxSplitRatio = 0.8f;
/// @brief 再生速度スライダの幅 [px]
constexpr float kSpeedSliderWidth = 160.0f;
/// @brief 再生速度の下限 [倍]
constexpr float kMinSpeed = 0.1f;
/// @brief 再生速度の上限 [倍]
constexpr float kMaxSpeed = 100.0f;
/// @brief 端に固定するパネル共通のウィンドウフラグ
constexpr ImGuiWindowFlags kPanelFlags = ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoTitleBar;
/// @brief 発生段階の表示名 (`LogStage`の順)
constexpr const char* kLogStageNames[] = {
        "project", "setup", "scene", "program", "motion", "clip", "trajectory",
        "gui"};
/// @brief モーダルの題名 (`ModalKind`の順)
constexpr const char* kModalTitles[] = {
        "", "Open Project", "Open Program with Virtual Machine",
        "Add Library Directory", "Screenshot", "Error", "Controls", "About"};
/// @brief 仮想機械の種類の選択肢 (`VirtualMachineKind`の順)
constexpr const char* kVirtualMachineItems[] = {
        "3-axis (XYZ)", "Head BC (XYZ + C, B on the head)",
        "Table AC (XYZ + A, C on the table)"};
/// @brief プログラム種別の選択肢 (`ProgramType`の順)
constexpr const char* kProgramTypeItems[] = {"gcode", "cl", "apt"};
/// @brief 仮想機械の工具の名前の接頭辞
constexpr const char* kVirtualToolNamePrefix = "Ball d";
/// @brief 仮想機械の工具のホルダ径の下限 [mm]
constexpr double kVirtualHolderMinDiameter = 40.0;
/// @brief 仮想機械の工具のホルダ長 [mm]
constexpr double kVirtualHolderLength = 60.0;

/// @brief 現在時刻を文字列にする (スクリーンショットのファイル名用)
/// @param format `std::put_time`の書式
std::string CurrentTimeString(const char* format = "%Y-%m-%d %H%M%S") {
    const auto now = std::chrono::system_clock::now();
    const auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm_buf, format);
    return ss.str();
}

/// @brief テキストを行に分割する (`\r\n`と`\n`の両方に対応)
/// @param text テキスト
/// @return 行の一覧 (末尾の改行の後の空行は含めない)
std::vector<std::string> SplitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string::size_type start = 0;
    while (start < text.size()) {
        std::string::size_type end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        std::string::size_type len = end - start;
        if (len > 0 && text[start + len - 1] == '\r') --len;
        lines.push_back(text.substr(start, len));
        start = end + 1;
    }
    return lines;
}

/// @brief 状態の表示名
/// @param state GUIの状態
/// @param playback プレイヤーの状態 (`kBound`のときの内訳に用いる)
const char* StateLabel(const SessionState state,
                       const anim::PlaybackState playback) {
    switch (state) {
        case SessionState::kEmpty: return "No project";
        case SessionState::kReady: return "Ready";
        case SessionState::kPlaying: return "Playing";
        case SessionState::kBound:
        default:
            if (playback == anim::PlaybackState::kPaused) return "Paused";
            if (playback == anim::PlaybackState::kFinished) return "Finished";
            return "Bound";
    }
}

/// @brief 2つの同次変換が等しいか (全成分の完全一致)
bool SameTransform(const igesio::Matrix4d& a, const igesio::Matrix4d& b) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (a(i, j) != b(i, j)) return false;
        }
    }
    return true;
}

/// @brief `std::string`を固定長の入力バッファにコピーする
/// @param text コピー元
/// @param[out] buffer コピー先 (終端を保証する)
void CopyToBuffer(const std::string& text, std::array<char, 1024>& buffer) {
    const std::size_t n = std::min(text.size(), buffer.size() - 1);
    std::memcpy(buffer.data(), text.data(), n);
    buffer[n] = '\0';
}

}  // namespace



const char* LogStageName(const LogStage stage) {
    return kLogStageNames[static_cast<int>(stage)];
}



/**
 * 初期化・終了
 */

void MachiningViewerGUI::ErrorCallback(const int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

MachiningViewerGUI::MachiningViewerGUI(
        const int width, const int height, const int msaa_samples,
        std::filesystem::path initial_project,
        std::vector<std::filesystem::path> library_dirs)
        : msaa_samples_(std::max(msaa_samples, 0)),
          initial_project_(std::move(initial_project)) {
    session_.library_dirs = std::move(library_dirs);
    // 工具軌跡は既定で非表示にする
    trajectory_visibility_.show = false;
    glfwSetErrorCallback(ErrorCallback);
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }
    // graphicsモジュールはテッセレーション (GL 4.0) とSSBO (GL 4.3) を使用する
    // ため、4.3 coreプロファイルのコンテキストを要求する. ウィンドウ自体には
    // ImGuiしか描画しないので、MSAAは各ビューのFBO側で行う
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    window_ = glfwCreateWindow(width, height, "Machining Viewer", nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    try {
        const auto loader = reinterpret_cast<GLProcLoader>(glfwGetProcAddress);
        gl_ = CreateGLBackend(loader);
        // 工具軌跡 (InstancedEntity) の描画クラスをレジストリに登録する
        extensions::inspection::RegisterInstancedEntityGraphics();
        scene_root_ = models::MakeAssembly("root");
        scene_ = std::make_unique<models::Scene>(scene_root_);
        scene_->SetGranularity(models::SelectionGranularity::kAssembly);
        machine_pane_ = std::make_unique<ViewPane>("Machine", gl_, loader,
                                                   msaa_samples_);
        work_pane_ = std::make_unique<ViewPane>("Work", gl_, loader,
                                                msaa_samples_);
        machine_pane_->SetScene(scene_.get());
        work_pane_->SetScene(scene_.get());
    } catch (const std::exception&) {
        machine_pane_.reset();
        work_pane_.reset();
        glfwDestroyWindow(window_);
        glfwTerminate();
        throw;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 430 core");
}

MachiningViewerGUI::~MachiningViewerGUI() {
    // FBOとGPUリソースはGLコンテキストが有効な間に破棄する
    session_.player.Unbind();
    machine_pane_.reset();
    work_pane_.reset();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
}



/**
 * 描画ループ
 */

void MachiningViewerGUI::Run() {
    if (!initial_project_.empty()) {
        LoadProject(initial_project_);
        initial_project_.clear();
    }
    last_frame_time_ = glfwGetTime();
    while (!glfwWindowShouldClose(window_)) {
        // 既定はイベント待ち. 再生中は短いタイムアウトで連続再描画し、ImGuiの
        // 遅延反映 (ポップアップの開閉等) のためイベントの次のフレームも描画する
        if (continuous_redraw_) {
            glfwWaitEventsTimeout(1.0 / 240.0);
        } else if (render_again_) {
            glfwWaitEventsTimeout(1.0 / 60.0);
        } else {
            glfwWaitEvents();
        }
        render_again_ = !render_again_ && !continuous_redraw_;
        needs_redraw_ = true;

        const double now = glfwGetTime();
        OnFrameUpdate(now - last_frame_time_);
        last_frame_time_ = now;
        if (!needs_redraw_) continue;
        needs_redraw_ = false;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        RenderFrame();
        ImGui::Render();

        int fb_width = 0, fb_height = 0;
        glfwGetFramebufferSize(window_, &fb_width, &fb_height);
        gl_->Viewport(0, 0, fb_width, fb_height);
        gl_->ClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        gl_->Clear(gl::kColorBufferBit | gl::kDepthBufferBit);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window_);
    }
}

void MachiningViewerGUI::UpdateState() {
    if (session_.player.IsBound()) {
        state_ = session_.player.State() == anim::PlaybackState::kPlaying
                ? SessionState::kPlaying : SessionState::kBound;
    } else if (session_.scene.IsBuilt()) {
        state_ = SessionState::kReady;
    } else {
        state_ = SessionState::kEmpty;
    }
}

void MachiningViewerGUI::OnFrameUpdate(const double dt_sec) {
    UpdateState();
    if (state_ == SessionState::kPlaying) {
        session_.player.Advance(dt_sec);
        if (dt_sec > 0.0) {
            measured_fps_ = 0.9 * measured_fps_ + 0.1 / dt_sec;
        }
        UpdateState();
    }
    if (session_.player.IsBound()) TrackCurrentRecord();
    // ワークビューはwork_mountコンポーネントに固定する (姿勢が変わったときだけ更新)
    if (session_.scene.IsBuilt()) {
        const igesio::Matrix4d frame = session_.scene.WorkMountWorldTransform();
        EntityRenderer& renderer = work_pane_->Renderer();
        if (!SameTransform(frame, renderer.ViewFrame())) {
            renderer.SetViewFrame(frame);
        }
    }
    continuous_redraw_ = state_ == SessionState::kPlaying;
}

void MachiningViewerGUI::RenderFrame() {
    HandleHotkeys();
    RenderMenuBar();

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float left_width = show_left_panel_ ? kLeftPanelWidth : 0.0f;
    const float right_width = show_right_panel_ ? kRightPanelWidth : 0.0f;
    const float body_height = vp->WorkSize.y - kStatusBarHeight;
    if (show_left_panel_) {
        RenderLeftPanel(vp->WorkPos, ImVec2(kLeftPanelWidth, body_height));
    }
    RenderCenter(ImVec2(vp->WorkPos.x + left_width, vp->WorkPos.y),
                 ImVec2(vp->WorkSize.x - left_width - right_width, body_height));
    if (show_right_panel_) {
        RenderRightPanel(
                ImVec2(vp->WorkPos.x + vp->WorkSize.x - right_width, vp->WorkPos.y),
                ImVec2(kRightPanelWidth, body_height));
    }
    RenderStatusBar();
    RenderModals();
}



/**
 * メニュー
 */

void MachiningViewerGUI::RenderMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;
    const bool loaded = state_ != SessionState::kEmpty;
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open Project...", "Ctrl+O")) {
            OpenModal(ModalKind::kOpenProject, session_.project_path.string());
        }
        if (ImGui::MenuItem("Reload Project", "F5", false,
                            !session_.project_path.empty())) {
            // 読込中に書き換わるため、パスはコピーして渡す
            LoadProject(std::filesystem::path(session_.project_path));
        }
        if (ImGui::MenuItem("Open Program with Virtual Machine...")) {
            OpenModal(ModalKind::kOpenVirtual, virtual_input_.program_path);
        }
        if (ImGui::MenuItem("Add Library Directory...")) {
            OpenModal(ModalKind::kAddLibraryDir);
        }
        if (ImGui::MenuItem("Clear", nullptr, false, loaded)) ClearSession();
        ImGui::Separator();
        if (ImGui::BeginMenu("Screenshot")) {
            if (ImGui::MenuItem("Machine View")) {
                screenshot_pane_ = machine_pane_.get();
                OpenModal(ModalKind::kScreenshot,
                          "screenshot machine " + CurrentTimeString() + ".png");
            }
            if (ImGui::MenuItem("Work View")) {
                screenshot_pane_ = work_pane_.get();
                OpenModal(ModalKind::kScreenshot,
                          "screenshot work " + CurrentTimeString() + ".png");
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Quit")) glfwSetWindowShouldClose(window_, GLFW_TRUE);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        if (ImGui::BeginMenu("Layout")) {
            if (ImGui::MenuItem("Dual", nullptr, layout_ == ViewLayout::kDual)) {
                layout_ = ViewLayout::kDual;
            }
            if (ImGui::MenuItem("Machine only", nullptr,
                                layout_ == ViewLayout::kMachineOnly)) {
                layout_ = ViewLayout::kMachineOnly;
            }
            if (ImGui::MenuItem("Work only", nullptr,
                                layout_ == ViewLayout::kWorkOnly)) {
                layout_ = ViewLayout::kWorkOnly;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Panels")) {
            ImGui::MenuItem("Left panel", nullptr, &show_left_panel_);
            ImGui::MenuItem("Right panel", nullptr, &show_right_panel_);
            ImGui::EndMenu();
        }
        ViewPane* target = last_hovered_pane_ ? last_hovered_pane_
                                              : machine_pane_.get();
        if (ImGui::BeginMenu("Standard View")) {
            for (int i = 0; i <= static_cast<int>(ZUpView::kIso); ++i) {
                const auto view = static_cast<ZUpView>(i);
                if (ImGui::MenuItem(ZUpViewName(view))) target->SetStandardView(view);
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Fit View", "F")) target->FitView();
        if (ImGui::BeginMenu("Projection")) {
            for (ViewPane* pane : {machine_pane_.get(), work_pane_.get()}) {
                if (!ImGui::BeginMenu(pane->Name().c_str())) continue;
                graphics::Camera& camera = pane->Renderer().Camera();
                const bool ortho = camera.GetProjectionMode()
                        == ProjectionMode::kOrthographic;
                if (ImGui::MenuItem("Perspective", nullptr, !ortho)) {
                    camera.SetProjectionMode(ProjectionMode::kPerspective);
                }
                if (ImGui::MenuItem("Orthographic", nullptr, ortho)) {
                    camera.SetProjectionMode(ProjectionMode::kOrthographic);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Controls...")) OpenModal(ModalKind::kControls);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Simulation")) {
        const bool ready = state_ == SessionState::kReady;
        const bool bound = state_ == SessionState::kBound
                || state_ == SessionState::kPlaying;
        if (ImGui::MenuItem("Generate Motion", "Ctrl+G", false, ready || bound)) {
            GenerateMotion();
        }
        if (ImGui::MenuItem("Release", nullptr, false, bound)) ReleaseMotion();
        ImGui::Separator();
        const bool playing = state_ == SessionState::kPlaying;
        if (ImGui::MenuItem(playing ? "Pause" : "Play", "Space", false, bound)) {
            TogglePlay();
        }
        if (ImGui::MenuItem("Stop", nullptr, false, bound)) {
            session_.player.Stop();
            RequestRedraw();
        }
        if (ImGui::MenuItem("Previous Record", "Left", false, bound)) StepRecord(-1);
        if (ImGui::MenuItem("Next Record", "Right", false, bound)) StepRecord(1);
        if (ImGui::MenuItem("Go to Start", "Home", false, bound)) SeekToTime(0.0);
        if (ImGui::MenuItem("Go to End", "End", false, bound)) {
            SeekToTime(session_.player.Duration());
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Reset to Zero Pose", nullptr, false, ready)) {
            SetJogPose(m::JointVector(session_.scene.Model().Axes().size(), 0.0));
            SetStatus("Zero pose");
        }
        if (ImGui::MenuItem("Default Pose", nullptr, false, ready)) {
            SetJogPose(m::InitialJoints(session_.scene.Model()));
            SetStatus("Default pose");
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("Controls")) OpenModal(ModalKind::kControls);
        if (ImGui::MenuItem("About")) OpenModal(ModalKind::kAbout);
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}

void MachiningViewerGUI::HandleHotkeys() {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput || modal_ != ModalKind::kNone) return;
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false)) {
        OpenModal(ModalKind::kOpenProject, session_.project_path.string());
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F5, false) && !session_.project_path.empty()) {
        LoadProject(std::filesystem::path(session_.project_path));
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F, false)) {
        ViewPane* target = last_hovered_pane_ ? last_hovered_pane_
                                              : machine_pane_.get();
        target->FitView();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        scene_->ActiveSelection().Clear();
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_G, false)
        && state_ != SessionState::kEmpty && state_ != SessionState::kPlaying) {
        GenerateMotion();
    }

    // 再生系 (バインド中のみ)
    if (!session_.player.IsBound()) return;
    if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) TogglePlay();
    if (ImGui::IsKeyPressed(ImGuiKey_Home, false)) SeekToTime(0.0);
    if (ImGui::IsKeyPressed(ImGuiKey_End, false)) {
        SeekToTime(session_.player.Duration());
    }
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true)) StepRecord(-1);
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true)) StepRecord(1);
}



/**
 * 中央領域 (2ビューと再生バー)
 */

void MachiningViewerGUI::RenderCenter(const ImVec2& pos, const ImVec2& size) {
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const ImGuiWindowFlags flags = kPanelFlags | ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoScrollWithMouse;
    const ImGuiWindowFlags child_flags = ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoScrollWithMouse;
    if (ImGui::Begin("##Center", nullptr, flags)) {
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float views_height = std::max(avail.y - kPlaybackBarHeight, 1.0f);
        if (layout_ == ViewLayout::kDual) {
            const float usable = std::max(avail.x - kSplitterWidth, 2.0f);
            const float left = std::floor(usable * split_ratio_);
            const ImVec2 left_size(left, views_height);
            const ImVec2 right_size(usable - left, views_height);
            if (ImGui::BeginChild("##machine_view", left_size, false, child_flags)) {
                RenderPane(*machine_pane_, left_size);
            }
            ImGui::EndChild();
            ImGui::SameLine(0.0f, 0.0f);
            // 分割バー: ドラッグで分割比を変える
            ImGui::InvisibleButton("##splitter", ImVec2(kSplitterWidth, views_height));
            if (ImGui::IsItemActive()) {
                split_ratio_ = std::clamp(
                        split_ratio_ + ImGui::GetIO().MouseDelta.x / usable,
                        kMinSplitRatio, kMaxSplitRatio);
            }
            if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            }
            const ImU32 bar_color = ImGui::GetColorU32(
                    ImGui::IsItemActive() ? ImGuiCol_SeparatorActive
                                          : ImGuiCol_Separator);
            ImGui::GetWindowDrawList()->AddRectFilled(
                    ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), bar_color);
            ImGui::SameLine(0.0f, 0.0f);
            if (ImGui::BeginChild("##work_view", right_size, false, child_flags)) {
                RenderPane(*work_pane_, right_size);
            }
            ImGui::EndChild();
        } else {
            ViewPane& pane = layout_ == ViewLayout::kMachineOnly
                    ? *machine_pane_ : *work_pane_;
            const ImVec2 pane_size(avail.x, views_height);
            if (ImGui::BeginChild("##single_view", pane_size, false, child_flags)) {
                RenderPane(pane, pane_size);
            }
            ImGui::EndChild();
        }
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
        if (ImGui::BeginChild("##playback", ImVec2(avail.x, kPlaybackBarHeight),
                              true, child_flags)) {
            RenderPlaybackBar();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void MachiningViewerGUI::RenderPane(ViewPane& pane, const ImVec2& size) {
    const std::optional<ClickInfo> click = pane.Render(size, input_config_);
    if (pane.IsHovered()) last_hovered_pane_ = &pane;
    if (click.has_value()) SelectByPick(pane, *click);
    if (pane.TakeScreenshotRequest()) {
        screenshot_pane_ = &pane;
        OpenModal(ModalKind::kScreenshot,
                  "screenshot " + pane.Name() + " " + CurrentTimeString() + ".png");
    }
}

void MachiningViewerGUI::RenderPlaybackBar() {
    if (!session_.player.IsBound() || !session_.track.has_value()) {
        ImGui::TextDisabled("Generate motion to enable playback");
        return;
    }
    RenderTransportRow();
    RenderCurrentInfoRow();
    RenderCurrentNcRow();
}

void MachiningViewerGUI::RenderTransportRow() {
    anim::AnimationPlayer& player = session_.player;
    if (ImGui::Button("|<")) SeekToTime(0.0);
    ImGui::SameLine();
    if (ImGui::Button("<")) StepRecord(-1);
    ImGui::SameLine();
    const bool playing = player.State() == anim::PlaybackState::kPlaying;
    if (ImGui::Button(playing ? "Pause" : "Play", ImVec2(56.0f, 0.0f))) {
        TogglePlay();
    }
    ImGui::SameLine();
    if (ImGui::Button(">")) StepRecord(1);
    ImGui::SameLine();
    if (ImGui::Button(">|")) SeekToTime(player.Duration());
    ImGui::SameLine();
    if (ImGui::Button("Stop")) {
        player.Stop();
        RequestRedraw();
    }
    ImGui::SameLine();

    // 時刻スライダ (残り幅から時刻表示の分を除く)
    const auto duration = static_cast<float>(player.Duration());
    auto time_ui = static_cast<float>(player.CurrentTime());
    const float text_width = ImGui::CalcTextSize("00000.00 / 00000.00 s").x;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    ImGui::SetNextItemWidth(std::max(
            ImGui::GetContentRegionAvail().x - text_width - spacing, 50.0f));
    if (ImGui::SliderFloat("##time", &time_ui, 0.0f, duration, "%.2f",
                           ImGuiSliderFlags_AlwaysClamp)) {
        SeekToTime(static_cast<double>(time_ui));
    }
    ImGui::SameLine();
    ImGui::Text("%.2f / %.2f s", player.CurrentTime(), player.Duration());
}

void MachiningViewerGUI::RenderCurrentInfoRow() {
    anim::AnimationPlayer& player = session_.player;
    ImGui::SetNextItemWidth(kSpeedSliderWidth);
    if (ImGui::SliderFloat("##speed", &speed_ui_, kMinSpeed, kMaxSpeed, "%.2fx",
                           ImGuiSliderFlags_Logarithmic
                           | ImGuiSliderFlags_AlwaysClamp)) {
        player.SetSpeed(static_cast<double>(speed_ui_));
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Loop", &loop_ui_)) player.SetLoop(loop_ui_);
    ImGui::SameLine();

    // 現在の工具、ワークオフセット、プログラム、行、レコード
    const MachiningSession& s = session_;
    std::string text;
    const std::optional<std::int64_t> tool = CurrentEventValue(m::kToolEventTrack);
    text += "| T" + (tool && *tool != m::kNoTool ? std::to_string(*tool) : "-");
    const std::optional<std::int64_t> record = CurrentEventValue(m::kRecordEventTrack);
    const std::size_t r = record && *record >= 0
            ? static_cast<std::size_t>(*record) : s.program->records.size();
    const bool valid = r < s.program->records.size();
    const int offset = valid && r < s.record_work_offsets.size()
            ? s.record_work_offsets[r] : -1;
    text += " | " + (offset >= 0 ? s.scene.WorkFrames()[offset].id : "-");
    const std::optional<std::int64_t> program =
            CurrentEventValue(m::kProgramEventTrack);
    for (const SourceFile& file : s.sources) {
        if (program && file.program_index == *program) text += " | " + file.name;
    }
    if (valid) {
        if (s.program->HasSources()) {
            text += " | line " + std::to_string(s.program->sources[r].line);
        }
        text += " | " + std::string(m::ClRecordKindName(s.program->records[r]));
        text += " | rec " + std::to_string(r);
    }
    ImGui::TextUnformatted(text.c_str());
}

void MachiningViewerGUI::RenderCurrentNcRow() {
    const MachiningSession& s = session_;
    const m::MotionTrack& track = *s.track;
    if (track.samples.empty()) return;
    const std::size_t index = m::SampleIndexAtTime(track, s.player.CurrentTime());
    const m::MachineModel& model = s.scene.Model();
    const m::NcValues nc = m::DisplayNc(model, track.samples[index].q);
    std::string text;
    for (const m::AxisInfo& axis : model.Axes()) {
        const double value = nc.GetOr(axis.register_name, 0.0);
        text += axis.register_name + " ";
        text += axis.kind == m::AxisKind::kRotary
                ? m::FormatFixed(m::ToDegrees(value), 2) : m::FormatFixed(value, 3);
        text += "   ";
    }
    ImGui::TextUnformatted(text.c_str());
}

void MachiningViewerGUI::SelectByPick(ViewPane& pane, const ClickInfo& click) {
    EntityRenderer& renderer = pane.Renderer();
    const Ray ray = renderer.GetRayFromScreen(click.x, click.y);
    const std::vector<EntityHit> hits = renderer.PickEntities(ray, click.x, click.y);
    const ModifierKey multi_mod = input_config_.multi_select_mod;
    const bool multi = multi_mod != ModifierKey::kNone
            && (click.mods & multi_mod) == multi_mod;
    models::SelectionSet& selection = scene_->ActiveSelection();
    if (hits.empty()) {
        if (!multi) selection.Clear();
        return;
    }

    const ObjectID id = hits.front().id;
    const bool by_assembly = scene_->Granularity()
            == models::SelectionGranularity::kAssembly;
    if (multi) {
        if (selection.Contains(id)) {
            if (by_assembly) {
                scene_->DeselectOwningAssembly(selection, id);
            } else {
                selection.Deselect(id);
            }
        } else if (by_assembly) {
            scene_->SelectOwningAssembly(selection, id);
        } else {
            scene_->TrySelectWithLock(selection, id);
        }
    } else {
        selection.Clear();
        if (by_assembly) {
            scene_->SelectOwningAssembly(selection, id);
        } else {
            scene_->TrySelectWithLock(selection, id);
        }
    }
    const models::Assembly* owner = scene_root_->FindOwner(id);
    SetStatus(std::string("Selected: ")
              + (owner && !owner->Metadata().name.empty()
                 ? owner->Metadata().name : "(unnamed)"));
}



/**
 * ステータスバーとモーダル
 */

void MachiningViewerGUI::RenderStatusBar() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(
            ImVec2(vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - kStatusBarHeight),
            ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, kStatusBarHeight),
                             ImGuiCond_Always);
    if (ImGui::Begin("##StatusBar", nullptr,
                     kPanelFlags | ImGuiWindowFlags_NoScrollbar)) {
        const std::string project = session_.project
                ? session_.project->name : std::string("-");
        const std::string machine = session_.project
                ? session_.project->machine.name : std::string("-");
        ImGui::Text("%s | %s | %s", project.c_str(), machine.c_str(),
                    StateLabel(state_, session_.player.State()));
        if (state_ == SessionState::kPlaying) {
            ImGui::SameLine();
            ImGui::Text("| %.0f fps", measured_fps_);
        }
        if (!status_.empty()) {
            ImGui::SameLine();
            ImGui::Text("| %s", status_.c_str());
        }
    }
    ImGui::End();
}

void MachiningViewerGUI::OpenModal(const ModalKind kind, std::string text) {
    modal_ = kind;
    modal_text_ = std::move(text);
    CopyToBuffer(modal_text_, modal_input_);
    modal_open_requested_ = true;
    RequestRedraw();
}

void MachiningViewerGUI::RenderModals() {
    if (modal_ == ModalKind::kNone) return;
    const char* title = kModalTitles[static_cast<int>(modal_)];
    if (modal_open_requested_) {
        ImGui::OpenPopup(title);
        modal_open_requested_ = false;
    }
    if (!ImGui::BeginPopupModal(title, nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        modal_ = ModalKind::kNone;
        return;
    }
    const ModalKind kind = modal_;
    const ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_EnterReturnsTrue;
    bool accepted = false;
    bool closed = false;
    switch (kind) {
        case ModalKind::kOpenProject:
            ImGui::SetNextItemWidth(600.0f);
            accepted = ImGui::InputText("Path", modal_input_.data(),
                                        modal_input_.size(), input_flags);
            accepted = ImGui::Button("Load") || accepted;
            break;
        case ModalKind::kOpenVirtual:
            ImGui::SetNextItemWidth(320.0f);
            ImGui::Combo("Machine", &virtual_input_.machine_kind, kVirtualMachineItems,
                         IM_ARRAYSIZE(kVirtualMachineItems));
            ImGui::SetNextItemWidth(600.0f);
            accepted = ImGui::InputText("Program path", modal_input_.data(),
                                        modal_input_.size(), input_flags);
            ImGui::SetNextItemWidth(120.0f);
            ImGui::Combo("Type", &virtual_input_.program_type, kProgramTypeItems,
                         IM_ARRAYSIZE(kProgramTypeItems));
            ImGui::SetNextItemWidth(120.0f);
            ImGui::InputFloat("Tool diameter [mm]", &virtual_input_.tool_diameter,
                              0.0f, 0.0f, "%.2f");
            ImGui::SetNextItemWidth(120.0f);
            ImGui::InputFloat("Tool length [mm]", &virtual_input_.tool_length,
                              0.0f, 0.0f, "%.1f");
            accepted = ImGui::Button("Load") || accepted;
            break;
        case ModalKind::kAddLibraryDir:
            ImGui::SetNextItemWidth(600.0f);
            accepted = ImGui::InputText("Directory", modal_input_.data(),
                                        modal_input_.size(), input_flags);
            accepted = ImGui::Button("Add") || accepted;
            break;
        case ModalKind::kScreenshot:
            ImGui::SetNextItemWidth(600.0f);
            accepted = ImGui::InputText("File", modal_input_.data(),
                                        modal_input_.size(), input_flags);
            accepted = ImGui::Button("Save") || accepted;
            break;
        case ModalKind::kError:
            ImGui::PushTextWrapPos(600.0f);
            ImGui::TextUnformatted(modal_text_.c_str());
            ImGui::PopTextWrapPos();
            closed = ImGui::Button("OK");
            break;
        case ModalKind::kControls:
            ImGui::TextUnformatted(
                    "Mouse (on a view)\n"
                    "  Middle drag         : rotate\n"
                    "  Ctrl + middle drag  : pan\n"
                    "  Shift + middle drag : zoom\n"
                    "  Wheel               : zoom\n"
                    "  Left click          : select\n"
                    "  Ctrl + left click   : add/remove selection\n"
                    "\n"
                    "Keys\n"
                    "  Ctrl+O : open project    F5  : reload project\n"
                    "  F      : fit view        Esc : clear selection\n"
                    "  Space  : play/pause      Home/End : start/end\n"
                    "  Left/Right : previous/next record\n"
                    "  Ctrl+G : generate motion");
            closed = ImGui::Button("Close");
            break;
        case ModalKind::kAbout:
            ImGui::TextUnformatted(
                    "Machining Viewer\n"
                    "IGESio machines extension example: machine definition, "
                    "project, tool path, motion and animation.");
            closed = ImGui::Button("Close");
            break;
        default:
            closed = true;
            break;
    }
    if (kind == ModalKind::kOpenProject || kind == ModalKind::kOpenVirtual
        || kind == ModalKind::kAddLibraryDir || kind == ModalKind::kScreenshot) {
        ImGui::SameLine();
        closed = ImGui::Button("Cancel");
    }
    if (accepted || closed) {
        ImGui::CloseCurrentPopup();
        modal_ = ModalKind::kNone;
    }
    ImGui::EndPopup();

    // 受け付けた入力の処理 (ポップアップの外で行う)
    if (!accepted) return;
    const std::string input(modal_input_.data());
    if (kind == ModalKind::kOpenProject) {
        LoadProject(input);
    } else if (kind == ModalKind::kOpenVirtual) {
        virtual_input_.program_path = input;
        if (std::filesystem::is_regular_file(input)) {
            LoadVirtualProject(virtual_input_);
        } else {
            ShowError("Program file not found: " + input);
        }
    } else if (kind == ModalKind::kAddLibraryDir) {
        if (std::filesystem::is_directory(input)) {
            session_.library_dirs.emplace_back(input);
            SetStatus("Library directory added (reload to apply)");
        } else {
            ShowError("Not a directory: " + input);
        }
    } else if (kind == ModalKind::kScreenshot && screenshot_pane_ != nullptr) {
        SaveScreenshot(*screenshot_pane_, input);
    }
}

void MachiningViewerGUI::ShowError(const std::string& message) {
    session_.log.push_back(LogEntry{
            LogStage::kGui, m::Diagnostic{m::Severity::kWarning, "", message, 0}});
    OpenModal(ModalKind::kError, message);
    SetStatus("Error: " + message);
}

void MachiningViewerGUI::SetStatus(std::string message) {
    status_ = std::move(message);
    RequestRedraw();
}

void MachiningViewerGUI::SaveScreenshot(ViewPane& pane, const std::string& path) {
    try {
        pane.CaptureScreenshot(path);
        SetStatus("Saved " + path);
    } catch (const std::exception& e) {
        ShowError(std::string("Failed to save the screenshot: ") + e.what());
    }
}



/**
 * セッション (読込と解除)
 */

void MachiningViewerGUI::AppendLog(
        const LogStage stage,
        const std::vector<m::Diagnostic>& diagnostics) {
    for (const m::Diagnostic& diagnostic : diagnostics) {
        session_.log.push_back(LogEntry{stage, diagnostic});
    }
}

void MachiningViewerGUI::ClearLogStages(
        const std::initializer_list<LogStage> stages) {
    std::vector<LogEntry>& log = session_.log;
    log.erase(std::remove_if(log.begin(), log.end(),
                             [&stages](const LogEntry& entry) {
                                 return std::find(stages.begin(), stages.end(),
                                                  entry.stage) != stages.end();
                             }),
              log.end());
}

void MachiningViewerGUI::DiscardLoadedData() {
    MachiningSession& s = session_;
    if (s.player.IsBound()) s.player.Unbind();
    s.track.reset();
    s.record_work_offsets.clear();
    s.path_of_record.clear();
    s.program.reset();
    s.loaded.clear();
    s.paths.clear();
    s.sources.clear();
    s.scene.Clear();
    s.setup.reset();
    s.project.reset();
    last_record_.reset();
    current_run_.reset();
    last_program_.reset();
    generate_result_.clear();
    settings_dirty_ = false;
    machine_pane_->Renderer().SetDisplayFilter(DisplayFilter{});
    work_pane_->Renderer().SetDisplayFilter(DisplayFilter{});
    work_pane_->Renderer().SetViewFrame(igesio::Matrix4d::Identity());
    scene_->ActiveSelection().Clear();
    state_ = SessionState::kEmpty;
}

void MachiningViewerGUI::ClearSession() {
    DiscardLoadedData();
    session_.project_path.clear();
    session_.log.clear();
    SetStatus("Cleared");
}

void MachiningViewerGUI::LoadProject(const std::filesystem::path& path) {
    DiscardLoadedData();
    session_.log.clear();
    try {
        m::ReadProjectOptions options;
        options.library_dirs = session_.library_dirs;
        session_.project = m::ReadProject(path, options);
        session_.project_path = path;
        AppendLog(LogStage::kProject, session_.project->warnings);
        FinishLoad();
        SetStatus("Loaded " + path.filename().string());
    } catch (const std::exception& e) {
        DiscardLoadedData();
        ShowError(e.what());
    }
}

void MachiningViewerGUI::LoadVirtualProject(const VirtualProjectInput& input) {
    DiscardLoadedData();
    session_.log.clear();
    session_.project_path.clear();
    try {
        const std::filesystem::path program_path =
                std::filesystem::absolute(input.program_path);
        m::VirtualMachineOptions machine_options;
        machine_options.name = "virtual";
        m::ProjectDefinition project = m::MakeProjectDefinition(
                m::MakeVirtualMachineDefinition(
                        static_cast<m::VirtualMachineKind>(input.machine_kind),
                        machine_options),
                "virtual");
        project.source_dir = program_path.parent_path();

        // 工具は径と長さから作る簡易アセンブリ1本 (番号1)
        const auto diameter = static_cast<double>(input.tool_diameter);
        const auto length = static_cast<double>(input.tool_length);
        m::SimpleToolSpec simple;
        simple.cutter = m::SimpleToolSpec::Cutter::kBall;
        simple.command_point = m::SimpleToolSpec::CommandPoint::kTip;
        simple.diameter = diameter;
        simple.cutting_length = 0.25 * length;
        simple.tool_length = length;
        simple.overhang = 0.75 * length;
        simple.holder_diameter = std::max(kVirtualHolderMinDiameter, 3.0 * diameter);
        simple.holder_length = kVirtualHolderLength;
        m::ToolEntry tool;
        tool.number = 1;
        tool.name = kVirtualToolNamePrefix + m::FormatFixed(diameter, 1);
        tool.source = simple;
        tool.control_point = m::ControlPoint::kTip;
        project.tools.push_back(std::move(tool));
        project.initial_tool = 1;

        m::ProgramSpec spec;
        spec.file.raw = program_path.string();
        spec.file.resolved = program_path;
        spec.type = static_cast<m::ProgramType>(input.program_type);
        project.programs.push_back(std::move(spec));

        session_.project = std::move(project);
        AppendLog(LogStage::kProject, session_.project->warnings);
        FinishLoad();
        SetStatus("Loaded " + program_path.filename().string()
                  + " with the virtual machine");
    } catch (const std::exception& e) {
        DiscardLoadedData();
        ShowError(e.what());
    }
}

void MachiningViewerGUI::FinishLoad() {
    MachiningSession& s = session_;
    InitializeRunSettingsUi();
    s.setup.emplace(*s.project);
    AppendLog(LogStage::kSetup, s.setup->Warnings());
    m::SceneBuildOptions options;
    options.lock_selection = false;
    s.scene.Build(*s.setup, scene_root_, options);
    AppendLog(LogStage::kScene, s.scene.Warnings());
    LoadProgramsForScene();
    ApplyMaterials();
    ApplyViewFilters();
    ApplyDisplayFlags();
    jog_q_ = m::JointVector(s.scene.Model().Axes().size(), 0.0);
    selected_component_.reset();
    target_ui_.result.clear();
    target_ui_.work_offset_index = 0;
    for (std::size_t i = 0; i < s.scene.WorkFrames().size(); ++i) {
        if (s.scene.WorkFrames()[i].id == s.scene.InitialWorkOffset()) {
            target_ui_.work_offset_index = static_cast<int>(i);
        }
    }
    target_ui_.tool_index = 0;
    int tool_position = 1;
    for (const auto& [number, spec] : s.scene.Tools()) {
        if (number == s.scene.ActiveTool()) target_ui_.tool_index = tool_position;
        ++tool_position;
    }
    trajectory_only_run_ui_ = 0;
    trajectory_dirty_ = false;
    if (!fit_done_) {
        machine_pane_->RequestFit();
        work_pane_->RequestFit();
        fit_done_ = true;
    }
    state_ = SessionState::kReady;
}

bool MachiningViewerGUI::LoadProgramsForScene() {
    MachiningSession& s = session_;
    s.loaded.clear();
    s.program.reset();
    s.paths.clear();
    s.sources.clear();
    std::vector<m::Diagnostic> warnings;
    try {
        m::ProgramLoadOptions options;
        options.dialect = m::DialectForProject(*s.project, m::DefaultFanucDialect());
        s.loaded = m::LoadPrograms(*s.setup, options, nullptr, &warnings);
        m::ClProgram program = m::ConcatenatePrograms(s.loaded);
        m::TrimToToolRange(program, s.project->run, &warnings);
        s.paths = m::EnumeratePaths(program);
        s.scene.RebuildPaths(program, &warnings);
        s.program = std::move(program);
        ReadSourceFiles();
        AppendLog(LogStage::kProgram, warnings);
        return true;
    } catch (const std::exception& e) {
        AppendLog(LogStage::kProgram, warnings);
        AppendLog(LogStage::kProgram,
                  {m::Diagnostic{m::Severity::kWarning, "", e.what(), 0}});
        s.loaded.clear();
        s.program.reset();
        s.paths.clear();
        s.sources.clear();
        return false;
    }
}

void MachiningViewerGUI::ReadSourceFiles() {
    MachiningSession& s = session_;
    for (const m::LoadedProgram& loaded : s.loaded) {
        const auto index = static_cast<std::size_t>(loaded.program_index);
        if (index >= s.project->programs.size()) continue;
        const m::ProgramSpec& spec = s.project->programs[index];
        SourceFile file;
        file.program_index = loaded.program_index;
        file.name = m::DisplayName(spec);
        try {
            file.lines = SplitLines(m::ReadTextFile(spec.file.resolved));
        } catch (const std::exception& e) {
            AppendLog(LogStage::kProgram,
                      {m::Diagnostic{m::Severity::kWarning, file.name,
                                     std::string("cannot read the source: ")
                                     + e.what(), 0}});
        }
        file.line_records.assign(file.lines.size() + 1, std::nullopt);
        s.sources.push_back(std::move(file));
    }

    // 行番号 → その行から生成された最初のレコード
    if (!s.program || !s.program->HasSources()) return;
    for (std::size_t i = 0; i < s.program->sources.size(); ++i) {
        const m::SourceLocation& location = s.program->sources[i];
        for (SourceFile& file : s.sources) {
            if (file.program_index != location.program_index) continue;
            const auto line = static_cast<std::size_t>(std::max(location.line, 0));
            if (line < file.line_records.size() && !file.line_records[line]) {
                file.line_records[line] = i;
            }
        }
    }
}

/**
 * 動作生成と再生
 */

void MachiningViewerGUI::InitializeRunSettingsUi() {
    const m::RunSettings& run = session_.project->run;
    run_ui_.start_tool = run.start_tool.value_or(0);
    run_ui_.stop_tool = run.stop_tool.value_or(0);
    run_ui_.overtravel_index = static_cast<int>(run.overtravel);
}

m::MotionOptions MachiningViewerGUI::MotionOptionsFromUi() const {
    m::MotionOptions options;
    options.fps = static_cast<double>(motion_ui_.fps);
    options.max_samples = static_cast<std::size_t>(std::max(motion_ui_.max_samples, 1));
    options.interpolate = motion_ui_.interpolate;
    options.fallback_feed = static_cast<double>(motion_ui_.fallback_feed_mm_per_min)
            / m::kSecondsPerMinute;
    options.arc_chord_tolerance = static_cast<double>(motion_ui_.arc_chord_tolerance);
    if (motion_ui_.branch_index > 0) {
        options.branch = static_cast<m::BranchPolicy>(motion_ui_.branch_index - 1);
    }
    options.overtravel = static_cast<m::OvertravelPolicy>(run_ui_.overtravel_index);
    if (motion_ui_.fixed_time) {
        options.fixed_record_seconds = static_cast<double>(motion_ui_.fixed_seconds);
    }
    return options;
}

void MachiningViewerGUI::GenerateMotion() {
    MachiningSession& s = session_;
    if (!s.project || !s.setup || !s.scene.IsBuilt()) return;
    const auto started = std::chrono::steady_clock::now();
    // セットアップからすべて作り直すので、前回の診断を捨てる
    ClearLogStages({LogStage::kSetup, LogStage::kProgram, LogStage::kMotion,
                    LogStage::kClip, LogStage::kTrajectory});
    if (s.player.IsBound()) s.player.Unbind();
    s.track.reset();
    s.record_work_offsets.clear();
    s.path_of_record.clear();
    last_record_.reset();
    current_run_.reset();
    last_program_.reset();
    try {
        // 実行制御のUI値を反映してセットアップを作り直す (シーンは作り直さない)
        m::RunSettings& run = s.project->run;
        run.start_tool = run_ui_.start_tool > 0
                ? std::optional<int>(run_ui_.start_tool) : std::nullopt;
        run.stop_tool = run_ui_.stop_tool > 0
                ? std::optional<int>(run_ui_.stop_tool) : std::nullopt;
        run.overtravel = static_cast<m::OvertravelPolicy>(run_ui_.overtravel_index);
        s.setup.emplace(*s.project);
        AppendLog(LogStage::kSetup, s.setup->Warnings());
        if (!LoadProgramsForScene() || !s.program || s.program->records.empty()) {
            ShowError("No program could be loaded (see the log)");
            return;
        }

        s.track = m::PlanMotion(*s.setup, *s.program, MotionOptionsFromUi());
        AppendLog(LogStage::kMotion, s.track->warnings);
        if (s.track->samples.empty()) {
            s.track.reset();
            ShowError("The program has no motion records");
            return;
        }
        // クリップはゼロポーズと初期工具を基準状態として作る
        s.scene.ResetToZeroPose();
        s.scene.SetActiveTool(s.setup->InitialTool());
        std::vector<m::Diagnostic> warnings;
        anim::AnimationClip clip =
                m::MakeMachineClip(s.scene, *s.track, *s.program, {}, &warnings);
        AppendLog(LogStage::kClip, warnings);
        s.player.Bind(scene_root_, std::move(clip));
        s.player.SetSpeed(static_cast<double>(speed_ui_));
        s.player.SetLoop(loop_ui_);
        m::RebuildMotionTrace(s.scene, *s.track, trace_options_);
        std::vector<m::Diagnostic> trajectory_warnings;
        m::RebuildToolTrajectory(s.scene, *s.track, *s.program, trajectory_options_,
                                 &trajectory_warnings);
        AppendLog(LogStage::kTrajectory, trajectory_warnings);
        ApplyMaterials();
        ApplyDisplayFlags();
        ApplyTrajectoryVisibility();
        m::SetToolTrajectoryHolderVisible(s.scene, trajectory_options_.holder_visible);
        BuildRecordIndices();
        trajectory_dirty_ = false;
    } catch (const std::exception& e) {
        if (s.player.IsBound()) s.player.Unbind();
        s.track.reset();
        ShowError(e.what());
        return;
    }

    const double elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - started).count();
    const m::MotionStats& stats = s.track->stats;
    generate_result_ = std::to_string(stats.motion_record_count) + " motion records, "
            + std::to_string(stats.sample_count) + " samples, "
            + m::FormatFixed(stats.duration_sec, 2) + " s ("
            + m::FormatFixed(elapsed, 1) + " s elapsed)";
    settings_dirty_ = false;
    SetStatus("Motion generated: " + generate_result_);
    UpdateState();
}

void MachiningViewerGUI::ReleaseMotion() {
    if (!session_.player.IsBound()) return;
    // Unbindは基準状態 (ゼロポーズと初期工具) に復元する
    session_.player.Unbind();
    last_record_.reset();
    jog_q_ = m::JointVector(session_.scene.Model().Axes().size(), 0.0);
    UpdateState();
    SetStatus("Animation released");
}

void MachiningViewerGUI::BuildRecordIndices() {
    MachiningSession& s = session_;
    const m::ClProgram& program = *s.program;
    const std::vector<m::WorkFrame>& frames = s.scene.WorkFrames();
    const auto index_of = [&frames](const std::string& id) {
        for (std::size_t i = 0; i < frames.size(); ++i) {
            if (frames[i].id == id) return static_cast<int>(i);
        }
        return -1;
    };
    // レコードごとのワークオフセット (未選択の間は初期ワークオフセット)
    const int initial = index_of(s.scene.InitialWorkOffset());
    s.record_work_offsets.assign(program.records.size(), -1);
    m::ClState state;
    std::string last_id;
    int last_index = initial;
    for (std::size_t i = 0; i < program.records.size(); ++i) {
        state.Apply(program.records[i]);
        if (state.work_offset != last_id) {
            last_id = state.work_offset;
            last_index = last_id.empty() ? initial : index_of(last_id);
        }
        s.record_work_offsets[i] = last_index;
    }
    // レコード → 経路区間
    s.path_of_record.assign(program.records.size(), 0);
    for (std::size_t k = 0; k < s.paths.size(); ++k) {
        for (std::size_t r = s.paths[k].begin;
             r < s.paths[k].end && r < s.path_of_record.size(); ++r) {
            s.path_of_record[r] = k;
        }
    }
}

std::optional<std::int64_t> MachiningViewerGUI::CurrentEventValue(
        const std::string_view track_name) const {
    const anim::AnimationPlayer& player = session_.player;
    if (!player.IsBound()) return std::nullopt;
    const anim::EventTrack* track =
            player.Clip().FindEventTrack(std::string(track_name));
    if (track == nullptr) return std::nullopt;
    return anim::ActiveEventValue(*track, player.CurrentTime());
}

void MachiningViewerGUI::TrackCurrentRecord() {
    MachiningSession& s = session_;
    const std::optional<std::int64_t> record = CurrentEventValue(m::kRecordEventTrack);
    if (record == last_record_) return;
    last_record_ = record;
    if (!record || *record < 0 || !s.track || !s.program) return;
    const auto r = static_cast<std::size_t>(*record);
    if (r >= s.program->records.size()) return;

    if (display_flags_.current_record) m::UpdateCurrentRecord(s.scene, *s.track, r);
    if (r < s.path_of_record.size() && current_run_ != s.path_of_record[r]) {
        current_run_ = s.path_of_record[r];
        if (trajectory_always_current_) ApplyTrajectoryVisibility();
    }
    source_follow_pending_ = true;
    RequestRedraw();
}



/**
 * ジョグ、工具、表示
 */

void MachiningViewerGUI::SetJogPose(const m::JointVector& q) {
    if (!session_.scene.IsBuilt() || session_.player.IsBound()) return;
    jog_q_ = q;
    session_.scene.ApplyPose(jog_q_);
    RequestRedraw();
}

m::JointVector MachiningViewerGUI::DisplayedPose() const {
    const MachiningSession& s = session_;
    if (s.player.IsBound() && s.track && !s.track->samples.empty()) {
        const std::size_t index =
                m::SampleIndexAtTime(*s.track, s.player.CurrentTime());
        return s.track->samples[index].q;
    }
    return jog_q_;
}

void MachiningViewerGUI::SelectComponent(const std::size_t index) {
    const MachiningSession& s = session_;
    if (!s.scene.IsBuilt() || index >= s.scene.Model().ComponentCount()) return;
    selected_component_ = index;
    models::SelectionSet& selection = scene_->ActiveSelection();
    selection.Clear();
    const std::shared_ptr<models::Assembly> assembly =
            s.scene.ComponentAssembly(s.scene.Model().Component(index).name);
    if (assembly == nullptr) return;
    for (const ObjectID& id : assembly->GetEntityIDs(true)) {
        scene_->TrySelectWithLock(selection, id);
    }
    SetStatus("Selected: " + s.scene.Model().Component(index).name);
}

void MachiningViewerGUI::SolveTarget() {
    MachiningSession& s = session_;
    if (!s.setup || !s.scene.IsBuilt() || s.player.IsBound()) return;
    m::ClTarget target;
    target.point = igesio::Vector3d(target_ui_.point[0], target_ui_.point[1],
                                    target_ui_.point[2]);
    target.tool_axis = igesio::Vector3d(target_ui_.tool_axis[0],
                                        target_ui_.tool_axis[1],
                                        target_ui_.tool_axis[2]);
    const std::vector<m::WorkFrame>& frames = s.scene.WorkFrames();
    if (target_ui_.work_offset_index >= 0
        && static_cast<std::size_t>(target_ui_.work_offset_index) < frames.size()) {
        target.work_offset =
                frames[static_cast<std::size_t>(target_ui_.work_offset_index)].id;
    }
    target.tool = m::kNoTool;
    int tool_position = 1;
    for (const auto& [number, spec] : s.scene.Tools()) {
        if (tool_position++ == target_ui_.tool_index) target.tool = number;
    }
    std::optional<m::BranchPolicy> branch;
    if (motion_ui_.branch_index > 0) {
        branch = static_cast<m::BranchPolicy>(motion_ui_.branch_index - 1);
    }

    std::vector<m::Diagnostic> warnings;
    try {
        const std::optional<m::IkSolution> solution =
                m::SolveClTarget(*s.setup, target, jog_q_, branch, &warnings);
        AppendLog(LogStage::kGui, warnings);
        if (!solution.has_value()) {
            target_ui_.result = "unreachable";
            return;
        }
        if (solution->q.has_value()) SetJogPose(*solution->q);
        target_ui_.result = solution->singular ? "solved (singular)" : "solved";
        if (solution->error.has_value()) {
            target_ui_.result += " (error: "
                    + m::FormatFixed(solution->error->position, 6) + " mm, "
                    + m::FormatDegrees(solution->error->angle, 6) + " deg)";
        }
        if (!warnings.empty()) {
            target_ui_.result += " with " + std::to_string(warnings.size())
                    + " warning(s)";
        }
    } catch (const std::exception& e) {
        target_ui_.result = e.what();
    }
}

void MachiningViewerGUI::ApplyDisplayFlags() {
    m::MachineScene& scene = session_.scene;
    if (!scene.IsBuilt()) return;
    const DisplayFlags& f = display_flags_;
    scene.SetPathsVisible(f.paths);
    scene.SetRapidPathsVisible(f.rapid_paths);
    scene.SetWorkFramesVisible(f.work_frames);
    scene.SetTriadsVisible(f.triads);
    scene.SetToolAxisVisible(f.tool_axis);
    scene.SetHolderVisible(f.holder);
    scene.SetMachinePartsVisible(f.machine_parts);
    for (std::size_t i = 0; i < f.model_roles.size(); ++i) {
        scene.SetModelRoleVisible(static_cast<m::ModelRole>(i), f.model_roles[i]);
    }
    if (const auto trace = scene.MachineTraceAssembly()) {
        trace->SetVisible(f.trace_machine);
    }
    if (const auto trace = scene.WorkTraceAssembly()) trace->SetVisible(f.trace_work);
    SetCurrentRecordVisible(f.current_record);
    RequestRedraw();
}

void MachiningViewerGUI::SetCurrentRecordVisible(const bool visible) {
    MachiningSession& s = session_;
    display_flags_.current_record = visible;
    if (!s.scene.IsBuilt()) return;
    for (const auto& parent : {s.scene.MachineTraceAssembly(),
                               s.scene.WorkTraceAssembly()}) {
        if (parent == nullptr) continue;
        const auto current = m::FindChildAssembly(*parent, m::kCurrentRecordName);
        if (current != nullptr && !visible) current->SetVisible(false);
    }
    // 表示に戻すときは現在レコードで折れ線と可視性を設定し直す
    if (visible && s.player.IsBound() && s.track && s.program && last_record_
        && *last_record_ >= 0
        && static_cast<std::size_t>(*last_record_) < s.program->records.size()) {
        m::UpdateCurrentRecord(s.scene, *s.track,
                               static_cast<std::size_t>(*last_record_));
    }
}

void MachiningViewerGUI::ApplyTrajectoryVisibility() {
    MachiningSession& s = session_;
    if (!s.scene.IsBuilt()) return;
    trajectory_visibility_.only_run = trajectory_only_run_ui_ > 0
            ? std::optional<std::size_t>(
                      static_cast<std::size_t>(trajectory_only_run_ui_ - 1))
            : std::nullopt;
    trajectory_visibility_.always_visible_run =
            trajectory_always_current_ ? current_run_ : std::nullopt;
    m::SetToolTrajectoryVisible(s.scene, trajectory_visibility_);
    RequestRedraw();
}

void MachiningViewerGUI::RebuildTrajectory() {
    MachiningSession& s = session_;
    if (!s.scene.IsBuilt() || !s.track || !s.program) return;
    trajectory_options_.opacity = trajectory_opacity_enabled_
            ? std::optional<float>(trajectory_opacity_ui_) : std::nullopt;
    std::vector<m::Diagnostic> warnings;
    try {
        m::RebuildToolTrajectory(s.scene, *s.track, *s.program, trajectory_options_,
                                 &warnings);
    } catch (const std::exception& e) {
        ShowError(e.what());
        return;
    }
    // 工具軌跡だけを作り直すので、前回の工具軌跡の診断と入れ替える
    ClearLogStages({LogStage::kTrajectory});
    AppendLog(LogStage::kTrajectory, warnings);
    ApplyMaterials();
    ApplyTrajectoryVisibility();
    m::SetToolTrajectoryHolderVisible(s.scene, trajectory_options_.holder_visible);
    trajectory_dirty_ = false;
    SetStatus("Tool trajectory rebuilt");
}

void MachiningViewerGUI::SeekToTime(const double time_sec) {
    anim::AnimationPlayer& player = session_.player;
    if (!player.IsBound()) return;
    player.Seek(std::clamp(time_sec, 0.0, player.Duration()));
    RequestRedraw();
}

void MachiningViewerGUI::StepRecord(const int direction) {
    const anim::AnimationPlayer& player = session_.player;
    if (!player.IsBound()) return;
    const anim::EventTrack* track =
            player.Clip().FindEventTrack(std::string(m::kRecordEventTrack));
    if (track == nullptr || track->keys.empty()) return;
    const std::vector<anim::EventKey>& keys = track->keys;
    // 現在時刻以下の最後のキーを現在のキーとし、その前後に移動する
    const auto upper = std::upper_bound(
            keys.begin(), keys.end(), player.CurrentTime(),
            [](const double time, const anim::EventKey& key) {
                return time < key.time_sec;
            });
    const auto current = static_cast<std::ptrdiff_t>(upper - keys.begin()) - 1;
    const auto last = static_cast<std::ptrdiff_t>(keys.size()) - 1;
    const std::ptrdiff_t target = std::clamp<std::ptrdiff_t>(
            current + direction, 0, last);
    SeekToTime(keys[static_cast<std::size_t>(target)].time_sec);
}

void MachiningViewerGUI::SeekToRecord(const std::size_t record) {
    const MachiningSession& s = session_;
    if (!s.player.IsBound() || !s.track) return;
    const m::MotionTrack& track = *s.track;
    if (record < track.record_first_sample.size()
        && track.record_first_sample[record] < track.samples.size()) {
        SeekToTime(track.samples[track.record_first_sample[record]].time);
    } else {
        SeekToTime(s.player.Duration());
    }
}

void MachiningViewerGUI::TogglePlay() {
    anim::AnimationPlayer& player = session_.player;
    if (!player.IsBound()) return;
    if (player.State() == anim::PlaybackState::kPlaying) {
        player.Pause();
    } else {
        if (player.State() == anim::PlaybackState::kFinished) player.Seek(0.0);
        player.Play();
    }
    UpdateState();
    RequestRedraw();
}



/**
 * 材質と表示フィルタ
 */

void MachiningViewerGUI::ApplyMaterials() {
    MaterialProperty material;
    material.metallic = m::kToolMetallic;
    material.roughness = m::kToolRoughness;
    std::vector<ObjectID> ids = session_.scene.MetallicSurfaceIds();
    const std::vector<ObjectID> trajectory =
            m::ToolTrajectoryMetallicIds(session_.scene);
    ids.insert(ids.end(), trajectory.begin(), trajectory.end());
    for (const ObjectID& id : ids) {
        machine_pane_->Renderer().SetMaterialProperty(id, material);
        work_pane_->Renderer().SetMaterialProperty(id, material);
    }
}

void MachiningViewerGUI::ApplyViewFilters() {
    DisplayFilter machine_filter;
    for (const ObjectID& id : session_.scene.MachineViewHiddenIds()) {
        machine_filter.hidden_assemblies.insert(id);
    }
    machine_pane_->Renderer().SetDisplayFilter(machine_filter);
    DisplayFilter work_filter;
    for (const ObjectID& id : session_.scene.WorkViewHiddenIds(work_view_options_)) {
        work_filter.hidden_assemblies.insert(id);
    }
    work_pane_->Renderer().SetDisplayFilter(work_filter);
}

}  // namespace igesio::graphics
