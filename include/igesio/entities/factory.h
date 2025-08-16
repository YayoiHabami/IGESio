/**
 * @file entities/factory.h
 * @brief 各エンティティクラスのファクトリクラス
 * @author Yayoi Habami
 * @date 2025-08-02
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_FACTORY_H_
#define IGESIO_ENTITIES_FACTORY_H_

#include <functional>
#include <memory>
#include <unordered_map>

#include "igesio/common/iges_parameter_vector.h"
#include "igesio/entities/entity_base.h"
#include "igesio/entities/de/de_field_wrapper.h"
#include "igesio/entities/pd.h"



namespace igesio::entities {

/// @brief エンティティのファクトリクラス
/// @note RawEntityDEとIGESParameterVectorからエンティティを作成する場合は、
///       このクラスを使用する。`std::shared_ptr<EntityBase>`を返す。
class EntityFactory {
 private:
    /// @brief エンティティ作成関数
    using CreateFunction = std::function<std::shared_ptr<EntityBase>(
            const RawEntityDE&, const IGESParameterVector&,
            const pointer2ID&, const uint64_t)>;

    /// @brief エンティティタイプと作成関数のマッピング
    inline static std::unordered_map<EntityType, CreateFunction> creators_;

    /// @brief 初期化されたか
    inline static bool initialized_ = false;

    /// @brief エンティティタイプと作成関数のマッピングを初期化する
    static void Initialize();



 public:
    /// @brief エンティティを作成する
    /// @param de DEレコードのパラメータ
    /// @param parameters PDレコードのパラメータ
    /// @param de2id DEポインターとIDのマッピング
    /// @param iges_id 親のIGESDataのID. 指定した場合、エンティティのIDは
    ///        ReservedされたIDを使用する.
    /// @return 作成されたエンティティのポインタ.
    ///         本ライブラリで未対応のエンティティの場合は、UnsupportedEntityの
    ///         ポインタを返す
    /// @throw igesio::DataFormatError parametersの数が正しくない場合
    /// @throw std::bad_variant_access parametersの型が不正な場合
    /// @throw std::out_of_range de2idが空でなく、かつparameters側で指定されている
    ///        ポインターの値がde2idに存在しない場合
    /// @throw std::invalid_argument iges_idがkUnsetIDではなく、かつ
    ///        de_record.sequence_numberがReservedされていない場合
    static std::shared_ptr<EntityBase>
    CreateEntity(const RawEntityDE&, const IGESParameterVector&,
                 const pointer2ID&, const uint64_t = kUnsetID);

    /// @brief エンティティを作成する
    /// @param de DEレコードのパラメータ
    /// @param pd PDレコードのパラメータ
    /// @param de2id DEポインターとIDのマッピング
    /// @param iges_id 親のIGESDataのID. 指定した場合、エンティティのIDは
    ///        ReservedされたIDを使用する.
    /// @return 作成されたエンティティのポインタ.
    ///         本ライブラリで未対応のエンティティの場合は、UnsupportedEntityの
    ///         ポインタを返す
    /// @throw igesio::DataFormatError parametersの数が正しくない場合
    /// @throw std::bad_variant_access parametersの型が不正な場合
    /// @throw std::out_of_range de2idが空でなく、かつparameters側で指定されている
    ///        ポインターの値がde2idに存在しない場合
    /// @throw std::invalid_argument iges_idがkUnsetIDではなく、かつ
    ///        de_record.sequence_numberがReservedされていない場合
    static std::shared_ptr<EntityBase>
    CreateEntity(const RawEntityDE&, const RawEntityPD&,
                 const pointer2ID&, const uint64_t = kUnsetID);
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_FACTORY_H_
