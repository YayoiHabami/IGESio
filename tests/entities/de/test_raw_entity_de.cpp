/**
 * @file entities/de/test_raw_entity_de.cpp
 * @brief entities/de/raw_entity_de.hのテスト
 * @author Yayoi Habami
 * @date 2025-04-19
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <string>

#include "igesio/common/errors.h"
#include "igesio/entities/de/raw_entity_de.h"

namespace {

namespace i_ent = igesio::entities;

}  // namespace



/******************************************************************************
 * EntityStatusのテスト
 *****************************************************************************/

// EntityStatusのデフォルトコンストラクタのテスト
TEST(EntityStatusTest, DefaultConstructor) {
    i_ent::EntityStatus status;

    // デフォルト値の確認
    EXPECT_TRUE(status.blank_status);  // デフォルトは表示状態 (00)
    EXPECT_EQ(i_ent::SubordinateEntitySwitch::kIndependent,
              status.subordinate_entity_switch);  // 独立 (00)
    EXPECT_EQ(i_ent::EntityUseFlag::kGeometry,
              status.entity_use_flag);  // ジオメトリ (00)
    EXPECT_EQ(i_ent::HierarchyType::kGlobalTopDown,
              status.hierarchy);  // Global top down (00)
}

// EntityStatusの文字列引数コンストラクタの正常系テスト
TEST(EntityStatusTest, StringConstructor_ValidInput) {
    // すべてゼロの場合
    {
        i_ent::EntityStatus status("00000000");
        EXPECT_TRUE(status.blank_status);
        EXPECT_EQ(i_ent::SubordinateEntitySwitch::kIndependent,
                  status.subordinate_entity_switch);
        EXPECT_EQ(i_ent::EntityUseFlag::kGeometry, status.entity_use_flag);
        EXPECT_EQ(i_ent::HierarchyType::kGlobalTopDown, status.hierarchy);
    }

    // 表示状態の設定 (非表示=01)
    {
        i_ent::EntityStatus status("01000000");
        EXPECT_FALSE(status.blank_status);
        EXPECT_EQ(i_ent::SubordinateEntitySwitch::kIndependent,
                  status.subordinate_entity_switch);
        EXPECT_EQ(i_ent::EntityUseFlag::kGeometry, status.entity_use_flag);
        EXPECT_EQ(i_ent::HierarchyType::kGlobalTopDown, status.hierarchy);
    }

    // 従属エンティティスイッチの設定
    i_ent::EntityStatus status_phys("00010000");
    EXPECT_EQ(i_ent::SubordinateEntitySwitch::kPhysicallyDependent,
              status_phys.subordinate_entity_switch);
    i_ent::EntityStatus status_log("00020000");
    EXPECT_EQ(i_ent::SubordinateEntitySwitch::kLogicallyDependent,
              status_log.subordinate_entity_switch);
    i_ent::EntityStatus status_both("00030000");
    EXPECT_EQ(i_ent::SubordinateEntitySwitch::kPhysicallyAndLogicallyDependent,
              status_both.subordinate_entity_switch);

    // エンティティ用途フラグの設定
    i_ent::EntityStatus status_annot("00000100");
    EXPECT_EQ(i_ent::EntityUseFlag::kAnnotation, status_annot.entity_use_flag);
    i_ent::EntityStatus status_def("00000200");
    EXPECT_EQ(i_ent::EntityUseFlag::kDefinition, status_def.entity_use_flag);
    i_ent::EntityStatus status_other("00000300");
    EXPECT_EQ(i_ent::EntityUseFlag::kOther, status_other.entity_use_flag);
    i_ent::EntityStatus status_logical("00000400");
    EXPECT_EQ(i_ent::EntityUseFlag::kLogicalPosition, status_logical.entity_use_flag);
    i_ent::EntityStatus status_param("00000500");
    EXPECT_EQ(i_ent::EntityUseFlag::k2DParametric, status_param.entity_use_flag);
    i_ent::EntityStatus status_structural("00000600");
    EXPECT_EQ(i_ent::EntityUseFlag::kStructuralGeometry, status_structural.entity_use_flag);

    // 階層タイプの設定
    i_ent::EntityStatus status_defer("00000001");
    EXPECT_EQ(i_ent::HierarchyType::kGlobalDefer, status_defer.hierarchy);
    i_ent::EntityStatus status_prop("00000002");
    EXPECT_EQ(i_ent::HierarchyType::kUseHierarchyProperty, status_prop.hierarchy);


    // 複合ケース
    i_ent::EntityStatus status("01030502");
    EXPECT_FALSE(status.blank_status);  // 非表示 (01)
    EXPECT_EQ(i_ent::SubordinateEntitySwitch::kPhysicallyAndLogicallyDependent,
              status.subordinate_entity_switch);  // 両方従属 (03)
    EXPECT_EQ(i_ent::EntityUseFlag::k2DParametric,
              status.entity_use_flag);  // 2Dパラメトリック (05)
    EXPECT_EQ(i_ent::HierarchyType::kUseHierarchyProperty,
              status.hierarchy);  // Use hierarchy property (02)
}

