/**
 * @file common/test_versions.cpp
 * @brief versions.hのテスト
 * @author Yayoi Habami
 * @date 2025-06-06
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <string>

#include "igesio/common/versions.h"



// 定数値のテスト
TEST(VersionsTest, ConstantValues) {
    // バージョン定数が0以上の値であることを確認
    EXPECT_GE(igesio::kVersionMajor, 0);
    EXPECT_GE(igesio::kVersionMinor, 0);
    EXPECT_GE(igesio::kVersionPatch, 0);

    // バージョン文字列がnullptrでないことを確認
    EXPECT_NE(igesio::kVersionString, nullptr);

    // バージョン文字列が空でないことを確認
    EXPECT_GT(std::string(igesio::kVersionString).length(), 0);
}

// GetVersionNumber関数のテスト
TEST(VersionsTest, GetVersionNumber) {
    int version_number = igesio::GetVersionNumber();

    // バージョン番号が正の値であることを確認
    EXPECT_GT(version_number, 0);

    // バージョン番号の計算式が正しいことを確認
    int expected = igesio::kVersionMajor * 10000
                 + igesio::kVersionMinor * 100
                 + igesio::kVersionPatch;
    EXPECT_EQ(version_number, expected);
}

// GetVersion関数のテスト
TEST(VersionsTest, GetVersion) {
    std::string version = igesio::GetVersion();

    // バージョン文字列が空でないことを確認
    EXPECT_FALSE(version.empty());

    // バージョン文字列に期待される形式の要素が含まれることを確認
    EXPECT_NE(version.find('.'), std::string::npos);

    // 少なくとも "x.y.z" の形式であることを確認（最低5文字）
    EXPECT_GE(version.length(), 5);
}

// GetLibraryName関数のテスト
TEST(VersionsTest, GetLibraryName) {
    std::string library_name = igesio::GetLibraryName();

    // ライブラリ名が空でないことを確認
    EXPECT_FALSE(library_name.empty());

    // ライブラリ名が妥当な長さであることを確認
    EXPECT_GT(library_name.length(), 0);
}

// 定数とマクロの一貫性テスト
TEST(VersionsTest, MacroConstantConsistency) {
    // マクロと定数が一致することを確認
    EXPECT_EQ(igesio::kVersionMajor, IGESIO_VERSION_MAJOR);
    EXPECT_EQ(igesio::kVersionMinor, IGESIO_VERSION_MINOR);
    EXPECT_EQ(igesio::kVersionPatch, IGESIO_VERSION_PATCH);
    EXPECT_STREQ(igesio::kVersionString, IGESIO_VERSION_STRING);
}
