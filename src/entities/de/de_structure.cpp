/**
 * @file entities/de/de_structure.cpp
 * @brief 3rd Directory Entryフィールド (Structure) を表すクラス
 * @author Yayoi Habami
 * @date 2025-06-08
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/de/de_structure.h"

#include <memory>

namespace {

namespace i_ent = igesio::entities;

}  // namespace



std::shared_ptr<const i_ent::IStructure> i_ent::DEStructure::GetPointer() const {
    return GetPointer<i_ent::IStructure>();
}
