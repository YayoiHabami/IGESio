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

}  // namespace



std::vector<ObjectID> AnimationPlayer::Bind(
        const std::shared_ptr<models::Assembly>& root, AnimationClip clip) {
    if (!root) {
        throw std::invalid_argument("Root assembly pointer cannot be null");
    }
    // 既存のバインドは基準姿勢へ復元してから解除する
    Unbind();

    clip_ = std::move(clip);
    bound_tracks_.reserve(clip_.Tracks().size());
    std::vector<ObjectID> unresolved;
    for (const auto& track : clip_.Tracks()) {
        BoundTrack bt;
        if (const auto node = FindAssemblyById(root, track.target)) {
            bt.node = node;
            bt.base_pose = node->GetGlobalTransform();
        } else {
            // 未解決トラックは弱参照を空のままにする (適用時にスキップ)
            unresolved.push_back(track.target);
        }
        bound_tracks_.push_back(std::move(bt));
    }
    bound_ = true;
    time_ = 0.0;
    state_ = PlaybackState::kStopped;
    // 時刻0にキーを持つトラックがあれば、その姿勢を反映する
    ApplyPoses();
    return unresolved;
}

void AnimationPlayer::Unbind() {
    if (!bound_) return;
    RestoreBasePoses();
    clip_ = AnimationClip();
    bound_tracks_.clear();
    bound_ = false;
    time_ = 0.0;
    state_ = PlaybackState::kStopped;
}

void AnimationPlayer::Play() {
    if (!bound_) return;
    // 終端に到達した後であれば、先頭へ巻き戻してから再生する
    if (state_ == PlaybackState::kFinished ||
        (!loop_ && time_ >= Duration())) {
        time_ = 0.0;
        ApplyPoses();
    }
    state_ = PlaybackState::kPlaying;
}

void AnimationPlayer::Pause() {
    if (state_ != PlaybackState::kPlaying) return;
    state_ = PlaybackState::kPaused;
}

void AnimationPlayer::Stop() {
    if (!bound_) return;
    RestoreBasePoses();
    time_ = 0.0;
    state_ = PlaybackState::kStopped;
}

void AnimationPlayer::Seek(const double time_sec) {
    if (!bound_) return;
    // [0, Duration]へクランプする
    const double duration = Duration();
    time_ = std::max(0.0, std::min(time_sec, duration));
    // 終端到達状態からのSeekは一時停止へ遷移し、再度Playできる状態にする
    if (state_ == PlaybackState::kFinished) {
        state_ = PlaybackState::kPaused;
    }
    ApplyPoses();
}

void AnimationPlayer::Advance(const double wall_dt_sec) {
    if (wall_dt_sec < 0.0) {
        throw std::invalid_argument("Time delta must be non-negative");
    }
    if (state_ != PlaybackState::kPlaying) return;

    time_ += wall_dt_sec * speed_;
    const double duration = Duration();
    if (time_ >= duration) {
        if (loop_ && duration > 0.0) {
            // 先頭へ巻き戻して再生を続ける (複数周分の経過にも対応)
            time_ = std::fmod(time_, duration);
        } else {
            // 最終姿勢を保持したまま終端で停止する
            time_ = duration;
            state_ = PlaybackState::kFinished;
        }
    }
    ApplyPoses();
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

void AnimationPlayer::ApplyPoses() {
    const auto& tracks = clip_.Tracks();
    for (std::size_t i = 0; i < tracks.size(); ++i) {
        auto& bt = bound_tracks_[i];
        const auto node = bt.node.lock();
        if (!node) continue;  // 未解決・ツリー編集で消滅したトラック
        const auto index = ActiveKeyIndex(tracks[i], time_);
        if (index == bt.applied_key) continue;  // 同一キー区間内は再適用しない
        if (index) {
            // キー変換を基準姿勢へ後掛けする (親フレームでの適用)
            node->SetGlobalTransform(
                    tracks[i].keys[*index].transform * bt.base_pose);
        } else {
            node->SetGlobalTransform(bt.base_pose);
        }
        bt.applied_key = index;
    }
}

void AnimationPlayer::RestoreBasePoses() {
    for (auto& bt : bound_tracks_) {
        // キー姿勢を適用済みのトラックのみ復元する (不要なリビジョンバンプ回避)
        if (!bt.applied_key) continue;
        if (const auto node = bt.node.lock()) {
            node->SetGlobalTransform(bt.base_pose);
        }
        bt.applied_key = std::nullopt;
    }
}

}  // namespace igesio::extensions::animation
