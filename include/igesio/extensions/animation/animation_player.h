/**
 * @file extensions/animation/animation_player.h
 * @brief キーフレームアニメーションの再生状態を管理するクラス
 * @author Yayoi Habami
 * @date 2026-09-02
 * @copyright 2026 Yayoi Habami
 * @note `AnimationClip`をAssemblyツリーへ結び付け、時刻の進行に応じて
 *       対象Assemblyの大域変換と可視状態を差し替える. 時計は持たず,
 *       実時間の取得は呼び出し側 (GUIなら`glfwGetTime`の差分等) が行い,
 *       `Advance(dt)`で渡す (ヘッドレス・テスト可能性のため).
 * @note 姿勢の適用は`Assembly::SetGlobalTransform`、可視状態の適用は
 *       `Assembly::SetVisible`経由で行う. いずれもモデルリビジョンのバンプを内蔵し,
 *       レンダラはワールド変換・表示状態のみを更新する (再テッセレーションさせない).
 *       さらにステップ補間のため、本クラスはキーが切り替わったトラックのみを適用する.
 *       これによりリビジョンバンプはキー境界を跨いだフレームのみに抑えられる.
 * @note イベントトラックはシーンに作用しないため、本クラスは適用処理を持たない.
 *       利用側は`Clip().FindEventTrack(name)`と`CurrentTime()`で値を問い合わせ、
 *       時刻の動き (再生/巻き戻し) は`TakeTimeChange`で判定する.
 * @note スレッド安全性は提供しない (GUIスレッド等の単一スレッドからの利用を想定).
 */
#ifndef IGESIO_EXTENSIONS_ANIMATION_ANIMATION_PLAYER_H_
#define IGESIO_EXTENSIONS_ANIMATION_ANIMATION_PLAYER_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "igesio/common/id_generator.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/models/assembly.h"
#include "igesio/extensions/animation/animation_clip.h"

namespace igesio::extensions::animation {

/// @brief 再生状態
enum class PlaybackState {
    /// @brief 停止中 (時刻0・基準状態)
    kStopped,
    /// @brief 再生中
    kPlaying,
    /// @brief 一時停止中 (時刻・状態を維持する)
    kPaused,
    /// @brief 終端到達 (最終状態を維持する. ループ無効時のみ遷移する)
    kFinished,
};

/// @brief 前回の`AnimationPlayer::TakeTimeChange`以降の時刻の動き
/// @note `from <= to`であっても`monotone == false`となる場合がある
///       (ループ巻き戻し経由、巻き戻してから再生).
///       利用側は`from`と`to`の大小で判定せず、`monotone`で判定すること
struct TimeChange {
    /// @brief 時刻が動いたか (Seekで同時刻へ移動した場合はfalse)
    bool changed = false;
    /// @brief 前回Take時点 (初回はBind時点=0) の時刻 [s]
    double from = 0.0;
    /// @brief 現在時刻 [s]
    double to = 0.0;
    /// @brief 途中に後退・巻き戻し・リセットが無かったか
    bool monotone = true;
};

namespace detail {

/// @brief バインド済みトラックの状態の基底
/// @tparam State 基準状態の型
///         (変換トラックなら`igesio::Matrix4d`、可視性トラックなら`bool`)
/// @note クリップ側の`TrackBase`と対になる、プレーヤー側の共通データ.
///       対象への弱参照と差分適用用の適用済みキー添字は全種で共通であり,
///       種別ごとの差異は`State`と、その捕捉・決定・適用の方法のみ.
///       後者は`animation_player.cpp`内で定義するポリシーが決定する
/// @note 対象Assemblyを持たないイベントトラックは適用処理を持たないため対象外.
template <class State>
struct BoundTrack {
    /// @brief 対象Assemblyへの弱参照
    /// @note ツリー編集による消失を安全に扱うため弱参照.
    ///       未解決トラックは空のままとし、適用時にスキップする
    std::weak_ptr<models::Assembly> node;
    /// @brief 基準状態 (Bind時に捕捉した大域変換・可視状態)
    /// @note 既定構築時の値は`State`の既定に従う (Eigenの行列は未初期化) ため,
    ///       未解決トラックを含め`Bind`時に必ず設定する
    State base_state{};
    /// @brief 最後に適用したキー添字 (nulloptは基準状態を適用済み)
    /// @note 現在の有効キーと一致する間は再適用しない (差分適用)
    std::optional<std::size_t> applied_key;
};

}  // namespace detail

/// @brief キーフレームアニメーションの再生状態を管理するクラス
/// @note 使い方：`Bind`でクリップとAssemblyツリーを結び付け,
///       `Play`後に毎フレーム`Advance(dt)`を呼ぶ.
///       `Seek`は停止中でも即時に状態を反映する.
///       `Stop`/`Unbind`は基準状態 (Bind時に捕捉した大域変換と可視状態) に戻す.
/// @note 再生中に利用側が対象の大域変換・可視状態を直接変更した場合、次のキー境界
///       でクリップの値に上書きされ、`Stop`では`Bind`時の値へ戻る.
/// @note 基本的には、同じシーンについて複数のインスタンスは作成しないこと.
///       `::Bind`で取得したアセンブリの大域変換・可視状態を直接書き換えるため、
///       複数のプレーヤーが同じAssemblyを操作すると競合する.
class AnimationPlayer {
 public:
    /// @brief クリップをAssemblyツリーに結び付ける
    /// @param root ターゲット解決の起点となるAssembly
    /// @param clip 再生するクリップ (コピーして保持する)
    /// @return 解決できなかったターゲットIDのリスト
    ///         (全解決なら空.変換と可視状態の両方で未解決の同一IDは1回だけ含む).
    ///         未解決トラックは再生対象から除外される (部分バインドは許容する)
    /// @throw std::invalid_argument rootがnullptrの場合
    /// @note 各ターゲットの現在の大域変換を基準姿勢`G_base`,
    ///       現在の可視状態 (`Display().visible`) を基準可視状態として記録する.
    ///       既にバインド済みの場合は先に`Unbind` (基準状態へ復元) する.
    ///       時刻0にキーを持つトラックがある場合、その状態をすぐに適用する.
    ///       時刻変化の記録は初期化される (`TakeTimeChange`は changed=false)
    std::vector<ObjectID> Bind(const std::shared_ptr<models::Assembly>& root,
                               AnimationClip clip);

