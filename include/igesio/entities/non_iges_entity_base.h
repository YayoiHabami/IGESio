/**
 * @file entities/non_iges_entity_base.h
 * @brief 非IGESエンティティ用の基底クラス
 * @author Yayoi Habami
 * @date 2026-06-10
 * @copyright 2026 Yayoi Habami
 * @note IGESへシリアライズされない計算・描画専用エンティティ (メッシュ・点群等の
 *       ユーザー定義型) を定義する際の基底クラス. 固有IDの管理 (採番・解放) と
 *       ジオメトリリビジョンの実装を肩代わりする.
 */
#ifndef IGESIO_ENTITIES_NON_IGES_ENTITY_BASE_H_
#define IGESIO_ENTITIES_NON_IGES_ENTITY_BASE_H_

#include <cstdint>

#include "igesio/common/id_generator.h"
#include "igesio/entities/entity_type.h"
#include "igesio/entities/interfaces/i_entity_identifier.h"



namespace igesio::entities {

/// @brief 非IGESエンティティ用の基底クラス
/// @note IGESファイルへ入出力しないエンティティ (EntityType::kNonIges) の
///       共通実装. 派生クラスは必要な能力インターフェース (`ICurve`/`ISurface`/
///       `IGeometry`等. いずれも`IEntityIdentifier`を仮想継承するため菱形継承は
///       共有される) を併せて実装することで、Assemblyへの保存と計算
///       パイプラインに乗る. IGES出力 (`WriteIges`) からはスキップされる.
/// @note 型同士の識別 (メッシュ/点群等) はEntityType列挙型ではなくC++の
///       型システムで行う. `GetType()`は常に`kNonIges`を返す.
/// @note 形状を編集した場合は派生クラスのmutator末尾で`MarkGeometryModified()`を
///       呼ぶこと (描画層が再テッセレーションの要否判定に用いる).
/// @note 固有IDの二重解放を避けるため、コピー不可・ムーブ可とする
///       (CurveView/SurfaceViewと同じ規約).
class NonIgesEntityBase : public virtual IEntityIdentifier {
 public:
    /// @brief デストラクタ
    /// @note 固有のint型IDを解放する
    ~NonIgesEntityBase() override {
        IDGenerator::Release(id_.ToInt());
    }

    /// @brief コピーコンストラクタ(削除)
    NonIgesEntityBase(const NonIgesEntityBase&) = delete;
    /// @brief コピー代入演算子(削除)
    NonIgesEntityBase& operator=(const NonIgesEntityBase&) = delete;
    /// @brief ムーブコンストラクタ
    /// @note ObjectIDの内部shared_ptrがムーブされ、ムーブ元は未設定IDとなる
    ///       (二重解放は起こらない)
    NonIgesEntityBase(NonIgesEntityBase&&) noexcept = default;
    /// @brief ムーブ代入演算子
    NonIgesEntityBase& operator=(NonIgesEntityBase&&) noexcept = default;

    /// @brief エンティティのIDを取得する
    const ObjectID& GetID() const override { return id_; }

    /// @brief エンティティタイプを取得する
    /// @return 常に`EntityType::kNonIges`
    EntityType GetType() const final { return EntityType::kNonIges; }

    /// @brief エンティティのフォーム番号を取得する
    /// @return 0 (非IGESエンティティにフォームの概念はない)
    int GetFormNumber() const override { return 0; }

    /// @brief ジオメトリリビジョンを取得する
    /// @return 形状定義の変更毎に単調増加する値
    uint64_t GeometryRevision() const override { return geometry_revision_; }

 protected:
    /// @brief コンストラクタ
    /// @note 固有のID (ObjectType::kNonIgesEntity) を採番する
    NonIgesEntityBase()
        : id_(IDGenerator::Generate(
                  ObjectType::kNonIgesEntity,
                  static_cast<uint16_t>(EntityType::kNonIges))) {}

    /// @brief ジオメトリリビジョンをインクリメントする
    /// @note 形状に影響するmutatorの末尾 (成功経路のみ) で呼ぶこと (規約).
    ///       呼び忘れは「編集が描画へ反映されない」として顕在化する.
    void MarkGeometryModified() { ++geometry_revision_; }

 private:
    /// @brief エンティティ固有のID (ObjectType::kNonIgesEntity)
    ObjectID id_;

    /// @brief ジオメトリリビジョン (形状編集毎にインクリメント)
    uint64_t geometry_revision_ = 0;
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_NON_IGES_ENTITY_BASE_H_
