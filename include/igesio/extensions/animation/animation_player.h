/**
 * @file extensions/animation/animation_player.h
 * @brief キーフレームアニメーションの再生状態を管理するクラス
 * @author Yayoi Habami
 * @date 2026-09-02
 * @copyright 2026 Yayoi Habami
 * @note `AnimationClip`をAssemblyツリーへ結び付け、時刻の進行に応じて
 *       対象Assemblyの大域変換を差し替える. 時計は持たず、実時間の取得は
 *       呼び出し側 (GUIなら`glfwGetTime`の差分等) が行い、`Advance(dt)`
 *       で渡す (ヘッドレス・テスト可能性のため).
 * @note 姿勢の適用は`Assembly::SetGlobalTransform`経由で行う. 同メンバ関数は
 *       モデルリビジョンのバンプを内蔵し、レンダラはワールド変換のみを更新する
 *       (再テッセレーションは発生しない). さらにステップ補間のため、本クラスは
 *       有効キーが切り替わったトラックのみを適用する.
 *       これによりリビジョンバンプはキー境界を跨いだフレームのみに抑えられる.
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
    /// @brief 停止中 (時刻0・基準姿勢)
    kStopped,
    /// @brief 再生中
    kPlaying,
    /// @brief 一時停止中 (時刻・姿勢を維持する)
    kPaused,
    /// @brief 終端到達 (最終姿勢を維持する. ループ無効時のみ遷移する)
    kFinished,
};

/// @brief キーフレームアニメーションの再生状態を管理するクラス
/// @note 使い方：`Bind`でクリップとAssemblyツリーを結び付け,
///       `Play`後に毎フレーム`Advance(dt)`を呼ぶ.
///       `Seek`は停止中でも即時に姿勢を反映する.
///       `Stop`/`Unbind`は基準姿勢 (Bind時に捕捉した大域変換) に戻す.
/// @note 基本的には、同じシーンについて複数のインスタンスは作成しないこと.
///       `::Bind`で取得したアセンブリの大域変換を直接書き換えるため、
///       複数のプレイヤーが同じAssemblyを操作すると競合する.
class AnimationPlayer {
 public:
    /// @brief クリップをAssemblyツリーに結び付ける
    /// @param root ターゲット解決の起点となるAssembly
    /// @param clip 再生するクリップ (コピーして保持する)
    /// @return 解決できなかったターゲットIDのリスト (全解決なら空).
    ///         未解決トラックは再生対象から除外される (部分バインドは許容する)
    /// @throw std::invalid_argument rootがnullptrの場合
    /// @note 各ターゲットの現在の大域変換を基準姿勢`G_base`として記録する.
    ///       既にバインド済みの場合は先に`Unbind` (基準姿勢へ復元) する.
    ///       時刻0にキーを持つトラックがある場合、その姿勢をすぐに適用する
    std::vector<ObjectID> Bind(const std::shared_ptr<models::Assembly>& root,
                               AnimationClip clip);

    /// @brief バインドを解除する
    /// @note 全トラックを基準姿勢へ復元し、クリップとバインド情報を破棄する.
    ///       未バインドの場合は何もしない
    void Unbind();

    /// @brief バインド済みか
    bool IsBound() const { return bound_; }

    /// @brief 再生を開始・再開する
    /// @note 終端に到達した後であれば、先頭へ巻き戻してから再生する.
    ///       未バインドの場合は何もしない
    void Play();

    /// @brief 一時停止する (時刻・姿勢を保持)
    /// @note 再生中でない場合は何もしない
    void Pause();

    /// @brief 再生を停止する (時刻0へ戻し、基準姿勢に戻す)
    /// @note 時刻0にキーを持つトラックも復元先は基準姿勢とする
    ///       (停止=アニメーション解除とする). 未バインドの場合は何もしない
    void Stop();

    /// @brief 指定時刻へ移動する (停止・一時停止中でも即時に姿勢を反映する)
    /// @param time_sec 移動先の時刻 [s] ([0, Duration()]へクランプされる)
    /// @note kFinishedからのSeekはkPausedへ遷移する (再度Playできる状態にする).
    ///       未バインドの場合は何もしない
    void Seek(double time_sec);

    /// @brief 時刻を進めて姿勢を適用する (毎フレーム呼ぶ)
    /// @param wall_dt_sec 前回呼び出しからの実経過時間 [s] (非負)
    /// @throw std::invalid_argument wall_dt_secが負の場合
    /// @note 内部時刻はwall_dt_sec×速度だけ進む. 再生中でない場合は何もしない.
    ///       終端到達時はループ設定に応じて先頭へ巻き戻すか、kFinishedへ遷移して
    ///       最終姿勢を保持する
    void Advance(double wall_dt_sec);

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
    /// @brief バインド済みトラックの状態 (クリップのトラックと同順)
    struct BoundTrack {
        /// @brief 対象Assemblyへの弱参照
        /// @note ツリー編集による消失を安全に扱うため弱参照
        std::weak_ptr<models::Assembly> node;
        /// @brief 基準姿勢 (Bind時の大域変換. キー変換はこれへ後掛けする)
        igesio::Matrix4d base_pose = igesio::Matrix4d::Identity();
        /// @brief 最後に適用したキー添字 (nulloptは基準姿勢を適用済み)
        /// @note 現在の有効キーと一致する間は再適用しない (差分適用)
        std::optional<std::size_t> applied_key;
    };

    /// @brief 現在時刻の姿勢を全トラックへ適用する (差分のみ適用)
    /// @note 有効キー添字が前回適用時から変化したトラックのみ
    ///       `SetGlobalTransform`を呼ぶ. 弱参照が消えたトラックはスキップする
    void ApplyPoses();

    /// @brief 全トラックを基準姿勢に戻す
    /// @note キー姿勢を適用済みの (applied_keyが値を持つ) トラックのみ戻す
    void RestoreBasePoses();

    /// @brief バインド中のクリップ
    AnimationClip clip_;
    /// @brief バインド済みトラックの状態 (clip_.Tracks()と同添字で対応)
    std::vector<BoundTrack> bound_tracks_;
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
};

}  // namespace igesio::extensions::animation

#endif  // IGESIO_EXTENSIONS_ANIMATION_ANIMATION_PLAYER_H_
