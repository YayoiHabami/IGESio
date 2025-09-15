/**
 * @file writer.h
 * @brief IGESファイルを読み込むための関数を定義する
 * @author Yayoi Habami
 * @date 2025-05-31
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_WRITER_H_
#define IGESIO_WRITER_H_

#include <string>

#include "igesio/entities/de/raw_entity_de.h"
#include "igesio/entities/pd.h"
#include "igesio/models/intermediate.h"
#include "igesio/models/iges_data.h"



namespace igesio {

/// @brief 中間生成物を読み込み、IGESファイルを生成する
/// @param data 中間生成物
/// @param file_path 出力するIGESファイルのパス
/// @return 書き込みに成功したか
/// @throw igesio::FileOpenError ファイルが開けなかった場合.
///        親ディレクトリが存在せず、かつ作成できなかった場合も含む.
/// @throw igesio::DataFormatError dataのRawEntityPDが連続した奇数でない場合.
/// @note dataが含む全てのDEポインタ（DEセクションのsequence_number）が、
///       1, 3, 5, ...のように、連続した奇数であることを前提とする.
///       この順番に従ってDEセクションを書き込む. また、各ポインタが指す
///       数値が、DEセクションのsequence_numberと一致することを前提とする.
///       こちらについては、検証を行わない.
/// @note すでにfile_pathに同名のファイルが存在する場合は上書きする.
bool WriteIgesIntermediate(const models::IntermediateIgesData&, const std::string&);

/// @brief IgesDataを読み込み、中間データ構造を作成する
/// @param data IgesData
/// @param save_unsupported UnsupportedEntityを変換するか、
///        falseの場合、dataにUnsupportedEntityが含まれていれば
///        igesio::TypeConversionErrorを投げる
/// @return 中間データ構造
/// @throw igesio::TypeConversionError dataにUnsupportedEntityが含まれており、
///        save_unsupportedがfalseの場合
/// @throw igesio::DataFormatError dataのエンティティが、dataが持たないエンティティを
///        参照している場合
models::IntermediateIgesData
ConvertToIntermediate(const models::IgesData&, const bool = false);

/// @brief IgesDataを読み込み、IGESファイルを出力する
/// @param data IgesData
/// @param file_path 出力するIGESファイルのパス
/// @param save_unsupported UnsupportedEntityを変換するか、
///        falseの場合、dataにUnsupportedEntityが含まれていれば
///        igesio::TypeConversionErrorを投げる
/// @return 書き込みに成功したか
/// @throw igesio::FileOpenError ファイルが開けなかった場合.
///        親ディレクトリが存在せず、かつ作成できなかった場合も含む.
/// @throw igesio::TypeConversionError dataにUnsupportedEntityが含まれており、
///        かつsave_unsupportedがfalseの場合
/// @throw igesio::DataFormatError dataのエンティティが、dataが持たないエンティティを
///        参照している場合
/// @note すでにfile_pathに同名のファイルが存在する場合は上書きする.
bool WriteIges(const models::IgesData&, const std::string&, const bool = false);

}  // namespace igesio

#endif  // IGESIO_WRITER_H_
