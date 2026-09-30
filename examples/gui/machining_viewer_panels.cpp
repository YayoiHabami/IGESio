/**
 * @file examples/gui/machining_viewer_panels.cpp
 * @brief 簡易CAM GUIの左右のパネル (Project/Programs/Source、Machine/Tools/
 *        Display/Log) の描画
 * @author Yayoi Habami
 * @date 2026-09-17
 * @copyright 2026 Yayoi Habami
 * @note `MachiningViewerGUI`のメンバ関数のうち、パネルの描画だけを本ファイルに置く.
 *       状態の更新 (読込、動作生成) は`machining_viewer_gui.cpp`の関数を呼ぶ.
 */
#include "./machining_viewer_gui.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <igesio/graphics/core/draw_context.h>

namespace igesio::graphics {

namespace {

namespace m = igesio::extensions::machines;

/// @brief 表の共通フラグ
constexpr ImGuiTableFlags kTableFlags = ImGuiTableFlags_Borders
        | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;

/// @brief 1つのセルに複数の項目を並べるときの区切り (" · ", UTF-8)
constexpr const char* kFieldSeparator = " \xC2\xB7 ";

/// @brief 重要度の表示名
const char* SeverityLabel(const m::Severity severity) {
    return severity == m::Severity::kWarning ? "warning" : "info";
}

/// @brief 可動範囲外の扱いの選択肢 (`OvertravelPolicy`の順)
constexpr const char* kOvertravelItems[] = {"Error", "Warning", "Ignore"};
/// @brief 回転角の解の選択方針の選択肢 (0は機械定義のまま. 以降`BranchPolicy`の順)
constexpr const char* kBranchItems[] = {
        "Machine default", "Positive", "Negative", "Continuous"};

/// @brief 無制限の回転軸のジョグの範囲 [deg]
constexpr float kUnlimitedRotaryRangeDeg = 360.0f;
/// @brief 無制限の直進軸のジョグの範囲 [mm]
constexpr float kUnlimitedLinearRangeMm = 1000.0f;
/// @brief 表示モードの選択肢 (`DisplayMode`の順)
constexpr const char* kDisplayModeItems[] = {"Shaded", "Wireframe", "No edge"};
/// @brief 投影の選択肢 (`ProjectionMode`の順)
constexpr const char* kProjectionItems[] = {"Perspective", "Orthographic"};
/// @brief 間引きの方式の選択肢 (`ThinningMode`の順)
constexpr const char* kThinningItems[] = {"None", "Rate", "Max count"};
/// @brief モデルの役割の表示名 (`ModelRole`の順)
constexpr const char* kModelRoleItems[] = {"stock", "fixture", "design", "display"};

/// @brief 文字列の一覧からコンボを描画する
/// @param label コンボのラベル
/// @param[in,out] index 選択のインデックス
/// @param items 選択肢
/// @return 選択が変わったら`true`
bool StringCombo(const char* label, int& index, const std::vector<std::string>& items) {
    const char* preview = index >= 0 && static_cast<std::size_t>(index) < items.size()
            ? items[static_cast<std::size_t>(index)].c_str() : "";
    bool changed = false;
    if (ImGui::BeginCombo(label, preview)) {
        for (std::size_t i = 0; i < items.size(); ++i) {
            const bool selected = static_cast<int>(i) == index;
            if (ImGui::Selectable(items[i].c_str(), selected)) {
                index = static_cast<int>(i);
                changed = true;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

/// @brief 間引きの設定 (方式と値) を描画する
/// @param id ImGuiのID
/// @param[in,out] thinning 間引きの設定
/// @return 変更があれば`true`
bool ThinningControls(const char* id, m::Thinning& thinning) {
    ImGui::PushID(id);
    bool changed = false;
    int mode = static_cast<int>(thinning.mode);
    ImGui::SetNextItemWidth(100.0f);
    if (ImGui::Combo("##mode", &mode, kThinningItems, IM_ARRAYSIZE(kThinningItems))) {
        thinning.mode = static_cast<m::ThinningMode>(mode);
        changed = true;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    if (thinning.mode == m::ThinningMode::kRate) {
        auto rate = static_cast<float>(thinning.rate);
        if (ImGui::SliderFloat("rate", &rate, 0.01f, 1.0f, "%.2f",
                               ImGuiSliderFlags_AlwaysClamp)) {
            thinning.rate = static_cast<double>(rate);
            changed = true;
        }
    } else if (thinning.mode == m::ThinningMode::kMaxCount) {
        if (ImGui::InputInt("max", &thinning.max_count, 0, 0)) {
            thinning.max_count = std::max(thinning.max_count, 1);
            changed = true;
        }
    } else {
        ImGui::TextDisabled("-");
    }
    ImGui::PopID();
    return changed;
}

/// @brief 部位要素のz範囲 (全セグメントの端点のzの最小と最大) を計算する
/// @param element 部位要素
/// @return {最小, 最大}. セグメントが無ければ{0, 0}
std::array<double, 2> ElementZRange(const m::ToolProfileElement& element) {
    if (element.segments.empty()) return {0.0, 0.0};
    double lo = std::numeric_limits<double>::max();
    double hi = std::numeric_limits<double>::lowest();
    for (const m::ProfileSegment& segment : element.segments) {
        lo = std::min({lo, segment.start.y(), segment.end.y()});
        hi = std::max({hi, segment.start.y(), segment.end.y()});
    }
    return {lo, hi};
}

/// @brief 統計の1行 (名前と値) を表に描画する
/// @param label 名前
/// @param value 値
void StatisticsRow(const char* label, const std::string& value) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(label);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(value.c_str());
}

/// @brief タブを描画し、選択の要求があればそのタブに切り替える
/// @param label タブ名
/// @param selected 選択の要求があるか (真なら今回のフレームで選択する)
/// @return タブが開いていれば`true` (呼び出し側で`EndTabItem`すること)
bool BeginTab(const char* label, const bool selected) {
    const ImGuiTabItemFlags flags =
            selected ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
    return ImGui::BeginTabItem(label, nullptr, flags);
}

/// @brief 読み取り専用の項目を「名前: 値」で描画する
/// @param label 名前
/// @param value 値
void LabeledText(const char* label, const std::string& value) {
    ImGui::TextDisabled("%s", label);
    ImGui::SameLine(110.0f);
    ImGui::TextWrapped("%s", value.empty() ? "-" : value.c_str());
}

}  // namespace



/**
 * 左パネル
 */

void MachiningViewerGUI::RenderLeftPanel(const ImVec2& pos, const ImVec2& size) {
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoBringToFrontOnFocus
            | ImGuiWindowFlags_NoTitleBar;
    if (ImGui::Begin("##LeftPanel", nullptr, flags)) {
        if (ImGui::BeginTabBar("##left_tabs")) {
            const std::optional<LeftTab> requested = requested_left_tab_;
            requested_left_tab_.reset();
            if (BeginTab("Project", requested == LeftTab::kProject)) {
                RenderProjectTab();
                ImGui::EndTabItem();
            }
            if (BeginTab("Programs", requested == LeftTab::kPrograms)) {
                RenderProgramsTab();
                ImGui::EndTabItem();
            }
            if (BeginTab("Source", requested == LeftTab::kSource)) {
                RenderSourceTab();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void MachiningViewerGUI::RenderProjectTab() {
    const MachiningSession& s = session_;

    // プロジェクトファイル
    ImGui::TextDisabled("Project file");
    ImGui::TextWrapped("%s", s.project_path.empty()
                             ? "(none)" : s.project_path.string().c_str());
    if (ImGui::Button("Open...")) {
        OpenModal(ModalKind::kOpenProject, s.project_path.string());
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(s.project_path.empty());
    if (ImGui::Button("Reload")) {
        // 読込中に書き換わるため、パスはコピーして渡す
        LoadProject(std::filesystem::path(s.project_path));
        ImGui::EndDisabled();
        return;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(state_ == SessionState::kEmpty);
    if (ImGui::Button("Clear")) {
        ClearSession();
        ImGui::EndDisabled();
        return;
    }
    ImGui::EndDisabled();

    // ライブラリ検索パス
    ImGui::Separator();
    ImGui::TextDisabled("Library directories");
    std::optional<std::size_t> remove_index;
    for (std::size_t i = 0; i < s.library_dirs.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::SmallButton("x")) remove_index = i;
        ImGui::SameLine();
        ImGui::TextWrapped("%s", s.library_dirs[i].string().c_str());
        ImGui::PopID();
    }
    if (remove_index.has_value()) {
        session_.library_dirs.erase(session_.library_dirs.begin()
                                    + static_cast<std::ptrdiff_t>(*remove_index));
    }
    if (ImGui::SmallButton("Add...")) OpenModal(ModalKind::kAddLibraryDir);

    if (!s.project.has_value()) return;
    const m::ProjectDefinition& project = *s.project;

    // プロジェクト情報
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Project", ImGuiTreeNodeFlags_DefaultOpen)) {
        LabeledText("Name", project.name);
        LabeledText("Description", project.description);
        LabeledText("Author", project.author);
        LabeledText("Modified", project.modified);
        LabeledText("Machine", project.machine.name);
        LabeledText("Units", std::string(m::LengthUnitName(
                                     project.units.length_unit))
                             + " / " + std::string(m::AngleUnitName(
                                     project.units.angle_unit)));
    }

    // ワーク座標系
    if (s.setup.has_value()
        && ImGui::CollapsingHeader("Work offsets", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("##work_offsets", 4, kTableFlags)) {
            ImGui::TableSetupColumn("id");
            ImGui::TableSetupColumn("attach");
            ImGui::TableSetupColumn("form");
            ImGui::TableSetupColumn("description");
            ImGui::TableHeadersRow();
            for (const m::WorkFrame& frame : s.setup->WorkFrames()) {
                const m::WorkOffsetSpec* spec = nullptr;
                for (const m::WorkOffsetSpec& candidate : project.work_offsets) {
                    if (candidate.id == frame.id) spec = &candidate;
                }
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(frame.id.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(spec ? spec->attach.c_str() : "work_mount");
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(frame.registered ? "values" : "geometric");
                ImGui::TableNextColumn();
                ImGui::TextWrapped("%s", spec ? spec->description.c_str() : "");
            }
            ImGui::EndTable();
        }
    }

    // モデル (読込の可否はシーンの形状で確認する)
    if (s.setup.has_value()
        && ImGui::CollapsingHeader("Models", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("##models", 4, kTableFlags)) {
            ImGui::TableSetupColumn("name");
            ImGui::TableSetupColumn("role");
            ImGui::TableSetupColumn("file");
            ImGui::TableSetupColumn("loaded");
            ImGui::TableHeadersRow();
            const std::vector<m::GeometryInstance>& geometries =
                    s.setup->Geometries();
            for (std::size_t i = 0; i < geometries.size(); ++i) {
                const m::GeometryInstance& geometry = geometries[i];
                if (geometry.kind != m::GeometryInstance::Kind::kModel) continue;
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(geometry.owner.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(geometry.role
                        ? std::string(m::ModelRoleName(*geometry.role)).c_str()
                        : "-");
                ImGui::TableNextColumn();
                ImGui::TextWrapped("%s", geometry.geometry.DisplayName().c_str());
                ImGui::TableNextColumn();
                const bool loaded = s.scene.IsBuilt()
                        && s.scene.GeometryAssembly(i) != nullptr;
                ImGui::TextUnformatted(loaded ? "yes"
                                       : (geometry.visible ? "no" : "hidden"));
            }
            ImGui::EndTable();
        }
    }

    // 警告の件数
    ImGui::Separator();
    std::size_t warnings = 0;
    for (const LogEntry& entry : s.log) {
        if (entry.diagnostic.severity == m::Severity::kWarning) ++warnings;
    }
    ImGui::Text("Warnings: %zu", warnings);
    ImGui::SameLine();
    if (ImGui::SmallButton("Show log")) requested_right_tab_ = RightTab::kLog;
}

void MachiningViewerGUI::RenderProgramsTab() {
    if (!session_.project.has_value()) {
        ImGui::TextDisabled("Load a project first");
        return;
    }
    if (ImGui::CollapsingHeader("Programs", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderProgramTable();
    }
    if (ImGui::CollapsingHeader("Run and motion", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderMotionSettings();
    }

    // 動作生成と解除
    ImGui::Separator();
    const bool playing = state_ == SessionState::kPlaying;
    ImGui::BeginDisabled(playing);
    if (ImGui::Button("Generate motion")) {
        GenerateMotion();
        ImGui::EndDisabled();
        return;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!session_.player.IsBound());
    if (ImGui::Button("Release")) ReleaseMotion();
    ImGui::EndDisabled();
    if (settings_dirty_) ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f),
                                            "settings changed; generate again");
    if (!generate_result_.empty()) ImGui::TextWrapped("%s", generate_result_.c_str());

    if (session_.track.has_value()
        && ImGui::CollapsingHeader("Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderMotionStatistics();
    }
    if (!session_.paths.empty()
        && ImGui::CollapsingHeader("Paths", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderPathTable();
    }
}

void MachiningViewerGUI::RenderProgramTable() {
    m::ProjectDefinition& project = *session_.project;
    // 左パネルは狭いため、種類・ファイル・工具/ワーク座標系は名前の下の行へ
    // まとめ、列幅は文字幅から決める
    if (!ImGui::BeginTable("##programs", 3, kTableFlags)) return;
    ImGui::TableSetupColumn("on", ImGuiTableColumnFlags_WidthFixed,
                            ImGui::GetFrameHeight());
    ImGui::TableSetupColumn("program", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("lines", ImGuiTableColumnFlags_WidthFixed,
                            ImGui::CalcTextSize("000000").x);
    ImGui::TableHeadersRow();
    for (std::size_t i = 0; i < project.programs.size(); ++i) {
        m::ProgramSpec& spec = project.programs[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        if (ImGui::Checkbox("##enabled", &spec.enabled)) settings_dirty_ = true;
        ImGui::TableNextColumn();
        const std::string type(m::ProgramTypeName(spec.type));
        const std::string file = spec.file.resolved.filename().string();
        const std::string tool = spec.tool ? std::to_string(*spec.tool) : "-";
        const std::string offset = spec.work_offset ? *spec.work_offset : "-";
        ImGui::BeginGroup();
        ImGui::TextUnformatted(m::DisplayName(spec).c_str());
        ImGui::TextDisabled("%s%s%s", type.c_str(), kFieldSeparator, file.c_str());
        ImGui::TextDisabled("T%s / %s", tool.c_str(), offset.c_str());
        ImGui::EndGroup();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s\ntype: %s\nfile: %s\ntool: %s\nwork offset: %s",
                              m::DisplayName(spec).c_str(), type.c_str(),
                              spec.file.resolved.string().c_str(),
                              tool.c_str(), offset.c_str());
        }
        ImGui::TableNextColumn();
        std::optional<std::size_t> lines;
        for (const SourceFile& file : session_.sources) {
            if (file.program_index == static_cast<int>(i)) lines = file.lines.size();
        }
        if (lines.has_value()) {
            ImGui::Text("%zu", *lines);
        } else {
            ImGui::TextUnformatted("-");
        }
        ImGui::PopID();
    }
    ImGui::EndTable();
}

void MachiningViewerGUI::RenderMotionSettings() {
    const float width = 120.0f;
    ImGui::TextDisabled("Run");
    ImGui::SetNextItemWidth(width);
    if (ImGui::InputInt("Start tool (0: none)", &run_ui_.start_tool)) {
        run_ui_.start_tool = std::max(run_ui_.start_tool, 0);
        settings_dirty_ = true;
    }
    ImGui::SetNextItemWidth(width);
    if (ImGui::InputInt("Stop tool (0: none)", &run_ui_.stop_tool)) {
        run_ui_.stop_tool = std::max(run_ui_.stop_tool, 0);
        settings_dirty_ = true;
    }
    ImGui::SetNextItemWidth(width);
    if (ImGui::Combo("Overtravel", &run_ui_.overtravel_index, kOvertravelItems,
                     IM_ARRAYSIZE(kOvertravelItems))) {
        settings_dirty_ = true;
    }

    ImGui::TextDisabled("Motion");
    MotionOptionsUi& ui = motion_ui_;
    ImGui::SetNextItemWidth(width);
    if (ImGui::InputFloat("fps", &ui.fps, 0.0f, 0.0f, "%.1f")) settings_dirty_ = true;
    ImGui::SetNextItemWidth(width);
    if (ImGui::InputInt("Max samples", &ui.max_samples, 0, 0)) settings_dirty_ = true;
    if (ImGui::Checkbox("Interpolate", &ui.interpolate)) settings_dirty_ = true;
    ImGui::SetNextItemWidth(width);
    if (ImGui::InputFloat("Fallback feed [mm/min]", &ui.fallback_feed_mm_per_min,
                          0.0f, 0.0f, "%.1f")) {
        settings_dirty_ = true;
    }
    ImGui::SetNextItemWidth(width);
    if (ImGui::InputFloat("Arc chord tolerance [mm]", &ui.arc_chord_tolerance,
                          0.0f, 0.0f, "%.3f")) {
        settings_dirty_ = true;
    }
    ImGui::SetNextItemWidth(width);
    if (ImGui::Combo("Branch", &ui.branch_index, kBranchItems,
                     IM_ARRAYSIZE(kBranchItems))) {
        settings_dirty_ = true;
    }
    if (ImGui::Checkbox("Fixed time per point", &ui.fixed_time)) settings_dirty_ = true;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70.0f);
    ImGui::BeginDisabled(!ui.fixed_time);
    if (ImGui::InputFloat("[s]", &ui.fixed_seconds, 0.0f, 0.0f, "%.3f")) {
        settings_dirty_ = true;
    }
    ImGui::EndDisabled();
}

void MachiningViewerGUI::RenderMotionStatistics() {
    const m::MotionStats& stats = session_.track->stats;
    const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
            | ImGuiTableFlags_SizingStretchProp;
    if (!ImGui::BeginTable("##stats", 2, flags)) return;
    StatisticsRow("motion records", std::to_string(stats.motion_record_count));
    StatisticsRow("samples", std::to_string(stats.sample_count));
    StatisticsRow("command points", std::to_string(stats.command_point_count));
    StatisticsRow("fps", m::FormatFixed(stats.fps, 2)
                         + (stats.fps_reduced ? " (reduced)" : ""));
    StatisticsRow("duration [s]", m::FormatFixed(stats.duration_sec, 2));
    StatisticsRow("unreachable", std::to_string(stats.unreachable_count));
    StatisticsRow("overtravel", std::to_string(stats.overtravel_count));
    StatisticsRow("self-checks", std::to_string(stats.check_count));
    StatisticsRow("max position error [mm]",
                  m::FormatFixed(stats.max_position_error, 6));
    StatisticsRow("max angle error [deg]",
                  m::FormatFixed(m::ToDegrees(stats.max_angle_error), 6));
    ImGui::EndTable();
}

void MachiningViewerGUI::RenderPathTable() {
    const std::vector<m::ClPathRange>& paths = session_.paths;
    const bool bound = session_.player.IsBound();
    ImGui::Text("%zu path ranges", paths.size());
    const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
            | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;
    const float height = std::min(ImGui::GetContentRegionAvail().y, 240.0f);
    if (!ImGui::BeginTable("##paths", 4, flags, ImVec2(0.0f, height))) return;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 36.0f);
    ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("kind", ImGuiTableColumnFlags_WidthFixed, 40.0f);
    ImGui::TableSetupColumn("records", ImGuiTableColumnFlags_WidthFixed, 96.0f);
    ImGui::TableHeadersRow();
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(paths.size()));
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            const auto k = static_cast<std::size_t>(row);
            const m::ClPathRange& range = paths[k];
            ImGui::PushID(row);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            // 行全体を選択項目にし、クリックで区間の先頭に移動する
            const bool selected = current_run_ == k;
            if (ImGui::Selectable(std::to_string(k).c_str(), selected,
                                  ImGuiSelectableFlags_SpanAllColumns)
                && bound) {
                SeekToRecord(range.begin);
            }
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(range.name.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(range.rapid ? "rapid" : "cut");
            ImGui::TableNextColumn();
            ImGui::Text("%zu - %zu", range.begin,
                        range.end > range.begin ? range.end - 1 : range.begin);
            ImGui::PopID();
        }
    }
    ImGui::EndTable();
}

void MachiningViewerGUI::RenderSourceTab() {
    if (session_.sources.empty()) {
        ImGui::TextDisabled("No program loaded");
        return;
    }
    ImGui::Checkbox("Follow", &follow_source_);

    // 現在レコードのプログラムと行 (バインド中のみ)
    const std::optional<std::int64_t> program =
            CurrentEventValue(m::kProgramEventTrack);
    const std::optional<std::int64_t> record =
            CurrentEventValue(m::kRecordEventTrack);
    int current_line = 0;
    int current_program = -1;
    const m::ClProgram* cl = session_.program ? &*session_.program : nullptr;
    if (cl != nullptr && cl->HasSources() && record && *record >= 0
        && static_cast<std::size_t>(*record) < cl->sources.size()) {
        const m::SourceLocation& location =
                cl->sources[static_cast<std::size_t>(*record)];
        current_line = location.line;
        current_program = location.program_index;
    } else if (program.has_value()) {
        current_program = static_cast<int>(*program);
    }
    const bool program_changed = program != last_program_;
    last_program_ = program;

    if (!ImGui::BeginTabBar("##sources")) return;
    for (const SourceFile& file : session_.sources) {
        const bool is_current = file.program_index == current_program;
        const ImGuiTabItemFlags flags = program_changed && is_current
                ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
        if (!ImGui::BeginTabItem(file.name.c_str(), nullptr, flags)) continue;
        RenderSourceLines(file, is_current ? current_line : 0);
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
}

void MachiningViewerGUI::RenderSourceLines(const SourceFile& file,
                                           const int current_line) {
    const bool bound = session_.player.IsBound();
    if (!ImGui::BeginChild("##lines", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Border)) {
        ImGui::EndChild();
        return;
    }
    const float row_height = ImGui::GetTextLineHeightWithSpacing();
    // 追従: 現在行を表示範囲の中央にする
    if (follow_source_ && source_follow_pending_ && current_line > 0) {
        const float visible_rows = ImGui::GetContentRegionAvail().y / row_height;
        ImGui::SetScrollY(std::max(
                (static_cast<float>(current_line - 1) - visible_rows / 2.0f)
                        * row_height, 0.0f));
        source_follow_pending_ = false;
    }
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(file.lines.size()), row_height);
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            const auto index = static_cast<std::size_t>(row);
            const int line = row + 1;
            const std::optional<std::size_t> record = file.line_records[index + 1];
            const std::string number = std::to_string(line);
            const std::string label = std::string(
                    number.size() < 6 ? 6 - number.size() : 0, ' ')
                    + number + "  " + file.lines[index];
            ImGui::PushID(row);
            if (record.has_value()) {
                if (ImGui::Selectable(label.c_str(), line == current_line) && bound) {
                    SeekToRecord(*record);
                }
            } else {
                ImGui::TextDisabled("%s", label.c_str());
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
}



/**
 * 右パネル
 */

void MachiningViewerGUI::RenderRightPanel(const ImVec2& pos, const ImVec2& size) {
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoBringToFrontOnFocus
            | ImGuiWindowFlags_NoTitleBar;
    if (ImGui::Begin("##RightPanel", nullptr, flags)) {
        if (ImGui::BeginTabBar("##right_tabs")) {
            const std::optional<RightTab> requested = requested_right_tab_;
            requested_right_tab_.reset();
            if (BeginTab("Machine", requested == RightTab::kMachine)) {
                RenderMachineTab();
                ImGui::EndTabItem();
            }
            if (BeginTab("Tools", requested == RightTab::kTools)) {
                RenderToolsTab();
                ImGui::EndTabItem();
            }
            if (BeginTab("Display", requested == RightTab::kDisplay)) {
                RenderDisplayTab();
                ImGui::EndTabItem();
            }
            const std::string log_label =
                    "Log (" + std::to_string(session_.log.size()) + ")###Log";
            if (BeginTab(log_label.c_str(), requested == RightTab::kLog)) {
                RenderLogTab();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void MachiningViewerGUI::RenderMachineTab() {
    if (!session_.scene.IsBuilt()) {
        ImGui::TextDisabled("Load a project first");
        return;
    }
    if (ImGui::CollapsingHeader("Kinematic tree", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderKinematicTree();
    }
    if (ImGui::CollapsingHeader("Jog", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderJog();
    }
    if (ImGui::CollapsingHeader("Target", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderTarget();
    }
}

void MachiningViewerGUI::RenderKinematicTree() {
    const m::MachineModel& model = session_.scene.Model();
    for (std::size_t i = 0; i < model.ComponentCount(); ++i) {
        if (!model.Component(i).parent.has_value()) RenderKinematicNode(i);
    }
}

void MachiningViewerGUI::RenderKinematicNode(const std::size_t index) {
    const m::MachineModel& model = session_.scene.Model();
    const m::ComponentInfo& component = model.Component(index);
    std::string label = component.name + " ["
            + std::string(m::ComponentTypeName(component.type)) + "]";
    if (component.axis.has_value()) {
        const m::AxisInfo& axis = model.Axes()[*component.axis];
        label += " axis " + axis.register_name
                + (axis.kind == m::AxisKind::kRotary ? " (rotary)" : " (linear)");
    }
    std::size_t shapes = 0;
    if (session_.setup.has_value()) {
        for (const m::GeometryInstance& geometry : session_.setup->Geometries()) {
            if (geometry.kind == m::GeometryInstance::Kind::kMachinePart
                && geometry.carrier == index) {
                ++shapes;
            }
        }
    }
    if (shapes > 0) label += ", shapes: " + std::to_string(shapes);

    const bool leaf = component.children.empty();
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
            | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth
            | ImGuiTreeNodeFlags_DefaultOpen;
    if (leaf) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (selected_component_ == index) flags |= ImGuiTreeNodeFlags_Selected;
    const bool open = ImGui::TreeNodeEx(component.name.c_str(), flags, "%s",
                                        label.c_str());
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        SelectComponent(index);
    }
    if (leaf || !open) return;
    for (const std::size_t child : component.children) RenderKinematicNode(child);
    ImGui::TreePop();
}

void MachiningViewerGUI::RenderJog() {
    const m::MachineModel& model = session_.scene.Model();
    const bool editable = state_ == SessionState::kReady;
    const m::JointVector q = DisplayedPose();
    if (q.Size() != model.Axes().size()) return;
    const m::NcValues nc = m::NcFromJoints(model, q);
    if (!editable) ImGui::TextDisabled("(release the animation to jog)");

    ImGui::BeginDisabled(!editable);
    m::NcValues edited;
    for (std::size_t i = 0; i < model.Axes().size(); ++i) {
        const m::AxisInfo& axis = model.Axes()[i];
        const bool rotary = axis.kind == m::AxisKind::kRotary;
        const double raw = nc.GetOr(axis.register_name, 0.0);
        auto value = static_cast<float>(rotary ? m::ToDegrees(raw) : raw);
        float lower = rotary ? -kUnlimitedRotaryRangeDeg : -kUnlimitedLinearRangeMm;
        float upper = -lower;
        if (axis.limits.has_value()) {
            lower = static_cast<float>(rotary ? m::ToDegrees((*axis.limits)[0])
                                              : (*axis.limits)[0]);
            upper = static_cast<float>(rotary ? m::ToDegrees((*axis.limits)[1])
                                              : (*axis.limits)[1]);
        }
        const char* format = rotary ? "%.2f" : "%.3f";
        ImGui::PushID(static_cast<int>(i));
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%s", axis.register_name.c_str());
        ImGui::SameLine(36.0f);
        ImGui::SetNextItemWidth(140.0f);
        bool changed = ImGui::SliderFloat("##slider", &value, lower, upper, format,
                                          ImGuiSliderFlags_AlwaysClamp);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(72.0f);
        changed = ImGui::InputFloat("##input", &value, 0.0f, 0.0f, format,
                                    ImGuiInputTextFlags_EnterReturnsTrue) || changed;
        ImGui::SameLine();
        ImGui::Text(rotary ? "q %.2f deg" : "q %.3f mm",
                    rotary ? m::ToDegrees(q[i]) : q[i]);
        if (changed) {
            edited.Set(axis.register_name,
                       rotary ? m::ToRadians(static_cast<double>(value))
                              : static_cast<double>(value));
        }
        ImGui::PopID();
    }
    if (!edited.Empty() && editable) {
        try {
            SetJogPose(m::JointsFromNc(model, edited, jog_q_));
        } catch (const std::exception& e) {
            SetStatus(e.what());
        }
    }
    if (ImGui::Button("Zero pose")) {
        SetJogPose(m::JointVector(model.Axes().size(), 0.0));
    }
    ImGui::SameLine();
    if (ImGui::Button("Default pose")) SetJogPose(m::InitialJoints(model));
    ImGui::EndDisabled();
}

void MachiningViewerGUI::RenderTarget() {
    const m::MachineScene& scene = session_.scene;
    const bool editable = state_ == SessionState::kReady;
    ImGui::BeginDisabled(!editable);
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputFloat3("Point [mm]", target_ui_.point, "%.3f");
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputFloat3("Tool axis", target_ui_.tool_axis, "%.3f");
    std::vector<std::string> offsets;
    for (const m::WorkFrame& frame : scene.WorkFrames()) offsets.push_back(frame.id);
    ImGui::SetNextItemWidth(120.0f);
    StringCombo("Work offset", target_ui_.work_offset_index, offsets);
    std::vector<std::string> tools = {"none"};
    for (const auto& [number, spec] : scene.Tools()) {
        tools.push_back("T" + std::to_string(number) + " " + spec.name);
    }
    ImGui::SetNextItemWidth(160.0f);
    StringCombo("Tool", target_ui_.tool_index, tools);
    if (ImGui::Button("Solve")) SolveTarget();
    ImGui::EndDisabled();
    if (!target_ui_.result.empty()) ImGui::TextWrapped("%s", target_ui_.result.c_str());
}

void MachiningViewerGUI::RenderToolsTab() {
    m::MachineScene& scene = session_.scene;
    if (!scene.IsBuilt()) {
        ImGui::TextDisabled("Load a project first");
        return;
    }
    const std::map<int, m::ToolAssemblySpec>& tools = scene.Tools();
    if (tools.empty()) {
        ImGui::TextDisabled("No tools");
        return;
    }
    const bool editable = state_ == SessionState::kReady;
    const int active = scene.ActiveTool();
    ImGui::BeginDisabled(!editable);
    if (ImGui::RadioButton("None (hide all tools)", active == m::kNoTool)) {
        scene.SetActiveTool(m::kNoTool);
    }
    ImGui::EndDisabled();

    // 左パネルは狭いため、工具の寸法は名前の下の行にまとめる
    if (ImGui::BeginTable("##tools", 3, kTableFlags)) {
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed,
                                ImGui::GetFrameHeight());
        ImGui::TableSetupColumn("no", ImGuiTableColumnFlags_WidthFixed,
                                ImGui::CalcTextSize("000").x);
        ImGui::TableSetupColumn("tool", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        for (const auto& [number, spec] : tools) {
            ImGui::PushID(number);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(!editable);
            if (ImGui::RadioButton("##active", active == number)) {
                scene.SetActiveTool(number);
            }
            ImGui::EndDisabled();
            ImGui::TableNextColumn();
            ImGui::Text("%d", number);
            ImGui::TableNextColumn();
            const std::string control(m::ControlPointName(spec.control_point));
            const double gauge = spec.profile.GaugeLength(nullptr);
            const double radius = spec.profile.MaxRadius();
            const double cutting = spec.profile.CuttingLength();
            const double reach = spec.profile.Reach();
            ImGui::BeginGroup();
            ImGui::TextUnformatted(spec.name.c_str());
            ImGui::TextDisabled("ctrl %s%sgauge %.2f", control.c_str(),
                                kFieldSeparator, gauge);
            ImGui::TextDisabled("R %.2f%scut %.2f%sreach %.2f", radius,
                                kFieldSeparator, cutting, kFieldSeparator, reach);
            ImGui::EndGroup();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                        "%s\ncontrol point: %s\ngauge length: %.2f\n"
                        "max radius: %.2f\ncutting length: %.2f\nreach: %.2f",
                        spec.name.c_str(), control.c_str(), gauge, radius,
                        cutting, reach);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    // 詳細は表示中の工具 (無ければ先頭)
    const auto shown = tools.find(active);
    const m::ToolAssemblySpec& spec =
            shown != tools.end() ? shown->second : tools.begin()->second;
    if (ImGui::CollapsingHeader("Details", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderToolDetails(spec);
    }
}

void MachiningViewerGUI::RenderToolDetails(const m::ToolAssemblySpec& spec) {
    ImGui::Text("T%d %s: elements %zu, command point z %.3f", spec.number,
                spec.name.c_str(), spec.profile.elements.size(),
                spec.profile.command_point_z);
    const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
            | ImGuiTableFlags_SizingFixedFit;
    if (!ImGui::BeginTable("##elements", 5, flags)) return;
    ImGui::TableSetupColumn("part");
    ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("segs");
    ImGui::TableSetupColumn("z range [mm]");
    ImGui::TableSetupColumn("color");
    ImGui::TableHeadersRow();
    int row = 0;
    for (const m::ToolProfileElement& element : spec.profile.elements) {
        ImGui::PushID(row++);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(std::string(m::ToolPartName(element.part)).c_str());
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(element.name.c_str());
        ImGui::TableNextColumn();
        ImGui::Text("%zu", element.segments.size());
        ImGui::TableNextColumn();
        const std::array<double, 2> range = ElementZRange(element);
        ImGui::Text("%.2f - %.2f", range[0], range[1]);
        ImGui::TableNextColumn();
        const Color color = element.color.value_or(m::DefaultPartColor(element.part));
        ImGui::ColorButton("##color",
                           ImVec4(static_cast<float>(color.r),
                                  static_cast<float>(color.g),
                                  static_cast<float>(color.b), 1.0f),
                           ImGuiColorEditFlags_NoTooltip, ImVec2(18.0f, 18.0f));
        ImGui::PopID();
    }
    ImGui::EndTable();
}

void MachiningViewerGUI::RenderDisplayTab() {
    if (!session_.scene.IsBuilt()) {
        ImGui::TextDisabled("Load a project first");
        return;
    }
    if (ImGui::CollapsingHeader("Shared", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderSharedDisplayFlags();
    }
    if (ImGui::CollapsingHeader("Views", ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderViewSettings();
    }
    if (ImGui::CollapsingHeader("Tool trajectory (work view)",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        RenderTrajectorySettings();
    }
}

void MachiningViewerGUI::RenderSharedDisplayFlags() {
    m::MachineScene& scene = session_.scene;
    DisplayFlags& f = display_flags_;
    if (ImGui::Checkbox("Paths", &f.paths)) scene.SetPathsVisible(f.paths);
    ImGui::SameLine();
    if (ImGui::Checkbox("Rapid paths", &f.rapid_paths)) {
        scene.SetRapidPathsVisible(f.rapid_paths);
    }
    if (ImGui::Checkbox("Motion trace (machine)", &f.trace_machine)) {
        if (const auto trace = scene.MachineTraceAssembly()) {
            trace->SetVisible(f.trace_machine);
        }
    }
    if (ImGui::Checkbox("Motion trace (work)", &f.trace_work)) {
        if (const auto trace = scene.WorkTraceAssembly()) {
            trace->SetVisible(f.trace_work);
        }
    }
    if (ImGui::Checkbox("Current record", &f.current_record)) {
        SetCurrentRecordVisible(f.current_record);
    }
    if (ImGui::Checkbox("Work frames", &f.work_frames)) {
        scene.SetWorkFramesVisible(f.work_frames);
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Triads", &f.triads)) scene.SetTriadsVisible(f.triads);
    if (ImGui::Checkbox("Tool axis and control point", &f.tool_axis)) {
        scene.SetToolAxisVisible(f.tool_axis);
    }
    if (ImGui::Checkbox("Holder", &f.holder)) scene.SetHolderVisible(f.holder);
    ImGui::SameLine();
    if (ImGui::Checkbox("Machine parts", &f.machine_parts)) {
        scene.SetMachinePartsVisible(f.machine_parts);
    }
    ImGui::TextDisabled("Models");
    for (std::size_t i = 0; i < f.model_roles.size(); ++i) {
        if (i > 0) ImGui::SameLine();
        if (ImGui::Checkbox(kModelRoleItems[i], &f.model_roles[i])) {
            scene.SetModelRoleVisible(static_cast<m::ModelRole>(i), f.model_roles[i]);
        }
    }
}

void MachiningViewerGUI::RenderViewSettings() {
    const ImGuiTableFlags flags = ImGuiTableFlags_Borders
            | ImGuiTableFlags_SizingStretchSame;
    if (!ImGui::BeginTable("##views", 2, flags)) return;
    ImGui::TableSetupColumn("Machine");
    ImGui::TableSetupColumn("Work");
    ImGui::TableHeadersRow();
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    RenderViewColumn(*machine_pane_);
    ImGui::TableNextColumn();
    RenderViewColumn(*work_pane_);
    ImGui::PushID("work_extra");
    if (ImGui::Checkbox("Show work_mount parts",
                        &work_view_options_.show_work_mount_parts)) {
        ApplyViewFilters();
    }
    ImGui::PopID();
    ImGui::EndTable();
}

void MachiningViewerGUI::RenderViewColumn(ViewPane& pane) {
    ImGui::PushID(pane.Name().c_str());
    EntityRenderer& renderer = pane.Renderer();
    const float width = -1.0f;
    int mode = static_cast<int>(renderer.GetDisplayMode());
    ImGui::SetNextItemWidth(width);
    if (ImGui::Combo("##mode", &mode, kDisplayModeItems,
                     IM_ARRAYSIZE(kDisplayModeItems))) {
        renderer.SetDisplayMode(static_cast<DisplayMode>(mode));
    }
    int projection = static_cast<int>(renderer.Camera().GetProjectionMode());
    ImGui::SetNextItemWidth(width);
    if (ImGui::Combo("##projection", &projection, kProjectionItems,
                     IM_ARRAYSIZE(kProjectionItems))) {
        renderer.Camera().SetProjectionMode(static_cast<ProjectionMode>(projection));
    }
    // 背景色 (レンダラはgetterを持たないため、UI値をレンダラに書き込む)
    float* background = pane.BackgroundUi();
    ImGui::SetNextItemWidth(width);
    if (ImGui::ColorEdit3("##background", background, ImGuiColorEditFlags_NoInputs)) {
        renderer.SetBackgroundColor(
                Color{background[0], background[1], background[2], 1.0});
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Background");
    if (msaa_samples_ > 0) {
        bool antialiasing = renderer.IsAntialiasingEnabled();
        if (ImGui::Checkbox("Antialiasing", &antialiasing)) {
            renderer.EnableAntialiasing(antialiasing);
        }
    }
    if (ImGui::Button("Screenshot...")) {
        screenshot_pane_ = &pane;
        OpenModal(ModalKind::kScreenshot, "screenshot " + pane.Name() + ".png");
    }
    ImGui::PopID();
}

void MachiningViewerGUI::RenderTrajectorySettings() {
    const bool available = session_.track.has_value();
    if (!available) ImGui::TextDisabled("(generate motion first)");
    ImGui::BeginDisabled(!available);

    // 生成の設定 (Rebuildで反映)
    ImGui::TextDisabled("Generation");
    m::ToolTrajectoryOptions& options = trajectory_options_;
    if (ImGui::Checkbox("Command points only", &options.command_points_only)) {
        trajectory_dirty_ = true;
    }
    ImGui::TextUnformatted("Sample thinning");
    ImGui::SameLine();
    if (ThinningControls("sample", options.thinning)) trajectory_dirty_ = true;
    if (ImGui::Checkbox("Holder", &options.holder_visible)) {
        m::SetToolTrajectoryHolderVisible(session_.scene, options.holder_visible);
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Opacity", &trajectory_opacity_enabled_)) {
        trajectory_dirty_ = true;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    ImGui::BeginDisabled(!trajectory_opacity_enabled_);
    if (ImGui::SliderFloat("##opacity", &trajectory_opacity_ui_, 0.05f, 1.0f, "%.2f",
                           ImGuiSliderFlags_AlwaysClamp)) {
        trajectory_dirty_ = true;
    }
    ImGui::EndDisabled();
    if (ImGui::Button("Rebuild")) RebuildTrajectory();
    if (trajectory_dirty_) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "settings changed");
    }

    // 可視性の設定 (即時反映)
    ImGui::TextDisabled("Visibility");
    m::TrajectoryVisibility& visibility = trajectory_visibility_;
    if (ImGui::Checkbox("Show trajectory", &visibility.show)) {
        ApplyTrajectoryVisibility();
    }
    ImGui::TextUnformatted("Run thinning");
    ImGui::SameLine();
    if (ThinningControls("run", visibility.thinning)) ApplyTrajectoryVisibility();
    std::vector<std::string> runs = {"All"};
    for (std::size_t k = 0; k < session_.paths.size(); ++k) {
        runs.push_back(std::to_string(k)
                       + (session_.paths[k].rapid ? " rapid " : " cut ")
                       + session_.paths[k].name);
    }
    ImGui::SetNextItemWidth(200.0f);
    if (StringCombo("Only run", trajectory_only_run_ui_, runs)) {
        ApplyTrajectoryVisibility();
    }
    if (ImGui::Checkbox("Always show current run", &trajectory_always_current_)) {
        ApplyTrajectoryVisibility();
    }
    ImGui::EndDisabled();
}

void MachiningViewerGUI::RenderLogTab() {
    ImGui::Checkbox("Show info", &log_show_info_);
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) session_.log.clear();

    // 表示する行のインデックス (フィルタ適用後)
    std::vector<std::size_t> rows;
    rows.reserve(session_.log.size());
    for (std::size_t i = 0; i < session_.log.size(); ++i) {
        const LogEntry& entry = session_.log[i];
        if (!log_show_info_ && entry.diagnostic.severity == m::Severity::kInfo) {
            continue;
        }
        rows.push_back(i);
    }
    ImGui::Text("%zu of %zu entries", rows.size(), session_.log.size());

    // 右パネルは狭いため、段階・文脈・行番号は本文の上の行へまとめる.
    // 列幅をimgui.iniへ保存させないよう、幅の変更は許可しない
    const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
            | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;
    if (!ImGui::BeginTable("##log", 2, flags, ImGui::GetContentRegionAvail())) {
        return;
    }
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("sev", ImGuiTableColumnFlags_WidthFixed,
                            ImGui::CalcTextSize("warning").x);
    ImGui::TableSetupColumn("message", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(rows.size()));
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            const LogEntry& entry = session_.log[rows[static_cast<std::size_t>(row)]];
            const m::Diagnostic& d = entry.diagnostic;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(SeverityLabel(d.severity));
            ImGui::TableNextColumn();
            std::string where = LogStageName(entry.stage);
            if (!d.context.empty()) where += kFieldSeparator + d.context;
            if (d.line > 0) {
                where += kFieldSeparator + std::string("L") + std::to_string(d.line);
            }
            ImGui::BeginGroup();
            ImGui::TextDisabled("%s", where.c_str());
            ImGui::TextWrapped("%s", d.message.c_str());
            ImGui::EndGroup();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s\n%s", where.c_str(), d.message.c_str());
            }
        }
    }
    ImGui::EndTable();
}

}  // namespace igesio::graphics
