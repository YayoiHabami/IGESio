/**
 * @file examples/gui/iges_viewer_gui.cpp
 * @brief IGESのエンティティを表示・編集する単一GUIクラスの実装
 * @author Yayoi Habami
 * @date 2025-08-05
 * @copyright 2025 Yayoi Habami
 */
#include "./iges_viewer_gui.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include <igesio/reader.h>

namespace {

using IgesViewerGUI = igesio::graphics::IgesViewerGUI;
using igesio::graphics::MouseButton;
using igesio::graphics::ModifierKey;
using igesio::graphics::ProjectionMode;

/// @brief クリックと判定する移動量の閾値 [px]
constexpr double kClickThresholdPx = 5.0;
/// @brief 左Outlinerパネルの幅 [px]
constexpr float kOutlinerWidth = 260.0f;
/// @brief 右Inspectorパネルの幅 [px]
constexpr float kInspectorWidth = 320.0f;
/// @brief 下部ステータスバーの高さ [px]
constexpr float kStatusBarHeight = 28.0f;
/// @brief 端アンカーパネル共通のウィンドウフラグ
constexpr ImGuiWindowFlags kPanelFlags = ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoBringToFrontOnFocus;

/// @brief GLFWのマウスボタン定数をMouseButtonへ変換する
MouseButton ToMouseButton(const int glfw_button) {
    switch (glfw_button) {
        case GLFW_MOUSE_BUTTON_LEFT: return MouseButton::kLeft;
        case GLFW_MOUSE_BUTTON_MIDDLE: return MouseButton::kMiddle;
        case GLFW_MOUSE_BUTTON_RIGHT: return MouseButton::kRight;
        default: return MouseButton::kNone;
    }
}

/// @brief GLFWの修飾キービットをModifierKeyへ変換する
ModifierKey ToModifierKey(const int glfw_mods) {
    ModifierKey mods = ModifierKey::kNone;
    if (glfw_mods & GLFW_MOD_CONTROL) mods = mods | ModifierKey::kCtrl;
    if (glfw_mods & GLFW_MOD_SHIFT) mods = mods | ModifierKey::kShift;
    if (glfw_mods & GLFW_MOD_ALT) mods = mods | ModifierKey::kAlt;
    if (glfw_mods & GLFW_MOD_SUPER) mods = mods | ModifierKey::kSuper;
    return mods;
}

/// @brief 指定IDのAssemblyノードをツリーから探す (再帰)
igesio::models::Assembly* FindAssemblyById(
        igesio::models::Assembly& node, const igesio::ObjectID& id) {
    if (node.GetID() == id) return &node;
    for (const auto& child : node.GetChildAssemblies()) {
        if (!child) continue;
        if (auto* found = FindAssemblyById(*child, id)) return found;
    }
    return nullptr;
}

/// @brief 現在時刻を文字列で取得する (スクリーンショットのファイル名用)
std::string CurrentTimeString(const std::string& format = "%Y-%m-%d %H%M%S") {
    const auto now = std::chrono::system_clock::now();
    const auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm_buf, format.c_str());
    return ss.str();
}

/// @brief エンティティのタイプ文字列を取得する
std::string GetEntityTypeString(const igesio::ObjectID& id) {
    if (auto entity_type = id.GetIdentifier()->GetEntityType()) {
        return igesio::entities::ToString(
            static_cast<igesio::entities::EntityType>(*entity_type));
    }
    return "Unknown";
}

}  // namespace



/**
 * 初期化・終了
 */

IgesViewerGUI::IgesViewerGUI(
        const int width, const int height,
        const int msaa_samples, const std::string& initial_iges_file)
        : renderer_(nullptr, width, height),
          initial_iges_file_(initial_iges_file) {
    glfwSetErrorCallback(ErrorCallback);
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
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
    glfwSetWindowUserPointer(window_, this);
    glfwSwapInterval(1);

    // コンテキストをカレント化した後にGLバックエンドをロードしてレンダラへ設定する
    // (CreateGLBackendはロード失敗時にImplementationErrorを投げる)
    try {
        renderer_.SetGLBackend(CreateGLBackend(
                reinterpret_cast<GLProcLoader>(glfwGetProcAddress)));
    } catch (const std::exception&) {
        glfwDestroyWindow(window_);
        glfwTerminate();
        throw;
    }

    SetupCallbacks();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 400 core");

    // 透過描画を有効化 (色/不透明度オーバーライドの表現用)
    renderer_.EnableTransparency(true);

    // 空のSceneを生成し、レンダラへ束ねる (ロード時にBindSceneRootで差し替える)
    scene_ = std::make_unique<models::Scene>(std::make_shared<models::Assembly>());
    renderer_.SetScene(scene_.get());
}

