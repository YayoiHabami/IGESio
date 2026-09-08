/**
 * @file examples/gui/animation_viewer_gui.cpp
 * @brief キーフレームアニメーション再生用GUI (ビューアの派生) の実装
 * @author Yayoi Habami
 * @date 2026-09-02
 * @copyright 2026 Yayoi Habami
 */
#include "./animation_viewer_gui.h"

#ifdef IGESIO_ANIMATION_EXTENSION_ENABLED

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include <imgui.h>

#include <igesio/numerics/core/matrix.h>
#include <igesio/numerics/geometric/bounding_box.h>
#include <igesio/models/assembly.h>

namespace igesio::graphics {

namespace {

namespace anim = igesio::extensions::animation;

/// @brief 分割数の上限
/// @note キー数の増大と、キー境界毎に生じるモデルリビジョンのバンプを抑える.
///       60fpsでは1秒あたり60分割で十分滑らかなため、実用上の上限として置く
constexpr int kMaxDemoSteps = 200;
/// @brief 片道の所要時間の下限 [s] (0だと同一トラックのキー時刻が重複する)
constexpr float kMinTravelSec = 0.05f;
/// @brief 移動量の基準スケールを取得できない場合の既定値
constexpr double kFallbackScale = 10.0;
/// @brief 軸選択コンボの項目 (ImGuiのゼロ区切り形式)
constexpr const char* kAxisItems = "X\0Y\0Z\0";
/// @brief 回転中心コンボの項目 (ImGuiのゼロ区切り形式)
constexpr const char* kCenterItems =
        "Each assembly BBox\0Whole model BBox\0World origin\0";

/// @brief 1対象分のデモ運動 (進捗s∈[0, 1]で基準姿勢から終端姿勢へ進む剛体運動)
struct DemoMotion {
    /// @brief 移動方向の軸 (0〜2)
    int move_axis = kDemoAxisX;
    /// @brief 総移動量 (符号付き)
    double distance = 0.0;
    /// @brief 回転軸 (0〜2)
    int rotation_axis = kDemoAxisZ;
    /// @brief 総回転角 [rad] (符号付き)
    double angle_rad = 0.0;
    /// @brief 回転の中心点 (ワールド座標)
    igesio::Vector3d center = igesio::Vector3d::Zero();
};

/// @brief モデル全体の広がり (運動量の基準)
struct ModelExtent {
    /// @brief 移動量の基準スケール (ワールドBBoxの最大の有限サイズ)
    double scale = kFallbackScale;
    /// @brief ワールドBBoxの中心点 (取得できない場合は原点)
    igesio::Vector3d center = igesio::Vector3d::Zero();
};

/// @brief 生成されるクリップの規模 (UIのプレビューと構築で共有する)
struct DemoClipPlan {
    /// @brief 対象の子Assembly数
    int target_count = 0;
    /// @brief 1トラックあたりのキー数
    int keys_per_track = 0;
    /// @brief クリップの総時間 [s]
    double duration_sec = 0.0;
};



/**
 * 変換行列の生成
 */

/// @brief 座標軸方向の平行移動行列を作成する
/// @param axis 移動方向の軸 (0〜2)
/// @param distance 移動量 (符号付き)
/// @return 平行移動のみの剛体変換行列
igesio::Matrix4d MakeAxisTranslation(const int axis, const double distance) {
    igesio::Matrix4d m = igesio::Matrix4d::Identity();
    m(axis, 3) = distance;
    return m;
}

/// @brief 座標軸回りの回転行列を作成する
/// @param axis 回転軸 (0〜2)
/// @param angle_rad 回転角 [rad]
/// @return 3x3の回転行列 (右手系)
/// @note 軸をa、巡回順で続く2軸を(b, d)として、(b, d)平面の回転を書き込む
igesio::Matrix3d MakeAxisRotation(const int axis, const double angle_rad) {
    const double c = std::cos(angle_rad), s = std::sin(angle_rad);
    const int b = (axis + 1) % 3, d = (axis + 2) % 3;
    igesio::Matrix3d rot = igesio::Matrix3d::Identity();
    rot(b, b) = c;
    rot(b, d) = -s;
    rot(d, b) = s;
    rot(d, d) = c;
    return rot;
}

/// @brief 指定点を中心とする座標軸回りの回転行列を作成する
/// @param axis 回転軸 (0〜2)
/// @param angle_rad 回転角 [rad]
/// @param center 回転の中心点 (ワールド座標)
/// @return 剛体変換行列 (Trans(center) · Rot · Trans(-center))
igesio::Matrix4d MakeRotationAbout(const int axis, const double angle_rad,
                                   const igesio::Vector3d& center) {
    const igesio::Matrix3d rot = MakeAxisRotation(axis, angle_rad);
    igesio::Matrix4d m = igesio::Matrix4d::Identity();
    m.block<3, 3>(0, 0) = rot;
    m.block<3, 1>(0, 3) = center - rot * center;
    return m;
}

/// @brief デモ運動の進捗sにおける相対変換を求める
/// @param motion 対象の運動
/// @param s 進捗 [0, 1] (0で恒等=基準姿勢、1で終端姿勢)
/// @return 基準姿勢へ後掛けする相対変換
/// @note 並進量・回転角の双方をsに比例させるため、sを等分割すれば
///       等速の剛体運動になる
igesio::Matrix4d DemoPoseAt(const DemoMotion& motion, const double s) {
    return MakeAxisTranslation(motion.move_axis, s * motion.distance) *
           MakeRotationAbout(motion.rotation_axis, s * motion.angle_rad,
                             motion.center);
}



/**
 * Assemblyツリーの探索
 */

/// @brief Assemblyツリーから指定IDのノードを探索する
/// @param node 探索の起点ノード
/// @param id 探索するAssemblyのID
/// @return 見つかったノード. 見つからない場合はnullptr
const models::Assembly* FindAssemblyNode(const models::Assembly& node,
                                         const ObjectID& id) {
    if (node.GetID() == id) return &node;
    for (const auto& child : node.GetChildAssemblies()) {
        if (!child) continue;
        if (const auto* found = FindAssemblyNode(*child, id)) return found;
    }
    return nullptr;
}

/// @brief Assemblyツリーから指定IDのノードの表示名を取得する
/// @param root 探索の起点ノード
/// @param id 探索するAssemblyのID
/// @return Metadata().name (空の場合は"(unnamed)").
///         見つからない場合は"(missing)"
std::string FindAssemblyName(const models::Assembly& root,
                             const ObjectID& id) {
    const auto* node = FindAssemblyNode(root, id);
    if (node == nullptr) return "(missing)";
    const auto& name = node->Metadata().name;
    return name.empty() ? "(unnamed)" : name;
}

/// @brief 選択中のIDから、root直下の祖先AssemblyのIDを求める
/// @param root ルートAssembly
/// @param id 選択中のID (エンティティIDまたはAssembly ID)
/// @return root直下の子AssemblyのID. root直下に祖先が無い場合はstd::nullopt
/// @note エンティティIDは逆引きインデックス (O(1)) で所有ノードへ、
///       Assembly IDはツリー探索でノードへ解決してから親を遡る
std::optional<ObjectID> TopLevelAncestorId(const models::Assembly& root,
                                           const ObjectID& id) {
    const models::Assembly* node = root.FindOwner(id);
    if (node == nullptr) node = FindAssemblyNode(root, id);
    while (node != nullptr) {
        const auto parent = node->GetParent().lock();
        // 親が無い=root自身へ到達した場合 (root直下に祖先が無い)
        if (!parent) return std::nullopt;
        if (parent->GetID() == root.GetID()) return node->GetID();
        node = parent.get();
    }
    return std::nullopt;
}

/// @brief バウンディングボックスの中心点を取得する
/// @param bb 対象のバウンディングボックス
/// @return 中心点 (基準点+各延伸方向×サイズ半分)
igesio::Vector3d BoxCenter(const numerics::BoundingBox& bb) {
    igesio::Vector3d center = bb.GetControl();
    const auto sizes = bb.GetSizes();
    const auto& dirs = bb.GetDirections();
    for (std::size_t i = 0; i < 3; ++i) {
        if (std::isfinite(sizes[i])) center += dirs[i] * (sizes[i] * 0.5);
    }
    return center;
}

/// @brief モデル全体のワールドBBoxから運動の基準量を求める
/// @param root ルートAssembly
/// @return 基準スケールと中心点 (BBoxを取得できない場合は既定値)
ModelExtent GetModelExtent(const models::Assembly& root) {
    ModelExtent extent;
    const auto bb = root.GetWorldBoundingBox();
    if (!bb) return extent;
    extent.center = BoxCenter(*bb);
    double max_size = 0.0;
    for (const auto size : bb->GetSizes()) {
        if (std::isfinite(size)) max_size = std::max(max_size, size);
    }
    if (max_size > 0.0) extent.scale = max_size;
    return extent;
}



/**
 * デモクリップの生成
 */

/// @brief 生成パラメータを有効範囲へ収める
/// @param params 入力パラメータ
/// @return クランプ済みのパラメータ
/// @note ImGuiのスライダーはCtrl+クリックでの直接入力により範囲外の値を
///       許すため、生成側でも必ず通す
DemoClipParams Sanitize(const DemoClipParams& params) {
    DemoClipParams s = params;
    s.span_ratio = std::max(0.0f, s.span_ratio);
    s.move_axis = std::clamp(s.move_axis, 0, 2);
    s.rotation_axis = std::clamp(s.rotation_axis, 0, 2);
    s.rotation_center = std::clamp(s.rotation_center, 0, 2);
    s.steps = std::clamp(s.steps, 1, kMaxDemoSteps);
    s.start_delay_sec = std::max(0.0f, s.start_delay_sec);
    s.travel_sec = std::max(kMinTravelSec, s.travel_sec);
    s.stagger_sec = std::max(0.0f, s.stagger_sec);
    s.hold_sec = std::max(0.0f, s.hold_sec);
    return s;
}

/// @brief 生成されるクリップの規模を算出する
/// @param params 生成パラメータ (サニタイズ済みであること)
/// @param target_count 対象の子Assembly数
/// @return キー数と総時間
DemoClipPlan ComputeDemoPlan(const DemoClipParams& params,
                             const int target_count) {
    DemoClipPlan plan;
    plan.target_count = target_count;
    plan.keys_per_track = params.ping_pong ? params.steps * 2 : params.steps;
    const double legs = params.ping_pong ? 2.0 : 1.0;
    // 最後の対象の開始時刻+片道 (往復) 分の所要時間+最終姿勢の保持時間
    const double stagger = static_cast<double>(params.stagger_sec) *
            static_cast<double>(std::max(0, target_count - 1));
    plan.duration_sec = static_cast<double>(params.start_delay_sec) + stagger +
            static_cast<double>(params.travel_sec) * legs +
            static_cast<double>(params.hold_sec);
    return plan;
}

/// @brief 1対象分のデモ運動を組み立てる
/// @param params 生成パラメータ (サニタイズ済みであること)
/// @param target 対象の子Assembly
/// @param extent モデル全体の広がり
/// @param index 対象の並び番号 (向きの交互反転に使う)
/// @return 対象の運動
DemoMotion MakeDemoMotion(const DemoClipParams& params,
                          const models::Assembly& target,
                          const ModelExtent& extent,
                          const std::size_t index) {
    const double dir =
            (params.alternate_direction && index % 2 == 1) ? -1.0 : 1.0;
    DemoMotion motion;
    motion.move_axis = params.move_axis;
    motion.distance =
            dir * extent.scale * static_cast<double>(params.span_ratio);
    motion.rotation_axis = params.rotation_axis;
    motion.angle_rad =
            dir * static_cast<double>(params.angle_deg) * igesio::kPi / 180.0;
    if (params.rotation_center == kDemoCenterModelBox) {
        motion.center = extent.center;
    } else if (params.rotation_center == kDemoCenterOwnBox) {
        if (const auto bb = target.GetWorldBoundingBox()) {
            motion.center = BoxCenter(*bb);
        }
    }
    return motion;
}

/// @brief 1対象分のキー列をクリップへ追加する
/// @param[out] clip 追加先のクリップ
/// @param target 対象AssemblyのID
/// @param motion 対象の運動
/// @param params 生成パラメータ (サニタイズ済みであること)
/// @param index 対象の並び番号 (開始時刻のずらし量に使う)
/// @throw std::invalid_argument キー時刻・変換がクリップの制約を満たさない場合
/// @note 進捗uを[0, 往復なら2]まで`steps`分割で刻み、各サンプルにキーを打つ.
///       恒等となるu=0は基準姿勢と同じため省く. 往復時はu>1で折り返す
///       三角波としてsを求めるため、最終キー (u=2) で基準姿勢へ戻る
void AppendMotionKeys(anim::AnimationClip* clip, const ObjectID& target,
                      const DemoMotion& motion, const DemoClipParams& params,
                      const std::size_t index) {
    const int legs = params.ping_pong ? 2 : 1;
    const double start = static_cast<double>(params.start_delay_sec) +
            static_cast<double>(params.stagger_sec) *
                    static_cast<double>(index);
    const double travel = static_cast<double>(params.travel_sec);
    for (int j = 1; j <= params.steps * legs; ++j) {
        const double u =
                static_cast<double>(j) / static_cast<double>(params.steps);
        const double s = (u <= 1.0) ? u : 2.0 - u;
        clip->AddKey(target, start + travel * u, DemoPoseAt(motion, s));
    }
}

/// @brief 1対象分の運動の開始時刻を求める
/// @param params 生成パラメータ (サニタイズ済みであること)
/// @param index 対象の並び番号 (開始時刻のずらし量に使う)
/// @return 最初の運動キーの直前となる時刻 [s]
double MotionStartSec(const DemoClipParams& params, const std::size_t index) {
    return static_cast<double>(params.start_delay_sec) +
           static_cast<double>(params.stagger_sec) *
                   static_cast<double>(index);
}

/// @brief 1対象分の可視性キーをクリップへ追加する
/// @param[out] clip 追加先のクリップ
/// @param target 対象AssemblyのID
/// @param params 生成パラメータ (サニタイズ済みであること)
/// @param index 対象の並び番号 (開始時刻のずらし量に使う)
/// @throw std::invalid_argument キー時刻がクリップの制約を満たさない場合
/// @note 片道到着時刻で非表示にし、復路終了 (ping_pong時) または保持終了で
///       再表示する. 再表示時刻が到着時刻と一致する (保持0秒) 場合は再表示
///       キーを省き、Stop/Releaseによる基準可視性への復元に任せる
void AppendVisibilityKeys(anim::AnimationClip* clip, const ObjectID& target,
                          const DemoClipParams& params,
                          const std::size_t index) {
    const double travel = static_cast<double>(params.travel_sec);
    const double arrival = MotionStartSec(params, index) + travel;
    const double reappear = params.ping_pong
            ? arrival + travel
            : arrival + static_cast<double>(params.hold_sec);
    clip->AddVisibilityKey(target, arrival, false);
    if (reappear > arrival) clip->AddVisibilityKey(target, reappear, true);
}

/// @brief "stage"イベントトラックの名前
constexpr const char* kStageTrackName = "stage";
/// @brief "stage"イベントの値 (待機・往路・復路・保持)
enum StageValue : std::int64_t {
    /// @brief 最初のキーまでの待機
    kStageIdle = 0,
    /// @brief 往路 (基準姿勢から終端姿勢へ)
    kStageOutbound = 1,
    /// @brief 復路 (終端姿勢から基準姿勢へ. ping_pong時のみ)
    kStageReturn = 2,
    /// @brief 最終姿勢の保持
    kStageHold = 3,
};

/// @brief "stage"イベントトラックをクリップへ追加する
/// @param[out] clip 追加先のクリップ
/// @param params 生成パラメータ (サニタイズ済みであること)
/// @throw std::invalid_argument キー時刻がクリップの制約を満たさない場合
/// @note 対象毎のずらし量は無視し、先頭対象 (index 0) の時刻を代表として用いる.
///       待機時間が0の場合は待機キーを省く (往路キーと同時刻になるため)
void AppendStageEvents(anim::AnimationClip* clip,
                       const DemoClipParams& params) {
    const double start = MotionStartSec(params, 0);
    const double travel = static_cast<double>(params.travel_sec);
    if (start > 0.0) clip->AddEvent(kStageTrackName, 0.0, kStageIdle);
    clip->AddEvent(kStageTrackName, start, kStageOutbound);
    if (params.ping_pong) {
        clip->AddEvent(kStageTrackName, start + travel, kStageReturn);
        clip->AddEvent(kStageTrackName, start + travel * 2.0, kStageHold);
    } else {
        clip->AddEvent(kStageTrackName, start + travel, kStageHold);
    }
}

/// @brief "stage"イベント値の表示ラベルを取得する
/// @param value イベント値 (無ければ先頭キーより前)
/// @return 表示用の文字列
const char* StageLabel(const std::optional<std::int64_t>& value) {
    if (!value) return "(before first key)";
    switch (*value) {
        case kStageIdle: return "0 (idle)";
        case kStageOutbound: return "1 (outbound)";
        case kStageReturn: return "2 (return)";
        case kStageHold: return "3 (hold)";
        default: return "(unknown)";
    }
}

/// @brief 秒数を小数点以下2桁の文字列にする
/// @param seconds 秒数
/// @return "4.60"形式の文字列
std::string FormatSeconds(const double seconds) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << seconds;
    return oss.str();
}

/// @brief 再生状態の表示ラベルを取得する
/// @param state 再生状態
/// @return 表示用の文字列
const char* StateLabel(const anim::PlaybackState state) {
    switch (state) {
        case anim::PlaybackState::kPlaying: return "Playing";
        case anim::PlaybackState::kPaused: return "Paused";
        case anim::PlaybackState::kFinished: return "Finished";
        default: return "Stopped";
    }
}

}  // namespace



