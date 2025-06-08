/**
 * @file utils/iges_binary_reader.h
 * @brief IGESファイルをバイナリで読み込むクラス
 * @author Yayoi Habami
 * @date 2025-04-13
 * @copyright 2025 Yayoi Habami
 * @note IGESはファイルがASCII形式であることを前提としているため、
 *       文字列にその他のエンコーディング形式が含まれると、固定長の
 *       フィールドの読み込みに失敗する可能性がある.
 *       そのため、各行の読み込みや固定長さフィールドの取得、
 *       検証などは、このクラスで行う.
 */
#ifndef IGESIO_UTILS_IGES_BINARY_READER_H_
#define IGESIO_UTILS_IGES_BINARY_READER_H_

#include <fstream>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "igesio/common/iges_metadata.h"

namespace igesio::utils {


/// @brief 隠蔽用名前空間
/// @note 直接使用しないこと
namespace detail {

/// @brief 正しい順序で行が進められているかを確認する
/// @param type 現在の行のセクションの種類
/// @param number 現在の行のシーケンス番号
/// @param prev_type 前の行のセクションの種類
/// @param prev_number 前の行のシーケンス番号
/// @return true: 正しい順序で進められている, false: 不正な順序
/// @note 例えばkGlobalの後にkStartが来たり、同じセクションの中で
///       シーケンス番号が逆転している場合は不正な順序
/// @note 圧縮形式のデータセクションは、シーケンス番号を持たないため、
///       この関数では検証しない
bool IsValidSectionOrder(const SectionType, const unsigned int,
                         const std::optional<SectionType>, const unsigned int);

}  // namespace detail



/// @brief IGESファイルを読み込むクラス
/// @note IGESファイルは基本的に固定行数のASCII形式であることを前提としているが、
///       文字列については他の方式でエンコードされたバイナリを含む可能性があるため、
///       バイナリとしてファイルを読み込む
class IgesBinaryReader {
 public:
    /// @brief コンストラクタ
    /// @param file_path 読み込むIGESファイルのパス
    /// @throw igesio::FileOpenError ファイルが開けなかった場合
    /// @throw igesio::LineFormatError 行の長さが規定値以外の場合
    ///        (規定値の1行文字列の後に改行文字がない場合など)
    explicit IgesBinaryReader(const std::string& file_path);

    /// @brief デストラクタ
    /// @note ファイルは自動的にクローズされる
    ~IgesBinaryReader();

    /// @brief 1行を読み込む
    /// @return タプル型の戻り値
    ///   - std::string: 読み込んだ行 (改行文字を除く)
    ///   - SectionType: その行のセクションの種類
    ///   - unsigned int: その行のシーケンス番号 (データ部は常に0)
    /// @throw igesio::LineFormatError 行の長さが規定値以外の場合
    /// @throw igesio::SectionFormatError セクションの順序やシーケンス番号が不正な場合
    /// @note 最終行に達した場合は、`{"", SectionType::kTerminate, 0}`を返す
    std::tuple<std::string, SectionType, unsigned int> GetLine();

    /// @brief ファイルの終端に達したかどうかを確認する
    /// @return ファイルの終端に達したか、Terminateセクションに達した場合、
    ///         および既に読み込んだ行においてエラーが発生した場合はtrue
    bool IsEndOfFile() const;

    /// @brief ファイルが圧縮形式かどうかを確認する
    bool IsCompressed() const noexcept { return is_compressed_; }

    /// @brief ファイルポインタを先頭に戻す
    void Reset();

    /// @brief 現在開いているファイルのパスを取得する
    /// @return 現在開いているファイルのパス
    const std::string& GetFilePath() const noexcept { return file_path_; }

    /// @brief 一つ前の行のセクションの種類
    /// @return 一つ前の行のセクションの種類
    /// @note 初期値はstd::nullopt (1行も読み込んでいない状態)
    const std::optional<SectionType>& GetPrevSectionType() const noexcept {
        return prev_section_type_;
    }

    /// @brief 一つ前の行のシーケンス番号
    /// @return 一つ前の行のシーケンス番号
    /// @note 初期値は0 (1行も読み込んでいない状態)
    /// @note データ部についても常に0 (圧縮形式のデータ部はシーケンス番号を持たないため)
    const unsigned int GetPrevSequenceNumber() const noexcept {
        return prev_sequence_number_;
    }

 private:
    /// @brief ファイル入出力ストリーム
    std::ifstream file_;

    /// @brief 開いているファイルのパス
    std::string file_path_;

    /// @brief ファイルが圧縮形式か否か
    /// @note コンストラクタで判定
    bool is_compressed_ = false;

    /// @brief 改行文字（またはその列）
    std::vector<char> line_break_;

    /// @brief 改行文字を検出するメンバ関数
    /// @throw igesio::LineFormatError 規定位置に改行文字が見つからなかった場合
    void DetectLineBreak();

    /// @brief 一つ前の行のセクション
    /// @note 初期値はstd::nullopt (1行も読み込んでいない状態)
    std::optional<SectionType> prev_section_type_ = std::nullopt;

    /// @brief 一つ前の行のシーケンス番号
    /// @note 初期値は0 (1行も読み込んでいない状態)
    /// @note データ部についても常に0 (圧縮形式のデータ部はシーケンス番号を持たないため)
    unsigned int prev_sequence_number_ = 0;

    /// @brief セクションとシーケンス番号を更新するメンバ関数
    /// @param type 現在の行のセクションの種類
    /// @param number 現在の行のシーケンス番号
    /// @throw igesio::SectionFormatError セクションの順序やシーケンス番号が不正な場合
    void UpdateSectionInfo(const SectionType, const unsigned int);

    /// @brief これまでの行でエラーが発生したかどうか
    bool has_error_occurred_ = false;
};

}  // namespace igesio::utils

#endif  // IGESIO_UTILS_IGES_BINARY_READER_H_