    /// @brief バインドを解除する
    /// @note 全トラックを基準状態へ復元し、クリップとバインド情報を破棄する.
    ///       時刻が0でなければ、0への非単調な移動として記録する.
    ///       未バインドの場合は何もしない
    void Unbind();

    /// @brief バインド済みか
    bool IsBound() const { return bound_; }

    /// @brief 再生を開始・再開する
    /// @note 終端に到達した後であれば、先頭へ巻き戻してから再生する
    ///       (非単調な移動として記録). 未バインドの場合は何もしない
    void Play();

    /// @brief 一時停止する (時刻・状態を保持)
    /// @note 再生中でない場合は何もしない
    void Pause();

    /// @brief 再生を停止する (時刻0へ戻し、基準状態に戻す)
    /// @note 時刻0にキーを持つトラックも復元先は基準状態とする
    ///       (停止=アニメーション解除とする). 時刻が0でなければ,
    ///       0への非単調な移動として記録する. 未バインドの場合は何もしない
    void Stop();

    /// @brief 指定時刻へ移動する (停止・一時停止中でも即時に状態を反映する)
    /// @param time_sec 移動先の時刻 [s] ([0, Duration()]へクランプされる)
    /// @note kFinishedからのSeekはkPausedへ遷移する (再度Playできる状態にする).
    ///       現在時刻より前への移動は非単調な移動として記録する.
    ///       未バインドの場合は何もしない
    void Seek(double time_sec);

    /// @brief 時刻を進めて状態を適用する (毎フレーム呼ぶ)
    /// @param wall_dt_sec 前回呼び出しからの実経過時間 [s] (非負)
    /// @throw std::invalid_argument wall_dt_secが負の場合
    /// @note 内部時刻はwall_dt_sec×速度だけ進む. 再生中でない場合は何もしない.
    ///       終端到達時はループ設定に応じて先頭へ巻き戻す (非単調な移動として記録する)
    ///       か、kFinishedへ遷移して最終状態を保持する
    void Advance(double wall_dt_sec);

