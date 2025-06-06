/**
 * @file utils/iges_string_utils.cpp
 * @brief IGESデータ文字列の検証・変換・生成を行う関数群
 * @author Yayoi Habami
 * @date 2025-04-11
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/utils/iges_string_utils.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "igesio/common/serialization.h"

namespace {

/// @brief igesio 名前空間のエイリアス
namespace iio = igesio;

/// @brief igesio::utils 名前空間のエイリアス
namespace i_util = igesio::utils;

}  // namespace



/**
 * 基本操作
 */

bool i_util::IsOnlySpace(const std::string& s) {
    // 文字列が空であれば、false
    if (s.empty()) return false;

    return s.find_first_not_of(" ") == std::string::npos;
}



/**
 * IGES文字列の検証
 */

void i_util::AssertLength(const std::string& line,
                          const bool is_compressed) {
    if (line.size() > kMaxColumn) {
        // どの形式であっても、最大長さを超えている場合はエラー
        throw iio::LineFormatError(
                "A single line of IGES is up to " + std::to_string(kMaxColumn) +
                " characters long, but this line is " + std::to_string(line.size()) +
                " characters long: '" + line + "'");
    } else if (line.size() < kMaxColumn) {
        if (!is_compressed) {
            // 圧縮形式でない場合、長さが規定値以外の場合はエラー
            throw iio::LineFormatError(
                    "A single line of IGES must be " + std::to_string(kMaxColumn) +
                    " characters long (if not in compressed format), but this line is " +
                    std::to_string(line.size()) + " characters long: '" + line + "'");
        } else if (line.size() >= kColIdentify) {
            // 73文字以上80文字未満の行は存在しない
            throw iio::LineFormatError(
                    "No lines longer than " + std::to_string(kColIdentify-1) +
                    " characters and shorter than " + std::to_string(kMaxColumn) +
                    " characters in IGES format: '" + line + "'");
        }
        // それ以外の場合（圧縮形式で73文字未満）は、データ部の行であるため問題なし
    }
}



/**
 * シンプルな操作 (IGES文字列)
 */

igesio::SectionType i_util::GetSectionType(const std::string& line,
                                           const bool is_compressed) {
    // 1行の長さを検証
    AssertLength(line, is_compressed);
    // 長さの検証が通り、かつ73文字未満の場合は圧縮形式のデータ部
    if (line.size() < kColIdentify) return SectionType::kData;

    char section_char = line[kColIdentify - 1];
    if (is_compressed && (section_char == 'D' || section_char == 'P')) {
        // 圧縮形式の場合、73文字目にDまたはPを持つ行は存在しない
        throw iio::SectionFormatError(
                "Compressed format does not have a line with 'D' or 'P' at "
                + std::to_string(kColIdentify) + " characters: " + line);
    }

    switch (section_char) {
        case 'C': return SectionType::kFlag;
        case 'S': return SectionType::kStart;
        case 'G': return SectionType::kGlobal;
        case 'D': return SectionType::kDirectory;
        case 'P': return SectionType::kParameter;
        case 'T': return SectionType::kTerminate;
        default:
            throw iio::SectionFormatError(
            "Unknown section character: " + std::string(1, section_char));
    }
}

unsigned int i_util::GetSequenceNumber(const std::string& line,
                                       const bool is_compressed) {
    // 1行の長さを検証
    AssertLength(line, is_compressed);

    // 圧縮形式のデータセクションの場合、シーケンス番号は存在しない
    if (GetSectionType(line, is_compressed) == SectionType::kData) {
        throw iio::SectionFormatError(
                "Compressed format does not have a sequence number: "
                + line);
    }

    // シーケンス番号は、行の末尾7桁の数値
    std::string seq_num = line.substr(kColIdentify, 7);
    try {
        // デフォルト値なしで整数に変換
        return FromIgesInteger(seq_num, std::nullopt);
    } catch (const iio::TypeConversionError&) {
        throw iio::SectionFormatError(
                "Invalid sequence number: '" + seq_num + "'");
    }
}

