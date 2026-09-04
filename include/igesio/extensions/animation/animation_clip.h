/**
 * @file extensions/animation/animation_clip.h
 * @brief キーフレームアニメーションのデータモデル (ANIMATION用)
 * @author Yayoi Habami
 * @date 2026-09-02
 * @copyright 2026 Yayoi Habami
 * @note アセンブリのIDを対象に、時刻とともに変換行列を切り替えるステップ型の
 *       キーフレーム列を保持する. 各キーは半開区間 [t_k, t_{k+1}) で有効
 *       (最終キーはクリップ終端まで保持) であり、補間は行わない.
 *       再生 (時刻の進行と姿勢の適用) は`AnimationPlayer`が担い、
 *       本ヘッダは純粋なデータと検索用関数のみを提供する (GL非依存・ヘッドレス可).
 *       現在は、変換において回転+並進のみを扱う.
 */
#ifndef IGESIO_EXTENSIONS_ANIMATION_ANIMATION_CLIP_H_
#define IGESIO_EXTENSIONS_ANIMATION_ANIMATION_CLIP_H_

#include <cstddef>
#include <optional>
#include <vector>

#include "igesio/common/id_generator.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/numerics/core/tolerance.h"

namespace igesio::extensions::animation {

/// @brief 1キーフレーム (時刻とその時刻以降に取る変換)
/// @note `transform`は基準姿勢に対する相対変換であり、適用姿勢は
///       `transform · G_base` (親フレームでの後掛け) となる.
///       先頭キーより前は恒等 (基準姿勢) とみなす.
struct TransformKeyframe {
    /// @brief キー時刻 [s] (非負)
    double time_sec = 0.0;
    /// @brief この時刻以降に取る変換 (回転+並進のみ)
    igesio::Matrix4d transform = igesio::Matrix4d::Identity();
};

/// @brief 1ターゲット分のキーフレーム列
/// @note `keys`は`time_sec`昇順とする.
///       構築は`AnimationClip::AddKey`経由で行うこと (昇順維持と検証のため).
struct AnimationTrack {
    /// @brief 対象AssemblyのID
    ObjectID target;
    /// @brief キーフレーム列 (time_sec昇順・同時刻重複なし)
    std::vector<TransformKeyframe> keys;
};

/// @brief キーフレームアニメーションのクリップ (トラックの集合＋総時間)
/// @note 総時間 (Duration) の既定値は全トラックの最大キー時刻.
///       最終キーの姿勢を一定時間表示したい場合は、`SetDuration`で延長する.
class AnimationClip {
 public:
    /// @brief キーフレームを追加する
    /// @param target 対象AssemblyのID
    /// @param time_sec キー時刻 [s] (>= 0)
    /// @param transform 基準姿勢への相対変換 (回転+並進のみ)
    /// @throw std::invalid_argument time_secが負の場合,
    ///        同一ターゲットに同時刻のキーが既に存在する場合,
    ///        transformが剛体変換でない場合、または
    ///        明示設定済みのdurationを超える時刻の場合
    /// @note ターゲット毎に1トラックへ集約し、時刻昇順を維持して挿入する
    void AddKey(const ObjectID& target, double time_sec,
                const igesio::Matrix4d& transform);

    /// @brief 総時間を設定する
    /// @param duration_sec 総時間 [s]
    /// @throw std::invalid_argument 負の場合、または最大キー時刻未満の場合
    /// @note ここで総時間を設定した場合、アニメーションは全トラックの
    ///       最大キー時刻ではなく、設定された総時間まで再生される.
    void SetDuration(double duration_sec);

    /// @brief 総時間を取得する
    /// @return 明示的に設定済みであればその値、無ければ全トラックの最大キー時刻
    ///         (キーが無い場合は0)
    double Duration() const;

    /// @brief トラックの一覧を取得する
    const std::vector<AnimationTrack>& Tracks() const { return tracks_; }

 private:
    /// @brief トラックの集合 (ターゲット毎に1トラック)
    std::vector<AnimationTrack> tracks_;
    /// @brief 明示設定された総時間 [s] (未設定時は最大キー時刻を用いる)
    std::optional<double> explicit_duration_;
    /// @brief 全トラックの最大キー時刻 [s] (キーが無い間は0)
    double max_key_time_ = 0.0;
};



/**
 * 非メンバ関数
 */

/// @brief 指定時刻において有効なキーの添字を取得する
/// @param track 検索対象のトラック (keysはtime_sec昇順であること)
/// @param time_sec 時刻 [s]
/// @return `keys[i].time_sec <= time_sec`を満たす最大の添字i.
///         先頭キーより前 (またはキーが無い) 場合は`std::nullopt` (基準姿勢)
/// @note 各キーは半開区間 [t_k, t_{k+1}) で有効 (t == t_kの場合はキーkが有効).
std::optional<std::size_t>
ActiveKeyIndex(const AnimationTrack& track, double time_sec);

/// @brief 4x4行列が回転+並進のみの剛体変換かを判定する
/// @param transform 判定する行列
/// @param tolerance 許容誤差
/// @return 左上3x3がほぼ回転行列 (直交かつ行列式+1) で、かつ最下行がほぼ
///         (0, 0, 0, 1) であればtrue (スケール・せん断・射影成分を除外)
/// @note 単精度を経由した行列を想定し、既定でfloat尺度の許容誤差を用いる
bool IsRigidTransform(const igesio::Matrix4d& transform,
                      double tolerance = numerics::kFloatGeometryTolerance);

}  // namespace igesio::extensions::animation

#endif  // IGESIO_EXTENSIONS_ANIMATION_ANIMATION_CLIP_H_
