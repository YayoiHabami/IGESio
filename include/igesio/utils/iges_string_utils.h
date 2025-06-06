/**
 * @file utils/iges_string_utils.h
 * @brief IGESデータ文字列の検証・変換・生成を行う関数群
 * @author Yayoi Habami
 * @date 2025-04-11
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_UTILS_IGES_STRING_UTILS_H_
#define IGESIO_UTILS_IGES_STRING_UTILS_H_

#include <optional>
#include <string>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/common/serialization.h"
#include "igesio/common/iges_parameter_vector.h"

/// @brief プロジェクト全体で使用する操作を定義する
namespace igesio::utils {

/**
 * 基本操作
 */

using igesio::rtrim;
using igesio::ltrim;
using igesio::trim;

/// @brief 与えられた文字列が半角スペースのみで構成されているかを確認する
/// @param s 文字列
/// @return true: 半角スペースのみ, false: それ以外
/// @note 与えられた文字列が空文字列の場合もfalseを返す
bool IsOnlySpace(const std::string&);



/**
 * IGES文字列の検証
 */

/// @brief 文字列長さがIGESの規定値以外ではないかを確認する
/// @param line 文字列
/// @param is_compressed 圧縮形式かどうかのフラグ、通常はfalse
/// @throw igesio::LineFormatError 文字列長さが規定値以外の場合
/// @note 基本的には80文字であるが、圧縮形式のデータセクションのみ73文字未満
void AssertLength(const std::string&, const bool = false);



/**
 * シンプルな操作 (IGES文字列)
 */

/// @brief セクションの種類を特定する
/// @param line セクションの行 (改行文字を含まない)
/// @param is_compressed 圧縮形式かどうかのフラグ、通常はfalse
/// @return セクションの種類
/// @throw igesio::LineFormatError 1行の長さが規定値以外の場合
/// @throw igesio::SectionFormatError 規定以外のセクション文字が指定された場合
SectionType GetSectionType(const std::string&, const bool = false);

/// @brief シーケンス番号 (各行の末尾7桁の数値) を取得する
/// @param line シーケンス番号を含む行
/// @param is_compressed 圧縮形式かどうかのフラグ、通常はfalse
/// @return シーケンス番号
/// @throw igesio::LineFormatError 1行の長さが規定値以外の場合
/// @throw igesio::SectionFormatError 圧縮形式のデータセクションの行が指定された場合、
///        シーケンス番号部分が数値に変換できない場合
unsigned int GetSequenceNumber(const std::string&, const bool = false);

/// @brief パラメータセクションの行から、DEポインタを取得する
/// @param line パラメータセクションの行
/// @return DEポインタ
/// @throw igesio::LineFormatError 1行の長さが規定値以外の場合、
///        圧縮形式のデータセクションの行が指定された場合
/// @throw igesio::SectionFormatError 規定以外のセクション文字が指定された場合
///        パラメータセクションでない場合
unsigned int GetDEPointer(const std::string&);

/// @brief データ部分を取り出す
/// @param line データ部分を含む行
/// @param section_type セクションの種類
/// @return データ部分の文字列
/// @note 返されるデータは以下の通り:
///       - 空文字列: フラグセクション
///       - 32文字: ターミネートセクション
///       - 64文字: パラメータデータセクション (DEポインタの1文字前まで)
///       - 72文字以下: 圧縮形式のデータセクション
///       - 72文字: それ以外のセクション (セクション判別文字の1文字前まで)
/// @throw igesio::LineFormatError 1行の長さが取得される文字数よりも短い場合
std::string GetDataPart(const std::string&, const SectionType);