unsigned int i_util::GetDEPointer(const std::string& line) {
    // パラメータセクションであることを確認
    if (GetSectionType(line, false) != SectionType::kParameter) {
        throw iio::SectionFormatError(
                "The line is not a parameter section: " + line);
    }

    std::string pd_pointer = line.substr(kColDEPointer - 1, kFixedColWidth);
    try {
        // デフォルト値なしで整数に変換
        return FromIgesInteger(pd_pointer, std::nullopt);
    } catch (const iio::TypeConversionError&) {
        throw iio::SectionFormatError(
                "Invalid DE Pointer: '" + pd_pointer + "'");
    }
}

std::string i_util::GetDataPart(const std::string& line,
                                const SectionType section_type) {
    std::string error_msg = "The length of the line is too short "
            "for the " + SectionTypeToString(section_type) + " section: '";

    switch (section_type) {
        case SectionType::kFlag:
            // フラグセクションはデータなし
            return "";
        case SectionType::kTerminate:
            // ターミネートセクションは8x4=32文字までデータ部
            if (line.size() < kColTerminateDataPart) {
                throw iio::LineFormatError(error_msg + line + "'");
            }
            return line.substr(0, kColTerminateDataPart);
        case SectionType::kParameter:
            // パラメータセクションはDEポインタの1文字前までデータ部
            if (line.size() < kColDEPointer - 1) {
                throw iio::LineFormatError(error_msg + line + "'");
            }
            return line.substr(0, kColDEPointer - 1);
        case SectionType::kData:
            // 圧縮形式のデータセクションはセクション判別文字の1文字前まで
            if (line.size() >= kColIdentify) {
                throw iio::LineFormatError("The line is not a data section: '" + line + "'");
            }
            return line;
        default:
            // その他はセクション判別文字の1文字前までデータ部
            if (line.size() < kColIdentify - 1) {
                throw iio::LineFormatError(error_msg + line + "'");
            }
            return line.substr(0, kColIdentify - 1);
    }
}

namespace {

/// @brief 与えられた文字列の冒頭にあるH文字列の長さを取得する
/// @param str 文字列
/// @return H文字列の長さ
/// @note 与えられた文字列の先頭にH文字列が存在しない場合 ('12,4Hello'等) は0を返す.
///       具体的な処理としては、strに初めてHが現れるまでの文字が、
///       すべて数字であればその数値を返し、それ以外の場合は0を返す.
int GetHStringLength(const std::string& str) {
    // 文字列中にHが存在しない場合は0を返す
    auto h_pos = str.find('H');
    if (h_pos == std::string::npos) return 0;

    // 最初にHが現れるまでの文字がすべて数字かどうかを確認
    int h_length = 0;
    for (size_t i = 0; i < h_pos; ++i) {
        // 最初のHまでの文字列に数字以外の文字が含まれている場合は0を返す
        if (!('0' <= str[i] && str[i] <= '9')) return 0;
        h_length = h_length * 10 + (str[i] - '0');
    }
    return h_length;
}

/// @brief 自由形式のデータから1つのパラメータを取得する
/// @param str 文字列
/// @param p_delim パラメータ区切り文字
/// @param r_delim レコード区切り文字
/// @return 文字列のペア:
///   - std::string: パラメータの文字列
///   - std::string: 残った文字列
/// @note 例えば`"12,4Hello,3.14"`のような文字列を与えた場合、
///       `"12"`と`",4Hello,3.14"`を返す. また、パラメータの前に半角スペースがある場合、
///       例えば`"   12,4Hello,3.14"`のような文字列を与えた場合には、それらを削除して
///       `"12"`と`",4Hello,3.14"`を返す.
/// @throw igesio::SectionFormatError 区切り文字が存在しない場合、
///        不正な文字列ステートメント（H文字列）が含まれる場合
std::pair<std::string, std::string>
GetParameterString(const std::string& str, const char p_delim, const char r_delim) {
    // 冒頭のスペースを削除
    auto tstr = i_util::ltrim(str);

    // 冒頭のパラメータの終端位置を取得 (end of parameter)
    int eop = 0;
    auto h_length = GetHStringLength(tstr);
    if (h_length == 0) {
        // 現在のパラメータはH文字列ではない -> 区切り文字まで取得
        eop = std::min(tstr.find(p_delim), tstr.find(r_delim));
    } else {
        // 現在のパラメータはH文字列 -> H文字列分だけ取得
        eop = tstr.find('H') + h_length + 1;
    }

    if (eop == std::string::npos) {
        // 区切り文字が存在しない場合はエラー
        throw iio::SectionFormatError(
                "No delimiter exists in the string: '" + tstr + "'");
    } else if (eop + 1 > tstr.size() ||
               tstr[eop] != p_delim && tstr[eop] != r_delim) {
        // パラメータ終端位置の後ろに区切り文字がない場合はエラー
        throw iio::SectionFormatError(
                "No delimiter exists after the parameter (at pos " +
                std::to_string(eop + 1) + "): '" + tstr + "'");
    }

    return {tstr.substr(0, eop), tstr.substr(eop)};
}

}  // namespace

