/**
 * @file extensions/machines/core/text_file.h
 * @brief テキストファイル (NC/CL等) の読み書き
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note NC/CLの読込はバイト列のまま扱う (文字コードの変換や改行の正規化は行わない).
 *       改行の種類は読込側の字句解析が両方 (LF/CRLF) を受け入れられるようにし、
 *       書き出し側は`NewlineStyle` (`core/formatting.h`) に従って文字列化した後に
 *       本ヘッダで書く.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_CORE_TEXT_FILE_H_
#define IGESIO_EXTENSIONS_MACHINES_CORE_TEXT_FILE_H_

#include <filesystem>
#include <string>
#include <string_view>

namespace igesio::extensions::machines {

/// @brief テキストファイルをすべて読み込む
/// @param path ファイルのパス
/// @return ファイルの内容 (バイト列のまま. 改行は変換しない)
/// @throw igesio::FileOpenError ファイルを開けない、または読み込めない場合
std::string ReadTextFile(const std::filesystem::path& path);

/// @brief テキストファイルに書き込む (既存のファイルは上書きする)
/// @param path ファイルのパス
/// @param text 書き込む内容 (バイト列のまま. 改行は変換しない)
/// @throw igesio::FileOpenError ファイルを開けない、または書き込めない場合
void WriteTextFile(const std::filesystem::path& path, std::string_view text);

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_CORE_TEXT_FILE_H_
