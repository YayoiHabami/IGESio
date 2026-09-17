/**
 * @file extensions/machines/simulation/motion_scene.cpp
 * @brief 動作のサンプル列からの表示オブジェクト
 * @author Yayoi Habami
 * @date 2026-09-16
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/machines/simulation/motion_scene.h"

#include <cmath>
#include <cstddef>
#include <memory>
#include <numeric>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/entities/curves/linear_path.h"
#include "igesio/models/assembly.h"
#include "igesio/extensions/machines/core/rotation.h"
#include "igesio/extensions/machines/machine/forward_kinematics.h"
#include "igesio/extensions/machines/machine/machine_model.h"
#include "igesio/extensions/machines/scene/tool_trajectory.h"
#include "igesio/extensions/machines/tools/tool_assembly.h"
#include "igesio/extensions/machines/toolpath/cl_transform.h"

namespace igesio::extensions::machines {

namespace {

namespace i_ent = igesio::entities;
namespace i_mod = igesio::models;
using igesio::Vector3d;

/// @brief 未構築のシーンなら`std::invalid_argument`を送出する
/// @param scene シーン
/// @param operation 例外の文言に含める操作名
/// @throw std::invalid_argument 未構築の場合
void RequireBuilt(const MachineScene& scene, const std::string_view operation) {
    if (scene.IsBuilt()) return;
    throw std::invalid_argument(
            std::string(operation) + ": MachineScene is not built");
}

/// @brief 間引きの指定を検証する
/// @param thinning 間引きの指定
/// @throw std::invalid_argument `kRate`で`rate`が正でない、または`kMaxCount`で
///        `max_count`が正でない場合
void ValidateThinning(const Thinning& thinning) {
    if (thinning.mode == ThinningMode::kRate && !(thinning.rate > 0.0)) {
        throw std::invalid_argument("Thinning rate must be positive");
    }
    if (thinning.mode == ThinningMode::kMaxCount && thinning.max_count <= 0) {
        throw std::invalid_argument("Thinning max_count must be positive");
    }
}

/// @brief 工具軌跡の区間のアセンブリの名前 (`"run<k>"`) から区間のインデックスを
///        取得する
/// @param name アセンブリの名前
/// @return 名前が`"run<k>"`の形でなければ`std::nullopt`
std::optional<std::size_t> RunIndexOf(const std::string_view name) {
    if (name.size() <= kTrajectoryRunPrefix.size()
        || name.compare(0, kTrajectoryRunPrefix.size(), kTrajectoryRunPrefix) != 0) {
        return std::nullopt;
    }
    std::size_t index = 0;
    for (const char c : name.substr(kTrajectoryRunPrefix.size())) {
        if (c < '0' || c > '9') return std::nullopt;
        index = index * 10 + static_cast<std::size_t>(c - '0');
    }
    return index;
}

/// @brief サンプルが早送りか
/// @param sample サンプル
bool IsRapid(const MotionSample& sample) {
    return sample.kind == MotionKind::kRapid;
}

}  // namespace



/**
 * ---- 工具軌跡 ----
 */

