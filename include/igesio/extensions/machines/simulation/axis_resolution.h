/**
 * @file extensions/machines/simulation/axis_resolution.h
 * @brief 工具軸方向から回転軸の指令への変換、および1点の制御点と工具軸方向からの
 *        軸変位量の計算
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note 各`ClGoto`の工具軸方向を姿勢IK (`SolveOrientation`) で回転軸の指令に変換し,
 *       `ClGoto::axis_words`に書き込む. G43.4形式 (`TcpStyle::kRotaryWords`)
 *       の出力や、回転軸のNC指令値を表示するGUI用. 直前の指令値の初期化と
 *       回転角の解の選択は動作生成 (`motion.h`) と同じ規則で行う.
 * @note `SolveClTarget`は`ClGoto`1レコード相当 (制御点+工具軸方向) を単独で
 *       逆運動学にかける. ジョグ、任意の点への工具の配置、GUI側で指定した工具姿勢の
 *       表示 (`MachineScene::ApplyPose`) などに用いる.
 * @note 姿勢IKと直前の指令値の扱いを動作生成 (`motion.h`) と共有するため,
 *       `simulation`に置く.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_SIMULATION_AXIS_RESOLUTION_H_
#define IGESIO_EXTENSIONS_MACHINES_SIMULATION_AXIS_RESOLUTION_H_

#include <optional>
#include <string>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/machine/axis_values.h"
#include "igesio/extensions/machines/machine/inverse_kinematics.h"
#include "igesio/extensions/machines/machine/machine_definition.h"
#include "igesio/extensions/machines/project/setup.h"
#include "igesio/extensions/machines/tools/tool_assembly.h"
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

/// @brief 1点の制御点と工具軸方向 (`SolveClTarget`の入力)
/// @note `ClGoto`のうち逆運動学に必要な項目と、制御点を決める工具の状態
///       (`ClState::tool`/工具長補正) をまとめたもの. 座標系はワーク座標
struct ClTarget {
    /// @brief 制御点 (ワーク座標) [mm]
    igesio::Vector3d point = igesio::Vector3d::Zero();
    /// @brief 工具軸方向 (ワーク座標. 内部で正規化する)
    igesio::Vector3d tool_axis = igesio::Vector3d::UnitZ();
    /// @brief ワークオフセットid
    /// @note 空なら`MachiningSetup::InitialWorkOffset()`
    std::string work_offset;
    /// @brief 工具番号 (`kNoTool`なら工具なし)
    int tool = kNoTool;
    /// @brief 有効な工具長補正 [mm] (無ければ`std::nullopt`)
    std::optional<double> g43_length;
};

/// @brief ワーク座標の制御点と工具軸方向から全軸の軸変位量を計算する
/// @param setup 加工セットアップ
/// @param target 制御点と工具軸方向
/// @param prev_q 直前の姿勢 (全軸の軸変位量. 回転角の解の選択、無制限回転軸の
///        回転方向、解かない軸の値に用いる)
/// @param branch 回転角の解の選択方針 (省略時は機械定義の`branch`)
/// @param[out] warnings 警告の追加先 (`nullptr`なら追加しない). 逆運動学の
///        警告は`context`が種別 (`"limits"`)、工具の未解決と到達不能は空
/// @return 解 (`nc`は全軸のNC指令値、`q`は全軸の軸変位量、`singular`、`error`,
///         `warnings`は逆運動学の警告). 到達不能なら`std::nullopt`
/// @throw std::invalid_argument `work_offset`がワーク座標系に無い,
///        `tool_axis`がゼロベクトル、または`prev_q`の長さが軸数と異なる場合
/// @throw igesio::NotImplementedError 逆運動学が対応しない軸構成の場合
/// @note 制御点は動作生成と同じ規則で決める. 工具表にある工具なら
///       `ControlLocal(spec, g43_length)`、工具なし (`kNoTool`) ならゲージライン,
///       工具表に無い番号なら警告してゲージライン. 無制限の回転軸は`prev_q`に
///       近い回転方向にする. 可動範囲外は警告のみで解を返す
std::optional<IkSolution> SolveClTarget(
        const MachiningSetup& setup, const ClTarget& target,
        const JointVector& prev_q,
        const std::optional<BranchPolicy>& branch = std::nullopt,
        std::vector<Diagnostic>* warnings = nullptr);

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_SIMULATION_AXIS_RESOLUTION_H_
