/**
 * @file extensions/machines/toolpath/nc_interpreter.cpp
 * @brief NCプログラムから`ClProgram`への変換
 *        (ブロックの走査、サブプログラム、初期レコード)
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note 外部サブプログラム (`subprogram_loader`で取得) は字句解析した結果を
 *       O番号ごとに保持し、同じ番号の2回目以降の呼び出しでは再読込しない.
 */
#include "igesio/extensions/machines/toolpath/nc_interpreter.h"

#include <cstddef>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/extensions/machines/core/text_file.h"
#include "igesio/extensions/machines/toolpath/nc_dialect.h"
#include "extensions/machines/toolpath/nc_modal.h"

namespace igesio::extensions::machines {

namespace {

using detail::BlockControl;
using detail::BlockWords;
using detail::InterpretContext;
using detail::MotionFlags;

/// @brief 走査の設定 (再帰呼び出しで共有する)
struct Runner {
    /// @brief 変換の作業状態
    InterpretContext& context;
    /// @brief 字句解析の設定 (外部サブプログラムのブロックスキップに用いる)
    const NcLexOptions& lex_options;
    /// @brief 読み込んだ外部サブプログラム (O番号 → 字句解析の結果)
    std::map<int, std::shared_ptr<NcLexResult>> external;
};

/// @brief モーダル状態を制御装置の定義のデフォルトに戻す
/// @param[in,out] state 戻す状態
/// @param dialect 制御装置の定義 (`defaults`を参照する)
/// @note 工具番号とワークオフセットidは保つ
void ResetModal(NcState& state, const NcDialect& dialect) {
    NcState fresh;
    fresh.tool_number = state.tool_number;
    fresh.work_offset_id = state.work_offset_id;
    fresh.absolute = dialect.defaults.absolute;
    fresh.plane = dialect.defaults.plane;
    fresh.metric = dialect.defaults.metric;
    fresh.feed_per_minute = dialect.defaults.feed_per_minute;
    state = fresh;
}

/// @brief 正規化した無効化コードの集合を作る
/// @param dialect 制御装置の定義
/// @return `disabled_codes`を`NormalizeCodeName`で正規化した集合
std::set<std::string> NormalizedDisabledCodes(const NcDialect& dialect) {
    std::set<std::string> codes;
    for (const std::string& code : dialect.disabled_codes) {
        codes.insert(NormalizeCodeName(code));
    }
    return codes;
}

/// @brief プログラム先頭の状態レコード (工具、ワークオフセット、長さ補正、送り) を
///        出力する
/// @param context 変換の作業状態
/// @note 単独でも`ClState`が定まるように、変化の有無によらず出力する (`line = 0`)
void EmitInitialRecords(InterpretContext& context) {
    const NcState& state = context.state;
    context.Emit(ClLoadTool{state.tool_number}, 0);
    context.Emit(ClSelectWorkOffset{state.work_offset_id}, 0);
    if (state.length_comp && state.h_number.has_value()) {
        context.Emit(ClLengthOffset{state.h_number}, 0);
    }
    if (state.feed.has_value()) context.Emit(ClFeed{*state.feed}, 0);
}

/// @brief 1ブロックを処理する
/// @param context 変換の作業状態
/// @param block ブロック
/// @return ブロック末尾のMコードの結果
/// @throw igesio::DataFormatError 無効化されたコード、または移動レコードの生成に
///        不備がある場合 (各`Process*`から伝播)
BlockControl ProcessBlock(InterpretContext& context, const NcBlock& block) {
    const BlockWords words = detail::ClassifyBlock(block);
    NcState& state = context.state;
    detail::CheckDisabledCodes(context, words);
    detail::EmitComments(context, words);
    // ベンダー固有のMだけのブロックは本文全体を保持するだけで処理しない
    if (detail::IsUnknownMOnlyBlock(context, words)) {
        detail::ProcessMCodes(context, words);
        return BlockControl{};
    }

    // 直前のブロックでのTCP形式の変化は、このブロックの増分指令の警告に使う
    const bool changed_before = state.tcp_changed;
    state.tcp_changed = false;
    detail::ProcessModes(context, words);
    detail::ProcessWorkOffset(context, words);
    detail::ProcessLengthOffset(context, words);
    MotionFlags flags = detail::ProcessTiltedPlane(context, words);
    const bool changed_now = state.tcp_changed;
    state.tcp_changed = changed_before || changed_now;
    flags = detail::ProcessNonModal(context, words, flags);
    detail::ProcessMotionCode(context, words);
    detail::ProcessFeedSpindleTool(context, words);
    detail::ProcessMCodes(context, words);
    detail::SyncStateRecords(context, words.line);
    detail::ProcessMotion(context, words, flags);
    state.tcp_changed = changed_now;
    return detail::ProcessBlockEnd(context, words);
}

/// @brief ブロック列を走査する (`CallSubprogram`と相互再帰するため前方宣言,
///        詳細は定義側)
void RunBlocks(Runner& runner, const NcLexResult& source, std::size_t start,
               int depth, bool is_subprogram);

/// @brief 外部サブプログラムを読み込む (`subprogram_loader`)
/// @param[in,out] runner 走査の設定 (読み込んだ結果を`external`に保持する)
/// @param number O番号
/// @return 字句解析の結果 (警告は`context.program.warnings`に移す).
///         `subprogram_loader`が未設定または`std::nullopt`を返せば`nullptr`
std::shared_ptr<NcLexResult> LoadExternal(Runner& runner, const int number) {
    const auto found = runner.external.find(number);
    if (found != runner.external.end()) return found->second;
    const auto& loader = runner.context.options.subprogram_loader;
    if (!loader) return nullptr;
    const std::optional<std::string> text = loader(number);
    if (!text.has_value()) return nullptr;

    NcLexOptions lex_options;
    lex_options.block_skip = runner.lex_options.block_skip;
    auto source = std::make_shared<NcLexResult>(LexNc(*text, lex_options));
    for (Diagnostic& warning : source->warnings) {
        runner.context.program.warnings.push_back(std::move(warning));
    }
    source->warnings.clear();
    runner.external[number] = source;
    return source;
}

/// @brief サブプログラム (M98) を呼び出す
/// @param runner 走査の設定
/// @param source 呼び出し元のブロック列 (同一ファイル内のO番号を探す)
/// @param control M98の内容
/// @param line 呼び出し元の行番号
/// @param depth 呼び出し元の深さ
/// @throw igesio::DataFormatError 深さ超過、またはサブプログラムが未定義の場合
void CallSubprogram(Runner& runner, const NcLexResult& source,
                    const BlockControl& control, const int line, const int depth) {
    const int number = *control.call_program;
    if (depth + 1 > runner.context.options.max_subprogram_depth) {
        throw igesio::DataFormatError(
                "line " + std::to_string(line) + ": subprogram depth exceeds "
                + std::to_string(runner.context.options.max_subprogram_depth));
    }

    const NcLexResult* target = &source;
    std::size_t start = 0;
    const std::optional<std::size_t> local = source.programs.Find(number);
    if (local.has_value()) {
        start = *local;
    } else {
        const std::shared_ptr<NcLexResult> external = LoadExternal(runner, number);
        if (external == nullptr) {
            throw igesio::DataFormatError(
                    "line " + std::to_string(line) + ": subprogram O"
                    + std::to_string(number) + " is not defined");
        }
        target = external.get();
        // 外部ファイルにO番号の定義が無ければ、ファイルの先頭から実行する
        start = external->programs.Find(number).value_or(0);
    }

    for (int i = 0; i < control.call_count; ++i) {
        if (runner.context.state.ended) return;
        RunBlocks(runner, *target, start, depth + 1, true);
    }
}

/// @brief ブロック列を走査する
/// @param runner 走査の設定
/// @param source ブロック列
/// @param start 開始ブロックの索引
/// @param depth サブプログラムの深さ (主プログラムは0)
/// @param is_subprogram サブプログラムとして走査しているか (M99で復帰する)
/// @throw igesio::DataFormatError ブロックの処理またはサブプログラムの呼び出しに
///        不備がある場合 (`ProcessBlock`/`CallSubprogram`から伝播)
void RunBlocks(Runner& runner, const NcLexResult& source, const std::size_t start,
               const int depth, const bool is_subprogram) {
    InterpretContext& context = runner.context;
    for (std::size_t i = start; i < source.blocks.size(); ++i) {
        const NcBlock& block = source.blocks[i];
        // 別のプログラム定義の開始に達した場合は、そのプログラムは実行しない
        if (i != start && block.program_number.has_value()) return;

        const BlockControl control = ProcessBlock(context, block);
        if (control.call_program.has_value()) {
            CallSubprogram(runner, source, control, block.line, depth);
        }
        if (control.end || (control.return_from_sub && !is_subprogram)) {
            context.state.ended = true;
            context.Emit(ClEnd{}, block.line);
            return;
        }
        if (context.state.ended || control.return_from_sub) return;
    }
}

/// @brief 未知のMコードの集計を情報診断にする
/// @param context 変換の作業状態
void ReportUnknownMCodes(InterpretContext& context) {
    if (context.unknown_m_codes.empty()) return;

    std::string message = "ignored M codes: ";
    bool first = true;
    for (const auto& [code, count] : context.unknown_m_codes) {
        if (!first) message += ", ";
        message += "M" + std::to_string(code) + " x" + std::to_string(count);
        first = false;
    }
    context.program.warnings.push_back(Diagnostic{Severity::kInfo, "", message, 0});
}

}  // namespace



ClProgram InterpretNc(const std::string& text, const NcLexOptions& lex_options,
                      const NcInterpretOptions& options, NcState* state) {
    NcState local;
    NcState& modal = state != nullptr ? *state : local;
    if (state == nullptr || options.dialect.reset_modal_at_program_start) {
        ResetModal(modal, options.dialect);
    }
    modal.ended = false;
    modal.pending_tool.reset();

    NcLexResult lexed = LexNc(text, lex_options);
    ClProgram program;
    program.warnings = std::move(lexed.warnings);
    lexed.warnings.clear();
    InterpretContext context{options, modal, program,
                             NormalizedDisabledCodes(options.dialect), ClState{}, {}};
    EmitInitialRecords(context);

    Runner runner{context, lex_options, {}};
    RunBlocks(runner, lexed, 0, 0, false);
    ReportUnknownMCodes(context);
    return program;
}

ClProgram InterpretNcFile(const std::filesystem::path& path,
                          const NcLexOptions& lex_options,
                          const NcInterpretOptions& options, NcState* state) {
    return InterpretNc(ReadTextFile(path), lex_options, options, state);
}

}  // namespace igesio::extensions::machines
