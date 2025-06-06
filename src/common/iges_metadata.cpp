/**
 * @file common/iges_metadata.cpp
 * @brief IGESに関するメタデータを定義するヘッダファイル
 * @author Yayoi Habami
 * @date 2025-04-19
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/common/iges_metadata.h"

#include <string>

std::string igesio::SectionTypeToString(const SectionType& section_type)  {
    switch (section_type) {
        case SectionType::kFlag: return "Flag";
        case SectionType::kStart: return "Start";
        case SectionType::kGlobal: return "Global";
        case SectionType::kDirectory: return "Directory";
        case SectionType::kParameter: return "Parameter";
        case SectionType::kTerminate: return "Terminate";
        case SectionType::kData: return "Data";
        default: return "Unknown";
    }
}
