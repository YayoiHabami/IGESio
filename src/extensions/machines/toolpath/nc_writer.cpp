/**
 * @file extensions/machines/toolpath/nc_writer.cpp
 * @brief ポストプロセッサ (`ClProgram` → NC)
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note 各レコードの出力は`NcEmitter` (`nc_emit.h`) のメンバで行い,
 *       本ファイルはその定義と公開関数を置く.
 */
#include "igesio/extensions/machines/toolpath/nc_writer.h"

#include <cmath>
#include <cstddef>
#include <ctime>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/extensions/machines/core/text_file.h"
#include "igesio/extensions/machines/core/tolerances.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/toolpath/cl_transform.h"
#include "igesio/extensions/machines/toolpath/nc_block.h"
#include "extensions/machines/toolpath/nc_emit.h"

namespace igesio::extensions::machines {

namespace {

/// @brief 直前のワードと同じ値とみなす許容誤差 (座標 [mm]、工具軸の成分)
constexpr double kSameValueTolerance = 1e-9;

/// @brief 座標のアドレス (成分順)
constexpr char kCoordinateAddresses[3] = {'X', 'Y', 'Z'};

/// @brief 工具軸のアドレス (成分順)
constexpr char kToolAxisAddresses[3] = {'I', 'J', 'K'};

/// @brief 円弧の平面と法線の向き
struct PlaneAlignment {
    /// @brief 円弧の平面
    ArcPlane plane = ArcPlane::kXY;
    /// @brief 法線が平面の正の軸 (+Z/+Y/+X) を向くか
    bool positive = true;
};

/// @brief 法線が主平面の軸に一致するか調べる
/// @param normal 正規化済みの法線
/// @return 一致する平面と向き. 一致しなければ`std::nullopt`
std::optional<PlaneAlignment> AlignToPrincipalPlane(const igesio::Vector3d& normal) {
    const double threshold = 1.0 - kUnitVectorTolerance;
    if (std::abs(normal.z()) > threshold) {
        return PlaneAlignment{ArcPlane::kXY, normal.z() > 0.0};
    }
    if (std::abs(normal.y()) > threshold) {
        return PlaneAlignment{ArcPlane::kZX, normal.y() > 0.0};
    }
    if (std::abs(normal.x()) > threshold) {
        return PlaneAlignment{ArcPlane::kYZ, normal.x() > 0.0};
    }
    return std::nullopt;
}

/// @brief 平面コードを取得する
/// @param vocab 出力で使うコード名
/// @param plane 円弧の平面
/// @return `plane_xy`/`plane_zx`/`plane_yz`のいずれか
std::string PlaneCode(const NcVocabulary& vocab, const ArcPlane plane) {
    switch (plane) {
        case ArcPlane::kZX: return vocab.plane_zx;
        case ArcPlane::kYZ: return vocab.plane_yz;
        default: return vocab.plane_xy;
    }
}

/// @brief 平面内の2成分のインデックス (0 = X, 1 = Y, 2 = Z) を取得する
/// @param plane 円弧の平面
/// @return 平面内の成分のインデックス (昇順)
std::pair<int, int> InPlaneComponents(const ArcPlane plane) {
    switch (plane) {
        case ArcPlane::kZX: return {0, 2};
        case ArcPlane::kYZ: return {1, 2};
        default: return {0, 1};
    }
}

/// @brief 今日の日付をISO 8601 (`YYYY-MM-DD`) で取得する
/// @return ローカル時刻の日付
std::string IsoDate() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::ostringstream stream;
    stream << std::put_time(&local, "%Y-%m-%d");
    return stream.str();
}

/// @brief 実数を整数の文字列にする (主軸回転数用)
/// @param value 整形する値
/// @return 四捨五入した整数の文字列
std::string IntegerText(const double value) {
    return std::to_string(std::lround(value));
}

/// @brief 送り [mm/s] を毎分のFコードの数値部分の文字列にする (アドレス無し)
/// @param mm_per_s 送り速度 [mm/s]
/// @param format Fコードの書式
/// @return 毎分の送りを書式に従って整形した文字列
std::string FeedText(const double mm_per_s, const NcNumberFormat& format) {
    return FormatNcNumber(mm_per_s * kSecondsPerMinute, format);
}

/// @brief 軸アドレス表を軸名→軸アドレスの向きに変換する
/// @param table 軸アドレス → 軸名
/// @return 軸名 → 軸アドレス
std::map<std::string, char> ReverseAddressMap(
        const std::map<char, std::string>& table) {
    std::map<std::string, char> reversed;
    for (const auto& [address, register_name] : table) {
        reversed.emplace(register_name, address);
    }
    return reversed;
}

/// @brief レコードが工具軸または回転軸の指令を伴う移動か
/// @param record レコード
/// @param rotary_registers 軸名 → 回転軸の軸アドレスの表
bool IsTcpMotion(const ClRecord& record,
                 const std::map<std::string, char>& rotary_registers) {
    if (const auto* motion = std::get_if<ClGoto>(&record)) {
        if (motion->tool_axis.has_value()) return true;
        for (const NcEntry& entry : motion->axis_words.Entries()) {
            if (rotary_registers.count(entry.register_name) > 0) return true;
        }
        return false;
    }
    if (const auto* arc = std::get_if<ClArc>(&record)) {
        return arc->tool_axis.has_value();
    }
    return false;
}

/// @brief テンプレート変数の最初の出現を集める関数オブジェクト (`std::visit`用)
struct FirstVariableCollector {
    /// @brief 集めた変数 (未出現のキーは持たない)
    std::map<std::string, std::string>& variables;
    /// @brief Fコードの書式
    const NcNumberFormat& feed_format;

