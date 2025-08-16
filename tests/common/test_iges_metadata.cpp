/**
 * @file common/test_iges_metadata.cpp
 * @brief common/iges_metadata.hのテスト
 * @author Yayoi Habami
 * @date 2025-04-13
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <string>
#include <optional>

#include "igesio/common/iges_metadata.h"



// IGESのセクションの種類を文字列に変換するテスト
TEST(MetadataTest, SectionTypeToString) {
    // 各セクションタイプが正しく文字列に変換されることを確認
    EXPECT_EQ("Flag", igesio::SectionTypeToString(igesio::SectionType::kFlag));
    EXPECT_EQ("Start", igesio::SectionTypeToString(igesio::SectionType::kStart));
    EXPECT_EQ("Global", igesio::SectionTypeToString(igesio::SectionType::kGlobal));
    EXPECT_EQ("Directory", igesio::SectionTypeToString(igesio::SectionType::kDirectory));
    EXPECT_EQ("Parameter", igesio::SectionTypeToString(igesio::SectionType::kParameter));
    EXPECT_EQ("Terminate", igesio::SectionTypeToString(igesio::SectionType::kTerminate));
    EXPECT_EQ("Data", igesio::SectionTypeToString(igesio::SectionType::kData));

    // デフォルトケースのテスト（未知の列挙型値）
    igesio::SectionType unknown_type = static_cast<igesio::SectionType>(999);
    EXPECT_EQ("Unknown", igesio::SectionTypeToString(unknown_type));
}

TEST(MetadataTest, IsCompatibleParameterType_CppToIGES) {
    // kBool
    EXPECT_TRUE(igesio::IsCompatibleParameterType(
            igesio::CppParameterType::kBool, igesio::IGESParameterType::kLogical));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::CppParameterType::kBool, igesio::IGESParameterType::kInteger));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::CppParameterType::kBool, igesio::IGESParameterType::kReal));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::CppParameterType::kBool, igesio::IGESParameterType::kPointer));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::CppParameterType::kBool, igesio::IGESParameterType::kString));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::CppParameterType::kBool, igesio::IGESParameterType::kLanguageStatement));

    // kInt
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::CppParameterType::kInt, igesio::IGESParameterType::kLogical));
    EXPECT_TRUE(igesio::IsCompatibleParameterType(
            igesio::CppParameterType::kInt, igesio::IGESParameterType::kInteger));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::CppParameterType::kInt, igesio::IGESParameterType::kReal));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::CppParameterType::kInt, igesio::IGESParameterType::kPointer));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::CppParameterType::kInt, igesio::IGESParameterType::kString));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::CppParameterType::kInt, igesio::IGESParameterType::kLanguageStatement));

    // kDouble
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::CppParameterType::kDouble, igesio::IGESParameterType::kLogical));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::CppParameterType::kDouble, igesio::IGESParameterType::kInteger));
    EXPECT_TRUE(igesio::IsCompatibleParameterType(
            igesio::CppParameterType::kDouble, igesio::IGESParameterType::kReal));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::CppParameterType::kDouble, igesio::IGESParameterType::kPointer));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::CppParameterType::kDouble, igesio::IGESParameterType::kString));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::CppParameterType::kDouble, igesio::IGESParameterType::kLanguageStatement));

    // kPointer
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::CppParameterType::kPointer, igesio::IGESParameterType::kLogical));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::CppParameterType::kPointer, igesio::IGESParameterType::kInteger));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::CppParameterType::kPointer, igesio::IGESParameterType::kReal));
    EXPECT_TRUE(igesio::IsCompatibleParameterType(
            igesio::CppParameterType::kPointer, igesio::IGESParameterType::kPointer));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::CppParameterType::kPointer, igesio::IGESParameterType::kString));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::CppParameterType::kPointer, igesio::IGESParameterType::kLanguageStatement));

    // kString
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::CppParameterType::kString, igesio::IGESParameterType::kLogical));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::CppParameterType::kString, igesio::IGESParameterType::kInteger));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::CppParameterType::kString, igesio::IGESParameterType::kReal));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::CppParameterType::kString, igesio::IGESParameterType::kPointer));
    EXPECT_TRUE(igesio::IsCompatibleParameterType(
            igesio::CppParameterType::kString, igesio::IGESParameterType::kString));
    EXPECT_TRUE(igesio::IsCompatibleParameterType(
            igesio::CppParameterType::kString, igesio::IGESParameterType::kLanguageStatement));

    // Unknown CppParameterType
    igesio::CppParameterType unknown_cpp_type = static_cast<igesio::CppParameterType>(999);
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            unknown_cpp_type, igesio::IGESParameterType::kLogical));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            unknown_cpp_type, igesio::IGESParameterType::kInteger));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            unknown_cpp_type, igesio::IGESParameterType::kReal));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            unknown_cpp_type, igesio::IGESParameterType::kPointer));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            unknown_cpp_type, igesio::IGESParameterType::kString));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            unknown_cpp_type, igesio::IGESParameterType::kLanguageStatement));
}

TEST(MetadataTest, IsCompatibleParameterType_IGESToCpp) {
    // kBool
    EXPECT_TRUE(igesio::IsCompatibleParameterType(
            igesio::IGESParameterType::kLogical, igesio::CppParameterType::kBool));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::IGESParameterType::kLogical, igesio::CppParameterType::kInt));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::IGESParameterType::kLogical, igesio::CppParameterType::kDouble));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::IGESParameterType::kLogical, igesio::CppParameterType::kPointer));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::IGESParameterType::kLogical, igesio::CppParameterType::kString));

    // kInt
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::IGESParameterType::kInteger, igesio::CppParameterType::kBool));
    EXPECT_TRUE(igesio::IsCompatibleParameterType(
            igesio::IGESParameterType::kInteger, igesio::CppParameterType::kInt));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::IGESParameterType::kInteger, igesio::CppParameterType::kDouble));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::IGESParameterType::kInteger, igesio::CppParameterType::kPointer));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::IGESParameterType::kInteger, igesio::CppParameterType::kString));

    // kDouble
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::IGESParameterType::kReal, igesio::CppParameterType::kBool));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::IGESParameterType::kReal, igesio::CppParameterType::kInt));
    EXPECT_TRUE(igesio::IsCompatibleParameterType(
            igesio::IGESParameterType::kReal, igesio::CppParameterType::kDouble));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::IGESParameterType::kReal, igesio::CppParameterType::kPointer));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::IGESParameterType::kReal, igesio::CppParameterType::kString));

    // kPointer
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::IGESParameterType::kPointer, igesio::CppParameterType::kBool));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::IGESParameterType::kPointer, igesio::CppParameterType::kInt));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::IGESParameterType::kPointer, igesio::CppParameterType::kDouble));
    EXPECT_TRUE(igesio::IsCompatibleParameterType(
            igesio::IGESParameterType::kPointer, igesio::CppParameterType::kPointer));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::IGESParameterType::kPointer, igesio::CppParameterType::kString));

    // kString
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::IGESParameterType::kString, igesio::CppParameterType::kBool));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::IGESParameterType::kString, igesio::CppParameterType::kInt));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::IGESParameterType::kString, igesio::CppParameterType::kDouble));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::IGESParameterType::kString, igesio::CppParameterType::kPointer));
    EXPECT_TRUE(igesio::IsCompatibleParameterType(
            igesio::IGESParameterType::kString, igesio::CppParameterType::kString));

    // kLanguageStatement
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::IGESParameterType::kLanguageStatement, igesio::CppParameterType::kBool));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::IGESParameterType::kLanguageStatement, igesio::CppParameterType::kInt));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::IGESParameterType::kLanguageStatement, igesio::CppParameterType::kDouble));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            igesio::IGESParameterType::kLanguageStatement, igesio::CppParameterType::kPointer));
    EXPECT_TRUE(igesio::IsCompatibleParameterType(
            igesio::IGESParameterType::kLanguageStatement, igesio::CppParameterType::kString));

    // Unknown IGESParameterType (no default case in the function,
    // so testing with an invalid value)
    igesio::IGESParameterType unknown_iges_type = static_cast<igesio::IGESParameterType>(999);
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            unknown_iges_type, igesio::CppParameterType::kBool));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            unknown_iges_type, igesio::CppParameterType::kInt));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            unknown_iges_type, igesio::CppParameterType::kDouble));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            unknown_iges_type, igesio::CppParameterType::kPointer));
    EXPECT_FALSE(igesio::IsCompatibleParameterType(
            unknown_iges_type, igesio::CppParameterType::kString));
}