std::vector<std::string> i_util::ParseFreeFormattedData(
        const std::vector<std::string>& lines,
        const char p_delim, const char r_delim) {
    // 全ての文字列を単純に結合
    std::string connected = "";
    for (const auto& line : lines) connected += line;

    // データごとに読み込み
    std::vector<std::string> result = {};
    while (true) {
        auto [param, rest] = GetParameterString(connected, p_delim, r_delim);
        result.push_back(param);

        // 区切り文字がレコード区切り文字であれば終了
        if (rest[0] == r_delim) break;

        // 区切り文字を削除
        connected = rest.substr(1);
        if (connected.empty()) {
            // 文字列がなくなるまでにレコード区切り文字が存在しない場合はエラー
            throw iio::SectionFormatError(
                    "No record delimiter '" + std::string(1, r_delim) +
                    "' exists in the string: '" + connected + "'");
        }
    }

    return result;
}



/**
 * IGESの各行の読み込み
 */

namespace {

/// @brief 無効な区切り文字
/// @brief パラメータ区切り文字とレコード区切り文字として使用できない文字
constexpr std::string_view kInvalidDelimiters = "Invalid delimiters: "
"control characters (0x00-0x1F, 0x7F), space (' '; 0x20), digits ('0'-'9'; 0x30-0x39), "
"'+', '-', '.' (0x2B, 0x2D, 0x2E), 'D', 'E', 'H' (0x44, 0x45, 0x48)";

}  // namespace

bool i_util::IsValidDelimiter(const char c) {
    // 制御文字
    if (c <= 0x1F || c == 0x7F) return false;

    // スペース
    if (c == 0x20) return false;

    // 半角数字
    if (c >= 0x30 && c <= 0x39) return false;

    // '+', '-', '.'
    if (c == 0x2B || c == 0x2D || c == 0x2E) return false;

    // 'D', 'E', 'H'
    if (c == 0x44 || c == 0x45 || c == 0x48) return false;

    // それ以外はOK
    return true;
}

char i_util::GetParameterDelimiter(const std::string& line) {
    // 1行目の1文字目が','の場合、区切り文字は','
    if (line[0] == ',') return ',';

    // 1行目の冒頭の文字が','ではない場合、区切り文字が異なる可能性がある
    // 1行目の冒頭の'\dH'の部分（\dは数字、区切り文字の文字数を表す）を取得する
    if (line.size() < 4) {
        // 1行目の長さが4文字未満の場合はエラー
        throw iio::LineFormatError(
                "The length of the first line of the global section is too short (" +
                std::to_string(line.size()) + " characters): '" + line + "'");
    }

    if (line.substr(0, 2) != "1H") {
        // 1行目の冒頭が'1H'ではない場合はエラー
        throw iio::SectionFormatError(
                "The first line of the global section does not start with '1H': '" +
                line + "'");
    }

    // 3文字目と4文字目を取得、これが異なればエラー
    if (line[2] != line[3]) {
        throw iio::SectionFormatError(
                "The first line of the global section does not have the same "
                "character at 3rd and 4th positions: '" + line + "'");
    }

    // 区切り文字が使用可能な文字でない場合はエラー
    if (!IsValidDelimiter(line[2])) {
        throw iio::SectionFormatError(
                "The delimiter '" + std::string(1, line[2]) +
                "' is not a valid delimiter (" + std::string(kInvalidDelimiters) +
                "): '" + line + "'");
    }
    return line[2];
}

