/**
 * @file reader.h
 * @brief IGESファイルを読み込むための関数を定義する
 * @author Yayoi Habami
 * @date 2025-04-19
 * @copyright 2025 Yayoi Habami
 * @brief 基本的には`ReadIges`関数を使用することを推奨するが、
 *        内部処理的には`IgesReader`クラスを使用しているため、
 *        `IgesReader`クラスを直接使用することも可能である.
 */
#ifndef IGESIO_READER_H_
#define IGESIO_READER_H_

#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/utils/iges_binary_reader.h"
#include "igesio/entities/de/raw_entity_de.h"
#include "igesio/entities/pd.h"
#include "igesio/models/intermediate.h"



namespace igesio {

class IgesReader {
    /// @brief リーダー
    utils::IgesBinaryReader reader_;

 public:
    /// @brief コンストラクタ
    /// @param file_path 読み込むIGESファイルのパス
    /// @throw igesio::FileOpenError ファイルが開けなかった場合
    /// @throw igesio::LineFormatError 行の長さが規定値以外の場合
    ///        (規定値の1行文字列の後に改行文字がない場合など)
    /// @throw igesio::NotImplementedError 圧縮形式のIGESファイルを読み込もうとした場合
    explicit IgesReader(const std::string&);

    /// @brief スタートセクションを読み込む
    /// @return スタートセクションの文字列.
    ///         既に読み込んだ場合は`std::nullopt`を返す
    /// @throw igesio::LineFormatError 行の長さが規定値以外の場合
    ///        (規定値の1行文字列の後に改行文字がない場合など)
    /// @throw igesio::SectionFormatError セクションの順序やシーケンス番号が不正な場合など
    std::optional<std::string> ReadStartSection();

    /// @brief グローバルセクションを読み込む
    /// @return グローバルセクションのパラメータ.
    ///         既に読み込んだ場合や、まだスタートセクションを読み込んでいない場合は
    ///         `std::nullopt`を返す
    /// @throw igesio::LineFormatError 行の長さが規定値以外の場合
    ///        (規定値の1行文字列の後に改行文字がない場合など)
    /// @throw igesio::SectionFormatError セクションの順序やシーケンス番号が不正な場合など
    /// @throw igesio::TypeConversionError IGES文字列からC++の型への変換に失敗した場合
    /// @throw igesio::NotImplementedError 対応していない単位名が指定された場合
    std::optional<models::GlobalParam> ReadGlobalSection();

    /// @brief ディレクトリエントリセクションを読み込む
    /// @return 1つのディレクトリエントリセクションのレコード.
    ///         全てのレコードを読み込んだ場合や、まだグローバルセクションを
    ///         読み込んでいない場合は`std::nullopt`を返す
    /// @throw igesio::LineFormatError 行の長さが規定値以外の場合
    ///        (規定値の1行文字列の後に改行文字がない場合など)
    /// @throw igesio::SectionFormatError セクションの順序やシーケンス番号が不正な場合など
    /// @throw igesio::TypeConversionError IGES文字列からC++の型への変換に失敗した場合
    std::optional<entities::RawEntityDE> ReadDirectoryEntryRecord();

    /// @brief パラメータデータセクションを読み込む
    /// @return 1つのパラメータデータセクションのレコード.
    ///         全てのレコードを読み込んだ場合や、まだグローバルセクションを
    ///         読み込んでいない場合は`std::nullopt`を返す.
    ///         レコード区切り文字の後のコメントは取得しない.
    /// @throw igesio::LineFormatError 行の長さが規定値以外の場合
    ///        (規定値の1行文字列の後に改行文字がない場合など)
    /// @throw igesio::SectionFormatError セクションの順序やシーケンス番号が不正な場合など
    /// @throw igesio::TypeConversionError エンティティタイプの変換に失敗した場合
    std::optional<entities::RawEntityPD> ReadParameterDataRecord();

    /// @brief ターミネートセクションを読み込む
    /// @return スタート、グローバル、ディレクトリエントリ、パラメータデータセクションの行数.
    ///         既に読み込んだ場合や、まだグローバルセクションを読み込んでいない場合は
    ///         `std::nullopt`を返す.
    /// @throw igesio::LineFormatError 行の長さが規定値以外の場合
    ///        (規定値の1行文字列の後に改行文字がない場合など)
    /// @throw igesio::SectionFormatError パラメータが不正な場合など
    /// @throw igesio::TypeConversionError IGES文字列からC++の型への変換に失敗した場合
    std::optional<std::array<unsigned int, 4>> ReadTerminateSection();




    /// @brief 現在開いているファイルのパスを取得する
    /// @return 現在開いているファイルのパス
    const std::string& GetFilePath() const { return reader_.GetFilePath(); }

