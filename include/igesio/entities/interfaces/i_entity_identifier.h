/**
 * @file entities/interfaces/i_entity_identifier.h
 * @brief 全エンティティクラス、およびエンティティ関連の
 *        インターフェースの基底クラス
 * @author Yayoi Habami
 * @date 2025-07-12
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_INTERFACES_I_ENTITY_IDENTIFIER_H_
#define IGESIO_ENTITIES_INTERFACES_I_ENTITY_IDENTIFIER_H_

#include "igesio/common/id_generator.h"
#include "igesio/entities/entity_type.h"



namespace igesio::entities {

/// @brief 全てのエンティティクラス、およびエンティティ関連の
///        インターフェースの基底クラス
/// @note このクラスは、エンティティのID、タイプ、およびフォーム番号を
///       取得するためのインターフェースを提供する.
class IEntityIdentifier {
 public:
    /// @brief デストラクタ
    virtual ~IEntityIdentifier() = default;

    /// @brief エンティティのIDを取得する
    /// @return エンティティのID
    virtual const ObjectID& GetID() const = 0;
    /// @brief エンティティタイプを取得する
    /// @return エンティティタイプ
    virtual EntityType GetType() const = 0;
    /// @brief エンティティのフォーム番号を取得する
    /// @return エンティティのフォーム番号
    virtual int GetFormNumber() const = 0;

    /// @brief 本ライブラリでサポートされているエンティティタイプか
    /// @return UnsupportedEntityのインスタンスを除き`true`を返す
    /// @note UnsupportedEntityを除き、オーバーライドは不要
    virtual bool IsSupported() const { return true; }
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_INTERFACES_I_ENTITY_IDENTIFIER_H_
