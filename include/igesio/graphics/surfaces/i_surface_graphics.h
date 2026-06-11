/**
 * @file graphics/surfaces/i_surface_graphics.h
 * @brief ISurfaceの描画用クラス
 * @author Yayoi Habami
 * @date 2025-10-10
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_GRAPHICS_SURFACES_I_SURFACE_GRAPHICS_H_
#define IGESIO_GRAPHICS_SURFACES_I_SURFACE_GRAPHICS_H_

#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "igesio/entities/interfaces/i_surface.h"
#include "igesio/graphics/core/entity_graphics.h"
#include "igesio/graphics/core/surface_edge_buffer.h"


namespace igesio::graphics {

/// @brief ISurfaceクラス用の、曲面全般の描画情報の管理クラス
class ISurfaceGraphics
    : public EntityGraphics<entities::ISurface, true> {
    /// @brief 面のVBO
    gl::Uint vbo_ = 0;
    /// @brief 面のEBO
    gl::Uint ebo_ = 0;

    /// @brief 面のinterleaved頂点列 (頂点あたり {x, y, z, nx, ny, nz, u, v})
    std::vector<float> vertices_;
    /// @brief 面のインデックスデータ
    std::vector<gl::Uint> indices_;
    /// @brief 境界エッジ (パラメータ矩形の4アイソ辺) の線分バッファ
    SurfaceEdgeBuffer edge_buffer_;

 public:
    /// @brief コンストラクタ
    /// @param entity 描画するエンティティのポインタ
    /// @param gl OpenGL関数のラッパー
    /// @throw std::invalid_argument entityがnullptrの場合、
    ////       entityがISurfaceを継承していない場合
    explicit ISurfaceGraphics(const std::shared_ptr<const entities::ISurface>&,
                              const std::shared_ptr<IOpenGL>&);

    /// @brief デストラクタ
    ~ISurfaceGraphics() override;

    // コピーコンストラクタとコピー代入演算子を禁止
    ISurfaceGraphics(const ISurfaceGraphics&) = delete;
    ISurfaceGraphics& operator=(const ISurfaceGraphics&) = delete;

    /// @brief 描画可能な状態かどうかを確認する
    bool IsDrawable() const override {
        return EntityGraphics::IsDrawable() &&
               vbo_ != 0 && ebo_ != 0;
    }

    // 基底のDrawオーバーロード (3引数版) を可視に保つ
    using EntityGraphics::Draw;

    /// @brief エンティティの描画を行う (シェーダー型で分岐)
    /// @note kSurfaceEdgeでは境界エッジを線描画し、それ以外は基底に委譲する
    void Draw(gl::Uint shader, const ShaderId shader_id,
              const std::pair<float, float>& viewport,
              const DrawContext& ctx) const override {
        if (shader_id == ShaderId::kSurfaceEdge) {
            if (edge_buffer_.IsEmpty()) return;
            const bool highlighted = ctx.IsHighlighted(GetEntityID());
            const auto& color = highlighted
                    ? ctx.highlight_color : kSurfaceEdgeColor;
            edge_buffer_.DrawWithState(shader, GetWorldTransform(),
                                       color, GetLineWidth(), highlighted);
            return;
        }
        EntityGraphics::Draw(shader, shader_id, viewport, ctx);
    }

    /// @brief 全ての可能なシェーダータイプを取得する
    /// @note 面シェーダーに加え、境界エッジ用のkSurfaceEdgeを常に含める。
    ///       描画バケットは同期前 (edge_buffer_未構築) のreconcile段で構築されるため、
    ///       構築状態に依存させてはならない (実際に空ならDrawがIsEmptyで早期return)。
    std::unordered_set<ShaderId> GetShaderIds() const override {
        auto types = EntityGraphics::GetShaderIds();
        types.insert(ShaderId::kSurfaceEdge);
        return types;
    }

    /// @brief エンティティをセットアップする
    /// @note 内部で参照するエンティティの状態に基づいて、
    ///       描画用のリソースを再セットアップする
    void DoSynchronize() override;

    /// @brief OpenGLリソースを解放する
    /// @note SSBO等のOpenGLリソースを解放する
    void Cleanup() override;

 protected:
    /// @brief エンティティの描画を行う
    /// @param shader プログラムシェーダーのID
    /// @param viewport ビューポートのサイズ (width, height)
    void DrawImpl(gl::Uint, const std::pair<float, float>&) const override;

    /// @brief 描画用の頂点/法線データとインデックスデータを生成する
    void GenerateSurfaceData();
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_SURFACES_I_SURFACE_GRAPHICS_H_
