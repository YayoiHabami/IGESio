/**
 * @file reader.cpp
 * @brief IGESファイルを読み込むための関数を定義する
 * @author Yayoi Habami
 * @date 2025-04-19
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/reader.h"

#include <array>
#include <filesystem>
#include <string>
#include <tuple>
#include <vector>

#include "igesio/utils/iges_string_utils.h"
#include "igesio/utils/iges_binary_reader.h"
#include "igesio/entities/factory.h"



namespace {

/// @brief igesio 名前空間のエイリアス
namespace iio = igesio;
/// @brief igesio::models 名前空間のエイリアス
namespace i_model = igesio::models;
/// @brief igesio::utils 名前空間のエイリアス
namespace i_util = igesio::utils;
/// @brief igesio::entities 名前空間のエイリアス
namespace i_ent = igesio::entities;

/// @brief グローバルセクションのパラメータ1,2部分を切り捨てる
/// @param line グローバルセクションの1行目
/// @param p_delim パラメータ区切り文字
/// @param r_delim レコード区切り文字
/// @return グローバルセクションのパラメータ1,2部分を切り捨てた文字列.
///         "1H,,1H;,8HTestCase,..."のような文字列の場合、"8HTestCase,..."を返す
/// @throw igesio::SectionFormatError 不正な形式の文字列が渡された場合
std::string TrimTwoParamsFromFirstGlobalSectionLine(
        const std::string& line, const char p_delim, const char r_delim) {
    // ",," 形式であれば最初の2つの文字を捨てる
    if (line.substr(0, 2) == ",,") return line.substr(2);

    // "1Hααα" 形式であれば、最初の5文字を捨てる
    std::string first_prm = std::string("1H") + p_delim + p_delim;
    if (line.substr(0, 5) == first_prm + p_delim) return line.substr(5);

    // ",1Hβ," 形式であれば、最初の5文字を捨てる
    std::string second_prm = std::string("1H") + r_delim + p_delim;
    if (line.substr(0, 5) == p_delim + second_prm) return line.substr(5);

    // "1Hαα1Hβα" 形式であれば、最初の8文字を捨てる
    if (line.substr(0, 8) == first_prm + second_prm) return line.substr(8);

    // それ以外の場合はエラー
    throw iio::SectionFormatError(
            "Invalid format for the first line of the global section: '" +
            line + "'");
}

}  // namespace



/**
 * IgesReaderクラスのメンバ関数
 */

iio::IgesReader::IgesReader(const std::string& file_path) : reader_(file_path) {
    // 圧縮形式であれば、エラーを投げる
    if (reader_.IsCompressed()) {
        throw iio::NotImplementedError(
                "Compressed IGES files are not supported yet. "
                "Please use uncompressed IGES files.");
    }
}

std::optional<std::tuple<std::string, iio::SectionType, unsigned int>>
iio::IgesReader::GetLine() {
    std::tuple<std::string, iio::SectionType, unsigned int> next_line;
    if (!IsNextLinePoolEmpty()) {
        // next_line_が空でない場合は、next_line_を空にして返す
        next_line = next_line_;
        PoolNextLine();
        return next_line;
    }

    return reader_.GetLine();
}

std::optional<std::tuple<std::string, iio::SectionType, unsigned int>>
iio::IgesReader::GetLine(const SectionType section) {
    auto next_section = GetNextSectionType();
    if (!next_section.has_value()) {
        // 次の行が存在しない場合
        return std::nullopt;
    } else if (next_section.value() != section) {
        // 次の行が現在のセクションと異なる場合
        return std::nullopt;
    }

    return GetLine();
}

std::optional<std::string> iio::IgesReader::ReadStartSection() {
    // スタートセクションを読み込む
    std::vector<std::string> lines = {};
    while (true) {
        auto info = GetLine(SectionType::kStart);
        if (!info.has_value()) break;  // std::nulloptの場合は終了
        lines.push_back(std::get<0>(info.value()));
    }
    // 読み込まれる行がない場合は処理を終了
    if (lines.empty()) return std::nullopt;

    // 最終行のみ末尾の空白を削除し、データ部を統合
    std::string result = "";
    for (const auto& line : lines) {
        result += utils::GetDataPart(line, SectionType::kStart);
    }
    return utils::rtrim(result);
}

std::optional<i_model::GlobalParam> iio::IgesReader::ReadGlobalSection() {
    std::vector<std::string> data_parts = {};
    while (true) {
        auto info = GetLine(SectionType::kGlobal);
        if (!info.has_value()) break;  // std::nulloptの場合は終了

        // データ部のみを取得
        data_parts.push_back(utils::GetDataPart(std::get<0>(info.value()),
                                                SectionType::kGlobal));
    }
    // 行が読み込まれない場合はstd::nulloptを返す
    // (スタート部がまだ読み込まれていないか、すでにグローバル部が全てが読み込まれた場合)
    if (data_parts.empty()) return std::nullopt;


    // 区切り文字を取得
    parameter_delimiter_ = utils::GetParameterDelimiter(data_parts[0]);
    record_delimiter_ = utils::GetRecordDelimiter(data_parts[0], parameter_delimiter_);
    data_parts[0] = TrimTwoParamsFromFirstGlobalSectionLine(
            data_parts[0], parameter_delimiter_, record_delimiter_);

    // データ部をパースして、データごとに分割
    auto params = utils::ParseFreeFormattedData(
        data_parts, parameter_delimiter_, record_delimiter_);

    return i_model::SetGlobalSectionParams(
            parameter_delimiter_, record_delimiter_, params);
}