IgesViewerGUI::~IgesViewerGUI() {
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
 * 実行ループ
 */

void IgesViewerGUI::Run(const bool vsync) {
    renderer_.Initialize();
    glfwSwapInterval(vsync ? 1 : 0);
    if (msaa_samples_ > 0) {
        renderer_.EnableAntialiasing(true);
    }

    // 起動時ファイルの読み込み (Initialize後にGLが有効)
    if (!initial_iges_file_.empty()) {
        LoadIgesFile(initial_iges_file_);
        initial_iges_file_.clear();
    }

    while (!glfwWindowShouldClose(window_)) {
        glfwWaitEvents();

        if (ImGui::IsAnyItemActive()) needs_redraw_ = true;
        if (!needs_redraw_) continue;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        HandleHotkeys();
        RenderMenuBar();
        RenderLoadPopup();
        RenderOutliner();
        RenderInspector();
        RenderStatusBar();
        RenderViewportOverlay();

        // 全パネル描画後に、遅延した構造編集を1回だけ実行する
        // (ツリー走査中の変更によるダングリングを避ける)
        if (pending_action_) {
            auto action = pending_action_;
            pending_action_ = nullptr;
            action();
        }

        renderer_.Draw();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window_);
    }
}

void IgesViewerGUI::CaptureScreenshot(const std::string& filename) {
    auto texture = renderer_.CaptureScreenshot();
    igesio::graphics::SaveTextureToFile(filename, texture);
}



/**
 * モデルの読み込み・同期
 */

void IgesViewerGUI::LoadIgesFile(const std::string& filename) {
    try {
        iges_data_ = igesio::ReadIges(filename);

        // 既存の描画オブジェクト・キャッシュをクリア
        for (auto& p : entities_) {
            for (auto& entity : p.second) renderer_.RemoveEntity(entity->GetID());
        }
        entities_.clear();
        show_entity_.clear();
        focused_assembly_id_ = std::nullopt;
        selected_hit_positions_.clear();
        last_edit_status_.clear();

        // エンティティを取得してレンダラ・キャッシュへ追加
        for (const auto& pair : iges_data_.Root().GetEntities()) {
            AddEntity(pair.second);
        }

        // シーンをモデルのrootへ束ねる (描画時にツリーを走査させる)
        BindSceneRoot(iges_data_.RootPtr());
        renderer_.Camera().Reset();
        // 起動後の初回読み込み時のみ、モデル全体を自動でフィットする
        if (!initial_fit_done_) {
            renderer_.FitView();
            initial_fit_done_ = true;
        }
        needs_redraw_ = true;
    } catch (const std::exception& e) {
        std::cerr << "Error loading IGES file: " << e.what() << std::endl;
        last_edit_status_ = std::string("Load error: ") + e.what();
    }
}

void IgesViewerGUI::AddEntity(std::shared_ptr<entities::EntityBase> entity) {
    if (!entity->IsSupported()) {
        std::cerr << "Entity type " << ToString(entity->GetType())
                  << " is not supported." << std::endl;
        return;
    } else if (auto result = entity->Validate(); !result.is_valid) {
        std::cerr << "Entity " << entity->GetID() << " is invalid: "
                  << result.Message() << std::endl;
        return;
    } else if (!result.errors.empty()) {
        // 警告のみ (幾何的品質の指摘など): 描画は継続しつつ診断を表示する
        std::cerr << "Entity " << entity->GetID() << " has warnings: "
                  << result.Message() << std::endl;
    }

    const entities::EntityType type = entity->GetType();
    if (entity->GetSubordinateEntitySwitch()
            == entities::SubordinateEntitySwitch::kPhysicallyDependent) {
        // 物理従属は親エンティティ経由で描画されるためスキップ
        return;
    } else if (type == entities::EntityType::kTransformationMatrix
               || type == entities::EntityType::kColorDefinition) {
        return;
    }

    if (!renderer_.AddEntity(entity)) {
        std::cerr << "Failed to add entity " << entity->GetID() << std::endl;
        return;
    }
    entities_[type].push_back(entity);
    show_entity_[type] = true;
}

void IgesViewerGUI::UpdateEntities() {
    for (auto& p : show_entity_) {
        const bool show = p.second;
        for (auto& entity : entities_[p.first]) {
            if (show) {
                renderer_.AddEntity(entity);
            } else {
                renderer_.RemoveEntity(entity->GetID());
            }
        }
    }
    needs_redraw_ = true;
}

void IgesViewerGUI::BindSceneRoot(std::shared_ptr<models::Assembly> root) {
    // 新Sceneを生成 → レンダラのポインタを更新 → 旧Sceneを解放 (ダングリング回避)
    auto new_scene = std::make_unique<models::Scene>(std::move(root));
    renderer_.SetScene(new_scene.get());
    scene_ = std::move(new_scene);
    needs_redraw_ = true;
}

