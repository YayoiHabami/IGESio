/**
 * @file src/utils/iges_binary_reader.cpp
 * @brief IGESファイルをバイナリで読み込むクラス
 * @author Yayoi Habami
 * @date 2025-04-13
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/utils/iges_binary_reader.h"

#include <array>
#include <string>
#include <tuple>
#include <vector>

#include "igesio/utils/iges_string_utils.h"



namespace {

/// @brief igesio 名前空間のエイリアス
namespace iio = igesio;
/// @brief igesio::utils 名前空間のエイリアス
namespace i_util = igesio::utils;

/// @brief IgesBinaryReader クラスのエイリアス
using IBReader = i_util::IgesBinaryReader;

/// @brief SectionType クラスのエイリアス
using SType = iio::SectionType;



/// @brief 改行文字の検出
/// @param bytes 読み込んだ2バイト
/// @param bytes_read 読み込んだバイト数
/// @return 改行文字の列
/// @throw igesio::LineFormatError 改行文字が見つからなかった場合
std::vector<char>
DetectLineBreakChar(const std::array<char, 2>& bytes, const std::size_t bytes_read) {
    // 1文字も読み込めていなければエラー
    if (bytes_read < 1) {
        throw iio::LineFormatError(
                "Failed to read the line break character at the first line. "
                "The file may be corrupted or not in IGES format.");
    }

    // 改行文字を判定
    if (bytes[0] == '\r' && bytes_read == 2 && bytes[1] == '\n') {
        // CRLF (Windows)
        return {'\r', '\n'};
    } else if (bytes[0] == '\r') {
        // CR (Old Mac)
        return {'\r'};
    } else if (bytes[0] == '\n') {
        // LF (Unix/Linux)
        return {'\n'};
    } else {
        throw iio::LineFormatError(
                "Failed to detect line break character in the first line. "
                "The file may be corrupted or not in IGES format.");
    }
}

/// @brief 次の改行文字まで読み込み、改行文字を除いて返す
/// @param fs 読み込むファイルストリーム
/// @param line_break 改行文字 (CRLFの場合は `{'\r', '\n'}`)
/// @return 改行文字を除いた文字列
/// @throw igesio::LineFormatError kMaxColumn+1文字目までに
///        改行文字が見つからなかった場合
/// @note 改行文字の末端までfsを読込 (次のfs.get()で次の行の1文字目を取得可能)
std::vector<char>
ReadToNextLineBreak(std::ifstream& fs, const std::vector<char>& line_break) {
    std::vector<char> line = {};
    std::size_t pos = 0;
    char c;
    bool line_break_found = false;

    while (pos <= iio::kMaxColumn && fs.get(c)) {
        pos++;
        if (c != line_break[0]) {
            // 改行文字の最初の文字と一致しない場合
            line.push_back(c);
            // 81文字目までに改行文字が出てこない場合はbreak
            if (pos == iio::kMaxColumn + 1) break;
            continue;
        }

        // 改行文字の最初の文字と一致した場合
        if (line_break.size() == 1) {
            // 単一文字の改行コード（LFやCR）の場合
            line_break_found = true;
            break;
        }
        // 複数文字の改行コード（CRLFなど）の場合は2文字目も一致したら改行
        if (fs.get(c) && c == line_break[1]) {
            pos++;
            line_break_found = true;
            break;
        } else {
            // 2文字目が一致しなかった場合は、2文字とも格納
            line.push_back(line_break[0]);
            line.push_back(c);
        }
    }

    if (!line_break_found) {
        // EOFに達している場合はエラーを出さない
        if (fs.eof()) return line;

        // kMaxColumn+1文字目までに改行文字が見つからなかった場合
        // 末尾に\0を追加してエラー
        line.push_back('\0');
        throw iio::LineFormatError(
            "The line break character was not found within " +
            std::to_string(iio::kMaxColumn) + " characters: " +
            line.data());
    }
    return line;
}

}  // namespace


bool i_util::detail::IsValidSectionOrder(
        const SType type, const unsigned int number,
        const std::optional<SType> prev_type,
        const unsigned int prev_number) {
    if (!prev_type.has_value()) {
        // 1行も読み込まれていない状態
        // フラグセクションの1行目ならOK
        if (type == SType::kFlag && number == 1) return true;
        // スタートセクションの1行目ならOK
        if (type == SType::kStart && number == 1) return true;
        // それ以外は不正な順序
        return false;
    }

    auto p_type = prev_type.value();
    if (p_type == type) {
        // 圧縮形式のデータセクションはシーケンス番号を持たないため、検証しない
        if (p_type == SType::kData) return true;
        // 同じセクションの場合は、セクション番号が連続していればOK
        if (number == prev_number + 1) return true;
        // それ以外は不正な順序
        return false;
    }
    // 異なるセクションの場合は、新しいシーケンス番号が1でなければ不正
    // ただし、圧縮形式のデータセクションを除く
    if (number != 1 && type != SType::kData) return false;

    switch (p_type) {
        case SType::kFlag: return type == SType::kStart;
        case SType::kStart: return type == SType::kGlobal;
        case SType::kGlobal:
            // kGlobalの後はkDirectory(通常)またはkData(圧縮形式)
            return type == SType::kDirectory || type == SType::kData;
        case SType::kDirectory: return type == SType::kParameter;
        case SType::kParameter: return type == SType::kTerminate;
        case SType::kTerminate: return false;
        case SType::kData: return type == SType::kTerminate;
        default: return false;  // それ以外のセクションは不正な順序
    }
}



IBReader::IgesBinaryReader(const std::string& file_path)
        : file_path_(file_path) {
    // バイナリモードでファイルを開く
    file_.open(file_path, std::ios::binary);
    if (!file_.is_open()) {
        throw iio::FileOpenError(file_path);
    }

    // 改行文字を検出する
    DetectLineBreak();

    // ファイルポインタを先頭に戻す
    Reset();
}

IBReader::~IgesBinaryReader() {
    // ファイルは自動的にクローズされる
    if (file_.is_open()) {
        file_.close();
    }
}

void IBReader::DetectLineBreak() {
    // NOTE: BOMはないものとする
    // 最初のkMaxColumn文字を読み込むためのバッファ
    std::vector<char> buffer(kMaxColumn);
    file_.read(buffer.data(), kMaxColumn);

    // 読み込んだバイト数がkMaxColumn未満の場合、EOFに達している
    if (file_.gcount() != kMaxColumn) {
        throw iio::LineFormatError(
                "The first line length is less than " + std::to_string(kMaxColumn) +
                " characters. The file may be corrupted or not in IGES format.");
    }

    // 73文字目がCの場合は圧縮形式
    if (buffer[iio::kColIdentify - 1] == 'C') is_compressed_ = true;

    // 改行文字の検出のため、次の2バイトを読み込む
    std::array<char, 2> next_bytes{};
    file_.read(next_bytes.data(), 2);
    auto bytes_read = file_.gcount();

    // 改行文字を検出する
    line_break_ = DetectLineBreakChar(next_bytes, bytes_read);
}

void IBReader::UpdateSectionInfo(const SType section_type,
                                 const unsigned int sequence_number) {
    if (!detail::IsValidSectionOrder(section_type, sequence_number,
                                     prev_section_type_, prev_sequence_number_)) {
        if (!prev_section_type_.has_value()) {
            // 1行目のセクションまたはシーケンス番号が不正な場合はエラー
            throw iio::SectionFormatError(
                    "Invalid section type or sequence number at the first line: " +
                    SectionTypeToString(section_type) + " " +
                    std::to_string(sequence_number));
        }
        // セクションの順序が不正な場合はエラー
        throw iio::SectionFormatError(
                "Invalid order of section type or sequence number: from (" +
                SectionTypeToString(prev_section_type_.value()) + ", " +
                std::to_string(prev_sequence_number_) + ") to (" +
                SectionTypeToString(section_type) + ", " +
                std::to_string(sequence_number) + ")");
    }

    // セクションの型とシーケンス番号を保存する
    prev_section_type_ = section_type;
    prev_sequence_number_ = sequence_number;
}

std::tuple<std::string, ::SType, unsigned int> IBReader::GetLine() {
    // ファイルの終端に達している場合は空の行を返す
    if (IsEndOfFile()) return {"", SType::kTerminate, 0};

    // 次の改行文字まで読み込む (必ずしもkMaxColumn文字ではない)
    try {
        auto line_v = ReadToNextLineBreak(file_, line_break_);
        std::string line(line_v.begin(), line_v.end());

        // セクションの型とシーケンス番号を取得する (同時に行数などもチェックされる)
        auto section_type = i_util::GetSectionType(line, IsCompressed());
        unsigned int sequence_number = 0;
        if (section_type != SType::kData) {
            // 圧縮形式のデータセクション以外の場合はシーケンス番号を取得
            sequence_number = i_util::GetSequenceNumber(line, IsCompressed());
        }

        // セクションの順序とシーケンス番号を更新する
        UpdateSectionInfo(section_type, sequence_number);

        return {line, section_type, sequence_number};
    } catch (const iio::LineFormatError& e) {
        has_error_occurred_ = true;
        throw e;
    } catch (const iio::SectionFormatError& e) {
        has_error_occurred_ = true;
        throw e;
    }
}

bool IBReader::IsEndOfFile() const {
    // EOFに達しているか、前のセクションがkTerminateの場合はtrue
    // 規定では、Terminateセクションは1行のみであり、それ以降は無視する
    return file_.eof() ||
           (prev_section_type_.has_value() &&
            prev_section_type_.value() == SType::kTerminate) ||
           has_error_occurred_;
}

void IBReader::Reset() {
    // ファイルポインタを先頭に戻す
    file_.clear();
    file_.seekg(0, std::ios::beg);
    prev_section_type_ = std::nullopt;
    prev_sequence_number_ = 0;
    has_error_occurred_ = false;
}
