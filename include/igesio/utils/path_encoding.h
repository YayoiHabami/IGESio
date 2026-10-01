/**
 * @file utils/path_encoding.h
 * @brief UTF-8文字列とファイルパスを相互に変換する関数群
 * @author Yayoi Habami
 * @date 2026-10-01
 * @copyright 2026 Yayoi Habami
 * @note ライブラリ内では、パスを表す`std::string`は常にUTF-8とする.
 *       `std::filesystem::path(std::string)`や`path.string()`は、
 *       WindowsではCP932で変換されるため、全角文字を含むパスで失敗する.
 *       パスと文字列の変換は必ずこのファイルの関数を用いること.
 */
#ifndef IGESIO_UTILS_PATH_ENCODING_H_
#define IGESIO_UTILS_PATH_ENCODING_H_

#include <filesystem>
#include <string>

namespace igesio::utils {

/// @brief UTF-8文字列をパスに変換する
/// @param utf8 UTF-8で表したパス
/// @return 変換したパス
std::filesystem::path PathFromUtf8(const std::string& utf8);

/// @brief パスをUTF-8文字列に変換する
/// @param path 変換するパス
/// @return UTF-8で表したパス. 区切り文字は環境に依存する
std::string PathToUtf8(const std::filesystem::path& path);

/// @brief パスを区切り文字が'/'のUTF-8文字列に変換する
/// @param path 変換するパス
/// @return UTF-8で表したパス
/// @note 設定ファイルに書き出すパスなど、環境に依存しない表記が必要な場合に用いる
std::string PathToGenericUtf8(const std::filesystem::path& path);

}  // namespace igesio::utils

#endif  // IGESIO_UTILS_PATH_ENCODING_H_
