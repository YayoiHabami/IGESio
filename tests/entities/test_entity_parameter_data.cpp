/**
 * @file entities/test_entity_parameter_data.cpp
 * @brief entities/pd.hのテスト
 * @author Yayoi Habami
 * @date 2025-04-23
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/common/iges_metadata.h"
#include "igesio/common/serialization.h"
#include "igesio/entities/pd.h"

namespace {

namespace i_ent = igesio::entities;

}  // namespace



/******************************************************************************
 * ToRawEntityPDのテスト
 *****************************************************************************/

namespace {

/// @brief RawEntityPDをチェックする
/// @param epd RawEntityPD構造体
/// @param type エンティティタイプ
/// @param pointer DEポインタ
/// @param number シーケンス番号
/// @param data エンティティデータ
void CheckEntityParam(
        const i_ent::RawEntityPD& epd,
        const i_ent::EntityType& type, unsigned int pointer,
        unsigned int number, const std::vector<std::string>& data) {
    EXPECT_EQ(epd.type, type) << "EntityType mismatch"
            << " (expected: " << static_cast<int>(type) << ", actual: "
            << static_cast<int>(epd.type) << ")";
    EXPECT_EQ(epd.de_pointer, pointer) << "DE pointer mismatch";
    EXPECT_EQ(epd.sequence_number, number) << "Sequence number mismatch";
    EXPECT_EQ(epd.data.size(), data.size()) << "Data size mismatch";
    for (size_t i = 0; i < data.size(); ++i) {
        EXPECT_EQ(epd.data[i], data[i]) << "Data mismatch at index " << i;
    }
}

}  // namespace

// 正常系
TEST(ToRawEntityPDTest, NormalCase) {
    char p_delim = ',', r_delim = ';';

    // 1行のみで完結する場合
    std::vector<std::string> lines = {
        "110,-25.,25.,10.,-25.,25.,-25.;                                        7P      6"
    };
    auto epd = i_ent::ToRawEntityPD(lines, p_delim, r_delim);
    CheckEntityParam(epd, i_ent::EntityType::kLine, 7, 6,
                     {"-25.", "25.", "10.", "-25.", "25.", "-25."});

    // 1行のみ、文字列あり
    lines = {
        "308,0,13HSubfigureName,1,31;                                          29P     19"
    };
    epd = i_ent::ToRawEntityPD(lines, p_delim, r_delim);
    CheckEntityParam(epd, i_ent::EntityType::kSubfigureDefinition, 29, 19,
                     {"0", "13HSubfigureName", "1", "31"});

    // 複数行
    lines = {
        "124,6.12323399573677E-17,-1.,0.,15.,6.12323399573677E-17,             23P     37",
        "3.74939945665464E-33,-1.,35.,1.,6.12323399573677E-17,                 23P     38",
        "6.12323399573677E-17,20.;                                             23P     39"
    };
    epd = i_ent::ToRawEntityPD(lines, p_delim, r_delim);
    CheckEntityParam(epd, i_ent::EntityType::kTransformationMatrix, 23, 37,
                     {"6.12323399573677E-17", "-1.", "0.", "15.",
                      "6.12323399573677E-17", "3.74939945665464E-33",
                      "-1.", "35.", "1.", "6.12323399573677E-17",
                      "6.12323399573677E-17", "20."});

    // 区切り文字以降にコメントがある場合
    // コメントは無視される
    lines = {
        "126,1,1,1,0,1,0,0.,0.,1.,1.,1.,1.,1.,1.,0.,0.,1.,0.,0.,1.,0.,         41P     50",
        "0.,1.;                                                                41P     51",
        "45HTHIS IS A COMMENT FOR RATIONAL B SPLINE CURVE                      41P     52"
    };
    epd = i_ent::ToRawEntityPD(lines, p_delim, r_delim);
    CheckEntityParam(epd, i_ent::EntityType::kRationalBSplineCurve, 41, 50,
                     {"1", "1", "1", "0", "1", "0", "0.", "0.", "1.", "1.",
                      "1.", "1.", "1.", "1.", "0.", "0.", "1.", "0.", "0.",
                      "1.", "0.", "0.", "1."});
}



/******************************************************************************
 * ToIGESParameterVectorの型分類のテスト
 *
 * pd.dataの各文字列を内容ヒューリスティック(ClassifyParameter)で型推定し、
 * 整数→実数→文字列→言語ステートメントの順にフォールバック変換する。
 * ここでは get_format(i).type が期待どおりに分類されるかを検証する。
 *****************************************************************************/

namespace {

/// @brief 与えた文字列列をpd.dataに持つRawEntityPDを変換する
/// @param data パラメータ文字列の列
/// @return 分類・変換済みのIGESParameterVector
/// @note typeは分類に影響しないためkNullを用いる
igesio::IGESParameterVector ClassifyParameters(
        const std::vector<std::string>& data) {
    return i_ent::ToIGESParameterVector(
            i_ent::RawEntityPD(i_ent::EntityType::kNull, data));
}

}  // namespace

// 整数形式("123"/"+5"/"-7")はInteger型に分類される
TEST(ToIGESParameterVectorTest, ClassifiesInteger) {
    const auto v = ClassifyParameters({"123", "+5", "-7"});
    ASSERT_EQ(v.size(), 3u);
    for (size_t i = 0; i < v.size(); ++i) {
        EXPECT_EQ(v.get_format(i).type, igesio::IGESParameterType::kInteger)
                << "index " << i;
    }
}

// 実数形式(".5"や指数部D/Eを含む)はReal型に分類される
TEST(ToIGESParameterVectorTest, ClassifiesReal) {
    const auto v = ClassifyParameters({"1.5", ".5", "1E-08", "1.0D+01"});
    ASSERT_EQ(v.size(), 4u);
    for (size_t i = 0; i < v.size(); ++i) {
        EXPECT_EQ(v.get_format(i).type, igesio::IGESParameterType::kReal)
                << "index " << i;
    }
}

// Hollerith形式("5Hhello"/"2HUM")はString型に分類される
TEST(ToIGESParameterVectorTest, ClassifiesString) {
    const auto v = ClassifyParameters({"5Hhello", "2HUM"});
    ASSERT_EQ(v.size(), 2u);
    for (size_t i = 0; i < v.size(); ++i) {
        EXPECT_EQ(v.get_format(i).type, igesio::IGESParameterType::kString)
                << "index " << i;
    }
}

// 'D'を含み実数と誤推定されるが、変換に失敗してLanguage Statementへフォールバックする
TEST(ToIGESParameterVectorTest, FallsBackToLanguageStatement) {
    const auto v = ClassifyParameters({"DEGREE", "SOLIDWORKS"});
    ASSERT_EQ(v.size(), 2u);
    for (size_t i = 0; i < v.size(); ++i) {
        EXPECT_EQ(v.get_format(i).type,
                  igesio::IGESParameterType::kLanguageStatement) << "index " << i;
    }
}

// 空パラメータはデフォルト値のString型として扱われる
TEST(ToIGESParameterVectorTest, EmptyParameterIsDefaultString) {
    const auto v = ClassifyParameters({""});
    ASSERT_EQ(v.size(), 1u);
    EXPECT_EQ(v.get_format(0).type, igesio::IGESParameterType::kString);
    EXPECT_TRUE(v.get_format(0).is_default);
}
