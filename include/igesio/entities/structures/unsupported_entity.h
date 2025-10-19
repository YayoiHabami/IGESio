/**
 * @file entities/structures/unsupported_entity.h
 * @brief UnsupportedEntityクラスの定義
 * @author Yayoi Habami
 * @date 2025-08-14
 * @copyright 2025 Yayoi Habami
 * @note このクラスは、本ライブラリでサポートされていない、または
 *       対応するクラスの実装がないエンティティを表す. 入力されたデータを
 *       そのまま保持し、再度出力可能にすることを目的とする.
 */
#ifndef IGESIO_ENTITIES_STRUCTURES_UNSUPPORTED_ENTITY_H_
#define IGESIO_ENTITIES_STRUCTURES_UNSUPPORTED_ENTITY_H_

#include "igesio/entities/entity_base.h"



namespace igesio::entities {

/// @brief 本ライブラリで未サポートのエンティティ
/// @note このクラスは、本ライブラリで未サポートのエンティティを読み込んだ際に
///       プログラム上で保持するために使用される. 具体的な処理は実装せず
///       入力されたデータをそのまま保持し、再度出力可能にすることを目的とする.
/// @note EntityBase継承クラスでは、一度設定したEntityTypeを変更することはできないため
///       デフォルトコンストラクタは基本的に使用しないこと. デフォルトコンストラクタでは
///       フォーム番号0のkNullとしてオブジェクトを作成する.
class UnsupportedEntity : public EntityBase {
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
    /// @note フォーム番号0のkNullエンティティとしてオブジェクトを作成する.
    ///       異なるフォーム番号のUnsupportedEntityを作成する場合は、別の
    ///       コンストラクタを使用すること.
    UnsupportedEntity();

    /// @brief コンストラクタ (デフォルトのDEレコードを使用)
    /// @param entity_type エンティティのタイプ
    /// @param parameters PDレコードのパラメータ
    /// @param de2id DEポインターとIDのマッピング
    /// @throw igesio::DataFormatError parametersのいずれかが正しくない場合
    /// @throw igesio::TypeConversionError parametersの型が不正な場合
    /// @throw std::out_of_range de2idが空でなく、かつparameters側で指定されている
    ///        ポインターの値がde2idに存在しない場合
    /// @note de2idを空のままにした場合、parameters側で指定されている
    ///       ポインター (int型) はそのままIDとして使用される.
    UnsupportedEntity(const EntityType, const IGESParameterVector,
                      const pointer2ID& de2id = {});

    /// @brief コンストラクタ
    /// @param de_record DEレコードのパラメータ
    /// @param parameters PDレコードのパラメータ
    /// @param de2id DEポインターとIDのマッピング
    /// @param iges_id 親のIGESDataのID. 指定した場合、エンティティのIDは
    ///        ReservedされたIDを使用する.
    /// @throw igesio::DataFormatError parametersのいずれかが正しくない場合
    /// @throw igesio::TypeConversionError parametersの型が不正な場合
    /// @throw std::out_of_range de2idが空でなく、かつparameters側で指定されている
    ///        ポインターの値がde2idに存在しない場合
    /// @throw std::invalid_argument iges_idがUnsetIDではなく、かつ
    ///        de_record.sequence_numberがReservedされていない場合
    UnsupportedEntity(const RawEntityDE&, const IGESParameterVector&,
                      const pointer2ID& = {}, const ObjectID& = IDGenerator::UnsetID());

    /// @brief 本ライブラリでサポートされているエンティティタイプか
    /// @return `false`
    bool IsSupported() const override { return false; }



    /**
     * EntityBase implementation
     */

    /// @brief PDレコードのパラメータが規格に適合しているかを確認する
    ValidationResult ValidatePD() const override {
        // UnsupportedEntityは常に有効とみなす
        return ValidationResult::Success();
    }
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_STRUCTURES_UNSUPPORTED_ENTITY_H_
