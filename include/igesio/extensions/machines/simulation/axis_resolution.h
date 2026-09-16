/**
 * @file extensions/machines/simulation/axis_resolution.h
 * @brief 工具軸方向から回転軸の指令への変換
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note 各`ClGoto`の工具軸方向を姿勢IK (`SolveOrientation`) で回転軸の指令に変換し,
 *       `ClGoto::axis_words`に書き込む. G43.4形式 (`TcpStyle::kRotaryWords`)
 *       の出力や、回転軸のNC指令値を表示するGUI用. 直前の指令値の初期化と
 *       回転角の解の選択は動作生成 (`motion.h`) と同じ規則で行う.
 * @note 姿勢IKと直前の指令値の扱いを動作生成 (`motion.h`) と共有するため,
 *       `simulation`に置く.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_SIMULATION_AXIS_RESOLUTION_H_
#define IGESIO_EXTENSIONS_MACHINES_SIMULATION_AXIS_RESOLUTION_H_

#include <optional>
#include <vector>

#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/machine/machine_definition.h"
#include "igesio/extensions/machines/project/setup.h"
#include "igesio/extensions/machines/toolpath/cl_program.h"

namespace igesio::extensions::machines {

/// @brief 回転軸指令値の計算設定
struct AxisResolutionOptions {
    /// @brief 回転角の解の選択方針 (省略時は機械定義の`branch`)
    std::optional<BranchPolicy> branch;
    /// @brief 解決後も`tool_axis`を残す (`false`なら回転軸の指令のみにする)
    bool keep_tool_axis = true;
};

/// @brief CLプログラムの工具軸方向を回転軸の指令に変換する
/// @param setup 加工セットアップ
/// @param program 変換するプログラム
/// @param options 設定
/// @param[out] warnings 到達不能・可動範囲外等の警告 (`context`は`"record N"`.
///        `nullptr`なら報告しない)
/// @return 回転軸の指令を書き込んだプログラム
/// @throw igesio::NotImplementedError 逆運動学が対応しない軸構成の場合
/// @note 対象は制御点を持ち、工具軸方向 (明示または直前の値の継続) が定まり,
///       回転軸の指令をまだ持たない`MotionFrame::kWork`の`ClGoto`.
///       対象外の`ClGoto`も、回転軸の指令があれば直前の指令値に反映する.
///       工具軸方向も回転軸の指令も無いプログラムでは+Z (ワーク座標) を仮定して
///       1つ警告を追加する. `ClArc`は終点の工具軸方向で直前の指令値を更新するだけで
///       レコードは変更しない (円弧の途中は動作生成で補間する).
///       到達不可能な工具軸方向は、警告して直前の回転軸の指令を書き込む
ClProgram ResolveAxisWords(
        const MachiningSetup& setup, const ClProgram& program,
        const AxisResolutionOptions& options = {},
        std::vector<Diagnostic>* warnings = nullptr);

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_SIMULATION_AXIS_RESOLUTION_H_
