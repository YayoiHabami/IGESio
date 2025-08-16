/**
 * @file entities/de/test_raw_entity_de_is_valid.cpp
 * @brief entities/de/raw_entity_de.hのIsValid関数のテスト
 * @author Yayoi Habami
 * @date 2025-08-13
 * @copyright 2025 Yayoi Habami
 * @note 合わせて`RawEntityDE::ByDefault`のテストも行う.
 *       各DEパラメータを1つだけ変えて検証し、それ以外は`ByDefault`で生成した
 *       値を使用するため、`ByDefault`の設定する値に問題がある場合も検出可能。
 */
#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/entities/de/raw_entity_de.h"

#include "./de_test_core.h"

namespace {

namespace iio = igesio;
namespace i_ent = igesio::entities;
namespace i_test = igesio::tests;

using ET = i_ent::EntityType;

constexpr auto Na = i_test::DEValueType::kNA;
constexpr auto I = i_test::DEValueType::kInt;
constexpr auto P = i_test::DEValueType::kPtr;
constexpr auto IP = i_test::DEValueType::kIPtr;
constexpr auto ZP = i_test::DEValueType::kZPtr;
constexpr auto O = i_test::DEValueType::kOne;
constexpr auto Z = i_test::DEValueType::kZero;
constexpr auto PZ = i_test::DEValueType::kPositive;

/// @brief （エラーを起こさない）デフォルトのRawEntityDEを生成する
/// @param entity_type エンティティタイプ
/// @param form_number フォーム番号
/// @param str StructureのDEValueType
/// @param lv LevelのDEValueType
/// @param xm Transformation MatrixのDEValueType
/// @return RawEntityDEのインスタンス
i_ent::RawEntityDE DefaultRawEntityDE(
        const ET entity_type, const int form_number,
        const i_test::DEValueType str_type,
        const i_test::DEValueType lv_type,
        const i_test::DEValueType xm_type) {
    auto de = i_ent::RawEntityDE::ByDefault(entity_type, form_number);
    if (str_type == i_test::DEValueType::kPtr) {
        // Structureがポインターのみの場合は、仮の値として1を設定
        de.structure = 1;
    } else if (lv_type == i_test::DEValueType::kPtr) {
        // Levelがポインターのみの場合は、仮の値として1を設定
        de.level = 1;
    } else if (xm_type == i_test::DEValueType::kPtr) {
        // Transformation Matrixがポインターのみの場合は、仮の値として1を設定
        de.transformation_matrix = 1;
    }

    return de;
}

/// @brief テスト用の構造体
template <typename T>
struct TestCases {
    /// @brief 正常系のテストケース値
    std::vector<T> valid;
    /// @brief 異常系のテストケース値
    std::vector<T> invalid;