    /// @brief 未設定なら設定する
    /// @param key 変数名
    /// @param value 値
    void Put(const std::string& key, const std::string& value) const {
        variables.emplace(key, value);
    }
    /// @brief 工具の選択から`tool`を集める
    /// @param record 対象のレコード
    void operator()(const ClLoadTool& record) const {
        Put("tool", std::to_string(record.number));
    }
    /// @brief 工具長補正から`h`を集める
    /// @param record 対象のレコード
    void operator()(const ClLengthOffset& record) const {
        if (record.number.has_value()) Put("h", std::to_string(*record.number));
    }
    /// @brief 主軸から`spindle`を集める
    /// @param record 対象のレコード
    void operator()(const ClSpindle& record) const {
        if (record.rpm.has_value()) Put("spindle", IntegerText(*record.rpm));
    }
    /// @brief 送りから`feed`を集める
    /// @param record 対象のレコード
    void operator()(const ClFeed& record) const {
        Put("feed", FeedText(record.mm_per_s, feed_format));
    }
    /// @brief ワーク座標系の選択から`work_offset`を集める
    /// @param record 対象のレコード
    void operator()(const ClSelectWorkOffset& record) const {
        Put("work_offset", record.id);
    }
    /// @brief その他のレコードは変数を持たないので何もしない
    template <typename T>
    void operator()(const T&) const {}
};

/// @brief レコードを`NcEmitter::Emit`のオーバーロードに振り分ける関数オブジェクト
///        (`std::visit`用)
struct RecordEmitter {
    /// @brief 出力先
    detail::NcEmitter& emitter;
    /// @brief 種別に対応する`Emit`を呼び出す
    /// @param record 出力するレコード
    template <typename T>
    void operator()(const T& record) const { emitter.Emit(record); }
};

}  // namespace



