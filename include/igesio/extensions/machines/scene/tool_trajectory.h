/**
 * @file extensions/machines/scene/tool_trajectory.h
 * @brief 工具軌跡 (経路上の複数の位置に置く工具モデルのコピー) の表示オブジェクト
 * @author Yayoi Habami
 * @date 2026-09-16
 * @copyright 2026 Yayoi Habami
 * @note 工具のアセンブリ (`MakeToolAssembly`の結果、またはシーンに置かれている
 *       `tool:<n>`) をテンプレートに、その幾何エンティティを共有したまま複数の
 *       同次変換でコピー表示する`InstancedEntity` (inspection拡張) を作る.
 *       切れ刃部+シャンク部とホルダ部は別のエンティティにし、ホルダ部だけを
 *       非表示にできるようにする.
 *       ```text
 *       <name>                 (親に追加するアセンブリ. 切れ刃部+シャンク部のコピー)
 *       └─ holder              (ホルダ部のコピー. ホルダの無い工具では作らない)
 *       ```
 * @note 同次変換の列は、テンプレートのルート座標系 (先端原点、+zが工具軸) →
 *       親アセンブリの座標系の同次変換である. テンプレートのルート自身に設定されて
 *       いる変換 (シーンの`tool:<n>`ではH_tm·T(0,0,-ゲージ長)) は含まれないため,
 *       必要なら呼び出し側が同次変換の列に含めること. 動作サンプル列からの同次変換の
 *       計算は`simulation/motion_scene.h`で行う.
 * @note テンプレートの工具軸線や制御点マーカー (`tool:<n>`の子`axis`/`control_point`)
 *       はコピーに含めない.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_SCENE_TOOL_TRAJECTORY_H_
#define IGESIO_EXTENSIONS_MACHINES_SCENE_TOOL_TRAJECTORY_H_

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/models/assembly.h"
#include "igesio/extensions/inspection/instanced_entity.h"

namespace igesio::extensions::machines {

/// @brief 工具軌跡のホルダ部のアセンブリの名前
constexpr std::string_view kTrajectoryHolderName = "holder";

/// @brief 工具軌跡1グループ分の表示オブジェクト
struct ToolTrajectoryGroup {
    /// @brief 親に追加するアセンブリ (切れ刃部+シャンク部のコピーを直接持つ)
    std::shared_ptr<models::Assembly> assembly;
    /// @brief 切れ刃部+シャンク部のコピー
    std::shared_ptr<inspection::InstancedEntity> body;
    /// @brief ホルダ部のコピー
    /// @note 子`holder`が持つ. ホルダの無い工具では`nullptr`
    std::shared_ptr<inspection::InstancedEntity> holder;

    /// @brief 同次変換の列を変更する
    /// @param placements 新しい同次変換の列
    /// @note 現在の列と同じなら何もしない (形状リビジョンを更新しない)
    void SetPlacements(std::vector<igesio::Matrix4d> placements);

    /// @brief ホルダ部の可視性を設定する
    /// @param visible 表示するなら`true`
    /// @note ホルダの無いグループでは何もしない
    void SetHolderVisible(bool visible);
};

/// @brief 工具のアセンブリをテンプレートに工具軌跡のグループを作る
/// @param tool_template テンプレート (`tool:<n>`のルート)
/// @param placements テンプレートのルート座標系→親アセンブリの座標系の同次変換の列
/// @param name アセンブリの名前
/// @param opacity 不透明度のオーバーライド (省略時はテンプレートの見た目のまま)
/// @return グループ (`assembly`はまだどの親にも属さない)
/// @throw std::invalid_argument テンプレートに切れ刃部とシャンク部の幾何エンティティが
///        無い場合
/// @note テンプレートの幾何エンティティは共有し、コピーしない. 金属材質を設定する
///       場合は`body`のIDをレンダラの材質設定の対象にする
ToolTrajectoryGroup MakeToolTrajectoryGroup(
        const models::Assembly& tool_template,
        std::vector<igesio::Matrix4d> placements, std::string_view name,
        std::optional<float> opacity = std::nullopt);

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_SCENE_TOOL_TRAJECTORY_H_
