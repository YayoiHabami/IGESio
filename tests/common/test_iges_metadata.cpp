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