std::optional<i_ent::RawEntityDE>
iio::IgesReader::ReadDirectoryEntryRecord() {
    // グローバル部の末端まで読まれていないか、すでにDE部が全て読まれた場合は終了
    auto first = GetLine(SectionType::kDirectory);
    if (!first.has_value()) return std::nullopt;

    auto second = GetLine(SectionType::kDirectory);
    if (!second.has_value()) {
        // 偶数行目が取得できない場合はフォーマットエラー
        throw iio::SectionFormatError(
                "One record of the directory entry section comprises two lines, "
                "but the line following '" + std::get<0>(first.value()) +
                "' does not exist.");
    }

    // データ部を取得
    return i_ent::ToRawEntityDE(
            std::get<0>(first.value()), std::get<0>(second.value()));
}

std::optional<i_ent::RawEntityPD>
iio::IgesReader::ReadParameterDataRecord() {
    // まだDE部の末端まで読まれていないか、すでにPD部が全て読まれた場合は終了
    auto line = GetLine(SectionType::kParameter);
    if (!line.has_value()) return std::nullopt;

    // DEポインタを取得
    auto de_pointer = i_util::GetDEPointer(std::get<0>(line.value()));

    // データ部を取得
    std::vector<std::string> lines = {std::get<0>(line.value())};
    while (true) {
        line = GetLine(SectionType::kParameter);

        // NOTE: グロ―バルパラメータ14を保持しておき、行数を取得する方が仕様通りだとは
        //       思うが、処理の簡便さのため、ここではDEポインタによる判別を行う
        if (!line.has_value()) {
            // PDセクションの末端に到達した場合は終了
            break;
        } else if (i_util::GetDEPointer(std::get<0>(line.value())) != de_pointer) {
            // DEポインタが変わった (レコードが変わった) 場合は終了
            // 取得した行は次のレコードなので、プール
            PoolNextLine(line.value());
            break;
        }

        lines.push_back(std::get<0>(line.value()));
    }
    // データが取得できない場合は終了
    if (lines.empty()) return std::nullopt;

    return i_ent::ToRawEntityPD(
            lines, parameter_delimiter_, record_delimiter_);
}

std::optional<std::array<unsigned int, 4>>
iio::IgesReader::ReadTerminateSection() {
    // PD部の末端まで読まれていないか、すでにターミネート部が全て読まれた場合は終了
    auto line = GetLine(SectionType::kTerminate);
    if (!line.has_value()) return std::nullopt;
    auto str = std::get<0>(line.value());
    auto w = kFixedColWidth;

    if (str[0] != 'S' || str[w] != 'G' ||
        str[2*w] != 'D' || str[3*w] != 'P') {
        // フォーマット違い
        throw iio::SectionFormatError(
                "Invalid format for the terminate section: '" + str + "'");
    }

    // 各セクションの行数を取得
    std::array<unsigned int, 4> lines = {0, 0, 0, 0};
    lines[0] = igesio::FromIgesInteger(str.substr(1, w - 1), std::nullopt);
    lines[1] = igesio::FromIgesInteger(str.substr(w + 1, w - 1), std::nullopt);
    lines[2] = igesio::FromIgesInteger(str.substr(2*w + 1, w - 1), std::nullopt);
    lines[3] = igesio::FromIgesInteger(str.substr(3*w + 1, w - 1), std::nullopt);

    return lines;
}

std::optional<iio::SectionType> iio::IgesReader::GetNextSectionType() {
    // プールされている行がある場合は、その行のセクションタイプを返す
    if (!IsNextLinePoolEmpty()) {
        return std::get<1>(next_line_);
    } else {
        // プールされている行がない場合
        if (reader_.IsEndOfFile()) {
            // ターミネートセクションに到達している場合は終了
            return std::nullopt;
        }

        auto info = GetLine();
        if (!info.has_value()) {
            // 新しい行が存在せず、EOFでもない場合はエラー
            throw iio::SectionFormatError(
                    "The next line is not found, but the end of file is not reached.");
        }

        // 新しい行をプールし、その行のセクションタイプを返す
        PoolNextLine(info.value());
        return std::get<1>(info.value());
    }
}



/**
 * それ以外の関数
 */

