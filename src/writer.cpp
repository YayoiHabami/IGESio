/**
 * @file writer.cpp
 * @brief IGESファイルを読み込むための関数を定義する
 * @author Yayoi Habami
 * @date 2025-05-31
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/writer.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "igesio/common/versions.h"
#include "igesio/common/errors.h"
#include "igesio/common/serialization.h"
#include "igesio/utils/iges_string_utils.h"



namespace {

/// @brief 与えられた各パラメータがString型かどうかを判定する
/// @param parameters パラメータの文字列
/// @return String型のパラメータはkString、それ以外はkIntegerを返す
/// @note 各パラメータが、`\d+H`で始まり、続く文字列が\d+の長さである
///       場合のみkStringとする
/// @note RawEntityPDのdata_typesが空であるか、長さがdataと一致しない
///       場合に、この関数を使用する
std::vector<igesio::IGESParameterType>
ProvisionalParameterTypes(
        const std::vector<std::string>& parameters) {
    std::vector<igesio::IGESParameterType> types;
    for (const auto& param_i : parameters) {
        // パラメータの先頭の空白を削除
        auto param = igesio::ltrim(param_i);

        // 空文字列や、先頭が数字でない場合はString型ではない
        if (param.empty() || param[0] < '0' || param[0] > '9') {
            types.push_back(igesio::IGESParameterType::kInteger);
            continue;
        }

        try {
            igesio::FromIgesString(param, std::nullopt);
            // 変換に成功した場合はString型
            types.push_back(igesio::IGESParameterType::kString);
        } catch (igesio::TypeConversionError&) {
            // 変換に失敗した場合はString型ではない
            types.push_back(igesio::IGESParameterType::kInteger);
        }
    }
    return types;
}

/// @brief シーケンス番号部分の文字列を作成する
/// @param section セクション名を表す1文字
/// @param sequence_number シーケンス番号
/// @return シーケンス番号を表す文字列
/// @note "D", 12であれば "D     12" のようにして返す
std::string SequenceNumberStr(char section, unsigned int sequence_number) {
    std::ostringstream oss;
    oss << section << std::setw(igesio::kFixedColWidth - 1)
        << std::setfill(' ') << sequence_number;
    return oss.str();
}

/// @brief PDセクションのDEバックポインタおよびシーケンス番号部分
/// @param de_sequence_number ディレクトリエントリセクションのシーケンス番号
/// @param pd_sequence_number パラメータデータセクションのシーケンス番号
/// @return PDセクションのDEバックポインタ～シーケンス番号部分
std::string PDSuffix(unsigned int de_sequence_number, unsigned int pd_sequence_number) {
    std::ostringstream oss;
    oss << std::setw(igesio::kFixedColWidth)
        << std::setfill(' ') << de_sequence_number;
    return oss.str() + SequenceNumberStr('P', pd_sequence_number);
}

/// @brief ディレクトリエントリセクションの各レコードのシーケンス番号を
///        昇順にソートする
/// @param de_section ディレクトリエントリセクションの各レコード
/// @return 昇順にソートされたシーケンス番号のリスト
/// @throw igesio::DataFormatError シーケンス番号が1から始まる連続した奇数でない場合
std::vector<unsigned int> SortDeSequenceNumbers(
        const std::vector<igesio::entities::RawEntityDE>& de_section) {
    std::vector<unsigned int> de_sequence_numbers;
    for (const auto& de : de_section) {
        de_sequence_numbers.push_back(de.sequence_number);
    }
    std::sort(de_sequence_numbers.begin(), de_sequence_numbers.end());

    // 1から始まる連続した奇数であることを確認する
    bool is_valid = true;
    if (de_sequence_numbers.empty() || de_sequence_numbers.front() != 1) {
        is_valid = false;
    } else {
        for (size_t i = 0; i < de_sequence_numbers.size(); ++i) {
            if (de_sequence_numbers[i] != 2 * i + 1) {
                is_valid = false;
                break;
            }
        }
    }
    if (!is_valid) {
        std::ostringstream oss;
        for (const auto& sn : de_sequence_numbers) oss << sn << " ";
        throw igesio::DataFormatError(
            "Directory Entry Section sequence numbers must be "
            "a sequence of odd integers starting from 1. ("
            "found: {" + oss.str() + "})");
    }

    return de_sequence_numbers;
}

/// @brief PDセクションの各レコードを文字列化する
/// @param pd_sections パラメータデータセクションの各レコード
/// @param de_sequence_numbers ディレクトリエントリセクションのシーケンス番号
/// @param p_delim パラメータ区切り文字
/// @param r_delim レコード区切り文字
/// @return 各レコードを文字列化した結果のリスト、および各PDレコードの
///         シーケンス番号、各レコードの行数 (後者2つの並びはde_sequence_numbersに従う)
std::tuple<std::vector<std::string>, std::vector<unsigned int>, std::vector<unsigned int>>
SerializePdSection(const std::vector<igesio::entities::RawEntityPD>& pd_sections,
                   const std::vector<unsigned int>& de_sequence_numbers,
                   char p_delim, char r_delim) {
    std::vector<std::string> pd_lines;
    std::vector<unsigned int> pd_sequence_numbers;
    std::vector<unsigned int> pd_lines_count;

    std::size_t pos = 0;
    for (const auto& de_seq : de_sequence_numbers) {
        // 対応するPDレコードを取得
        auto it = std::find_if(pd_sections.begin(), pd_sections.end(),
                               [de_seq](const igesio::entities::RawEntityPD& pd) {
                                   return pd.de_pointer == de_seq;
                               });
        if (it == pd_sections.end()) {
            throw igesio::DataFormatError(
                "No PD record found for DE sequence number: " +
                std::to_string(de_seq));
        }
        const auto& pd = *it;

        // PDレコードを文字列化
        std::vector<igesio::IGESParameterType> types = pd.data_types;
        if (pd.data_types.empty() || pd.data_types.size() != pd.data.size())
            types = ProvisionalParameterTypes(pd.data);

        // パラメータデータの先頭にエンティティタイプを追加
        auto data = pd.data;
        data.insert(data.begin(), std::to_string(static_cast<int>(pd.type)));
        types.insert(types.begin(), igesio::IGESParameterType::kInteger);

        auto lines = igesio::utils::ToFreeFormattedLines(
            data, types, igesio::kColDEPointer - 1, p_delim, r_delim);

        // シーケンス番号を保存 (RawEntityPDのデータは信頼できないため無視)
        pd_sequence_numbers.push_back(pos + 1);
        pd_lines_count.push_back(lines.size());

        // 各行をpd_linesに追加
        for (const auto& line : lines) {
            // PDレコードのDEバックポインタとシーケンス番号を追加
            pd_lines.push_back(line + PDSuffix(de_seq, ++pos));
        }
    }

    return {pd_lines, pd_sequence_numbers, pd_lines_count};
}

std::vector<std::string>
SerializeDeSection(const std::vector<igesio::entities::RawEntityDE>& de_section,
                   const std::vector<unsigned int>& pd_sequence_numbers,
                   const std::vector<unsigned int>& pd_lines_count) {
    std::vector<std::string> de_lines;

    for (size_t i = 0; i < de_section.size(); ++i) {
        auto [line1, line2] = igesio::entities::ToStrings(
                de_section[i], pd_sequence_numbers[i],
                2*i + 1, pd_lines_count[i]);
        de_lines.push_back(line1);
        de_lines.push_back(line2);
    }

    return de_lines;
}

std::vector<std::string>
SerializeStartSection(const std::string& start_section) {
    // kColIdentify - 1文字ごとに分割する
    std::vector<std::string> start_lines;

    // start_sectionが空文字列の場合は1行だけ空白行を追加する
    if (start_section.empty()) {
        start_lines.push_back(std::string(igesio::kColIdentify - 1, ' ')
                              + SequenceNumberStr('S', 1));
        return start_lines;
    }

    for (size_t i = 0; i * (igesio::kColIdentify - 1) < start_section.size(); ++i) {
        // kColIdentify - 1文字に満たない場合は、右側を空白で埋める
        auto pos = i * (igesio::kColIdentify - 1);
        auto line = start_section.substr(pos, igesio::kColIdentify - 1);
        if (line.size() < igesio::kColIdentify - 1) {
            line += std::string(igesio::kColIdentify - 1 - line.size(), ' ');
        }
        // シーケンス番号を追加
        start_lines.push_back(line + SequenceNumberStr('S', i + 1));
    }
    return start_lines;
}

std::vector<std::string>
SerializeGlobalSection(const igesio::models::GlobalParam& global_section,
                       const char p_delim, const char r_delim,
                       const std::string& file_name) {
    std::vector<std::string> global_lines;

    auto vector = igesio::models::ToVector(global_section, file_name);
    auto lines = igesio::utils::ToFreeFormattedLines(
        vector, igesio::kColIdentify - 1, p_delim, r_delim,
        global_section.GetSerializationConfig());

    // グローバルセクションの各行を追加
    for (unsigned int i = 0; i < lines.size(); ++i) {
        global_lines.push_back(lines[i] + SequenceNumberStr('G', i + 1));
    }

    return global_lines;
}

std::string SerializeTerminateSection(int start_lines, int global_lines,
                                      int de_lines, int pd_lines) {
    std::ostringstream oss;
    int w = igesio::kFixedColWidth;
    oss << 'S' << std::setw(w - 1) << std::setfill('0') << start_lines
        << 'G' << std::setw(w - 1) << std::setfill('0') << global_lines
        << 'D' << std::setw(w - 1) << std::setfill('0') << de_lines
        << 'P' << std::setw(w - 1) << std::setfill('0') << pd_lines
        << std::setw(igesio::kColIdentify - 1 - 4 * w) << std::setfill(' ') << ""
        << 'T' << std::setw(w - 1) << std::setfill('0') << 1;
    return oss.str();
}



/**
 * ファイルの存在確認・作成など
 */

