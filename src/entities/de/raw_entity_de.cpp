/**
 * @file entities/de/raw_entity_de.cpp
 * @brief ディレクトリエントリセクションのパラメータを保持する構造体
 * @author Yayoi Habami
 * @date 2025-04-10
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/de/raw_entity_de.h"

#include <array>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

#include "igesio/common/errors.h"
#include "igesio/common/iges_metadata.h"
#include "igesio/utils/iges_string_utils.h"

namespace {

/// @brief igesio 名前空間のエイリアス
namespace iio = igesio;
/// @brief igesio::entities 名前空間のエイリアス
namespace i_ent = igesio::entities;
/// @brief igesio::utils 名前空間のエイリアス
namespace i_util = igesio::utils;



/// @brief DEセクションの各レコードに入る数値の種類
enum class DEValueType {
    /// @brief Not Applicable (<n.a.>)
    kNA,
    /// @brief Integer (#)
    kInt,
    /// @brief Pointer (=>)
    kPtr,
    /// @brief Integer or Pointer (#,=>; pointer is negated)
    kIPtr,
    /// @brief Zero or Pointer (0,=>)
    kZPtr,
    /// @brief 1
    kOne,
    /// @brief 0
    kZero,
    /// @brief 1以上
    /// @note Type 304のパラメータ4と、Type 314のパラメータ13では、
    ///       未指定 (0) を許容せず、かつポインター (負値) も許容しない
    //        ため、これを使用する
    kPositive
};

/// @brief RawEntityDEの個別の値の検証を行う
/// @param value 検証する値 (パラメータ3~9, 11~14)
/// @param expected 期待される値の種類
/// @param range_max 期待される値の最大値. 正の値に設定された場合のみ有効.
///        パラメータ4 (Line Font Pattern, max=5)、パラメータ13 (Color Number,
///        max=8) では、これを指定する必要がある.
/// @return true: 有効, false: 無効
bool IsValidValue(const int value, const DEValueType expected,
                  const int range_max = -1) {
    switch (expected) {
        case DEValueType::kNA:
            return value == 0;  // N.A.は未定義 (デフォルト値の0のみを許容)
        case DEValueType::kInt:
            return (range_max > 0) ? (value >= 0 && value <= range_max) : (value >= 0);
        case DEValueType::kPtr:
            return (value > 0 && value <= iio::kMaxPointerValue);
        case DEValueType::kIPtr:
            return (range_max > 0) ? (value >= iio::kMinPointerValue && value <= range_max)
                                   : (value >= iio::kMinPointerValue);
        case DEValueType::kZPtr:
            return (value >= 0 && value <= iio::kMaxPointerValue);
        case DEValueType::kOne:
            return (value == 1);
        case DEValueType::kZero:
            return (value == 0);
        case DEValueType::kPositive:
            return (range_max > 0) ? (value > 0 && value <= range_max)
                                   : (value > 0);
        default:
            return false;  // 無効な値
    }
}

/// @brief DEのStatus Number (パラメータ9) の検証を行う
/// @param status_num EntityStatusの各パラメータをint型に変換した値.
/// @param expected_1 期待される数値
/// @param expected_2 期待される数値
/// @return true: 有効, false: 無効
/// @throw iio::ImplementationError 期待される数値が無効な場合
/// @note 期待される数値は、'??'、'**'、'00'~'06'のいずれか
bool IsValidStatusNumber(const int status_num, const char expected_1, const char expected_2) {
    if (expected_1 == '*' && expected_2 == '*') {
        return (status_num == 0);  // '**'と'00'は機能的に同義 (Section 2.2.4.4)
    } else if (expected_1 == '?' && expected_2 == '?') {
        return true;  // 任意値
    } else if (expected_1 == '0') {
        int expected_num = expected_2 - '0';
        return (expected_num == status_num)
               && (expected_num >= 0 && expected_num <= 6);
    }
    throw iio::ImplementationError(
            "Invalid expected status number; '" + std::string(1, expected_1) +
            std::string(1, expected_2) + "'");
}

/// @brief RawEntityDEの値が有効かどうかを検証する
/// @param param 検証するRawEntityDE
/// @param expected 期待されるディレクトリエントリパラメータの配列.
///        パラメータ `{3, 4, 5, 6, 7, 8, 12, 13, 14}`
/// @param expected_stat パラメータ9 (Status Number) として期待される文字列.
///        "**??01**"のような、8桁の文字列
/// @throw iio::DataFormatError エンティティタイプとFormが指定するパラメータの条件に
///        合致しない場合
/// @throw iio::ImplementationError 期待されるステータス文字列の長さが無効な場合
void IsValid(const i_ent::RawEntityDE& param,
             const std::array<DEValueType, 9>& expected,
             const std::string& expected_stat) {
    // 検証用文字列の長さをチェック
    if (expected_stat.size() != 8) {
        throw iio::ImplementationError(
            "Invalid expected status string size; expected 8 digits, got '" + expected_stat + "'");
    }

    // エラーメッセージの末尾
    std::string error_suffix = " for entity:\n" +
            i_ent::ToString(param, param.parameter_data_pointer,
                            param.sequence_number, param.parameter_line_count);

    // Parameter 3 (Structure)
    if (!IsValidValue(param.structure, expected[0])) {
        throw iio::DataFormatError("Invalid structure value" + error_suffix);
    }
    // Parameter 4 (Line Font Pattern)
    if (!IsValidValue(param.line_font_pattern, expected[1], 5)) {
        throw iio::DataFormatError("Invalid line font pattern value" + error_suffix);
    }
    // Parameter 5 (Level)
    if (!IsValidValue(param.level, expected[2])) {
        throw iio::DataFormatError("Invalid level value" + error_suffix);
    }
    // Parameter 6 (View)
    if (!IsValidValue(param.view, expected[3])) {
        throw iio::DataFormatError("Invalid view value" + error_suffix);
    }
    // Parameter 7 (Transformation Matrix)
    if (!IsValidValue(param.transformation_matrix, expected[4])) {
        throw iio::DataFormatError("Invalid transformation matrix value" + error_suffix);
    }
    // Parameter 8 (Label Display Associativity)
    if (!IsValidValue(param.label_display_associativity, expected[5])) {
        throw iio::DataFormatError("Invalid label display associativity value" + error_suffix);
    }
    // Parameter 9 (Status Number)
    const auto& st = param.status;
    const auto& es = expected_stat;
    // blank_statusは 00 (表示) がtrue、 01 (非表示) がfalseとしているため反転
    if (!IsValidStatusNumber(static_cast<int>(!st.blank_status), es[0], es[1]) ||
        !IsValidStatusNumber(static_cast<int>(st.subordinate_entity_switch), es[2], es[3]) ||
        !IsValidStatusNumber(static_cast<int>(st.entity_use_flag), es[4], es[5]) ||
        !IsValidStatusNumber(static_cast<int>(st.hierarchy), es[6], es[7])) {
        throw iio::DataFormatError("Invalid status number value (expected: '" + es +
                                   "') " + error_suffix);
    }
    // Parameter 12 (Line Weight Number)
    if (!IsValidValue(param.line_weight_number, expected[6])) {
        throw iio::DataFormatError("Invalid line weight number value" + error_suffix);
    }
    // Parameter 13 (Color Number)
    if (!IsValidValue(param.color_number, expected[7], 8)) {
        throw iio::DataFormatError("Invalid color number value" + error_suffix);
    }
    // Parameter 14 (Parameter Line Count)
    if (!IsValidValue(param.parameter_line_count, expected[8])) {
        throw iio::DataFormatError("Invalid parameter line count "
            + std::to_string(param.parameter_line_count) + " " + error_suffix);
    }
}

/// @brief 数値が範囲内にあるかどうかを検証する
/// @param number 検証する数値
/// @param range 期待される値のの配列
/// @return true: 有効, false: 無効
/// @note 例えば `{1, 2, 5}` が期待される値であれば、
///       `form == 3` の場合などにfalseを返す
bool IsInRange(const int number,
               const std::initializer_list<int>& range) {
    for (const auto& expected : range) {
        if (number == expected) return true;
    }
    return false;
}

/// @brief 数値が範囲内にあるかどうかを検証する
/// @param number 検証する数値
/// @param range_min 範囲の最小値
/// @param range_max 範囲の最大値
/// @return min <= number <= max の場合にtrueを返す
bool IsInRange(const int number, const int range_min, const int range_max) {
    return (number >= range_min && number <= range_max);
}

/// @brief "****01??"のような8桁の文字列をEntityStatusに変換する
/// @param status 8桁の文字列
/// @return EntityStatus
/// @throw igesio::ImplementationError statusが無効な場合
/// @note `??`と`**`は0に設定される. `01`等の数字はその数字を設定する.
igesio::entities::EntityStatus
DefaultEntityStatus(const std::string& status) {
    if (status.size() != 8) {
        throw igesio::ImplementationError("Invalid EntityStatus string length");
    }
    igesio::entities::EntityStatus entity_status;

    try {
        // 1-2桁目: 表示状態
        auto bs = status.substr(0, 2);
        if (bs == "??" || bs == "**") {
            entity_status.blank_status = true;
        } else {
            entity_status.blank_status = igesio::entities::ToBlankStatus(bs);
        }
        // 3-4桁目: 従属エンティティスイッチ
        auto ses = status.substr(2, 2);
        if (ses == "??" || ses == "**") {
            entity_status.subordinate_entity_switch
                    = igesio::entities::SubordinateEntitySwitch::kIndependent;
        } else {
            entity_status.subordinate_entity_switch
                    = igesio::entities::ToSubordinateEntitySwitch(ses);
        }
        // 5-6桁目: エンティティ用途フラグ
        auto euf = status.substr(4, 2);
        if (euf == "??" || euf == "**") {
            entity_status.entity_use_flag = igesio::entities::EntityUseFlag::kGeometry;
        } else {
            entity_status.entity_use_flag
                    = igesio::entities::ToEntityUseFlag(euf);
        }
        // 7-8桁目: 階層の種類
        auto ht = status.substr(6, 2);
        if (ht == "??" || ht == "**") {
            entity_status.hierarchy = igesio::entities::HierarchyType::kGlobalTopDown;
        } else {
            entity_status.hierarchy = igesio::entities::ToHierarchyType(ht);
        }
    } catch (const igesio::ParseError&) {
        throw igesio::ImplementationError("Invalid status string: " + status);
    }

    return entity_status;
}

/// @brief DEValueTypeをintに変換する
/// @param type DEValueType or int値
/// @return int値
int DefaultValue(const std::variant<int, DEValueType>& type) {
    if (std::holds_alternative<int>(type)) {
        return std::get<int>(type);
    }
    switch (std::get<DEValueType>(type)) {
        case DEValueType::kNA:
        case DEValueType::kInt:
        case DEValueType::kPtr:
        case DEValueType::kIPtr:
        case DEValueType::kZPtr:
        case DEValueType::kZero:
            return 0;
        case DEValueType::kOne:
            return 1;
        case DEValueType::kPositive:
            return 1;  // 1以上の値を期待するが、ここでは1を返す
        default:
            return 0;
    }
}

/// @brief RawEntityDEをデフォルト値で初期化する
/// @param entity_type エンティティタイプ
/// @param form_number フォーム番号
/// @param params ディレクトリエントリパラメータの配列.
///        パラメータ `{3, 4, 5, 6, 7, 8, 12, 13, 14}`
/// @param stat パラメータ9 (Status Number) 用のデフォルト文字列.
///        "**??01**"のような、8桁の文字列
/// @throw iio::DataFormatError エンティティタイプとFormが指定するパラメータの条件に
///        合致しない場合
/// @throw iio::ImplementationError 期待されるステータス文字列が無効な場合
/// @note paramについて、特定の値を指定する必要がある場合 (例えば
///       type106form20~40のパラメータ4や、type125のパラメータ4など）は、
///       DEValueTypeの代わりにint値を指定すること.
igesio::entities::RawEntityDE
DefaultDE(const igesio::entities::EntityType entity_type,
          const int form_number,
          const std::array<std::variant<int, DEValueType>, 9>& params,
          const std::string& stat) {
    auto de = igesio::entities::RawEntityDE();

    // 1 - Entity Type Number
    de.entity_type = entity_type;
    // 2 - Parameter Data (PD部は未定のため0)
    de.parameter_data_pointer = 0;
    // 3 - Structure
    de.structure = DefaultValue(params[0]);
    // 4 - Line Font Pattern
    de.line_font_pattern = DefaultValue(params[1]);
    // 5 - Entity Level
    de.level = DefaultValue(params[2]);
    // 6 - View
    de.view = DefaultValue(params[3]);
    // 7 - Transformation Matrix
    de.transformation_matrix = DefaultValue(params[4]);
    // 8 - Label Display Associativity
    de.label_display_associativity = DefaultValue(params[5]);
    // 9 - Status
    de.status = DefaultEntityStatus(stat);
    // 10 - Sequence Number
    de.sequence_number = 0;  // シーケンス番号は未定のため0

    // 11 - Entity Type Number (same as 1)
    // 12 - Line Weight Number
    de.line_weight_number = DefaultValue(params[6]);
    // 13 - Color Number
    de.color_number = DefaultValue(params[7]);
    // 14 - Parameter Line Count
    de.parameter_line_count = DefaultValue(params[8]);
    // 15 - Form Number
    de.form_number = form_number;
    // 16 - Reserved
    // 17 - Reserved
    // 18 - Entity Label
    de.entity_label = "";  // デフォルトは空文字列
    // 19 - Entity Subscript Number
    de.entity_subscript_number = 0;  // デフォルトは0
    // 20 - Sequence Number (same as 10)

    // is_default_の初期化
    std::array<size_t, 10> default_params = {3, 4, 5, 6, 7, 8, 12, 13, 15, 18};
    for (const auto& idx : default_params) {
        de.SetIsDefault(idx, true);
    }

    return de;
}

}  // namespace



i_ent::EntityStatus::EntityStatus(const std::string& status) {
    if (status.size() != 8) {
        throw iio::ParseError(
            "Invalid entity status string size; expected 8 digits, got '" + status + "'");
    }

    // 0の数値部分を空白として出力するポストプロセッサもあるため、
    // 半角スペースは0に置換する
    std::string replaced = status;
    std::replace(replaced.begin(), replaced.end(), ' ', '0');

    // 表示状態の変換
    blank_status = ToBlankStatus(replaced.substr(0, 2));

    // 従属エンティティスイッチの変換
    subordinate_entity_switch = ToSubordinateEntitySwitch(replaced.substr(2, 2));

    // エンティティ使用フラグの変換
    entity_use_flag = ToEntityUseFlag(replaced.substr(4, 2));

    // 階層の変換
    hierarchy = ToHierarchyType(replaced.substr(6, 2));
}

bool i_ent::RawEntityDE::SetIsDefault(
        const size_t index, const bool value) {
    if (3 <= index && index <= 8) {
        // 3-8 => 0-5
        is_default_[index - 3] = value;
    } else if (index == 12) {  // 12 => 6
        is_default_[6] = value;
    } else if (index == 13) {  // 13 => 7
        is_default_[7] = value;
    } else if (index == 15) {  // 15 => 8
        is_default_[8] = value;
    } else if (index == 18) {  // 18 => 9
        is_default_[9] = value;
    } else {
        return false;
    }
    return true;
}




/**
 * 列挙体への変換
 */

