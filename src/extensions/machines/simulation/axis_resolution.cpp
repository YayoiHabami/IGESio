/**
 * @file extensions/machines/simulation/axis_resolution.cpp
 * @brief 工具軸方向から回転軸の指令への変換
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/machines/simulation/axis_resolution.h"

#include <cstddef>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/rotation.h"
#include "igesio/extensions/machines/machine/machine_model.h"
#include "extensions/machines/simulation/motion_planning.h"

namespace igesio::extensions::machines {

namespace {

/// @brief 姿勢IKの警告と到達不能をレコードの警告として記録する
/// @param state 作業状態 (警告の格納先)
/// @param result 姿勢IKの結果
/// @param index レコードのインデックス
/// @param line 行番号
void RecordOrientationWarnings(detail::PlannerState& state,
                               const detail::OrientationResult& result,
                               const std::size_t index, const int line) {
    for (const Diagnostic& warning : result.warnings) {
        detail::WarnRecord(state, warning.message, index, line);
    }
    if (result.unreachable.has_value()) {
        detail::WarnRecord(state, "unreachable; the previous rotary words are kept: "
                                  + *result.unreachable, index, line);
    }
}

/// @brief ワーク座標の工具軸方向を姿勢IKで回転軸の指令にし、直前の指令値を更新する
/// @param setup 加工セットアップ
/// @param state 作業状態 (`prev_nc`を更新する)
/// @param axis_work 工具軸方向 (ワーク座標)
/// @param policy 回転角の解の選択方針
/// @param index レコードのインデックス
/// @param line 行番号
/// @return 回転軸の指令 (到達不能なら直前の値)
NcValues ResolveToolAxis(const MachiningSetup& setup, detail::PlannerState& state,
                         const igesio::Vector3d& axis_work, const BranchPolicy policy,
                         const std::size_t index, const int line) {
    const WorkFrame& frame = detail::CurrentWorkFrame(setup, state, index, line);
    const detail::OrientationResult result = detail::SolveToolAxis(
            setup.Model(), state.prev_nc, ApplyDirection(frame.w0, axis_work), policy);
    RecordOrientationWarnings(state, result, index, line);
    state.prev_nc.Merge(result.rotary);
    return result.rotary;
}

/// @brief 直線移動の工具軸方向を回転軸の指令に変換する
/// @param setup 加工セットアップ
/// @param state 作業状態
/// @param options 設定
/// @param policy 回転角の解の選択方針
/// @param index レコードのインデックス
/// @param line 行番号
/// @param[in,out] motion 変換する直線移動
/// @note 対象外 (機械座標、制御点なし、回転軸の指令あり) なら変更せず、回転軸の
///       指令があれば直前の指令値に反映する. 工具軸方向が無く直前の回転軸の指令を
///       引き継ぐ場合は、その値を書き込む
void ResolveGoto(const MachiningSetup& setup, detail::PlannerState& state,
                 const AxisResolutionOptions& options, const BranchPolicy policy,
                 const std::size_t index, const int line, ClGoto& motion) {
    const MachineModel& model = setup.Model();
    if (motion.frame == MotionFrame::kMachine || !motion.point.has_value()
        || detail::HasRotaryWords(model, motion.axis_words)) {
        // 動作生成と同じく、明示された回転軸の指令は後続の解の選択の基準にする
        state.prev_nc.Merge(detail::RotaryWordsOf(model, motion.axis_words));
        return;
    }

    const std::optional<igesio::Vector3d> axis = detail::ToolAxisOrFallback(
            model, state, motion.tool_axis, motion.axis_words, index, line);
    const NcValues rotary = axis.has_value()
            ? ResolveToolAxis(setup, state, *axis, policy, index, line)
            : detail::PrevRotaryWords(model, state.prev_nc);
    motion.axis_words.Merge(rotary);
    if (!options.keep_tool_axis) motion.tool_axis = std::nullopt;
}

}  // namespace



ClProgram ResolveAxisWords(const MachiningSetup& setup, const ClProgram& program,
                           const AxisResolutionOptions& options,
                           std::vector<Diagnostic>* warnings) {
    const BranchPolicy policy =
            options.branch.value_or(setup.Model().Definition().branch);
    ClProgram resolved = program;
    detail::PlannerState state = detail::MakePlannerState(setup, warnings);
    for (std::size_t i = 0; i < resolved.records.size(); ++i) {
        ClRecord& record = resolved.records[i];
        const int line = detail::LineOf(program, i);
        if (auto* motion = std::get_if<ClGoto>(&record)) {
            ResolveGoto(setup, state, options, policy, i, line, *motion);
        } else if (const auto* arc = std::get_if<ClArc>(&record)) {
            // 円弧は終点の工具軸方向で直前の指令値を更新するだけ (途中は動作生成で補間)
            const std::optional<igesio::Vector3d> axis = detail::ToolAxisOrFallback(
                    setup.Model(), state, arc->tool_axis, NcValues{}, i, line);
            if (axis.has_value()) {
                ResolveToolAxis(setup, state, *axis, policy, i, line);
            }
        }
        state.cl.Apply(program.records[i]);
    }
    return resolved;
}

}  // namespace igesio::extensions::machines
