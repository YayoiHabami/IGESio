/**
 * @file extensions/machines/simulation/motion_planning.cpp
 * @brief 動作生成の内部構造
 *        (IKの計算が容易な形式への動作レコードの変換、逆運動学、直前の指令値の管理)
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 */
#include "extensions/machines/simulation/motion_planning.h"

#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/extensions/machines/core/formatting.h"
#include "igesio/extensions/machines/core/rotation.h"
#include "igesio/extensions/machines/core/tolerances.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/machine/forward_kinematics.h"
#include "igesio/extensions/machines/machine/inverse_kinematics.h"
#include "igesio/extensions/machines/tools/tool_assembly.h"
#include "igesio/extensions/machines/toolpath/cl_transform.h"

namespace igesio::extensions::machines::detail {

namespace {

/// @brief 文言中の角度/長さの小数桁数
constexpr int kMessageDigits = 3;

/// @brief 軸の指令から機械定義に無い軸名を除く
/// @param model 運動学モデル
/// @param state 作業状態 (未知の軸名は軸名ごとに1件警告する)
/// @param words 軸の指令
/// @param index レコードのインデックス (警告用)
/// @param line 行番号 (警告用)
/// @return 既知の軸名のみの指令
NcValues KnownAxisWords(const MachineModel& model, PlannerState& state,
                        const NcValues& words, const std::size_t index,
                        const int line) {
    NcValues known;
    for (const NcEntry& entry : words.Entries()) {
        if (model.FindAxis(entry.register_name).has_value()) {
            known.Set(entry.register_name, entry.value);
        } else if (state.once.unknown_axes.insert(entry.register_name).second) {
            WarnRecord(state, "axis '" + entry.register_name
                              + "' is not defined in the machine; ignored",
                       index, line);
        }
    }
    return known;
}

/// @brief 工具軸に平行なIK対象の直進軸を探す
/// @param model 運動学モデル
/// @return (軸のインデックス, 工具軸方向との内積の符号). 無ければ`std::nullopt`
std::optional<std::pair<std::size_t, double>>
FindToolAxisParallelLinear(const MachineModel& model) {
    const igesio::Vector3d& t = model.ToolAxisHome();
    for (std::size_t i = 0; i < model.Axes().size(); ++i) {
        const AxisInfo& axis = model.Axes()[i];
        if (axis.kind != AxisKind::kLinear || !axis.IsIkTarget()) continue;
        const double dot = axis.direction_world.dot(t);
        if (std::abs(dot) >= 1.0 - kUnitVectorTolerance) {
            return std::make_pair(i, dot < 0.0 ? -1.0 : 1.0);
        }
    }
    return std::nullopt;
}

/// @brief 登録値相対の座標語を機械のNC指令値にする (登録値と工具長補正を加える)
/// @param setup 加工セットアップ
/// @param state 作業状態
/// @param words 既知の軸名のみの軸の指令 (直進軸を含む)
/// @param index レコードのインデックス
/// @param line 行番号
/// @return 直進軸に登録値を加え、工具軸に平行な直進軸には工具長補正
///         (制御点の`tool_mount`フレーム座標のz成分の符号反転) も加えた指令
/// @throw KinematicsError ワークオフセットが登録値形式でない場合
NcValues RegisteredToMachine(const MachiningSetup& setup, PlannerState& state,
                             const NcValues& words, const std::size_t index,
                             const int line) {
    const MachineModel& model = setup.Model();
    const WorkFrame& frame = CurrentWorkFrame(setup, state, index, line);
    if (!frame.registered.has_value()) {
        throw KinematicsError(
                "record " + std::to_string(index) + ": linear axis words under a "
                "geometric work offset '" + frame.id + "' require registered values");
    }
    const igesio::Vector3d control = ControlLocalFor(setup, state, index, line);
    const std::optional<std::pair<std::size_t, double>> parallel =
            FindToolAxisParallelLinear(model);
    if (!parallel.has_value() && std::abs(control.z()) > kZeroTolerance
        && !state.once.no_parallel_axis) {
        state.once.no_parallel_axis = true;
        WarnRecord(state, "no linear axis is parallel to the tool axis; the tool "
                          "length compensation is not applied to axis words",
                   index, line);
    }

    NcValues machine;
    for (const NcEntry& entry : words.Entries()) {
        const std::size_t axis_index = *model.FindAxis(entry.register_name);
        double value = entry.value;
        if (model.Axes()[axis_index].kind == AxisKind::kLinear) {
            value += frame.registered->GetOr(entry.register_name, 0.0);
            if (parallel.has_value() && parallel->first == axis_index) {
                value -= control.z() * parallel->second;
            }
        }
        machine.Set(entry.register_name, value);
    }
    return machine;
}

/// @brief 直接指令する軸の指令値を正規化し可動範囲を検査する
/// @param model 運動学モデル
/// @param state 作業状態 (回転方向の正規化は軸ごとに1件警告する)
/// @param words 既知の軸名のみの軸の指令
/// @param target 通過点 (警告用のインデックスと行番号)
/// @param[in,out] warnings 可動範囲外の警告 (`context = "limits"`) の追記先
/// @return 無制限の回転軸を直前の指令値に近い回転方向にした指令
NcValues PrepareDirectWords(const MachineModel& model, PlannerState& state,
                            const NcValues& words, const Target& target,
                            std::vector<Diagnostic>& warnings) {
    NcValues prepared;
    for (const NcEntry& entry : words.Entries()) {
        const AxisInfo& axis = model.Axes()[*model.FindAxis(entry.register_name)];
        double value = entry.value;
        if (axis.kind == AxisKind::kRotary) {
            const double base = state.prev_nc.GetOr(entry.register_name, value);
            value = ShortestTurn(axis, entry.value, base);
            if (std::abs(value - entry.value) > kZeroTolerance
                && state.once.normalized_rotaries.insert(entry.register_name).second) {
                WarnRecord(state, "rotary axis " + entry.register_name
                                  + ": the commanded turn exceeds a half turn; "
                                  "the shortest direction is used ("
                                  + FormatDegrees(entry.value, kMessageDigits) + " -> "
                                  + FormatDegrees(value, kMessageDigits) + " deg)",
                           target.record_index, target.line);
            }
        }
        if (!IsWithinLimits(axis, value)) {
            const bool rotary = axis.kind == AxisKind::kRotary;
            const std::string text = rotary ? FormatDegrees(value, kMessageDigits)
                                            : FormatFixed(value, kMessageDigits);
            warnings.push_back(Diagnostic{
                    Severity::kWarning, kLimitsContext,
                    "axis " + entry.register_name + "=" + text
                    + (rotary ? " deg" : " mm") + " is out of range", 0});
        }
        prepared.Set(entry.register_name, value);
    }
    return prepared;
}

/// @brief 到達不能の通過点に直前の姿勢を設定する
/// @param state 作業状態 (直前の姿勢)
/// @param reason 到達不能の理由
/// @param[in,out] result 解 (直前の姿勢、到達不能、警告を設定する)
void KeepPreviousPose(const PlannerState& state, const std::string& reason,
                      PoseResult& result) {
    result.q = state.prev_q;
    result.nc = state.prev_nc;
    result.unreachable = true;
    result.warnings.push_back(Diagnostic{
            Severity::kWarning, "",
            "unreachable; the previous pose is kept: " + reason, 0});
}

/// @brief 位置IKを行い、到達不能なら直前の姿勢を返す
/// @param model 運動学モデル
/// @param state 作業状態
/// @param target 通過点 (制御点を持つ形式)
/// @param rotary 回転軸の指令
/// @param[in,out] result 解 (警告と到達不能を追記する)
void SolvePositionOrKeep(const MachineModel& model, const PlannerState& state,
                         const Target& target, const NcValues& rotary,
                         PoseResult& result) {
    try {
        const IkSolution solution = SolvePosition(
                model, *target.point_home, target.control_home, rotary, state.prev_q);
        result.q = *solution.q;
        result.nc = NcFromJoints(model, result.q);
        result.warnings.insert(result.warnings.end(), solution.warnings.begin(),
                               solution.warnings.end());
    } catch (const KinematicsError& e) {
        KeepPreviousPose(state, e.what(), result);
    }
}

/// @brief 現在の工具と工具長補正から制御点のゼロポーズ機械座標を計算する
/// @param setup 加工セットアップ
/// @param state 作業状態
/// @param index レコードのインデックス (警告用)
/// @param line 行番号 (警告用)
/// @return `ControlLocalFor`の値に`MountPlacement(kToolMount)`を掛けたもの
///         (`Target::control_home`)
igesio::Vector3d ControlHomeFor(
        const MachiningSetup& setup, PlannerState& state,
        const std::size_t index, const int line) {
    return ApplyPoint(setup.Model().MountPlacement(MountKind::kToolMount),
                      ControlLocalFor(setup, state, index, line));
}

/// @brief 直線移動 (`ClGoto`) から通過点を計算する
/// @param setup 加工セットアップ
/// @param state 作業状態
/// @param record 直線移動
/// @param base インデックス、行番号、工具番号、送りを設定済みの通過点
/// @return 通過点
/// @throw KinematicsError 座標語が幾何形式のワークオフセットで指令された場合
Target NormalizeGoto(const MachiningSetup& setup, PlannerState& state,
                     const ClGoto& record, Target base) {
    const MachineModel& model = setup.Model();
    Target target = std::move(base);
    target.kind = record.kind;
    const NcValues words =
            KnownAxisWords(model, state, record.axis_words,
                           target.record_index, target.line);
    if (record.frame == MotionFrame::kMachine) {
        target.form = TargetForm::kAxisWords;
        target.nc_words = words;
        return target;
    }
    if (!record.point.has_value()) {
        target.form = TargetForm::kAxisWords;
        bool has_linear = false;
        for (const NcEntry& entry : words.Entries()) {
            const AxisInfo& axis =
                    model.Axes()[*model.FindAxis(entry.register_name)];
            has_linear = has_linear || axis.kind == AxisKind::kLinear;
        }
        target.nc_words = has_linear
                ? RegisteredToMachine(setup, state, words, target.record_index,
                                      target.line)
                : words;
        return target;
    }

    const WorkFrame& frame =
            CurrentWorkFrame(setup, state, target.record_index, target.line);
    target.control_home =
            ControlHomeFor(setup, state, target.record_index, target.line);
    target.point_home = ApplyPoint(frame.w0, *record.point);
    const std::optional<igesio::Vector3d> axis =
            ToolAxisOrFallback(model, state, record.tool_axis, words,
                               target.record_index, target.line);
    if (axis.has_value()) {
        target.form = TargetForm::kToolAxis;
        target.tool_axis_home = ApplyDirection(frame.w0, *axis);
        return target;
    }
    target.form = TargetForm::kRotaryWords;
    target.nc_words = RotaryWordsOf(model, words);
    return target;
}

/// @brief 円弧 (`ClArc`) を分割点ごとに通過点に変換する
/// @param setup 加工セットアップ
/// @param state 作業状態
/// @param record 円弧
/// @param base インデックス、行番号、工具番号、送りを設定済みの通過点
/// @param chord_tolerance 弦誤差 [mm]
/// @return 分割点ごとの対応する通過点 (最終点のみ`is_command_point`).
///         始点が不明なら空
std::vector<Target> NormalizeArc(
        const MachiningSetup& setup, PlannerState& state,
        const ClArc& record, const Target& base, const double chord_tolerance) {
    if (!state.cl.position.has_value()) {
        WarnRecord(state, "arc without a known start point is skipped",
                   base.record_index, base.line);
        return {};
    }
    const MachineModel& model = setup.Model();
    const WorkFrame& frame =
            CurrentWorkFrame(setup, state, base.record_index, base.line);
    const igesio::Vector3d control_home =
            ControlHomeFor(setup, state, base.record_index, base.line);
    const std::vector<igesio::Vector3d> points =
            DiscretizeArc(*state.cl.position, record, chord_tolerance);
    const std::optional<igesio::Vector3d> from = state.cl.tool_axis;
    const std::optional<igesio::Vector3d> to =
            record.tool_axis.has_value() ? record.tool_axis : from;

    std::vector<Target> targets;
    for (std::size_t k = 0; k < points.size(); ++k) {
        Target target = base;
        target.kind = record.kind;
        target.is_command_point = (k + 1 == points.size());
        target.control_home = control_home;
        target.point_home = ApplyPoint(frame.w0, points[k]);
        std::optional<igesio::Vector3d> axis_work = to;
        if (from.has_value() && to.has_value()) {
            const double t = static_cast<double>(k + 1)
                             / static_cast<double>(points.size());
            axis_work = Slerp(*from, *to, t);
        }
        const std::optional<igesio::Vector3d> axis = ToolAxisOrFallback(
                model, state, axis_work, NcValues{}, base.record_index, base.line);
        if (axis.has_value()) {
            target.form = TargetForm::kToolAxis;
            target.tool_axis_home = ApplyDirection(frame.w0, *axis);
        } else {
            target.form = TargetForm::kRotaryWords;
        }
        targets.push_back(std::move(target));
    }
    return targets;
}

}  // namespace



PlannerState MakePlannerState(const MachiningSetup& setup,
                              std::vector<Diagnostic>* warnings) {
    PlannerState state;
    state.cl.tool = setup.InitialTool();
    state.cl.work_offset = setup.InitialWorkOffset();
    state.prev_q = setup.BaseQ();
    state.prev_nc = NcFromJoints(setup.Model(), state.prev_q);
    state.warnings = warnings;
    return state;
}

void WarnRecord(PlannerState& state, const std::string& message,
                const std::size_t index, const int line) {
    if (state.warnings == nullptr) return;

    state.warnings->push_back(Diagnostic{Severity::kWarning,
                                         "record " + std::to_string(index),
                                         message, line});
}

int LineOf(const ClProgram& program, const std::size_t index) {
    return program.HasSources() ? program.sources[index].line : 0;
}

const WorkFrame& CurrentWorkFrame(const MachiningSetup& setup, PlannerState& state,
                                  const std::size_t index, const int line) {
    const std::string& id = state.cl.work_offset.empty()
                          ? setup.InitialWorkOffset() : state.cl.work_offset;
    if (const WorkFrame* frame = setup.FindWorkFrame(id); frame != nullptr) {
        state.frame = frame;
        return *frame;
    }
    if (state.once.unknown_work_offsets.insert(id).second) {
        WarnRecord(state, "work offset '" + id + "' is not defined; the previous "
                          "work frame is kept", index, line);
    }
    if (state.frame == nullptr) state.frame = &setup.WorkFrames().front();
    return *state.frame;
}

igesio::Vector3d GaugeControlLocal(const std::optional<double> g43_length) {
    return igesio::Vector3d(0.0, 0.0, g43_length.has_value() ? -*g43_length : 0.0);
}

std::optional<igesio::Vector3d> ToolControlLocal(
        const MachiningSetup& setup, const int tool,
        const std::optional<double> g43_length) {
    const auto it = setup.Tools().find(tool);
    if (tool == kNoTool || it == setup.Tools().end()) return std::nullopt;
    return ControlLocal(it->second, g43_length);
}

igesio::Vector3d ControlLocalFor(const MachiningSetup& setup, PlannerState& state,
                                 const std::size_t index, const int line) {
    std::optional<double> g43;
    if (state.cl.length_offset.has_value()) {
        const int number = *state.cl.length_offset;
        const auto it = setup.ToolOffsets().find(number);
        if (it != setup.ToolOffsets().end()) {
            g43 = it->second.length + it->second.length_wear;
        } else if (state.once.unknown_length_offsets.insert(number).second) {
            WarnRecord(state, "length offset #" + std::to_string(number)
                              + " is not defined; ignored", index, line);
        }
    }

    const int tool = state.cl.tool;
    const std::optional<igesio::Vector3d> control = ToolControlLocal(setup, tool, g43);
    if (control.has_value()) return *control;
    if (state.once.unresolved_tools.insert(tool).second) {
        WarnRecord(state, tool == kNoTool
                          ? std::string("no tool is selected; the gauge line is used "
                                        "as the control point")
                          : "tool #" + std::to_string(tool) + " is unresolved; the "
                            "gauge line is used as the control point",
                   index, line);
    }
    return GaugeControlLocal(g43);
}

bool HasRotaryWords(const MachineModel& model, const NcValues& words) {
    for (const NcEntry& entry : words.Entries()) {
        const std::optional<std::size_t> axis = model.FindAxis(entry.register_name);
        if (axis.has_value() && model.Axes()[*axis].kind == AxisKind::kRotary) {
            return true;
        }
    }
    return false;
}

NcValues RotaryWordsOf(const MachineModel& model, const NcValues& words) {
    NcValues rotary;
    for (const NcEntry& entry : words.Entries()) {
        const std::optional<std::size_t> axis = model.FindAxis(entry.register_name);
        if (axis.has_value() && model.Axes()[*axis].kind == AxisKind::kRotary) {
            rotary.Set(entry.register_name, entry.value);
        }
    }
    return rotary;
}

NcValues PrevRotaryWords(const MachineModel& model, const NcValues& prev_nc) {
    NcValues rotary;
    for (const std::size_t axis : model.OrientationAxes()) {
        const std::string& name = model.Axes()[axis].register_name;
        if (prev_nc.Contains(name)) rotary.Set(name, prev_nc.At(name));
    }
    return rotary;
}

std::optional<igesio::Vector3d> ToolAxisOrFallback(
        const MachineModel& model, PlannerState& state,
        const std::optional<igesio::Vector3d>& explicit_axis,
        const NcValues& record_words, const std::size_t index, const int line) {
    if (explicit_axis.has_value()) return explicit_axis;
    if (HasRotaryWords(model, record_words)) return std::nullopt;
    if (state.cl.tool_axis.has_value()) return state.cl.tool_axis;
    if (HasRotaryWords(model, state.cl.axis_words)) return std::nullopt;

    if (!state.once.assumed_tool_axis) {
        state.once.assumed_tool_axis = true;
        WarnRecord(state, "the tool axis is unknown; +Z (work coordinates) is assumed",
                   index, line);
    }
    return igesio::Vector3d::UnitZ();
}

OrientationResult SolveToolAxis(
        const MachineModel& model, const NcValues& prev_nc,
        const igesio::Vector3d& axis_home, const BranchPolicy policy) {
    OrientationResult result;
    try {
        const IkSolution solution =
                SolveOrientation(model, axis_home, prev_nc, policy);
        result.singular = solution.singular;
        for (const Diagnostic& warning : solution.warnings) {
            if (warning.context != kSingularContext) {
                result.warnings.push_back(warning);
            }
        }
        for (const NcEntry& entry : solution.nc.Entries()) {
            const AxisInfo& axis =
                    model.Axes()[*model.FindAxis(entry.register_name)];
            const double base = prev_nc.GetOr(entry.register_name, entry.value);
            result.rotary.Set(entry.register_name,
                              ShortestTurn(axis, entry.value, base));
        }
    } catch (const KinematicsError& e) {
        result.unreachable = e.what();
        result.rotary = PrevRotaryWords(model, prev_nc);
    }
    return result;
}

double ShortestTurn(const AxisInfo& axis, const double raw, const double base) {
    if (axis.kind != AxisKind::kRotary) return raw;

    const bool wide = axis.limits.has_value()
                      && (*axis.limits)[1] - (*axis.limits)[0]
                         >= kFullTurn - kLimitTolerance;
    if (!axis.unlimited && !wide) return raw;

    // 幅が2π以上の制限軸では、正規化後の値が可動範囲外なら指令値のままにする
    const double turned = raw - kFullTurn * std::round((raw - base) / kFullTurn);
    return IsWithinLimits(axis, turned) ? turned : raw;
}

std::vector<Target> NormalizeRecord(
        const MachiningSetup& setup, PlannerState& state,
        const ClProgram& program, const std::size_t index,
        const double chord_tolerance) {
    const ClRecord& record = program.records[index];
    if (!IsMotion(record)) return {};

    Target base;
    base.record_index = index;
    base.line = LineOf(program, index);
    base.tool_number = state.cl.tool;
    base.feed = state.cl.feed;
    if (const auto* dwell = std::get_if<ClDwell>(&record)) {
        base.form = TargetForm::kAxisWords;
        base.dwell_sec = dwell->seconds;
        return {base};
    }
    if (const auto* arc = std::get_if<ClArc>(&record)) {
        return NormalizeArc(setup, state, *arc, base, chord_tolerance);
    }
    return {NormalizeGoto(setup, state, std::get<ClGoto>(record), std::move(base))};
}

PoseResult SolveTarget(const MachiningSetup& setup, PlannerState& state,
                       const Target& target, const BranchPolicy policy) {
    const MachineModel& model = setup.Model();
    PoseResult result;
    if (target.form == TargetForm::kToolAxis) {
        OrientationResult orientation =
                SolveToolAxis(model, state.prev_nc, *target.tool_axis_home, policy);
        result.warnings = std::move(orientation.warnings);
        if (orientation.unreachable.has_value()) {
            KeepPreviousPose(state, *orientation.unreachable, result);
            return result;
        }
        SolvePositionOrKeep(model, state, target, orientation.rotary, result);
        return result;
    }

    const NcValues words = PrepareDirectWords(model, state, target.nc_words,
                                              target, result.warnings);
    if (target.form == TargetForm::kRotaryWords) {
        SolvePositionOrKeep(model, state, target, words, result);
        return result;
    }
    result.q = JointsFromNc(model, words, state.prev_q);
    result.nc = NcFromJoints(model, result.q);
    return result;
}

}  // namespace igesio::extensions::machines::detail
