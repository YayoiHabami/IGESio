/**
 * @file entities/ientity.h
 * @brief 個別エンティティクラスの基底クラス (型消去版)
 * @author Yayoi Habami
 * @date 2025-06-08
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_ENTITIES_IENTITY_H_
#define IGESIO_ENTITIES_IENTITY_H_

#include <memory>
#include <string>
#include <vector>

#include "igesio/common/iges_parameter_vector.h"
#include "igesio/entities/entity_type.h"
#include "igesio/entities/directory_entry_param.h"



namespace igesio::entities {

/// @brief 全てのエンティティクラスの基底クラス
struct IEntity {
    /// @brief デストラクタ
    virtual ~IEntity() = default;

    /**
     * DEセクション関係: 列挙体やプリミティブ型を返すもの
     *
     * See entity_base_class_architecture_ja.md
     */

    /// @brief エンティティのIDを取得する
    /// @return エンティティのID
    virtual uint64_t GetID() const = 0;

    /// @brief エンティティタイプを取得する
    /// @return エンティティタイプ
    virtual EntityType GetType() const = 0;

    /// @brief エンティティのフォーム番号を取得する
    /// @return エンティティのフォーム番号
    virtual unsigned int GetFormNumber() const { return 0; }

    /// @brief エンティティのステータス (9th DE field) を取得する
    /// @return エンティティのステータス (Status Number)
    /// @note セッターは存在しない (blank status以外は他のエンティティとの
    ///       関係の中で決定されるため)
    virtual EntityStatus GetEntityStatus() const = 0;

    /// @brief エンティティの表示状態 (blank status) を取得する
    /// @return 表示状態 (true: visible, false: invisible)
    virtual bool GetBlankStatus() const = 0;
    /// @brief エンティティの表示状態 (blank status) を設定する
    /// @param status 表示状態 (true: visible, false: invisible)
    virtual void SetBlankStatus(const bool) = 0;

    /// @brief エンティティの従属状態 (subordinate entity switch) を取得する
    /// @return 従属状態
    virtual SubordinateEntitySwitch GetSubordinateEntitySwitch() const = 0;
    // SubordinateEntitySwitchのセッターは定義不可.

    /// @brief エンティティの用途フラグ (entity use flag) を取得する
    /// @return 用途フラグ
    virtual EntityUseFlag GetEntityUseFlag() const = 0;

    /// @brief エンティティの階層 (hierarchy) を取得する
    /// @return 階層
    virtual HierarchyType GetHierarchy() const = 0;

    /// @brief エンティティラベル (18th DE field) を取得する
    /// @return エンティティラベル
    virtual std::string GetEntityLabel() const = 0;
    /// @brief エンティティラベル (18th DE field) を設定する
    /// @param label エンティティラベル
    /// @throw igesio::DataFormatError ラベルが8文字を超過する場合
    virtual void SetEntityLabel(const std::string&) = 0;

    /// @brief エンティティの添字番号 (19th DE field) を取得する
    /// @return エンティティの添字番号
    virtual int GetEntitySubscript() const = 0;
    /// @brief エンティティの添字番号 (19th DE field) を設定する
    /// @param subscript エンティティの添字番号
    virtual void SetEntitySubscript(const int) = 0;



    /**
     * PDセクションでの参照に関連したメンバ変数
     *
     * See entity_base_class_architecture_ja.md
     */

    /// @brief 子要素 (物理的に従属するエンティティ) のIDを取得する
    /// @return 子要素のIDのリスト
    virtual std::vector<uint64_t> GetChildIDs() const { return {}; }
    /// @brief 子要素 (物理的に従属するエンティティ) のポインタを取得する
    /// @return 子要素のポインタ (変更不可)、存在しない場合はnullptr
    virtual std::shared_ptr<const IEntity>
    GetChild([[maybe_unused]] const uint64_t id) const { return nullptr; }

    /// @brief このエンティティが参照する全てのエンティティのIDを取得する
    /// @return 参照するエンティティのIDのリスト
    virtual std::vector<uint64_t> GetPointerIDs() const { return {}; }
    /// @brief エンティティが参照するすべてのエンティティのポインタが設定済みか
    /// @return 一つでもポインタが未設定のものがあればfalse
    virtual bool AreAllPointersSet() const { return true; }
    /// @brief ポインタが未設定のエンティティのIDを取得する
    /// @return ポインタが未設定のエンティティのIDのリスト
    virtual std::vector<uint64_t> GetUnsetPointerIDs() const { return {}; }
    /// @brief ポインタを設定する
    /// @param entity 設定する参照先のエンティティ
    /// @return ポインタの設定に成功した場合はtrue、失敗した場合はfalse
    virtual bool SetPointer([[maybe_unused]] const std::shared_ptr<IEntity>& entity) {
        return false;  // デフォルトではポインタを設定しない
    }



    /**
     * 構造検証用
     *
     * See entity_base_class_architecture_ja.md
     */

    /// @brief エンティティのパラメータが仕様に従っているかを検証する
    /// @return 構造検証結果 (true: valid, false: invalid)
    virtual bool Validate() const { return true; }



    /**
     * その他細かいメンバ関数
     *
     * See entity_base_class_architecture_ja.md
     */

    /// @brief エンティティのパラメータをベクトルとして取得する
    /// @return エンティティのパラメータを格納したIGESParameterVector.
    /// @note 戻り値は、Parameter Dataセクションで定義されるパラメータのうち、
    ///       1つ目のパラメータ (Entity Type) を除いたものを格納する。
    virtual IGESParameterVector GetParameters() const = 0;
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_IENTITY_H_