bool i_ent::ToBlankStatus(const int n) {
    if (n == 0) return true;   // 表示状態 (00)
    if (n == 1) return false;  // 非表示状態 (01)

    throw iio::ParseError("Invalid blank status value: " + std::to_string(n));
}

bool i_ent::ToBlankStatus(const std::string& status) {
    if (status == "00") return true;   // 表示状態 (00)
    if (status == "01") return false;  // 非表示状態 (01)

    throw iio::ParseError("Invalid blank status value: '" + status + "'");
}

i_ent::SubordinateEntitySwitch
i_ent::ToSubordinateEntitySwitch(const int n) {
    if (n < 0 || n > 3)
            throw iio::ParseError("Invalid subordinate entity switch value: "
                                  + std::to_string(n));

    return static_cast<SubordinateEntitySwitch>(n);
}

i_ent::SubordinateEntitySwitch
i_ent::ToSubordinateEntitySwitch(const std::string& status) {
    // 独立 (00)
    if (status == "00") return SubordinateEntitySwitch::kIndependent;
    // 物理的に従属 (01)
    if (status == "01") return SubordinateEntitySwitch::kPhysicallyDependent;
    // 論理的に従属 (02)
    if (status == "02") return SubordinateEntitySwitch::kLogicallyDependent;
    // 物理的かつ論理的に従属 (03)
    if (status == "03") return SubordinateEntitySwitch::kPhysicallyAndLogicallyDependent;

    throw iio::ParseError("Invalid subordinate entity switch value: '" + status + "'");
}