namespace detail {

NcEmitter::NcEmitter(const NcDialect& dialect, const NcWriteOptions& options,
                     std::vector<Diagnostic>* warnings)
    : dialect_(dialect), options_(options),
      syntax_(options.syntax.has_value() ? *options.syntax : dialect.syntax),
      warnings_(warnings), last_plane_(dialect.defaults.plane),
      rotary_register_to_address_(
              ReverseAddressMap(dialect.rotary_address_to_register)),
      linear_register_to_address_(
              ReverseAddressMap(dialect.linear_address_to_register)) {}

void NcEmitter::Warn(const std::string& message) {
    if (warnings_ == nullptr) return;

    warnings_->push_back(Diagnostic{Severity::kWarning, "", message, current_line_});
}

void NcEmitter::PushRaw(const std::string& text) {
    lines_.push_back(text);
}

void NcEmitter::PushLine(const std::string& text) {
    if (!syntax_.line_number_step.has_value()) {
        lines_.push_back(text);
        return;
    }

    ++line_count_;
    const int number = *syntax_.line_number_step * static_cast<int>(line_count_);
    lines_.push_back(FormatNcInteger('N', number, syntax_.line_number_digits)
                     + syntax_.word_separator + text);
}

void NcEmitter::PushBlock(const std::vector<std::string>& words) {
    if (words.empty()) return;

    std::string text;
    for (std::size_t i = 0; i < words.size(); ++i) {
        if (i > 0) text += syntax_.word_separator;
        text += words[i];
    }
    PushLine(text);
}

std::string NcEmitter::FormatComment(const std::string& text) const {
    std::string body = text;
    // 括弧形式では本文の`)`がコメントを閉じてしまうので除く
    if (syntax_.comment_close == ")") {
        std::string stripped;
        for (const char c : body) {
            if (c != ')' && c != '(') stripped.push_back(c);
        }
        body = stripped;
    }
    return syntax_.comment_open + body + syntax_.comment_close;
}

bool NcEmitter::SameToolAxis(const igesio::Vector3d& axis) const {
    if (!last_tool_axis_.has_value()) return false;

    for (int i = 0; i < 3; ++i) {
        if (std::abs((*last_tool_axis_)[i] - axis[i]) > kSameValueTolerance) return false;
    }
    return true;
}

std::string NcEmitter::Join(const NewlineStyle newline) const {
    const std::string_view separator = NewlineText(newline);
    std::string text;
    for (const std::string& line : lines_) {
        text += line;
        text += separator;
    }
    return text;
}



/**
 * ---- プログラムの冒頭と末尾 ----
 */

std::map<std::string, std::string> NcEmitter::Variables(
        const bool first_occurrence) const {
    std::map<std::string, std::string> variables;
    std::ostringstream program;
    program << std::setw(syntax_.program_digits) << std::setfill('0')
            << options_.program_number;
    variables["program"] = program.str();
    variables["name"] = program_name_;
    variables["date"] = IsoDate();
    variables["tool"] = (state_.tool == kNoTool && first_occurrence)
                        ? std::string() : std::to_string(state_.tool);
    variables["h"] = state_.length_offset.has_value()
                     ? std::to_string(*state_.length_offset) : std::string();
    variables["spindle"] = state_.spindle.rpm.has_value()
                           ? IntegerText(*state_.spindle.rpm) : std::string();
    variables["feed"] = state_.feed.has_value()
                        ? FeedText(*state_.feed, syntax_.feed) : std::string();
    variables["work_offset"] = state_.work_offset;
    if (!first_occurrence) return variables;

    // ヘッダでは、まだ現れていない値をプログラム中の最初の出現で補う
    for (const auto& [key, value] : first_variables_) {
        if (variables[key].empty()) variables[key] = value;
    }
    return variables;
}

void NcEmitter::EmitTemplate(const std::vector<std::string>& lines,
                             const bool first_occurrence) {
    const std::map<std::string, std::string> variables = Variables(first_occurrence);
    for (const std::string& line : lines) {
        std::vector<Diagnostic> expansion_warnings;
        const std::string expanded =
                ExpandTemplate(line, variables, &expansion_warnings);
        for (const Diagnostic& warning : expansion_warnings) Warn(warning.message);
        PushLine(expanded);
    }
}

void NcEmitter::EmitPreamble() {
    const NcDialect::Defaults& defaults = dialect_.defaults;
    std::vector<std::string> words;
    if (!defaults.absolute) words.push_back(dialect_.vocab.absolute);
    if (!defaults.metric) words.push_back(dialect_.vocab.metric);
    if (!defaults.feed_per_minute) words.push_back(dialect_.vocab.feed_per_minute);
    if (defaults.plane != ArcPlane::kXY) {
        words.push_back(dialect_.vocab.plane_xy);
        last_plane_ = ArcPlane::kXY;
    }
    PushBlock(words);
}

void NcEmitter::Precheck(const ClProgram& program) {
    if (warnings_ != nullptr) {
        for (const Diagnostic& warning : ValidateClProgram(program)) {
            warnings_->push_back(warning);
        }
    }
    if (options_.tcp == TcpStyle::kNone) return;

    // 最初のTCP移動より前に長さ補正が無い場合は、G43.5/G43.4が出力されない
    for (const ClRecord& record : program.records) {
        if (std::holds_alternative<ClLengthOffset>(record)) return;
        if (IsTcpMotion(record, rotary_register_to_address_)) {
            Warn("no length offset before the first TCP motion; "
                 "G43.5/G43.4 is not written (template {h} is empty)");
            return;
        }
    }
}

void NcEmitter::BeginProgram(const ClProgram& program) {
    program_name_ = program.name;
    current_line_ = 0;
    Precheck(program);
    for (const ClRecord& record : program.records) {
        std::visit(FirstVariableCollector{first_variables_, syntax_.feed}, record);
    }

    if (syntax_.percent) PushRaw("%");
    std::ostringstream number;
    number << syntax_.program_prefix << std::setw(syntax_.program_digits)
           << std::setfill('0') << options_.program_number;
    PushLine(number.str());
    if (options_.write_header_footer) {
        EmitTemplate(options_.header.has_value() ? *options_.header : dialect_.header,
                     true);
    }
    if (options_.preamble_modal_codes) EmitPreamble();
}

void NcEmitter::EmitEnd() {
    if (ended_) return;

    if (options_.write_header_footer) {
        EmitTemplate(options_.footer.has_value() ? *options_.footer : dialect_.footer,
                     false);
    }
    PushLine(dialect_.vocab.program_end);
    ended_ = true;
}

void NcEmitter::EndProgram() {
    current_line_ = 0;
    EmitEnd();
    if (syntax_.percent) PushRaw("%");
    // 終了後のレコードは`ValidateClProgram`の警告 (事前検査で転記済み) で報告する
    if (linearized_arcs_ > 0) {
        Warn(std::to_string(linearized_arcs_) + " arc(s) linearized");
    }
}



/**
 * ---- レコードの出力 ----
 */

void NcEmitter::Emit(const ClRecord& record, const SourceLocation* source) {
    current_line_ = source != nullptr ? source->line : 0;
    // 終了後のレコードは出力しない (`ValidateClProgram`の警告で報告済み)
    if (ended_) return;

    std::visit(RecordEmitter{*this}, record);
    state_.Apply(record);
}

void NcEmitter::Emit(const ClGoto& record) {
    if (record.frame == MotionFrame::kMachine) {
        EmitMachineGoto(record);
        return;
    }

    EmitGoto(record);
}

void NcEmitter::Emit(const ClArc& record) {
    EmitRoleComment(record.role);
    if (!state_.position.has_value()) {
        Warn("arc start position is unknown; skipped");
        return;
    }
    if (record.normal.norm() < kDegenerateTolerance) {
        Warn("arc normal is zero; skipped");
        return;
    }

    const igesio::Vector3d start = *state_.position;
    const std::optional<PlaneAlignment> alignment =
            AlignToPrincipalPlane(record.normal.normalized());
    if (options_.arc == ArcOutput::kLinearize || !alignment.has_value()) {
        EmitLinearizedArc(record, start);
        return;
    }

    EmitPlanarArc(record, start, alignment->plane, alignment->positive);
}

void NcEmitter::Emit(const ClDwell& record) {
    const char address = dialect_.vocab.dwell_address;
    const double value = record.seconds * dialect_.vocab.dwell_scale;
    const NcNumberFormat format =
            IsIntegerAddress(address) ? NcNumberFormat{} : syntax_.coordinate;
    PushBlock({dialect_.vocab.dwell, FormatNcWord(address, value, format)});
}

void NcEmitter::Emit(const ClFeed&) {}

void NcEmitter::Emit(const ClSpindle& record) {
    std::vector<std::string> words;
    if (record.rpm.has_value()) words.push_back("S" + IntegerText(*record.rpm));
    switch (record.mode) {
        case ClSpindle::Mode::kCw: words.push_back(dialect_.vocab.spindle_cw); break;
        case ClSpindle::Mode::kCcw: words.push_back(dialect_.vocab.spindle_ccw); break;
        default: words.push_back(dialect_.vocab.spindle_off); break;
    }
    PushBlock(words);
}

void NcEmitter::Emit(const ClCoolant& record) {
    PushLine(record.on ? dialect_.vocab.coolant_on : dialect_.vocab.coolant_off);
}

void NcEmitter::Emit(const ClLoadTool& record) {
    if (!dialect_.tool_change.empty()) {
        // テンプレートの`{tool}`は選択する工具なので、状態の更新を先取りする
        const int previous = state_.tool;
        state_.tool = record.number;
        EmitTemplate(dialect_.tool_change, false);
        state_.tool = previous;
        return;
    }

    PushBlock({"T" + std::to_string(record.number), dialect_.vocab.tool_change});
}

void NcEmitter::Emit(const ClSelectWorkOffset& record) {
    const std::optional<std::string> word = WorkOffsetWordFromId(dialect_, record.id);
    if (word.has_value()) {
        PushLine(*word);
        return;
    }

    Warn("work offset '" + record.id + "' has no NC word; written as comment");
    PushLine(FormatComment("WORK OFFSET " + record.id));
}

void NcEmitter::Emit(const ClLengthOffset& record) {
    if (!record.number.has_value()) {
        PushLine(dialect_.vocab.cancel_comp);
        return;
    }

    std::string code = dialect_.vocab.length_comp;
    if (options_.tcp == TcpStyle::kVector) code = dialect_.vocab.tcp_vector;
    if (options_.tcp == TcpStyle::kRotaryWords) code = dialect_.vocab.tcp_rotary;
    PushBlock({code, "H" + std::to_string(*record.number)});
}

void NcEmitter::Emit(const ClComment& record) {
    if (!options_.comments) return;

    PushLine(FormatComment(record.text));
}

void NcEmitter::Emit(const ClMarker& record) {
    if (!options_.comments) return;

    std::string text;
    switch (record.kind) {
        case ClMarker::Kind::kPathBegin: text = "Path Start "; break;
        case ClMarker::Kind::kPathEnd: text = "Path End "; break;
        default: text = "Operation "; break;
    }
    PushLine(FormatComment(text + record.name));
}

void NcEmitter::Emit(const ClPassThrough& record) {
    if (record.dialect == "nc" && options_.passthrough) {
        PushLine(record.text);
        return;
    }

    if (record.dialect != "nc") {
        Warn("pass-through of dialect '" + record.dialect + "' written as comment");
    }
    PushLine(FormatComment(record.text));
}

void NcEmitter::Emit(const ClEnd&) { EmitEnd(); }

void NcEmitter::EmitRoleComment(const PathRole role) {
    if (!options_.role_comments) return;
    if (last_role_.has_value() && *last_role_ == role) return;

    last_role_ = role;
    switch (role) {
        case PathRole::kApproach: PushLine(FormatComment("Approach")); break;
        case PathRole::kRetract: PushLine(FormatComment("Retract")); break;
        case PathRole::kLink: PushLine(FormatComment("Link")); break;
        default: PushLine(FormatComment("Cut")); break;
    }
}

void NcEmitter::AppendMotionCode(const MotionKind kind,
                                 std::vector<std::string>& words) {
    const bool same = last_motion_code_.has_value() && *last_motion_code_ == kind;
    if (!options_.suppress.motion_code || !same) {
        words.push_back(kind == MotionKind::kRapid ? dialect_.vocab.rapid
                                                   : dialect_.vocab.linear);
    }
    last_motion_code_ = kind;
}

void NcEmitter::AppendCoordinates(const igesio::Vector3d& point,
                                  std::vector<std::string>& words) {
    for (int i = 0; i < 3; ++i) {
        const bool same = last_point_.has_value()
                          && std::abs((*last_point_)[i] - point[i]) <= kSameValueTolerance;
        if (options_.suppress.coordinates && same) continue;
        words.push_back(FormatNcWord(kCoordinateAddresses[i], point[i],
                                     syntax_.coordinate));
    }
    last_point_ = point;
}

void NcEmitter::AppendToolAxis(const igesio::Vector3d& axis,
                               std::vector<std::string>& words) {
    if (!(options_.suppress.tool_axis && SameToolAxis(axis))) {
        for (int i = 0; i < 3; ++i) {
            words.push_back(FormatNcWord(kToolAxisAddresses[i], axis[i], syntax_.vector));
        }
    }
    last_tool_axis_ = axis;
}

void NcEmitter::AppendAxisWords(const NcValues& axis_words, const bool linear,
                                const bool rotary, std::vector<std::string>& words) {
    for (const NcEntry& entry : axis_words.Entries()) {
        const auto linear_found = linear_register_to_address_.find(entry.register_name);
        if (linear_found != linear_register_to_address_.end()) {
            if (linear) {
                words.push_back(FormatNcWord(linear_found->second, entry.value,
                                             syntax_.coordinate));
            }
            continue;
        }
        const auto rotary_found = rotary_register_to_address_.find(entry.register_name);
        if (rotary_found != rotary_register_to_address_.end()) {
            if (rotary) {
                words.push_back(FormatNcWord(rotary_found->second,
                                             ToDegrees(entry.value), syntax_.angle));
            }
            continue;
        }
        // 直進軸の走査は移動ごとに必ず1回行うので、未知の軸名はそこでだけ警告する
        if (linear) Warn("axis '" + entry.register_name +
                         "' has no NC address; omitted");
    }
}

void NcEmitter::AppendFeed(std::vector<std::string>& words) {
    if (!state_.feed.has_value()) return;

    const bool changed = !emitted_feed_.has_value()
                         || std::abs(*emitted_feed_ - *state_.feed) > kSameValueTolerance;
    if (options_.feed == FeedOutput::kEveryCutBlock || changed) {
        words.push_back("F" + FeedText(*state_.feed, syntax_.feed));
        emitted_feed_ = state_.feed;
    }
}

void NcEmitter::AppendTcpWords(const ClGoto& record, std::vector<std::string>& words) {
    if (options_.tcp == TcpStyle::kVector) {
        // 工具軸は制御点を持つ移動にだけ付ける
        // (軸の指令のみの移動は回転軸の指令をそのまま出力する)
        if (!record.point.has_value()) {
            AppendAxisWords(record.axis_words, false, true, words);
            return;
        }

        std::optional<igesio::Vector3d> axis =
                record.tool_axis.has_value() ? record.tool_axis : state_.tool_axis;
        if (!axis.has_value()) {
            if (!assumed_axis_warned_) {
                Warn("no tool axis is defined; +Z is assumed");
                assumed_axis_warned_ = true;
            }
            axis = igesio::Vector3d::UnitZ();
        }
        AppendToolAxis(*axis, words);
        return;
    }

    if (options_.tcp == TcpStyle::kRotaryWords) {
        const std::size_t before = words.size();
        AppendAxisWords(record.axis_words, false, true, words);
        if (words.size() > before || !record.point.has_value()) return;

        // 明示された回転軸の指令が無くても、直前までの軸の指令に回転軸があればモーダル値で十分
        for (const NcEntry& entry : state_.axis_words.Entries()) {
            if (rotary_register_to_address_.count(entry.register_name) > 0) return;
        }
        throw igesio::DataFormatError(
                "line " + std::to_string(current_line_)
                + ": rotary axis words are required for G43.4 output");
    }

    AppendAxisWords(record.axis_words, false, true, words);
}

void NcEmitter::EmitGoto(const ClGoto& record) {
    EmitRoleComment(record.role);
    std::vector<std::string> words;
    AppendMotionCode(record.kind, words);
    if (record.point.has_value()) AppendCoordinates(*record.point, words);
    AppendTcpWords(record, words);
    AppendAxisWords(record.axis_words, true, false, words);
    if (record.kind == MotionKind::kLinear) AppendFeed(words);
    PushBlock(words);
}

void NcEmitter::EmitMachineGoto(const ClGoto& record) {
    EmitRoleComment(record.role);
    std::vector<std::string> words;
    words.push_back(dialect_.vocab.machine_coordinates);
    words.push_back(record.kind == MotionKind::kRapid ? dialect_.vocab.rapid
                                                      : dialect_.vocab.linear);
    AppendAxisWords(record.axis_words, true, true, words);
    PushBlock(words);
    // G53は非モーダルなので、次のブロックでは動作コードと座標を省略しない
    last_motion_code_.reset();
    last_point_.reset();
}

void NcEmitter::EmitPlanarArc(const ClArc& record, const igesio::Vector3d& start,
                              const ArcPlane plane, const bool positive) {
    std::vector<std::string> words;
    if (!options_.suppress.plane || last_plane_ != plane) {
        words.push_back(PlaneCode(dialect_.vocab, plane));
    }
    last_plane_ = plane;
    const bool ccw = (record.kind == MotionKind::kArcCcw) == positive;
    words.push_back(ccw ? dialect_.vocab.arc_ccw : dialect_.vocab.arc_cw);
    // 円弧の後は動作コードを必ず出力する
    last_motion_code_.reset();

    const auto [first, second] = InPlaneComponents(plane);
    for (int i = 0; i < 3; ++i) {
        const bool in_plane = (i == first || i == second);
        const bool same =
                last_point_.has_value()
                && std::abs((*last_point_)[i] - record.end[i]) <= kSameValueTolerance;
        if (!in_plane && options_.suppress.coordinates && same) continue;
        words.push_back(FormatNcWord(kCoordinateAddresses[i], record.end[i],
                                     syntax_.coordinate));
    }
    last_point_ = record.end;
    const igesio::Vector3d center_offset = record.center - start;
    words.push_back(FormatNcWord(kToolAxisAddresses[first], center_offset[first],
                                 syntax_.coordinate));
    words.push_back(FormatNcWord(kToolAxisAddresses[second], center_offset[second],
                                 syntax_.coordinate));

    if (options_.tcp == TcpStyle::kVector && record.tool_axis.has_value()
        && state_.tool_axis.has_value() && !arc_axis_warned_) {
        const igesio::Vector3d diff = *record.tool_axis - *state_.tool_axis;
        if (diff.norm() > kSameValueTolerance) {
            Warn("tool axis change along an arc cannot be written with G02/G03");
            arc_axis_warned_ = true;
        }
    }
    AppendFeed(words);
    PushBlock(words);
}

void NcEmitter::EmitLinearizedArc(const ClArc& record, const igesio::Vector3d& start) {
    const std::vector<igesio::Vector3d> points =
            DiscretizeArc(start, record, options_.arc_chord_tolerance);
    ++linearized_arcs_;
    for (std::size_t k = 0; k < points.size(); ++k) {
        ClGoto motion;
        motion.kind = MotionKind::kLinear;
        motion.point = points[k];
        motion.role = record.role;
        if (k + 1 == points.size()) motion.tool_axis = record.tool_axis;
        EmitGoto(motion);
    }
}

}  // namespace detail



