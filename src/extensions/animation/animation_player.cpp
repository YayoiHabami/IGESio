/**
 * @file extensions/animation/animation_player.cpp
 * @brief キーフレームアニメーションの再生状態を管理するクラス
 * @author Yayoi Habami
 * @date 2026-09-02
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/animation/animation_player.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace igesio::extensions::animation {

namespace {

/// @brief Assemblyツリーから指定IDのノードを再帰的に探索する
/// @param node 探索の起点ノード (shared_ptr管理であること)
/// @param id 探索するAssemblyのID
/// @return 見つかったノード. 見つからない場合はnullptr
std::shared_ptr<models::Assembly> FindAssemblyById(
        const std::shared_ptr<models::Assembly>& node, const ObjectID& id) {
    if (!node) return nullptr;
    if (node->GetID() == id) return node;
    for (const auto& child : node->GetChildAssemblies()) {
        if (auto found = FindAssemblyById(child, id)) return found;
    }
    return nullptr;
}

/// @brief 未解決IDを重複なく記録する
/// @param[out] unresolved 未解決IDのリスト
/// @param id 追加するID (既に含まれていれば何もしない)
/// @note 変換トラックと可視性トラックの両方で未解決の同一IDを1回に抑える
void AddUnresolved(std::vector<ObjectID>* unresolved, const ObjectID& id) {
    if (std::find(unresolved->begin(), unresolved->end(), id) ==
        unresolved->end()) {
        unresolved->push_back(id);
    }
}

/// @brief 変換トラックの適用規則
/// @note トラック種別ごとの差異 (状態の型・捕捉・キーからの決定・適用) を
///       1箇所へ集約し、結合/適用/復元の手順はテンプレート側で共有する
struct TransformPolicy {
    /// @brief 対応するクリップ側のトラック型
    using Track = AnimationTrack;
    /// @brief 基準状態・適用状態の型
    using State = igesio::Matrix4d;

    /// @brief 未解決トラック用の既定状態
    static State Default() { return State::Identity(); }
    /// @brief 対象の現在の状態を基準状態として捕捉する
    static State Capture(const models::Assembly& node) {
        return node.GetGlobalTransform();
    }
    /// @brief 有効キーから適用する状態を求める
    /// @note キー変換は基準姿勢へ後掛けする (親フレームでの適用)
    static State Resolve(const TransformKeyframe& key, const State& base) {
        return key.transform * base;
    }
    /// @brief 状態を対象へ適用する
    static void Apply(models::Assembly* node, const State& state) {
        node->SetGlobalTransform(state);
    }
};

/// @brief 可視性トラックの適用規則
struct VisibilityPolicy {
    /// @brief 対応するクリップ側のトラック型
    using Track = VisibilityTrack;
    /// @brief 基準状態・適用状態の型
    using State = bool;

    /// @brief 未解決トラック用の既定状態
    static State Default() { return true; }
    /// @brief 対象の現在の状態を基準状態として捕捉する
    static State Capture(const models::Assembly& node) {
        return node.Display().visible;
    }
    /// @brief 有効キーから適用する状態を求める (基準状態には依存しない)
    static State Resolve(const VisibilityKeyframe& key, const State&) {
        return key.visible;
    }
    /// @brief 状態を対象へ適用する
    /// @note ノード自身の可視性のみ与える (子孫への波及はレンダラの実効判定に任せ,
    ///       復元時に子孫の元の状態を失わないようにする)
    static void Apply(models::Assembly* node, const State state) {
        node->SetVisible(state);
    }
};

/// @brief トラック列をAssemblyツリーへ結び付け、基準状態を捕捉する
/// @tparam Policy トラック種別ごとの適用規則
/// @param root ターゲット解決の起点となるAssembly
/// @param tracks クリップ側のトラック列
/// @param[out] bound バインド結果 (tracksと同添字で対応)
/// @param[out] unresolved 未解決IDのリスト (重複なく追加する)
/// @note 未解決トラックも添字を合わせるために追加し、弱参照を空のままとする
///       (適用・復元時にスキップされる)
template <class Policy>
void BindTracks(
        const std::shared_ptr<models::Assembly>& root,
        const std::vector<typename Policy::Track>& tracks,
        std::vector<detail::BoundTrack<typename Policy::State>>* bound,
        std::vector<ObjectID>* unresolved) {
    bound->reserve(tracks.size());
    for (const auto& track : tracks) {
        detail::BoundTrack<typename Policy::State> bt;
        if (const auto node = FindAssemblyById(root, track.target)) {
            bt.node = node;
            bt.base_state = Policy::Capture(*node);
        } else {
            bt.base_state = Policy::Default();
            AddUnresolved(unresolved, track.target);
        }
        bound->push_back(std::move(bt));
    }
}

/// @brief 有効キーが変わったトラックのみ現在時刻の状態を適用する
/// @tparam Policy トラック種別ごとの適用規則
/// @param tracks クリップ側のトラック列
/// @param time_sec 現在時刻 [s]
/// @param[in,out] bound バインド結果 (tracksと同添字で対応)
template <class Policy>
void ApplyTrackStates(
        const std::vector<typename Policy::Track>& tracks,
        const double time_sec,
        std::vector<detail::BoundTrack<typename Policy::State>>* bound) {
    for (std::size_t i = 0; i < tracks.size(); ++i) {
        auto& bt = (*bound)[i];
        const auto node = bt.node.lock();
        if (!node) continue;  // 未解決・ツリー編集で消滅したトラック
        const auto index = ActiveKeyIndex(tracks[i], time_sec);
        if (index == bt.applied_key) continue;  // 同一キー区間内は再適用しない
        Policy::Apply(node.get(),
                      index ? Policy::Resolve(tracks[i].keys[*index],
                                              bt.base_state)
                            : bt.base_state);
        bt.applied_key = index;
    }
}

/// @brief キー状態を適用済みのトラックを基準状態へ戻す
/// @tparam Policy トラック種別ごとの適用規則
/// @param[in,out] bound バインド結果
/// @note 未適用のトラックには触れない (不要なリビジョンバンプ回避)
template <class Policy>
void RestoreTracks(
        std::vector<detail::BoundTrack<typename Policy::State>>* bound) {
    for (auto& bt : *bound) {
        if (!bt.applied_key) continue;
        if (const auto node = bt.node.lock()) {
            Policy::Apply(node.get(), bt.base_state);
        }
        bt.applied_key = std::nullopt;
    }
}

}  // namespace



std::vector<ObjectID> AnimationPlayer::Bind(
        const std::shared_ptr<models::Assembly>& root, AnimationClip clip) {
    if (!root) {
        throw std::invalid_argument("Root assembly pointer cannot be null");
    }
    // 既存のバインドは基準状態へ復元してから解除する
    Unbind();

    clip_ = std::move(clip);
    std::vector<ObjectID> unresolved;

    BindTracks<TransformPolicy>(root, clip_.Tracks(), &bound_tracks_,
                                &unresolved);
    BindTracks<VisibilityPolicy>(root, clip_.VisibilityTracks(),
                                 &bound_visibility_tracks_, &unresolved);

    bound_ = true;
    time_ = 0.0;
    state_ = PlaybackState::kStopped;
    // 時刻変化の記録はBind時点 (時刻0) を起点に初期化する
    change_from_ = 0.0;
    change_monotone_ = true;
    change_occurred_ = false;
    // 時刻0にキーを持つトラックがあれば、その状態を反映する
    ApplyTracks();
    return unresolved;
}

void AnimationPlayer::Unbind() {
    if (!bound_) return;
    RestoreBaseState();
    SetTime(0.0, /*forward=*/false);
    clip_ = AnimationClip();
    bound_tracks_.clear();
    bound_visibility_tracks_.clear();
    bound_ = false;
    state_ = PlaybackState::kStopped;
}

