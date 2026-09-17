/**
 * @file extensions/machines/simulation/animation_bridge.h
 * @brief 動作のサンプル列からのアニメーションクリップの生成
 * @author Yayoi Habami
 * @date 2026-09-16
 * @copyright 2026 Yayoi Habami
 * @note `MotionTrack`の各サンプルを順運動学で各コンポーネントの同次変換F_c(q)に
 *       変換し、`MachineScene`のアセンブリを対象にしたステップ型のクリップ
 *       (animation拡張) を作る. 作るトラックは以下.
 *       (1) 変換トラック: コンポーネントごとに1つ. 動かないコンポーネントには作らない,
 *       (2) 可視性トラック: 工具ごとに1つ (選択中の工具だけを表示する),
 *       (3) イベントトラック: `"tool"` (工具番号)、`"record"` (動作レコードの
 *           インデックス)、`"program"` (`ClProgram::sources`の`program_index`).
 * @note `AnimationPlayer::Bind`では各アセンブリの現在の大域変換と可視性を基準として
 *       記録する. クリップは基準がゼロポーズ (全コンポーネントが単位行列)
 *       である前提で作るため、`MakeMachineClip`の前と`Bind`の前に
 *       `MachineScene::ResetToZeroPose()`と`SetActiveTool(初期工具)`を呼ぶこと.
 *       `Stop`はこの状態に戻る.
 * @note 同時刻のサンプル (所要時間0の区間) は最後の値だけをキーにする. クリップの
 *       総時間は`MotionStats::duration_sec`で固定する.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_SIMULATION_ANIMATION_BRIDGE_H_
#define IGESIO_EXTENSIONS_MACHINES_SIMULATION_ANIMATION_BRIDGE_H_

#include <string_view>
#include <vector>

#include "igesio/extensions/animation/animation_clip.h"
#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/core/tolerances.h"
#include "igesio/extensions/machines/scene/machine_scene.h"
#include "igesio/extensions/machines/simulation/motion.h"
#include "igesio/extensions/machines/toolpath/cl_program.h"

namespace igesio::extensions::machines {

/// @brief 工具番号のイベントトラックの名前
constexpr std::string_view kToolEventTrack = "tool";
/// @brief 動作レコードのインデックスのイベントトラックの名前
constexpr std::string_view kRecordEventTrack = "record";
/// @brief `[[program]]`のインデックスのイベントトラックの名前
constexpr std::string_view kProgramEventTrack = "program";

/// @brief クリップ生成の設定
struct ClipBuildOptions {
    /// @brief 直前のキーと同じ (許容誤差内) 変換・可視性のキーを省略するか
    /// @note イベントは設定によらず値が変わったときだけキーにする
    bool skip_unchanged = true;
    /// @brief 変換の同一判定の許容誤差 (行列の成分の差の最大)
    double tolerance = kZeroTolerance;
    /// @brief `"record"`イベントを作るか
    bool emit_record_events = true;
};

/// @brief 動作のサンプル列からシーンのアニメーションクリップを作る
/// @param scene 構築済みのシーン (対象のアセンブリのIDと運動学モデルを用いる)
/// @param track 動作のサンプル列
/// @param program `track`の元のCLプログラム (`"program"`イベントの`sources`に使用)
/// @param options 設定
/// @param[out] warnings 警告の追加先 (`nullptr`なら追加しない)
/// @return クリップ (総時間は`track.stats.duration_sec`)
/// @throw std::invalid_argument `scene`が未構築、`track.samples`が空,
///        またはサンプルの軸変位量の長さが軸数と異なる場合
/// @note `program.HasSources()`でなければ`"program"`イベントは作らない.
///       工具表に無い工具番号は番号ごとに1回警告し、その区間と`kNoTool`の区間では
///       全工具を非表示にする
animation::AnimationClip MakeMachineClip(
        const MachineScene& scene, const MotionTrack& track,
        const ClProgram& program, const ClipBuildOptions& options = {},
        std::vector<Diagnostic>* warnings = nullptr);

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_SIMULATION_ANIMATION_BRIDGE_H_
