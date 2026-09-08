/**
 * @file extensions/animation/animation_clip.h
 * @brief キーフレームアニメーションのデータモデル (ANIMATION用)
 * @author Yayoi Habami
 * @date 2026-09-02
 * @copyright 2026 Yayoi Habami
 * @note アセンブリのIDを対象に、時刻とともに状態を切り替えるステップ型の
 *       キーフレーム列を保持する. 各キーは半開区間 [t_k, t_{k+1}) で有効
 *       (最終キーはクリップ終端まで保持) であり、補間は行わない.
 *       再生 (時刻の進行と状態の適用) は`AnimationPlayer`が担い、
 *       本ヘッダは純粋なデータと検索用関数のみを提供する (GL非依存・ヘッドレス可).
 * @note キーは次の3種を扱う. いずれも区間については同じ規則を共有するため,
 *       時刻は`KeyframeBase`、キー列は`TrackBase`を共通の基底とする.
 *       - 変換キー (回転+並進): 対象Assemblyの大域変換を切り替える
 *       - 可視性キー: 対象Assemblyの可視性 (`Assembly::SetVisible`) を切り替える
 *       - イベントキー: 対象を持たない名前付きの整数値列. シーンには作用せず、
 *         利用側 (GUI・外部シミュレーション等) が現在時刻の値を問い合わせる
 */
#ifndef IGESIO_EXTENSIONS_ANIMATION_ANIMATION_CLIP_H_
#define IGESIO_EXTENSIONS_ANIMATION_ANIMATION_CLIP_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "igesio/common/id_generator.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/numerics/core/tolerance.h"

namespace igesio::extensions::animation {

/// @brief キーの基底
/// @note すべての種類のキーが共有する要素の定義.
///       各キーは半開区間 [t_k, t_{k+1}) で有効であり (t == t_kのときキーkが有効),
///       最終キーはクリップ終端まで保持される. 補間は行わない.
/// @note データの共有のみを目的とした基底であり、多態的な利用 (基底へのポインタ・参照
///       経由での保持や破棄) は想定しない (仮想デストラクタは設定しない).
/// @note 派生型の集約初期化の際は基底が先頭要素
///       (例: `TransformKeyframe{{time_sec}, transform}`).
struct KeyframeBase {
    /// @brief キー時刻 [s] (非負)
    double time_sec = 0.0;
};

/// @brief トラックの基底
/// @tparam Key `KeyframeBase`を継承したキー型
/// @note 対象 (AssemblyのIDやトラック名) は派生側が持ち、本基底は全トラックに
///       共通するキー列のみを持つ. 検索用の非メンバ関数 (`ActiveKeyIndex`) は
///       本基底に対して定義され、3種のトラックで共有される.
/// @note `keys`の構築は`AnimationClip`のキー追加メソッド経由で行うこと
///       (昇順維持と検証のため).
/// @note `KeyframeBase`と同じく多態的な利用は想定しない.
template <class Key>
struct TrackBase {
    /// @brief キー列 (time_sec昇順・同時刻重複なし)
    std::vector<Key> keys;
};

/// @brief 1キーフレーム (時刻とその時刻以降に取る変換)
/// @note `transform`は基準姿勢に対する相対変換であり、適用姿勢は
///       `transform · G_base` (親フレームでの後掛け) となる.
///       先頭キーより前は恒等 (基準姿勢) とみなす.
struct TransformKeyframe : KeyframeBase {
    /// @brief この時刻以降に取る変換 (回転+並進のみ)
    igesio::Matrix4d transform = igesio::Matrix4d::Identity();
};

/// @brief 1ターゲット分のキーフレーム列
/// @note 構築は`AnimationClip::AddKey`経由で行うこと (昇順維持と検証のため).
struct AnimationTrack : TrackBase<TransformKeyframe> {
    /// @brief 対象AssemblyのID
    ObjectID target;
};

/// @brief 可視性キー (時刻とその時刻以降の可視性)
/// @note 先頭キーより前は基準の可視性 (`AnimationPlayer::Bind`時の値) とみなす.
struct VisibilityKeyframe : KeyframeBase {
    /// @brief この時刻以降の可視性
    bool visible = true;
};

/// @brief 1ターゲット分の可視性キー列
/// @note 構築は`AnimationClip::AddVisibilityKey`経由で行うこと
///       (昇順維持と検証のため).
/// @note 同じAssemblyを`AnimationTrack`等で使用してよい
///       (別のトラック列として独立に保持する).
struct VisibilityTrack : TrackBase<VisibilityKeyframe> {
    /// @brief 対象AssemblyのID
    ObjectID target;
};

/// @brief イベントキー (時刻とその時刻以降に取る値)
struct EventKey : KeyframeBase {
    /// @brief この時刻以降に取る値
    std::int64_t value = 0;
};

/// @brief 名前付きの整数値ステップ列 (対象Assemblyを持たない)
/// @note 用途例: 選択中の工具番号、現在のブロック索引、切削区間索引.
///       文字列等のペイロードは持たず、値→情報の対応表は利用側が持つ.
///       構築は`AnimationClip::AddEvent`経由で行うこと (昇順維持と検証のため).
struct EventTrack : TrackBase<EventKey> {
    /// @brief トラック名 (空でない. クリップ内で一意)
    std::string name;
};

/// @brief キーフレームアニメーションのクリップ (トラックの集合＋総時間)
/// @note 総時間 (Duration) の既定値は全種類のキーの最大時刻.
///       最終キーの状態を一定時間表示したい場合は、`SetDuration`で延長する.
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

