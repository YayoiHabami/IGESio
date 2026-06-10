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
#include "igesio/entities/de/raw_entity_de.h"
#include "igesio/entities/factory.h"
#include "igesio/models/iges_data.h"
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



/*******************************************************************************
 * ユーザー定義エンティティ (kUserDefined) の往復テスト
 ******************************************************************************/

// ユーザー定義番号 (602) のエンティティがWriteIges→ReadIgesで往復すること.
// creator未登録のためUnsupportedEntityとして保持され、ファイルへは
// 実番号 (602) が出力される
TEST(WriteIgesTest, UserDefinedEntityRoundTrip) {
    iio::models::IgesData data;
    auto entity = iio::entities::EntityFactory::CreateEntity(
            iio::entities::RawEntityDE::ByDefaultUserDefined(602),
            iio::IGESParameterVector{1.0, 2.0, 3.0}, {});
    data.Root().AddEntity(entity);

    const std::string output_path =
            fs::path(kOutputDirPath).append("user_defined_roundtrip.iges").string();
    // UnsupportedEntityを含むため、save_unsupported = true で書き出す
    ASSERT_NO_THROW(iio::WriteIges(data, output_path, true));
    ASSERT_TRUE(fs::exists(output_path));

    // 読み戻して実番号が保持されていることを確認
    auto read = iio::ReadIges(output_path);
    const auto& entities = read.Root().GetEntities();
    ASSERT_EQ(entities.size(), 1u);
    const auto& read_entity = entities.begin()->second;
    EXPECT_EQ(read_entity->GetType(), iio::entities::EntityType::kUserDefined);
    EXPECT_EQ(read_entity->GetTypeNumber(), 602);
    EXPECT_FALSE(read_entity->IsSupported());
}
