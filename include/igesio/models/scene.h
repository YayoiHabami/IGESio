/**
 * @file models/scene.h
 * @brief セッション状態を集約する Scene クラスを定義する
 * @author Yayoi Habami
 * @date 2026-05-30
 * @copyright 2026 Yayoi Habami
 * @note Sceneはroot(モデル) + 選択セット群 + ピックフィルタ等の(a')セッション状態を保持する
 *       権威オブジェクト. GUI/ヘッドレスの双方がここを操作し、描画層(EntityRenderer)はconst
 *       参照でPULLする. IgesData(I/O器)とはrootのshared_ptrを共有する.
 */
#ifndef IGESIO_MODELS_SCENE_H_
#define IGESIO_MODELS_SCENE_H_

#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "igesio/common/id_generator.h"
#include "igesio/models/assembly.h"
#include "igesio/models/selection_set.h"
#include "igesio/models/pick_filter.h"



namespace igesio::models {

/// @brief セッション状態 (root + 選択セット群 + ピックフィルタ) を集約する権威オブジェクト
/// @note 複数ビュー(レンダラ)が同一Sceneを共有でき、非GUIコードも同じものを操作する.
///       選択セットのハンドルは一意なObjectIDを用いる (実Assembly IDとは別整数なので衝突なし).
class Scene {
 public:
    /// @brief コンストラクタ
    /// @param root モデルのルートAssembly (IgesDataと共有)
    /// @note 既定の選択セットを1つ生成し、アクティブにする.
    explicit Scene(std::shared_ptr<Assembly> root);

    /// @brief ルートAssemblyを取得する (非const)
    Assembly& Root();
    /// @brief ルートAssemblyを取得する (const)
    const Assembly& Root() const;
    /// @brief ルートAssemblyのshared_ptrを取得する (共有用)
    std::shared_ptr<Assembly> RootPtr() const;

    /// @brief アクティブな選択セットを取得する (非const; GUI/操作用)
    SelectionSet& ActiveSelection();
    /// @brief アクティブな選択セットを取得する (const; 描画PULL用)
    const SelectionSet& ActiveSelection() const;
    /// @brief アクティブな選択セットのハンドルを取得する
    ObjectID ActiveSelectionId() const;

    /// @brief 新規の(空の)選択セットを作成する
    /// @return 作成したセットのハンドル
    /// @note 作成のみ. アクティブは変更しない.
    ObjectID CreateSelectionSet();
    /// @brief アクティブな選択セットを切り替える
    /// @param id 切り替え先のハンドル
    /// @return 成功した場合はtrue, 当該ハンドルが存在しない場合はfalse
    bool ActivateSelectionSet(const ObjectID& id);
    /// @brief 保持する選択セットのハンドル一覧を取得する (順不同)
    std::vector<ObjectID> SelectionSetIds() const;

    /// @brief ロック・ピックフィルタを尊重して選択する (対話ピック経路用)
    /// @param set 選択を加える対象のセット
    /// @param id 選択するID
    /// @return 選択した場合はtrue. ロック/フィルタで拒否した場合はfalse
    /// @note SelectionSet::Select自体は純粋なまま(ヘッドレスは意図的にロックを無視可).
    ///       v1はエンティティ(body)単位のみ尊重する.
    bool TrySelectWithLock(SelectionSet& set, const ObjectID& id);

    /// @brief ピックフィルタを取得する (非const)
    PickFilter& Filter();
    /// @brief ピックフィルタを取得する (const)
    const PickFilter& Filter() const;

    /// @brief 編集コンテキストを取得する (将来用. 現状は常にnullopt)
    std::optional<ObjectID> ActiveContext() const;

 private:
    /// @brief モデルのルートAssembly (IgesDataと共有)
    std::shared_ptr<Assembly> root_;
    /// @brief ハンドル付きの選択セット群 (現在の作業セット + 保存セット)
    std::unordered_map<ObjectID, SelectionSet> selections_;
    /// @brief 現在アクティブな選択セットのハンドル
    ObjectID active_selection_;
    /// @brief 編集コンテキスト (将来用. いまどのサブアセンブリを編集中か)
    std::optional<ObjectID> active_context_;
    /// @brief セッション既定のピックフィルタ
    PickFilter pick_filter_;
};

}  // namespace igesio::models

#endif  // IGESIO_MODELS_SCENE_H_