    /// @brief コンストラクタ
    /// @param valid_cases 正常系のテストケース値
    /// @param invalid_cases 異常系のテストケース値
    TestCases(std::vector<int> valid_cases,
              std::vector<int> invalid_cases) {
        for (int v : valid_cases) valid.push_back(static_cast<T>(v));
        for (int iv : invalid_cases) invalid.push_back(static_cast<T>(iv));
    }
};

/// @brief DEValueTypeのテストケース
/// @param param_index パラメータのインデックス.
///        0~8が {3, 4, 5, 6, 7, 8, 12, 13, 14} に対応
/// @param type DEValueType
/// @todo ポインターの絶対値の上限が`9999999`であることを確認する
///       ためのテストケースを追加する
TestCases<int> DEValueTypeCases(const unsigned int param_index,
                                const i_test::DEValueType type) {
    int large_v = 100;
    if (param_index == 1) large_v = 5;  // Line Font Patternの最大値
    else if (param_index == 7) large_v = 8;  // Color Numberの最大値

    switch (type) {
        case i_test::DEValueType::kNA:
            // N.A.は未定義 (デフォルト値の0のみを許容)
            return TestCases<int>({0}, {1, -1, 2});
        case i_test::DEValueType::kInt:
            // `#`は0を含む任意の正の整数値を許容
            return TestCases<int>({0, 1, 2, large_v}, {-1, -2});
        case i_test::DEValueType::kPtr:
            // `=>`は1以上の正値を許容
            return TestCases<int>({1, large_v}, {0, -1, -100});
        case i_test::DEValueType::kIPtr:
            // `#,=>`はすべての整数値を許容
            return TestCases<int>({-1, -2, 0, 1, large_v}, {});
        case i_test::DEValueType::kZPtr:
            // `0,=>`は0または1以上の正値を許容
            return TestCases<int>({0, 1, 100}, {-1, -100});
        case i_test::DEValueType::kOne:
            return TestCases<int>({1}, {0, -1, 2});
        case i_test::DEValueType::kZero:
            return TestCases<int>({0}, {1, -1, 2});
        case i_test::DEValueType::kPositive:
            return TestCases<int>({1, large_v}, {0, -1, 2});
        default:
            throw iio::ImplementationError("Unknown DEValueType");
    }
}

/// @brief IsValidの検証時のエラーメッセージ
/// @param entity_type エンティティタイプ
/// @param form_number フォーム番号
/// @param param_index パラメータのインデックス
/// @param value 検証した値
/// @param is_valid_case 検証が有効なケースかどうか
/// @return エラーメッセージ
std::string IsValidErrorMessage(i_ent::EntityType entity_type,
                                unsigned int form_number,
                                unsigned int param_index,
                                const int value,
                                const bool is_valid_case) {
    std::string param_name;
    switch (param_index) {
        case 0:
            param_name = "Structure";
            break;
        case 1:
            param_name = "Line Font Pattern";
            break;
        case 2:
            param_name = "Level";
            break;
        case 3:
            param_name = "View";
            break;
        case 4:
            param_name = "Transformation Matrix";
            break;
        case 5:
            param_name = "Label Display Associativity";
            break;
        case 6:
            param_name = "Line Weight Number";
            break;
        case 7:
            param_name = "Color Number";
            break;
        case 8:
            param_name = "Parameter Line Count";
            break;
        default:
            throw iio::ImplementationError("Invalid parameter index: " +
                                           std::to_string(param_index));
    }

    std::string error_suffix;
    if (is_valid_case) {
        error_suffix = " must accept value '" + std::to_string(value) + "'";
    } else {
        error_suffix = " must reject value '" + std::to_string(value) + "'";
    }

    return "The " + param_name + " parameter of " + i_ent::ToString(entity_type) +
           " entity (Type " + std::to_string(static_cast<int>(entity_type)) +
           ") with Form " + std::to_string(form_number) + error_suffix;
}

/// @brief ステータス番号の2桁の文字列から有効な値のリストを生成する
/// @param index ステータス番号のインデックス (0~3)
/// @param char1 ステータス番号の値 (1桁目)
/// @param char2 ステータス番号の値 (2桁目)
std::vector<int> StatusNumberValues(
        const unsigned int index, const char char1, const char char2) {
    if (char1 == '*' && char2 == '*') return {0};  // N.A.と同等
    if (char1 == '0' && (char2 >= '0' && char2 <= '6')) {
        // 指定された数値
        return {static_cast<int>(char2 - '0')};
    }
    if (char1 == '?' && char2 == '?') {
        // 任意値
        switch (index) {
            case 0:  // blank status
                return {0, 1};
            case 1:  // subordinate entity switch
                return {0, 1, 2, 3};
            case 2:  // entity use flag
                return {0, 1, 2, 3, 4, 5, 6};
            case 3:  // hierarchy
                return {0, 1, 2};
        }
    }

    throw iio::ImplementationError(
            "Invalid status number characters: '" + std::string(1, char1) +
            std::string(1, char2) + "' for index " + std::to_string(index));
}

/// @brief RawEntityDEのステータス番号が有効かどうかを検証する
/// @param de_original 検証するRawEntityDEのインスタンス
/// @param expected_stat 期待されるステータス番号の文字列.
void TestStatusNumberIsValid(const i_ent::RawEntityDE& de_original,
                             const std::string& expected_stat) {
    if (expected_stat.size() != 8) {
        throw iio::ImplementationError(
            "Invalid expected status string size; expected 8 digits, got '" +
            expected_stat + "'");
    }

    const auto& es = expected_stat;
    std::vector<i_ent::EntityStatus> status_list;
    for (auto bs : StatusNumberValues(0, es[0], es[1])) {
        auto status = i_test::DefaultEntityStatus(es);
        status.blank_status = (bs == 0);
        status_list.push_back(status);
    }
    for (auto ses : StatusNumberValues(1, es[2], es[3])) {
        auto status = i_test::DefaultEntityStatus(es);
        status.subordinate_entity_switch =
            static_cast<i_ent::SubordinateEntitySwitch>(ses);
        status_list.push_back(status);
    }
    for (auto eus : StatusNumberValues(2, es[4], es[5])) {
        auto status = i_test::DefaultEntityStatus(es);
        status.entity_use_flag = static_cast<i_ent::EntityUseFlag>(eus);
        status_list.push_back(status);
    }
    for (auto hs : StatusNumberValues(3, es[6], es[7])) {
        auto status = i_test::DefaultEntityStatus(es);
        status.hierarchy = static_cast<i_ent::HierarchyType>(hs);
        status_list.push_back(status);
    }

    for (const auto& status : status_list) {
        auto de = de_original;  // コピーを作成
        de.status = status;
        EXPECT_NO_THROW(i_ent::IsValid(de));
    }
}

/// @brief RawEntityDEのIsValid関数のテスト
/// @param entity_type エンティティタイプ
/// @param form_number フォーム番号
/// @param expected 期待されるテストケース.
///        (DEパラメータ `{3, 4, 5, 6, 7, 8, 12, 13, 14}`)
/// @param expected_stat パラメータ9 (Status Number) として期待される文字列.
///        "**??01**"のような、8桁の文字列
void TestIsValid(const unsigned int entity_type_number,
                 const unsigned int form_number,
                 const std::array<i_test::DEValueType, 9>& expected,
                 const std::string& expected_stat) {
    // 各DEパラメータごとに、validな値とinvalidな値をテストする
    auto entity_type = static_cast<i_ent::EntityType>(entity_type_number);
    for (size_t i = 0; i < expected.size(); ++i) {
        // 検証するDEパラメータごとに、新規インスタンスを作成
        auto de = DefaultRawEntityDE(
                entity_type, form_number, expected[0], expected[2], expected[4]);

        auto expected_v = DEValueTypeCases(i, expected[i]);
        auto cases = std::vector<std::pair<int, bool>>();
        for (const auto& v : expected_v.valid) cases.emplace_back(v, true);
        for (const auto& iv : expected_v.invalid) cases.emplace_back(iv, false);

        for (const auto& [v, is_valid_case] : cases) {
            switch (i) {
                case 0:
                    de.structure = v;
                    break;
                case 1:
                    de.line_font_pattern = v;
                    break;
                case 2:
                    de.level = v;
                    break;
                case 3:
                    de.view = v;
                    break;
                case 4:
                    de.transformation_matrix = v;
                    break;
                case 5:
                    de.label_display_associativity = v;
                    break;
                case 6:
                    de.line_weight_number = v;
                    break;
                case 7:
                    de.color_number = v;
                    break;
                case 8:
                    de.parameter_line_count = v;
                    break;
                default:
                    throw iio::ImplementationError("Unexpected index");
            }
            if (is_valid_case) {
                EXPECT_NO_THROW(i_ent::IsValid(de))
                    << IsValidErrorMessage(
                        entity_type, form_number, i, v, is_valid_case);
            } else {
                EXPECT_THROW(i_ent::IsValid(de), iio::DataFormatError)
                    << IsValidErrorMessage(
                        entity_type, form_number, i, v, is_valid_case);
            }
        }
    }

    // Status Numberの検証
    auto de = DefaultRawEntityDE(
            entity_type, form_number, expected[0], expected[2], expected[4]);
    TestStatusNumberIsValid(de, expected_stat);
}

}  // namespace