// EntityStatusの文字列引数コンストラクタの異常系テスト
TEST(EntityStatusTest, StringConstructor_InvalidInput) {
    // 空文字列
    EXPECT_THROW({
        i_ent::EntityStatus status("");
    }, igesio::ParseError);

    // 8桁未満
    EXPECT_THROW({
        i_ent::EntityStatus status("0000");
    }, igesio::ParseError);

    // 8桁を超える
    EXPECT_THROW({
        i_ent::EntityStatus status("0000000000");
    }, igesio::ParseError);

    // 数字以外の文字を含む
    EXPECT_THROW({
        i_ent::EntityStatus status("0000000A");
    }, igesio::ParseError);

    // 無効な表示状態値
    EXPECT_THROW({
        i_ent::EntityStatus status("02000000");  // 02は未定義
    }, igesio::ParseError);

    // 無効な従属エンティティスイッチ値
    EXPECT_THROW({
        i_ent::EntityStatus status("00040000");  // 04は未定義
    }, igesio::ParseError);

    // 無効なエンティティ用途フラグ値
    EXPECT_THROW({
        i_ent::EntityStatus status("00000700");  // 07は未定義
    }, igesio::ParseError);

    // 無効な階層タイプ値
    EXPECT_THROW({
        i_ent::EntityStatus status("00000003");  // 03は未定義
    }, igesio::ParseError);

    // 先頭のゼロが省略されたケース
    EXPECT_THROW({
        i_ent::EntityStatus status("1000002");
    }, igesio::ParseError);

    // 空白を含むケース
    EXPECT_NO_THROW({
        i_ent::EntityStatus status("000000  ");
    });
    EXPECT_NO_THROW({
        i_ent::EntityStatus status("0000 000");
    });
    EXPECT_THROW({
        i_ent::EntityStatus status(" 00000000 ");
    }, igesio::ParseError);
    EXPECT_THROW({
        i_ent::EntityStatus status(" 000000");
    }, igesio::ParseError);
}



/******************************************************************************
 * ToRawEntityDEのテスト
 *****************************************************************************/