/// @brief 指定されたファイルパスの親ディレクトリが存在するか確認し、
///        存在しない場合は再帰的に作成する
/// @param filePath ファイルパス (絶対パス)
/// @throw igesio::FileOpenError 親ディレクトリの作成に失敗した場合
void EnsureParentDirectoryExists(const std::string& filePath) {
    std::filesystem::path path(filePath);
    std::filesystem::path parentDir = path.parent_path();

    // 親フォルダが存在するかチェック
    if (std::filesystem::exists(parentDir)) return;

    // 存在しない場合は再帰的に作成
    std::error_code ec;
    bool created = std::filesystem::create_directories(parentDir, ec);

    if (ec) {
        throw igesio::FileOpenError("Error creating directory: "
                + ec.message() + " for path: " + parentDir.string());
    }
}

}  // namespace



bool igesio::WriteIgesIntermediate(
        const models::IntermediateIgesData& data, const std::string& file_path) {
    // DEセクションの各レコードのシーケンス番号を取得・昇順にソートする
    auto de_seq_numbers = SortDeSequenceNumbers(data.directory_entry_section);

    // PDセクションの各レコードを文字列化する
    auto [pd_lines, pd_seq_numbers, pd_lines_count] = SerializePdSection(
        data.parameter_data_section, de_seq_numbers,
        data.global_section.param_delim, data.global_section.record_delim);

    // DEセクションの各レコードを文字列化する
    auto de_lines = SerializeDeSection(
        data.directory_entry_section, pd_seq_numbers, pd_lines_count);

    // スタートセクションの文字列化
    auto start_lines = SerializeStartSection(data.start_section);

    // ファイル名を取得
    auto absolute_path = std::filesystem::absolute(file_path).string();
    if (absolute_path.empty()) {
        throw igesio::FileOpenError("File path is empty.");
    }
    std::string file_name = std::filesystem::path(absolute_path).filename().string();

    // グローバルセクションの文字列化
    auto global_lines = SerializeGlobalSection(
        data.global_section, data.global_section.param_delim,
        data.global_section.record_delim, file_name);

    // ターミネートセクションの文字列化
    auto terminate_line = SerializeTerminateSection(
        start_lines.size(), global_lines.size(),
        de_lines.size(), pd_lines.size());

    // 親ディレクトリの存在確認・作成
    EnsureParentDirectoryExists(absolute_path);

    // ファイルに書き込む
    std::ofstream ofs(absolute_path);
    if (!ofs) {
        throw igesio::FileOpenError("Failed to open file for writing: " + file_path);
    }
    for (const auto& line : start_lines) ofs << line << '\n';
    for (const auto& line : global_lines) ofs << line << '\n';
    for (const auto& line : de_lines) ofs << line << '\n';
    for (const auto& line : pd_lines) ofs << line << '\n';
    ofs << terminate_line << '\n';
    ofs.close();
    return true;
}

