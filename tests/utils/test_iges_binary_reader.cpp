/**
 * @file utils/test_iges_binary_reader.cpp
 * @brief utils/iges_binary_reader.hの`IgesBinaryReader`クラスのテスト
 * @author Yayoi Habami
 * @date 2025-04-19
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <filesystem>
#include <map>
#include <string>
#include <optional>

#include "igesio/common/errors.h"
#include "igesio/utils/iges_binary_reader.h"

namespace {

// エイリアスを使用して簡潔に記述
namespace u_detail = igesio::utils::detail;
using SType = igesio::SectionType;
namespace fs = std::filesystem;

/// @brief テスト用IGESファイルを格納するディレクトリ
const std::string kTestIgesDirPath =
        fs::path(__FILE__).parent_path().parent_path().append("test_data").string();
/// @brief テスト用IGESファイルのパス (立方体の一辺が丸められたもの)
const std::string kSingleRoundCubePath =
        fs::path(kTestIgesDirPath).append("single_rounded_cube.iges").string();
/// @brief `first_line_82_cols.iges`: 1行目のみ、改行を除き82文字
const std::string kFirstLine82ColsPath =
        fs::path(kTestIgesDirPath).append("first_line_82_cols.iges").string();
/// @brief `first_line_without_80_cols.iges`: 1行目の文字数が改行を含めて80文字
/// @note 73文字以上80文字未満の行は存在しない
const std::string kFirstLineWithout80ColsPath =
        fs::path(kTestIgesDirPath).append("first_line_without_80_cols.iges").string();
/// @brief `up_to_line_7_of_DE_section.iges`: ディレクトリエントリ部の7行目まで
const std::string kUpToLine7OfDESectionPath =
        fs::path(kTestIgesDirPath).append("up_to_line_7_of_DE_section.iges").string();

}  // namespace



/******************************************************************************
 * セクションの順序を検証する (IsValidSectionOrder) のテスト
 *****************************************************************************/

// IsValidSectionOrderのテスト
class IsValidSectionOrderTest : public ::testing::Test {
 protected:
    // よく使われるセクションタイプをメンバ変数として定義
    const SType flag = SType::kFlag;
    const SType start = SType::kStart;
    const SType global = SType::kGlobal;
    const SType directory = SType::kDirectory;
    const SType parameter = SType::kParameter;
    const SType terminate = SType::kTerminate;
    const SType data = SType::kData;
};

// 初回呼び出し（初めてのセクション）のテスト
TEST_F(IsValidSectionOrderTest, FirstSection) {
    // 通常形式では最初はStartセクション
    EXPECT_TRUE(u_detail::IsValidSectionOrder(start, 1, std::nullopt, 0));

    // 圧縮形式では最初はFlagセクション
    EXPECT_TRUE(u_detail::IsValidSectionOrder(flag, 1, std::nullopt, 0));

    // その他のセクションで始まる場合は不正
    EXPECT_FALSE(u_detail::IsValidSectionOrder(global, 1, std::nullopt, 0));
    EXPECT_FALSE(u_detail::IsValidSectionOrder(directory, 1, std::nullopt, 0));
    EXPECT_FALSE(u_detail::IsValidSectionOrder(parameter, 1, std::nullopt, 0));
    EXPECT_FALSE(u_detail::IsValidSectionOrder(terminate, 1, std::nullopt, 0));
    EXPECT_FALSE(u_detail::IsValidSectionOrder(data, 1, std::nullopt, 0));
}

