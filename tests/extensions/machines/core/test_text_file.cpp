/**
 * @file tests/extensions/machines/core/test_text_file.cpp
 * @brief テキストファイルの読み書き (core/text_file) のテスト
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note 対象: ReadTextFile / WriteTextFile
 *       - 正常系 (代表値): 書いた内容がそのまま読める、CRLFが変換されない,
 *         既存ファイルの上書き
 *       - 正常系 (境界値・退化): 空文字列、末尾に改行の無い内容
 *       - 異常系: 存在しないファイルの読込 (`FileOpenError`)、存在しない
 *         ディレクトリへの書込 (`FileOpenError`)
 *       TODO: 読込途中の失敗 (`stream.bad()`) は再現手段が無いため未検証
 */
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "igesio/common/errors.h"
#include "igesio/extensions/machines/core/text_file.h"

namespace {

namespace fs = std::filesystem;
namespace mc = igesio::extensions::machines;

/// @brief テスト用の一時ディレクトリを用意し、終了時に削除するフィクスチャ
class TextFileTest : public ::testing::Test {
 protected:
    /// @brief 一時ディレクトリを作る
    void SetUp() override {
        dir_ = fs::temp_directory_path() / "igesio_text_file_test";
        fs::create_directories(dir_);
    }
    /// @brief 一時ディレクトリを削除する
    void TearDown() override {
        std::error_code ec;
        fs::remove_all(dir_, ec);
    }
    /// @brief 一時ディレクトリ内のパスを作る
    fs::path Path(const std::string& name) const { return dir_ / name; }

 private:
    /// @brief 一時ディレクトリ
    fs::path dir_;
};

}  // namespace



// ---- ReadTextFile / WriteTextFile ----

TEST_F(TextFileTest, ReadWrite_RoundTripKeepsContent) {
    const std::string text = "G00 X1.\nG01 Y2.\n";
    mc::WriteTextFile(Path("lf.nc"), text);
    EXPECT_EQ(mc::ReadTextFile(Path("lf.nc")), text);
}

TEST_F(TextFileTest, ReadWrite_RoundTripKeepsCrlf) {
    // バイナリモードなのでCRLFがLFに変換されない
    const std::string text = "%\r\nO0001\r\nM30\r\n%\r\n";
    mc::WriteTextFile(Path("crlf.nc"), text);
    const std::string read = mc::ReadTextFile(Path("crlf.nc"));
    EXPECT_EQ(read, text);
    EXPECT_NE(read.find("\r\n"), std::string::npos);
}

TEST_F(TextFileTest, Write_OverwritesExistingFile) {
    mc::WriteTextFile(Path("over.nc"), "long long long content\n");
    mc::WriteTextFile(Path("over.nc"), "short\n");
    EXPECT_EQ(mc::ReadTextFile(Path("over.nc")), "short\n");
}

TEST_F(TextFileTest, ReadWrite_EmptyContent) {
    mc::WriteTextFile(Path("empty.nc"), "");
    EXPECT_EQ(mc::ReadTextFile(Path("empty.nc")), "");
}

TEST_F(TextFileTest, ReadWrite_NoTrailingNewline) {
    mc::WriteTextFile(Path("nonl.nc"), "M30");
    EXPECT_EQ(mc::ReadTextFile(Path("nonl.nc")), "M30");
}

TEST_F(TextFileTest, Read_ThrowsFileOpenErrorWhenFileIsMissing) {
    EXPECT_THROW(mc::ReadTextFile(Path("missing.nc")), igesio::FileOpenError);
}

TEST_F(TextFileTest, Write_ThrowsFileOpenErrorWhenDirectoryIsMissing) {
    EXPECT_THROW(mc::WriteTextFile(Path("no_such_dir") / "x.nc", "x"),
                 igesio::FileOpenError);
}
