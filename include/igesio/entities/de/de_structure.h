/**
 * @file entities/de/de_structure.h
 * @brief 3rd Directory Entryフィールド (Structure) を表すクラス
 * @author Yayoi Habami
 * @date 2025-06-08
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_DE_DE_STRUCTURE_H_
#define IGESIO_ENTITIES_DE_DE_STRUCTURE_H_

#include <memory>

#include "igesio/entities/interfaces/de_related.h"
#include "igesio/entities/de/de_field_wrapper.h"



namespace igesio::entities {

/// @brief 構造定義フィールドを表すクラス
/// @note DEフィールド3: Structure
///       負の値の場合、絶対値がこのエンティティタイプ番号の
///       スキーマを指定する構造定義エンティティを参照する
/// @todo IStructureを継承する形でよいかは要検討
class DEStructure : public DEFieldWrapper<IStructure> {
 public:
    using DEFieldWrapper<IStructure>::DEFieldWrapper;

    using DEFieldWrapper<IStructure>::GetPointer;

    /// @brief 指定された型のポインタを取得
    /// @return 指定された型のポインタ
    std::shared_ptr<const IStructure> GetPointer() const;
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_DE_DE_STRUCTURE_H_
