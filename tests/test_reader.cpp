/**
 * @file test_reader.cpp
 * @brief reader.hのテスト
 * @author Yayoi Habami
 * @date 2025-04-24
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/entities/pd.h"
#include "igesio/reader.h"
#include "igesio/utils/path_encoding.h"
#include "unicode_test_path.h"

namespace {

namespace iio = igesio;
namespace fs = std::filesystem;

/// @brief テスト用IGESファイルを格納するディレクトリ
const std::string kTestIgesDirPath =
        fs::path(__FILE__).parent_path().append("test_data").string();
/// @brief テスト用IGESファイルのパス (立方体の一辺が丸められたもの)
const std::string kSingleRoundCubePath =
        fs::path(kTestIgesDirPath).append("single_rounded_cube.iges").string();

}  // namespace




/*******************************************************************************
 * ReadIgesのテスト
 *****************************************************************************/

TEST(ReadIgesIntermediateTest, NormalCase) {
    // エラーが生じないことの確認
    ASSERT_NO_THROW(iio::ReadIgesIntermediate(kSingleRoundCubePath));

    auto data = iio::ReadIgesIntermediate(kSingleRoundCubePath);

    // スタートセクションの確認
    std::string expected =
    "This file represents the shape of a cube with one side filleted.";
    EXPECT_EQ(data.start_section, expected);

    // グローバルセクションの確認

    // ディレクトリエントリセクションの確認
    // エンティティは102個
    ASSERT_EQ(data.directory_entry_section.size(), 102);

    // パラメータデータセクションの確認
    // エンティティは102個
    ASSERT_EQ(data.parameter_data_section.size(), 102);

    // DEセクションのエンティティと同じエンティティタイプ
    std::size_t i = 0;
    for (const auto& de : data.directory_entry_section) {
        EXPECT_EQ(de.entity_type, data.parameter_data_section[i].type);
        i++;
    }

    // ターミネートセクションの確認
    // 1, 4, 204, 185
    ASSERT_EQ(data.terminate_section.size(), 4);
    EXPECT_EQ(data.terminate_section[0], 1);
    EXPECT_EQ(data.terminate_section[1], 4);
    EXPECT_EQ(data.terminate_section[2], 204);
    EXPECT_EQ(data.terminate_section[3], 185);
}

// kSingleRoundCubePathのDEパラメータは、仕様に厳密に従っているわけではないため、
// エラーが発生することを確認する
TEST(ReadIgesIntermediateTest, InvalidDEParameter) {
    // エラーが発生することの確認
    ASSERT_THROW(iio::ReadIgesIntermediate(kSingleRoundCubePath, true),
                 iio::DataFormatError);

    // エラーの内容を確認
    try {
        iio::ReadIgesIntermediate(kSingleRoundCubePath, true);
    } catch (const iio::DataFormatError& e) {
        // IGES 5.3に従えば、Curve on a Parametric Surface (Type 142) の
        // ステータス番号は "????00**" でなければならないが、テスト用では "????05**" となっている
        // (厳密に仕様に従っているわけではないプロセッサも存在することに留意)
        std::string message = e.what();
        EXPECT_TRUE(message.find(
            "     142      43       0       0       0                        00010500D     31")
            != std::string::npos)
            << "Expected error message to contain the invalid DE parameter line."
            << " Actual message: " << message;
    }
}




/*******************************************************************************
 * 全角パス
 *****************************************************************************/

// 全角名のファイルをUTF-8のパスで読み込める
TEST(ReadIgesTest, UnicodePath_Reads) {
    const iio::tests::UnicodeTempDir dir("reader");
    const std::string path = dir.JoinUtf8(iio::tests::kUnicodeName + ".iges");
    fs::copy_file(fs::path(kSingleRoundCubePath),
                  iio::utils::PathFromUtf8(path));

    const auto data = iio::ReadIgesIntermediate(path);
    EXPECT_EQ(data.directory_entry_section.size(), 102u);
    EXPECT_NO_THROW(iio::ReadIges(path));
}

// 存在しない全角名のファイルはFileOpenErrorで、文言にUTF-8のパスを含む
TEST(ReadIgesTest, UnicodePath_ThrowsFileOpenErrorWhenFileMissing) {
    const iio::tests::UnicodeTempDir dir("reader_missing");
    const std::string path = dir.JoinUtf8(iio::tests::kUnicodeName + ".iges");
    try {
        iio::ReadIges(path);
        FAIL() << "FileOpenError was not thrown";
    } catch (const iio::FileOpenError& e) {
        EXPECT_NE(std::string(e.what()).find(iio::tests::kUnicodeName),
                  std::string::npos) << e.what();
    }
}




/*******************************************************************************
 * 非ASCII文字を含むファイル (桁や文字列長を文字数で数えたファイル) の読込
 *****************************************************************************/

