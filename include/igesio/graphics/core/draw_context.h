/**
 * @file graphics/core/draw_context.h
 * @brief 描画時に下ろす表示コンテキスト (選択・ハイライト等をPULLする導管)
 * @author Yayoi Habami
 * @date 2026-05-29
 * @copyright 2026 Yayoi Habami
 * @note 描画オブジェクトは選択状態を保持せず、`Draw`時に本コンテキストから参照(PULL)して
 *       ハイライト色を決める. 選択状態は`models::SelectionSet`(GUI非依存)が保持する.
 *       ビューごとに別の`DrawContext`を渡せば、同一モデルを複数ビューで別選択表示できる.
 */
#ifndef IGESIO_GRAPHICS_CORE_DRAW_CONTEXT_H_
#define IGESIO_GRAPHICS_CORE_DRAW_CONTEXT_H_

#include <array>

#include "igesio/common/id_generator.h"
#include "igesio/models/selection_set.h"



namespace igesio::graphics {

/// @brief 描画時に下ろす表示コンテキスト
/// @note GLfloatはfloatのエイリアスのため、色は`std::array<float, 4>`で持つ
///       (gladに依存させない). 描画側の`mainColor` uniformへそのまま渡せる.
struct DrawContext {
    /// @brief 選択集合への参照 (非所有). nullptrの場合はハイライトしない
    const models::SelectionSet* selection = nullptr;
    /// @brief ハイライト色 (RGBA; [0, 1])
    std::array<float, 4> highlight_color = {1.0f, 0.6f, 0.0f, 1.0f};

    /// @brief 指定IDをハイライト表示すべきか
    /// @param id 判定対象のID
    /// @return selectionが当該IDを含む場合はtrue
    bool IsHighlighted(const ObjectID& id) const {
        return selection != nullptr && selection->Contains(id);
    }
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CORE_DRAW_CONTEXT_H_