namespace {

/// @brief 区間内で用いるサンプルのインデックスを集める
/// @param track 動作のサンプル列
/// @param range 区間
/// @param options 設定 (レコードの終点のみ、除外するサンプル)
/// @return サンプルのインデックス (昇順)
std::vector<std::size_t> RunSampleIndices(const MotionTrack& track,
                                          const ClPathRange& range,
                                          const ToolTrajectoryOptions& options) {
    std::vector<std::size_t> indices;
    for (std::size_t i = 0; i < track.samples.size(); ++i) {
        const MotionSample& sample = track.samples[i];
        if (sample.record_index < range.begin || sample.record_index >= range.end) {
            continue;
        }
        if (options.command_points_only && !sample.is_command_point) continue;
        if (options.skip_sample.has_value() && *options.skip_sample == i) continue;
        indices.push_back(i);
    }
    return indices;
}

/// @brief 同じ工具番号が連続するサンプルのまとまり
struct ToolSegment {
    /// @brief 工具番号
    int tool_number = kNoTool;
    /// @brief サンプルのインデックス (昇順)
    std::vector<std::size_t> samples;
};

/// @brief サンプルのインデックス列を工具番号の連続で分ける
/// @param track 動作のサンプル列
/// @param indices サンプルのインデックス (昇順)
/// @return 工具番号ごとのまとまり (出現順)
std::vector<ToolSegment> SplitByTool(const MotionTrack& track,
                                     const std::vector<std::size_t>& indices) {
    std::vector<ToolSegment> segments;
    for (const std::size_t i : indices) {
        const int tool = track.samples[i].tool_number;
        if (segments.empty() || segments.back().tool_number != tool) {
            segments.push_back(ToolSegment{tool, {}});
        }
        segments.back().samples.push_back(i);
    }
    return segments;
}

/// @brief 工具軌跡の同次変換の列を計算する
/// @param scene 構築済みのシーン
/// @param track 動作のサンプル列
/// @param samples 用いるサンプルのインデックス
/// @param spec 工具の定義
/// @return テンプレートのルート座標系→`work_mount`コンポーネント座標系の同次変換
///         P_i = F_wm(q_i)⁻¹·F_tm(q_i)·H_tm·T(0,0,-ゲージ長)
std::vector<igesio::Matrix4d> TrajectoryPlacements(
        const MachineScene& scene, const MotionTrack& track,
        const std::vector<std::size_t>& samples, const ToolAssemblySpec& spec) {
    const MachineModel& model = scene.Model();
    const igesio::Matrix4d mount = model.MountPlacement(MountKind::kToolMount)
                                   * ToolMountOffset(spec);
    std::vector<igesio::Matrix4d> frames;
    std::vector<igesio::Matrix4d> placements;
    placements.reserve(samples.size());
    for (const std::size_t i : samples) {
        Forward(model, track.samples[i].q, &frames);
        placements.push_back(RigidInverse(frames[model.WorkMountIndex()])
                             * frames[model.ToolMountIndex()] * mount);
    }
    return placements;
}

/// @brief 工具軌跡の1区間 (`run<k>`) を作る
/// @param scene 構築済みのシーン
/// @param track 動作のサンプル列
/// @param run_index 区間のインデックス
/// @param indices 区間内で用いるサンプルのインデックス (間引き後)
/// @param options 設定
/// @param[in,out] reported 警告済みの工具番号
/// @param[out] warnings 警告の追加先 (`nullptr`なら追加しない)
/// @return 区間のアセンブリ. 表示できる工具が無ければ`nullptr`
std::shared_ptr<i_mod::Assembly> MakeTrajectoryRun(
        const MachineScene& scene, const MotionTrack& track,
        const std::size_t run_index, const std::vector<std::size_t>& indices,
        const ToolTrajectoryOptions& options, std::set<int>& reported,
        std::vector<Diagnostic>* warnings) {
    std::shared_ptr<i_mod::Assembly> run;
    for (const ToolSegment& segment : SplitByTool(track, indices)) {
        const int number = segment.tool_number;
        const auto tool = scene.ToolAssembly(number);
        if (!tool) {
            if (warnings != nullptr && reported.insert(number).second) {
                std::string message = "no tool is selected; trajectory skipped";
                if (number != kNoTool) {
                    message = "tool is not in the tool table; trajectory skipped: T"
                              + std::to_string(number);
                }
                warnings->push_back(Diagnostic{
                        Severity::kWarning,
                        std::string(kTrajectoryRunPrefix) + std::to_string(run_index),
                        message, 0});
            }
            continue;
        }

        if (!run) {
            run = i_mod::MakeAssembly(std::string(kTrajectoryRunPrefix)
                                      + std::to_string(run_index));
        }
        ToolTrajectoryGroup group = MakeToolTrajectoryGroup(
                *tool,
                TrajectoryPlacements(scene, track, segment.samples,
                                     scene.Tools().at(number)),
                std::string(kTrajectoryToolPrefix) + std::to_string(number),
                options.opacity);
        group.SetHolderVisible(options.holder_visible);
        run->AddChildAssembly(group.assembly);
    }
    return run;
}

}  // namespace