i_ent::EntityUseFlag i_ent::ToEntityUseFlag(const int n) {
    if (n < 0 || n > 6)
            throw iio::ParseError("Invalid entity use flag value: "
                                  + std::to_string(n));

    return static_cast<EntityUseFlag>(n);
}

i_ent::EntityUseFlag i_ent::ToEntityUseFlag(const std::string& status) {
    // ジオメトリ (00)
    if (status == "00") return EntityUseFlag::kGeometry;
    // 注釈 (01)
    if (status == "01") return EntityUseFlag::kAnnotation;
    // 定義 (02)
    if (status == "02") return EntityUseFlag::kDefinition;
    // その他 (03)
    if (status == "03") return EntityUseFlag::kOther;
    // 論理/位置 (04)
    if (status == "04") return EntityUseFlag::kLogicalPosition;
    // 2Dパラメトリック (05)
    if (status == "05") return EntityUseFlag::k2DParametric;
    // 構造ジオメトリ (06)
    if (status == "06") return EntityUseFlag::kStructuralGeometry;

    throw iio::ParseError("Invalid entity use flag value: '" + status + "'");
}

i_ent::HierarchyType i_ent::ToHierarchyType(const int n) {
    if (n < 0 || n > 2)
            throw iio::ParseError("Invalid hierarchy type value: "
                                  + std::to_string(n));

    return static_cast<HierarchyType>(n);
}

i_ent::HierarchyType i_ent::ToHierarchyType(const std::string& status) {
    // Global top down (00)
    if (status == "00") return HierarchyType::kGlobalTopDown;
    // Global defer (01)
    if (status == "01") return HierarchyType::kGlobalDefer;
    // Use hierarchy property (02)
    if (status == "02") return HierarchyType::kUseHierarchyProperty;

    throw iio::ParseError("Invalid hierarchy type value: '" + status + "'");
}



/**
 * RawEntityDEの変換
 */

namespace {

/// @brief ディレクトリエントリセクションのパラメータ名を取得する
/// @param is_first 1行目 (奇数行目) か否か
/// @param col_number 列番号 (1-10)
/// @return 行・列番号に対応するパラメータ名
/// @throw igesio::SectionFormatError 無効な列番号の場合
std::string GetRawEntityDEName(
        const bool is_first, const int col_number) {
    // 共通の項目
    if (col_number == 1) return "EntityType";
    if (col_number == 10) return "SequenceNumber";

    if (is_first) {
        // 奇数行の項目
        switch (col_number) {
            case 2: return "PDPointer";
            case 3: return "Structure";
            case 4: return "LineFontPattern";
            case 5: return "Level";
            case 6: return "View";
            case 7: return "TransformationMatrix";
            case 8: return "LabelDisplayAssociativity";
            case 9: return "Status";
        }
    } else {
        // 偶数行の項目
        switch (col_number) {
            case 2: return "LineWeightNumber";
            case 3: return "ColorNumber";
            case 4: return "ParameterLineCount";
            case 5: return "FormNumber";
            case 6: return "Reserved";
            case 7: return "Reserved";
            case 8: return "EntityLabel";
            case 9: return "EntitySubscriptNumber";
        }
    }

    throw iio::SectionFormatError("Invalid column number: " + std::to_string(col_number));
}

}  // namespace

