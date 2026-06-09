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

#include <cstdint>

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
    /// @brief 元エンティティのIDを取得する
    /// @return 通常のエンティティでは`GetID()`と同じ値.
    ///         ビュー(CurveView/SurfaceView)では元エンティティのID
    /// @note ピッキング結果(ビュー)から元エンティティを特定するために用いる.
    ///       ビュー等を除き、オーバーライドは不要
    virtual const ObjectID& GetSourceID() const { return GetID(); }
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

    /// @brief ジオメトリリビジョンを取得する
    /// @return 形状定義の変更毎に単調増加する値
    /// @note 描画層が再テッセレーション (Synchronize) の要否判定に用いる.
    ///       既定実装は0 (形状を持たない実装用). 実装は`EntityBase`が一元化し、
    ///       ビュー(CurveView/SurfaceView)は元エンティティへ転送する
    virtual uint64_t GeometryRevision() const { return 0; }
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_INTERFACES_I_ENTITY_IDENTIFIER_H_
