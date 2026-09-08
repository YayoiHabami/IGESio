/**
 * @file extensions/animation/animation_clip.cpp
 * @brief キーフレームアニメーションのデータモデル
 * @author Yayoi Habami
 * @date 2026-09-02
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/animation/animation_clip.h"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace igesio::extensions::animation {

namespace {

/// @brief 時刻について昇順を維持してキーを挿入する
/// @tparam Key `KeyframeBase`を継承したキー型
/// @param[out] keys 挿入先のキー列 (時刻昇順)
/// @param key 挿入するキー
/// @param duplicate_message 同時刻のキーが既に存在する場合の例外メッセージ
/// @throw std::invalid_argument 同時刻のキーが既に存在する場合
template <class Key>
void InsertKeySorted(std::vector<Key>* keys, Key key,
                     const char* duplicate_message) {
    // upper_boundの直前要素と同時刻なら重複
    const auto pos = detail::UpperBoundByTime(*keys, key.time_sec);
    if (pos != keys->begin() && std::prev(pos)->time_sec == key.time_sec) {
        throw std::invalid_argument(duplicate_message);
    }
    keys->insert(pos, std::move(key));
}

/// @brief 指定されたトラックに一致するものを探し、無ければ末尾に新規作成して返す
/// @tparam Track トラック型
/// @tparam Pred トラックを受け取りboolを返す関数
/// @tparam Make 新規トラックを生成する関数
/// @param[out] tracks トラックの集合
/// @param pred 既存トラックの一致判定
/// @param make 新規トラックの生成
/// @return 一致した (または新規作成した) トラックへの参照
/// @note トラック数は少数として線形探索する
template <class Track, class Pred, class Make>
Track& FindOrCreateTrack(std::vector<Track>* tracks, Pred pred, Make make) {
    auto it = std::find_if(tracks->begin(), tracks->end(), pred);
    if (it != tracks->end()) return *it;
    tracks->push_back(make());
    return tracks->back();
}

}  // namespace



void AnimationClip::AddKey(const ObjectID& target, const double time_sec,
                           const igesio::Matrix4d& transform) {
    ValidateKeyTime(time_sec);
    if (!IsRigidTransform(transform)) {
        throw std::invalid_argument(
                "Keyframe transform must be a rigid transform "
                "(rotation + translation only)");
    }

    // ターゲット毎に1トラックへ集約する
    auto& track = FindOrCreateTrack(
            &tracks_,
            [&target](const AnimationTrack& t) { return t.target == target; },
            [&target]() {
                return AnimationTrack{{}, target};
            });
    InsertKeySorted(&track.keys, TransformKeyframe{{time_sec}, transform},
                    "Duplicate keyframe time for the same target");

    max_key_time_ = std::max(max_key_time_, time_sec);
}

void AnimationClip::AddVisibilityKey(const ObjectID& target,
                                     const double time_sec,
                                     const bool visible) {
    ValidateKeyTime(time_sec);

    auto& track = FindOrCreateTrack(
            &visibility_tracks_,
            [&target](const VisibilityTrack& t) { return t.target == target; },
            [&target]() {
                return VisibilityTrack{{}, target};
            });
    InsertKeySorted(&track.keys, VisibilityKeyframe{{time_sec}, visible},
                    "Duplicate visibility key time for the same target");

    max_key_time_ = std::max(max_key_time_, time_sec);
}

void AnimationClip::AddEvent(const std::string& name, const double time_sec,
                             const std::int64_t value) {
    if (name.empty()) {
        throw std::invalid_argument("Event track name must not be empty");
    }
    ValidateKeyTime(time_sec);

    auto& track = FindOrCreateTrack(
            &event_tracks_,
            [&name](const EventTrack& t) { return t.name == name; },
            [&name]() {
                return EventTrack{{}, name};
            });
    InsertKeySorted(&track.keys, EventKey{{time_sec}, value},
                    "Duplicate event key time for the same track");

    max_key_time_ = std::max(max_key_time_, time_sec);
}

void AnimationClip::SetDuration(const double duration_sec) {
    if (duration_sec < 0.0) {
        throw std::invalid_argument("Clip duration must be non-negative");
    }
    if (duration_sec < max_key_time_) {
        throw std::invalid_argument(
                "Clip duration must not be less than the last keyframe time");
    }
    explicit_duration_ = duration_sec;
}

double AnimationClip::Duration() const {
    return explicit_duration_ ? *explicit_duration_ : max_key_time_;
}

const EventTrack* AnimationClip::FindEventTrack(
        const std::string& name) const {
    const auto it = std::find_if(
            event_tracks_.begin(), event_tracks_.end(),
            [&name](const EventTrack& t) { return t.name == name; });
    return it == event_tracks_.end() ? nullptr : &*it;
}

void AnimationClip::ValidateKeyTime(const double time_sec) const {
    if (time_sec < 0.0) {
        throw std::invalid_argument("Keyframe time must be non-negative");
    }
    if (explicit_duration_ && time_sec > *explicit_duration_) {
        throw std::invalid_argument(
                "Keyframe time exceeds the explicit clip duration");
    }
}



/**
 * 非メンバ関数
 */

std::optional<std::int64_t> ActiveEventValue(const EventTrack& track,
                                             const double time_sec) {
    const auto index = ActiveKeyIndex(track, time_sec);
    if (!index) return std::nullopt;
    return track.keys[*index].value;
}

std::pair<std::size_t, std::size_t>
EventKeysBetween(const EventTrack& track, const double from, const double to) {
    // 前進・後退のいずれも`min < t_k <= max`のキーが対象となる
    // (左開・右閉のため、両端ともupper_boundで求まる)
    const auto& keys = track.keys;
    const auto lo = detail::UpperBoundByTime(keys, std::min(from, to));
    const auto hi = detail::UpperBoundByTime(keys, std::max(from, to));
    return {static_cast<std::size_t>(std::distance(keys.begin(), lo)),
            static_cast<std::size_t>(std::distance(keys.begin(), hi))};
}

bool IsRigidTransform(const igesio::Matrix4d& transform,
                      const double tolerance) {
    // 左上3x3が回転行列であること (スケール・せん断以外)
    const igesio::Matrix3d rotation = transform.block<3, 3>(0, 0);
    if (!numerics::IsRotation(rotation, tolerance)) return false;
    // 最下行が (0, 0, 0, 1) であること (射影成分を除外)
    return numerics::IsApproxEqual(transform(3, 0), 0.0, tolerance) &&
           numerics::IsApproxEqual(transform(3, 1), 0.0, tolerance) &&
           numerics::IsApproxEqual(transform(3, 2), 0.0, tolerance) &&
           numerics::IsApproxEqual(transform(3, 3), 1.0, tolerance);
}

}  // namespace igesio::extensions::animation