i_ent::RawEntityDE
i_ent::ToRawEntityDE(const std::string& first, const std::string& second) {
    const auto w = kFixedColWidth;
    RawEntityDE de;
    int i = -1;
    // 一時保存用文字列
    std::string tmp;

    // 1行目を変換
    try {
        // Parameter 1: Entity Type Number
        auto e_type1 = ToEntityType(igesio::FromIgesInteger(
                first.substr(++i, w), std::nullopt));
        auto e_type2 = ToEntityType(igesio::FromIgesInteger(
                second.substr(i, w), std::nullopt));
        if (e_type1 != e_type2) {
                throw iio::SectionFormatError(
                        "Entity type mismatch: '" + first + "' and '" + second + "'");
        } else if (!e_type1.has_value()) {
                throw iio::SectionFormatError(
                        "Invalid entity type: '" + first + "' and '" + second + "'");
        }
        de.entity_type = e_type1.value();

        // Parameter 2: Pointer to Parameter Data Record
        de.parameter_data_pointer = igesio::FromIgesInteger(
                first.substr(++i * w, w), std::nullopt);

        // Parameter 3: Structure
        tmp = first.substr(++i * w, w);
        de.structure = igesio::FromIgesInteger(tmp, kDefaultStructure);
        de.SetIsDefault(3, i_util::IsOnlySpace(tmp));

        // Parameter 4: Line Font Pattern
        tmp = first.substr(++i * w, w);
        de.line_font_pattern = igesio::FromIgesInteger(tmp, kDefaultLineFontPattern);
        de.SetIsDefault(4, i_util::IsOnlySpace(tmp));

        // Parameter 5: Level
        tmp = first.substr(++i * w, w);
        de.level = igesio::FromIgesInteger(tmp, kDefaultLevel);
        de.SetIsDefault(5, i_util::IsOnlySpace(tmp));

        // Parameter 6: View
        tmp = first.substr(++i * w, w);
        de.view = igesio::FromIgesInteger(tmp, kDefaultView);
        de.SetIsDefault(6, i_util::IsOnlySpace(tmp));

        // Parameter 7: Transformation Matrix
        tmp = first.substr(++i * w, w);
        de.transformation_matrix = igesio::FromIgesInteger(tmp, kDefaultTransformationMatrix);
        de.SetIsDefault(7, i_util::IsOnlySpace(tmp));

        // Parameter 8: Label Display Associativity
        tmp = first.substr(++i * w, w);
        de.label_display_associativity = igesio::FromIgesInteger(
                tmp, kDefaultLabelDisplayAssociativity);
        de.SetIsDefault(8, i_util::IsOnlySpace(tmp));

        // Parameter 9: Status Number
        de.status = i_ent::EntityStatus(first.substr(++i * w, w));

        // Parameter 10: Sequence Number
        de.sequence_number = igesio::FromIgesInteger(
                first.substr(++i * w + 1, w - 1), std::nullopt);
    } catch (const iio::TypeConversionError& e) {
        throw iio::TypeConversionError(
                std::string(e.what()) + " (on line 1, column " + std::to_string(i+1) +
                "; " + GetRawEntityDEName(true, i+1) + ")");
    }

    // 2行目を変換
    i = 0;
    try {
        // Parameter 11: Entity Type Number (skip)

        // Parameter 12: Line Weight Number
        tmp = second.substr(++i * w, w);
        de.line_weight_number = igesio::FromIgesInteger(tmp, std::nullopt);
        de.SetIsDefault(12, i_util::IsOnlySpace(tmp));

        // Parameter 13: Color Number
        tmp = second.substr(++i * w, w);
        de.color_number = igesio::FromIgesInteger(tmp, kDefaultColorNumber);
        de.SetIsDefault(13, i_util::IsOnlySpace(tmp));

        // Parameter 14: Parameter Line Count
        de.parameter_line_count = igesio::FromIgesInteger(
                second.substr(++i * w, w), std::nullopt);

        // Parameter 15: Form Number
        tmp = second.substr(++i * w, w);
        de.form_number = igesio::FromIgesInteger(tmp, kDefaultFormNumber);
        de.SetIsDefault(15, i_util::IsOnlySpace(tmp));

        // Parameter 16-17: Reserved (skip)
        i += 2;

        // Parameter 18: Entity Label
        tmp = second.substr(++i * w, w);
        de.entity_label = i_util::ltrim(tmp);
        de.SetIsDefault(18, i_util::IsOnlySpace(tmp));

        // Parameter 19: Entity Subscript Number
        de.entity_subscript_number = igesio::FromIgesInteger(
                second.substr(++i * w, w), kDefaultEntitySubscriptNumber);

        // Parameter 20: Sequence Number (skip)
    } catch (const iio::TypeConversionError& e) {
        throw iio::TypeConversionError(
                std::string(e.what()) + " (on line 2, column " + std::to_string(i+1) +
                "; " + GetRawEntityDEName(false, i+1) + ")");
    }

    return de;
}



/**
 * 文字列への変換
 */

std::string i_ent::ToString(const i_ent::EntityStatus& status) {
    std::string result;
    result.reserve(8);

    // 表示状態 (2桁)
    result += status.blank_status ? "00" : "01";

    // 従属エンティティスイッチ (2桁)
    result += "0" + std::to_string(static_cast<int>(status.subordinate_entity_switch));

    // エンティティ使用フラグ (2桁)
    result += "0" + std::to_string(static_cast<int>(status.entity_use_flag));

    // 階層 (2桁)
    result += "0" + std::to_string(static_cast<int>(status.hierarchy));
    return result;
}

std::string i_ent::ToString(
        const i_ent::RawEntityDE& param, const int pd_pointer,
        const int sequence_number, const int line_count) {
    auto strings = ToStrings(param, pd_pointer, sequence_number, line_count);
    return strings.first + "\n" + strings.second + "\n";
}

std::pair<std::string, std::string> i_ent::ToStrings(
        const i_ent::RawEntityDE& param,
        const int pd_pointer, const int sequence_number, const int line_count) {
    std::ostringstream oss1, oss2;
    oss1 << std::setfill(' ');

    // 1行目の出力
    // Parameter 1: Entity Type Number
    oss1 << std::setw(kFixedColWidth) << static_cast<int>(param.entity_type);
    // Parameter 2: Pointer to Parameter Data Record
    oss1 << std::setw(kFixedColWidth)
         << ((pd_pointer >= 0) ? std::to_string(pd_pointer) : "xxx");
    // Parameter 3: Structure
    oss1 << std::setw(kFixedColWidth)
         << (param.IsDefault()[0] ? " " : std::to_string(param.structure));
    // Parameter 4: Line Font Pattern
    oss1 << std::setw(kFixedColWidth)
         << (param.IsDefault()[1] ? " " : std::to_string(param.line_font_pattern));
    // Parameter 5: Level
    oss1 << std::setw(kFixedColWidth)
         << (param.IsDefault()[2] ? " " : std::to_string(param.level));
    // Parameter 6: View
    oss1 << std::setw(kFixedColWidth)
         << (param.IsDefault()[3] ? " " : std::to_string(param.view));
    // Parameter 7: Transformation Matrix
    oss1 << std::setw(kFixedColWidth)
         << (param.IsDefault()[4] ? " " : std::to_string(param.transformation_matrix));
    // Parameter 8: Label Display Associativity
    oss1 << std::setw(kFixedColWidth)
         << (param.IsDefault()[5] ? " " : std::to_string(param.label_display_associativity));
    // Parameter 9: Status Number
    oss1 << std::setw(kFixedColWidth) << ToString(param.status);
    // Parameter 10: Sequence Number
    oss1 << "D" << std::setw(kFixedColWidth - 1)
         << ((sequence_number > 0) ? std::to_string(sequence_number) : "xxx");

    // 2行目の出力
    // Parameter 11: Entity Type Number
    oss2 << std::setw(kFixedColWidth) << static_cast<int>(param.entity_type);
    // Parameter 12: Line Weight Number
    oss2 << std::setw(kFixedColWidth)
         << ((param.IsDefault()[6] ? " " : std::to_string(param.line_weight_number)));
    // Parameter 13: Color Number
    oss2 << std::setw(kFixedColWidth)
         << ((param.IsDefault()[7] ? " " : std::to_string(param.color_number)));
    // Parameter 14: Parameter Line Count
    oss2 << std::setw(kFixedColWidth)
         << ((line_count >= 0) ? std::to_string(line_count) : "xxx");
    // Parameter 15: Form Number
    oss2 << std::setw(kFixedColWidth)
         << ((param.IsDefault()[8] ? " " : std::to_string(param.form_number)));
    // Parameter 16-17: Reserved
    oss2 << std::setw(kFixedColWidth) << " " << std::setw(kFixedColWidth) << " ";
    // Parameter 18: Entity Label
    oss2 << std::setw(kFixedColWidth)
         << ((param.IsDefault()[9] ? " " : i_util::ltrim(param.entity_label)));
    // Parameter 19: Entity Subscript Number
    oss2 << std::setw(kFixedColWidth) << param.entity_subscript_number;
    // Parameter 20: Sequence Number
    oss2 << "D" << std::setw(kFixedColWidth - 1)
         << ((sequence_number > 0) ? std::to_string(sequence_number + 1) : "xxx+1");

    return {oss1.str(), oss2.str()};
}



