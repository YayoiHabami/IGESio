/**
 * @file tests/utils/test_path_encoding.cpp
 * @brief utils/path_encoding.h (UTF-8文字列とパスの相互変換) のテスト
 * @author Yayoi Habami
 * @date 2026-10-01
 * @copyright 2026 Yayoi Habami
 * @note 対象: PathFromUtf8 / PathToUtf8 / PathToGenericUtf8
 *       - 正常系: ASCII/全角/CP932外の文字の往復、区切り文字の正規化,
 *         環境固有の内部表現 (Windowsはワイド文字) との一致
 *       - 実ファイル: 全角名のディレクトリとファイルの作成と存在確認
 *       - 境界値: 空文字列
 */
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "igesio/utils/path_encoding.h"
#include "unicode_test_path.h"

namespace {

namespace fs = std::filesystem;
namespace i_util = igesio::utils;
using igesio::tests::kUnicodeName;
using igesio::tests::UnicodeTempDir;

}  // namespace



// ---- 文字列の往復 ----

// ASCIIのパスは変換の往復で変わらない
TEST(PathEncodingTest, RoundTrip_Ascii) {
    const std::string text = "dir/file.toml";
    EXPECT_EQ(i_util::PathToGenericUtf8(i_util::PathFromUtf8(text)), text);
}

// 全角文字とCP932外の文字を含むパスは変換の往復で変わらない
TEST(PathEncodingTest, RoundTrip_Unicode) {
    const std::string text = kUnicodeName + "/" + kUnicodeName + ".stl";
    EXPECT_EQ(i_util::PathToGenericUtf8(i_util::PathFromUtf8(text)), text);
    EXPECT_EQ(i_util::PathToUtf8(i_util::PathFromUtf8(kUnicodeName)),
              kUnicodeName);
}

// 空文字列は空のパスになり、空文字列に戻る
TEST(PathEncodingTest, RoundTrip_Empty) {
    EXPECT_TRUE(i_util::PathFromUtf8("").empty());
    EXPECT_EQ(i_util::PathToUtf8(fs::path()), "");
}

// 要素の分解 (ファイル名、拡張子) がUTF-8のまま取り出せる
TEST(PathEncodingTest, Decompose_FilenameAndExtension) {
    const fs::path path =
            i_util::PathFromUtf8(kUnicodeName + "/" + kUnicodeName + ".stl");
    EXPECT_EQ(i_util::PathToUtf8(path.filename()), kUnicodeName + ".stl");
    EXPECT_EQ(i_util::PathToUtf8(path.stem()), kUnicodeName);
    EXPECT_EQ(i_util::PathToUtf8(path.extension()), ".stl");
}

// 区切り文字は、PathToGenericUtf8では'/'になる
TEST(PathEncodingTest, Generic_UsesSlashSeparator) {
    const fs::path path = fs::path("a") / i_util::PathFromUtf8(kUnicodeName);
    EXPECT_EQ(i_util::PathToGenericUtf8(path), "a/" + kUnicodeName);
}

// 内部表現が環境の文字コードと一致する
TEST(PathEncodingTest, Native_MatchesPlatformEncoding) {
    const fs::path path = i_util::PathFromUtf8(kUnicodeName);
#ifdef _WIN32
    // Windowsの内部表現はUTF-16
    EXPECT_EQ(path.native(), std::wstring(L"全角_\U0001F642"));
#else
    // POSIX環境の内部表現はバイト列をそのまま保持する
    EXPECT_EQ(path.native(), kUnicodeName);
#endif
}



// ---- 実ファイル ----

// 全角名のディレクトリとファイルを作成し、変換したパスで参照できる
TEST(PathEncodingTest, FileSystem_CreateAndFindUnicodeFile) {
    const UnicodeTempDir dir("path_encoding");
    const std::string name = kUnicodeName + ".txt";
    {
        std::ofstream stream(dir.Join(name), std::ios::binary);
        ASSERT_TRUE(stream.good());
        stream << "x";
    }
    EXPECT_TRUE(fs::is_regular_file(
            i_util::PathFromUtf8(dir.JoinUtf8(name))));

    // ディレクトリの列挙で得たファイル名がUTF-8で一致する
    bool found = false;
    for (const auto& entry : fs::directory_iterator(dir.Path())) {
        if (i_util::PathToUtf8(entry.path().filename()) == name) found = true;
    }
    EXPECT_TRUE(found);
}
