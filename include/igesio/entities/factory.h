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
 public:
    /// @brief エンティティ作成関数
    /// @note 引数は順に、DEレコード・PDレコードのパラメータ・DEポインターとIDの
    ///       マッピング・親IGESDataのID (`CreateEntity`の引数と同順).
    using CreateFunction = std::function<std::shared_ptr<EntityBase>(
            const RawEntityDE&, const IGESParameterVector&,
            const pointer2ID&, const ObjectID&)>;

 private:
    /// @brief 組み込みエンティティ作成関数のマッピングを取得する
    /// @return エンティティタイプと作成関数のマッピング (組み込み実装)
    /// @note 初回呼び出し時に全組み込み実装を登録する (関数内static).
    ///       静的初期化子からの登録・参照 (自動登録) でも初期化順に依存せず安全
    static std::unordered_map<EntityType, CreateFunction>& BuiltinCreators();

    /// @brief ユーザー登録のエンティティ作成関数のマッピングを取得する
    /// @return type番号と作成関数のマッピング (ユーザー登録)
    /// @note キーはエンティティのtype番号. `RegisterEntityCreator`/
    ///       `RegisterUserEntityCreator`で登録される. 組み込みとは分離し、
    ///       登録・解除が組み込み実装へ影響しないようにする. キーを`int`と
    ///       するのは、enum定義済み番号とユーザー定義番号 (enum外) の登録で
    ///       同一マップを共用するため
    static std::unordered_map<int, CreateFunction>& UserCreators();



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
    /// @throw igesio::EntityDataError parametersの数や値が正しくない場合
    /// @throw igesio::ReferenceError 参照先エンティティの解決に失敗した場合
    /// @throw std::bad_variant_access parametersの型が不正な場合
    /// @throw std::out_of_range de2idが空でなく、かつparameters側で指定されている
    ///        ポインターの値がde2idに存在しない場合
    /// @throw std::invalid_argument iges_idがUnsetIDではなく、かつ
    ///        de_record.sequence_numberがReservedされていない場合
    static std::shared_ptr<EntityBase>
    CreateEntity(const RawEntityDE&, const IGESParameterVector&,
                 const pointer2ID&, const ObjectID& = IDGenerator::UnsetID());

    /// @brief エンティティを作成する
    /// @param de DEレコードのパラメータ
    /// @param pd PDレコードのパラメータ
    /// @param de2id DEポインターとIDのマッピング
    /// @param iges_id 親のIGESDataのID. 指定した場合、エンティティのIDは
    ///        ReservedされたIDを使用する.
    /// @return 作成されたエンティティのポインタ.
    ///         本ライブラリで未対応のエンティティの場合は、UnsupportedEntityの
    ///         ポインタを返す
    /// @throw igesio::EntityDataError parametersの数や値が正しくない場合
    /// @throw igesio::ReferenceError 参照先エンティティの解決に失敗した場合
    /// @throw std::bad_variant_access parametersの型が不正な場合
    /// @throw std::out_of_range de2idが空でなく、かつparameters側で指定されている
    ///        ポインターの値がde2idに存在しない場合
    /// @throw std::invalid_argument iges_idがUnsetIDではなく、かつ
    ///        de_record.sequence_numberがReservedされていない場合
    static std::shared_ptr<EntityBase>
    CreateEntity(const RawEntityDE&, const RawEntityPD&,
                 const pointer2ID&, const ObjectID& = IDGenerator::UnsetID());

    /// @brief エンティティ作成関数を登録する
    /// @param type 対象のエンティティタイプ
    /// @param creator 作成関数
    /// @throw std::invalid_argument creatorが空の場合、typeが有効なIGESの
    ///        type番号でない場合、またはtypeの作成関数が登録済み
    ///        (組み込み実装を含む) の場合
    /// @note 組み込み実装の上書きは許可しない (本ライブラリ対応済みのtypeは
    ///       登録できない). フォーム番号による分岐はcreator内部で行うこと.
    /// @note スレッド安全性は保証しない. 登録はファイル読み込みの開始前に
    ///       完了させること
    static void RegisterEntityCreator(EntityType, CreateFunction);

    /// @brief ユーザー登録のエンティティ作成関数を解除する
    /// @param type 対象のエンティティタイプ
    /// @return 解除した場合はtrue. 未登録、または組み込み実装のtypeを
    ///         指定した場合はfalse
    /// @note 組み込み実装は解除できない
    static bool UnregisterEntityCreator(EntityType);

    /// @brief 指定タイプのエンティティ作成関数が存在するか
    /// @param type 対象のエンティティタイプ
    /// @return 組み込み実装またはユーザー登録の作成関数が存在する場合はtrue
    static bool IsEntityCreatorRegistered(EntityType);

    /// @brief ユーザー定義番号のエンティティ作成関数を登録する
    /// @param type_number ユーザー定義のtype番号 (600-699, 10000-99999)
    /// @param creator 作成関数
    /// @throw std::invalid_argument creatorが空の場合、type_numberがユーザー
    ///        定義番号でない場合、またはその番号の作成関数が登録済みの場合
    /// @note 登録した番号のエンティティは読み込み時にこの関数で生成される
    ///       (未登録のユーザー定義番号はUnsupportedEntityとして保持される).
    ///       作成関数へ渡されるDEレコードはentity_typeがkUserDefinedであり、
    ///       実番号はuser_type_numberに設定されている.
    /// @note スレッド安全性は保証しない. 登録はファイル読み込みの開始前に
    ///       完了させること
    static void RegisterUserEntityCreator(int, CreateFunction);

    /// @brief ユーザー定義番号のエンティティ作成関数を解除する
    /// @param type_number ユーザー定義のtype番号
    /// @return 解除した場合はtrue. 未登録の場合はfalse
    static bool UnregisterUserEntityCreator(int);

    /// @brief ユーザー定義番号のエンティティ作成関数が登録済みか
    /// @param type_number ユーザー定義のtype番号
    /// @return ユーザー登録の作成関数が存在する場合はtrue
    static bool IsUserEntityCreatorRegistered(int);
};

/// @brief エンティティを複製する
/// @param entity 複製元のエンティティ
/// @return 新しい`kEntityNew` IDを持つ複製. 参照(子・変換124・色314等)は複製元と同じ
///         エンティティを共有する(浅い複製)
/// @note DE/PDへ一旦シリアライズし、`EntityFactory::CreateEntity`で再構築することで複製する.
///       `EntityBase::id_`がconstのためコピー後のID振り直しができないことへの対処.
///       本ライブラリ未対応のエンティティは`UnsupportedEntity`として複製される.
std::shared_ptr<EntityBase> CloneEntity(const EntityBase&);

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_FACTORY_H_