char i_util::GetRecordDelimiter(const std::string& line,
                        const char p_delim) {
    auto error_msg = "The parameter delimiter is incorrectly specified"
                     " in the first line of the global section: '" + line + "'";

    // 1つめのp_delimが見つからない場合はエラー
    std::size_t pos1 = line.find(p_delim);
    if (line.substr(0, 2) == "1H" && line[2] == p_delim) {
        // パラメータ区切り文字が'1Hα'による指定の場合は、pos1を一つずらす
        pos1 = line.find(p_delim, pos1 + 1);
    }
    if (pos1 == std::string::npos) throw iio::SectionFormatError(error_msg);

    // 2つめのp_delimが見つからない場合はエラー
    std::size_t pos2 = line.find(p_delim, pos1 + 1);
    if (pos2 == std::string::npos) throw iio::SectionFormatError(error_msg);

    // 1つめのp_delimから、2つめのp_delimまでの文字列を取得
    std::string record_delim = line.substr(pos1 + 1, pos2 - pos1 - 1);
    // これが空文字列であれば区切り文字を';'として返す
    if (record_delim.empty()) return ';';

    try {
        auto delim = FromIgesString(record_delim, std::nullopt);
        if (delim.size() != 1 || !IsValidDelimiter(delim[0])) {
            // 区切り文字が使用可能な文字でない場合はエラー
            throw iio::SectionFormatError(
                    "The delimiter '" + delim +
                    "' is not a valid delimiter (" + std::string(kInvalidDelimiters) +
                    "): '" + line + "'");
        }
        return delim[0];
    } catch (iio::TypeConversionError& e) {
        // 変換できなかった場合はエラー
        throw iio::TypeConversionError(
                "The record delimiter is incorrectly specified"
                " in the first line of the global section: '" + line + "'\n" +
                e.what());
    }
}



/**
 * 出力
 */

namespace {

/// @brief current_lineの後ろに、parameterを追加する
/// @param current_line 文字列を付け足そうとしている行
/// @param parameter String型のパラメータ
/// @param max_length 最大行長
/// @return 追加後の行
/// @note 例として、`max_length = 10`、`current_line = "-0.1234,"`のとき、
///       `parameter = "5HHello"`であれば`{"-0.1234,5H", "Hello"}`を返す.
///       `parameter = "18Hbut depth of life."`であれば、
///       `{"-0.1234, "18Hbut dep", "th of life", "."}`を返す (String型の
///       冒頭の数字とHまでは同じ行に表示する必要があるため).
/// @throw igesio::DataFormatError parameterがString型として不正な場合
std::vector<std::string> AppendString(
        const std::string& current_line, const std::string& parameter,
        const unsigned int max_length) {
    // 空文字列の場合は、基本的にそのまま返す
    if (parameter.empty()) {
        // 現在の行にカンマを追加すると長さが超える場合は、改行して新しい行を作成
        if (current_line.size() + 1 > max_length) return {current_line, ""};

        return {current_line + ""};
    }

    // Hの位置を取得
    int h_pos = parameter.find('H');
    if (h_pos == std::string::npos) {
        // Hが存在しない場合はエラー
        throw iio::DataFormatError(
                "Invalid string parameter format: '" + parameter + "'");
    }

    std::vector<std::string> result;
    std::string new_line = current_line;
    if ((parameter.size() + current_line.size()) % max_length == 0) {
        // 単純に文字列を配置すると、末尾にカンマが配置出来なくなる場合
        //   例) 上の例で`current_line = "-0.12,"`、`parameter = "2Hok"`の場合、
        //   新しい行は`"-0.12,2Hok"`となり、末尾にカンマが配置できない.
        //   そのため、現在の行をresultに追加し、改行する必要がある.
        new_line.append(max_length - current_line.size(), ' ');
        result.push_back(new_line);
        new_line.clear();
    } else if (new_line.size() + h_pos + 1 > max_length) {
        // 現在の行に文字列のHまで入らない場合 ('5HHello'のとき'5'で改行が必要な場合)
        // 現在の行をresultに追加
        new_line.append(max_length - current_line.size(), ' ');
        result.push_back(new_line);
        new_line.clear();
    }

    std::size_t pos = 0;
    while (pos < parameter.size()) {
        // new_lineに入る文字数を計算
        std::size_t space_left = max_length - new_line.size();
        if (space_left == 0) {
            // new_lineがいっぱいの場合は、改行して新しい行を作成
            result.push_back(new_line);
            new_line.clear();
            space_left = max_length;
        }

        // parameterのposから、new_lineに入る文字数分だけ追加
        std::size_t substr_length = std::min(space_left, parameter.size() - pos);
        new_line += parameter.substr(pos, substr_length);
        pos += substr_length;
    }

    result.push_back(new_line);
    return result;
}

}  // namespace

