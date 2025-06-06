/**
 * @file test_writer.cpp
 * @brief writer.hのテスト
 * @author Yayoi Habami
 * @date 2025-05-31
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "igesio/common/errors.h"
#include "igesio/reader.h"
#include "igesio/writer.h"

namespace {

namespace iio = igesio;
namespace fs = std::filesystem;

/// @brief テスト用IGESファイルを格納するディレクトリ
const std::string kTestIgesDirPath =
        fs::path(__FILE__).parent_path().append("test_data").string();
/// @brief テスト用IGESファイルのパス (立方体の一辺が丸められたもの)
const std::string kSingleRoundCubePath =
        fs::path(kTestIgesDirPath).append("single_rounded_cube.iges").string();
/// @brief テストによる出力フォルダ
const std::string kOutputDirPath =
        fs::path(kTestIgesDirPath).append("output").string();

}  // namespace


/*******************************************************************************
 * WriteIgesIntermediateのテスト
 ******************************************************************************/

TEST(WriteIgesIntermediateTest, NormalCase) {
    // まずは読み込み
    auto data = iio::ReadIgesIntermediate(kSingleRoundCubePath);

    // 書き込み先のファイルパス
    const std::string output_path =
            fs::path(kOutputDirPath).append("single_rounded_cube_copied.iges").string();

    // 書き込みを実行
    ASSERT_NO_THROW(iio::WriteIgesIntermediate(data, output_path));

    // 書き込んだファイルが存在することを確認
    ASSERT_TRUE(fs::exists(output_path));
}