AnimationViewerGUI::AnimationViewerGUI(
        const int width, const int height,
        const int msaa_samples, const std::string& initial_file)
        : AnimationViewerBase(width, height, msaa_samples, initial_file) {}

void AnimationViewerGUI::RenderExtraMenus() {
    // 基底のメニュー (検証ツール等) を維持したまま追加する
    AnimationViewerBase::RenderExtraMenus();
    if (ImGui::BeginMenu("Animation")) {
        ImGui::MenuItem("Animation Panel", nullptr, &anim_window_open_);
        ImGui::EndMenu();
    }
}

void AnimationViewerGUI::RenderExtraWindows() {
    AnimationViewerBase::RenderExtraWindows();
    RenderAnimationWindow();
}

void AnimationViewerGUI::OnFrameUpdate(const double dt_sec) {
    AnimationViewerBase::OnFrameUpdate(dt_sec);
    if (player_.State() == anim::PlaybackState::kPlaying) {
        player_.Advance(dt_sec);
        RequestRedraw();
    }
    // 利用側の手本: 毎フレーム、Advanceの後に時刻の動きを問い合わせる.
    // Seek/Stop等のUI操作による移動もここでまとめて取得される
    if (player_.IsBound()) {
        const auto change = player_.TakeTimeChange();
        if (change.changed) last_time_change_ = change;
    }
    // 再生状態に連動して連続描画を切り替える (停止・終端到達で省電力待機へ復帰)
    SetContinuousRedraw(player_.State() == anim::PlaybackState::kPlaying);
}