/// @brief 各セクションのデータ部をパースする
/// @param lines 各セクションの行のデータ部のみを含む文字列のベクタ.
///        対象はkGlobal, kParameterのセクションの行 (see Section 2.2.3).
///        各行の長さは問わない.
/// @param p_delim パラメータ区切り文字
/// @param r_delim レコード区切り文字
/// @return データ部をパラメータ区切り文字で分割した文字列のベクタ
/// @throw igesio::SectionFormatError 区切り文字が存在しない場合、
///        不正な文字列ステートメント（H文字列）が含まれる場合、
///        与えられた行の中にレコード区切り文字がない (レコードの末端がない) 場合
/// @note IGES5.3で定義されるデータ型のうち、言語ステートメントデータ型についてのみ
///       完全な対応が出来ていないため、注意が必要. IGES5.3では、
///       言語ステートメントデータ型は、ASCII文字セットの英数字、句読点 (punctuation)、
///       およびスペースを含む任意の文字列を許可しているが、本実装では、
///       区切り文字 (`p_delim`と`r_delim`) を含む文字列は許可していない.
///       そのため、たとえば区切り文字が'R'の場合、マクロの'REPEAT'は'R','P','AT'に
///       分割されてしまう.
/// @note PDセクションのコメント (レコード区切り文字の後に追加されうる文字列) は
///       無視される (単純に切り捨てられ、返されない).
std::vector<std::string>
ParseFreeFormattedData(const std::vector<std::string>&, const char, const char);



/**
 * グローバルセクションの読み込み
 */

/// @brief 区切り文字として使用可能な文字かを確認する
/// @param delim 区切り文字
/// @return 区切り文字として使用可能な場合はtrue
/// @note 許容されないのは、制御文字 (0x00～0x1F, 0x7F) と
///       スペース (0x20)、数字 (0x30～0x39)、+-. (0x2B, 0x2D, 0x2E)、
///       および文字DEH (0x44, 0x45, 0x48)
bool IsValidDelimiter(const char);

/// @brief パラメータ区切り文字を判定する
/// @param line グローバルセクションの1行目
/// @return 区切り文字、通常は','
/// @throw igesio::LineFormatError 1行の長さが規定値以外の場合
/// @throw igesio::SectionFormatError 区切り文字の指定が不正な場合
char GetParameterDelimiter(const std::string&);

/// @brief レコード区切り文字を判定する
/// @param line グローバルセクションの1行目
/// @param p_delim パラメータ区切り文字
/// @return 区切り文字、通常は';'
/// @throw igesio::LineFormatError 1行の長さが規定値以外の場合
/// @throw igesio::SectionFormatError 区切り文字の指定が不正な場合
/// @throw igesio::TypeConversionError 区切り文字を文字列に変換できない場合
char GetRecordDelimiter(const std::string&, const char = ',');



/**
 * 出力
 */

/// @brief パラメータを表す文字列のベクタをIGESの自由形式の行に変換する
/// @param parameters パラメータを表す文字列のベクタ
/// @param parameter_types 各パラメータの型
/// @param max_line_length 1行あたりの最大文字数.
///        グローバルセクションでは72文字、パラメータデータセクションでは
///        64文字など.
/// @param p_delim パラメータ区切り文字
/// @param r_delim レコード区切り文字
/// @return IGESの自由形式の行. max_line_length文字に満たない場合は
///         右側を空白で埋め、全ての行がmax_line_length文字になるようにする.
/// @note 各パラメータの型が分からない場合は、String型を除きInteger型とされたい.
///       String型のみ、文字列長さが長い場合に、行をまたいで出力されることが
///       あるため、その区別を行うために、パラメータの型を指定する必要がある.
/// @throw std::invalid_argument parametersとparameter_typesのサイズが
///         一致しない場合.
std::vector<std::string> ToFreeFormattedLines(
    const std::vector<std::string>&, const std::vector<IGESParameterType>&,
    const unsigned int, const char, const char);

/// @brief パラメータを表すベクタをIGESの自由形式の行に変換する
/// @param parameters パラメータを表すベクタ.
/// @param max_line_length 1行あたりの最大文字数.
///        グローバルセクションでは72文字、パラメータデータセクションでは
///        64文字など.
/// @param p_delim パラメータ区切り文字
/// @param r_delim レコード区切り文字
/// @param config 数値を文字列化する際のパラメータ
/// @return IGESの自由形式の行. max_line_length文字に満たない場合は
///         右側を空白で埋め、全ての行がmax_line_length文字になるようにする.
std::vector<std::string> ToFreeFormattedLines(
    const IGESParameterVector&, const unsigned int, const char, const char,
    const SerializationConfig&);

}  // namespace igesio::utils

#endif  // IGESIO_UTILS_IGES_STRING_UTILS_H_
