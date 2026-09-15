/**
 * @file tests/extensions/machines/toolpath/test_nc_dialect.cpp
 * @brief 制御装置の方言 (toolpath/nc_dialect) のテスト
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note 対象: DefaultFanucDialect / NormalizeCodeName / ExpandTemplate /
 *       WorkOffsetIdFromWord / WorkOffsetWordFromId
 *       - 正常系 (代表値): デフォルトのコード名/構文/アドレス表、コード名の表記揺れ
 *         (`G68.2`/`g68.20`/`G068.2`、`M6`/`M06`)、テンプレートの変数展開,
 *         ワーク座標系の選択コードとidの相互変換 (対応表あり/なし)
 *       - 正常系 (境界値): 空文字列、数値でないコード名、閉じられていない`{`,
 *         `G54.1`でPが無い場合、`P0`は推奨表記ではない
 *       - 警告: 未知のテンプレート変数 (空文字に展開)
 *       TODO: 例外を送出するAPIは無い
 */
#include <gtest/gtest.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/toolpath/nc_block.h"
#include "igesio/extensions/machines/toolpath/nc_dialect.h"

namespace {

namespace mc = igesio::extensions::machines;

/// @brief 対応表を持つ方言 (`G54` → `"table"`、`G54.1P2` → `"fixture-2"`)
mc::NcDialect DialectWithWorkOffsetIds() {
    mc::NcDialect dialect = mc::DefaultFanucDialect();
    dialect.work_offset_ids = {{"G54", "table"}, {"G54.1P2", "fixture-2"}};
    return dialect;
}

}  // namespace



// ---- DefaultFanucDialect ----

TEST(NcDialectTest, DefaultFanuc_Vocabulary) {
    const mc::NcDialect dialect = mc::DefaultFanucDialect();
    EXPECT_EQ(dialect.name, "fanuc");
    EXPECT_EQ(dialect.vocab.rapid, "G00");
    EXPECT_EQ(dialect.vocab.tcp_vector, "G43.5");
    EXPECT_EQ(dialect.vocab.program_end, "M30");
    EXPECT_EQ(dialect.vocab.dwell_address, 'P');
    EXPECT_DOUBLE_EQ(dialect.vocab.dwell_scale, 1000.0);
}

TEST(NcDialectTest, DefaultFanuc_Syntax) {
    const mc::NcDialect dialect = mc::DefaultFanucDialect();
    EXPECT_TRUE(dialect.syntax.percent);
    EXPECT_EQ(dialect.syntax.program_prefix, "O");
    EXPECT_EQ(dialect.syntax.program_digits, 4);
    EXPECT_FALSE(dialect.syntax.line_number_step.has_value());
    EXPECT_TRUE(dialect.syntax.word_separator.empty());
    EXPECT_EQ(dialect.syntax.coordinate.decimals, 4);
    EXPECT_EQ(dialect.syntax.vector.decimals, 6);
    EXPECT_EQ(dialect.syntax.angle.decimals, 3);
    // Fコードは整数で小数点を付けない
    EXPECT_EQ(dialect.syntax.feed.decimals, 0);
    EXPECT_FALSE(dialect.syntax.feed.trailing_zeros);
    EXPECT_FALSE(dialect.syntax.feed.force_decimal_point);
    EXPECT_EQ(mc::FormatNcWord('F', 200.0, dialect.syntax.feed), "F200");
}

TEST(NcDialectTest, DefaultFanuc_AddressTablesAndDefaults) {
    const mc::NcDialect dialect = mc::DefaultFanucDialect();
    EXPECT_EQ(dialect.rotary_address_to_register.at('A'), "A");
    EXPECT_EQ(dialect.rotary_address_to_register.at('C'), "C");
    EXPECT_EQ(dialect.linear_address_to_register.at('X'), "X");
    EXPECT_TRUE(dialect.disabled_codes.empty());
    EXPECT_TRUE(dialect.work_offset_ids.empty());
    EXPECT_TRUE(dialect.defaults.absolute);
    EXPECT_EQ(dialect.defaults.plane, mc::ArcPlane::kXY);
    EXPECT_TRUE(dialect.defaults.metric);
    EXPECT_TRUE(dialect.defaults.feed_per_minute);
    EXPECT_FALSE(dialect.reset_modal_at_program_start);
    EXPECT_TRUE(dialect.header.empty());
    EXPECT_TRUE(dialect.footer.empty());
    EXPECT_TRUE(dialect.tool_change.empty());
}

// ---- NormalizeCodeName ----

TEST(NcDialectTest, NormalizeCodeName_FractionalVariants) {
    EXPECT_EQ(mc::NormalizeCodeName("G68.2"), "G68.2");
    EXPECT_EQ(mc::NormalizeCodeName("g68.20"), "G68.2");
    EXPECT_EQ(mc::NormalizeCodeName("G068.2"), "G68.2");
    EXPECT_EQ(mc::NormalizeCodeName(" G68.2 "), "G68.2");
}

TEST(NcDialectTest, NormalizeCodeName_IntegerVariants) {
    EXPECT_EQ(mc::NormalizeCodeName("M6"), "M6");
    EXPECT_EQ(mc::NormalizeCodeName("M06"), "M6");
    EXPECT_EQ(mc::NormalizeCodeName("m006.0"), "M6");
    EXPECT_EQ(mc::NormalizeCodeName("G54.1"), "G54.1");
    EXPECT_EQ(mc::NormalizeCodeName("G0"), "G0");
    EXPECT_EQ(mc::NormalizeCodeName("G00"), "G0");
}

