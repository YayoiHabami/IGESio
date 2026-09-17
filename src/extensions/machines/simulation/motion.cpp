/**
 * @file extensions/machines/simulation/motion.cpp
 * @brief 動作生成
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note 処理は2段階. はじめに全ての通過点（機械座標系）を正規化して逆運動学を解き,
 *       区間ごとの移動時間を決める. 総サンプル数が上限を超える場合はfpsを下げてから
 *       次の段階で区間ごとに補間してサンプリング点列を作る. 通過点の正規化と逆運動学は
 *       `motion_planning.h`に委譲する.
 */
#include "igesio/extensions/machines/simulation/motion.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "igesio/extensions/machines/core/formatting.h"
#include "igesio/extensions/machines/core/rotation.h"
#include "igesio/extensions/machines/core/tolerances.h"
#include "igesio/extensions/machines/machine/forward_kinematics.h"
#include "igesio/extensions/machines/machine/inverse_kinematics.h"
#include "extensions/machines/simulation/motion_planning.h"

namespace igesio::extensions::machines {

namespace {

/// @brief 順運動学による自己検証を行う通過点の間隔
constexpr std::size_t kCheckInterval = 100;

/// @brief fpsの下限 (これ未満に下がる場合は補間しない)
constexpr double kMinimumFps = 1e-6;

/// @brief 第1段階で計画した通過点 (区間の終点)
struct PlannedTarget {
    /// @brief 通過点
    detail::Target target;
    /// @brief 終点の軸変位量
    JointVector q;
    /// @brief 補間する回転軸の指令 (制御点を持つ形式のみ)
    /// @note `kToolAxis`では姿勢IKで求めた軸、`kRotaryWords`では指令した軸
    NcValues rotary_end;
    /// @brief 区間時間 [s]
    double duration = 0.0;
    /// @brief 警告のインデックス (`MotionTrack::warnings`)
    std::optional<std::size_t> warning;
    /// @brief 到達不能で直前の姿勢を保持したか
    bool unreachable = false;
};

/// @brief 設定の値を検証する
/// @param options 設定
/// @throw std::invalid_argument `fps`/`fallback_feed`/`arc_chord_tolerance`/
///        `fixed_record_seconds`が正でない, または`max_samples`が0の場合
void ValidateOptions(const MotionOptions& options) {
    if (!(options.fps > 0.0)) {
        throw std::invalid_argument("MotionOptions::fps must be positive");
    }
    if (options.max_samples == 0) {
        throw std::invalid_argument("MotionOptions::max_samples must be positive");
    }
    if (!(options.fallback_feed > 0.0)) {
        throw std::invalid_argument("MotionOptions::fallback_feed must be positive");
    }
    if (!(options.arc_chord_tolerance > 0.0)) {
        throw std::invalid_argument(
                "MotionOptions::arc_chord_tolerance must be positive");
    }
    if (options.fixed_record_seconds.has_value()
        && !(*options.fixed_record_seconds > 0.0)) {
        throw std::invalid_argument(
                "MotionOptions::fixed_record_seconds must be positive");
    }
}

/// @brief 制御点のゼロポーズ機械座標 (`work_mount`基準) を順運動学で計算する
/// @param model 運動学モデル
/// @param q 全軸の軸変位量
/// @param control_home 制御点のゼロポーズ機械座標 (`tool_mount`に固定)
/// @return `F_wm(q)⁻¹ F_tm(q) c`. 位置IKの目標点と同じ座標系
igesio::Vector3d ControlPointHome(const MachineModel& model, const JointVector& q,
                                  const igesio::Vector3d& control_home) {
    const std::vector<igesio::Matrix4d> frames = Forward(model, q);
    return ApplyPoint(RigidInverse(frames[model.WorkMountIndex()]),
                      ApplyPoint(frames[model.ToolMountIndex()], control_home));
}

/// @brief 可動範囲外の扱いを適用し、通過点の警告を1件に連結して記録する
/// @param solved 逆運動学と直接指令の警告 (`context`は種別)
/// @param policy 可動範囲外の扱い
/// @param target 通過点 (インデックスと行番号)
/// @param[in,out] track 警告と統計の格納先
/// @return 記録した警告のインデックス. 警告が無ければ`std::nullopt`
/// @throw KinematicsError `kError`で可動範囲外の警告がある場合
std::optional<std::size_t> RecordTargetWarnings(
        const std::vector<Diagnostic>& solved, const OvertravelPolicy policy,
        const detail::Target& target, MotionTrack& track) {
    std::string message;
    for (const Diagnostic& warning : solved) {
        if (warning.context == detail::kLimitsContext) {
            if (policy == OvertravelPolicy::kError) {
                const std::string where = target.line > 0
                        ? " (line " + std::to_string(target.line) + ")"
                        : std::string();
                throw KinematicsError("record " + std::to_string(target.record_index)
                                      + where + ": " + warning.message);
            }
            if (policy == OvertravelPolicy::kIgnore) continue;
            ++track.stats.overtravel_count;
        }
        if (!message.empty()) message += "; ";
        message += warning.message;
    }
    if (message.empty()) return std::nullopt;

    track.warnings.push_back(Diagnostic{
            Severity::kWarning, "record " + std::to_string(target.record_index),
            message, target.line});
    return track.warnings.size() - 1;
}

/// @brief 区間の制御点の移動距離を計算する
/// @param model 運動学モデル
/// @param state 作業状態 (直前の姿勢)
/// @param target 通過点
/// @param nc_end 終点の全軸のNC指令値
/// @return 制御点を持つ形式では制御点の移動距離、直接指令では直進軸の指令値の
///         差のノルム [mm]
double SegmentLength(const MachineModel& model, const detail::PlannerState& state,
                     const detail::Target& target, const NcValues& nc_end) {
    if (target.point_home.has_value()) {
        return (*target.point_home
                - ControlPointHome(model, state.prev_q, target.control_home)).norm();
    }
    double squared = 0.0;
    for (const AxisInfo& axis : model.Axes()) {
        if (axis.kind != AxisKind::kLinear) continue;
        const double from = state.prev_nc.GetOr(axis.register_name, 0.0);
        const double delta = nc_end.GetOr(axis.register_name, from) - from;
        squared += delta * delta;
    }
    return std::sqrt(squared);
}

/// @brief 区間時間を計算する
/// @param model 運動学モデル
/// @param state 作業状態 (直前の姿勢. `fallback_feed`の情報を1件報告する)
/// @param target 通過点
/// @param q_end 終点の軸変位量
/// @param length 制御点の移動距離 [mm]
/// @param options 設定
/// @return 軸ごとの`|Δq| / v`の最大と、切削なら`L / F`との最大 [s].
///         ドウェルは`dwell_sec`. 軸の動特性が無い軸は無視する.
///         `fixed_record_seconds`の指定時は通過点の種類によらずその値
double SegmentDuration(const MachineModel& model, detail::PlannerState& state,
                       const detail::Target& target, const JointVector& q_end,
                       const double length, const MotionOptions& options) {
    if (options.fixed_record_seconds.has_value()) return *options.fixed_record_seconds;
    if (target.dwell_sec > 0.0) return target.dwell_sec;

    const bool rapid = target.kind == MotionKind::kRapid;
    double duration = 0.0;
    for (std::size_t i = 0; i < model.Axes().size(); ++i) {
        const AxisDynamics& dynamics = model.Axes()[i].dynamics;
        const std::optional<double> speed = rapid ? dynamics.rapid_feed
                                                  : dynamics.max_feed;
        if (!speed.has_value() || !(*speed > 0.0)) continue;
        duration = std::max(duration, std::abs(q_end[i] - state.prev_q[i]) / *speed);
    }
    if (rapid || !(length > 0.0)) return duration;

    double feed = target.feed.value_or(0.0);
    if (!(feed > 0.0)) {
        feed = options.fallback_feed;
        if (!state.once.fallback_feed && state.warnings != nullptr) {
            state.once.fallback_feed = true;
            state.warnings->push_back(Diagnostic{
                    Severity::kInfo, "record " + std::to_string(target.record_index),
                    "feed is unknown; the fallback feed "
                    + FormatFixed(options.fallback_feed * kSecondsPerMinute, 0)
                    + " mm/min is used", target.line});
        }
    }
    return std::max(duration, length / feed);
}

/// @brief 順運動学で解を自己検証し、統計を更新する
/// @param model 運動学モデル
/// @param planned 計画した通過点
/// @param[in,out] stats 統計 (検証回数と最大誤差)
/// @note `kToolAxis`は工具軸方向と制御点、`kRotaryWords`は制御点のみ検証する.
///       直接指令とドウェルは検証しない
void CheckPlanned(const MachineModel& model, const PlannedTarget& planned,
                  MotionStats& stats) {
    const detail::Target& target = planned.target;
    if (!target.point_home.has_value() || planned.unreachable) return;

    SolutionError error;
    if (target.form == detail::TargetForm::kToolAxis) {
        error = CheckSolution(model, planned.q, *target.tool_axis_home,
                              *target.point_home, target.control_home);
    } else {
        error.position = (ControlPointHome(model, planned.q, target.control_home)
                          - *target.point_home).norm();
    }
    ++stats.check_count;
    stats.max_angle_error = std::max(stats.max_angle_error, error.angle);
    stats.max_position_error = std::max(stats.max_position_error, error.position);
}

/// @brief 補間する回転軸の指令を取り出す
/// @param model 運動学モデル
/// @param target 通過点
/// @param nc_end 終点の全軸のNC指令値
/// @return `kToolAxis`では姿勢IKの対象軸、`kRotaryWords`では指令した回転軸の
///         終点の値. 直接指令では空
NcValues RotaryEndOf(const MachineModel& model, const detail::Target& target,
                     const NcValues& nc_end) {
    NcValues rotary;
    if (target.form == detail::TargetForm::kToolAxis) {
        for (const std::size_t axis : model.OrientationAxes()) {
            const std::string& name = model.Axes()[axis].register_name;
            rotary.Set(name, nc_end.At(name));
        }
    } else if (target.form == detail::TargetForm::kRotaryWords) {
        for (const NcEntry& entry : target.nc_words.Entries()) {
            rotary.Set(entry.register_name, nc_end.At(entry.register_name));
        }
    }
    return rotary;
}

/// @brief 1つの通過点の逆運動学と区間時間を計画する (第1段階の1ステップ)
/// @param setup 加工セットアップ
/// @param state 作業状態 (直前の姿勢を更新する)
/// @param target 通過点
/// @param policy 回転角の解の選択方針
/// @param overtravel 可動範囲外の扱い
/// @param options 設定
/// @param[in,out] track 警告と統計の格納先
/// @return 計画した通過点
PlannedTarget PlanTarget(const MachiningSetup& setup, detail::PlannerState& state,
                         const detail::Target& target, const BranchPolicy policy,
                         const OvertravelPolicy overtravel,
                         const MotionOptions& options, MotionTrack& track) {
    const MachineModel& model = setup.Model();
    const detail::PoseResult pose = detail::SolveTarget(setup, state, target, policy);
    PlannedTarget planned;
    planned.target = target;
    planned.q = pose.q;
    planned.unreachable = pose.unreachable;
    planned.warning = RecordTargetWarnings(pose.warnings, overtravel, target, track);
    if (pose.unreachable) ++track.stats.unreachable_count;
    planned.rotary_end = RotaryEndOf(model, target, pose.nc);
    const double length = SegmentLength(model, state, target, pose.nc);
    planned.duration = SegmentDuration(model, state, target, pose.q, length, options);
    state.prev_q = pose.q;
    state.prev_nc = pose.nc;
    return planned;
}

/// @brief 通過点の分割数を計算する
/// @param planned 計画した通過点
/// @param fps サンプリングレート (補間しないなら`std::nullopt`)
/// @return `max(1, ⌈T fps⌉)`. ドウェルと補間なしは1
std::size_t DivisionCount(const PlannedTarget& planned,
                          const std::optional<double> fps) {
    if (!fps.has_value() || planned.target.dwell_sec > 0.0) return 1;

    const double count = std::ceil(planned.duration * *fps);
    return static_cast<std::size_t>(std::max(1.0, count));
}

/// @brief 総サンプリング点数を見積もる
/// @param planned 計画した通過点の列
/// @param fps サンプリングレート (補間しないなら`std::nullopt`)
/// @return 初期姿勢のサンプリング点を含む総数
std::size_t TotalSamples(const std::vector<PlannedTarget>& planned,
                         const std::optional<double> fps) {
    std::size_t total = 1;
    for (const PlannedTarget& target : planned) total += DivisionCount(target, fps);
    return total;
}

/// @brief 総サンプリング点数が上限に収まるfpsを決める
/// @param planned 計画した通過点の列
/// @param options 設定
/// @param[in,out] track 統計 (`fps`/`fps_reduced`) と情報診断の格納先
/// @return 補間に用いるfps. 補間しない (設定、または補間なしでも上限を超える)
///         なら`std::nullopt`
std::optional<double> ResolveFps(const std::vector<PlannedTarget>& planned,
                                 const MotionOptions& options, MotionTrack& track) {
    track.stats.fps = options.fps;
    if (!options.interpolate) return std::nullopt;

    const std::size_t max_samples = options.max_samples;
    const std::size_t base = TotalSamples(planned, std::nullopt);
    double fps = options.fps;
    std::size_t total = TotalSamples(planned, fps);
    if (total <= max_samples) return fps;
    if (base >= max_samples) {
        track.warnings.push_back(Diagnostic{
                Severity::kInfo, "",
                "the number of command points (" + std::to_string(base)
                + ") reaches max_samples (" + std::to_string(max_samples)
                + "); interpolation is disabled", 0});
        track.stats.fps_reduced = true;
        return std::nullopt;
    }

    // ⌈T fps⌉の和は fps に対して単調なので、超過分の比で縮めて収束するまで繰り返す
    while (total > max_samples && fps > kMinimumFps) {
        fps *= static_cast<double>(max_samples - base)
               / static_cast<double>(total - base);
        total = TotalSamples(planned, fps);
    }
    track.stats.fps = fps;
    track.stats.fps_reduced = true;
    track.warnings.push_back(Diagnostic{
            Severity::kInfo, "",
            "fps reduced from " + FormatFixed(options.fps, 3) + " to "
            + FormatFixed(fps, 3) + " to keep the samples within max_samples ("
            + std::to_string(max_samples) + ")", 0});
    if (fps <= kMinimumFps) return std::nullopt;
    return fps;
}

/// @brief 補間点の軸変位量を計算する
/// @param model 運動学モデル
/// @param planned 計画した通過点
/// @param start_q 区間の始点の軸変位量
/// @param start_nc 区間の始点のNC指令値
/// @param start_home 区間の始点の制御点 (制御点を持つ形式のみ)
/// @param s 区間内の位置 (0.0〜1.0)
/// @return 軸変位量. 制御点を持つ形式では回転軸の指令と制御点を線形補間して
///         位置IKで直進軸を再計算し、直接指令では軸空間で線形補間する
/// @throw KinematicsError 位置IKが退化している場合
JointVector InterpolatePose(const MachineModel& model, const PlannedTarget& planned,
                            const JointVector& start_q, const NcValues& start_nc,
                            const igesio::Vector3d& start_home, const double s) {
    const detail::Target& target = planned.target;
    if (!target.point_home.has_value()) return Lerp(start_q, planned.q, s);

    NcValues rotary;
    for (const NcEntry& entry : planned.rotary_end.Entries()) {
        const double from = start_nc.GetOr(entry.register_name, entry.value);
        rotary.Set(entry.register_name, from + s * (entry.value - from));
    }
    const igesio::Vector3d point = start_home + s * (*target.point_home - start_home);
    return *SolvePosition(model, point, target.control_home, rotary, start_q).q;
}

/// @brief 1つの通過点の区間をサンプリング列に追加する (第2段階の1ステップ)
/// @param model 運動学モデル
/// @param planned 計画した通過点
/// @param fps サンプリングレート (補間しないなら`std::nullopt`)
/// @param[in,out] track サンプリング列と統計の格納先 (末尾が区間の始点)
void AppendSegment(const MachineModel& model, const PlannedTarget& planned,
                   const std::optional<double> fps, MotionTrack& track) {
    const detail::Target& target = planned.target;
    const MotionSample start = track.samples.back();
    const std::size_t count = DivisionCount(planned, fps);
    NcValues start_nc;
    igesio::Vector3d start_home = igesio::Vector3d::Zero();
    if (count > 1 && target.point_home.has_value()) {
        start_nc = NcFromJoints(model, start.q);
        start_home = ControlPointHome(model, start.q, target.control_home);
    }

    for (std::size_t k = 1; k <= count; ++k) {
        const bool last = (k == count);
        MotionSample sample;
        sample.record_index = target.record_index;
        sample.kind = target.kind;
        sample.tool_number = target.tool_number;
        sample.is_command_point = last && target.is_command_point;
        sample.time = start.time + planned.duration * static_cast<double>(k)
                                   / static_cast<double>(count);
        if (last) {
            sample.q = planned.q;
            sample.warning = planned.warning;
        } else {
            const double s = static_cast<double>(k) / static_cast<double>(count);
            try {
                sample.q = InterpolatePose(model, planned, start.q, start_nc,
                                           start_home, s);
            } catch (const KinematicsError& e) {
                sample.q = track.samples.back().q;
                ++track.stats.unreachable_count;
                track.warnings.push_back(Diagnostic{
                        Severity::kWarning,
                        "record " + std::to_string(target.record_index),
                        std::string("unreachable interpolation point; the previous "
                                    "pose is kept: ") + e.what(), target.line});
                sample.warning = track.warnings.size() - 1;
            }
        }
        track.samples.push_back(std::move(sample));
    }
}

/// @brief レコード → 先頭サンプリング点の表を作る
/// @param program プログラム
/// @param first_of_motion 動作レコードの先頭サンプリング点
///        (動作レコードでない場合は`std::nullopt`)
/// @param sample_count サンプリング点の総数
/// @return 各レコード以降で最初の動作レコードの先頭サンプリング点.
///         末尾は`sample_count`
std::vector<std::size_t> RecordFirstSamples(
        const ClProgram& program,
        const std::vector<std::optional<std::size_t>>& first_of_motion,
        const std::size_t sample_count) {
    std::vector<std::size_t> table(program.records.size(), sample_count);
    std::size_t next = sample_count;
    for (std::size_t i = program.records.size(); i-- > 0;) {
        if (first_of_motion[i].has_value()) next = *first_of_motion[i];
        table[i] = next;
    }
    return table;
}

}  // namespace



MotionTrack PlanMotion(const MachiningSetup& setup, const ClProgram& program,
                       const MotionOptions& options) {
    ValidateOptions(options);
    const MachineModel& model = setup.Model();
    const BranchPolicy policy = options.branch.value_or(model.Definition().branch);
    const OvertravelPolicy overtravel =
            options.overtravel.value_or(setup.Project().run.overtravel);

    MotionTrack track;
    detail::PlannerState state = detail::MakePlannerState(setup, &track.warnings);
    std::vector<PlannedTarget> planned;
    std::optional<MotionSample> initial;
    for (std::size_t i = 0; i < program.records.size(); ++i) {
        const ClRecord& record = program.records[i];
        if (IsMotion(record)) {
            ++track.stats.motion_record_count;
            if (!initial.has_value()) {
                initial = MotionSample{};
                initial->q = setup.BaseQ();
                initial->record_index = i;
                initial->tool_number = state.cl.tool;
            }
            const std::vector<detail::Target> targets = detail::NormalizeRecord(
                    setup, state, program, i, options.arc_chord_tolerance);
            for (const detail::Target& target : targets) {
                planned.push_back(PlanTarget(setup, state, target, policy,
                                             overtravel,options, track));
                if ((planned.size() - 1) % kCheckInterval == 0) {
                    CheckPlanned(model, planned.back(), track.stats);
                }
            }
        }
        state.cl.Apply(record);
    }
    if (!initial.has_value()) {
        track.record_first_sample.assign(program.records.size(), 0);
        return track;
    }

    const std::optional<double> fps = ResolveFps(planned, options, track);
    initial->kind = planned.empty() ? MotionKind::kLinear
                                    : planned.front().target.kind;
    track.samples.push_back(*initial);
    std::vector<std::optional<std::size_t>> first_of_motion(program.records.size());
    first_of_motion[initial->record_index] = 0;
    for (const PlannedTarget& target : planned) {
        if (!first_of_motion[target.target.record_index].has_value()) {
            first_of_motion[target.target.record_index] = track.samples.size();
        }
        AppendSegment(model, target, fps, track);
    }
    track.record_first_sample =
            RecordFirstSamples(program, first_of_motion, track.samples.size());

    track.stats.sample_count = track.samples.size();
    track.stats.command_point_count = static_cast<std::size_t>(std::count_if(
            track.samples.begin(), track.samples.end(),
            [](const MotionSample& sample) { return sample.is_command_point; }));
    track.stats.duration_sec = track.samples.back().time;
    return track;
}

std::size_t SampleIndexAtTime(const MotionTrack& track, const double time_sec) {
    if (track.samples.empty()) return 0;

    const auto upper = std::upper_bound(
            track.samples.begin(), track.samples.end(), time_sec,
            [](const double value, const MotionSample& sample) {
                return value < sample.time;
            });
    if (upper == track.samples.begin()) return 0;
    return static_cast<std::size_t>(std::distance(track.samples.begin(), upper)) - 1;
}

std::optional<std::size_t> CommandSampleOfRecord(const MotionTrack& track,
                                                 const std::size_t record_index) {
    if (record_index >= track.record_first_sample.size()) return std::nullopt;

    // 先頭サンプリング点から同じレコードの間を走査する. 状態レコードでは先頭が
    // 次の動作レコードを指すため、レコードのインデックスが一致せず見つからない
    for (std::size_t i = track.record_first_sample[record_index];
         i < track.samples.size() && track.samples[i].record_index == record_index;
         ++i) {
        if (track.samples[i].is_command_point) return i;
    }
    return std::nullopt;
}

NcValues DisplayNc(const MachineModel& model, const JointVector& q) {
    NcValues nc = NcFromJoints(model, q);
    NcValues display;
    for (const AxisInfo& axis : model.Axes()) {
        double value = nc.At(axis.register_name);
        if (axis.kind == AxisKind::kRotary && axis.unlimited
            && axis.wrap_start.has_value()) {
            value = WrapAngleIntoLimits(value, axis).value_or(value);
        }
        display.Set(axis.register_name, value);
    }
    return display;
}

}  // namespace igesio::extensions::machines
