/**
 * @file entities/de/de_view.h
 * @brief 6th DE fieldで参照するView Entity (Type 410)
 * @author Yayoi Habami
 * @date 2025-06-08
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_DE_DE_VIEW_H_
#define IGESIO_ENTITIES_DE_DE_VIEW_H_

#include <memory>

#include "igesio/entities/interfaces/de_related.h"
#include "igesio/entities/de/de_field_wrapper.h"



namespace igesio::entities {

/// @brief ビューフィールドを表すクラス
/// @note DEフィールド6: View
///       0は全ビューで可視、負の値はView Entity (Type 410)または
///       Views Visible Associativity Entity (Type 402, Form 3, 4, 19)への参照
class DEView : public DEFieldWrapper<IView, IViewsVisibleAssociativity> {
 public:
    using DEFieldWrapper<IView, IViewsVisibleAssociativity>::DEFieldWrapper;
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_DE_DE_VIEW_H_