i_model::IntermediateIgesData igesio::ReadIgesIntermediate(
        const std::string& file_path, const bool validate_strictly) {
    // IGESファイルを読み込む
    auto absolute_path = std::filesystem::absolute(file_path);
    if (!std::filesystem::exists(absolute_path)) {
        throw iio::FileOpenError(
                "The file does not exist: " + absolute_path.string());
    } else if (!std::filesystem::is_regular_file(absolute_path)) {
        throw iio::FileOpenError(
                "The path is not a regular file: " + absolute_path.string());
    }
    IgesReader reader(absolute_path.string());

    i_model::IntermediateIgesData data;

    // スタートセクションを読み込む
    auto start = reader.ReadStartSection();
    if (!start.has_value()) {
        throw iio::SectionFormatError(
                "Start section is not found in the file: " + file_path);
    }
    data.start_section = start.value();

    // グローバルセクションを読み込む
    auto global = reader.ReadGlobalSection();
    if (!global.has_value()) {
        throw iio::SectionFormatError(
                "Global section is not found in the file: " + file_path);
    }
    data.global_section = global.value();

    // ディレクトリエントリセクションを読み込む
    while (true) {
        auto de = reader.ReadDirectoryEntryRecord();
        if (!de.has_value()) break;  // std::nulloptの場合は終了

        // ディレクトリエントリの妥当性を検証 (validate_strictlyがtrueの場合)
        if (validate_strictly) i_ent::IsValid(de.value());
        data.directory_entry_section.push_back(de.value());
    }

    // パラメータデータセクションを読み込む
    while (true) {
        auto pd = reader.ReadParameterDataRecord();
        if (!pd.has_value()) break;  // std::nulloptの場合は終了
        data.parameter_data_section.push_back(pd.value());
    }

    // ターミネートセクションを読み込む
    auto terminate_section = reader.ReadTerminateSection();
    if (!terminate_section.has_value()) {
        throw iio::SectionFormatError(
                "Terminate section is not found in the file: " + file_path);
    }
    data.terminate_section = terminate_section.value();

    return data;
}

i_model::IgesData igesio::ConvertFromIntermediate(
        const models::IntermediateIgesData& intermediate) {
    i_model::IgesData iges;
    iges.description = intermediate.start_section;
    iges.global_section = intermediate.global_section;

    auto iges_id = iges.GetID();

    // DEとPDの数が一致するか確認
    if (intermediate.directory_entry_section.size() !=
        intermediate.parameter_data_section.size()) {
        throw iio::DataFormatError(
                "The number of directory entry records (" +
                std::to_string(intermediate.directory_entry_section.size()) +
                ") does not match the number of parameter data records (" +
                std::to_string(intermediate.parameter_data_section.size()) +
                ").");
    }

    // すべてのエンティティのIDを先に生成・取得する
    pointer2ID de2id;
    for (const auto& de : intermediate.directory_entry_section) {
        auto de_pointer = de.sequence_number;
        // すでにde2idにde_pointerが存在する場合はエラー
        if (de2id.find(de_pointer) != de2id.end()) {
            throw iio::DataFormatError("Duplicate directory entry pointer"
                    " found: " + std::to_string(de_pointer));
        }
        de2id[de_pointer] = IDGenerator::Reserve(
                iges_id, static_cast<uint16_t>(de.entity_type), de_pointer);
    }

    // DEとPDを組み合わせてエンティティを生成し、IgesDataに追加
    for (const auto& de : intermediate.directory_entry_section) {
        // pd_pointerとシーケンス番号が一致するPDを探す
        auto pd_pointer = de.parameter_data_pointer;
        auto pd_it = std::find_if(
                intermediate.parameter_data_section.begin(),
                intermediate.parameter_data_section.end(),
                [pd_pointer](const entities::RawEntityPD& pd) {
                    return pd.sequence_number == pd_pointer;
                });
        if (pd_it == intermediate.parameter_data_section.end()) {
            throw iio::DataFormatError(
                    "Parameter data record with pointer " +
                    std::to_string(pd_pointer) +
                    " not found for directory entry with sequence number " +
                    std::to_string(de.sequence_number) + ".");
        }
        auto pd = *pd_it;

        // DEとPDを組み合わせてエンティティを生成
        try {
            auto entity = entities::EntityFactory::CreateEntity(de, pd, de2id, iges_id);
            iges.AddEntity(entity);
        } catch (const std::out_of_range& e) {
            // エンティティがIGESファイル内に存在しないエンティティを参照している場合
            throw iio::DataFormatError(
                    "Entity with DE sequence number " +
                    std::to_string(de.sequence_number) +
                    " references an entity that does not exist in the IGES file. "
                    "Details: " + std::string(e.what()));
        } catch (const std::bad_variant_access& e) {
            // DEまたはPDのフィールドにおいて、期待される型と異なる型が使用されている場合
            throw iio::DataFormatError(
                    "Entity with DE sequence number " +
                    std::to_string(de.sequence_number) +
                    " has a field with an unexpected type. "
                    "Details: " + std::string(e.what()));
        }
    }

    return iges;
}

i_model::IgesData igesio::ReadIges(const std::string& file_path,
                                   const bool validate_strictly) {
    // IGESファイルを読み込み、中間データ構造を生成
    auto intermediate = ReadIgesIntermediate(file_path, validate_strictly);

    // 中間データ構造からIgesDataクラスを生成
    return ConvertFromIntermediate(intermediate);
}