namespace {

/// @brief "年度"のUTF-8のバイト列
const std::string kNendoUtf8 = "\xE5\xB9\xB4\xE5\xBA\xA6";
/// @brief "年度"のShift_JISのバイト列
const std::string kNendoShiftJis = "\x94" "N" "\x93" "x";

/// @brief テスト用IGESファイルの書き方
struct NonAsciiWritingStyle {
    /// @brief 非ASCII文字のバイト列 ("年度")
    std::string non_ascii;
    /// @brief Shift_JISで書くか (falseの場合はUTF-8)
    bool is_shift_jis;
    /// @brief H文字列の長さを文字数で数えるか (falseの場合はバイト数)
    bool counts_hollerith_in_chars;
    /// @brief 行の桁を文字数で揃えるか (falseの場合はバイト数)
    bool pads_line_in_chars;
};

/// @brief 文字列の文字数を数える
/// @param str 対象文字列
/// @param is_shift_jis Shift_JISとして数えるか (falseの場合はUTF-8)
/// @return 文字数
std::size_t CountChars(const std::string& str, const bool is_shift_jis) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < str.size(); ++i) {
        const auto c = static_cast<unsigned char>(str[i]);
        if (is_shift_jis) {
            // 2バイト文字の先頭バイトであれば2バイト目を飛ばす
            if ((0x81 <= c && c <= 0x9F) || (0xE0 <= c && c <= 0xFC)) ++i;
            ++count;
        } else if ((c & 0xC0) != 0x80) {
            // UTF-8の継続バイト以外を数える
            ++count;
        }
    }
    return count;
}

/// @brief 書き方に応じて文字列の長さを求める
/// @param str 対象文字列
/// @param style 書き方
/// @param in_chars 文字数で数えるか (falseの場合はバイト数)
/// @return 長さ
std::size_t MeasureLength(const std::string& str,
                          const NonAsciiWritingStyle& style,
                          const bool in_chars) {
    return in_chars ? CountChars(str, style.is_shift_jis) : str.size();
}

/// @brief 書き方に応じてH文字列を作成する
/// @param content H文字列の内容
/// @param style 書き方
/// @return H文字列
std::string MakeHollerith(const std::string& content,
                          const NonAsciiWritingStyle& style) {
    return std::to_string(MeasureLength(
            content, style, style.counts_hollerith_in_chars)) + "H" + content;
}

/// @brief 書き方に応じてデータ部を空白で埋め、行末の固定欄を付けた行を作成する
/// @param data データ部
/// @param width データ部の桁数
/// @param tail 行末の固定欄
/// @param style 書き方
/// @return 改行文字を含まない行
std::string MakeFixedLine(const std::string& data, const std::size_t width,
                          const std::string& tail,
                          const NonAsciiWritingStyle& style) {
    const std::size_t length = MeasureLength(
            data, style, style.pads_line_in_chars);
    return data + std::string(width - length, ' ') + tail;
}

/// @brief 非ASCII文字を含むテスト用IGESファイルを作成する
/// @param path 出力先のパス (UTF-8)
/// @param style 書き方
/// @note スタートセクション、グローバルセクションの製品ID/ファイル名、
///       DEのエンティティラベル、Type 406のPDの文字列に非ASCII文字を含める
void WriteNonAsciiIges(const std::string& path,
                       const NonAsciiWritingStyle& style) {
    const auto& jp = style.non_ascii;
    const auto line = [&](const std::string& data, const std::size_t width,
                          const std::string& tail) {
        return MakeFixedLine(data, width, tail, style);
    };
    const auto h = [&](const std::string& content) {
        return MakeHollerith(content, style);
    };
    // エンティティラベル (8桁、右詰め)
    const std::string label = std::string(
            8 - MeasureLength(jp, style, style.pads_line_in_chars), ' ') + jp;

    const std::vector<std::string> lines = {
        line("Start " + jp + " section", 72, "S      1"),
        line("1H,,1H;," + h("work-" + jp) + "," +
             h("\\\\share\\2026" + jp + "\\a.igs") + ",", 72, "G      1"),
        line("15HExampleIgesFile,15HExampleIgesFile,32,308,15,308,15,",
             72, "G      2"),
        line("19Hsingle_rounded_cube,1.,2,2HMM,50,0.125,13H250408.163937,",
             72, "G      3"),
        line("1E-08,499990.,11HYayoiHabami,,11,0,13H250408.163937;",
             72, "G      4"),
        line("     110       1       0       0       0                "
             "        00000000", 72, "D      1"),
        line("     110       0       0       1       0                "
             "               0", 72, "D      2"),
        line("     406       2       0       0       0                "
             "        00010000", 72, "D      3"),
        // エンティティラベル (8桁) に非ASCII文字を含める
        line("     406       0       0       1      15                " +
             label + "       0", 72, "D      4"),
        line("110,0.,0.,0.,1.,1.,1.;", 64, "       1P      1"),
        line("406,1," + h("name-" + jp) + ";", 64, "       3P      2"),
        line("S      1G      4D      4P      2", 72, "T      1"),
    };

    std::ofstream ofs(iio::utils::PathFromUtf8(path), std::ios::binary);
    for (const auto& l : lines) ofs << l << "\r\n";
}

