/**
 * @file entities/test_entity_parameter_data.cpp
 * @brief entities/entity_parameter_data.hのテスト
 * @author Yayoi Habami
 * @date 2025-04-23
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <string>

#include "igesio/common/errors.h"
#include "igesio/entities/entity_parameter_data.h"

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