TEST(NcDialectTest, NormalizeCodeName_NonNumericIsUppercasedOnly) {
    EXPECT_EQ(mc::NormalizeCodeName("abc"), "ABC");
    EXPECT_EQ(mc::NormalizeCodeName("G"), "G");
    EXPECT_EQ(mc::NormalizeCodeName(""), "");
    EXPECT_EQ(mc::NormalizeCodeName("12"), "12");
}

// ---- ExpandTemplate ----

TEST(NcDialectTest, Template_ExpandsKnownVariables) {
    const std::map<std::string, std::string> variables = {
            {"tool", "12"}, {"h", "12"}, {"program", "0022"}};
    std::vector<mc::Diagnostic> warnings;
    EXPECT_EQ(mc::ExpandTemplate("T{tool}M06", variables, &warnings), "T12M06");
    EXPECT_EQ(mc::ExpandTemplate("H{h}D{h}", variables, &warnings), "H12D12");
    EXPECT_EQ(mc::ExpandTemplate("O{program}", variables, &warnings), "O0022");
    EXPECT_EQ(mc::ExpandTemplate("G91G28Z0", variables, &warnings), "G91G28Z0");
    EXPECT_TRUE(warnings.empty());
}

TEST(NcDialectTest, Template_UnknownVariableIsEmptyAndWarns) {
    const std::map<std::string, std::string> variables = {{"tool", "1"}};
    std::vector<mc::Diagnostic> warnings;
    EXPECT_EQ(mc::ExpandTemplate("S{spindle}M03 T{tool}", variables, &warnings),
              "SM03 T1");
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_NE(warnings[0].message.find("spindle"), std::string::npos);
    EXPECT_EQ(warnings[0].line, 0);
    EXPECT_TRUE(warnings[0].context.empty());
}

TEST(NcDialectTest, Template_UnclosedBraceIsKeptAndNullWarningsAllowed) {
    const std::map<std::string, std::string> variables;
    EXPECT_EQ(mc::ExpandTemplate("X{abc", variables, nullptr), "X{abc");
    EXPECT_EQ(mc::ExpandTemplate("", variables, nullptr), "");
    EXPECT_EQ(mc::ExpandTemplate("{x}", variables, nullptr), "");
}

// ---- WorkOffsetIdFromWord / WorkOffsetWordFromId ----

TEST(NcDialectTest, WorkOffsetIds_EmptyTableUsesStandardNotation) {
    const mc::NcDialect dialect = mc::DefaultFanucDialect();
    EXPECT_EQ(mc::WorkOffsetIdFromWord(dialect, "G54", std::nullopt), "G54");
    EXPECT_EQ(mc::WorkOffsetIdFromWord(dialect, "G59", std::nullopt), "G59");
    EXPECT_EQ(mc::WorkOffsetIdFromWord(dialect, "G54.1", 2), "G54.1P2");
    EXPECT_EQ(mc::WorkOffsetIdFromWord(dialect, "G54.1", std::nullopt), "G54.1");
}

TEST(NcDialectTest, WorkOffsetIds_TableMapsWordToId) {
    const mc::NcDialect dialect = DialectWithWorkOffsetIds();
    EXPECT_EQ(mc::WorkOffsetIdFromWord(dialect, "G54", std::nullopt), "table");
    EXPECT_EQ(mc::WorkOffsetIdFromWord(dialect, "G54.1", 2), "fixture-2");
    // 表に無いコードは標準の形式のまま
    EXPECT_EQ(mc::WorkOffsetIdFromWord(dialect, "G55", std::nullopt), "G55");
}

TEST(NcDialectTest, WorkOffsetIds_ReverseLookupAndStandardIds) {
    const mc::NcDialect dialect = DialectWithWorkOffsetIds();
    EXPECT_EQ(mc::WorkOffsetWordFromId(dialect, "table"), "G54");
    EXPECT_EQ(mc::WorkOffsetWordFromId(dialect, "fixture-2"), "G54.1P2");
    EXPECT_EQ(mc::WorkOffsetWordFromId(dialect, "G55"), "G55");
    EXPECT_EQ(mc::WorkOffsetWordFromId(dialect, "G54.1P10"), "G54.1P10");
}

TEST(NcDialectTest, WorkOffsetIds_UnconvertibleIdIsNullopt) {
    const mc::NcDialect dialect = mc::DefaultFanucDialect();
    EXPECT_FALSE(mc::WorkOffsetWordFromId(dialect, "fixture").has_value());
    EXPECT_FALSE(mc::WorkOffsetWordFromId(dialect, "G53").has_value());
    EXPECT_FALSE(mc::WorkOffsetWordFromId(dialect, "G60").has_value());
    EXPECT_FALSE(mc::WorkOffsetWordFromId(dialect, "G54.1P0").has_value());
    EXPECT_FALSE(mc::WorkOffsetWordFromId(dialect, "G54.1P").has_value());
    EXPECT_FALSE(mc::WorkOffsetWordFromId(dialect, "").has_value());
}