/**
 * パネル描画
 */

void AnimationViewerGUI::RenderAnimationWindow() {
    if (!anim_window_open_) return;
    ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Animation", &anim_window_open_)) {
        if (!player_.IsBound()) {
            ImGui::TextWrapped(
                    "Builds a demo clip that moves each loaded child "
                    "assembly with stepwise keyframes.");
            RenderDemoSettings();
            if (ImGui::Button("Build & Bind Demo Clip")) {
                BuildAndBindDemoClip();
            }
        } else {
            RenderPlaybackControls();
            // Releaseされたフレームでは以降を描かない
            if (player_.IsBound()) {
                ImGui::Separator();
                if (ImGui::CollapsingHeader("Demo settings")) {
                    RenderDemoSettings();
                    if (ImGui::Button("Rebuild")) BuildAndBindDemoClip();
                }
                ImGui::Separator();
                RenderTrackList();
            }
        }
        if (!anim_status_.empty()) {
            ImGui::Separator();
            ImGui::TextWrapped("%s", anim_status_.c_str());
        }
    }
    ImGui::End();
}

void AnimationViewerGUI::RenderPlaybackControls() {
    const auto state = player_.State();

    // トランスポート (Play/Pause/Stop/Release)
    if (state == anim::PlaybackState::kPlaying) {
        if (ImGui::Button("Pause")) player_.Pause();
    } else {
        if (ImGui::Button("Play")) {
            player_.Play();
            RequestRedraw();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop")) {
        player_.Stop();
        RequestRedraw();
    }
    ImGui::SameLine();
    if (ImGui::Button("Release")) {
        ReleaseAnimation();
        return;  // Unbind後はこのフレームの残りの操作部を描かない
    }

    // シークバー (再生中もドラッグ可能)
    float time_ui = static_cast<float>(player_.CurrentTime());
    const float duration = static_cast<float>(player_.Duration());
    if (ImGui::SliderFloat("Time [s]", &time_ui, 0.0f, duration, "%.2f")) {
        player_.Seek(static_cast<double>(time_ui));
        RequestRedraw();
    }

    // 再生速度 (0.1〜4.0倍; スライダー範囲により正値が保証される)
    if (ImGui::SliderFloat("Speed", &speed_ui_, 0.1f, 4.0f, "%.2fx")) {
        player_.SetSpeed(static_cast<double>(speed_ui_));
    }

    // ループ再生
    if (ImGui::Checkbox("Loop", &loop_ui_)) {
        player_.SetLoop(loop_ui_);
    }
    ImGui::SameLine();
    ImGui::Text("| %s | %.2f / %.2f s", StateLabel(state),
                player_.CurrentTime(), player_.Duration());

    // イベントトラックの現在値 (利用側の問い合わせ例)
    if (const auto* stage = player_.Clip().FindEventTrack(kStageTrackName)) {
        ImGui::Text("Stage: %s", StageLabel(anim::ActiveEventValue(
                                          *stage, player_.CurrentTime())));
    }
    // 直近の時刻変化 (TakeTimeChangeの結果)
    if (last_time_change_) {
        ImGui::Text("Time change: %.2f -> %.2f s (%s)",
                    last_time_change_->from, last_time_change_->to,
                    last_time_change_->monotone ? "monotone" : "non-monotone");
    } else {
        ImGui::Text("Time change: (none yet)");
    }
}

void AnimationViewerGUI::RenderDemoSettings() {
    auto& p = demo_params_;
    if (ImGui::CollapsingHeader("Motion", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Move span", &p.span_ratio, 0.0f, 2.0f, "%.2f");
        ImGui::Combo("Move axis", &p.move_axis, kAxisItems);
        ImGui::SliderFloat("Rotation [deg]", &p.angle_deg,
                           -360.0f, 360.0f, "%.0f");
        ImGui::Combo("Rotation axis", &p.rotation_axis, kAxisItems);
        ImGui::Combo("Rotation center", &p.rotation_center, kCenterItems);
        ImGui::Checkbox("Alternate direction", &p.alternate_direction);
        ImGui::SameLine();
        ImGui::Checkbox("Ping-pong", &p.ping_pong);
        ImGui::Checkbox("Hide after travel", &p.hide_after_travel);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                    "Adds visibility keys: hidden on arrival, shown again "
                    "when the return leg (Ping-pong) or the hold ends.");
        }
    }
    if (ImGui::CollapsingHeader("Smoothness", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderInt("Steps", &p.steps, 1, kMaxDemoSteps);
        ImGui::TextWrapped(
                "Splits the motion into N stepwise keys. 1 moves in a single "
                "jump; about 60 keys per second of travel already looks "
                "continuous at 60 fps.");
    }
    if (ImGui::CollapsingHeader("Timing", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Start delay [s]", &p.start_delay_sec,
                           0.0f, 5.0f, "%.2f");
        ImGui::SliderFloat("Travel [s]", &p.travel_sec,
                           kMinTravelSec, 10.0f, "%.2f");
        ImGui::SliderFloat("Stagger [s]", &p.stagger_sec, 0.0f, 5.0f, "%.2f");
        ImGui::SliderFloat("Hold [s]", &p.hold_sec, 0.0f, 5.0f, "%.2f");
    }
    if (ImGui::CollapsingHeader("Targets", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Selected assemblies only", &p.selected_only);
    }

    // 生成結果のプレビュー (対象数・キー数・総時間)
    const auto plan = ComputeDemoPlan(
            Sanitize(p), static_cast<int>(CollectTargets().size()));
    ImGui::Text("%d target(s), %d keys/track, %s s", plan.target_count,
                plan.keys_per_track, FormatSeconds(plan.duration_sec).c_str());
}

void AnimationViewerGUI::RenderTrackList() {
    const auto& clip = player_.Clip();
    const auto& root = GetScene().Root();

    // 分割数によりキー数が多くなるため、時刻は範囲のみを示す
    const auto& tracks = clip.Tracks();
    ImGui::Text("Transform tracks: %d", static_cast<int>(tracks.size()));
    for (const auto& track : tracks) {
        const auto name = FindAssemblyName(root, track.target);
        if (track.keys.empty()) {
            ImGui::BulletText("%s: no keys", name.c_str());
            continue;
        }
        ImGui::BulletText("%s: %d keys [%.2f - %.2f s]", name.c_str(),
                          static_cast<int>(track.keys.size()),
                          track.keys.front().time_sec,
                          track.keys.back().time_sec);
    }

    const auto& vtracks = clip.VisibilityTracks();
    ImGui::Text("Visibility tracks: %d", static_cast<int>(vtracks.size()));
    for (const auto& track : vtracks) {
        const auto name = FindAssemblyName(root, track.target);
        if (track.keys.empty()) {
            ImGui::BulletText("%s: no keys", name.c_str());
            continue;
        }
        ImGui::BulletText("%s: %d keys [%.2f - %.2f s]", name.c_str(),
                          static_cast<int>(track.keys.size()),
                          track.keys.front().time_sec,
                          track.keys.back().time_sec);
    }

    const auto& etracks = clip.EventTracks();
    ImGui::Text("Event tracks: %d", static_cast<int>(etracks.size()));
    for (const auto& track : etracks) {
        ImGui::BulletText("\"%s\": %d keys", track.name.c_str(),
                          static_cast<int>(track.keys.size()));
    }
}



/**
 * クリップ構築・解除
 */

std::vector<std::shared_ptr<models::Assembly>>
AnimationViewerGUI::CollectTargets() {
    const auto root = GetScene().RootPtr();

    // 対象: root直下の子Assembly (読み込んだファイル1つにつき1子)
    std::vector<std::shared_ptr<models::Assembly>> targets;
    for (const auto& child : root->GetChildAssemblies()) {
        if (child) targets.push_back(child);
    }
    if (!demo_params_.selected_only) return targets;

    // 選択中のID (エンティティ単位) をroot直下の祖先へ畳み込んで絞り込む
    std::unordered_set<ObjectID> selected;
    for (const auto& id : GetScene().ActiveSelection().Items()) {
        if (const auto top = TopLevelAncestorId(*root, id)) {
            selected.insert(*top);
        }
    }
    targets.erase(
            std::remove_if(targets.begin(), targets.end(),
                           [&selected](const auto& child) {
                               return selected.count(child->GetID()) == 0;
                           }),
            targets.end());
    return targets;
}

void AnimationViewerGUI::BuildAndBindDemoClip() {
    const auto root = GetScene().RootPtr();
    const auto targets = CollectTargets();
    if (targets.empty()) {
        anim_status_ = demo_params_.selected_only
                ? "No selected assemblies. Select entities, or turn off "
                  "'Selected assemblies only'."
                : "No child assemblies to animate. Load a model first.";
        return;
    }

    const auto params = Sanitize(demo_params_);
    const auto plan = ComputeDemoPlan(params, static_cast<int>(targets.size()));
    const auto extent = GetModelExtent(*root);
    // 設定変更による再構築 (Rebuild) では再生を継続する
    const bool was_playing = player_.State() == anim::PlaybackState::kPlaying;

    anim::AnimationClip clip;
    try {
        for (std::size_t i = 0; i < targets.size(); ++i) {
            const auto motion =
                    MakeDemoMotion(params, *targets[i], extent, i);
            AppendMotionKeys(&clip, targets[i]->GetID(), motion, params, i);
            if (params.hide_after_travel) {
                AppendVisibilityKeys(&clip, targets[i]->GetID(), params, i);
            }
        }
        AppendStageEvents(&clip, params);
        clip.SetDuration(plan.duration_sec);
    } catch (const std::exception& e) {
        anim_status_ = std::string("Failed to build clip: ") + e.what();
        return;
    }

    const auto unresolved = player_.Bind(root, clip);
    // Bindで時刻変化の記録が初期化されるため、表示側も揃える
    last_time_change_.reset();
    player_.SetSpeed(static_cast<double>(speed_ui_));
    player_.SetLoop(loop_ui_);
    if (was_playing) player_.Play();
    anim_status_ = "Bound " +
            std::to_string(targets.size() - unresolved.size()) +
            " track(s), " + std::to_string(plan.keys_per_track) +
            " keys each (" + FormatSeconds(plan.duration_sec) + " s).";
    RequestRedraw();
}

void AnimationViewerGUI::ReleaseAnimation() {
    player_.Unbind();
    last_time_change_.reset();
    SetContinuousRedraw(false);
    anim_status_ = "Animation released (base poses restored).";
    RequestRedraw();
}

}  // namespace igesio::graphics

#endif  // IGESIO_ANIMATION_EXTENSION_ENABLED
