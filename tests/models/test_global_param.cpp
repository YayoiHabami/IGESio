/**
 * @file models/test_global_param.cpp
 * @brief models/global_param.hのテスト
 * @author Yayoi Habami
 * @date 2025-04-21
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <string>

#include "igesio/utils/iges_string_utils.h"
#include "igesio/models/global_param.h"

namespace {

namespace i_util = igesio::utils;
namespace i_model = igesio::models;

}  // namespace



// 正常系
TEST(SetGlobalSectionParamsTest, NormalCase) {
    // テストデータ
    char p_delim = ',', r_delim = ';';
    std::vector<std::string> lines = {
        R"(19Hsingle_rounded_cube,49HThis\is\the\path\to\iges\single_rounde)",
        R"(d_cube.iges,15HExampleIgesFile,15HExampleIgesFile,32,308,15,308,15,19Hsi)",
        R"(ngle_rounded_cube,1.,2,2HMM,50,0.125,13H250408.163937,1E-08,499990.,11HY)",
        R"(ayoiHabami,,11,0,13H250408.163937;                                      )"
    };

    auto prm = i_util::ParseFreeFormattedData(lines, p_delim, r_delim);
    auto gp = i_model::SetGlobalSectionParams(p_delim, r_delim, prm);

    // パラメータの確認
    EXPECT_EQ(gp.param_delim, p_delim);
    EXPECT_EQ(gp.record_delim, r_delim);
    EXPECT_EQ(gp.product_id, "single_rounded_cube");
    EXPECT_EQ(gp.file_name, R"(This\is\the\path\to\iges\single_rounded_cube.iges)");
    EXPECT_EQ(gp.native_system_id, "ExampleIgesFile");
    EXPECT_EQ(gp.preprocessor_version, "ExampleIgesFile");
    EXPECT_EQ(gp.integer_bits, 32);
    EXPECT_EQ(gp.single_precision_power_max, 38);  // C++の仕様を越える値は指定されない
    EXPECT_EQ(gp.single_precision_digits, 6);  // C++の仕様を越える値は指定されない
    EXPECT_EQ(gp.double_precision_power_max, 308);
    EXPECT_EQ(gp.double_precision_digits, 15);
    EXPECT_EQ(gp.receiving_system_id, "single_rounded_cube");
    EXPECT_EQ(gp.model_space_scale, 1.0);
    EXPECT_EQ(gp.units_flag, i_model::UnitFlag::kMillimeter);
    EXPECT_EQ(gp.line_weight_gradations, 50);
    EXPECT_EQ(gp.max_line_weight, 0.125);
    EXPECT_EQ(gp.date_time_generation, "250408.163937");
    EXPECT_EQ(gp.min_resolution, 1E-08);
    EXPECT_EQ(gp.max_coordinate, 499990.0);
    EXPECT_EQ(gp.author_name, "YayoiHabami");
    EXPECT_EQ(gp.author_organization, "");  // デフォルト値は空文字列
    EXPECT_EQ(gp.specification_version, i_model::VersionFlag::kVersion5_3);
    EXPECT_EQ(gp.drafting_standard_flag, i_model::DraftingStandardFlag::kNone);
    EXPECT_EQ(gp.date_time_modified, "250408.163937");
    EXPECT_EQ(gp.protocol_identifier, "");  // デフォルト値は空文字列
}
