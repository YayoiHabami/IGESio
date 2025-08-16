/**
 * @file entities/de/de_label_display_associativity.h
 * @brief 8th DE fieldで参照するLabel Display Associativity Entity
 * @author Yayoi Habami
 * @date 2025-06-08
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_DE_DE_LABEL_DISPLAY_ASSOCIATIVITY_H_
#define IGESIO_ENTITIES_DE_DE_LABEL_DISPLAY_ASSOCIATIVITY_H_

#include <memory>

#include "igesio/entities/interfaces/de_related.h"
#include "igesio/entities/de/de_field_wrapper.h"



namespace igesio::entities {

/// @brief ラベル表示結合性フィールドを表すクラス
/// @note DEフィールド8: Label Display Associativity
///       0はデフォルト、負の値はLabel Display Associativity Entity (Type 402, Form 5)への参照
class DELabelDisplayAssociativity : public DEFieldWrapper<ILabelDisplayAssociativity> {
 public:
    using DEFieldWrapper<ILabelDisplayAssociativity>::DEFieldWrapper;

    using DEFieldWrapper<ILabelDisplayAssociativity>::GetPointer;

    /// @brief 指定された型のポインタを取得
    /// @return 指定された型のポインタ
    std::shared_ptr<const ILabelDisplayAssociativity> GetPointer() const;
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_DE_DE_LABEL_DISPLAY_ASSOCIATIVITY_H_
