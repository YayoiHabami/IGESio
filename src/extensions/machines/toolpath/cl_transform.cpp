/**
 * @file extensions/machines/toolpath/cl_transform.cpp
 * @brief CLプログラムの変換 (円弧の分割、単位換算、区間の列挙、アプローチ/リトラクト)
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/machines/toolpath/cl_transform.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "igesio/extensions/machines/core/rotation.h"
#include "igesio/extensions/machines/core/tolerances.h"
#include "igesio/extensions/machines/core/units.h"
#include "extensions/machines/toolpath/text_utils.h"

namespace igesio::extensions::machines {

namespace {

/// @brief 始点と終点が一致するとみなす回転角の許容誤差 [rad]
constexpr double kSweepTolerance = 1e-9;

/// @brief 円弧の回転角 (符号なし. 向きに沿って始点から終点まで) を計算する
/// @param u 中心→始点
/// @param v 中心→終点
/// @param normal 正規化した法線
/// @param arc 円弧 (向きと全周の数)
/// @return 回転角 [rad]. 始点と終点が一致し`full_turns = 0`なら0
double SweepAngle(const igesio::Vector3d& u, const igesio::Vector3d& v,
                  const igesio::Vector3d& normal, const ClArc& arc) {
    // 法線まわりの右ねじで測った始点→終点の角 (-π, π]
    const double signed_angle = std::atan2(normal.dot(u.cross(v)), u.dot(v));
    double sweep = arc.kind == MotionKind::kArcCcw ? signed_angle : -signed_angle;
    if (std::abs(sweep) < kSweepTolerance) {
        sweep = 0.0;
    } else if (sweep < 0.0) {
        sweep += kFullTurn;
    }
    return sweep + kFullTurn * static_cast<double>(std::max(0, arc.full_turns));
}

/// @brief 弦誤差から分割数を計算する
/// @param sweep 回転角θ [rad]
/// @param radius 半径r [mm]
/// @param chord_tolerance 弦誤差ε [mm]
/// @return N = max(1, ⌈θ / (2 arccos(1 - ε/r))⌉)、ε ≥ r なら ⌈2θ/π⌉
std::size_t DivisionCount(const double sweep, const double radius,
                          const double chord_tolerance) {
    if (sweep <= 0.0) return 1;

    double step = 0.0;
    if (chord_tolerance >= radius) {
        step = kHalfTurn / 2.0;
    } else {
        step = 2.0 * std::acos(1.0 - chord_tolerance / radius);
    }
    if (step <= 0.0) return 1;

    return static_cast<std::size_t>(std::max(1.0, std::ceil(sweep / step)));
}

/// @brief 2つの単位ベクトルを球面線形補間する
/// @param from 始点の方向 (正規化済み)
/// @param to 終点の方向 (正規化済み)
/// @param t 補間係数 (0.0〜1.0)
/// @return 補間した方向 (正規化済み). 両者がほぼ平行なら`to`
igesio::Vector3d Slerp(const igesio::Vector3d& from, const igesio::Vector3d& to,
                       const double t) {
    const double cos_angle = std::clamp(from.dot(to), -1.0, 1.0);
    const double angle = std::acos(cos_angle);
    if (std::abs(std::sin(angle)) < kDegenerateTolerance) return to;

    const double wa = std::sin((1.0 - t) * angle) / std::sin(angle);
    const double wb = std::sin(t * angle) / std::sin(angle);
    return (from * wa + to * wb).normalized();
}

/// @brief ソース位置をコピーして指定数並べる
/// @param source コピーするソース位置 (無ければ何もしない)
/// @param count 個数
/// @param[out] sources 追記先
void AppendSources(const std::optional<SourceLocation>& source,
                   const std::size_t count,
                   std::vector<SourceLocation>& sources) {
    if (!source.has_value()) return;

    sources.insert(sources.end(), count, *source);
}

/// @brief 区間を閉じて追加する (空の区間は追加しない)
/// @param begin 区間の先頭
/// @param end 区間の末尾の次
/// @param name 区間名
/// @param rapid 早送りの連続区間か
/// @param[out] ranges 追記先
void PushRange(const std::size_t begin, const std::size_t end, std::string name,
               const bool rapid, std::vector<ClPathRange>& ranges) {
    if (end <= begin) return;

    ranges.push_back(ClPathRange{begin, end, std::move(name), rapid});
}

/// @brief 動作レコードが早送りか
/// @param record 判定するレコード
/// @return `ClGoto`の`kRapid`なら`true`、円弧は切削扱いで`false`.
///         動作レコードでなければ`std::nullopt`
std::optional<bool> RapidFlag(const ClRecord& record) {
    if (const auto* motion = std::get_if<ClGoto>(&record)) {
        return motion->kind == MotionKind::kRapid;
    }
    if (std::holds_alternative<ClArc>(record)) return false;
    return std::nullopt;
}

/// @brief 区間内の移動が全て早送りか
/// @param program 対象のプログラム
/// @param begin 区間の先頭
/// @param end 区間の末尾の次
/// @return 移動が1つ以上あり、全て`kRapid`なら`true`
bool AllRapid(const ClProgram& program, const std::size_t begin,
              const std::size_t end) {
    bool found = false;
    for (std::size_t i = begin; i < end; ++i) {
        const std::optional<bool> rapid = RapidFlag(program.records[i]);
        if (!rapid.has_value()) continue;
        if (!*rapid) return false;
        found = true;
    }
    return found;
}

/// @brief `ClMarker` (`kPathBegin`/`kPathEnd`) で区間を分ける
/// @param program 対象のプログラム
/// @return 区間の一覧 (`records`全体を覆う)
std::vector<ClPathRange> EnumerateByMarkers(const ClProgram& program) {
    std::vector<ClPathRange> ranges;
    std::size_t begin = 0;
    std::string name;
    bool inside = false;
    for (std::size_t i = 0; i < program.records.size(); ++i) {
        const auto* marker = std::get_if<ClMarker>(&program.records[i]);
        if (marker == nullptr) continue;
        if (marker->kind == ClMarker::Kind::kPathBegin) {
            // 外側 (または閉じられていない直前の区間) をここで閉じる
            PushRange(begin, i, name, AllRapid(program, begin, i), ranges);
            begin = i;
            name = marker->name;
            inside = true;
        } else if (marker->kind == ClMarker::Kind::kPathEnd && inside) {
            PushRange(begin, i + 1, name, AllRapid(program, begin, i + 1), ranges);
            begin = i + 1;
            name.clear();
            inside = false;
        }
    }
    const std::size_t end = program.records.size();
    PushRange(begin, end, name, AllRapid(program, begin, end), ranges);
    return ranges;
}

/// @brief 早送りの連続区間と切削の連続区間の切り替わりで区間を分ける
/// @param program 対象のプログラム
/// @return 区間の一覧 (`records`全体を覆う)
std::vector<ClPathRange> EnumerateByRapids(const ClProgram& program) {
    std::vector<ClPathRange> ranges;
    std::size_t begin = 0;
    std::optional<bool> current;
    for (std::size_t i = 0; i < program.records.size(); ++i) {
        const std::optional<bool> rapid = RapidFlag(program.records[i]);
        if (!rapid.has_value()) continue;
        if (current.has_value() && *current != *rapid) {
            PushRange(begin, i, "", *current, ranges);
            begin = i;
        }
        current = rapid;
    }
    PushRange(begin, program.records.size(), "", current.value_or(false), ranges);
    return ranges;
}

/// @brief 区間の先頭または末尾の動作の位置と工具軸
struct PathEndpoint {
    /// @brief 動作レコードの索引
    std::size_t index = 0;
    /// @brief 制御点 (ワーク座標)
    igesio::Vector3d point = igesio::Vector3d::Zero();
    /// @brief 工具軸方向 (定まらなければ`std::nullopt`)
    std::optional<igesio::Vector3d> tool_axis;
    /// @brief その時点の送り [mm/s]
    std::optional<double> feed;
};

/// @brief 区間の先頭の動作 (切削開始点) を探す
/// @param program 対象のプログラム
/// @param range 区間
/// @param[in,out] state 区間の先頭までを適用した状態 (走査で更新する)
/// @return 制御点を持つ最初の動作. 無ければ`std::nullopt`
std::optional<PathEndpoint>
FindFirstMotion(const ClProgram& program, const ClPathRange& range,
                ClState& state) {
    for (std::size_t i = range.begin; i < range.end; ++i) {
        const ClRecord& record = program.records[i];
        if (const auto* motion = std::get_if<ClGoto>(&record)) {
            if (motion->frame != MotionFrame::kWork || !motion->point.has_value()) {
                return std::nullopt;
            }
            const std::optional<igesio::Vector3d> axis =
                    motion->tool_axis.has_value() ? motion->tool_axis : state.tool_axis;
            return PathEndpoint{i, *motion->point, axis, state.feed};
        }
        if (std::holds_alternative<ClArc>(record)) {
            // 円弧の始点は直前の位置
            if (!state.position.has_value()) return std::nullopt;
            return PathEndpoint{i, *state.position, state.tool_axis, state.feed};
        }
        state.Apply(record);
    }
    return std::nullopt;
}

/// @brief 区間の末尾の動作 (切削終了点) を探す
/// @param program 対象のプログラム
/// @param range 区間
/// @param[in,out] state 区間の先頭までを適用した状態 (区間の末尾まで更新する)
/// @return 制御点を持つ最後の動作. 無ければ`std::nullopt`
std::optional<PathEndpoint>
FindLastMotion(const ClProgram& program, const ClPathRange& range,
               ClState& state) {
    std::optional<PathEndpoint> last;
    for (std::size_t i = range.begin; i < range.end; ++i) {
        const ClRecord& record = program.records[i];
        state.Apply(record);
        if (const auto* motion = std::get_if<ClGoto>(&record)) {
            if (motion->frame != MotionFrame::kWork || !motion->point.has_value()) {
                last.reset();
                continue;
            }
            last = PathEndpoint{i, *motion->point, state.tool_axis, state.feed};
        } else if (const auto* arc = std::get_if<ClArc>(&record)) {
            last = PathEndpoint{i, arc->end, state.tool_axis, state.feed};
        }
    }
    return last;
}

/// @brief 退避ベクトル (制御点から退避点への差分) を計算する
/// @param spec 退避の指定
/// @param tool_axis 工具軸方向 (`kToolAxis`で必要)
/// @return 退避ベクトル. `kToolAxis`で工具軸が無ければ`std::nullopt`
std::optional<igesio::Vector3d> RetractVector(
        const ApproachRetractSpec& spec,
        const std::optional<igesio::Vector3d>& tool_axis) {
    if (spec.direction == ApproachRetractSpec::Direction::kWork) return spec.offset;
    if (!tool_axis.has_value()) return std::nullopt;
    return *tool_axis * spec.distance;
}

/// @brief 挿入するレコード列 (ソース位置は`line = 0`)
struct Insertion {
    /// @brief 挿入位置 (このインデックスの前に挿入する)
    std::size_t at = 0;
    /// @brief 挿入するレコード
    std::vector<ClRecord> records;
};

/// @brief 直線移動レコードを作る
/// @param kind 動作の種類
/// @param point 制御点 (ワーク座標)
/// @param role 経路上の役割
/// @return 工具軸と軸の指令を持たない`ClGoto`
ClGoto MakeGoto(const MotionKind kind, const igesio::Vector3d& point,
                const PathRole role) {
    ClGoto motion;
    motion.kind = kind;
    motion.point = point;
    motion.role = role;
    return motion;
}

/// @brief 退避区間の送り変更を追加する
/// @param feed 直前の送り
/// @param ratio 送りの比
/// @param restore `true`なら元の送りに戻す
/// @param[out] records 追記先
/// @note `ratio`が1でなく送りが分かる場合のみ追加する
void PushFeedChange(const std::optional<double>& feed, const double ratio,
                    const bool restore, std::vector<ClRecord>& records) {
    if (!feed.has_value() || ratio == 1.0) return;

    records.push_back(ClFeed{restore ? *feed : *feed * ratio});
}

/// @brief アプローチのレコード列を作る
/// @param start 切削開始点
/// @param retract 退避ベクトル
/// @param spec 退避の指定
/// @return 切削開始点の直前に挿入するレコード列
Insertion MakeApproach(const PathEndpoint& start, const igesio::Vector3d& retract,
                       const ApproachRetractSpec& spec) {
    Insertion insertion;
    insertion.at = start.index;
    const igesio::Vector3d retract_point = start.point + retract;
    if (spec.clearance.has_value()) {
        insertion.records.push_back(MakeGoto(
                MotionKind::kRapid, retract_point + *spec.clearance,
                PathRole::kApproach));
    }
    insertion.records.push_back(MakeGoto(MotionKind::kRapid, retract_point,
                                         PathRole::kApproach));
    PushFeedChange(start.feed, spec.feed_ratio, false, insertion.records);
    insertion.records.push_back(MakeGoto(MotionKind::kLinear, start.point,
                                         PathRole::kApproach));
    PushFeedChange(start.feed, spec.feed_ratio, true, insertion.records);
    return insertion;
}

/// @brief リトラクトのレコード列を作る
/// @param end 切削終了点
/// @param retract 退避ベクトル
/// @param spec 退避の指定
/// @return 切削終了点の直後に挿入するレコード列
Insertion MakeRetract(const PathEndpoint& end, const igesio::Vector3d& retract,
                      const ApproachRetractSpec& spec) {
    Insertion insertion;
    insertion.at = end.index + 1;
    const igesio::Vector3d retract_point = end.point + retract;
    PushFeedChange(end.feed, spec.feed_ratio, false, insertion.records);
    insertion.records.push_back(MakeGoto(MotionKind::kLinear, retract_point,
                                         PathRole::kRetract));
    PushFeedChange(end.feed, spec.feed_ratio, true, insertion.records);
    if (spec.clearance.has_value()) {
        insertion.records.push_back(MakeGoto(
                MotionKind::kRapid, retract_point + *spec.clearance,
                PathRole::kRetract));
    }
    return insertion;
}

/// @brief 挿入を後ろから順に適用する (前方のインデックスを保つため)
/// @param insertions 挿入 (`at`の昇順)
/// @param[in,out] program 対象のプログラム
void ApplyInsertions(std::vector<Insertion>& insertions, ClProgram& program) {
    const bool has_sources = program.HasSources();
    for (auto it = insertions.rbegin(); it != insertions.rend(); ++it) {
        const std::size_t at = it->at;
        if (has_sources) {
            // ソース位置は隣接レコードのプログラムインデックスを引き継ぎ、行番号は不明にする
            const std::size_t neighbor = std::min(at, program.sources.size() - 1);
            SourceLocation source{program.sources[neighbor].program_index, 0};
            program.sources.insert(program.sources.begin() + static_cast<long>(at),
                                   it->records.size(), source);
        }
        program.records.insert(program.records.begin() + static_cast<long>(at),
                               it->records.begin(), it->records.end());
    }
}

}  // namespace



std::vector<igesio::Vector3d>
DiscretizeArc(const igesio::Vector3d& start, const ClArc& arc,
              const double chord_tolerance) {
    if (chord_tolerance <= 0.0) {
        throw std::invalid_argument("chord_tolerance must be positive");
    }
    if (arc.normal.norm() < kDegenerateTolerance) {
        throw std::invalid_argument("arc normal must not be zero");
    }

    const igesio::Vector3d normal = arc.normal.normalized();
    const igesio::Vector3d u = start - arc.center;
    const igesio::Vector3d v = arc.end - arc.center;
    const double radius = u.norm();
    // 半径が0の場合は回転できないので終点のみ
    if (radius < kDegenerateTolerance) return {arc.end};

    const double sweep = SweepAngle(u, v, normal, arc);
    const std::size_t count = DivisionCount(sweep, radius, chord_tolerance);
    const double sign = arc.kind == MotionKind::kArcCcw ? 1.0 : -1.0;
    std::vector<igesio::Vector3d> points;
    points.reserve(count);
    for (std::size_t k = 1; k < count; ++k) {
        const double angle = sign * sweep * static_cast<double>(k)
                             / static_cast<double>(count);
        points.push_back(arc.center + RotationAboutAxis(normal, angle) * u);
    }
    // 最終点は丸めを避けて終点そのものにする
    points.push_back(arc.end);
    return points;
}

void ScaleLengths(ClProgram& program, const double scale) {
    for (ClRecord& record : program.records) {
        if (auto* motion = std::get_if<ClGoto>(&record)) {
            if (motion->point.has_value()) *motion->point *= scale;
        } else if (auto* arc = std::get_if<ClArc>(&record)) {
            arc->end *= scale;
            arc->center *= scale;
        } else if (auto* feed = std::get_if<ClFeed>(&record)) {
            feed->mm_per_s *= scale;
        }
    }
}

void LinearizeArcs(ClProgram& program, const double chord_tolerance) {
    const bool has_sources = program.HasSources();
    std::vector<ClRecord> records;
    std::vector<SourceLocation> sources;
    ClState state;
    for (std::size_t i = 0; i < program.records.size(); ++i) {
        const ClRecord& record = program.records[i];
        const std::optional<SourceLocation> source =
                has_sources ? std::optional<SourceLocation>(program.sources[i])
                            : std::nullopt;
        const auto* arc = std::get_if<ClArc>(&record);
        // 始点が分からない円弧は分割できないのでそのまま残す
        if (arc == nullptr || !state.position.has_value()) {
            records.push_back(record);
            AppendSources(source, 1, sources);
            state.Apply(record);
            continue;
        }

        const std::vector<igesio::Vector3d> points =
                DiscretizeArc(*state.position, *arc, chord_tolerance);
        const std::optional<igesio::Vector3d> from = state.tool_axis;
        for (std::size_t k = 0; k < points.size(); ++k) {
            ClGoto motion = MakeGoto(MotionKind::kLinear, points[k], arc->role);
            const bool last = (k + 1 == points.size());
            if (arc->tool_axis.has_value()) {
                if (from.has_value()) {
                    const double t = static_cast<double>(k + 1)
                                     / static_cast<double>(points.size());
                    motion.tool_axis = Slerp(*from, *arc->tool_axis, t);
                } else if (last) {
                    motion.tool_axis = arc->tool_axis;
                }
            }
            records.push_back(motion);
        }
        AppendSources(source, points.size(), sources);
        state.Apply(record);
    }
    program.records = std::move(records);
    program.sources = std::move(sources);
}

std::vector<ClPathRange> EnumeratePaths(const ClProgram& program) {
    const bool has_markers = std::any_of(
            program.records.begin(), program.records.end(),
            [](const ClRecord& record) {
                const auto* marker = std::get_if<ClMarker>(&record);
                return marker != nullptr
                       && marker->kind == ClMarker::Kind::kPathBegin;
            });
    return has_markers ? EnumerateByMarkers(program)
                       : EnumerateByRapids(program);
}

void InsertApproachRetract(ClProgram& program, const ApproachRetractSpec& spec,
                           std::vector<Diagnostic>* warnings) {
    std::vector<Insertion> insertions;
    ClState state;
    for (const ClPathRange& range : EnumeratePaths(program)) {
        if (range.rapid) {
            for (std::size_t i = range.begin; i < range.end; ++i) {
                state.Apply(program.records[i]);
            }
            continue;
        }

        ClState start_state = state;
        const std::optional<PathEndpoint> first =
                FindFirstMotion(program, range, start_state);
        const std::optional<PathEndpoint> last =
                FindLastMotion(program, range, state);
        if (!first.has_value() || !last.has_value()) {
            detail::PushWarning(warnings, "path '" + range.name
                                + "' has no work-frame motion; approach/retract skipped");
            continue;
        }

        const std::optional<igesio::Vector3d> approach_vector =
                RetractVector(spec, first->tool_axis);
        const std::optional<igesio::Vector3d> retract_vector =
                RetractVector(spec, last->tool_axis);
        if (!approach_vector.has_value() || !retract_vector.has_value()) {
            detail::PushWarning(warnings, "path '" + range.name
                                + "' has no tool axis; approach/retract skipped");
            continue;
        }

        insertions.push_back(MakeApproach(*first, *approach_vector, spec));
        insertions.push_back(MakeRetract(*last, *retract_vector, spec));
    }
    ApplyInsertions(insertions, program);
}

}  // namespace igesio::extensions::machines
