/**
 * @file examples/gui/animation_viewer_gui.h
 * @brief キーフレームアニメーション再生用GUI (ビューアの派生)
 * @author Yayoi Habami
 * @date 2026-09-02
 * @copyright 2026 Yayoi Habami
 * @note 基底クラスの挙動は変更しない. アニメーション拡張 (extensions/animation)
 *       の実用例として、読み込んだモデルの子Assemblyを対象にデモクリップを構築し、
 *       再生・一時停止・シーク・速度変更・ループを行うパネルを提供する.
 *       再生中もカメラ操作 (パン/ズーム/回転) は通常どおり行える.
 */
#ifndef EXAMPLES_GUI_ANIMATION_VIEWER_GUI_H_
#define EXAMPLES_GUI_ANIMATION_VIEWER_GUI_H_

#ifdef IGESIO_ANIMATION_EXTENSION_ENABLED

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "./iges_viewer_gui.h"
#ifdef IGESIO_INSPECTION_EXTENSION_ENABLED
#include "./surface_algorithm_verifier_gui.h"
#endif  // IGESIO_INSPECTION_EXTENSION_ENABLED

#include <igesio/models/assembly.h>
#include <igesio/extensions/animation.h>



namespace igesio::graphics {

#ifdef IGESIO_INSPECTION_EXTENSION_ENABLED
/// @brief AnimationViewerGUIの基底 (検証ツール付きGUIへ積み上げる)
using AnimationViewerBase = SurfaceAlgorithmVerifierGUI;
#else
/// @brief AnimationViewerGUIの基底 (検証拡張が無効の場合は素のビューア)
using AnimationViewerBase = IgesViewerGUI;
#endif  // IGESIO_INSPECTION_EXTENSION_ENABLED

/// @brief デモクリップの運動軸 (DemoClipParams::move_axis・rotation_axis)
/// @note ImGui::Comboのint*引数へそのまま渡すため、スコープなし列挙とする
enum DemoAxis {
    /// @brief X軸
    kDemoAxisX = 0,
    /// @brief Y軸
    kDemoAxisY = 1,
    /// @brief Z軸
    kDemoAxisZ = 2,
};

/// @brief デモクリップの回転中心 (DemoClipParams::rotation_center)
/// @note ImGui::Comboのint*引数へそのまま渡すため、スコープなし列挙とする
enum DemoRotationCenter {
    /// @brief 対象Assembly自身のワールドBBoxの中心
    kDemoCenterOwnBox = 0,
    /// @brief モデル全体のワールドBBoxの中心
    kDemoCenterModelBox = 1,
    /// @brief ワールド原点
    kDemoCenterOrigin = 2,
};

/// @brief デモクリップの生成パラメータ
/// @note 基準姿勢から終端姿勢 (総移動量+総回転角) へ向かう1本の剛体運動を
///       `steps`分割してキー化する. キーフレームはステップ型 (補間なし) のため、
///       分割数を上げるほど連続的な運動に見える.
struct DemoClipParams {
    /// @brief 総移動量 (ワールドBBoxの最大サイズに対する比率)
    float span_ratio = 0.3f;
    /// @brief 移動方向の軸
    int move_axis = kDemoAxisX;
    /// @brief 総回転角 [deg]
    float angle_deg = 90.0f;
    /// @brief 回転軸
    int rotation_axis = kDemoAxisZ;
    /// @brief 回転中心
    int rotation_center = kDemoCenterOwnBox;
    /// @brief 子Assembly毎に運動の向きを交互に反転するか
    bool alternate_direction = true;
    /// @brief 復路 (終端姿勢から基準姿勢へ戻る運動) を続けて追加するか
    bool ping_pong = false;
    /// @brief 片道到着後に対象を非表示にするか (可視性トラックの動作確認用)
    /// @note trueのとき各対象に、片道到着時刻でfalse、復路終了 (ping_pong時)
    ///       または保持終了でtrueの可視性キーを打つ
    bool hide_after_travel = false;
    /// @brief 片道の分割数 (1で分割なし=一気に動く)
    int steps = 20;
    /// @brief 最初のキーまでの待機時間 [s]
    float start_delay_sec = 0.5f;
    /// @brief 片道の所要時間 [s]
    float travel_sec = 2.0f;
    /// @brief 子Assembly毎の開始時刻のずらし量 [s]
    float stagger_sec = 0.5f;
    /// @brief 最終姿勢の保持時間 [s]
    float hold_sec = 1.0f;
    /// @brief 選択中のAssemblyのみを対象にするか
    bool selected_only = false;
};

/// @brief キーフレームアニメーション再生用GUI
/// @note 「Animation」メニューからパネルを開き、読み込み済みモデルの子Assemblyを
///       対象にデモクリップを構築 (Bind) して再生する. 再生の時間駆動は
///       OnFrameUpdateフック+連続描画モードで行う.
class AnimationViewerGUI : public AnimationViewerBase {
 public:
    /// @brief コンストラクタ
    /// @param width ウィンドウ幅の初期値 [px]
    /// @param height ウィンドウ高さの初期値 [px]
    /// @param msaa_samples マルチサンプリングのサンプル数 (0で無効)
    /// @param initial_file 起動時に読み込むファイル (空で無し)
    /// @throw std::runtime_error ウィンドウの初期化に失敗した場合
    explicit AnimationViewerGUI(int width = kDefaultDisplayWidth,
                                int height = kDefaultDisplayHeight,
                                int msaa_samples = 0,
                                const std::string& initial_file = "");