std::vector<std::size_t> ThinIndices(const std::size_t count,
                                     const Thinning& thinning) {
    ValidateThinning(thinning);
    std::vector<std::size_t> all(count);
    std::iota(all.begin(), all.end(), std::size_t{0});
    if (thinning.mode == ThinningMode::kNone || count <= 2) return all;

    std::vector<std::size_t> kept;
    if (thinning.mode == ThinningMode::kRate) {
        if (thinning.rate >= 1.0) return all;
        // 累積の割合が整数を跨いだ要素を残す
        kept.push_back(0);
        for (std::size_t i = 1; i + 1 < count; ++i) {
            const double previous =
                    std::floor(static_cast<double>(i - 1) * thinning.rate);
            if (std::floor(static_cast<double>(i) * thinning.rate) > previous) {
                kept.push_back(i);
            }
        }
        kept.push_back(count - 1);
        return kept;
    }

    const std::size_t limit = static_cast<std::size_t>(thinning.max_count);
    if (limit >= count) return all;
    if (limit <= 2) return {0, count - 1};
    // 両端を含む`limit`個を等間隔に選ぶ
    for (std::size_t j = 0; j < limit; ++j) {
        const std::size_t index = static_cast<std::size_t>(std::lround(
                static_cast<double>(j) * static_cast<double>(count - 1)
                / static_cast<double>(limit - 1)));
        if (kept.empty() || kept.back() != index) kept.push_back(index);
    }
    return kept;
}

void RebuildToolTrajectory(MachineScene& scene, const MotionTrack& track,
                           const ClProgram& program,
                           const ToolTrajectoryOptions& options,
                           std::vector<Diagnostic>* warnings) {
    RequireBuilt(scene, "RebuildToolTrajectory");
    ValidateThinning(options.thinning);
    const auto trajectory = scene.TrajectoryAssembly();
    trajectory->Clear();

    std::set<int> reported;
    const std::vector<ClPathRange> ranges = EnumeratePaths(program);
    for (std::size_t k = 0; k < ranges.size(); ++k) {
        const std::vector<std::size_t> all =
                RunSampleIndices(track, ranges[k], options);
        std::vector<std::size_t> thinned;
        for (const std::size_t j : ThinIndices(all.size(), options.thinning)) {
            thinned.push_back(all[j]);
        }
        if (const auto run = MakeTrajectoryRun(scene, track, k, thinned, options,
                                               reported, warnings)) {
            trajectory->AddChildAssembly(run);
        }
    }
}

void SetToolTrajectoryVisible(MachineScene& scene,
                              const TrajectoryVisibility& visibility) {
    ValidateThinning(visibility.thinning);
    const auto trajectory = scene.TrajectoryAssembly();
    if (!trajectory) return;

    const auto& runs = trajectory->GetChildAssemblies();
    const std::vector<std::size_t> kept_list =
            ThinIndices(runs.size(), visibility.thinning);
    const std::set<std::size_t> kept(kept_list.begin(), kept_list.end());
    for (std::size_t j = 0; j < runs.size(); ++j) {
        const std::optional<std::size_t> index = RunIndexOf(runs[j]->Metadata().name);
        if (!index.has_value()) continue;

        bool visible = visibility.only_run.has_value() ? *visibility.only_run == *index
                                                       : kept.count(j) > 0;
        if (visibility.always_visible_run.has_value()
            && *visibility.always_visible_run == *index) {
            visible = true;
        }
        runs[j]->SetVisible(visibility.show && visible);
    }
}

void SetToolTrajectoryHolderVisible(MachineScene& scene, const bool visible) {
    const auto trajectory = scene.TrajectoryAssembly();
    if (!trajectory) return;

    for (const auto& run : trajectory->GetChildAssemblies()) {
        for (const auto& group : run->GetChildAssemblies()) {
            const auto holder = FindChildAssembly(*group, kTrajectoryHolderName);
            if (holder) holder->SetVisible(visible);
        }
    }
}

std::vector<ObjectID> ToolTrajectoryMetallicIds(const MachineScene& scene) {
    std::vector<ObjectID> ids;
    const auto trajectory = scene.TrajectoryAssembly();
    if (!trajectory) return ids;

    for (const auto& run : trajectory->GetChildAssemblies()) {
        for (const auto& group : run->GetChildAssemblies()) {
            for (const ObjectID& id : group->GetEntityIDs(false)) ids.push_back(id);
        }
    }
    return ids;
}



