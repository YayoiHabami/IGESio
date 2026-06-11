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

/// @brief 表示モード (面と面エッジの描画組み合わせ)
/// @note 従属しない曲線エンティティはいずれのモードでも常に描画される.
enum class DisplayMode {
    /// @brief 面と面エッジの両方を描画する
    kShaded,
    /// @brief 面エッジのみを描画する (面の塗りつぶしを描画しない)
    kWireFrame,
    /// @brief 面のみを描画する (面エッジを描画しない)
    kNoEdge
};

/// @brief 描画時に下ろす表示コンテキスト
/// @note GLfloatはfloatのエイリアスのため、色は`std::array<float, 4>`で持つ
///       (gladに依存させない). 描画側の`mainColor` uniformへそのまま渡せる.
struct DrawContext {
    /// @brief 選択集合への参照 (非所有). nullptrの場合はハイライトしない
    const models::SelectionSet* selection = nullptr;
    /// @brief ハイライト色 (RGBA; [0, 1])
    std::array<float, 4> highlight_color = {1.0f, 0.6f, 0.0f, 1.0f};
    /// @brief 親(複合/委譲ノード)が選択中で、子へハイライトを強制するか
    /// @note 複合ノードは描画を子(別ID)へ委譲するため、親が選択された場合は本フラグを
    ///       立てた複製コンテキストを子へ渡し、子のID判定に依らずハイライトさせる.
    bool force_highlight = false;
    /// @brief 表示モード (面/面エッジの描画組み合わせ)
    /// @note メッシュ系グラフィックスがkSurfaceEdge描画時のエッジ集合
    ///       (全エッジ/特徴エッジ) の選択に使用する
    DisplayMode display_mode = DisplayMode::kShaded;

    /// @brief 指定IDをハイライト表示すべきか
    /// @param id 判定対象のID
    /// @return force_highlightが立つ、またはselectionが当該IDを含む場合はtrue
    bool IsHighlighted(const ObjectID& id) const {
        return force_highlight
            || (selection != nullptr && selection->Contains(id));
    }
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CORE_DRAW_CONTEXT_H_
