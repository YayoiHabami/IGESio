/**
 * @file entities/de.h
 * @brief Directory Entry (DE) セクションに関連したクラス・関数群
 * @author Yayoi Habami
 * @date 2025-08-02
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_DE_H_
#define IGESIO_ENTITIES_DE_H_

#include <string>

// IGESファイルとの入出力における中間生成物 (RawEntityDE)
// - (class) EntityStatus: ステータス番号 (DEパラメータ9)を表す構造体
//   - (enum) SubordinateEntitySwitch
//   - (enum) EntityUseFlag
//   - (enum) HierarchyType
// - (class) RawEntityDE: IGESファイルとの入出力における中間生成物
#include "igesio/entities/de/raw_entity_de.h"

// エンティティクラスにおいてDEフィールドの値を格納するクラス
// - (class) DEFieldWrapper: DEStructure ~ DEColorまでの基底クラス
#include "igesio/entities/de/de_field_wrapper.h"

// -> 3rd DE field: Structure
// - (class) DEStructure
#include "igesio/entities/de/de_structure.h"

// -> 4th DE field: Line Font Pattern
// - (enum) LineFontPattern: 線種パターンを表す列挙体
// - (class) DELineFontPattern
#include "igesio/entities/de/de_line_font_pattern.h"

// -> 5th DE field: Level
// - (class) DELevel
#include "igesio/entities/de/de_level.h"

// -> 6th DE field: View
// - (class) DEView
#include "igesio/entities/de/de_view.h"

// -> 7th DE field: Transformation Matrix
// - (class) DETransformationMatrix
#include "igesio/entities/de/de_transformation_matrix.h"

// -> 8th DE field: Label Display Associativity
// - (class) DELabelDisplayAssociativity
#include "igesio/entities/de/de_label_display_associativity.h"

// -> 13th DE field: Color
// - (enum) ColorNumber: 色番号を表す列挙体
// - (class) DEColor
#include "igesio/entities/de/de_color.h"



namespace igesio::entities {

/// @brief DEフィールドの値からDEFieldWrapper継承クラスを作成する
/// @tparam T DEFieldWrapperを継承した型
/// @param value DEフィールドの値
/// @param de2id DEポインターとIDのマッピング (valueが負の値の場合に使用)
/// @return DEFieldWrapper継承クラスのインスタンス
/// @throw std::out_of_range de2idが空でない場合に、valueの絶対値がde2idに存在しない場合
/// @throw igesio::DataFormatError valueが無効な場合 (DELineFontPattern, DELevel,
///        DEColor以外の型で正の値を指定した場合)、valueが負かつde2idが空の場合
template<typename T>
std::enable_if_t<std::is_same_v<T, DEStructure> ||
                 std::is_same_v<T, DELineFontPattern> ||
                 std::is_same_v<T, DELevel> ||
                 std::is_same_v<T, DEView> ||
                 std::is_same_v<T, DETransformationMatrix> ||
                 std::is_same_v<T, DELabelDisplayAssociativity> ||
                 std::is_same_v<T, DEColor>, T>
CreateDEFieldWrapper(const int value, const pointer2ID& de2id) {
    if (value == 0) {
        // デフォルト値の場合はデフォルトコンストラクタを使用
        return T();
    } else if (value < 0) {
        // 負の値の場合はポインタまたはIDとして扱う
        auto p_value = static_cast<unsigned int>(std::abs(value));
        if (de2id.empty()) {
            // de2idが空の場合はエラー (int値をIDに変換できないため)
            throw igesio::DataFormatError("No ID mapping provided for pointer value: "
                                          + std::to_string(p_value));
        } else if (de2id.find(p_value) == de2id.end()) {
            // de2idが空でなく、p_valueがde2idに存在しない場合はエラー
            throw std::out_of_range("Pointer value not found in ID mapping: "
                                    + std::to_string(p_value));
        } else {
            // de2idに存在する場合は、そのIDを使用
            return T(de2id.at(p_value));
        }
    } else {
        // 正の値を取るのは以下の型のみ; それ以外の場合はエラー
        //  -> DELineFontPattern, DELevel, DEColor
        if constexpr (std::is_same_v<T, DELineFontPattern> ||
                      std::is_same_v<T, DELevel> ||
                      std::is_same_v<T, DEColor>) {
            return T(value);
        } else {
            // 厳格にはポインタは負値で表現されるが (Section 2.2.4.4)、
            // 準拠していない出力を行うプロセッサも存在するため、上以外の型で正の値を
            // 指定した場合は、ポインタと解釈する
            return CreateDEFieldWrapper<T>(-value, de2id);
            /*  // TODO: マクロ等で厳格なチェックを行うかを分岐させる
            std::string type_name = typeid(T).name();
            throw igesio::DataFormatError("Invalid positive value for DEFieldWrapper: "
                                          + std::to_string(value) + " (type: " + type_name + ")");
            */
        }
    }
}

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_DE_H_
