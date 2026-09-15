/**
 * @file extensions/machines/toolpath/cl_io.cpp
 * @brief CLファイル (3行1組/APT) の読み書き
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note 例外の文言の行番号は元ファイルの行番号 (`NumberedLine::line`) を用いる.
 */
#include "igesio/extensions/machines/toolpath/cl_io.h"

#include <cmath>
#include <cstddef>
#include <exception>
#include <optional>
#include <sstream>
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
#include "extensions/machines/toolpath/text_utils.h"

namespace igesio::extensions::machines {

namespace {

using detail::PushDiagnostic;
using detail::PushWarning;
using detail::ToUpper;
using detail::Trim;

/// @brief 3行1組の早送りコード (読込は大文字/小文字を区別しない)
constexpr std::string_view kRapidCode = "7f";
/// @brief 3行1組の切削コード (出力用)
constexpr std::string_view kCutCode = "80";
/// @brief 3行1組の出力で円弧を折れ線化する弦誤差 [mm]
constexpr double kTripletChordTolerance = 0.01;

/// @brief 行番号付きの行
struct NumberedLine {
    /// @brief 元ファイルの行番号 (1始まり)
    int line = 0;
    /// @brief 空白を除いた本文
    std::string text;
};

/// @brief 行範囲を適用し、空白を除いた空でない行を行番号付きで並べる
/// @param text 入力 (改行はLF/CRLFの両方)
/// @param options 読込設定 (`start_line`/`end_line`)
/// @return 行の一覧 (出現順)
std::vector<NumberedLine> SelectLines(const std::string& text,
                                      const ClReadOptions& options) {
    std::vector<NumberedLine> lines;
    std::istringstream stream(text);
    std::string raw;
    int number = 0;
    while (std::getline(stream, raw)) {
        ++number;
        if (number < options.start_line) continue;
        if (options.end_line.has_value() && number > *options.end_line) break;
        const std::string trimmed(Trim(raw));
        if (trimmed.empty()) continue;
        lines.push_back(NumberedLine{number, trimmed});
    }
    return lines;
}

/// @brief 文字列を実数に変換する
/// @param token 変換する文字列 (前後の空白は除去済み)
/// @param line 行番号 (文言用)
/// @return 実数
/// @throw igesio::DataFormatError 数値でない場合
double ParseReal(const std::string& token, const int line) {
    std::size_t consumed = 0;
    double value = 0.0;
    try {
        value = std::stod(token, &consumed);
    } catch (const std::exception&) {
        consumed = 0;
    }
    if (consumed == 0 || consumed != token.size()) {
        throw igesio::DataFormatError("CL line " + std::to_string(line)
                                      + ": not a number: '" + token + "'");
    }
    return value;
}

/// @brief 区切り文字で分けて前後の空白を除く (空の要素も残す)
/// @param text 分ける文字列
/// @param separator 区切り文字
/// @return 分けた文字列の一覧
std::vector<std::string> Split(const std::string& text, const char separator) {
    std::vector<std::string> parts;
    std::string current;
    for (const char c : text) {
        if (c == separator) {
            parts.emplace_back(Trim(current));
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    parts.emplace_back(Trim(current));
    return parts;
}

/// @brief 空白区切りの実数列を変換する
/// @param text 変換する文字列
/// @param line 行番号 (文言用)
/// @return 実数の一覧
/// @throw igesio::DataFormatError 数値でない要素がある場合 (`ParseReal`から伝播)
std::vector<double> ParseSpaceSeparated(const std::string& text, const int line) {
    std::vector<double> values;
    std::istringstream stream(text);
    std::string token;
    while (stream >> token) values.push_back(ParseReal(token, line));
    return values;
}

/// @brief 実数列の指定位置から3次元ベクトルを作る
/// @param values 実数列 (`offset + 2`までの要素があること)
/// @param offset 先頭成分のインデックス
/// @return ベクトル
igesio::Vector3d ToVector(const std::vector<double>& values, const std::size_t offset) {
    return igesio::Vector3d(values[offset], values[offset + 1], values[offset + 2]);
}

/// @brief 方向ベクトルを正規化する
/// @param vector 正規化するベクトル
/// @param line 行番号 (文言用)
/// @param what ベクトルの名称 (文言用)
/// @return 正規化したベクトル
/// @throw igesio::DataFormatError ゼロベクトルの場合
igesio::Vector3d NormalizeDirection(
        const igesio::Vector3d& vector, const int line,
        const std::string_view what) {
    if (vector.norm() < kDegenerateTolerance) {
        throw igesio::DataFormatError("CL line " + std::to_string(line) + ": "
                                      + std::string(what) + " is a zero vector");
    }
    return vector.normalized();
}

/// @brief 読込結果にレコードを追加する
/// @param[in,out] program 追加先
/// @param record 追加するレコード
/// @param program_index `SourceLocation::program_index`に設定する値
/// @param line 元ファイルの行番号
void PushRecord(ClProgram& program, ClRecord record, const int program_index,
                const int line) {
    program.records.push_back(std::move(record));
    program.sources.push_back(SourceLocation{program_index, line});
}

/// @brief 行を改行で連結する (末尾にも改行を付ける)
/// @param lines 行の一覧
/// @param newline 改行の種類
/// @return 連結した文字列
std::string JoinLines(const std::vector<std::string>& lines,
                      const NewlineStyle newline) {
    std::string text;
    for (const std::string& line : lines) {
        text += line;
        text += NewlineText(newline);
    }
    return text;
}



/**
 * ---- 3行1組の読込 ----
 */

/// @brief 3行1組の1組を`ClGoto`に変換する
/// @param lines 座標行、軸行、コード行の3要素
/// @return 制御点と工具軸方向を持つ直線移動
/// @throw igesio::DataFormatError 実数が3つでない、または軸がゼロベクトルの場合
ClGoto ParseTriplet(const NumberedLine* lines) {
    const std::vector<double> point = ParseSpaceSeparated(lines[0].text, lines[0].line);
    if (point.size() != 3) {
        throw igesio::DataFormatError("CL line " + std::to_string(lines[0].line)
                                      + ": expected 3 coordinates");
    }
    const std::vector<double> axis = ParseSpaceSeparated(lines[1].text, lines[1].line);
    if (axis.size() != 3) {
        throw igesio::DataFormatError("CL line " + std::to_string(lines[1].line)
                                      + ": expected 3 tool axis components");
    }

    ClGoto motion;
    motion.kind = ToUpper(lines[2].text) == ToUpper(kRapidCode) ? MotionKind::kRapid
                                                                : MotionKind::kLinear;
    motion.point = ToVector(point, 0);
    motion.tool_axis = NormalizeDirection(ToVector(axis, 0), lines[1].line, "tool axis");
    return motion;
}

/// @brief 3行1組の文字列を読み込む
/// @param text CLデータ
/// @param options 読込設定
/// @return CLプログラム (`ClGoto`のみ)
/// @throw igesio::DataFormatError 行数が3の倍数でない場合、または各組の不備
///        (`ParseTriplet`から伝播)
ClProgram ReadTriplet(const std::string& text, const ClReadOptions& options) {
    const std::vector<NumberedLine> lines = SelectLines(text, options);
    if (lines.size() % 3 != 0) {
        throw igesio::DataFormatError("CL: line count " + std::to_string(lines.size())
                                      + " is not a multiple of 3");
    }

    ClProgram program;
    for (std::size_t i = 0; i < lines.size(); i += 3) {
        PushRecord(program, ParseTriplet(&lines[i]), options.program_index,
                   lines[i].line);
    }
    return program;
}



/**
 * ---- 3行1組の出力 ----
 */

/// @brief 3行1組の出力状態 (`std::visit`でレコードを出力する)
struct TripletWriter {
    /// @brief 出力設定で初期化する
    /// @param write_options 出力設定 (出力が終わるまで有効であること)
    explicit TripletWriter(const ClWriteOptions& write_options)
        : options(write_options) {}

    /// @brief 出力設定
    const ClWriteOptions& options;
    /// @brief 出力行
    std::vector<std::string> lines;
    /// @brief 直前までの状態
    ClState state;
    /// @brief 表現できずに省いたレコードの数
    std::size_t omitted = 0;
    /// @brief 始点不明または法線がゼロベクトルで省いた円弧の数
    std::size_t invalid_arcs = 0;
    /// @brief 工具軸が定まらず+Zを仮定した移動の数
    std::size_t missing_axis = 0;

    /// @brief 3次元ベクトルを空白区切りで整形する
    /// @param v 整形するベクトル
    /// @return `"x y z"`形式の文字列
    std::string FormatVector(const igesio::Vector3d& v) const {
        return FormatFixed(v.x(), options.decimals) + " "
               + FormatFixed(v.y(), options.decimals) + " "
               + FormatFixed(v.z(), options.decimals);
    }
    /// @brief 出力する工具軸を決める (レコード → 直前値 → +Z の優先順)
    /// @param axis レコード自身の工具軸
    /// @return 工具軸方向
    igesio::Vector3d ResolveAxis(const std::optional<igesio::Vector3d>& axis) {
        if (axis.has_value()) return *axis;
        if (state.tool_axis.has_value()) return *state.tool_axis;
        ++missing_axis;
        return igesio::Vector3d::UnitZ();
    }
    /// @brief 1組 (座標、軸、コード) を出力する
    /// @param point 制御点
    /// @param axis 工具軸方向
    /// @param kind 動作の種類
    void Emit(const igesio::Vector3d& point, const igesio::Vector3d& axis,
              const MotionKind kind) {
        lines.push_back(FormatVector(point));
        lines.push_back(FormatVector(axis));
        lines.push_back(std::string(kind == MotionKind::kRapid ? kRapidCode
                                                                : kCutCode));
    }
    /// @brief 直線移動を出力する (制御点を持つ`kWork`のみ)
    /// @param record 出力するレコード
    void operator()(const ClGoto& record) {
        if (record.frame == MotionFrame::kMachine || !record.point.has_value()) {
            ++omitted;
            return;
        }

        Emit(*record.point, ResolveAxis(record.tool_axis), record.kind);
    }
    /// @brief 円弧を折れ線化して出力する
    /// @param record 出力するレコード
    /// @note 始点が分からない、または法線がゼロベクトルの場合は省く
    void operator()(const ClArc& record) {
        if (!state.position.has_value()
            || record.normal.norm() < kDegenerateTolerance) {
            ++invalid_arcs;
            return;
        }

        const igesio::Vector3d axis = ResolveAxis(record.tool_axis);
        for (const igesio::Vector3d& point :
             DiscretizeArc(*state.position, record, kTripletChordTolerance)) {
            Emit(point, axis, MotionKind::kLinear);
        }
    }
    /// @brief ドウェルを省く (表現不可)
    void operator()(const ClDwell&) { ++omitted; }
    /// @brief 送りを省く (表現不可)
    void operator()(const ClFeed&) { ++omitted; }
    /// @brief 主軸を省く (表現不可)
    void operator()(const ClSpindle&) { ++omitted; }
    /// @brief クーラントを省く (表現不可)
    void operator()(const ClCoolant&) { ++omitted; }
    /// @brief 工具の選択を省く (表現不可)
    void operator()(const ClLoadTool&) { ++omitted; }
    /// @brief ワーク座標系の選択を省く (表現不可)
    void operator()(const ClSelectWorkOffset&) { ++omitted; }
    /// @brief 工具長補正を省く (表現不可)
    void operator()(const ClLengthOffset&) { ++omitted; }
    /// @brief コメントを省く (表現不可)
    void operator()(const ClComment&) { ++omitted; }
    /// @brief 区切りを省く (表現不可)
    void operator()(const ClMarker&) { ++omitted; }
    /// @brief 変換せずに保持する文字列を省く (表現不可)
    void operator()(const ClPassThrough&) { ++omitted; }
    /// @brief 終了を省く (表現不可)
    void operator()(const ClEnd&) { ++omitted; }
};

/// @brief 3行1組の文字列を作る
/// @param program 出力するプログラム
/// @param options 出力設定
/// @param[out] warnings 省いたレコードの警告 (`nullptr`なら報告しない)
/// @return 3行1組の文字列
std::string WriteTriplet(const ClProgram& program, const ClWriteOptions& options,
                         std::vector<Diagnostic>* warnings) {
    TripletWriter writer(options);
    for (const ClRecord& record : program.records) {
        std::visit(writer, record);
        writer.state.Apply(record);
    }

    if (writer.omitted > 0) {
        PushWarning(warnings, std::to_string(writer.omitted)
                    + " record(s) cannot be represented in triplet CL and were omitted");
    }
    if (writer.invalid_arcs > 0) {
        PushWarning(warnings, std::to_string(writer.invalid_arcs)
                    + " arc(s) with unknown start or zero normal were omitted");
    }
    if (writer.missing_axis > 0) {
        PushWarning(warnings, std::to_string(writer.missing_axis)
                    + " motion(s) had no tool axis; +Z assumed");
    }
    return JoinLines(writer.lines, options.newline);
}



/**
 * ---- APTの読込 ----
 */

/// @brief APT文 (メジャーワードと引数)
struct AptStatement {
    /// @brief メジャーワード (大文字)
    std::string major;
    /// @brief `/`以降の引数 (`,`区切り. 無ければ空)
    std::vector<std::string> args;
    /// @brief `/`以降の文字列 (`PARTNO`/`PPRINT`の本文用. 無ければ空)
    std::string rest;
};

/// @brief 1行をAPT文に分ける
/// @param text 1行の本文 (前後の空白は除去済み)
/// @return メジャーワードと引数
/// @note `PARTNO`/`PPRINT`は`/`を使わない形式 (`PPRINT text`) も受理する
AptStatement ParseStatement(const std::string& text) {
    AptStatement statement;
    const std::size_t slash = text.find('/');
    const std::string upper = ToUpper(text);
    if (upper.rfind("PARTNO", 0) == 0 || upper.rfind("PPRINT", 0) == 0) {
        statement.major = upper.substr(0, 6);
        std::string rest(Trim(text.substr(6)));
        if (!rest.empty() && rest.front() == '/') {
            rest = std::string(Trim(rest.substr(1)));
        }
        statement.rest = rest;
        return statement;
    }
    if (slash == std::string::npos) {
        statement.major = ToUpper(Trim(text));
        return statement;
    }

    statement.major = ToUpper(Trim(text.substr(0, slash)));
    statement.rest = std::string(Trim(text.substr(slash + 1)));
    statement.args = Split(statement.rest, ',');
    return statement;
}

/// @brief 引数の数を検査する
/// @param statement 検査するAPT文
/// @param min 引数の最小数
/// @param max 引数の最大数
/// @param line 行番号 (文言用)
/// @throw igesio::DataFormatError 引数の数が`min`〜`max`に無い場合
void RequireArgs(const AptStatement& statement, const std::size_t min,
                 const std::size_t max, const int line) {
    if (statement.args.size() < min || statement.args.size() > max) {
        throw igesio::DataFormatError("CL line " + std::to_string(line) + ": "
                                      + statement.major + " expects " + std::to_string(min)
                                      + (min == max ? "" : "-" + std::to_string(max))
                                      + " argument(s)");
    }
}

/// @brief 引数を実数列に変換する
/// @param statement 対象のAPT文
/// @param line 行番号 (文言用)
/// @return 実数の一覧 (引数の順)
/// @throw igesio::DataFormatError 数値でない引数がある場合 (`ParseReal`から伝播)
std::vector<double> ArgsAsReals(const AptStatement& statement, const int line) {
    std::vector<double> values;
    for (const std::string& arg : statement.args) values.push_back(ParseReal(arg, line));
    return values;
}

/// @brief APTの読込状態
struct AptReader {
    /// @brief 読込設定で初期化する
    /// @param read_options 読込設定 (読込が終わるまで有効であること)
    explicit AptReader(const ClReadOptions& read_options) : options(read_options) {}

    /// @brief 読込設定
    const ClReadOptions& options;
    /// @brief 読込結果
    ClProgram program;
    /// @brief `MULTAX/ON`中か
    bool multax = false;
    /// @brief 直前の`RAPID`が次の`GOTO`を待っているか
    bool pending_rapid = false;
    /// @brief 直前の`CIRCLE` (次の`GOTO`が円弧の終点)
    std::optional<ClArc> pending_circle;

    /// @brief レコードを追加する
    /// @param record 追加するレコード
    /// @param line 元ファイルの行番号
    void Push(ClRecord record, const int line) {
        PushRecord(program, std::move(record), options.program_index, line);
    }
    /// @brief `GOTO`の工具軸を読む
    /// @param values `GOTO`の引数 (3つまたは6つ)
    /// @param line 行番号
    /// @return 工具軸方向 (正規化済み). 引数が3つなら`std::nullopt`
    /// @note `MULTAX/OFF`中の工具軸は警告して無視する
    /// @throw igesio::DataFormatError 工具軸がゼロベクトルの場合
    std::optional<igesio::Vector3d> ReadAxis(const std::vector<double>& values,
                                             const int line) {
        if (values.size() != 6) return std::nullopt;
        if (!multax) {
            PushWarning(&program.warnings, "tool axis ignored (MULTAX/OFF)", line);
            return std::nullopt;
        }
        return NormalizeDirection(ToVector(values, 3), line, "tool axis");
    }
    /// @brief `GOTO`を読む (直前の`CIRCLE`があれば円弧の終点にする)
    /// @param statement APT文
    /// @param line 行番号
    /// @throw igesio::DataFormatError 引数が3つでも6つでもない、または数値の
    ///        不備がある場合
    void ReadGoto(const AptStatement& statement, const int line) {
        if (statement.args.size() != 3 && statement.args.size() != 6) {
            throw igesio::DataFormatError("CL line " + std::to_string(line)
                                          + ": GOTO expects 3 or 6 values");
        }

        const std::vector<double> values = ArgsAsReals(statement, line);
        const std::optional<igesio::Vector3d> axis = ReadAxis(values, line);
        if (pending_circle.has_value()) {
            ClArc arc = *pending_circle;
            arc.end = ToVector(values, 0);
            arc.tool_axis = axis;
            pending_circle.reset();
            pending_rapid = false;
            Push(arc, line);
            return;
        }

        ClGoto motion;
        motion.kind = pending_rapid ? MotionKind::kRapid : MotionKind::kLinear;
        motion.point = ToVector(values, 0);
        motion.tool_axis = axis;
        pending_rapid = false;
        Push(motion, line);
    }
    /// @brief `FEDRAT/f[,IPM|MMPM]`を読む (毎分 → mm/s)
    /// @param statement APT文
    /// @param line 行番号
    /// @throw igesio::DataFormatError 引数の数または数値の不備がある場合
    void ReadFedrat(const AptStatement& statement, const int line) {
        RequireArgs(statement, 1, 2, line);
        double feed = ParseReal(statement.args[0], line);
        if (statement.args.size() == 2) {
            const std::string unit = ToUpper(statement.args[1]);
            if (unit == "IPM") {
                feed *= kInchToMillimeter;
            } else if (unit != "MMPM") {
                PushWarning(&program.warnings, "unknown feed unit '"
                            + statement.args[1] + "'; MMPM assumed", line);
            }
        }
        Push(ClFeed{feed / kSecondsPerMinute}, line);
    }
    /// @brief `SPINDL/rpm[,CLW|CCLW]`または`SPINDL/OFF`を読む
    /// @param statement APT文
    /// @param line 行番号
    /// @throw igesio::DataFormatError 引数の数または数値の不備がある場合
    void ReadSpindl(const AptStatement& statement, const int line) {
        RequireArgs(statement, 1, 2, line);
        ClSpindle spindle;
        if (ToUpper(statement.args[0]) == "OFF") {
            Push(spindle, line);
            return;
        }

        spindle.mode = ClSpindle::Mode::kCw;
        spindle.rpm = ParseReal(statement.args[0], line);
        if (statement.args.size() == 2 && ToUpper(statement.args[1]) == "CCLW") {
            spindle.mode = ClSpindle::Mode::kCcw;
        }
        Push(spindle, line);
    }
    /// @brief `ON`/`OFF`の引数を読む
    /// @param statement APT文
    /// @param line 行番号
    /// @return `ON`なら`true`
    /// @throw igesio::DataFormatError 引数が1つでない、または`ON`/`OFF`以外の場合
    bool ReadOnOff(const AptStatement& statement, const int line) const {
        RequireArgs(statement, 1, 1, line);
        const std::string value = ToUpper(statement.args[0]);
        if (value == "ON") return true;
        if (value == "OFF") return false;
        throw igesio::DataFormatError("CL line " + std::to_string(line) + ": "
                                      + statement.major + " expects ON or OFF");
    }
    /// @brief `CIRCLE/cx,cy,cz,nx,ny,nz,r`を読む (次の`GOTO`を待つ)
    /// @param statement APT文
    /// @param line 行番号
    /// @throw igesio::DataFormatError 引数が7つでない、数値の不備がある,
    ///        または法線がゼロベクトルの場合
    void ReadCircle(const AptStatement& statement, const int line) {
        RequireArgs(statement, 7, 7, line);
        const std::vector<double> values = ArgsAsReals(statement, line);
        ClArc arc;
        arc.kind = MotionKind::kArcCcw;
        arc.center = ToVector(values, 0);
        arc.normal = NormalizeDirection(ToVector(values, 3), line, "circle normal");
        pending_circle = arc;
    }
    /// @brief 1行を読む
    /// @param numbered 行番号付きの行
    /// @throw igesio::DataFormatError 各文の引数または数値に不備がある場合
    ///        (各`Read*`から伝播)
    void ReadLine(const NumberedLine& numbered) {
        const std::string& text = numbered.text;
        const int line = numbered.line;
        if (text.rfind("$$", 0) == 0) {
            Push(ClComment{std::string(Trim(text.substr(2)))}, line);
            return;
        }

        const AptStatement statement = ParseStatement(text);
        if (statement.major == "PARTNO") {
            program.name = statement.rest;
        } else if (statement.major == "PPRINT") {
            Push(ClComment{statement.rest}, line);
        } else if (statement.major == "GOTO") {
            ReadGoto(statement, line);
        } else if (statement.major == "RAPID") {
            pending_rapid = true;
        } else if (statement.major == "FEDRAT") {
            ReadFedrat(statement, line);
        } else if (statement.major == "LOADTL") {
            RequireArgs(statement, 1, 1, line);
            Push(ClLoadTool{static_cast<int>(
                    std::lround(ParseReal(statement.args[0], line)))}, line);
        } else if (statement.major == "SPINDL") {
            ReadSpindl(statement, line);
        } else if (statement.major == "COOLNT") {
            Push(ClCoolant{ReadOnOff(statement, line)}, line);
        } else if (statement.major == "DELAY") {
            RequireArgs(statement, 1, 1, line);
            Push(ClDwell{ParseReal(statement.args[0], line)}, line);
        } else if (statement.major == "MULTAX") {
            multax = ReadOnOff(statement, line);
        } else if (statement.major == "CIRCLE") {
            ReadCircle(statement, line);
        } else if (statement.major == "FINI") {
            Push(ClEnd{}, line);
        } else {
            PushWarning(&program.warnings,
                        "unknown APT statement: " + statement.major, line);
            Push(ClPassThrough{"apt", text}, line);
        }
    }
};

/// @brief APTの文字列を読み込む
/// @param text CLデータ
/// @param options 読込設定
/// @return CLプログラム
/// @throw igesio::DataFormatError 各文の引数または数値に不備がある場合
///        (`AptReader::ReadLine`から伝播)
ClProgram ReadApt(const std::string& text, const ClReadOptions& options) {
    AptReader reader(options);
    for (const NumberedLine& line : SelectLines(text, options)) reader.ReadLine(line);
    return std::move(reader.program);
}



/**
 * ---- APTの出力 ----
 */

/// @brief APTの出力状態 (`std::visit`でレコードを出力する)
struct AptWriter {
    /// @brief 出力設定で初期化する
    /// @param write_options 出力設定 (出力が終わるまで有効であること)
    explicit AptWriter(const ClWriteOptions& write_options) : options(write_options) {}

    /// @brief 出力設定
    const ClWriteOptions& options;
    /// @brief 出力行
    std::vector<std::string> lines;
    /// @brief 直前までの状態
    ClState state;
    /// @brief `MULTAX/ON`を出力済みか
    bool multax = false;
    /// @brief 表現できずに省いたレコードの数
    std::size_t omitted = 0;
    /// @brief `PPRINT`にした状態レコードの数
    std::size_t pprint_state = 0;
    /// @brief `PPRINT`にした他方言の`ClPassThrough`の数
    std::size_t pprint_passthrough = 0;
    /// @brief `FINI`を出力済みか
    bool ended = false;

    /// @brief 実数を整形する
    /// @param value 整形する値
    /// @return `options.decimals`桁の固定小数
    std::string Real(const double value) const {
        return FormatFixed(value, options.decimals);
    }
    /// @brief 3次元ベクトルを`,`区切りで整形する
    /// @param v 整形するベクトル
    /// @return `"x,y,z"`形式の文字列
    std::string FormatVector(const igesio::Vector3d& v) const {
        return Real(v.x()) + "," + Real(v.y()) + "," + Real(v.z());
    }
    /// @brief `GOTO`行を出力する
    /// @param point 制御点
    /// @param own_axis レコード自身の工具軸 (無ければ`MULTAX/ON`中に限り直前値を継続)
    /// @param rapid `true`なら直前に`RAPID`を出力する
    /// @note 工具軸を初めて出力する前に`MULTAX/ON`を出力する (`RAPID`より前)
    void EmitGoto(const igesio::Vector3d& point,
                  const std::optional<igesio::Vector3d>& own_axis, const bool rapid) {
        std::optional<igesio::Vector3d> axis = own_axis;
        if (!axis.has_value() && multax) axis = state.tool_axis;
        if (axis.has_value() && !multax) {
            lines.push_back("MULTAX/ON");
            multax = true;
        }
        if (rapid) lines.push_back("RAPID");
        std::string line = "GOTO/" + FormatVector(point);
        if (axis.has_value()) line += "," + FormatVector(*axis);
        lines.push_back(line);
    }
    /// @brief 直線移動を出力する (制御点を持つ`kWork`のみ)
    /// @param record 出力するレコード
    void operator()(const ClGoto& record) {
        if (record.frame == MotionFrame::kMachine || !record.point.has_value()) {
            ++omitted;
            return;
        }

        EmitGoto(*record.point, record.tool_axis, record.kind == MotionKind::kRapid);
    }
    /// @brief 円弧を`CIRCLE`+`GOTO`で出力する (時計回りは法線を反転する)
    /// @param record 出力するレコード
    void operator()(const ClArc& record) {
        const igesio::Vector3d normal =
                record.kind == MotionKind::kArcCw ? igesio::Vector3d(-record.normal)
                                                  : record.normal;
        const double radius = state.position.has_value()
                              ? (*state.position - record.center).norm() : 0.0;
        lines.push_back("CIRCLE/" + FormatVector(record.center) + ","
                        + FormatVector(normal) + "," + Real(radius));
        EmitGoto(record.end, record.tool_axis, false);
    }
    /// @brief ドウェルを出力する
    /// @param record 出力するレコード
    void operator()(const ClDwell& record) {
        lines.push_back("DELAY/" + Real(record.seconds));
    }
    /// @brief 送りを毎分で出力する
    /// @param record 出力するレコード
    void operator()(const ClFeed& record) {
        lines.push_back("FEDRAT/" + Real(record.mm_per_s * kSecondsPerMinute) + ",MMPM");
    }
    /// @brief 主軸を出力する
    /// @param record 出力するレコード
    /// @note 回転数が省略された場合は直前値、それも無ければ0を出力する
    void operator()(const ClSpindle& record) {
        if (record.mode == ClSpindle::Mode::kOff) {
            lines.push_back("SPINDL/OFF");
            return;
        }

        const double rpm = record.rpm.has_value() ? *record.rpm
                                                  : state.spindle.rpm.value_or(0.0);
        lines.push_back("SPINDL/" + FormatFixed(rpm, 0) + ","
                        + (record.mode == ClSpindle::Mode::kCw ? "CLW" : "CCLW"));
    }
    /// @brief クーラントを出力する
    /// @param record 出力するレコード
    void operator()(const ClCoolant& record) {
        lines.push_back(record.on ? "COOLNT/ON" : "COOLNT/OFF");
    }
    /// @brief 工具の選択を出力する
    /// @param record 出力するレコード
    void operator()(const ClLoadTool& record) {
        lines.push_back("LOADTL/" + std::to_string(record.number));
    }
    /// @brief ワーク座標系の選択を`PPRINT`で出力する (APTに対応する文が無い)
    /// @param record 出力するレコード
    void operator()(const ClSelectWorkOffset& record) {
        lines.push_back("PPRINT WORK OFFSET " + record.id);
        ++pprint_state;
    }
    /// @brief 工具長補正を`PPRINT`で出力する (APTに対応する文が無い)
    /// @param record 出力するレコード
    void operator()(const ClLengthOffset& record) {
        lines.push_back("PPRINT LENGTH OFFSET "
                        + (record.number.has_value() ? std::to_string(*record.number)
                                                     : std::string("OFF")));
        ++pprint_state;
    }
    /// @brief コメントを出力する
    /// @param record 出力するレコード
    void operator()(const ClComment& record) {
        if (options.comments) lines.push_back("PPRINT " + record.text);
    }
    /// @brief 区切りを`PPRINT`で出力する
    /// @param record 出力するレコード
    void operator()(const ClMarker& record) {
        if (!options.comments) return;

        std::string kind = "OPERATION";
        if (record.kind == ClMarker::Kind::kPathBegin) kind = "PATH BEGIN";
        if (record.kind == ClMarker::Kind::kPathEnd) kind = "PATH END";
        lines.push_back("PPRINT " + kind + " " + record.name);
    }
    /// @brief 変換せずに保持する文字列を出力する (同じ方言ならそのまま、違えば`PPRINT`)
    /// @param record 出力するレコード
    void operator()(const ClPassThrough& record) {
        if (record.dialect == "apt") {
            lines.push_back(record.text);
            return;
        }

        lines.push_back("PPRINT " + record.text);
        ++pprint_passthrough;
    }
    /// @brief 終了を出力する
    void operator()(const ClEnd&) {
        lines.push_back("FINI");
        ended = true;
    }
};

/// @brief APTの文字列を作る
/// @param program 出力するプログラム
/// @param options 出力設定
/// @param[out] warnings 省いたレコードと`PPRINT`化の警告/情報 (`nullptr`なら
///        報告しない)
/// @return APTの文字列
std::string WriteApt(const ClProgram& program, const ClWriteOptions& options,
                     std::vector<Diagnostic>* warnings) {
    AptWriter writer(options);
    if (!program.name.empty()) writer.lines.push_back("PARTNO/" + program.name);
    for (const ClRecord& record : program.records) {
        // 終了後のレコードは出力しない
        if (writer.ended) {
            ++writer.omitted;
            continue;
        }
        std::visit(writer, record);
        writer.state.Apply(record);
    }
    if (!writer.ended) writer.lines.push_back("FINI");

    if (writer.omitted > 0) {
        PushWarning(warnings, std::to_string(writer.omitted)
                    + " record(s) cannot be represented in APT and were omitted");
    }
    if (writer.pprint_state > 0) {
        PushDiagnostic(warnings, Severity::kInfo,
                       std::to_string(writer.pprint_state)
                       + " record(s) written as PPRINT");
    }
    if (writer.pprint_passthrough > 0) {
        PushWarning(warnings, std::to_string(writer.pprint_passthrough)
                    + " pass-through record(s) of another dialect written as PPRINT");
    }
    return JoinLines(writer.lines, options.newline);
}

}  // namespace



ClProgram ReadClText(const std::string& text, const ClFileFormat format,
                     const ClReadOptions& options) {
    ClProgram program = format == ClFileFormat::kTriplet ? ReadTriplet(text, options)
                                                          : ReadApt(text, options);
    if (options.unit_scale != 1.0) ScaleLengths(program, options.unit_scale);
    return program;
}

ClProgram ReadClFile(const std::filesystem::path& path, const ClFileFormat format,
                     const ClReadOptions& options) {
    return ReadClText(ReadTextFile(path), format, options);
}

std::string WriteClText(const ClProgram& program, const ClFileFormat format,
                        const ClWriteOptions& options,
                        std::vector<Diagnostic>* warnings) {
    return format == ClFileFormat::kTriplet ? WriteTriplet(program, options, warnings)
                                            : WriteApt(program, options, warnings);
}

void WriteClFile(const std::filesystem::path& path, const ClProgram& program,
                 const ClFileFormat format, const ClWriteOptions& options,
                 std::vector<Diagnostic>* warnings) {
    // 文字列化の例外はファイルを作る前に送出する
    const std::string text = WriteClText(program, format, options, warnings);
    WriteTextFile(path, text);
}

}  // namespace igesio::extensions::machines
