/**
 * @file extensions/machines/toolpath/nc_modal.cpp
 * @brief NC変換のグループ別処理 (モーダル状態の更新と移動レコードの生成)
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note 本ファイル内の「座標語の座標系」は`NcState::position_work`の座標系
 *       (TCPの形式で決まる. ワーク座標、特徴座標、または登録値相対) を指す.
 */
#include "extensions/machines/toolpath/nc_modal.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/extensions/machines/core/rotation.h"
#include "igesio/extensions/machines/core/tolerances.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/toolpath/cl_transform.h"
#include "igesio/extensions/machines/toolpath/nc_dialect.h"

namespace igesio::extensions::machines::detail {

namespace {

/// @brief 回転軸の指令つきの円弧と登録値相対の円弧を折れ線化する弦誤差 [mm]
constexpr double kArcLinearizeTolerance = 0.05;

/// @brief 始点と終点の一致判定の許容誤差 [mm]
constexpr double kSamePointTolerance = 1e-9;

/// @brief 行番号つきの`DataFormatError`を作る
/// @param line 元ファイルの行番号
/// @param message 内容
/// @return `"line N: message"`を文言とする例外
igesio::DataFormatError FormatError(const int line, const std::string& message) {
    return igesio::DataFormatError("line " + std::to_string(line) + ": " + message);
}

/// @brief 長さの換算係数を取得する
/// @param state モーダル状態
/// @return G21なら1.0、G20なら`kInchToMillimeter`
double LengthScale(const NcState& state) {
    return state.metric ? 1.0 : kInchToMillimeter;
}

/// @brief 直進軸の軸アドレス (X/Y/Z) をインデックス (0/1/2) に変換する
/// @param address アドレス
/// @return 0/1/2. X/Y/Z以外なら`std::nullopt`
std::optional<std::size_t> LinearIndex(const char address) {
    if (address == 'X') return 0;
    if (address == 'Y') return 1;
    if (address == 'Z') return 2;
    return std::nullopt;
}

/// @brief 平面の法線を取得する (ワーク座標. 傾斜面の回転前)
/// @param plane 円弧の平面
/// @return +Z/+Y/+X
igesio::Vector3d PlaneNormal(const ArcPlane plane) {
    if (plane == ArcPlane::kZX) return igesio::Vector3d::UnitY();
    if (plane == ArcPlane::kYZ) return igesio::Vector3d::UnitX();
    return igesio::Vector3d::UnitZ();
}

/// @brief 平面の中心指定に使うアドレスとインデックスを取得する
/// @param plane 円弧の平面
/// @return {アドレス, インデックス}の組を第1成分、第2成分の順に2つ
/// @note ZX平面なら{K,2},{I,0}、YZ平面なら{J,1},{K,2}、XY平面なら{I,0},{J,1}
std::vector<std::pair<char, std::size_t>> PlaneCenterWords(const ArcPlane plane) {
    if (plane == ArcPlane::kZX) return {{'K', 2}, {'I', 0}};
    if (plane == ArcPlane::kYZ) return {{'J', 1}, {'K', 2}};
    return {{'I', 0}, {'J', 1}};
}

/// @brief 平面に含まれない第3のアドレス (螺旋ピッチ. 未対応) を取得する
/// @param plane 円弧の平面
/// @return K/J/I
char PlaneThirdWord(const ArcPlane plane) {
    if (plane == ArcPlane::kZX) return 'J';
    if (plane == ArcPlane::kYZ) return 'I';
    return 'K';
}

/// @brief ブロックの回転軸の指令 (制御装置定義の表にある軸アドレス) を読み,
///        モーダル値を更新する
/// @param context 変換の作業状態
/// @param words ブロックのワード
/// @return 明示された回転軸の指令 (軸名は変換済み. rad)
NcValues ReadRotaryWords(InterpretContext& context, const BlockWords& words) {
    NcValues explicit_words;
    NcState& state = context.state;
    for (const auto& [address, register_name] :
         context.options.dialect.rotary_address_to_register) {
        const std::optional<double> value = words.Get(address);
        if (!value.has_value()) continue;
        const double angle = ToRadians(*value);
        const double modal = state.absolute
                ? angle : state.rotary_values.GetOr(register_name, 0.0) + angle;
        state.rotary_values.Set(register_name, modal);
        explicit_words.Set(register_name, modal);
    }
    return explicit_words;
}

/// @brief ブロックの座標語を読み、`position_work`を更新する
/// @param context 変換の作業状態
/// @param words ブロックのワード
/// @return 座標語が1つ以上あれば`true`
bool ReadLinearWords(InterpretContext& context, const BlockWords& words) {
    NcState& state = context.state;
    const double scale = LengthScale(state);
    bool found = false;
    for (const char address : {'X', 'Y', 'Z'}) {
        const std::optional<double> value = words.Get(address);
        if (!value.has_value()) continue;
        const std::size_t index = *LinearIndex(address);
        const double length = *value * scale;
        state.position_work[index] =
                state.absolute ? length : state.position_work[index] + length;
        found = true;
    }
    if (found && !state.absolute && state.tcp_changed) {
        context.Warn("incremental move right after a TCP mode change; the reference "
                     "position is ambiguous", words.line);
    }
    return found;
}

/// @brief 制御装置定義の表にない軸アドレスを警告する
///        (X/Y/Z/I/J/K/R/F/S/T/H/D/P/L/Q以外)
/// @param context 変換の作業状態
/// @param words ブロックのワード
void WarnUnknownAddresses(InterpretContext& context, const BlockWords& words) {
    const std::string known = "XYZIJKRFSTHDPLQ";
    for (const auto& [address, value] : words.values) {
        if (known.find(address) != std::string::npos) continue;
        if (context.options.dialect.rotary_address_to_register.count(address) > 0) {
            continue;
        }
        context.Warn(std::string("unsupported address ") + address + " ignored",
                     words.line);
    }
}

/// @brief `kVector`のIJK指令を読み、工具軸を更新する
/// @param context 変換の作業状態
/// @param words ブロックのワード
/// @return IJK指令が1つ以上あれば`true`
/// @throw igesio::DataFormatError IJKがゼロベクトルの場合
bool ReadToolAxisWords(InterpretContext& context, const BlockWords& words) {
    if (!words.Has('I') && !words.Has('J') && !words.Has('K')) return false;

    igesio::Vector3d axis(words.Get('I').value_or(0.0), words.Get('J').value_or(0.0),
                          words.Get('K').value_or(0.0));
    if (axis.norm() < kDegenerateTolerance) {
        throw FormatError(words.line, "tool axis I/J/K is a zero vector");
    }
    context.state.tool_axis_work = axis.normalized();
    return true;
}

/// @brief 現在の座標語の値をワーク座標の制御点に変換する
/// @param state モーダル状態
/// @return 制御点 (ワーク座標). 傾斜面では特徴座標→ワーク座標に変換する
igesio::Vector3d ControlPointWork(const NcState& state) {
    if (state.tcp == NcState::Tcp::kTiltedPlane && state.feature_frame.has_value()) {
        return ApplyPoint(*state.feature_frame, state.position_work);
    }
    return state.position_work;
}

/// @brief 直進軸の座標語 (`kOff`) を軸名に対応付けて軸の指令にする
/// @param context 変換の作業状態
/// @param words ブロックのワード
/// @return ブロックにあった直進軸の指令 (値は`position_work`の対応成分)
NcValues LinearAxisWords(const InterpretContext& context, const BlockWords& words) {
    NcValues values;
    const NcState& state = context.state;
    for (const auto& [address, register_name] :
         context.options.dialect.linear_address_to_register) {
        const std::optional<std::size_t> index = LinearIndex(address);
        if (!index.has_value() || !words.Has(address)) continue;
        values.Set(register_name, state.position_work[*index]);
    }
    return values;
}

/// @brief 機械座標のブロック (G53) の移動レコードを生成する
/// @param context 変換の作業状態
/// @param words ブロックのワード
/// @note 軸の指令が1つも無い場合は警告して生成しない
void EmitMachineFrameMotion(InterpretContext& context, const BlockWords& words) {
    NcState& state = context.state;
    ClGoto motion;
    motion.kind = state.motion == MotionKind::kRapid ? MotionKind::kRapid
                                                     : MotionKind::kLinear;
    motion.frame = MotionFrame::kMachine;
    const double scale = LengthScale(state);
    for (const auto& [address, register_name] :
         context.options.dialect.linear_address_to_register) {
        const std::optional<double> value = words.Get(address);
        if (value.has_value()) motion.axis_words.Set(register_name, *value * scale);
    }
    motion.axis_words.Merge(ReadRotaryWords(context, words));
    if (motion.axis_words.Empty()) {
        context.Warn("G53 without axis words is ignored", words.line);
        return;
    }

    context.Emit(motion, words.line);
}

/// @brief 直線移動 (G00/G01) のレコードを作る
/// @param context 変換の作業状態
/// @param words ブロックのワード
/// @param has_xyz 座標語があったか
/// @param has_ijk IJK指令 (工具軸) があったか
/// @param rotary 明示された回転軸の指令
/// @return TCPの形式に応じて制御点/工具軸/軸の指令を設定した`ClGoto`
ClGoto MakeLinearMotion(const InterpretContext& context, const BlockWords& words,
                        const bool has_xyz, const bool has_ijk, const NcValues& rotary) {
    const NcState& state = context.state;
    ClGoto motion;
    motion.kind = state.motion;
    switch (state.tcp) {
        case NcState::Tcp::kVector:
            motion.point = ControlPointWork(state);
            if (has_ijk) motion.tool_axis = state.tool_axis_work;
            break;
        case NcState::Tcp::kTiltedPlane:
            motion.point = ControlPointWork(state);
            if (state.tool_axis_work.has_value()) motion.tool_axis = state.tool_axis_work;
            break;
        case NcState::Tcp::kRotaryWords:
            motion.point = ControlPointWork(state);
            motion.axis_words = rotary;
            break;
        case NcState::Tcp::kOff:
            if (has_xyz) motion.axis_words = LinearAxisWords(context, words);
            motion.axis_words.Merge(rotary);
            break;
    }
    return motion;
}

/// @brief 円弧の中心をI/J/K (始点相対) から計算する
/// @param context 変換の作業状態
/// @param words ブロックのワード
/// @param start 始点 (座標語の座標系)
/// @param[out] full_circle 始点と終点が一致する全円なら`true`
/// @return 中心 (座標語の座標系)
igesio::Vector3d CenterFromIjk(InterpretContext& context, const BlockWords& words,
                               const igesio::Vector3d& start, bool& full_circle) {
    const NcState& state = context.state;
    const double scale = LengthScale(state);
    igesio::Vector3d center = start;
    for (const auto& [address, index] : PlaneCenterWords(state.plane)) {
        center[index] += words.Get(address).value_or(0.0) * scale;
    }
    if (words.Has(PlaneThirdWord(state.plane))) {
        context.Warn("helical pitch word is not supported and ignored", words.line);
    }
    full_circle = (state.position_work - start).norm() < kSamePointTolerance;
    return center;
}

/// @brief 円弧の中心をR (正=劣弧、負=優弧) から計算する
/// @param context 変換の作業状態
/// @param words ブロックのワード
/// @param start 始点 (座標語の座標系)
/// @param end 終点 (座標語の座標系)
/// @return 中心 (座標語の座標系)
/// @throw igesio::DataFormatError 始点と終点が一致する、または弦が直径より長い場合
igesio::Vector3d CenterFromRadius(InterpretContext& context, const BlockWords& words,
                                  const igesio::Vector3d& start,
                                  const igesio::Vector3d& end) {
    const NcState& state = context.state;
    const double radius = *words.Get('R') * LengthScale(state);
    const igesio::Vector3d chord = end - start;
    const double half = chord.norm() / 2.0;
    if (half < kSamePointTolerance) {
        throw FormatError(words.line, "arc with R requires distinct start and end");
    }
    if (half > std::abs(radius) + kSamePointTolerance) {
        throw FormatError(words.line, "arc radius is smaller than half the chord");
    }

    const double height = std::sqrt(std::max(0.0, radius * radius - half * half));
    // 反時計回りの劣弧の場合は中心が弦の左側 (法線×弦方向) にある
    const igesio::Vector3d left = PlaneNormal(state.plane).cross(chord / (2.0 * half));
    double sign = state.motion == MotionKind::kArcCcw ? 1.0 : -1.0;
    if (radius < 0.0) sign = -sign;
    return start + chord / 2.0 + left * (sign * height);
}

/// @brief 円弧レコードを作る (中心指定の計算とワーク座標への変換)
/// @param context 変換の作業状態
/// @param words ブロックのワード
/// @param start 始点 (座標語の座標系)
/// @return 円弧 (ワーク座標)
/// @throw igesio::DataFormatError `kVector`でI/J/Kの中心指定がある, I/J/KもRも
///        無い, またはRの指定に不備がある (`CenterFromRadius`から伝播) 場合
ClArc MakeArc(InterpretContext& context, const BlockWords& words,
              const igesio::Vector3d& start) {
    NcState& state = context.state;
    const bool has_ijk = words.Has('I') || words.Has('J') || words.Has('K');
    if (state.tcp == NcState::Tcp::kVector && has_ijk) {
        throw FormatError(words.line,
                          "I/J/K arc center is not allowed under G43.5; use R");
    }

    ClArc arc;
    arc.kind = state.motion;
    bool full_circle = false;
    igesio::Vector3d center;
    if (words.Has('R')) {
        center = CenterFromRadius(context, words, start, state.position_work);
    } else if (has_ijk) {
        center = CenterFromIjk(context, words, start, full_circle);
    } else {
        throw FormatError(words.line, "arc requires I/J/K or R");
    }
    arc.full_turns = full_circle ? 1 : 0;

    igesio::Vector3d normal = PlaneNormal(state.plane);
    igesio::Vector3d end = state.position_work;
    if (state.tcp == NcState::Tcp::kTiltedPlane && state.feature_frame.has_value()) {
        center = ApplyPoint(*state.feature_frame, center);
        end = ApplyPoint(*state.feature_frame, end);
        normal = ApplyDirection(*state.feature_frame, normal);
    }
    arc.center = center;
    arc.end = end;
    arc.normal = normal;
    if (state.tcp == NcState::Tcp::kTiltedPlane) arc.tool_axis = state.tool_axis_work;
    return arc;
}

/// @brief 登録値相対の円弧 (`kOff`) を座標語の`ClGoto`列にする
/// @param context 変換の作業状態
/// @param words ブロックのワード
/// @param arc 円弧 (座標語の座標系)
/// @param start 始点 (座標語の座標系)
/// @param rotary 明示された回転軸の指令 (最終点に付ける)
/// @note TCP無効時の座標語は登録値相対の直進軸の指令であり、`ClArc` (ワーク座標)
///       では表せないので、平面の2成分の座標語を持つ`ClGoto`の列にする. 動作生成が
///       直線ブロックと同様に扱えるようにするため、`point`は持たせない
void EmitRegisteredArc(InterpretContext& context, const BlockWords& words,
                       const ClArc& arc, const igesio::Vector3d& start,
                       const NcValues& rotary) {
    const std::vector<igesio::Vector3d> points =
            DiscretizeArc(start, arc, kArcLinearizeTolerance);
    const std::map<char, std::string>& table =
            context.options.dialect.linear_address_to_register;
    for (std::size_t i = 0; i < points.size(); ++i) {
        ClGoto motion;
        motion.kind = MotionKind::kLinear;
        for (const auto& [address, index] : PlaneCenterWords(context.state.plane)) {
            // 平面の中心指定のアドレス (I/J/K) と同じ成分の座標語 (X/Y/Z) を出す
            const char coordinate = "XYZ"[index];
            const auto it = table.find(coordinate);
            if (it == table.end()) continue;
            motion.axis_words.Set(it->second, points[i][index]);
        }
        if (i + 1 == points.size()) motion.axis_words.Merge(rotary);
        context.Emit(motion, words.line);
    }
}

/// @brief 円弧 (G02/G03) のレコードを生成する
/// @param context 変換の作業状態
/// @param words ブロックのワード
/// @param start 始点 (座標語の座標系)
/// @param rotary 明示された回転軸の指令
/// @throw igesio::DataFormatError 中心指定に不備がある場合 (`MakeArc`から伝播)
void EmitArc(InterpretContext& context, const BlockWords& words,
             const igesio::Vector3d& start, const NcValues& rotary) {
    const ClArc arc = MakeArc(context, words, start);
    const NcState::Tcp tcp = context.state.tcp;
    if (tcp == NcState::Tcp::kOff) {
        EmitRegisteredArc(context, words, arc, start, rotary);
        return;
    }
    if (tcp != NcState::Tcp::kRotaryWords) {
        context.Emit(arc, words.line);
        return;
    }

    // 回転軸の指令つきの円弧は表せないので折れ線化し、軸の指令は最終点に付ける
    // (`kRotaryWords`では傾斜面が無いので、始点はワーク座標そのもの)
    context.Warn("arc under G43.4 is linearized", words.line);
    const std::vector<igesio::Vector3d> points =
            DiscretizeArc(start, arc, kArcLinearizeTolerance);
    for (std::size_t i = 0; i < points.size(); ++i) {
        ClGoto motion;
        motion.kind = MotionKind::kLinear;
        motion.point = points[i];
        if (i + 1 == points.size()) motion.axis_words = rotary;
        context.Emit(motion, words.line);
    }
}

/// @brief M98のプログラム番号 (P) と繰り返し回数 (L/K) を読む
/// @param context 変換の作業状態
/// @param words ブロックのワード
/// @return `call_program`と`call_count`を設定した指示. Pが無ければ空の指示
BlockControl ReadSubprogramCall(InterpretContext& context, const BlockWords& words) {
    BlockControl control;
    const std::optional<double> p = words.Get('P');
    if (!p.has_value()) {
        context.Warn("M98 without P is ignored", words.line);
        return control;
    }

    control.call_program = static_cast<int>(std::lround(*p));
    const std::optional<double> count =
            words.Has('L') ? words.Get('L') : words.Get('K');
    if (count.has_value()) control.call_count = static_cast<int>(std::lround(*count));
    return control;
}

/// @brief 正規化した名前の一覧に含まれるか
/// @param codes 正規化した名前の一覧
/// @param code 探す名前
bool ContainsCode(const std::vector<std::string>& codes, const std::string& code) {
    return std::find(codes.begin(), codes.end(), code) != codes.end();
}

}  // namespace



std::optional<double> BlockWords::Get(const char address) const {
    const auto it = values.find(address);
    if (it == values.end()) return std::nullopt;
    return it->second;
}

bool BlockWords::HasG(const std::string& code) const {
    return ContainsCode(g_codes, code);
}

BlockWords ClassifyBlock(const NcBlock& block) {
    BlockWords words;
    words.line = block.line;
    words.text = block.text;
    words.comments = block.comments;
    for (const NcWord& word : block.words) {
        if (word.address == 'G' || word.address == 'M') {
            // 正規化した名前は`FormatNcWord`の整数/小数コードの整形と同じにする
            const std::string name =
                    FormatNcWord(word.address, word.value, NcNumberFormat{});
            (word.address == 'G' ? words.g_codes : words.m_codes).push_back(name);
        } else if (word.address != 'N' && word.address != 'O') {
            words.values[word.address] = word.value;
        }
    }
    return words;
}

void InterpretContext::Emit(ClRecord record, const int line) {
    emitted.Apply(record);
    program.records.push_back(std::move(record));
    program.sources.push_back(SourceLocation{options.program_index, line});
}

void InterpretContext::Warn(const std::string& message, const int line) {
    program.warnings.push_back(Diagnostic{Severity::kWarning, "", message, line});
}



/**
 * ---- グループ別処理 ----
 */

void CheckDisabledCodes(const InterpretContext& context, const BlockWords& words) {
    if (context.disabled.empty()) return;

    for (const std::vector<std::string>* codes : {&words.g_codes, &words.m_codes}) {
        for (const std::string& code : *codes) {
            if (context.disabled.count(code) > 0) {
                throw FormatError(words.line, code + " is disabled by the project");
            }
        }
    }
}

void EmitComments(InterpretContext& context, const BlockWords& words) {
    if (!context.options.keep_comments) return;

    for (const std::string& comment : words.comments) {
        context.Emit(ClComment{comment}, words.line);
    }
}

void ProcessModes(InterpretContext& context, const BlockWords& words) {
    NcState& state = context.state;
    for (const std::string& code : words.g_codes) {
        if (code == "G20") {
            state.metric = false;
        } else if (code == "G21") {
            state.metric = true;
        } else if (code == "G90") {
            state.absolute = true;
        } else if (code == "G91") {
            state.absolute = false;
        } else if (code == "G17") {
            state.plane = ArcPlane::kXY;
        } else if (code == "G18") {
            state.plane = ArcPlane::kZX;
        } else if (code == "G19") {
            state.plane = ArcPlane::kYZ;
        } else if (code == "G94") {
            state.feed_per_minute = true;
        } else if (code == "G95") {
            context.Warn("G95 is treated as feed per minute", words.line);
            state.feed_per_minute = true;
        }
    }
}

void ProcessWorkOffset(InterpretContext& context, const BlockWords& words) {
    NcState& state = context.state;
    const NcDialect& dialect = context.options.dialect;
    for (const std::string& code : words.g_codes) {
        if (code == "G54.1") {
            const std::optional<double> p = words.Get('P');
            if (!p.has_value()) context.Warn("G54.1 without P", words.line);
            std::optional<int> number;
            if (p.has_value()) number = static_cast<int>(std::lround(*p));
            state.work_offset_id = WorkOffsetIdFromWord(dialect, code, number);
        } else if (code == "G54" || code == "G55" || code == "G56" || code == "G57"
                   || code == "G58" || code == "G59") {
            state.work_offset_id = WorkOffsetIdFromWord(dialect, code, std::nullopt);
        }
    }
}

void ProcessLengthOffset(InterpretContext& context, const BlockWords& words) {
    NcState& state = context.state;
    const std::optional<double> h = words.Get('H');
    const std::optional<double> d = words.Get('D');
    if (h.has_value()) state.h_number = static_cast<int>(std::lround(*h));
    if (d.has_value()) state.d_number = static_cast<int>(std::lround(*d));

    const NcState::Tcp before = state.tcp;
    for (const std::string& code : words.g_codes) {
        if (code == "G43") {
            state.length_comp = true;
        } else if (code == "G43.4") {
            state.length_comp = true;
            state.tcp = NcState::Tcp::kRotaryWords;
        } else if (code == "G43.5") {
            state.length_comp = true;
            state.tcp = NcState::Tcp::kVector;
        } else if (code == "G49") {
            state.length_comp = false;
            state.tcp = NcState::Tcp::kOff;
        } else if (code == "G44") {
            context.Warn("G44 is not supported and ignored", words.line);
        }
    }
    if (state.tcp != before) state.tcp_changed = true;
}

MotionFlags ProcessTiltedPlane(InterpretContext& context, const BlockWords& words) {
    MotionFlags flags;
    NcState& state = context.state;
    if (words.HasG("G68.2")) {
        const double scale = LengthScale(state);
        const igesio::Vector3d origin(words.Get('X').value_or(0.0) * scale,
                                      words.Get('Y').value_or(0.0) * scale,
                                      words.Get('Z').value_or(0.0) * scale);
        const igesio::Vector3d euler(ToRadians(words.Get('I').value_or(0.0)),
                                     ToRadians(words.Get('J').value_or(0.0)),
                                     ToRadians(words.Get('K').value_or(0.0)));
        state.feature_frame = MakeRigid(RotationFromEulerIjk(euler), origin);
        if (state.tcp != NcState::Tcp::kTiltedPlane) state.tcp_changed = true;
        state.tcp = NcState::Tcp::kTiltedPlane;
        // 以降の座標語は特徴座標なので、直前位置も特徴座標に変換し直す
        state.position_work = ApplyPoint(RigidInverse(*state.feature_frame),
                                         state.position_work);
        flags.skip_motion = true;
    }

    if (words.HasG("G53.1")) {
        if (!state.feature_frame.has_value()) {
            context.Warn("G53.1 without G68.2 is ignored", words.line);
        } else {
            state.tool_axis_work =
                    ApplyDirection(*state.feature_frame, igesio::Vector3d::UnitZ());
        }
    }

    if (words.HasG("G69") && state.feature_frame.has_value()) {
        // 特徴座標で保持していた直前位置をワーク座標に戻す
        state.position_work = ApplyPoint(*state.feature_frame, state.position_work);
        state.feature_frame.reset();
        state.tcp = NcState::Tcp::kOff;
        state.tcp_changed = true;
    }
    return flags;
}

MotionFlags ProcessNonModal(InterpretContext& context, const BlockWords& words,
                            MotionFlags flags) {
    NcState& state = context.state;
    if (words.HasG("G4")) {
        double seconds = 0.0;
        if (words.Has('P')) {
            seconds = *words.Get('P') / 1000.0;
        } else if (words.Has('X')) {
            seconds = *words.Get('X');
        }
        context.Emit(ClDwell{seconds}, words.line);
        flags.skip_motion = true;
    }
    if (words.HasG("G92")) {
        context.Warn("G92 is not supported and ignored", words.line);
        flags.skip_motion = true;
    }
    if (words.HasG("G53")) flags.machine_frame = true;

    if (words.HasG("G28") || words.HasG("G30")) {
        // 中間点への移動 (通常の移動として生成) の後、指定軸を基準点 (機械座標0) に戻す
        const MotionKind saved = state.motion;
        const bool saved_set = state.motion_set;
        state.motion = MotionKind::kRapid;
        state.motion_set = true;
        ProcessMotion(context, words, MotionFlags{});
        state.motion = saved;
        state.motion_set = saved_set;
        if (words.HasG("G30")) {
            context.Warn("G30 reference point is unknown; only the intermediate "
                         "point is generated", words.line);
        } else {
            ClGoto reference;
            reference.kind = MotionKind::kRapid;
            reference.frame = MotionFrame::kMachine;
            for (const auto& [address, register_name] :
                 context.options.dialect.linear_address_to_register) {
                if (words.Has(address)) reference.axis_words.Set(register_name, 0.0);
            }
            for (const auto& [address, register_name] :
                 context.options.dialect.rotary_address_to_register) {
                if (words.Has(address)) reference.axis_words.Set(register_name, 0.0);
            }
            if (!reference.axis_words.Empty()) context.Emit(reference, words.line);
        }
        flags.skip_motion = true;
    }
    return flags;
}

void ProcessMotionCode(InterpretContext& context, const BlockWords& words) {
    // 動作コード以外で本ファイルが処理するGコード (未対応の判定用)
    static const std::vector<std::string> kKnown = {
            "G4", "G17", "G18", "G19", "G20", "G21", "G28", "G30", "G43", "G43.4",
            "G43.5", "G44", "G49", "G53", "G53.1", "G54", "G55", "G56", "G57",
            "G58", "G59", "G54.1", "G68.2", "G69", "G90", "G91", "G92", "G94",
            "G95"};
    NcState& state = context.state;
    int motion_count = 0;
    for (const std::string& code : words.g_codes) {
        std::optional<MotionKind> kind;
        if (code == "G0") kind = MotionKind::kRapid;
        if (code == "G1") kind = MotionKind::kLinear;
        if (code == "G2") kind = MotionKind::kArcCw;
        if (code == "G3") kind = MotionKind::kArcCcw;
        if (kind.has_value()) {
            state.motion = *kind;
            state.motion_set = true;
            ++motion_count;
            continue;
        }
        if (ContainsCode(kKnown, code)) continue;

        context.Warn("unsupported code " + code + " ignored", words.line);
        if (context.options.keep_unknown_words) {
            context.Emit(ClPassThrough{"nc", code}, words.line);
        }
    }
    if (motion_count > 1) {
        context.Warn("multiple motion codes in one block; the last one is used",
                     words.line);
    }
}

void ProcessFeedSpindleTool(InterpretContext& context, const BlockWords& words) {
    NcState& state = context.state;
    const std::optional<double> f = words.Get('F');
    if (f.has_value()) {
        state.feed = *f * LengthScale(state) / kSecondsPerMinute;
    }
    const std::optional<double> s = words.Get('S');
    if (s.has_value()) state.spindle.rpm = *s;
    const std::optional<double> t = words.Get('T');
    if (t.has_value()) state.pending_tool = static_cast<int>(std::lround(*t));
}

bool IsUnknownMOnlyBlock(const InterpretContext& context, const BlockWords& words) {
    // 本ファイルが処理するMコード (未知の判定用)
    static const std::vector<std::string> kKnownM = {
            "M0", "M1", "M2", "M3", "M4", "M5", "M6", "M8", "M9", "M30", "M98", "M99"};
    bool has_unknown = false;
    for (const std::string& code : words.m_codes) {
        if (!ContainsCode(kKnownM, code)) has_unknown = true;
    }
    if (!has_unknown || !words.g_codes.empty()) return false;

    // 軸の指令 (制御装置の直進軸/回転軸)、IJK指令、Fコードがあれば移動を伴うブロック
    const NcDialect& dialect = context.options.dialect;
    for (const auto& [address, register_name] : dialect.linear_address_to_register) {
        if (words.Has(address)) return false;
    }
    for (const auto& [address, register_name] : dialect.rotary_address_to_register) {
        if (words.Has(address)) return false;
    }
    for (const char address : {'I', 'J', 'K', 'F'}) {
        if (words.Has(address)) return false;
    }
    return true;
}

void ProcessMCodes(InterpretContext& context, const BlockWords& words) {
    NcState& state = context.state;
    for (const std::string& code : words.m_codes) {
        if (code == "M3" || code == "M4") {
            const ClSpindle::Mode mode =
                    code == "M3" ? ClSpindle::Mode::kCw : ClSpindle::Mode::kCcw;
            state.spindle.mode = mode;
            context.Emit(ClSpindle{mode, state.spindle.rpm}, words.line);
        } else if (code == "M5") {
            state.spindle.mode = ClSpindle::Mode::kOff;
            context.Emit(ClSpindle{ClSpindle::Mode::kOff, std::nullopt}, words.line);
        } else if (code == "M8" || code == "M9") {
            context.Emit(ClCoolant{code == "M8"}, words.line);
        } else if (code == "M6") {
            if (!state.pending_tool.has_value()) {
                context.Warn("M06 without a pending T word is ignored", words.line);
            } else {
                state.tool_number = *state.pending_tool;
                state.pending_tool.reset();
            }
        } else if (code == "M0" || code == "M1") {
            context.Emit(ClPassThrough{"nc", code == "M0" ? "M00" : "M01"}, words.line);
        } else if (code == "M2" || code == "M30" || code == "M98" || code == "M99") {
            continue;
        } else {
            ++context.unknown_m_codes[std::stoi(code.substr(1))];
            if (context.options.keep_unknown_words) {
                context.Emit(ClPassThrough{
                        "nc", IsUnknownMOnlyBlock(context, words) ? words.text : code},
                             words.line);
            }
        }
    }

    // Sコードだけのブロック (主軸の回転数の更新) も主軸レコードにする
    if (words.Has('S') && state.spindle.mode != ClSpindle::Mode::kOff) {
        bool has_spindle_m = false;
        for (const std::string& code : words.m_codes) {
            has_spindle_m = has_spindle_m || code == "M3" || code == "M4" || code == "M5";
        }
        if (!has_spindle_m) {
            context.Emit(ClSpindle{state.spindle.mode, state.spindle.rpm}, words.line);
        }
    }
}

void SyncStateRecords(InterpretContext& context, const int line) {
    const NcState& state = context.state;
    const ClState& emitted = context.emitted;
    if (emitted.tool != state.tool_number) {
        context.Emit(ClLoadTool{state.tool_number}, line);
    }
    if (emitted.work_offset != state.work_offset_id) {
        context.Emit(ClSelectWorkOffset{state.work_offset_id}, line);
    }
    const std::optional<int> effective =
            state.length_comp ? state.h_number : std::nullopt;
    if (emitted.length_offset != effective) {
        context.Emit(ClLengthOffset{effective}, line);
    }
    if (state.feed.has_value() && emitted.feed != state.feed) {
        context.Emit(ClFeed{*state.feed}, line);
    }
}

void ProcessMotion(InterpretContext& context, const BlockWords& words,
                   const MotionFlags& flags) {
    if (flags.skip_motion) return;

    WarnUnknownAddresses(context, words);
    if (flags.machine_frame) {
        EmitMachineFrameMotion(context, words);
        return;
    }

    NcState& state = context.state;
    const igesio::Vector3d start = state.position_work;
    const bool has_xyz = ReadLinearWords(context, words);
    const NcValues rotary = ReadRotaryWords(context, words);
    const bool is_arc = state.motion == MotionKind::kArcCw
                        || state.motion == MotionKind::kArcCcw;
    bool has_ijk = false;
    if (state.tcp == NcState::Tcp::kVector && !is_arc) {
        has_ijk = ReadToolAxisWords(context, words);
    }
    const bool has_coordinates = has_xyz || has_ijk || !rotary.Empty()
                                 || (is_arc && (words.Has('R') || words.Has('I')
                                                || words.Has('J') || words.Has('K')));
    if (!has_coordinates) return;
    if (!state.motion_set) {
        context.Warn("coordinate words before a motion code are ignored", words.line);
        return;
    }

    if (is_arc) {
        EmitArc(context, words, start, rotary);
    } else {
        context.Emit(MakeLinearMotion(context, words, has_xyz, has_ijk, rotary),
                     words.line);
    }
}

BlockControl ProcessBlockEnd(InterpretContext& context, const BlockWords& words) {
    BlockControl control;
    for (const std::string& code : words.m_codes) {
        if (code == "M30" || code == "M2") {
            control.end = true;
        } else if (code == "M99") {
            control.return_from_sub = true;
        } else if (code == "M98") {
            const BlockControl call = ReadSubprogramCall(context, words);
            control.call_program = call.call_program;
            control.call_count = call.call_count;
        }
    }
    return control;
}

}  // namespace igesio::extensions::machines::detail