// 通常形式のセクション遷移テスト
TEST_F(IsValidSectionOrderTest, NormalFormatTransitions) {
    // 正しい遷移
    EXPECT_TRUE(u_detail::IsValidSectionOrder(start, 2, start, 1));
    EXPECT_TRUE(u_detail::IsValidSectionOrder(global, 1, start, 10));
    EXPECT_TRUE(u_detail::IsValidSectionOrder(directory, 1, global, 5));
    EXPECT_TRUE(u_detail::IsValidSectionOrder(parameter, 1, directory, 20));
    EXPECT_TRUE(u_detail::IsValidSectionOrder(terminate, 1, parameter, 30));

    // 不正な遷移
    EXPECT_FALSE(u_detail::IsValidSectionOrder(flag, 1, start, 5));  // 通常形式でFlag
    EXPECT_FALSE(u_detail::IsValidSectionOrder(global, 1, directory, 10));  // 逆順
    EXPECT_FALSE(u_detail::IsValidSectionOrder(start, 1, global, 5));  // 逆順
}

// 圧縮形式のセクション遷移テスト
TEST_F(IsValidSectionOrderTest, CompressedFormatTransitions) {
    // 正しい遷移
    EXPECT_TRUE(u_detail::IsValidSectionOrder(start, 1, flag, 1));
    EXPECT_TRUE(u_detail::IsValidSectionOrder(global, 1, start, 10));
    EXPECT_TRUE(u_detail::IsValidSectionOrder(data, 0, global, 5));
    EXPECT_TRUE(u_detail::IsValidSectionOrder(data, 0, data, 0));  // データ部のみ、常に0
    EXPECT_TRUE(u_detail::IsValidSectionOrder(terminate, 1, data, 0));

    // 不正な遷移
    EXPECT_FALSE(u_detail::IsValidSectionOrder(parameter, 1, data, 10));  // 圧縮形式でParameter
    EXPECT_FALSE(u_detail::IsValidSectionOrder(flag, 1, start, 5));  // 逆順
}

// 同一セクション内のシーケンス番号の連続性テスト
TEST_F(IsValidSectionOrderTest, SequenceNumberContinuity) {
    // 正しい連番
    EXPECT_TRUE(u_detail::IsValidSectionOrder(start, 1, std::nullopt, 0));
    EXPECT_TRUE(u_detail::IsValidSectionOrder(start, 2, start, 1));
    EXPECT_TRUE(u_detail::IsValidSectionOrder(start, 3, start, 2));

    // 番号が飛ぶ
    EXPECT_FALSE(u_detail::IsValidSectionOrder(start, 3, start, 1));

    // 番号が逆転
    EXPECT_FALSE(u_detail::IsValidSectionOrder(start, 1, start, 2));
    EXPECT_FALSE(u_detail::IsValidSectionOrder(global, 2, global, 3));
}

// シーケンス番号が1から始まることの確認
TEST_F(IsValidSectionOrderTest, SequenceNumberStartsFromOne) {
    // 初めてのセクションで番号が1以外
    EXPECT_FALSE(u_detail::IsValidSectionOrder(start, 0, std::nullopt, 0));
    EXPECT_FALSE(u_detail::IsValidSectionOrder(start, 2, std::nullopt, 0));
    EXPECT_FALSE(u_detail::IsValidSectionOrder(flag, 0, std::nullopt, 0));
    EXPECT_FALSE(u_detail::IsValidSectionOrder(flag, 2, std::nullopt, 0));

    // セクション遷移時に次のセクションの番号が1以外
    EXPECT_FALSE(u_detail::IsValidSectionOrder(global, 2, start, 10));
    EXPECT_FALSE(u_detail::IsValidSectionOrder(global, 0, start, 10));
}

// セクション遷移時にシーケンス番号がリセットされることの確認
TEST_F(IsValidSectionOrderTest, SequenceNumberResetsOnSectionChange) {
    // 正しい遷移（セクション変更時に番号が1に戻る）
    EXPECT_TRUE(u_detail::IsValidSectionOrder(global, 1, start, 10));

    // 不正な遷移（セクション変更時に番号が1以外）
    EXPECT_FALSE(u_detail::IsValidSectionOrder(global, 2, start, 10));
}

// エッジケースのテスト
TEST_F(IsValidSectionOrderTest, EdgeCases) {
    // 非常に大きなシーケンス番号でも正しく処理される
    EXPECT_TRUE(u_detail::IsValidSectionOrder(start, 999999, start, 999998));

    // 同じセクション間では連続した番号である必要がある
    EXPECT_FALSE(u_detail::IsValidSectionOrder(start, 999999, start, 999997));
}



