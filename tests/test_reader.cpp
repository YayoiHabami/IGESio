/**
 * @file test_reader.cpp
 * @brief reader.hのテスト
 * @author Yayoi Habami
 * @date 2025-04-24
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "igesio/common/errors.h"
#include "igesio/reader.h"

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
