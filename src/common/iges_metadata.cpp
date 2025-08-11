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

bool igesio::IsCompatibleParameterType(const CppParameterType cpp_type,
                                       const IGESParameterType iges_type) {
    switch (cpp_type) {
        case CppParameterType::kBool:
            return iges_type == IGESParameterType::kLogical;
        case CppParameterType::kInt:
            return iges_type == IGESParameterType::kInteger;
        case CppParameterType::kDouble:
            return iges_type == IGESParameterType::kReal;
        case CppParameterType::kPointer:
            return iges_type == IGESParameterType::kPointer;
        case CppParameterType::kString:
            return iges_type == IGESParameterType::kString ||
                   iges_type == IGESParameterType::kLanguageStatement;
        default:
            return false;  // Unsupported type
    }
}

bool igesio::IsCompatibleParameterType(const IGESParameterType iges_type,
                                       const CppParameterType cpp_type) {
    return IsCompatibleParameterType(cpp_type, iges_type);
}