 protected:
    /// @brief 「Animation」メニューを追加する (基底のメニューも維持する)
    void RenderExtraMenus() override;
    /// @brief アニメーションパネルを描画する (平時非表示)
    void RenderExtraWindows() override;
    /// @brief 再生中の時刻前進と連続描画モードの切り替えを行う
    /// @param dt_sec 前フレームからの実経過時間 [s]
    void OnFrameUpdate(double dt_sec) override;

 private:
    /// @brief アニメーションパネル本体を描画する
    void RenderAnimationWindow();
    /// @brief 再生操作部 (Play/Pause/Stop・シーク・速度・ループ) を描画する
    /// @note 末尾に"stage"イベントトラックの現在値と、直近の時刻変化
    ///       (`TakeTimeChange`の結果) を表示する
    void RenderPlaybackControls();
    /// @brief デモクリップの生成パラメータの編集部を描画する
    /// @note 末尾に生成結果 (対象数・キー数・総時間) のプレビューを表示する
    void RenderDemoSettings();
    /// @brief トラック一覧を描画する
    /// @note 変換・可視性トラックは対象Assembly名+キー数+時刻範囲、
    ///       イベントトラックは名前+キー数を示す
    void RenderTrackList();
    /// @brief アニメーション対象の子Assemblyを収集する
    /// @return root直下の子Assembly. `selected_only`が有効な場合は、
    ///         選択中の要素を含むもののみ
    std::vector<std::shared_ptr<models::Assembly>> CollectTargets();
    /// @brief 読み込み済みの子Assemblyからデモクリップを構築してBindする
    /// @note 子Assembly毎に、基準姿勢から終端姿勢へ向かう剛体運動を
    ///       `DemoClipParams::steps`分割したキー列を生成する. `hide_after_travel`
    ///       有効時は可視性キーも打つ. 先頭対象の時刻を代表として"stage"イベント
    ///       トラック (0=待機, 1=往路, 2=復路, 3=保持) を打つ.
    ///       バインド済みの場合は再構築 (再生中だった場合は先頭から再生を継続)
    void BuildAndBindDemoClip();
    /// @brief アニメーションを解除する (基準姿勢へ復元)
    void ReleaseAnimation();

    /// @brief アニメーションの再生状態機械
    extensions::animation::AnimationPlayer player_;
    /// @brief アニメーションパネルを表示中か
    bool anim_window_open_ = false;
    /// @brief デモクリップの生成パラメータ
    DemoClipParams demo_params_;
    /// @brief 再生速度のUI値 (SliderFloat用)
    float speed_ui_ = 1.0f;
    /// @brief ループ再生のUI値 (Checkbox用)
    bool loop_ui_ = false;
    /// @brief 直近の操作の結果メッセージ
    std::string anim_status_;
    /// @brief 直近に時刻が動いたときの`TakeTimeChange`の結果 (未取得なら無し)
    /// @note 毎フレーム`Advance`の後に`TakeTimeChange`を呼び、changedのものを保持
    std::optional<extensions::animation::TimeChange> last_time_change_;
};

}  // namespace igesio::graphics

#endif  // IGESIO_ANIMATION_EXTENSION_ENABLED

#endif  // EXAMPLES_GUI_ANIMATION_VIEWER_GUI_H_