void AnimationPlayer::Play() {
    if (!bound_) return;
    // 終端に到達した後であれば、先頭へ巻き戻してから再生する
    if (state_ == PlaybackState::kFinished ||
        (!loop_ && time_ >= Duration())) {
        SetTime(0.0, /*forward=*/false);
        ApplyTracks();
    }
    state_ = PlaybackState::kPlaying;
}

void AnimationPlayer::Pause() {
    if (state_ != PlaybackState::kPlaying) return;
    state_ = PlaybackState::kPaused;
}

void AnimationPlayer::Stop() {
    if (!bound_) return;
    RestoreBaseState();
    SetTime(0.0, /*forward=*/false);
    state_ = PlaybackState::kStopped;
}

void AnimationPlayer::Seek(const double time_sec) {
    if (!bound_) return;
    // [0, Duration]へクランプする
    const double duration = Duration();
    const double target = std::max(0.0, std::min(time_sec, duration));
    SetTime(target, /*forward=*/target >= time_);
    // 終端到達状態からのSeekは一時停止へ遷移し、再度Playできる状態にする
    if (state_ == PlaybackState::kFinished) {
        state_ = PlaybackState::kPaused;
    }
    ApplyTracks();
}

void AnimationPlayer::Advance(const double wall_dt_sec) {
    if (wall_dt_sec < 0.0) {
        throw std::invalid_argument("Time delta must be non-negative");
    }
    if (state_ != PlaybackState::kPlaying) return;

    const double advanced = time_ + wall_dt_sec * speed_;
    const double duration = Duration();
    if (advanced < duration) {
        SetTime(advanced, /*forward=*/true);
    } else if (loop_ && duration > 0.0) {
        // 先頭へ巻き戻して再生を続ける (複数周分の経過にも対応).
        // 1周分の経過を含むため、時刻が偶然一致しても非単調な変化として記録する
        time_ = std::fmod(advanced, duration);
        change_occurred_ = true;
        change_monotone_ = false;
    } else {
        // 最終状態を保持したまま終端で停止する
        SetTime(duration, /*forward=*/true);
        state_ = PlaybackState::kFinished;
    }
    ApplyTracks();
}

TimeChange AnimationPlayer::TakeTimeChange() {
    TimeChange change;
    change.changed = change_occurred_;
    change.from = change_from_;
    change.to = time_;
    change.monotone = change_monotone_;
    // 現在時刻を次回の起点としてリセットする
    change_from_ = time_;
    change_occurred_ = false;
    change_monotone_ = true;
    return change;
}

void AnimationPlayer::SetSpeed(const double speed) {
    if (speed <= 0.0) {
        throw std::invalid_argument("Playback speed must be positive");
    }
    speed_ = speed;
}

double AnimationPlayer::Duration() const {
    return bound_ ? clip_.Duration() : 0.0;
}

void AnimationPlayer::ApplyTracks() {
    ApplyTrackStates<TransformPolicy>(clip_.Tracks(), time_, &bound_tracks_);
    ApplyTrackStates<VisibilityPolicy>(clip_.VisibilityTracks(), time_,
                                       &bound_visibility_tracks_);
}

void AnimationPlayer::RestoreBaseState() {
    RestoreTracks<TransformPolicy>(&bound_tracks_);
    RestoreTracks<VisibilityPolicy>(&bound_visibility_tracks_);
}

void AnimationPlayer::SetTime(const double new_time, const bool forward) {
    if (new_time == time_) return;
    time_ = new_time;
    change_occurred_ = true;
    if (!forward) change_monotone_ = false;
}

}  // namespace igesio::extensions::animation
