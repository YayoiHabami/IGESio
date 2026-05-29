/**
 * @file models/selection_set.h
 * @brief GUI非依存の選択状態を管理するクラス
 * @author Yayoi Habami
 * @date 2026-05-29
 * @copyright 2026 Yayoi Habami
 * @note 選択はCAD/CAMのドメイン状態であり、GLレンダラに依存しない. エンティティ/Assemblyの
 *       ObjectIDを保持し、ヘッドレス(GUIなし)でも操作・単体テストできる. 描画側はこれを参照
 *       (PULL)してハイライトを決める. エンティティ自身には選択フラグを持たせない.
 */
#ifndef IGESIO_MODELS_SELECTION_SET_H_
#define IGESIO_MODELS_SELECTION_SET_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_set>

#include "igesio/common/id_generator.h"



namespace igesio::models {

/// @brief 選択状態を管理する (GUI非依存・ヘッドレス可)
/// @note エンティティIDとAssembly IDのいずれも保持できる. 描画ハイライトや操作対象の
///       入力として用いる. 変更のたびにversionを増やし、GUIは前回値との比較で再描画を
///       判断する (versionは正しさには不要).
class SelectionSet {
 public:
    /// @brief 指定IDを選択に加える
    /// @param id 選択するID
    /// @note 主選択(active)を当該IDへ更新する. 既に選択済みの場合もactiveを更新する.
    void Select(const ObjectID& id);

    /// @brief 指定IDの選択を解除する
    /// @param id 解除するID
    /// @note 選択されていない場合は何もしない. activeが当該IDの場合はactiveを解除する.
    void Deselect(const ObjectID& id);

    /// @brief 指定IDの選択状態を反転する
    /// @param id 対象のID
    void Toggle(const ObjectID& id);

    /// @brief 選択を単一のIDに置き換える
    /// @param id 選択するID
    /// @note 既存選択をすべてクリアしてidのみを選択する.
    void Replace(const ObjectID& id);

    /// @brief すべての選択を解除する
    void Clear();

    /// @brief 指定IDが選択中か
    /// @param id 対象のID
    /// @return 選択中の場合はtrue
    bool Contains(const ObjectID& id) const;

    /// @brief 選択中のID集合を取得する
    /// @return 選択中のIDの集合
    const std::unordered_set<ObjectID>& Items() const { return selected_; }

    /// @brief 選択数を取得する
    std::size_t Size() const { return selected_.size(); }

    /// @brief 選択が空か
    bool Empty() const { return selected_.empty(); }

    /// @brief 主選択(操作の基準. 最後に選んだ要素)を取得する
    /// @return 主選択のID. 無い場合はstd::nullopt
    std::optional<ObjectID> Active() const { return active_; }

    /// @brief 変更検知用のバージョンを取得する
    /// @return 変更のたびに増加する値 (再描画判断用. 正しさには不要)
    std::uint64_t Version() const { return version_; }

 private:
    /// @brief 選択中のID集合
    std::unordered_set<ObjectID> selected_;
    /// @brief 主選択 (最後に選んだ要素. 操作の基準)
    std::optional<ObjectID> active_;
    /// @brief 変更検知用のバージョン
    std::uint64_t version_ = 0;
};

}  // namespace igesio::models

#endif  // IGESIO_MODELS_SELECTION_SET_H_