/******************************************************************************
 * IgesBinaryReaderクラスのテスト --- コンストラクタ
 *****************************************************************************/

// コンストラクタのテスト
TEST(IgesBinaryReaderTest, Constructor) {
    // 存在するファイルの場合、正常に開けることを確認
    ASSERT_TRUE(fs::exists(kSingleRoundCubePath))
            << "Test file " + kSingleRoundCubePath + " does not exist.";
    ASSERT_NO_THROW(igesio::utils::IgesBinaryReader i(kSingleRoundCubePath))
            << "Failed to open test file: " + kSingleRoundCubePath;
}

// コンストラクタのエラーケース
TEST(IgesBinaryReaderTest, ConstructorError) {
    // 存在しないファイルの場合、`igesio::FileOpenError`がスローされることを確認
    std::string non_existent_file_path
            = fs::path(kTestIgesDirPath).append("non_existent_file.iges").string();
    ASSERT_FALSE(fs::exists(non_existent_file_path))
            << "Test file " + non_existent_file_path + " exists.";
    ASSERT_THROW(igesio::utils::IgesBinaryReader i(non_existent_file_path),
                 igesio::FileOpenError)
            << "Expected igesio::FileOpenError, but no exception was thrown.";

    // 1行目の長さが80文字以上の場合、`igesio::LineFormatError`がスローされることを確認
    ASSERT_TRUE(fs::exists(kFirstLine82ColsPath))
            << "Test file " + kFirstLine82ColsPath + " does not exist.";
    ASSERT_THROW(igesio::utils::IgesBinaryReader i(kFirstLine82ColsPath),
                 igesio::LineFormatError)
            << "Expected igesio::LineFormatError, but no exception was thrown.";

    // 1行目の長さが80文字未満の場合、`igesio::LineFormatError`がスローされることを確認
    ASSERT_TRUE(fs::exists(kFirstLineWithout80ColsPath))
            << "Test file " + kFirstLineWithout80ColsPath + " does not exist.";
    ASSERT_THROW(igesio::utils::IgesBinaryReader i(kFirstLineWithout80ColsPath),
                 igesio::LineFormatError)
            << "Expected igesio::LineFormatError, but no exception was thrown.";
}



/******************************************************************************
 * IgesBinaryReaderクラスのテスト --- GetLine (行の取得)
 *****************************************************************************/

namespace {

/// @brief 行をカウント
/// @throw std::runtime_error 行の取得中にエラーが発生した場合
std::map<SType, int> GetLineCountMap(const std::string& file_path) {
    // IgesBinaryReaderを作成
    igesio::utils::IgesBinaryReader reader(file_path);

    std::map<igesio::SectionType, int> section_count = {
        {igesio::SectionType::kFlag, 0},
        {igesio::SectionType::kStart, 0},
        {igesio::SectionType::kGlobal, 0},
        {igesio::SectionType::kDirectory, 0},
        {igesio::SectionType::kParameter, 0},
        {igesio::SectionType::kTerminate, 0},
        {igesio::SectionType::kData, 0}
    };

    SType prev_sec_type = SType::kTerminate;
    unsigned int prev_seq_num = 0;
    int line_count = 0;
    try {
        while (true) {
            // 1行取得
            ++line_count;
            auto [line, section_type, sequence_number] = reader.GetLine();
            // ファイルの終端に達したら終了
            if (line == "" && section_type == SType::kTerminate) break;

            // セクションの種類をカウント
            section_count[section_type]++;
            // シーケンス番号の順序を確認
            if (prev_sec_type == section_type) {
                EXPECT_EQ(sequence_number, prev_seq_num + 1) << "Invalid sequence number: "
                    << "Expected " << prev_seq_num + 1 << ", but got " << sequence_number;
            } else {
                EXPECT_EQ(sequence_number, 1) << "Invalid sequence number: "
                    << "Expected 1, but got " << sequence_number;
            }

            prev_sec_type = section_type;
            prev_seq_num = sequence_number;
        }
    } catch (const igesio::IGESioError& e) {
        throw std::runtime_error(
            "Error while reading line " + std::to_string(line_count) + ": " + e.what());
    }

    return section_count;
}

}  // namespace