igesio::models::IntermediateIgesData
igesio::ConvertToIntermediate(const models::IgesData& data,
                              const bool save_unsupported) {
    // id -> de_pointerのマッピングを作成する
    entities::id2pointer id2de;
    for (const auto& [id, entity] : data.GetEntities()) {
        // TODO: 親子関係を考慮してDEポインタを割り当てる
        id2de[id] = id2de.size() * 2 + 1;

        if (!save_unsupported && !entity->IsSupported()) {
            // save_unsupportedがfalseの場合、UnsupportedEntityは保存しない
            throw igesio::TypeConversionError(
                "IgesData contains UnsupportedEntity. "
                "Set save_unsupported to true to include them.");
        }
    }

    // 出力時にライブラリ名やバージョンは自動的に設定されるため、ここでは操作しない
    models::IntermediateIgesData intermediate;
    intermediate.start_section = data.description;
    intermediate.global_section = data.global_section;

    // DEセクションとPDセクションを変換する
    for (const auto& [id, entity] : data.GetEntities()) {
        auto de = entity->GetRawEntityDE();
        de.sequence_number = id2de.at(id);
        intermediate.directory_entry_section.push_back(de);

        try {
            auto pd = entities::ToRawEntityPD(
                entity->GetType(), id, entity->GetParameters(), id2de);
            intermediate.parameter_data_section.push_back(pd);
        } catch (std::out_of_range& e) {
            // dataが持たないエンティティへの参照を含む場合
            throw igesio::DataFormatError(
                "Entity with ID " + std::to_string(id) +
                " contains reference to unknown entity. " + e.what());
        }
    }

    return intermediate;
}

bool igesio::WriteIges(const models::IgesData& data,
                       const std::string& file_path,
                       const bool save_unsupported) {
    auto intermediate = ConvertToIntermediate(data, save_unsupported);
    return WriteIgesIntermediate(intermediate, file_path);
}