    /// @brief 前回呼び出し以降の時刻の動きを取得し、記録をリセットする
    /// @return 時刻の動き. 複数回の変更が1回の呼び出しにまとめられる場合、
    ///         `from`は最初の値、`to`は最後の値、巻き戻しがなければ`monotone`はtrue
    /// @note 毎フレーム1回、`Advance`の後に呼ぶ想定. 未バインドの間は時刻が動かないため
    ///       changed=false (`Unbind`による0への巻き戻しは、その後の最初の呼び出しで
    ///       1回だけ報告される). `Pause`・`SetSpeed`・`SetLoop`は記録に影響しない
    TimeChange TakeTimeChange();

    /// @brief 再生速度を設定する
    /// @param speed 再生速度 (1.0で等速. 正値のみ)
    /// @throw std::invalid_argument speedが正でない場合
    void SetSpeed(double speed);

    /// @brief 再生速度を取得する
    double Speed() const { return speed_; }

    /// @brief ループ再生を設定する
    /// @param loop trueの場合、終端到達時に先頭へ巻き戻して再生を続ける
    void SetLoop(const bool loop) { loop_ = loop; }

    /// @brief ループ再生が有効か
    bool Loop() const { return loop_; }

    /// @brief 現在時刻を取得する [s]
    double CurrentTime() const { return time_; }

    /// @brief 総時間を取得する [s] (未バインド時は0)
    double Duration() const;

    /// @brief 再生状態を取得する
    PlaybackState State() const { return state_; }

    /// @brief バインド中のクリップを取得する (未バインド時は空クリップ)
    const AnimationClip& Clip() const { return clip_; }

 private:
    /// @brief 現在時刻の状態 (変換+可視状態) を全トラックへ適用する (差分のみ適用)
    /// @note 有効キー添字が前回適用時から変化したトラックのみ
    ///       `SetGlobalTransform`/`SetVisible`を呼ぶ.
    ///       弱参照が消えたトラックはスキップする
    void ApplyTracks();

    /// @brief 全トラックを基準状態 (変換+可視状態) に戻す
    /// @note キー状態を適用済みの (applied_keyが値を持つ) トラックのみ戻す
    void RestoreBaseState();

    /// @brief 時刻を書き換え、`TakeTimeChange`用に動きを記録する
    /// @param new_time 新しい時刻 [s]
    /// @param forward 再生 (巻き戻しなし) として扱うか.
    ///        falseの場合、時刻が実際に動いたときのみ非単調として記録する
    void SetTime(double new_time, bool forward);

    /// @brief バインド中のクリップ
    AnimationClip clip_;
    /// @brief バインド済み変換トラックの状態 (clip_.Tracks()と同添字で対応)
    /// @note 基準状態はBind時の大域変換であり、キー変換はこれへ後掛けする
    std::vector<detail::BoundTrack<igesio::Matrix4d>> bound_tracks_;
    /// @brief バインド済み可視性トラックの状態
    ///        (clip_.VisibilityTracks()と同添字で対応)
    /// @note 基準状態はBind時の`Display().visible`
    std::vector<detail::BoundTrack<bool>> bound_visibility_tracks_;
    /// @brief バインド済みか
    bool bound_ = false;
    /// @brief 現在時刻 [s]
    double time_ = 0.0;
    /// @brief 再生速度 (1.0で等速)
    double speed_ = 1.0;
    /// @brief ループ再生が有効か
    bool loop_ = false;
    /// @brief 再生状態
    PlaybackState state_ = PlaybackState::kStopped;
    /// @brief 前回`TakeTimeChange`時点の時刻 [s]
    double change_from_ = 0.0;
    /// @brief 前回`TakeTimeChange`以降に非単調な移動が無かったか
    bool change_monotone_ = true;
    /// @brief 前回`TakeTimeChange`以降に時刻が動いたか
    bool change_occurred_ = false;
};

}  // namespace igesio::extensions::animation

#endif  // IGESIO_EXTENSIONS_ANIMATION_ANIMATION_PLAYER_H_