TEST(RawEntityDEIsValidTest, Type000) {
    TestIsValid(0, 0, {Na, Na, Na, Na, Na, Na, Na, Na, Na}, "********");
}
TEST(RawEntityDEIsValidTest, Type100) {
    TestIsValid(100, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
}

TEST(RawEntityDEIsValidTest, Type102) {
    TestIsValid(102, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????????");
}

TEST(RawEntityDEIsValidTest, Type104) {
    for (unsigned int fn : {1U, 2U, 3U}) {
        TestIsValid(104, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
    }
}

TEST(RawEntityDEIsValidTest, Type106) {
    for (unsigned int fn : {1U, 2U, 3U}) {
        TestIsValid(106, fn, {Na, Na, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
    }
    for (unsigned int fn : {11U, 12U, 13U}) {
        TestIsValid(106, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
    }
    for (unsigned int fn : {20U, 21U}) {
        TestIsValid(106, fn, {Na, O, IP, ZP, ZP, ZP, I, IP, I}, "????01**");
    }
    for (unsigned int fn : {31U, 32U, 33U, 34U, 35U, 36U, 37U, 38U}) {
        TestIsValid(106, fn, {Na, O, IP, ZP, ZP, ZP, I, IP, I}, "????01**");
    }
    TestIsValid(106, 40, {Na, O, IP, ZP, ZP, ZP, I, IP, I}, "????01**");
    TestIsValid(106, 63, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
}

TEST(RawEntityDEIsValidTest, Type108) {
    for (int fn : {-1, 0, 1}) {
        TestIsValid(108, fn, {Na, Na, IP, ZP, ZP, ZP, Na, IP, I}, "??????**");
    }
}

TEST(RawEntityDEIsValidTest, Type110) {
    TestIsValid(110, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
    for (unsigned int fn : {1U, 2U}) {
        TestIsValid(110, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????06**");
    }
}

TEST(RawEntityDEIsValidTest, Type112) {
    TestIsValid(112, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
}

TEST(RawEntityDEIsValidTest, Type114) {
    TestIsValid(114, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
}

TEST(RawEntityDEIsValidTest, Type116) {
    TestIsValid(116, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????????");
}

TEST(RawEntityDEIsValidTest, Type118) {
    for (unsigned int fn : {0U, 1U}) {
        TestIsValid(118, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
    }
}

TEST(RawEntityDEIsValidTest, Type120) {
    TestIsValid(120, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
}

TEST(RawEntityDEIsValidTest, Type122) {
    TestIsValid(122, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
}

TEST(RawEntityDEIsValidTest, Type123) {
    TestIsValid(123, 0, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0102**");
}

TEST(RawEntityDEIsValidTest, Type124) {
    for (unsigned int fn : {0U, 1U, 10U, 11U, 12U}) {
        TestIsValid(124, fn, {Na, Na, Na, Na, ZP, Na, Na, Na, I}, "****??**");
    }
}

TEST(RawEntityDEIsValidTest, Type125) {
    for (unsigned int fn : {0U, 1U, 2U, 3U, 4U}) {
        TestIsValid(125, fn, {Na, O, IP, ZP, ZP, ZP, I, IP, I}, "??????00");
    }
}

TEST(RawEntityDEIsValidTest, Type126) {
    for (unsigned int fn : {0U, 1U, 2U, 3U, 4U, 5U}) {
        TestIsValid(126, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
    }
}

TEST(RawEntityDEIsValidTest, Type128) {
    for (unsigned int fn : {0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U}) {
        TestIsValid(128, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
    }
}

TEST(RawEntityDEIsValidTest, Type130) {
    TestIsValid(130, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
}

TEST(RawEntityDEIsValidTest, Type132) {
    TestIsValid(132, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????04??");
}

TEST(RawEntityDEIsValidTest, Type134) {
    TestIsValid(134, 0, {Na, Na, Na, Na, P, Na, Na, IP, I}, "????04??");
}

TEST(RawEntityDEIsValidTest, Type136) {
    TestIsValid(136, 0, {Na, IP, Na, Na, Na, ZP, Na, IP, I}, "********");
}

TEST(RawEntityDEIsValidTest, Type138) {
    TestIsValid(138, 0, {Na, Na, Na, Na, Na, ZP, Na, Na, I}, "??????**");
}

TEST(RawEntityDEIsValidTest, Type140) {
    TestIsValid(140, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
}

TEST(RawEntityDEIsValidTest, Type141) {
    TestIsValid(141, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
}

TEST(RawEntityDEIsValidTest, Type142) {
    TestIsValid(142, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????00**");
}

TEST(RawEntityDEIsValidTest, Type143) {
    TestIsValid(143, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????00**");
}

TEST(RawEntityDEIsValidTest, Type144) {
    TestIsValid(144, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????00**");
}

TEST(RawEntityDEIsValidTest, Type146) {
    for (unsigned int fn = 0; fn <= 34; ++fn) {
        TestIsValid(146, fn, {Na, Na, Na, Na, Na, ZP, Na, IP, I}, "**??03**");
    }
}

TEST(RawEntityDEIsValidTest, Type148) {
    for (unsigned int fn = 0; fn <= 34; ++fn) {
        TestIsValid(148, fn, {Na, Na, Na, Na, Na, ZP, Na, IP, I}, "**??03**");
    }
}

TEST(RawEntityDEIsValidTest, Type150) {
    TestIsValid(150, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????00**");
}

TEST(RawEntityDEIsValidTest, Type152) {
    TestIsValid(152, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????00**");
}

TEST(RawEntityDEIsValidTest, Type154) {
    TestIsValid(154, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????00**");
}

TEST(RawEntityDEIsValidTest, Type156) {
    TestIsValid(156, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????00**");
}

TEST(RawEntityDEIsValidTest, Type158) {
    TestIsValid(158, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????00**");
}

TEST(RawEntityDEIsValidTest, Type160) {
    TestIsValid(160, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "00000000");
}

TEST(RawEntityDEIsValidTest, Type162) {
    for (unsigned int fn : {0U, 1U}) {
        TestIsValid(162, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????00**");
    }
}

TEST(RawEntityDEIsValidTest, Type164) {
    TestIsValid(164, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????00**");
}

TEST(RawEntityDEIsValidTest, Type168) {
    TestIsValid(168, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????00**");
}

TEST(RawEntityDEIsValidTest, Type180) {
    for (unsigned int fn : {0U, 1U}) {
        TestIsValid(180, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????00??");
    }
}

TEST(RawEntityDEIsValidTest, Type182) {
    TestIsValid(182, 0, {Na, Na, IP, ZP, ZP, ZP, Na, IP, I}, "**??03**");
}

TEST(RawEntityDEIsValidTest, Type184) {
    for (unsigned int fn : {0U, 1U}) {
        TestIsValid(184, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????02??");
    }
}

TEST(RawEntityDEIsValidTest, Type186) {
    TestIsValid(186, 0, {Na, Na, IP, Na, ZP, ZP, Na, Na, I}, "????????");
}

TEST(RawEntityDEIsValidTest, Type190) {
    for (unsigned int fn : {0U, 1U}) {
        TestIsValid(190, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "**????**");
    }
}

TEST(RawEntityDEIsValidTest, Type192) {
    for (unsigned int fn : {0U, 1U}) {
        TestIsValid(192, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "**01??**");
    }
}

TEST(RawEntityDEIsValidTest, Type194) {
    for (unsigned int fn : {0U, 1U}) {
        TestIsValid(194, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "**01??**");
    }
}

TEST(RawEntityDEIsValidTest, Type196) {
    for (unsigned int fn : {0U, 1U}) {
        TestIsValid(196, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "**01??**");
    }
}

TEST(RawEntityDEIsValidTest, Type198) {
    for (unsigned int fn : {0U, 1U}) {
        TestIsValid(198, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "**01??**");
    }
}

TEST(RawEntityDEIsValidTest, Type202) {
    TestIsValid(202, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????01??");
}

TEST(RawEntityDEIsValidTest, Type204) {
    TestIsValid(204, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????01??");
}

TEST(RawEntityDEIsValidTest, Type206) {
    TestIsValid(206, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????01??");
}

TEST(RawEntityDEIsValidTest, Type208) {
    TestIsValid(208, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????01??");
}

TEST(RawEntityDEIsValidTest, Type210) {
    TestIsValid(210, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????01??");
}

TEST(RawEntityDEIsValidTest, Type212) {
    for (unsigned int fn : {0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 100U, 101U, 102U, 105U}) {
        TestIsValid(212, fn, {Na, O, IP, ZP, ZP, ZP, I, IP, I}, "????01**");
    }
}

TEST(RawEntityDEIsValidTest, Type213) {
    TestIsValid(213, 0, {Na, O, IP, ZP, ZP, ZP, I, IP, I}, "????01**");
}

TEST(RawEntityDEIsValidTest, Type214) {
    for (unsigned int fn : {1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U}) {
        TestIsValid(214, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????01**");
    }
}

TEST(RawEntityDEIsValidTest, Type216) {
    for (unsigned int fn : {0U, 1U, 2U}) {
        TestIsValid(216, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????01??");
    }
}

TEST(RawEntityDEIsValidTest, Type218) {
    for (unsigned int fn : {0U, 1U}) {
        TestIsValid(218, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????01??");
    }
}

TEST(RawEntityDEIsValidTest, Type220) {
    TestIsValid(220, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????01??");
}

TEST(RawEntityDEIsValidTest, Type222) {
    for (unsigned int fn : {0U, 1U}) {
        TestIsValid(222, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????01??");
    }
}

TEST(RawEntityDEIsValidTest, Type228) {
    for (unsigned int fn : {0U, 1U, 2U, 3U, 5001U, 6000U, 7000U, 8000U, 9000U, 9999U}) {
        TestIsValid(228, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????01??");
    }
}

TEST(RawEntityDEIsValidTest, Type230) {
    for (unsigned int fn : {0U, 1U}) {
        TestIsValid(230, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????01??");
    }
}

TEST(RawEntityDEIsValidTest, Type302) {
    for (unsigned int fn : {5001U, 6000U, 7000U, 8000U, 9000U, 9999U}) {
        TestIsValid(302, fn, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0002**");
    }
}

TEST(RawEntityDEIsValidTest, Type304) {
    for (unsigned int fn : {1U, 2U}) {
        TestIsValid(304, fn, {Na, P, Na, Na, ZP, Na, Na, Na, I}, "**0002**");
    }
}

TEST(RawEntityDEIsValidTest, Type306) {
    TestIsValid(306, 0, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0002**");
}

TEST(RawEntityDEIsValidTest, Type308) {
    TestIsValid(308, 0, {Na, IP, IP, Na, ZP, ZP, I, IP, I}, "**??02??");
}

TEST(RawEntityDEIsValidTest, Type310) {
    TestIsValid(310, 0, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0002**");
}

TEST(RawEntityDEIsValidTest, Type312) {
    for (unsigned int fn : {0U, 1U}) {
        TestIsValid(312, fn, {Na, Na, IP, ZP, ZP, ZP, Na, IP, I}, "??000200");
    }
}

TEST(RawEntityDEIsValidTest, Type314) {
    TestIsValid(314, 0, {Na, Na, Na, Na, Na, Na, Na, P, I}, "**0002**");
}

TEST(RawEntityDEIsValidTest, Type316) {
    TestIsValid(316, 0, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0002**");
}

TEST(RawEntityDEIsValidTest, Type320) {
    TestIsValid(320, 0, {Na, IP, IP, Na, ZP, ZP, I, IP, I}, "**??02??");
}

TEST(RawEntityDEIsValidTest, Type322) {
    for (unsigned int fn : {0U, 1U, 2U}) {
        TestIsValid(322, fn, {Na, IP, IP, Na, ZP, ZP, I, IP, I}, "**??02??");
    }
}

TEST(RawEntityDEIsValidTest, Type402) {
    for (unsigned int fn : {1U, 5U, 7U, 9U, 12U, 13U, 14U, 15U}) {
        TestIsValid(402, fn, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**????**");
    }
    for (unsigned int fn : {3U, 4U, 19U}) {
        TestIsValid(402, fn, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0001**");
    }
    TestIsValid(402, 16, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**??05**");
    for (unsigned int fn : {18U, 20U}) {
        TestIsValid(402, fn, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**??03**");
    }
    TestIsValid(402, 21, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0102**");
}

TEST(RawEntityDEIsValidTest, Type404) {
    for (unsigned int fn : {0U, 1U}) {
        TestIsValid(404, fn, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0001**");
    }
}

TEST(RawEntityDEIsValidTest, Type406) {
    for (unsigned int fn : {1U, 2U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U, 20U, 21U, 22U, 23U, 24U, 25U}) {
        TestIsValid(406, fn, {Na, Na, IP, Na, Na, Na, Na, Na, I}, "**??****");
    }
    TestIsValid(406, 3, {Na, Na, IP, Na, Na, Na, Na, Na, I}, "**00****");
    TestIsValid(406, 26, {Na, Na, P, Na, Na, Na, Na, Na, I}, "**??****");
    for (unsigned int fn : {27U, 31U}) {
        TestIsValid(406, fn, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0102**");
    }
    for (unsigned int fn : {28U, 29U, 30U}) {
        TestIsValid(406, fn, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0202**");
    }
    for (unsigned int fn : {32U, 33U, 34U, 35U}) {
        TestIsValid(406, fn, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0101**");
    }
    TestIsValid(406, 36, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "00010300");
}

TEST(RawEntityDEIsValidTest, Type408) {
    TestIsValid(408, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????????");
}

TEST(RawEntityDEIsValidTest, Type410) {
    TestIsValid(410, 0, {Na, Na, Na, Na, ZP, Na, Na, Na, I}, "????01**");
    TestIsValid(410, 1, {Na, Na, Na, Na, Z, Na, Na, Na, I}, "????01**");
}

TEST(RawEntityDEIsValidTest, Type412) {
    TestIsValid(412, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????????");
}

TEST(RawEntityDEIsValidTest, Type414) {
    TestIsValid(414, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????????");
}

TEST(RawEntityDEIsValidTest, Type416) {
    for (unsigned int fn : {0U, 1U, 2U, 3U, 4U}) {
        TestIsValid(416, fn, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**????**");
    }
}

TEST(RawEntityDEIsValidTest, Type418) {
    TestIsValid(418, 0, {Na, Na, Na, ZP, ZP, ZP, Na, Na, I}, "??????**");
}

TEST(RawEntityDEIsValidTest, Type420) {
    TestIsValid(420, 0, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????????");
}

TEST(RawEntityDEIsValidTest, Type422) {
    for (unsigned int fn : {0U, 1U}) {
        TestIsValid(422, fn, {P, Na, Na, Na, Na, Na, Na, Na, I}, "**????**");
    }
}

TEST(RawEntityDEIsValidTest, Type430) {
    for (unsigned int fn : {0U, 1U}) {
        TestIsValid(430, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????????");
    }
}

TEST(RawEntityDEIsValidTest, Type502) {
    TestIsValid(502, 1, {Na, Na, IP, Na, Na, ZP, Na, Na, I}, "??01??**");
}

TEST(RawEntityDEIsValidTest, Type504) {
    TestIsValid(504, 1, {Na, Na, IP, Na, Na, ZP, Na, Na, I}, "??01??01");
}

TEST(RawEntityDEIsValidTest, Type508) {
    for (unsigned int fn : {0U, 1U}) {
        TestIsValid(508, fn, {Na, Na, IP, Na, Na, ZP, Na, Na, I}, "??01????");
    }
}

TEST(RawEntityDEIsValidTest, Type510) {
    TestIsValid(510, 1, {Na, Na, IP, Na, Na, ZP, Na, Na, I}, "??01????");
}

TEST(RawEntityDEIsValidTest, Type514) {
    for (unsigned int fn : {1U, 2U}) {
        TestIsValid(514, fn, {Na, Na, IP, Na, Na, ZP, Na, Na, I}, "????????");
    }
}