    /// @brief 可視性キーを追加する
    /// @param target 対象AssemblyのID
    /// @param time_sec キー時刻 [s] (>= 0)
    /// @param visible この時刻以降の可視性
    /// @throw std::invalid_argument time_secが負の場合,
    ///        同一ターゲットに同時刻の可視性キーが既に存在する場合,
    ///        または明示設定済みのdurationを超える時刻の場合
    /// @note ターゲット毎に1トラックへ集約し、時刻昇順を維持して挿入する.
    ///       連続する同値キー (true, true) は拒否しない (境界で`SetVisible`は
    ///       呼ばれるが、値が変わらなければ描画の再更新はかからない)
    void AddVisibilityKey(const ObjectID& target, double time_sec,
                          bool visible);

    /// @brief イベントキーを追加する
    /// @param name トラック名 (空でない)
    /// @param time_sec キー時刻 [s] (>= 0)
    /// @param value この時刻以降に取る値
    /// @throw std::invalid_argument nameが空の場合、time_secが負の場合,
    ///        同名トラックに同時刻のキーが既に存在する場合,
    ///        または明示設定済みのdurationを超える時刻の場合
    /// @note 名前毎に1トラックへ集約し、時刻昇順を維持して挿入する
    void AddEvent(const std::string& name, double time_sec,
                  std::int64_t value);

    /// @brief 総時間を設定する
    /// @param duration_sec 総時間 [s]
    /// @throw std::invalid_argument 負の場合、または最大キー時刻未満の場合
    /// @note ここで総時間を設定した場合、アニメーションは全トラックの
    ///       最大キー時刻ではなく、設定された総時間まで再生される.
    void SetDuration(double duration_sec);

    /// @brief 総時間を取得する
    /// @return 明示的に設定済みであればその値、無ければ全種類のトラックの
    ///         最大キー時刻 (キーが無い場合は0)
    double Duration() const;

    /// @brief 変換トラックの一覧を取得する
    const std::vector<AnimationTrack>& Tracks() const { return tracks_; }

    /// @brief 可視性トラックの一覧を取得する
    const std::vector<VisibilityTrack>& VisibilityTracks() const {
        return visibility_tracks_;
    }

    /// @brief イベントトラックの一覧を取得する
    const std::vector<EventTrack>& EventTracks() const {
        return event_tracks_;
    }

    /// @brief 名前でイベントトラックを検索する
    /// @param name トラック名
    /// @return 見つかったトラック. 見つからなければnullptr
    /// @note 戻り値はクリップへ次にキーを追加するまで有効
    const EventTrack* FindEventTrack(const std::string& name) const;