void IgesViewerGUI::OnModelEdited() {
    auto& sel = scene_->ActiveSelection();
    // 消えたIDを選択・hit座標から除去する
    const std::vector<ObjectID> snapshot(sel.Items().begin(), sel.Items().end());
    for (const auto& id : snapshot) {
        if (scene_->Root().FindOwner(id) == nullptr) sel.Deselect(id);
    }
    for (auto it = selected_hit_positions_.begin();
         it != selected_hit_positions_.end();) {
        if (scene_->Root().FindOwner(it->first) == nullptr) {
            it = selected_hit_positions_.erase(it);
        } else {
            ++it;
        }
    }
    // ツリーから消えたエンティティの描画オブジェクトを除去し、型別キャッシュも剪定する
    for (auto& p : entities_) {
        auto& vec = p.second;
        vec.erase(std::remove_if(vec.begin(), vec.end(),
                [&](const std::shared_ptr<entities::EntityBase>& e) {
                    if (scene_->Root().FindOwner(e->GetID()) != nullptr) {
                        return false;
                    }
                    renderer_.RemoveEntity(e->GetID());
                    return true;
                }), vec.end());
    }
    renderer_.MarkSceneDirty();
    needs_redraw_ = true;
}



/**
 * パネル描画
 */

void IgesViewerGUI::RenderMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Load IGES...")) request_load_popup_ = true;
        if (ImGui::MenuItem("Screenshot")) {
            CaptureScreenshot("screenshot " + CurrentTimeString() + ".png");
            last_edit_status_ = "Saved screenshot.";
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) {
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        int mode = static_cast<int>(renderer_.Camera().GetProjectionMode());
        if (ImGui::RadioButton("Perspective", &mode,
                static_cast<int>(ProjectionMode::kPerspective))) {
            renderer_.Camera().SetProjectionMode(ProjectionMode::kPerspective);
            needs_redraw_ = true;
        }
        if (ImGui::RadioButton("Orthographic", &mode,
                static_cast<int>(ProjectionMode::kOrthographic))) {
            renderer_.Camera().SetProjectionMode(ProjectionMode::kOrthographic);
            needs_redraw_ = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Reset Camera")) {
            renderer_.Camera().Reset();
            needs_redraw_ = true;
        }
        if (ImGui::MenuItem("Fit View", "F")) {
            renderer_.FitView();
            needs_redraw_ = true;
        }
        if (ImGui::BeginMenu("Standard Views")) {
            if (ImGui::MenuItem("Front"))  ApplyStandardView(StandardView::kFront);
            if (ImGui::MenuItem("Back"))   ApplyStandardView(StandardView::kBack);
            if (ImGui::MenuItem("Top"))    ApplyStandardView(StandardView::kTop);
            if (ImGui::MenuItem("Bottom")) ApplyStandardView(StandardView::kBottom);
            if (ImGui::MenuItem("Right"))  ApplyStandardView(StandardView::kRight);
            if (ImGui::MenuItem("Left"))   ApplyStandardView(StandardView::kLeft);
            ImGui::Separator();
            if (ImGui::MenuItem("Isometric")) ApplyStandardView(StandardView::kIso);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Display Mode")) {
            // 面と面エッジの描画組み合わせを切り替える (従属しない曲線は常時表示)
            const DisplayMode mode = renderer_.GetDisplayMode();
            if (ImGui::MenuItem("Shaded", nullptr, mode == DisplayMode::kShaded)) {
                renderer_.SetDisplayMode(DisplayMode::kShaded);
                needs_redraw_ = true;
            }
            if (ImGui::MenuItem("Wireframe", nullptr,
                                mode == DisplayMode::kWireFrame)) {
                renderer_.SetDisplayMode(DisplayMode::kWireFrame);
                needs_redraw_ = true;
            }
            if (ImGui::MenuItem("No Edge", nullptr, mode == DisplayMode::kNoEdge)) {
                renderer_.SetDisplayMode(DisplayMode::kNoEdge);
                needs_redraw_ = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::ColorEdit3("Background",
                              renderer_.GetBackgroundColorRef().data())) {
            needs_redraw_ = true;
        }
        bool aa = renderer_.IsAntialiasingEnabled();
        if (ImGui::MenuItem("Antialiasing", nullptr, &aa)) {
            renderer_.EnableAntialiasing(aa);
            needs_redraw_ = true;
        }
        bool transparency = renderer_.IsTransparencyEnabled();
        if (ImGui::MenuItem("Transparency", nullptr, &transparency)) {
            renderer_.EnableTransparency(transparency);
            needs_redraw_ = true;
        }
        ImGui::Separator();
        if (ImGui::BeginMenu("Filters")) {
            if (ImGui::Checkbox("Show All", &show_all_)) {
                for (auto& p : show_entity_) p.second = show_all_;
                UpdateEntities();
            }
            ImGui::Separator();
            bool changed = false;
            for (auto& p : show_entity_) {
                if (ImGui::Checkbox(ToString(p.first).c_str(), &p.second)) {
                    changed = true;
                    if (show_all_ && !p.second) show_all_ = false;
                }
            }
            if (changed) UpdateEntities();
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Select")) {
        int gran = (scene_->Granularity()
                == models::SelectionGranularity::kAssembly) ? 1 : 0;
        if (ImGui::RadioButton("Body", &gran, 0)) {
            scene_->SetGranularity(models::SelectionGranularity::kBody);
        }
        if (ImGui::RadioButton("Assembly", &gran, 1)) {
            scene_->SetGranularity(models::SelectionGranularity::kAssembly);
        }
        ImGui::Separator();
        ImGui::Checkbox("Pick bodies", &scene_->Filter().bodies);
        ImGui::Separator();
        ImGui::TextDisabled("Removal policy");
        ImGui::RadioButton("Reject", &removal_policy_, 0);
        ImGui::RadioButton("Cascade", &removal_policy_, 1);
        ImGui::RadioButton("Orphan", &removal_policy_, 2);
        ImGui::Separator();
        if (ImGui::MenuItem("Deselect All")) {
            scene_->ActiveSelection().Clear();
            selected_hit_positions_.clear();
            needs_redraw_ = true;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
        ImGui::TextDisabled("Camera");
        ImGui::TextDisabled("  Middle Drag: Rotate");
        ImGui::TextDisabled("  Ctrl + Middle: Pan");
        ImGui::TextDisabled("  Wheel / Shift + Middle: Zoom");
        ImGui::Separator();
        ImGui::TextDisabled("Selection");
        ImGui::TextDisabled("  Left Click (+Ctrl): Select / multi");
        ImGui::TextDisabled("  Left Drag: Box select");
        ImGui::Separator();
        ImGui::TextDisabled("Hotkeys");
        ImGui::TextDisabled("  Del: Delete   Ctrl+G: Group");
        ImGui::TextDisabled("  Esc: Deselect   F: Fit view");
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

void IgesViewerGUI::RenderLoadPopup() {
    if (request_load_popup_) {
        ImGui::OpenPopup("Load IGES");
        request_load_popup_ = false;
    }
    if (ImGui::BeginPopupModal("Load IGES", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        static char path_buf[512] = "";
        ImGui::InputText("Path", path_buf, IM_ARRAYSIZE(path_buf));
        if (ImGui::Button("Load")) {
            LoadIgesFile(path_buf);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void IgesViewerGUI::RenderOutliner() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(
            ImVec2(kOutlinerWidth, vp->WorkSize.y - kStatusBarHeight),
            ImGuiCond_Always);
    if (ImGui::Begin("Outliner", nullptr, kPanelFlags)) {
        RenderAssemblyNode(scene_->Root());
    }
    ImGui::End();
}

void IgesViewerGUI::RenderInspector() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(
            ImVec2(vp->WorkPos.x + vp->WorkSize.x - kInspectorWidth,
                   vp->WorkPos.y),
            ImGuiCond_Always);
    ImGui::SetNextWindowSize(
            ImVec2(kInspectorWidth, vp->WorkSize.y - kStatusBarHeight),
            ImGuiCond_Always);
    if (ImGui::Begin("Inspector", nullptr, kPanelFlags)) {
        RenderSelectionSection();
        ImGui::Separator();
        RenderPropertiesSection();
    }
    ImGui::End();
}

void IgesViewerGUI::RenderStatusBar() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(
            ImVec2(vp->WorkPos.x,
                   vp->WorkPos.y + vp->WorkSize.y - kStatusBarHeight),
            ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, kStatusBarHeight),
                             ImGuiCond_Always);
    const ImGuiWindowFlags flags = kPanelFlags
            | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar;
    if (ImGui::Begin("##StatusBar", nullptr, flags)) {
        const char* mode = (scene_->Granularity()
                == models::SelectionGranularity::kAssembly)
                ? "Assembly" : "Body";
        ImGui::Text("entities: %d | selected: %d | mode: %s | %s",
                static_cast<int>(scene_->Root().GetEntityIDs(true).size()),
                static_cast<int>(scene_->ActiveSelection().Size()),
                mode, last_edit_status_.c_str());
    }
    ImGui::End();
}

void IgesViewerGUI::RenderViewportOverlay() {
    if (!is_box_selecting_) return;

    const double dx = last_x_ - press_x_, dy = last_y_ - press_y_;
    if (dx * dx + dy * dy < kClickThresholdPx * kClickThresholdPx) return;

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

void IgesViewerGUI::RenderAssemblyNode(models::Assembly& node) {
    ImGui::PushID(node.GetID().ToInt());

    // 可視チェック (このノードのサブツリー表示をトグル)
    bool visible = node.Metadata().visible;
    if (ImGui::Checkbox("##vis", &visible)) {
        node.Metadata().visible = visible;
        renderer_.MarkSceneDirty();
        needs_redraw_ = true;
    }
    ImGui::SameLine();

    const std::string label = node.Metadata().name.empty()
            ? ("Assembly #" + std::to_string(node.GetID().ToInt()))
            : node.Metadata().name;
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
            | ImGuiTreeNodeFlags_DefaultOpen;
    if (focused_assembly_id_.has_value()
            && *focused_assembly_id_ == node.GetID()) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    const bool open = ImGui::TreeNodeEx(label.c_str(), flags);
    if (ImGui::IsItemClicked()) focused_assembly_id_ = node.GetID();
    RenderNodeContextMenu(node);

    if (open) {
        for (const auto& child : node.GetChildAssemblies()) {
            if (child) RenderAssemblyNode(*child);
        }
        for (const auto& id : node.GetEntityIDs(/*recursive=*/false)) {
            RenderEntityRow(id);
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void IgesViewerGUI::RenderEntityRow(const ObjectID& id) {
    auto& sel = scene_->ActiveSelection();
    const std::string label = "[" + std::to_string(id.ToInt()) + "] "
            + GetEntityTypeString(id);

    ImGui::PushID(id.ToInt());
    if (ImGui::Selectable(label.c_str(), sel.Contains(id))) {
        if (ImGui::GetIO().KeyCtrl) {
            // Ctrl: トグル (解除は常に可)
            if (sel.Contains(id)) {
                sel.Deselect(id);
                selected_hit_positions_.erase(id);
            } else {
                scene_->TrySelectWithLock(sel, id);
            }
        } else {
            sel.Clear();
            selected_hit_positions_.clear();
            scene_->TrySelectWithLock(sel, id);
        }
        needs_redraw_ = true;
    }
    ImGui::PopID();
}

void IgesViewerGUI::RenderNodeContextMenu(models::Assembly& node) {
    if (!ImGui::BeginPopupContextItem()) return;
    focused_assembly_id_ = node.GetID();

    models::Assembly* np = &node;
    if (ImGui::MenuItem("New child")) {
        pending_action_ = [this, np]() {
            auto created = std::make_shared<models::Assembly>();
            created->Metadata().name = "Assembly";
            np->AddChildAssembly(created);
            renderer_.MarkSceneDirty();
            needs_redraw_ = true;
        };
    }
    if (ImGui::MenuItem("Group selection here")) {
        const auto nid = node.GetID();
        pending_action_ = [this, nid]() {
            focused_assembly_id_ = nid;
            GroupSelectionIntoNewAssembly();
        };
    }
    if (ImGui::MenuItem("Clear")) {
        pending_action_ = [this, np]() {
            np->Clear();
            OnModelEdited();
        };
    }
    if (auto parent = node.GetParent().lock()) {
        if (ImGui::MenuItem("Remove")) {
            const auto nid = node.GetID();
            std::shared_ptr<models::Assembly> par = parent;
            pending_action_ = [this, par, nid]() {
                if (par->RemoveChildAssembly(nid, CurrentRemovalPolicy())) {
                    focused_assembly_id_ = std::nullopt;
                    last_edit_status_ = "Removed assembly.";
                    OnModelEdited();
                } else {
                    last_edit_status_ = "Remove rejected (try Cascade).";
                }
            };
        }
    }
    ImGui::EndPopup();
}

void IgesViewerGUI::RenderSelectionSection() {
    const auto& selected = scene_->ActiveSelection().Items();
    ImGui::Text("Selection: %d", static_cast<int>(selected.size()));

    if (ImGui::BeginChild("##sel_list", ImVec2(0, 120), true)) {
        for (const auto& id : selected) {
            const auto type_str = GetEntityTypeString(id);
            auto it = selected_hit_positions_.find(id);
            if (it != selected_hit_positions_.end()) {
                const auto& p = it->second;
                ImGui::Text("[%d] %s  (%.3f, %.3f, %.3f)",
                            id.ToInt(), type_str.c_str(), p.x(), p.y(), p.z());
            } else {
                ImGui::Text("[%d] %s", id.ToInt(), type_str.c_str());
            }
        }
    }
    ImGui::EndChild();

    if (ImGui::Button("Delete selected")) DeleteSelectedEntities();
    ImGui::SameLine();
    if (ImGui::Button("Group")) GroupSelectionIntoNewAssembly();
    ImGui::SameLine();
    if (ImGui::Button("Deselect")) {
        scene_->ActiveSelection().Clear();
        selected_hit_positions_.clear();
        needs_redraw_ = true;
    }
}

void IgesViewerGUI::RenderPropertiesSection() {
    ImGui::Text("Properties");
    models::Assembly* node = FocusedNode();
    if (node != nullptr) {
        RenderAssemblyProperties(*node);
        return;
    }
    // ノード未フォーカス時は主選択エンティティの情報を表示する
    const auto active = scene_->ActiveSelection().Active();
    if (!active.has_value()) {
        ImGui::TextDisabled("(select an entity or assembly node)");
        return;
    }
    const ObjectID id = *active;
    ImGui::Text("Entity [%d]", id.ToInt());
    std::string type_str = GetEntityTypeString(id);
    ImGui::Text("Type: %s", type_str.c_str());
    if (const auto* owner = scene_->Root().FindOwner(id)) {
        ImGui::Text("Owner: Assembly #%d", owner->GetID().ToInt());
    }
    auto it = selected_hit_positions_.find(id);
    if (it != selected_hit_positions_.end()) {
        const auto& p = it->second;
        ImGui::Text("Hit: (%.3f, %.3f, %.3f)", p.x(), p.y(), p.z());
    }
}

void IgesViewerGUI::RenderAssemblyProperties(models::Assembly& node) {
    ImGui::Text("Assembly #%d", node.GetID().ToInt());

    // 名前
    char name_buf[128] = {};
    const std::string& nm = node.Metadata().name;
    nm.copy(name_buf, std::min(nm.size(), sizeof(name_buf) - 1));
    if (ImGui::InputText("Name", name_buf, sizeof(name_buf))) {
        node.Metadata().name = name_buf;
    }

    // 可視・抑制
    bool node_visible = node.Metadata().visible;
    if (ImGui::Checkbox("Visible", &node_visible)) {
        node.SetVisibleRecursive(node_visible);
        renderer_.MarkSceneDirty();
        needs_redraw_ = true;
    }
    bool node_suppressed = node.Metadata().suppressed;
    if (ImGui::Checkbox("Suppressed", &node_suppressed)) {
        node.SetSuppressedRecursive(node_suppressed);
        renderer_.MarkSceneDirty();
        needs_redraw_ = true;
    }

    // 色オーバーライド
    bool has_color = node.Metadata().color_override.has_value();
    if (ImGui::Checkbox("Color override", &has_color)) {
        node.Metadata().color_override = has_color
                ? std::optional<std::array<float, 3>>(
                        std::array<float, 3>{0.8f, 0.8f, 0.8f})
                : std::nullopt;
        renderer_.MarkSceneDirty();
        needs_redraw_ = true;
    }
    if (node.Metadata().color_override.has_value()) {
        if (ImGui::ColorEdit3("##color",
                              node.Metadata().color_override->data())) {
            renderer_.MarkSceneDirty();
            needs_redraw_ = true;
        }
    }

    // 不透明度オーバーライド
    bool has_opacity = node.Metadata().opacity_override.has_value();
    if (ImGui::Checkbox("Opacity override", &has_opacity)) {
        node.Metadata().opacity_override =
                has_opacity ? std::optional<float>(1.0f) : std::nullopt;
        renderer_.MarkSceneDirty();
        needs_redraw_ = true;
    }
    if (node.Metadata().opacity_override.has_value()) {
        float opacity = *node.Metadata().opacity_override;
        if (ImGui::SliderFloat("##opacity", &opacity, 0.0f, 1.0f)) {
            node.Metadata().opacity_override = opacity;
            renderer_.MarkSceneDirty();
            needs_redraw_ = true;
        }
    }

    // ロック
    ImGui::Checkbox("Selectable", &node.Metadata().lock.selectable);
    ImGui::SameLine();
    ImGui::Checkbox("Editable", &node.Metadata().lock.editable);

    // 変換ナッジ (並進; スケールなし前提)
    static float translate[3] = {0.0f, 0.0f, 0.0f};
    ImGui::InputFloat3("Translate", translate);
    ImGui::SameLine();
    if (ImGui::Button("Apply")) {
        igesio::Matrix4d m = igesio::Matrix4d::Identity();
        m(0, 3) = translate[0];
        m(1, 3) = translate[1];
        m(2, 3) = translate[2];
        node.ComposeGlobalTransform(m);
        renderer_.MarkSceneDirty();
        needs_redraw_ = true;
    }

    ImGui::Separator();

    // 構造操作 (いずれもツリー整合のため遅延実行する)
    models::Assembly* np = &node;
    if (ImGui::Button("New child")) {
        pending_action_ = [this, np]() {
            auto created = std::make_shared<models::Assembly>();
            created->Metadata().name = "Assembly";
            np->AddChildAssembly(created);
            renderer_.MarkSceneDirty();
            needs_redraw_ = true;
        };
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        pending_action_ = [this, np]() {
            np->Clear();
            OnModelEdited();
        };
    }
    if (auto parent = node.GetParent().lock()) {
        ImGui::SameLine();
        if (ImGui::Button("Remove")) {
            const auto nid = node.GetID();
            std::shared_ptr<models::Assembly> par = parent;
            pending_action_ = [this, par, nid]() {
                if (par->RemoveChildAssembly(nid, CurrentRemovalPolicy())) {
                    focused_assembly_id_ = std::nullopt;
                    last_edit_status_ = "Removed assembly.";
                    OnModelEdited();
                } else {
                    last_edit_status_ = "Remove rejected (try Cascade).";
                }
            };
        }
    }
}



/**
 * 編集操作
 */

igesio::models::Assembly* IgesViewerGUI::FocusedNode() {
    if (!focused_assembly_id_.has_value()) return nullptr;
    return FindAssemblyById(scene_->Root(), *focused_assembly_id_);
}

void IgesViewerGUI::DeleteSelectedEntities() {
    auto& root = scene_->Root();
    auto& sel = scene_->ActiveSelection();
    const std::vector<ObjectID> ids(sel.Items().begin(), sel.Items().end());
    const models::RemovalPolicy policy = CurrentRemovalPolicy();

    int removed = 0, failed = 0;
    for (const auto& id : ids) {
        if (root.RemoveEntity(id, policy)) {
            ++removed;
        } else {
            ++failed;
        }
    }
    last_edit_status_ = "Deleted " + std::to_string(removed)
            + (failed > 0 ? (", " + std::to_string(failed)
                             + " rejected (referenced/locked).") : ".");
    if (removed > 0) OnModelEdited();
}

void IgesViewerGUI::GroupSelectionIntoNewAssembly() {
    auto& root = scene_->Root();
    auto& sel = scene_->ActiveSelection();
    if (sel.Empty()) return;

    // フォーカス中ノードがあればその直下、無ければルート直下へまとめる
    models::Assembly* parent = FocusedNode();
    if (parent == nullptr) parent = &root;

    auto group = std::make_shared<models::Assembly>();
    group->Metadata().name = "Group";
    parent->AddChildAssembly(group);

    const std::vector<ObjectID> ids(sel.Items().begin(), sel.Items().end());
    for (const auto& id : ids) {
        if (root.FindOwner(id) != nullptr) root.MoveEntityTo(id, *group);
    }
    focused_assembly_id_ = group->GetID();
    last_edit_status_ = "Grouped " + std::to_string(ids.size()) + " entities.";
    OnModelEdited();
}



/**
 * コールバック・入力
 */

void IgesViewerGUI::SetupCallbacks() {
    glfwSetMouseButtonCallback(window_, [](GLFWwindow* window, int button,
                                           int action, int mods) {
        auto* v = static_cast<IgesViewerGUI*>(glfwGetWindowUserPointer(window));
        v->MouseButtonCallback(button, action, mods);
    });
    glfwSetCursorPosCallback(window_, [](GLFWwindow* window,
                                         double xpos, double ypos) {
        auto* v = static_cast<IgesViewerGUI*>(glfwGetWindowUserPointer(window));
        v->CursorPositionCallback(xpos, ypos);
    });
    glfwSetScrollCallback(window_, [](GLFWwindow* window,
                                      double x_offset, double y_offset) {
        auto* v = static_cast<IgesViewerGUI*>(glfwGetWindowUserPointer(window));
        v->ScrollCallback(x_offset, y_offset);
    });
    glfwSetFramebufferSizeCallback(window_, [](GLFWwindow* window,
                                               int width, int height) {
        auto* v = static_cast<IgesViewerGUI*>(glfwGetWindowUserPointer(window));
        v->FramebufferSizeCallback(width, height);
    });
    // キー入力は再描画を促し、ホットキーをフレーム内で評価できるようにする
    // (ImGui_ImplGlfw_InitForOpenGLが本コールバックへチェーンする)
    glfwSetKeyCallback(window_, [](GLFWwindow* window, int key, int scancode,
                                   int action, int mods) {
        auto* v = static_cast<IgesViewerGUI*>(glfwGetWindowUserPointer(window));
        v->KeyCallback(key, action, mods);
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

        if (Matches(input_config_.rotate, btn, mod)) {
            drag_mode_ = DragMode::kRotate;
        } else if (Matches(input_config_.pan, btn, mod)) {
            drag_mode_ = DragMode::kPan;
        } else if (Matches(input_config_.zoom_drag, btn, mod)) {
            drag_mode_ = DragMode::kZoom;
        } else {
            drag_mode_ = DragMode::kNone;
        }

        is_box_selecting_ = (btn == input_config_.select.button);
    } else if (action == GLFW_RELEASE) {
        drag_mode_ = DragMode::kNone;

        if (btn != input_config_.select.button) return;
        is_box_selecting_ = false;

        double x, y;
        glfwGetCursorPos(window_, &x, &y);
        const double dx = x - press_x_, dy = y - press_y_;
        const bool is_click =
                dx * dx + dy * dy < kClickThresholdPx * kClickThresholdPx;

        if (is_click) {
            // ウィンドウ座標 -> フレームバッファ座標へ換算 (HiDPI対応)
            int win_w = 0, win_h = 0;
            glfwGetWindowSize(window_, &win_w, &win_h);
            const auto [fb_w, fb_h] = renderer_.GetDisplaySize();
            const double sx = (win_w > 0)
                    ? static_cast<double>(fb_w) / win_w : 1.0;
            const double sy = (win_h > 0)
                    ? static_cast<double>(fb_h) / win_h : 1.0;
            HandleClickSelection(x * sx, y * sy, mods);
        } else {
            HandleBoxSelection(x, y, mods);
        }
        needs_redraw_ = true;
    }
}

void IgesViewerGUI::HandleClickSelection(
        const double x, const double y, const int mods) {
    const Ray ray = renderer_.GetRayFromScreen(x, y);
    const auto hits = renderer_.PickEntities(ray, x, y);
    const ModifierKey mod = ToModifierKey(mods);
    const ModifierKey multi_mod = input_config_.multi_select_mod;
    const bool ctrl = multi_mod != ModifierKey::kNone
            && (mod & multi_mod) == multi_mod;

    auto& selection = scene_->ActiveSelection();
    if (hits.empty()) {
        if (!ctrl) {
            selection.Clear();
            selected_hit_positions_.clear();
        }
        return;
    }

    const ObjectID id = hits.front().id;
    const Vector3d pos = hits.front().hit.position;

    // Assembly一括選択モード: 所有Assemblyのメンバをまとめて選択する
    if (scene_->Granularity() == models::SelectionGranularity::kAssembly) {
        if (ctrl) {
            // グループ単位のトグル
            if (selection.Contains(id)) {
                scene_->DeselectOwningAssembly(selection, id);
                for (auto it = selected_hit_positions_.begin();
                     it != selected_hit_positions_.end();) {
                    if (selection.Contains(it->first)) {
                        ++it;
                    } else {
                        it = selected_hit_positions_.erase(it);
                    }
                }
            } else if (scene_->SelectOwningAssembly(selection, id)) {
                selected_hit_positions_[id] = pos;
            }
        } else {
            selection.Clear();
            selected_hit_positions_.clear();
            if (scene_->SelectOwningAssembly(selection, id)) {
                selected_hit_positions_[id] = pos;
            }
        }
        return;
    }

    if (ctrl) {
        if (selection.Contains(id)) {
            selection.Deselect(id);
            selected_hit_positions_.erase(id);
        } else if (scene_->TrySelectWithLock(selection, id)) {
            selected_hit_positions_[id] = pos;
        }
    } else {
        selection.Clear();
        selected_hit_positions_.clear();
        if (scene_->TrySelectWithLock(selection, id)) {
            selected_hit_positions_[id] = pos;
        }
    }
}

void IgesViewerGUI::HandleBoxSelection(
        const double x, const double y, const int mods) {
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

    const ModifierKey mod = ToModifierKey(mods);
    const ModifierKey multi_mod = input_config_.multi_select_mod;
    const bool additive = multi_mod != ModifierKey::kNone
            && (mod & multi_mod) == multi_mod;

    auto& selection = scene_->ActiveSelection();
    if (!additive) {
        selection.Clear();
        selected_hit_positions_.clear();
    }
    for (const auto& id : ids) {
        scene_->TrySelectWithLock(selection, id);
    }
}

void IgesViewerGUI::HandleHotkeys() {
    ImGuiIO& io = ImGui::GetIO();
    // テキスト入力中などキーボードをImGuiが使用中は無視する
    if (io.WantCaptureKeyboard) return;

    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        DeleteSelectedEntities();
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_G)) {
        GroupSelectionIntoNewAssembly();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        scene_->ActiveSelection().Clear();
        selected_hit_positions_.clear();
        needs_redraw_ = true;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F)) {
        renderer_.FitView();
        needs_redraw_ = true;
    }
}

void IgesViewerGUI::ApplyStandardView(const StandardView view) {
    renderer_.Camera().SetStandardView(view);
    renderer_.FitView();
    needs_redraw_ = true;
}

void IgesViewerGUI::CursorPositionCallback(
        const double xpos, const double ypos) {
    const auto dx = static_cast<float>(xpos - last_x_);
    const auto dy = static_cast<float>(ypos - last_y_);

    if (is_box_selecting_) needs_redraw_ = true;

    switch (drag_mode_) {
        case DragMode::kRotate: {
            constexpr float kSensitivity = 0.006f;
            renderer_.Camera().Rotate(-dx * kSensitivity, -dy * kSensitivity);
            needs_redraw_ = true;
            break;
        }
        case DragMode::kPan: {
            constexpr float kSensitivity = 0.001f;
            renderer_.Camera().Pan(dx * kSensitivity, dy * kSensitivity);
            needs_redraw_ = true;
            break;
        }
        case DragMode::kZoom: {
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
    const float sensitivity = 0.1f;
    const float zoom_factor = 1.0f - static_cast<float>(y_offset) * sensitivity;
    renderer_.Camera().Zoom(zoom_factor);
    needs_redraw_ = true;
}

void IgesViewerGUI::FramebufferSizeCallback(const int width, const int height) {
    renderer_.SetDisplaySize(width, height);
    needs_redraw_ = true;
}
