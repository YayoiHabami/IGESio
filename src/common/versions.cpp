/**
 * @file common/versions.cpp
 * @brief 本ライブラリのバージョン情報を定義するヘッダファイル
 * @author Yayoi Habami
 * @date 2025-05-31
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/common/versions.h"

#include <string>



int igesio::GetVersionNumber() {
    return kVersionMajor * 10000 + kVersionMinor * 100 + kVersionPatch;
}

std::string igesio::GetVersion() {
    return std::to_string(kVersionMajor) + "." +
           std::to_string(kVersionMinor) + "." +
           std::to_string(kVersionPatch);
}

std::string igesio::GetLibraryName() {
    return "IGESio C++ Library";
}
