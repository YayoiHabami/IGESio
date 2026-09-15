/**
 * @file extensions/machines/core/text_file.cpp
 * @brief テキストファイル (NC/CL等) の読み書き
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/machines/core/text_file.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

#include "igesio/common/errors.h"

namespace igesio::extensions::machines {

std::string ReadTextFile(const std::filesystem::path& path) {
    // 改行を変換しないためバイナリモードで開く
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw igesio::FileOpenError("Failed to open text file: " +
                                    path.string());
    }
    std::string text((std::istreambuf_iterator<char>(stream)),
                     std::istreambuf_iterator<char>());
    if (stream.bad()) {
        throw igesio::FileOpenError("Failed to read text file: " +
                                    path.string());
    }
    return text;
}

void WriteTextFile(const std::filesystem::path& path,
                   const std::string_view text) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        throw igesio::FileOpenError("Failed to open text file: " +
                                    path.string());
    }
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!stream) {
        throw igesio::FileOpenError("Failed to write text file: " +
                                    path.string());
    }
}

}  // namespace igesio::extensions::machines