 private:
    /// @brief キー時刻の共通検証 (非負・明示duration以下)
    /// @param time_sec 検証するキー時刻 [s]
    /// @throw std::invalid_argument 負の場合,
    ///        または明示設定済みのdurationを超える場合
    void ValidateKeyTime(double time_sec) const;

    /// @brief 変換トラックの集合 (ターゲット毎に1トラック)
    std::vector<AnimationTrack> tracks_;
    /// @brief 可視性トラックの集合 (ターゲット毎に1トラック)
    std::vector<VisibilityTrack> visibility_tracks_;
    /// @brief イベントトラックの集合 (名前毎に1トラック)
    std::vector<EventTrack> event_tracks_;
    /// @brief 明示設定された総時間 [s] (未設定時は最大キー時刻を用いる)
    std::optional<double> explicit_duration_;
    /// @brief 全種類のトラックの最大キー時刻 [s] (キーが無い間は0)
    double max_key_time_ = 0.0;
};



/**
 * 非メンバ関数
 */

namespace detail {

/// @brief 時刻について昇順のキー列から、指定時刻より後の先頭キーの位置を求める
/// @tparam Key `KeyframeBase`を継承したキー型
/// @param keys 時刻昇順のキー列
/// @param time_sec 時刻 [s]
/// @return `time_sec < keys[i].time_sec`を満たす最初の位置
///         (無ければ`keys.end()`)
template <class Key>
typename std::vector<Key>::const_iterator
UpperBoundByTime(const std::vector<Key>& keys, const double time_sec) {
    return std::upper_bound(keys.begin(), keys.end(), time_sec,
                            [](const double t, const Key& key) {
                                return t < key.time_sec;
                            });
}

}  // namespace detail

/// @brief 指定時刻において有効なキーの添字を取得する
/// @tparam Key `KeyframeBase`を継承したキー型
/// @param track 検索対象のトラック (keysはtime_sec昇順であること)
/// @param time_sec 時刻 [s]
/// @return `keys[i].time_sec <= time_sec`を満たす最大の添字i.
///         先頭キーより前 (またはキーが無い) 場合は`std::nullopt`
///         (変換トラックなら基準姿勢、可視性トラックなら基準可視性を意味する)
/// @note 各キーは半開区間 [t_k, t_{k+1}) で有効 (t == t_kの場合はキーkが有効).
///       3種のトラックで規則を共有するため、基底`TrackBase`に対して定義する
///       (派生トラックを渡した場合は基底への変換で`Key`が決まる)
template <class Key>
std::optional<std::size_t>
ActiveKeyIndex(const TrackBase<Key>& track, const double time_sec) {
    // time_secより後のキーの先頭を求め、その直前が有効なキーとする
    const auto pos = detail::UpperBoundByTime(track.keys, time_sec);
    if (pos == track.keys.begin()) return std::nullopt;
    return static_cast<std::size_t>(
                   std::distance(track.keys.begin(), pos)) - 1;
}

/// @brief 指定時刻において有効なイベント値を取得する
/// @param track 検索対象のトラック (keysはtime_sec昇順であること)
/// @param time_sec 時刻 [s]
/// @return 有効なキーの値. 先頭キーより前 (またはキーが無い) 場合は
///         `std::nullopt`
std::optional<std::int64_t>
ActiveEventValue(const EventTrack& track, double time_sec);

/// @brief 時刻がfromからtoへ動く際に跨ぐイベントキーの添字範囲を取得する
/// @param track 検索対象のトラック (keysはtime_sec昇順であること)
/// @param from 移動前の時刻 [s]
/// @param to 移動後の時刻 [s]
/// @return 添字範囲 [first, last) (昇順. 跨ぐキーが無ければ first == last)
/// @note `from <= to`なら`from < t_k <= to`を満たすキー (新たに有効になった順)
///       `to < from`なら`to < t_k <= from`を満たすキー (巻き戻しで無効になったもの.
///       昇順で返す). `from == to` なら空.
/// @note 時刻fromで既に有効なキーを二重に処理しないため、`from < t_k`とする.
///       再生開始直後の初期状態は`ActiveEventValue(track, 0)`で取る
std::pair<std::size_t, std::size_t>
EventKeysBetween(const EventTrack& track, double from, double to);

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