std::vector<std::string>
i_util::ToFreeFormattedLines(
        const std::vector<std::string>& parameters,
        const std::vector<igesio::IGESParameterType>& parameter_types,
        const unsigned int max_line_length,
        const char p_delim, const char r_delim) {
    // parametersとparameter_typesのサイズが一致しない場合はエラー
    if (parameters.size() != parameter_types.size()) {
        throw std::invalid_argument(
                "The size of parameters and parameter_types must be the same: " +
                std::to_string(parameters.size()) + " != " +
                std::to_string(parameter_types.size()));
    }

    std::vector<std::string> lines;
    std::string current_line;
    for (std::size_t i = 0; i < parameters.size(); ++i) {
        // パラメータとその型を取得 (左側に空白がある場合は削除)
        const auto& param = ltrim(parameters[i]);
        const auto& type = parameter_types[i];

        if (type != igesio::IGESParameterType::kString) {
            // String型以外
            if (current_line.size() + param.size() + 1 > max_line_length) {
                // 現在の行にパラメータと区切り文字を追加すると長さが超える場合
                // 現在の行をlinesに追加
                lines.push_back(current_line);
                current_line.clear();
            }
            // 現在の行にパラメータを追加
            current_line += param;
        } else {
            // String型の場合
            auto new_lines = AppendString(current_line, param, max_line_length);
            // new_linesの最後の行をcurrent_lineに設定、それ以外はlinesに追加
            current_line = new_lines.back();
            for (std::size_t j = 0; j < new_lines.size() - 1; ++j) {
                lines.push_back(new_lines[j]);
            }
        }

        // 最後のパラメータでない場合は、区切り文字を追加
        if (i < parameters.size() - 1) current_line += p_delim;
    }
    current_line += r_delim;  // 最後の区切り文字を追加
    // 最後の行をlinesに追加
    lines.push_back(current_line);

    // 各行の長さをmax_line_lengthに合わせる
    for (auto& line : lines) {
        if (line.size() < max_line_length) {
            // 長さがmax_line_length未満の場合、右側を空白で埋める
            line.append(max_line_length - line.size(), ' ');
        }
    }

    return lines;
}

std::vector<std::string>
i_util::ToFreeFormattedLines(
        const igesio::IGESParameterVector& parameters,
        const unsigned int max_line_length,
        const char p_delim, const char r_delim,
        const SerializationConfig& config) {
    std::vector<std::string> params;
    std::vector<igesio::IGESParameterType> types;
    for (size_t i = 0; i < parameters.size(); ++i) {
        params.push_back(parameters.get_as_string(i, config));
        types.push_back(parameters.get_format(i).type);
    }
    return ToFreeFormattedLines(params, types, max_line_length, p_delim, r_delim);
}
