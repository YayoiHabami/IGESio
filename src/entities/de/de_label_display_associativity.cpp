/**
 * @file entities/de/de_label_display_associativity.cpp
 * @brief 8th Directory Entryフィールド (Label Display Associativity) を表すクラス
 * @author Yayoi Habami
 * @date 2025-06-08
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/de/de_label_display_associativity.h"

#include <memory>

namespace {

namespace i_ent = igesio::entities;
using DELabelDisplayAssociativity = i_ent::DELabelDisplayAssociativity;

}  // namespace



std::shared_ptr<const i_ent::ILabelDisplayAssociativity>
DELabelDisplayAssociativity::GetPointer() const {
    return GetPointer<ILabelDisplayAssociativity>();
}