namespace {

/// @brief シーケンス番号を除く1行目のパラメータをチェックする
void CheckDEParamL1(
        const i_ent::RawEntityDE& de_param,
        const i_ent::EntityType& type, unsigned int pointer,
        const int structure, const int font,
        const int level, const int view, const int matrix,
        const int label, const i_ent::EntityStatus& status,
        const unsigned int sequence_number) {
    EXPECT_EQ(de_param.entity_type, type) << "EntityType mismatch";
    EXPECT_EQ(de_param.parameter_data_pointer, pointer) << "PD pointer mismatch";
    EXPECT_EQ(de_param.structure, structure) << "Structure mismatch";
    EXPECT_EQ(de_param.line_font_pattern, font) << "Line font pattern mismatch";
    EXPECT_EQ(de_param.level, level) << "Level mismatch";
    EXPECT_EQ(de_param.view, view) << "View mismatch";
    EXPECT_EQ(de_param.transformation_matrix, matrix)
            << "Transformation matrix mismatch";
    EXPECT_EQ(de_param.label_display_associativity, label)
            << "Label display associativity mismatch";
    EXPECT_EQ(de_param.status.blank_status, status.blank_status)
            << "Blank status mismatch";
    EXPECT_EQ(de_param.status.subordinate_entity_switch,
              status.subordinate_entity_switch) << "Subordinate entity switch mismatch";
    EXPECT_EQ(de_param.status.entity_use_flag, status.entity_use_flag)
            << "Entity use flag mismatch";
    EXPECT_EQ(de_param.status.hierarchy, status.hierarchy) << "Hierarchy mismatch";
    EXPECT_EQ(de_param.sequence_number, sequence_number)
            << "Sequence number mismatch";
}

/// @brief 2行目のパラメータをチェックする
void CheckDEParamL2(
        const i_ent::RawEntityDE& de_param,
        const int weight, const int color,
        const int line_count, const int form,
        const std::string& label, const int subscript) {
    EXPECT_EQ(de_param.line_weight_number, weight) << "Line weight mismatch";
    EXPECT_EQ(de_param.color_number, color) << "Color mismatch";
    EXPECT_EQ(de_param.parameter_line_count, line_count)
            << "Parameter line count mismatch";
    EXPECT_EQ(de_param.form_number, form) << "Form number mismatch";
    EXPECT_EQ(de_param.entity_label, label) << "Entity label mismatch";
    EXPECT_EQ(de_param.entity_subscript_number, subscript)
            << "Entity subscript number mismatch";
}

}  // namespace

// 正常系
TEST(ToRawEntityDETest, NormalCase) {
    std::string f, s;  // 1行目と2行目の文字列

    f = "     100     172       0       0       0             183        01010000D    185";
    s = "     100       0       0       1       0                               0D    186";
    auto de = i_ent::ToRawEntityDE(f, s);
    CheckDEParamL1(de, i_ent::EntityType::kCircularArc, 172, 0, 0, 0, 0, 183, 0,
                   i_ent::EntityStatus("01010000"), 185);
    CheckDEParamL2(de, 0, 0, 1, 0, "", 0);
    // IsDefault()の確認
    std::array<bool, 10> is_default = {false, false, false, true, false, true,
                                       false, false, false, true};
    EXPECT_EQ(de.IsDefault(), is_default) << "IsDefault() mismatch";

    f = "     308      10               1       0               0        00020201D     15";
    s = "     308       0               1                          SubFig        D     16";
    de = i_ent::ToRawEntityDE(f, s);
    CheckDEParamL1(de, i_ent::EntityType::kSubfigureDefinition, 10, 0, 1, 0, 0, 0, 0,
                     i_ent::EntityStatus("00020201"), 15);
    CheckDEParamL2(de, 0, 0, 1, 0, "SubFig", 0);
    is_default = {true, false, false, true, false, true,
                  false, true, true, false};
    EXPECT_EQ(de.IsDefault(), is_default) << "IsDefault() mismatch";
}



/******************************************************************************
 * ToStringsのテスト
 *****************************************************************************/

TEST(ToStringsTest, NormalCase) {
    std::string f, s, s_exp;  // 1行目と2行目の文字列
    f = "     308      10               1       0               0        00020201D     15";
    s = "     308       0               1                          SubFig        D     16";
    auto de = i_ent::ToRawEntityDE(f, s);

    auto [f2, s2] = i_ent::ToStrings(de, de.parameter_data_pointer,
                                     de.sequence_number, de.parameter_line_count);
    EXPECT_EQ(f, f2) << "First line mismatch ";

    // パラメータ19については、仕様としてはデフォルト値を持たないが、
    // NULL (デフォルト値を使用する) として出力するプロセッサがあるため、入力と出力が異なる
    s_exp = "     308       0               1                          SubFig       0D     16";
    EXPECT_EQ(s2, s_exp) << "Second line mismatch";
}
