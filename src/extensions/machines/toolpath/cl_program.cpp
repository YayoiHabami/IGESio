/**
 * @file extensions/machines/toolpath/cl_program.cpp
 * @brief 制御装置に依存しない工具経路 (CLプログラム) の公開データモデル
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/machines/toolpath/cl_program.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "igesio/extensions/machines/core/tolerances.h"
#include "extensions/machines/toolpath/text_utils.h"

namespace igesio::extensions::machines {

namespace {

/// @brief レコードを`ClState`に適用する関数オブジェクト (`std::visit`用)
/// @note レコード種別の追加時にオーバーロードの不足はコンパイルエラーになる
struct StateApplier {
    /// @brief 更新する状態
    ClState& state;

    /// @brief 直線移動を適用する
    /// @param record 適用するレコード
    /// @note `kWork`のみ更新し、`kMachine`は何も更新しない
    void operator()(const ClGoto& record) const {
        if (record.frame == MotionFrame::kMachine) return;

        if (record.point.has_value()) state.position = record.point;
        if (record.tool_axis.has_value()) state.tool_axis = record.tool_axis;
        state.axis_words.Merge(record.axis_words);
    }
    /// @brief 円弧を適用する (終点と終点の工具軸)
    /// @param record 適用するレコード
    void operator()(const ClArc& record) const {
        state.position = record.end;
        if (record.tool_axis.has_value()) state.tool_axis = record.tool_axis;
    }
    /// @brief ドウェルを適用する (状態は変えない)
    void operator()(const ClDwell&) const {}
    /// @brief 送りを適用する
    /// @param record 適用するレコード
    void operator()(const ClFeed& record) const { state.feed = record.mm_per_s; }
    /// @brief 主軸を適用する
    /// @param record 適用するレコード
    /// @note 回転数が省略された場合は直前の値を保つ
    void operator()(const ClSpindle& record) const {
        state.spindle.mode = record.mode;
        if (record.rpm.has_value()) state.spindle.rpm = record.rpm;
    }
    /// @brief クーラントを適用する
    /// @param record 適用するレコード
    void operator()(const ClCoolant& record) const { state.coolant = record.on; }
    /// @brief 工具の選択を適用する
    /// @param record 適用するレコード
    void operator()(const ClLoadTool& record) const { state.tool = record.number; }
    /// @brief ワーク座標系の選択を適用する
    /// @param record 適用するレコード
    void operator()(const ClSelectWorkOffset& record) const {
        state.work_offset = record.id;
    }
    /// @brief 工具長補正を適用する
    /// @param record 適用するレコード
    void operator()(const ClLengthOffset& record) const {
        state.length_offset = record.number;
    }
    /// @brief コメントを適用する (状態は変えない)
    void operator()(const ClComment&) const {}
    /// @brief 区切りを適用する (状態は変えない)
    void operator()(const ClMarker&) const {}
    /// @brief 変換せずに保持する文字列を適用する (状態は変えない)
    void operator()(const ClPassThrough&) const {}
    /// @brief 終了を適用する
    void operator()(const ClEnd&) const { state.ended = true; }
};

/// @brief レコード種別を名称に変換する関数オブジェクト (`std::visit`用)
struct KindNamer {
    /// @brief 直線移動の名称を取得する
    std::string_view operator()(const ClGoto&) const { return "goto"; }
    /// @brief 円弧の名称を取得する
    std::string_view operator()(const ClArc&) const { return "arc"; }
    /// @brief ドウェルの名称を取得する
    std::string_view operator()(const ClDwell&) const { return "dwell"; }
    /// @brief 送りの名称を取得する
    std::string_view operator()(const ClFeed&) const { return "feed"; }
    /// @brief 主軸の名称を取得する
    std::string_view operator()(const ClSpindle&) const { return "spindle"; }
    /// @brief クーラントの名称を取得する
    std::string_view operator()(const ClCoolant&) const { return "coolant"; }
    /// @brief 工具の選択の名称を取得する
    std::string_view operator()(const ClLoadTool&) const { return "load_tool"; }
    /// @brief ワーク座標系の選択の名称を取得する
    std::string_view operator()(const ClSelectWorkOffset&) const {
        return "select_work_offset";
    }
    /// @brief 工具長補正の名称を取得する
    std::string_view operator()(const ClLengthOffset&) const {
        return "length_offset";
    }
    /// @brief コメントの名称を取得する
    std::string_view operator()(const ClComment&) const { return "comment"; }
    /// @brief 区切りの名称を取得する
    std::string_view operator()(const ClMarker&) const { return "marker"; }
    /// @brief 変換せずに保持する文字列の名称を取得する
    std::string_view operator()(const ClPassThrough&) const {
        return "pass_through";
    }
    /// @brief 終了の名称を取得する
    std::string_view operator()(const ClEnd&) const { return "end"; }
};

/// @brief 1レコードの整合性を検査する
/// @param record 検査するレコード
/// @param index レコードインデックス (文言用)
/// @param line 行番号 (不明なら0)
/// @param[out] warnings 警告の追記先
void ValidateRecord(const ClRecord& record,
                    const std::size_t index, const int line,
                    std::vector<Diagnostic>& warnings) {
    const std::string prefix = "record " + std::to_string(index) + ": ";
    if (const auto* motion = std::get_if<ClGoto>(&record)) {
        if (motion->kind != MotionKind::kRapid &&
            motion->kind != MotionKind::kLinear) {
            detail::PushWarning(&warnings, prefix + "goto must be rapid or linear",
                                line);
        }
        if (motion->frame == MotionFrame::kMachine && motion->point.has_value()) {
            detail::PushWarning(&warnings,
                                prefix + "machine-frame goto must not have a point",
                                line);
        }
    } else if (const auto* arc = std::get_if<ClArc>(&record)) {
        if (arc->kind != MotionKind::kArcCw && arc->kind != MotionKind::kArcCcw) {
            detail::PushWarning(&warnings,
                                prefix + "arc must be clockwise or counterclockwise",
                                line);
        }
        if (arc->normal.norm() < kDegenerateTolerance) {
            detail::PushWarning(&warnings, prefix + "arc normal is zero", line);
        }
    }
}

}  // namespace



bool ClProgram::HasSources() const {
    return !sources.empty() && sources.size() == records.size();
}

void ClState::Apply(const ClRecord& record) {
    std::visit(StateApplier{*this}, record);
}

bool IsMotion(const ClRecord& record) {
    return std::holds_alternative<ClGoto>(record) ||
           std::holds_alternative<ClArc>(record) ||
           std::holds_alternative<ClDwell>(record);
}

std::string_view ClRecordKindName(const ClRecord& record) {
    return std::visit(KindNamer{}, record);
}

std::vector<Diagnostic> ValidateClProgram(const ClProgram& program) {
    std::vector<Diagnostic> warnings;
    if (!program.sources.empty() && !program.HasSources()) {
        detail::PushWarning(
            &warnings,
            "sources has " + std::to_string(program.sources.size()) + " entries"
            " for " + std::to_string(program.records.size()) + " records");
    }
    const bool sources_usable = program.HasSources();

    bool ended = false;
    bool reported_after_end = false;
    for (std::size_t i = 0; i < program.records.size(); ++i) {
        const int line = sources_usable ? program.sources[i].line : 0;
        // 終了後のレコードは最初の1件だけ報告する (件数は文言に含める)
        if (ended && !reported_after_end) {
            detail::PushWarning(&warnings,
                                "record " + std::to_string(i) + ": "
                                + std::to_string(program.records.size() - i)
                                + " record(s) after end", line);
            reported_after_end = true;
        }
        ValidateRecord(program.records[i], i, line, warnings);
        if (std::holds_alternative<ClEnd>(program.records[i])) ended = true;
    }
    return warnings;
}

}  // namespace igesio::extensions::machines