    /// @brief 次の行のセクションの種類
    /// @return 次の行のセクションの種類
    /// @throw igesio::SectionFormatError ターミネートセクションがまだ読み込まれて
    ///        いない状態で、次の行が存在しない場合
    /// @note すでにターミネートセクションを読み込んでいる場合は
    ///       std::nulloptを返す. それ以外の場合は、有効な値を返す.
    std::optional<SectionType> GetNextSectionType();



 private:
    /// @brief next_line_のデフォルト値
    inline static const std::tuple<std::string, SectionType, unsigned int>
    default_next_line_ = {"", SectionType::kTerminate, 0};

    /// @brief 次の行の情報
    /// @note [次の行の文字列, セクションの種類, シーケンス番号]、
    ///       デフォルト値は `{"", SectionType::kTerminate, 0}`
    /// @note ここの値が空でない場合、`GetLine`メンバ関数では`IgesBinaryReader`の
    ///       `GetLine`メンバ関数は呼び出さず、この値をそのまま返す
    std::tuple<std::string, SectionType, unsigned int> next_line_ = default_next_line_;

    /// @brief next_line_が空か
    /// @return next_line_が空であればtrue、そうでなければfalse
    bool IsNextLinePoolEmpty() const {
        return std::get<0>(next_line_).empty() &&
               std::get<1>(next_line_) == SectionType::kTerminate;
    }

    /// @brief next_line_に値をセットする
    /// @param line 読み込んだ行の情報
    void PoolNextLine(const std::tuple<std::string, SectionType, unsigned int>& line
                      = default_next_line_) {
        next_line_ = line;
    }

    /// @brief 次の行を取得する. プールしている行がある場合はそれを返す
    /// @return 次の行の情報、次の行が存在しない場合はstd::nulloptを返す
    ///   - std::string: 次の行の文字列
    ///   - SectionType: セクションの種類
    ///   - unsigned int: シーケンス番号
    /// @throw igesio::LineFormatError 行の長さが規定値以外の場合
    ///        (規定値の1行文字列の後に改行文字がない場合など)
    /// @throw igesio::SectionFormatError セクションの順序やシーケンス番号が不正な場合
    /// @note next_line_から値を取得した場合は、next_line_をデフォルト値に戻す
    std::optional<std::tuple<std::string, SectionType, unsigned int>>
    GetLine();

    /// @brief 指定したセクションの行を読み込む
    /// @param section 読み込むセクションの種類
    /// @return 次の行の情報、次の行がsectionではない場合にはstd::nulloptを返す
    ///   - std::string: 次の行の文字列
    ///   - SectionType: セクションの種類
    ///   - unsigned int: シーケンス番号
    /// @throw igesio::LineFormatError 行の長さが規定値以外の場合
    /// @throw igesio::SectionFormatError セクションの順序やシーケンス番号が不正な場合
    /// @note next_line_から値を取得した場合は、next_line_をデフォルト値に戻す
    /// @note 一回前のGetLineでkGlobalを読み込んだ(戻り値として取得した)場合に、
    ///       `section = kGlobal`を指定して呼び出すと、次の行もkGlobalの場合のみ
    ///       読み込むことができる。一方、次の行がkDirectoryやkDataの
    ///       (kGlobalの末端行まで読み込んだ) 場合はstd::nulloptを返す
    std::optional<std::tuple<std::string, SectionType, unsigned int>>
    GetLine(const SectionType);

    /// @brief パラメータ区切り文字
    /// @note グローバルセクション読み込み時に更新する
    char parameter_delimiter_ = ',';
    /// @brief レコード区切り文字
    /// @note グローバルセクション読み込み時に更新する
    char record_delimiter_ = ';';
};



/// @brief IGESファイルを読み込み、入出力用の中間生成物を返す
/// @param file_path 読み込むIGESファイルのパス
/// @param validate_strictly 仕様にのっとった厳密な検証を行うかどうか.
///        現状はDEセクションのデータ形式の検証のみを行う.
/// @throw igesio::FileOpenError ファイルが開けなかった場合
/// @throw igesio::LineFormatError 行の長さが規定値以外の場合
/// @throw igesio::DataFormatError 読み込んだファイルのデータの形式が
///        仕様に合致しない場合. validate_strictlyがfalseの場合は発生しない.
/// @note 仕様に厳密には従っていないIGESファイルも存在するため、
///       validate_strictlyをtrueにする際は注意が必要.
models::IntermediateIgesData
ReadIgesIntermediate(const std::string&, const bool = false);

}  // namespace igesio

#endif  // IGESIO_READER_H_