/**
 * ---- 動作軌跡 ----
 */

namespace {

/// @brief 動作軌跡の点列 (機械座標と`work_mount`座標)
struct TracePoints {
    /// @brief 機械座標の指令点 p_i = F_tm(q_i)·c
    std::vector<Vector3d> machine;
    /// @brief `work_mount`座標の指令点 F_wm(q_i)⁻¹·p_i
    std::vector<Vector3d> work;
};

/// @brief 工具の指令点のゼロポーズ機械座標cを計算する
/// @param scene 構築済みのシーン
/// @param tool_number 工具番号
/// @return H_tm·(0, 0, command_point_z - ゲージ長). 工具表に無い番号と工具なしでは
///         取り付けフレームの原点 (H_tmの並進部)
Vector3d CommandPointHome(const MachineScene& scene, const int tool_number) {
    const igesio::Matrix4d h_tm = scene.Model().MountPlacement(MountKind::kToolMount);
    const auto it = scene.Tools().find(tool_number);
    if (it == scene.Tools().end()) return TranslationPart(h_tm);

    const ToolProfile& profile = it->second.profile;
    return ApplyPoint(h_tm, Vector3d(0.0, 0.0, profile.command_point_z
                                               - profile.GaugeLength(nullptr)));
}

/// @brief サンプル列の一部の指令点を計算する
/// @param scene 構築済みのシーン
/// @param track 動作のサンプル列
/// @param begin 先頭のサンプルのインデックス
/// @param end 末尾の次のサンプルのインデックス
/// @return 機械座標と`work_mount`座標の点列 (`end - begin`個)
TracePoints ComputeTracePoints(const MachineScene& scene, const MotionTrack& track,
                               const std::size_t begin, const std::size_t end) {
    const MachineModel& model = scene.Model();
    TracePoints points;
    std::vector<igesio::Matrix4d> frames;
    std::optional<int> cached_tool;
    Vector3d home = Vector3d::Zero();
    for (std::size_t i = begin; i < end; ++i) {
        const MotionSample& sample = track.samples[i];
        if (!cached_tool.has_value() || *cached_tool != sample.tool_number) {
            cached_tool = sample.tool_number;
            home = CommandPointHome(scene, sample.tool_number);
        }
        Forward(model, sample.q, &frames);
        const Vector3d p = ApplyPoint(frames[model.ToolMountIndex()], home);
        points.machine.push_back(p);
        points.work.push_back(
                ApplyPoint(RigidInverse(frames[model.WorkMountIndex()]), p));
    }
    return points;
}

/// @brief 連続するサンプルの範囲 (早送りか否かと工具番号が同じ)
struct SampleSpan {
    /// @brief 早送りか
    bool rapid = false;
    /// @brief 先頭のサンプルのインデックス
    std::size_t begin = 0;
    /// @brief 末尾の次のサンプルのインデックス
    std::size_t end = 0;
};

/// @brief サンプル列を早送りか否かと工具番号の変化で分ける
/// @note 先頭の初期姿勢サンプルは最初の通過点の種類を持つため、最初の範囲に含まれる
std::vector<SampleSpan> SplitTraceSpans(const MotionTrack& track) {
    std::vector<SampleSpan> spans;
    for (std::size_t i = 0; i < track.samples.size(); ++i) {
        const MotionSample& sample = track.samples[i];
        const bool split = spans.empty() || spans.back().rapid != IsRapid(sample)
                           || track.samples[i - 1].tool_number != sample.tool_number;
        if (split) spans.push_back(SampleSpan{IsRapid(sample), i, i});
        spans.back().end = i + 1;
    }
    return spans;
}

/// @brief 範囲の折れ線の頂点列を取得する
/// @param points 全サンプルの点列
/// @param span 範囲
/// @return 頂点列 (`span.begin > 0`なら`span.begin - 1`から)
/// @note 直前のサンプルを始点に含めて繋ぐ
std::vector<Vector3d> SpanVertices(const std::vector<Vector3d>& points,
                                   const SampleSpan& span) {
    const std::size_t first = span.begin > 0 ? span.begin - 1 : span.begin;
    const auto begin = points.begin();
    return std::vector<Vector3d>(begin + static_cast<std::ptrdiff_t>(first),
                                 begin + static_cast<std::ptrdiff_t>(span.end));
}

/// @brief 1つの配置先 (`trace:*`) に動作軌跡の折れ線と`current`を作る
/// @param trace 配置先 (空であること)
/// @param points 全サンプルの点列 (配置先の座標系)
/// @param spans 範囲
void FillTrace(i_mod::Assembly& trace, const std::vector<Vector3d>& points,
               const std::vector<SampleSpan>& spans) {
    const auto rapid = MakeChildAssembly(trace, kRapidPathsName);
    rapid->SetColorOverride(kRapidPathColor);
    const auto cut = MakeChildAssembly(trace, kCutPathsName);
    cut->SetColorOverride(kCutPathColor);
    for (const SampleSpan& span : spans) {
        const std::vector<Vector3d> vertices = SpanVertices(points, span);
        if (vertices.size() < 2) continue;
        (span.rapid ? rapid : cut)->AddEntity(i_ent::MakeLinearPath(vertices));
    }

    const auto current = MakeChildAssembly(trace, kCurrentRecordName);
    current->SetColorOverride(kCurrentRecordColor);
    current->SetVisible(false);
}

/// @brief レコードのサンプルの範囲 (直前のサンプルを含む) を求める
/// @param track 動作のサンプル列
/// @param record_index レコードのインデックス
/// @return [begin, end). レコードにサンプルが無ければ空の範囲
std::pair<std::size_t, std::size_t> RecordSampleRange(const MotionTrack& track,
                                                      const std::size_t record_index) {
    if (record_index >= track.record_first_sample.size()) return {0, 0};

    const std::size_t first = track.record_first_sample[record_index];
    if (first >= track.samples.size()
        || track.samples[first].record_index != record_index) {
        return {0, 0};
    }
    std::size_t end = first;
    while (end < track.samples.size()
           && track.samples[end].record_index == record_index) {
        ++end;
    }
    return {first > 0 ? first - 1 : first, end};
}

/// @brief 配置先の`current`を折れ線に置き換える
/// @param trace 配置先 (`current`が無ければ何もしない)
/// @param vertices 頂点列 (2点未満なら非表示にする)
void ReplaceCurrent(const i_mod::Assembly& trace,
                    const std::vector<Vector3d>& vertices) {
    const auto current = FindChildAssembly(trace, kCurrentRecordName);
    if (!current) return;

    current->Clear();
    if (vertices.size() < 2) {
        current->SetVisible(false);
        return;
    }
    current->AddEntity(i_ent::MakeLinearPath(vertices));
    current->SetVisible(true);
}

}  // namespace



void RebuildMotionTrace(MachineScene& scene, const MotionTrack& track,
                        const MotionTraceOptions& options) {
    RequireBuilt(scene, "RebuildMotionTrace");
    const auto machine_trace = scene.MachineTraceAssembly();
    const auto work_trace = scene.WorkTraceAssembly();
    machine_trace->Clear();
    work_trace->Clear();
    if (track.samples.empty()) return;

    const TracePoints points =
            ComputeTracePoints(scene, track, 0, track.samples.size());
    const std::vector<SampleSpan> spans = SplitTraceSpans(track);
    if (options.machine_frame) FillTrace(*machine_trace, points.machine, spans);
    if (options.work_frame) FillTrace(*work_trace, points.work, spans);
}

void UpdateCurrentRecord(MachineScene& scene, const MotionTrack& track,
                         const std::size_t record_index) {
    RequireBuilt(scene, "UpdateCurrentRecord");
    const auto [begin, end] = RecordSampleRange(track, record_index);
    const TracePoints points = ComputeTracePoints(scene, track, begin, end);
    ReplaceCurrent(*scene.MachineTraceAssembly(), points.machine);
    ReplaceCurrent(*scene.WorkTraceAssembly(), points.work);
}

}  // namespace igesio::extensions::machines
