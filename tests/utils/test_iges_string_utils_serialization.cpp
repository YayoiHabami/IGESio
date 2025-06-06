/**
 * @file utils/test_iges_string_utils_serialization.cpp
 * @brief utils/iges_string_utils.hのうち、IGES文字列への変換のテスト
 * @author Yayoi Habami
 * @date 2025-05-31
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "igesio/common/iges_metadata.h"
#include "igesio/utils/iges_string_utils.h"

namespace {

using PT = igesio::IGESParameterType;
constexpr auto PTS = PT::kString;
constexpr auto PTI = PT::kInteger;
constexpr auto PTR = PT::kReal;
namespace i_util = igesio::utils;

}  // namespace



/******************************************************************************
 * 自由形式への変換 (ToFreeFormattedLines) のテスト
 *****************************************************************************/

// 基本的なテストケース (グローバルセクションそのまま)
TEST(ToFreeFormattedLines, Basic) {
    std::vector<std::string> parameters = {
        "1H,", "1H;", "19Hsingle_rounded_cube",
        "49HThis\\is\\the\\path\\to\\iges\\single_rounded_cube.iges",
        "15HExampleIgesFile", "15HExampleIgesFile", "32", "308", "15", "308", "15",
        "19Hsingle_rounded_cube", "1.", "2", "2HMM", "50", "0.125", "13H250408.163937",
        "1E-08", "499990.", "11HYayoiHabami", "", "11,0,13H250408.163937"};
    std::vector<PT> parameter_types = {
        PTS, PTS, PTS, PTS, PTS, PTS, PTI, PTI, PTI, PTI, PTI, PTS, PTR, PTI,
        PTS, PTI, PTR, PTS, PTR, PTR, PTS, PTS, PTS};
    unsigned int max_line_length = 72;
    char p_delim = ',', r_delim = ';';

    std::vector<std::string> expected = {
        R"(1H,,1H;,19Hsingle_rounded_cube,49HThis\is\the\path\to\iges\single_rounde)",
        R"(d_cube.iges,15HExampleIgesFile,15HExampleIgesFile,32,308,15,308,15,19Hsi)",
        R"(ngle_rounded_cube,1.,2,2HMM,50,0.125,13H250408.163937,1E-08,499990.,11HY)",
        R"(ayoiHabami,,11,0,13H250408.163937;                                      )"};
    auto result = i_util::ToFreeFormattedLines(
        parameters, parameter_types, max_line_length, p_delim, r_delim);

    ASSERT_EQ(result.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(result[i], expected[i]) << "Mismatch at line " << i + 1;
    }
}