/**
 * ---- 公開関数 ----
 */

std::optional<std::string> NcWriteOptions::Validate() const {
    if (program_number < 0) return "program_number must be non-negative";
    if (!(arc_chord_tolerance > 0.0)) return "arc_chord_tolerance must be positive";
    if (syntax.has_value()) {
        if (syntax->program_digits < 1) return "syntax.program_digits must be positive";
        if (syntax->line_number_step.has_value() && *syntax->line_number_step <= 0) {
            return "syntax.line_number_step must be positive";
        }
        if (syntax->line_number_digits < 1) {
            return "syntax.line_number_digits must be positive";
        }
    }
    return std::nullopt;
}

std::string WriteNcToString(const ClProgram& program, const NcDialect& dialect,
                            const NcWriteOptions& options,
                            std::vector<Diagnostic>* warnings) {
    if (const std::optional<std::string> error = options.Validate()) {
        throw std::invalid_argument(*error);
    }

    const bool has_sources = program.HasSources();
    detail::NcEmitter emitter(dialect, options, warnings);
    emitter.BeginProgram(program);
    for (std::size_t i = 0; i < program.records.size(); ++i) {
        emitter.Emit(program.records[i], has_sources ? &program.sources[i] : nullptr);
    }
    emitter.EndProgram();
    return emitter.Join(options.newline);
}

void WriteNc(const std::filesystem::path& path, const ClProgram& program,
             const NcDialect& dialect, const NcWriteOptions& options,
             std::vector<Diagnostic>* warnings) {
    // 文字列化の例外はファイルを作る前に送出する
    const std::string text = WriteNcToString(program, dialect, options, warnings);
    WriteTextFile(path, text);
}

}  // namespace igesio::extensions::machines