/// @brief 非ASCII文字を含むファイルの読込テストのパラメータ
struct NonAsciiReadCase {
    /// @brief 一時ディレクトリの識別子 (テスト名にも用いる)
    std::string tag;
    /// @brief 書き方
    NonAsciiWritingStyle style;
};

/// @brief 非ASCII文字を含むファイルの読込テストのフィクスチャ
class NonAsciiReadTest : public ::testing::TestWithParam<NonAsciiReadCase> {};

}  // namespace

// 非ASCII文字を含むファイルを読み込み、スタートセクション、グローバルセクションの文字列、
// DEのエンティティラベル、PDの文字列を非ASCII文字を含めて取得できる
TEST_P(NonAsciiReadTest, ReadIgesIntermediate_NonAsciiStrings) {
    const auto& param = GetParam();
    const iio::tests::UnicodeTempDir dir(param.tag);
    const std::string path = dir.JoinUtf8("non_ascii.iges");
    WriteNonAsciiIges(path, param.style);

    iio::models::IntermediateIgesData data;
    ASSERT_NO_THROW(data = iio::ReadIgesIntermediate(path));

    const auto& jp = param.style.non_ascii;
    EXPECT_EQ(data.start_section, "Start " + jp + " section");
    EXPECT_EQ(data.global_section.product_id, "work-" + jp);
    EXPECT_EQ(data.global_section.file_name,
              "\\\\share\\2026" + jp + "\\a.igs");

    ASSERT_EQ(data.directory_entry_section.size(), 2u);
    EXPECT_EQ(data.directory_entry_section[1].entity_label, jp);
    EXPECT_EQ(data.directory_entry_section[1].form_number, 15);
    EXPECT_EQ(data.directory_entry_section[1].entity_subscript_number, 0);

    ASSERT_EQ(data.parameter_data_section.size(), 2u);
    EXPECT_EQ(data.parameter_data_section[1].de_pointer, 3u);
    const auto params = iio::entities::ToIGESParameterVector(
            data.parameter_data_section[1]);
    ASSERT_EQ(params.size(), 2u);
    EXPECT_EQ(params.get<std::string>(1), "name-" + jp);
}

// 各パラメータの意味: {識別子, {非ASCII文字, Shift_JISか,
//                     H文字列の長さを文字数で数えるか, 桁を文字数で揃えるか}}
INSTANTIATE_TEST_SUITE_P(
    ReadIgesIntermediateTest, NonAsciiReadTest,
    ::testing::Values(
        // 規定通り (H文字列の長さと桁をバイト数で数えた) のファイル
        NonAsciiReadCase{"utf8_bytes", {kNendoUtf8, false, false, false}},
        NonAsciiReadCase{"sjis_bytes", {kNendoShiftJis, true, false, false}},
        // 桁を文字数で揃えた (80バイトを超える行を含む) ファイル
        NonAsciiReadCase{"utf8_pad", {kNendoUtf8, false, false, true}},
        NonAsciiReadCase{"sjis_pad", {kNendoShiftJis, true, false, true}},
        // H文字列の長さを文字数で数えたファイル
        NonAsciiReadCase{"utf8_count", {kNendoUtf8, false, true, false}},
        NonAsciiReadCase{"sjis_count", {kNendoShiftJis, true, true, false}},
        // 桁とH文字列の長さの両方を文字数で数えたファイル
        NonAsciiReadCase{"utf8_all", {kNendoUtf8, false, true, true}},
        NonAsciiReadCase{"sjis_all", {kNendoShiftJis, true, true, true}}),
    [](const ::testing::TestParamInfo<NonAsciiReadCase>& info) {
        return info.param.tag;
    });

// ASCII文字のみで80バイトを超える行は、これまで通りエラーとする
TEST(ReadIgesIntermediateTest,
     ReadIgesIntermediate_ThrowsLineFormatErrorWhenAsciiLineExceeds80Bytes) {
    const iio::tests::UnicodeTempDir dir("ascii_wide_line");
    const std::string path = dir.JoinUtf8("ascii_wide.iges");
    WriteNonAsciiIges(path, {"ab", false, false, false});

    // グローバルセクション1行目の固定欄の前に空白を2バイト追加する
    std::string content;
    {
        std::ifstream ifs(iio::utils::PathFromUtf8(path), std::ios::binary);
        content.assign(std::istreambuf_iterator<char>(ifs),
                       std::istreambuf_iterator<char>());
    }
    content.insert(content.find("G      1"), "  ");
    {
        std::ofstream ofs(iio::utils::PathFromUtf8(path), std::ios::binary);
        ofs << content;
    }

    EXPECT_THROW(iio::ReadIgesIntermediate(path), iio::LineFormatError);
}