/**
 * 検証用関数
 */

void i_ent::IsValid(const i_ent::RawEntityDE& de) {
    constexpr auto Na = DEValueType::kNA;
    constexpr auto I = DEValueType::kInt;
    constexpr auto P = DEValueType::kPtr;
    constexpr auto IP = DEValueType::kIPtr;
    constexpr auto ZP = DEValueType::kZPtr;
    constexpr auto O = DEValueType::kOne;
    constexpr auto Z = DEValueType::kZero;
    constexpr auto PZ = DEValueType::kPositive;
    const auto form = de.form_number;

    // とりあえずformが0 (デフォルト) のときは有効とする
    //   -> そのほかの値を許容する場合は、個別ケースで上書きする
    bool is_form_valid = IsInRange(form, {0});

    switch (de.entity_type) {
        case EntityType::kNull:
            ::IsValid(de, {Na, Na, Na, Na, Na, Na, Na, Na, Na}, "********");
            break;
        case EntityType::kCircularArc:
            ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
            break;
        case EntityType::kCompositeCurve:
            ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????????");
            break;
        case EntityType::kConicArc:
            is_form_valid = IsInRange(form, 1, 3);
            ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
            break;
        case EntityType::kCopiousData:
            is_form_valid = true;
            if (IsInRange(form, 1, 3)) {
                ::IsValid(de, {Na, Na, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
            } else if (IsInRange(form, 11, 13) || IsInRange(form, {63})) {
                ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
            } else if (IsInRange(form, {20, 21, 40}) || IsInRange(form, 31, 38)) {
                ::IsValid(de, {Na, O, IP, ZP, ZP, ZP, I, IP, I}, "????01**");
            } else {
                is_form_valid = false;
            }
            break;
        case EntityType::kPlane:
            is_form_valid = IsInRange(form, -1, 1);
            ::IsValid(de, {Na, Na, IP, ZP, ZP, ZP, Na, IP, I}, "??????**");
            break;
        case EntityType::kLine:
            if (IsInRange(form, {0})) {
                ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
            } else if (IsInRange(form, 1, 2)) {
                ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????06**");
                is_form_valid = true;
            }
            break;
        case EntityType::kParametricSplineCurve:  // fallthrough Type 112~114
        case EntityType::kParametricSplineSurface:
            ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
            break;
        case EntityType::kPoint:
            ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????????");
            break;
        case EntityType::kRuledSurface:  // fallthrough Type 113~115
            is_form_valid = IsInRange(form, 0, 1);  // Type 118のみformsが異なる
            [[fallthrough]];
        case EntityType::kSurfaceOfRevolution:
        case EntityType::kTabulatedCylinder:
            ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
            break;
        case EntityType::kDirection:
            ::IsValid(de, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0102**");
            break;
        case EntityType::kTransformationMatrix:
            is_form_valid = IsInRange(form, {0, 1}) || IsInRange(form, 10, 12);
            ::IsValid(de, {Na, Na, Na, Na, ZP, Na, Na, Na, I}, "****??**");
            break;
        case EntityType::kFlash:
            is_form_valid = IsInRange(form, 0, 4);
            ::IsValid(de, {Na, O, IP, ZP, ZP, ZP, I, IP, I}, "??????00");
            break;
        case EntityType::kRationalBSplineCurve:
            is_form_valid = IsInRange(form, 0, 5);
            ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
            break;
        case EntityType::kRationalBSplineSurface:
            is_form_valid = IsInRange(form, 0, 9);
            ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
            break;
        case EntityType::kOffsetCurve:
            ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
            break;
        case EntityType::kConnectPoint:
            ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????04??");
            break;
        case EntityType::kNode:
            ::IsValid(de, {Na, Na, Na, Na, P, Na, Na, IP, I}, "????04??");
            break;
        case EntityType::kFiniteElement:
            ::IsValid(de, {Na, IP, Na, Na, Na, ZP, Na, IP, I}, "********");
            break;
        case EntityType::kNodalDisplacementAndRotation:
            ::IsValid(de, {Na, Na, Na, Na, Na, ZP, Na, Na, I}, "??????**");
            break;
        case EntityType::kOffsetSurface:  // fallthrough Type 140~141
        case EntityType::kBoundary:
            ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
            break;
        case EntityType::kCurveOnAParametricSurface:  // fallthrough Type 142~144
        case EntityType::kBoundedSurface:
        case EntityType::kTrimmedSurface:
            ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????00**");
            break;
        case EntityType::kNodalResults:  // fallthrough Type 146~148
        case EntityType::kElementResults:
            is_form_valid = IsInRange(form, 0, 34);
            ::IsValid(de, {Na, Na, Na, Na, Na, ZP, Na, IP, I}, "**??03**");
            break;
        case EntityType::kBlock:  // fallthrough Type 150~158
        case EntityType::kRightAngularWedge:
        case EntityType::kRightCircularCylinder:
        case EntityType::kRightCircularCone:
        case EntityType::kSphere:
            ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????00**");
            break;
        case EntityType::kTorus:
            ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "00000000");
            break;
        case EntityType::kSolidOfRevolution:  // fallthrough Type 162~168
            is_form_valid = IsInRange(form, 0, 1);  // Type 162のみformsが異なる
            [[fallthrough]];
        case EntityType::kSolidOfLinearExtrusion:
        case EntityType::kEllipsoid:
            ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????00**");
            break;
        case EntityType::kBooleanTree:
            is_form_valid = IsInRange(form, 0, 1);
            ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????00??");
            break;
        case EntityType::kSelectedComponent:
            ::IsValid(de, {Na, Na, IP, ZP, ZP, ZP, Na, IP, I}, "**??03**");
            break;
        case EntityType::kSolidAssembly:
            is_form_valid = IsInRange(form, 0, 1);
            ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????02??");
            break;
        case EntityType::kManifoldSolidBRepObject:
            ::IsValid(de, {Na, Na, IP, Na, ZP, ZP, Na, Na, I}, "????????");
            break;
        case EntityType::kPlaneSurface:
            is_form_valid = IsInRange(form, 0, 1);
            ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "**????**");
            break;
        case EntityType::kRightCircularCylinderSurface:  // fallthrough Type 192~198
        case EntityType::kRightCircularConicalSurface:
        case EntityType::kSphericalSurface:
        case EntityType::kToroidalSurface:
            is_form_valid = IsInRange(form, 0, 1);
            ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "**01??**");
            break;
        case EntityType::kAngularDimension:  // fallthrough Type 202~210
        case EntityType::kCurveDimension:
        case EntityType::kDiameterDimension:
        case EntityType::kFlagNote:
        case EntityType::kGeneralLabel:
            ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????01??");
            break;
        case EntityType::kGeneralNote:  // fallthrough Type 212~213
            is_form_valid = IsInRange(form, 0, 8) || IsInRange(form, {100, 101, 102, 105});
            [[fallthrough]];
        case EntityType::kNewGeneralNote:
            ::IsValid(de, {Na, O, IP, ZP, ZP, ZP, I, IP, I}, "????01**");
            break;
        case EntityType::kLeaderArrow:
            is_form_valid = IsInRange(form, 1, 12);
            ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????01**");
            break;
        case EntityType::kLinearDimension:
            is_form_valid = IsInRange(form, 0, 2);
            ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????01??");
            break;
        case EntityType::kOrdinateDimension:
            is_form_valid = IsInRange(form, {0, 1});
            ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????01??");
            break;
        case EntityType::kPointDimension:
            ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????01??");
            break;
        case EntityType::kRadiusDimension:
            is_form_valid = IsInRange(form, 0, 1);
            ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????01??");
            break;
        case EntityType::kGeneralSymbol:
            is_form_valid = IsInRange(form, 0, 3) || IsInRange(form, 5001, 9999);
            ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????01??");
            break;
        case EntityType::kSectionedArea:
            is_form_valid = IsInRange(form, {0, 1});
            ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????01??");
            break;
        case EntityType::kAssociativityDefinition:
            is_form_valid = IsInRange(form, 5001, 9999);
            ::IsValid(de, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0002**");
            break;
        case EntityType::kLineFontDefinition:
            is_form_valid = IsInRange(form, 1, 2);
            ::IsValid(de, {Na, PZ, Na, Na, ZP, Na, Na, Na, I}, "**0002**");
            break;
        case EntityType::kMacroDefinition:
            ::IsValid(de, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0002**");
            break;
        case EntityType::kSubfigureDefinition:
            ::IsValid(de, {Na, IP, IP, Na, ZP, ZP, I, IP, I}, "**??02??");
            break;
        case EntityType::kTextFont:
            ::IsValid(de, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0002**");
            break;
        case EntityType::kTextDisplayTemplate:
            is_form_valid = IsInRange(form, 0, 1);
            ::IsValid(de, {Na, Na, IP, ZP, ZP, ZP, Na, IP, I}, "??000200");
            break;
        case EntityType::kColorDefinition:
            ::IsValid(de, {Na, Na, Na, Na, Na, Na, Na, PZ, I}, "**0002**");
            break;
        case EntityType::kUnitsData:
            ::IsValid(de, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0002**");
            break;
        case EntityType::kNetworkSubfigureDefinition:
            ::IsValid(de, {Na, IP, IP, Na, ZP, ZP, I, IP, I}, "**??02??");
            break;
        case EntityType::kAttributeTableDefinition:
            is_form_valid = IsInRange(form, 0, 2);
            ::IsValid(de, {Na, IP, IP, Na, ZP, ZP, I, IP, I}, "**??02??");
            break;
        case EntityType::kAssociativityInstance:
            is_form_valid = true;
            if (IsInRange(form, {1, 5, 7, 9}) || IsInRange(form, 12, 15)) {
                ::IsValid(de, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**????**");
            } else if (IsInRange(form, {3, 4, 19})) {
                ::IsValid(de, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0001**");
            } else if (IsInRange(form, {16})) {
                ::IsValid(de, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**??05**");
            } else if (IsInRange(form, {18, 20})) {
                ::IsValid(de, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**??03**");
            } else if (IsInRange(form, {21})) {
                ::IsValid(de, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0102**");
            } else {
                is_form_valid = false;
            }
            break;
        case EntityType::kDrawing:
            is_form_valid = IsInRange(form, 0, 1);
            ::IsValid(de, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0001**");
            break;
        case EntityType::kProperty:
            is_form_valid = true;
            if (IsInRange(form, 1, 2) || IsInRange(form, 5, 17) || IsInRange(form, 18, 25)) {
                ::IsValid(de, {Na, Na, IP, Na, Na, Na, Na, Na, I}, "**??****");
            } else if (IsInRange(form, {3})) {
                ::IsValid(de, {Na, Na, IP, Na, Na, Na, Na, Na, I}, "**00****");
            } else if (IsInRange(form, {26})) {
                ::IsValid(de, {Na, Na, P, Na, Na, Na, Na, Na, I}, "**??****");
            } else if (IsInRange(form, {27, 31})) {
                ::IsValid(de, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0102**");
            } else if (IsInRange(form, 28, 30)) {
                ::IsValid(de, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0202**");
            } else if (IsInRange(form, 32, 35)) {
                ::IsValid(de, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0101**");
            } else if (IsInRange(form, {36})) {
                ::IsValid(de, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "00010300");
            } else {
                is_form_valid = false;
            }
            break;
        case EntityType::kSingularSubfigureInstance:
            ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????????");
            break;
        case EntityType::kView:
            is_form_valid = IsInRange(form, 0, 1);
            if (IsInRange(form, {0})) {
                ::IsValid(de, {Na, Na, Na, Na, ZP, Na, Na, Na, I}, "????01**");
            } else if (IsInRange(form, {1})) {
                ::IsValid(de, {Na, Na, Na, Na, Z, Na, Na, Na, I}, "????01**");
            }
            break;
        case EntityType::kRectangularArraySubfigureInstance:  // fallthrough Type 412~414
        case EntityType::kCircularArraySubfigureInstance:
            ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????????");
            break;
        case EntityType::kExternalReference:
            is_form_valid = IsInRange(form, 0, 4);
            ::IsValid(de, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**????**");
            break;
        case EntityType::kNodalLoadConstraint:
            ::IsValid(de, {Na, Na, Na, ZP, ZP, ZP, Na, Na, I}, "??????**");
            break;
        case EntityType::kNetworkSubfigureInstance:
            ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????????");
            break;
        case EntityType::kAttributeTableInstance:
            is_form_valid = IsInRange(form, 0, 1);
            ::IsValid(de, {P, Na, Na, Na, Na, Na, Na, Na, I}, "**????**");
            break;
        case EntityType::kSolidInstance:
            is_form_valid = IsInRange(form, 0, 1);
            ::IsValid(de, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????????");
            break;
        case EntityType::kVertex:
            is_form_valid = IsInRange(form, {1});
            ::IsValid(de, {Na, Na, IP, Na, Na, ZP, Na, Na, I}, "??01??**");
            break;
        case EntityType::kEdge:
            is_form_valid = IsInRange(form, {1});
            ::IsValid(de, {Na, Na, IP, Na, Na, ZP, Na, Na, I}, "??01??01");
            break;
        case EntityType::kLoop:
            is_form_valid = IsInRange(form, 0, 1);
            ::IsValid(de, {Na, Na, IP, Na, Na, ZP, Na, Na, I}, "??01????");
            break;
        case EntityType::kFace:
            is_form_valid = IsInRange(form, {1});
            ::IsValid(de, {Na, Na, IP, Na, Na, ZP, Na, Na, I}, "??01????");
            break;
        case EntityType::kShell:
            is_form_valid = IsInRange(form, 1, 2);
            ::IsValid(de, {Na, Na, IP, Na, Na, ZP, Na, Na, I}, "????????");
            break;
        default:
            // Unhandled entity type
            throw iio::DataFormatError("Unhandled entity type: " +
                                       std::to_string(static_cast<int>(de.entity_type)));
    }

    // フォーム番号の検証
    if (!is_form_valid) {
        throw iio::DataFormatError(
            "Invalid form number: " + std::to_string(form) +
            " for " + ToString(de.entity_type) + " entity (Type " +
            std::to_string(static_cast<int>(de.entity_type)) + ")");
    }
}



igesio::entities::RawEntityDE igesio::entities::RawEntityDE::ByDefault(
        const EntityType entity_type, const int form_number) {
    constexpr auto Na = DEValueType::kNA;
    constexpr auto I = DEValueType::kInt;
    constexpr auto P = DEValueType::kPtr;
    constexpr auto IP = DEValueType::kIPtr;
    constexpr auto ZP = DEValueType::kZPtr;
    const auto et = entity_type;
    auto fn = form_number;
    const auto error = igesio::DataFormatError(
            "Invalid form number for entity type: " + std::to_string(static_cast<int>(et))
            + ", form number: " + std::to_string(fn));

    // デフォルト値のパラメータを設定
    switch (entity_type) {
        case EntityType::kNull:
            return DefaultDE(et, fn, {Na, Na, Na, Na, Na, Na, Na, Na, Na}, "********");
        case EntityType::kCircularArc:
            return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
        case EntityType::kCompositeCurve:
            return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????????");
        case EntityType::kConicArc:
            if (!IsInRange(fn, 1, 3)) {
                if (fn != 0) throw error;
                fn = 1;  // デフォルト値の場合は暗黙的に1に変更
            }
            return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
        case EntityType::kCopiousData:
            if (IsInRange(fn, 1, 3)) {
                return DefaultDE(et, fn, {Na, Na, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
            } else if (IsInRange(fn, 11, 13) || IsInRange(fn, {63})) {
                return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
            } else if (IsInRange(fn, {20, 21, 40}) || IsInRange(fn, 31, 38)) {
                return DefaultDE(et, fn, {Na, 1, IP, ZP, ZP, ZP, I, IP, I}, "????01**");
            } else if (fn == 0) {
                // デフォルト値の場合は暗黙的に1に変更
                return DefaultDE(et, 1, {Na, Na, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
            }
            throw error;
        case EntityType::kPlane:
            if (!IsInRange(fn, -1, 1)) throw error;
            return DefaultDE(et, fn, {Na, Na, IP, ZP, ZP, ZP, Na, IP, I}, "??????**");
        case EntityType::kLine:
            if (IsInRange(fn, {0})) {
                return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
            } else if (IsInRange(fn, 1, 2)) {
                return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????06**");
            }
            throw error;
        case EntityType::kParametricSplineCurve:  // fallthrough Type 112~114
        case EntityType::kParametricSplineSurface:
            return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
        case EntityType::kPoint:
            return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????????");
        case EntityType::kRuledSurface:  // fallthrough Type 113~115
            if (!IsInRange(fn, 0, 1)) throw error;  // Type 118のみformsが異なる
            [[fallthrough]];
        case EntityType::kSurfaceOfRevolution:
        case EntityType::kTabulatedCylinder:
            return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
        case EntityType::kDirection:
            return DefaultDE(et, fn, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0102**");
        case EntityType::kTransformationMatrix:
            if (!IsInRange(fn, {0, 1}) && !IsInRange(fn, 10, 12)) throw error;
            return DefaultDE(et, fn, {Na, Na, Na, Na, ZP, Na, Na, Na, I}, "****??**");
        case EntityType::kFlash:
            if (!IsInRange(fn, 0, 4)) throw error;
            return DefaultDE(et, fn, {Na, 1, IP, ZP, ZP, ZP, I, IP, I}, "??????00");
        case EntityType::kRationalBSplineCurve:
            if (!IsInRange(fn, 0, 5)) throw error;
            return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
        case EntityType::kRationalBSplineSurface:
            if (!IsInRange(fn, 0, 9)) throw error;
            return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
        case EntityType::kOffsetCurve:
            return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
        case EntityType::kConnectPoint:
            return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????04??");
        case EntityType::kNode:
            return DefaultDE(et, fn, {Na, Na, Na, Na, P, Na, Na, IP, I}, "????04??");
        case EntityType::kFiniteElement:
            return DefaultDE(et, fn, {Na, IP, Na, Na, Na, ZP, Na, IP, I}, "********");
        case EntityType::kNodalDisplacementAndRotation:
            return DefaultDE(et, fn, {Na, Na, Na, Na, Na, ZP, Na, Na, I}, "??????**");
        case EntityType::kOffsetSurface:  // fallthrough Type 140~141
        case EntityType::kBoundary:
            return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "??????**");
        case EntityType::kCurveOnAParametricSurface:  // fallthrough Type 142~144
        case EntityType::kBoundedSurface:
        case EntityType::kTrimmedSurface:
            return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????00**");
        case EntityType::kNodalResults:  // fallthrough Type 146~148
        case EntityType::kElementResults:
            if (!IsInRange(fn, 0, 34)) throw error;
            return DefaultDE(et, fn, {Na, Na, Na, Na, Na, ZP, Na, IP, I}, "**??03**");
        case EntityType::kBlock:  // fallthrough Type 150~158
        case EntityType::kRightAngularWedge:
        case EntityType::kRightCircularCylinder:
        case EntityType::kRightCircularCone:
        case EntityType::kSphere:
            return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????00**");
        case EntityType::kTorus:
            return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "00000000");
        case EntityType::kSolidOfRevolution:  // fallthrough Type 162~168
            if (!IsInRange(fn, 0, 1)) throw error;  // Type 162のみformsが異なる
            [[fallthrough]];
        case EntityType::kSolidOfLinearExtrusion:
        case EntityType::kEllipsoid:
            return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????00**");
        case EntityType::kBooleanTree:
            if (!IsInRange(fn, 0, 1)) throw error;
            return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????00??");
        case EntityType::kSelectedComponent:
            return DefaultDE(et, fn, {Na, Na, IP, ZP, ZP, ZP, Na, IP, I}, "**??03**");
        case EntityType::kSolidAssembly:
            if (!IsInRange(fn, 0, 1)) throw error;
            return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????02??");
        case EntityType::kManifoldSolidBRepObject:
            return DefaultDE(et, fn, {Na, Na, IP, Na, ZP, ZP, Na, Na, I}, "????????");
        case EntityType::kPlaneSurface:
            if (!IsInRange(fn, 0, 1)) throw error;
            return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "**????**");
        case EntityType::kRightCircularCylinderSurface:  // fallthrough Type 192~198
        case EntityType::kRightCircularConicalSurface:
        case EntityType::kSphericalSurface:
        case EntityType::kToroidalSurface:
            if (!IsInRange(fn, 0, 1)) throw error;
            return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "**01??**");
        case EntityType::kAngularDimension:  // fallthrough Type 202~210
        case EntityType::kCurveDimension:
        case EntityType::kDiameterDimension:
        case EntityType::kFlagNote:
        case EntityType::kGeneralLabel:
            return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????01??");
        case EntityType::kGeneralNote:  // fallthrough Type 212~213
            if (!IsInRange(fn, 0, 8) && !IsInRange(fn, {100, 101, 102, 105})) throw error;
            [[fallthrough]];
        case EntityType::kNewGeneralNote:
            return DefaultDE(et, fn, {Na, 1, IP, ZP, ZP, ZP, I, IP, I}, "????01**");
        case EntityType::kLeaderArrow:
            if (!IsInRange(fn, 1, 12)) {
                if (fn != 0) throw error;
                fn = 1;  // デフォルト値の場合は暗黙的に1に変更
            }
            return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????01**");
        case EntityType::kLinearDimension:
            if (!IsInRange(fn, 0, 2)) throw error;
            return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????01??");
        case EntityType::kOrdinateDimension:
            if (!IsInRange(fn, {0, 1})) throw error;
            return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????01??");
        case EntityType::kPointDimension:
            return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????01??");
        case EntityType::kRadiusDimension:
            if (!IsInRange(fn, 0, 1)) throw error;
            return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????01??");
        case EntityType::kGeneralSymbol:
            if (!IsInRange(fn, 0, 3) && !IsInRange(fn, 5001, 9999)) throw error;
            return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????01??");
        case EntityType::kSectionedArea:
            if (!IsInRange(fn, {0, 1})) throw error;
            return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????01??");
        case EntityType::kAssociativityDefinition:
            if (!IsInRange(fn, 5001, 9999)) {
                if (fn != 0) throw error;
                fn = 5001;  // デフォルト値の場合は暗黙的に5001に変更
            }
            return DefaultDE(et, fn, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0002**");
        case EntityType::kLineFontDefinition:
            if (!IsInRange(fn, 1, 2)) {
                if (fn != 0) throw error;
                fn = 1;  // デフォルト値の場合は暗黙的に1に変更
            }
            return DefaultDE(et, fn, {Na, 1, Na, Na, ZP, Na, Na, Na, I}, "**0002**");
        case EntityType::kMacroDefinition:
            return DefaultDE(et, fn, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0002**");
        case EntityType::kSubfigureDefinition:
            return DefaultDE(et, fn, {Na, IP, IP, Na, ZP, ZP, I, IP, I}, "**??02??");
        case EntityType::kTextFont:
            return DefaultDE(et, fn, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0002**");
        case EntityType::kTextDisplayTemplate:
            if (!IsInRange(fn, 0, 1)) throw error;
            return DefaultDE(et, fn, {Na, Na, IP, ZP, ZP, ZP, Na, IP, I}, "??000200");
        case EntityType::kColorDefinition:
            return DefaultDE(et, fn, {Na, Na, Na, Na, Na, Na, Na, 1, I}, "**0002**");
        case EntityType::kUnitsData:
            return DefaultDE(et, fn, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0002**");
        case EntityType::kNetworkSubfigureDefinition:
            return DefaultDE(et, fn, {Na, IP, IP, Na, ZP, ZP, I, IP, I}, "**??02??");
        case EntityType::kAttributeTableDefinition:
            if (!IsInRange(fn, 0, 2)) throw error;
            return DefaultDE(et, fn, {Na, IP, IP, Na, ZP, ZP, I, IP, I}, "**??02??");
        case EntityType::kAssociativityInstance:
            if (IsInRange(fn, {1, 5, 7, 9}) || IsInRange(fn, 12, 15)) {
                return DefaultDE(et, fn, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**????**");
            } else if (IsInRange(fn, {3, 4, 19})) {
                return DefaultDE(et, fn, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0001**");
            } else if (IsInRange(fn, {16})) {
                return DefaultDE(et, fn, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**??05**");
            } else if (IsInRange(fn, {18, 20})) {
                return DefaultDE(et, fn, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**??03**");
            } else if (IsInRange(fn, {21})) {
                return DefaultDE(et, fn, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0102**");
            } else if (fn == 0) {
                // デフォルト値の場合は暗黙的に1に変更
                return DefaultDE(et, 1, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**????**");
            }
            throw error;
        case EntityType::kDrawing:
            if (!IsInRange(fn, 0, 1)) throw error;
            return DefaultDE(et, fn, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0001**");
        case EntityType::kProperty:
            if (IsInRange(fn, 1, 2) || IsInRange(fn, 5, 17) || IsInRange(fn, 18, 25)) {
                return DefaultDE(et, fn, {Na, Na, IP, Na, Na, Na, Na, Na, I}, "**??****");
            } else if (IsInRange(fn, {3})) {
                return DefaultDE(et, fn, {Na, Na, IP, Na, Na, Na, Na, Na, I}, "**00****");
            } else if (IsInRange(fn, {26})) {
                return DefaultDE(et, fn, {Na, Na, P, Na, Na, Na, Na, Na, I}, "**??****");
            } else if (IsInRange(fn, {27, 31})) {
                return DefaultDE(et, fn, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0102**");
            } else if (IsInRange(fn, 28, 30)) {
                return DefaultDE(et, fn, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0202**");
            } else if (IsInRange(fn, 32, 35)) {
                return DefaultDE(et, fn, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**0101**");
            } else if (IsInRange(fn, {36})) {
                return DefaultDE(et, fn, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "00010300");
            } else if (fn == 0) {
                // デフォルト値の場合は暗黙的に1に変更
                return DefaultDE(et, 1, {Na, Na, IP, Na, Na, Na, Na, Na, I}, "**??****");
            }
            throw error;
        case EntityType::kSingularSubfigureInstance:
            return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????????");
        case EntityType::kView:
            if (IsInRange(fn, {0})) {
                return DefaultDE(et, fn, {Na, Na, Na, Na, ZP, Na, Na, Na, I}, "????01**");
            } else if (IsInRange(fn, {1})) {
                return DefaultDE(et, fn, {Na, Na, Na, Na, 0, Na, Na, Na, I}, "????01**");
            }
            throw error;
        case EntityType::kRectangularArraySubfigureInstance:  // fallthrough Type 412~414
        case EntityType::kCircularArraySubfigureInstance:
            return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????????");
        case EntityType::kExternalReference:
            if (!IsInRange(fn, 0, 4)) throw error;
            return DefaultDE(et, fn, {Na, Na, Na, Na, Na, Na, Na, Na, I}, "**????**");
        case EntityType::kNodalLoadConstraint:
            return DefaultDE(et, fn, {Na, Na, Na, ZP, ZP, ZP, Na, Na, I}, "??????**");
        case EntityType::kNetworkSubfigureInstance:
            return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????????");
        case EntityType::kAttributeTableInstance:
            if (!IsInRange(fn, 0, 1)) throw error;
            return DefaultDE(et, fn, {P, Na, Na, Na, Na, Na, Na, Na, I}, "**????**");
        case EntityType::kSolidInstance:
            if (!IsInRange(fn, 0, 1)) throw error;
            return DefaultDE(et, fn, {Na, IP, IP, ZP, ZP, ZP, I, IP, I}, "????????");
        case EntityType::kVertex:
            if (!IsInRange(fn, {1}) && fn != 0) throw error;
            return DefaultDE(et, 1, {Na, Na, IP, Na, Na, ZP, Na, Na, I}, "??01??**");
        case EntityType::kEdge:
            if (!IsInRange(fn, {1}) && fn != 0) throw error;
            return DefaultDE(et, 1, {Na, Na, IP, Na, Na, ZP, Na, Na, I}, "??01??01");
        case EntityType::kLoop:
            if (!IsInRange(fn, 0, 1)) throw error;
            return DefaultDE(et, fn, {Na, Na, IP, Na, Na, ZP, Na, Na, I}, "??01????");
        case EntityType::kFace:
            if (!IsInRange(fn, {1}) && fn != 0) throw error;
            return DefaultDE(et, 1, {Na, Na, IP, Na, Na, ZP, Na, Na, I}, "??01????");
        case EntityType::kShell:
            if (!IsInRange(fn, 1, 2)) {
                if (fn != 0) throw error;
                fn = 1;  // デフォルト値の場合は暗黙的に1に変更
            }
            return DefaultDE(et, fn, {Na, Na, IP, Na, Na, ZP, Na, Na, I}, "????????");
    }

    // エンティティタイプが無効な場合
    throw DataFormatError("Invalid entity type: " +
            std::to_string(static_cast<int>(entity_type)));
}
