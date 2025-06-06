/**
 * @file utils/test_iges_string_utils_basic.cpp
 * @brief utils/iges_string_utils.hのうち、基本操作のテスト
 * @author Yayoi Habami
 * @date 2025-04-12
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <string>
#include <optional>

#include "igesio/utils/iges_string_utils.h"

namespace {

namespace i_util = igesio::utils;

}  // namespace

// rtrim関数のテスト - 末尾の空白を削除する機能をテストする
TEST(IgesStringUtilsTest, RtrimTest) {
    // 末尾に空白がある場合
    EXPECT_EQ("test", i_util::rtrim("test  "));
    // 複数の末尾空白を削除
    EXPECT_EQ("hello", i_util::rtrim("hello    "));

    // 空白がない場合は変化なし
    EXPECT_EQ("nochange", i_util::rtrim("nochange"));

    // 空文字列の場合
    EXPECT_EQ("", i_util::rtrim(""));
    // 全て空白の場合
    EXPECT_EQ("", i_util::rtrim("  "));

    // 先頭に空白がある場合は削除しない
    EXPECT_EQ("  test", i_util::rtrim("  test"));
    // 両端に空白がある場合は末尾のみ削除
    EXPECT_EQ("  test", i_util::rtrim("  test  "));
}

// ltrim関数のテスト - 先頭の空白を削除する機能をテストする
TEST(IgesStringUtilsTest, LtrimTest) {
    // 先頭に空白がある場合
    EXPECT_EQ("test", i_util::ltrim("  test"));
    // 複数の先頭空白を削除
    EXPECT_EQ("hello", i_util::ltrim("    hello"));

    // 空白がない場合は変化なし
    EXPECT_EQ("nochange", i_util::ltrim("nochange"));

    // 空文字列の場合
    EXPECT_EQ("", i_util::ltrim(""));
    // 全て空白の場合
    EXPECT_EQ("", i_util::ltrim("  "));

    // 末尾に空白がある場合は削除しない
    EXPECT_EQ("test  ", i_util::ltrim("test  "));
    // 両端に空白がある場合は先頭のみ削除
    EXPECT_EQ("test  ", i_util::ltrim("  test  "));
}

/**
 * @brief trim関数のテスト - 両端の空白を削除する機能をテストする
 */
TEST(IgesStringUtilsTest, TrimTest) {
    // 両端に空白がある場合
    EXPECT_EQ("test", i_util::trim("  test  "));
    // 先頭のみ空白がある場合
    EXPECT_EQ("test", i_util::trim("  test"));
    // 末尾のみ空白がある場合
    EXPECT_EQ("test", i_util::trim("test  "));

    // 空白がない場合は変化なし
    EXPECT_EQ("nochange", i_util::trim("nochange"));

    // 空文字列の場合
    EXPECT_EQ("", i_util::trim(""));
    // 全て空白の場合
    EXPECT_EQ("", i_util::trim("    "));

    // 中間に空白がある場合は削除しない
    EXPECT_EQ("hello world", i_util::trim("  hello world  "));
}
