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

namespace igesio::extensions::animation {

void AnimationClip::AddKey(const ObjectID& target, const double time_sec,
                           const igesio::Matrix4d& transform) {
    if (time_sec < 0.0) {
        throw std::invalid_argument("Keyframe time must be non-negative");
    }
    if (explicit_duration_ && time_sec > *explicit_duration_) {
        throw std::invalid_argument(
                "Keyframe time exceeds the explicit clip duration");
    }
    if (!IsRigidTransform(transform)) {
        throw std::invalid_argument(
                "Keyframe transform must be a rigid transform "
                "(rotation + translation only)");
    }

    // ターゲット毎に1トラックへ集約する (トラック数は少数として線形探索)
    auto it = std::find_if(tracks_.begin(), tracks_.end(),
                           [&target](const AnimationTrack& t) {
                               return t.target == target;
                           });
    if (it == tracks_.end()) {
        tracks_.push_back(AnimationTrack{target, {}});
        it = std::prev(tracks_.end());
    }

    // 時刻昇順を維持して挿入する. upper_boundの直前要素と同時刻なら重複
    auto& keys = it->keys;
    const auto pos = std::upper_bound(
            keys.begin(), keys.end(), time_sec,
            [](const double t, const TransformKeyframe& key) {
                return t < key.time_sec;
            });
    if (pos != keys.begin() && std::prev(pos)->time_sec == time_sec) {
        throw std::invalid_argument(
                "Duplicate keyframe time for the same target");
    }
    keys.insert(pos, TransformKeyframe{time_sec, transform});

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



/**
 * 非メンバ関数
 */

std::optional<std::size_t> ActiveKeyIndex(const AnimationTrack& track,
                                          const double time_sec) {
    const auto& keys = track.keys;
    // time_secより後のキーの先頭を求め、その直前が有効なキーとする
    const auto pos = std::upper_bound(
            keys.begin(), keys.end(), time_sec,
            [](const double t, const TransformKeyframe& key) {
                return t < key.time_sec;
            });
    if (pos == keys.begin()) return std::nullopt;
    return static_cast<std::size_t>(std::distance(keys.begin(), pos)) - 1;
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
