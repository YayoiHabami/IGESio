/**
 * @file models/iges_data.h
 * @brief 一つのIGESファイルに対応するデータ構造を管理するクラス
 * @author Yayoi Habami
 * @date 2025-08-02
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_MODELS_IGES_DATA_H_
#define IGESIO_MODELS_IGES_DATA_H_

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "igesio/common/id_generator.h"
#include "igesio/entities/entity_base.h"



namespace igesio::models {

/// @brief IGESデータクラス
/// @note 一つのIGESファイルに対応するデータ構造を表す.
class IgesData {
 private:
    /// @brief IGESデータのID
    uint64_t id_ = igesio::IDGenerator::Generate();

    /// @brief エンティティのIDとポインタのマッピング
    std::unordered_map<uint64_t, std::shared_ptr<entities::EntityBase>> entities_;

    /// @brief ポインタを設定する
    /// @param[out] entity ポインタを設定するエンティティ
    /// @note entityが未登録のポインタを持ち、かつentities_がそれを持つ場合、
    ///       entityにそのポインタを設定する.
    void SetPointerIfUnset(std::shared_ptr<entities::EntityBase>);



 public:
    /// @brief IgesDataのIDを取得する
    /// @return IgesDataのID
    uint64_t GetID() const { return id_; }

    /// @brief エンティティを追加する
    /// @param entity 追加するエンティティ
    /// @return 追加に成功した場合は、そのエンティティのIDを返す.
    /// @throw std::invalid_argument entityがnullptrの場合
    template<typename T>
    std::enable_if_t<std::is_base_of_v<entities::EntityBase, T>, uint64_t>
    AddEntity(const std::shared_ptr<T> entity) {
        // nullptrチェック
        if (!entity) {
            throw std::invalid_argument("Entity pointer is null");
        }

        // エンティティをマップに追加
        auto id = entity->GetID();
        SetPointerIfUnset(entity);
        entities_[id] = entity;

        return id;
    }

    /// @brief エンティティが参照する全てのエンティティのポインタが設定済みか
    /// @return 一つでも未設定のポインタがある場合は`false`
    /// @note Directory Entry フィールド関連のメンバも含む
    bool AreAllReferencesSet() const;

    /// @brief エンティティが参照する全てのエンティティのうち、
    ///        ポインタが未設定のもののIDを取得する
    /// @return ポインタが未設定のエンティティのIDのリスト
    /// @note Directory Entry フィールド関連のメンバも含む
    std::unordered_set<uint64_t> GetUnresolvedReferences() const;

    /// @brief エンティティのポインタを取得する
    /// @param id エンティティのID
    /// @return 指定されたIDのエンティティのポインタ. 存在しない場合は`nullptr`.
    std::shared_ptr<entities::EntityBase> GetEntity(const uint64_t) const;

    /// @brief エンティティへの参照の取得
    /// @return エンティティのポインタのマップ
    const std::unordered_map<uint64_t, std::shared_ptr<entities::EntityBase>>&
    GetEntities() const { return entities_; }

    /// @brief エンティティの数を取得する
    /// @return エンティティの数
    size_t GetEntityCount() const { return entities_.size(); }



    /// @brief すべてのエンティティの準備ができているかを確認する
    /// @note (1) 自身を含む、すべてのエンティティのポインタが設定済みであること
    ///       (2) すべてのエンティティが有効であること
    /// @return すべてのエンティティが準備できている場合は`true`, そうでない場合は`false`.
    bool IsReady() const;

    /// @brief 本オブジェクトがIGESファイルとして有効かを確認する
    /// @return 検証結果
    /// @note 以下を検証する:
    ///       - 未設定の参照がないこと
    ///       - すべてのエンティティが有効であること
    ValidationResult Validate() const;
};

}  // namespace igesio::models

#endif  // IGESIO_MODELS_IGES_DATA_H_