// GetLineの基本的なテスト
// `single_round_cube.iges`:
//     Start, Global, Directory, Parameter, Terminateがそれぞれ1, 4, 204, 185, 1行、
//     圧縮形式ではないためそれ以外は0行
TEST(IgesBinaryReaderTest, GetLine) {
    // テスト用ファイルが存在することを確認
    ASSERT_TRUE(fs::exists(kSingleRoundCubePath))
            << "Test file " + kSingleRoundCubePath + " does not exist.";

    // 行のマッピング
    std::map<SType, int> section_count;
    try {
        section_count = GetLineCountMap(kSingleRoundCubePath);
    } catch (const std::runtime_error& e) {
        FAIL() << e.what();
    }

    // セクションの行数を確認
    EXPECT_EQ(section_count[SType::kFlag], 0);
    EXPECT_EQ(section_count[SType::kStart], 1);
    EXPECT_EQ(section_count[SType::kGlobal], 4);
    EXPECT_EQ(section_count[SType::kDirectory], 204);
    EXPECT_EQ(section_count[SType::kParameter], 185);
    EXPECT_EQ(section_count[SType::kTerminate], 1);
    EXPECT_EQ(section_count[SType::kData], 0);
}

// ファイルの途中でEOFに達する場合のGetLineのテスト
TEST(IgesBinaryReaderTest, GetLineEOF) {
    // テスト用ファイルが存在することを確認
    ASSERT_TRUE(fs::exists(kUpToLine7OfDESectionPath))
            << "Test file " + kUpToLine7OfDESectionPath + " does not exist.";

    // Igesを読み込む
    igesio::utils::IgesBinaryReader reader(kUpToLine7OfDESectionPath);

    // 11行目まで読み込む間、エラーを投げないことを確認
    for (int i = 0; i < 11; ++i) {
        EXPECT_NO_THROW(reader.GetLine())
                << "Error while reading line " << i + 1 << " of " << kUpToLine7OfDESectionPath;
    }
    // 12行目を読み込む際に、エラーが投げられることを確認
    EXPECT_THROW(reader.GetLine(), igesio::LineFormatError)
            << "Expected igesio::LineFormatError while reading line 12 of "
            << kUpToLine7OfDESectionPath;
    // 12行目を読み込んだ後、EOFに達していることを確認
    EXPECT_TRUE(reader.IsEndOfFile())
            << "Expected EOF after reading line 12 of " << kUpToLine7OfDESectionPath;
}



/******************************************************************************
 * IgesBinaryReaderクラスのテスト --- IsEndOfFile (EOFの確認)
 *****************************************************************************/

// IsEndOfFileのテスト
// `single_round_cube.iges`は395回のGetLineを呼び出すとEOFに達する
TEST(IgesBinaryReaderTest, IsEndOfFile) {
    // テスト用ファイルが存在することを確認
    ASSERT_TRUE(fs::exists(kSingleRoundCubePath))
            << "Test file " + kSingleRoundCubePath + " does not exist.";

    // IgesBinaryReaderを作成
    igesio::utils::IgesBinaryReader reader(kSingleRoundCubePath);

    // 395回のGetLineを呼び出す
    for (int i = 0; i < 395; ++i) {
        auto [line, section_type, sequence_number] = reader.GetLine();
        if (i == 394) {
            // 最後の行はEOFに達していることを確認
            EXPECT_TRUE(reader.IsEndOfFile()) << "Expected EOF at line " << i + 1;
        } else {
            // EOFに達していないことを確認
            EXPECT_FALSE(reader.IsEndOfFile()) << "Unexpected EOF at line " << i + 1;
        }
    }
}
