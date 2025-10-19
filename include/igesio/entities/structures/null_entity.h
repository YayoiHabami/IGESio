/**
 * @file entities/structures/null_entity.h
 * @brief NullEntity (Type 0) クラスの定義
 * @author Yayoi Habami
 * @date 2025-08-02
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_STRUCTURES_NULL_ENTITY_H_
#define IGESIO_ENTITIES_STRUCTURES_NULL_ENTITY_H_

#include "igesio/entities/entity_base.h"



namespace igesio::entities {

/// @brief Nullエンティティ
/// @note プロセッサによって無視されることを意図したエンティティ.
///       すべてのフィールドには任意の値が設定可能
class NullEntity : public EntityBase {
 protected:
    /// @brief Parameter Dataセクションの、追加ポインタを除いたデータを取得する
    /// @return パラメータデータのベクトル
    IGESParameterVector GetMainPDParameters() const override;

    /// @brief エンティティのPDレコードのパラメータを設定する
    /// @param de2id DEポインターとIDのマッピング
    /// @return 設定したパラメータの終了インデックス
    size_t SetMainPDParameters([[maybe_unused]] const pointer2ID& de2id) override;

 public:
    /// @brief 引数を取らないコンストラクタ
    /// @note デフォルトのDEレコードと空のPDレコードを使用する
    NullEntity();

    /// @brief コンストラクタ
    /// @param de_record DEレコードのパラメータ
    /// @param parameters PDレコードのパラメータ
    /// @param de2id DEポインターとIDのマッピング
    /// @param iges_id 親のIGESDataのID. 指定した場合、エンティティのIDは
    ///        ReservedされたIDを使用する.
    /// @throw igesio::DataFormatError parametersの数が0でない場合
    /// @throw igesio::TypeConversionError parametersの型が不正な場合
    /// @throw std::out_of_range de2idが空でなく、かつparameters側で指定されている
    ///        ポインターの値がde2idに存在しない場合
    /// @throw std::invalid_argument iges_idがUnsetIDではなく、かつ
    ///        de_record.sequence_numberがReservedされていない場合
    NullEntity(const RawEntityDE&, const IGESParameterVector&,
               const pointer2ID& = {}, const ObjectID& = IDGenerator::UnsetID());



    /**
     * EntityBase implementation
     */

    /// @brief PDレコードのパラメータが規格に適合しているかを確認する
    ValidationResult ValidatePD() const override {
        // Nullエンティティは常に有効とみなす
        return ValidationResult::Success();
    }
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_STRUCTURES_NULL_ENTITY_H_
