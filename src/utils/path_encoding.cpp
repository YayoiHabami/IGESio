/**
 * @file utils/path_encoding.cpp
 * @brief UTF-8文字列とファイルパスを相互に変換する関数群
 * @author Yayoi Habami
 * @date 2026-10-01
 * @copyright 2026 Yayoi Habami
 * @note C++17の`u8path`/`u8string`に依存する. C++20に移行する際は、
 *       `u8path`の非推奨化と`u8string`の戻り値型（`std::u8string`）
 *       の変更について、このファイル内で対応する.
 */
#include "igesio/utils/path_encoding.h"

#include <filesystem>
#include <string>

namespace igesio::utils {

std::filesystem::path PathFromUtf8(const std::string& utf8) {
    return std::filesystem::u8path(utf8);
}

std::string PathToUtf8(const std::filesystem::path& path) {
    return path.u8string();
}

std::string PathToGenericUtf8(const std::filesystem::path& path) {
    return path.generic_u8string();
}

}  // namespace igesio::utils
