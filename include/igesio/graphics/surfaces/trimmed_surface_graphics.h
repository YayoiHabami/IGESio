/**
 * @file graphics/surfaces/trimmed_surface_graphics.h
 * @brief TrimmedSurfaceの描画用クラス
 * @author Yayoi Habami
 * @date 2026-04-13
 * @copyright 2026 Yayoi Habami
 */
#ifndef IGESIO_GRAPHICS_SURFACES_TRIMMED_SURFACE_GRAPHICS_H_
#define IGESIO_GRAPHICS_SURFACES_TRIMMED_SURFACE_GRAPHICS_H_

#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "igesio/numerics/matrix.h"
#include "igesio/entities/surfaces/trimmed_surface.h"
#include "igesio/graphics/core/entity_graphics.h"
#include "igesio/graphics/core/surface_edge_buffer.h"



namespace igesio::graphics {

/// @brief TrimmedSurfaceエンティティの描画情報の管理クラス
/// @note メッシュ生成は制限付き曲面(143/144)共通のテッセレーション
///       (entities::TessellateRestrictedSurface) に委譲する。境界駆動の
///       制限付き四分木により、トリム境界近傍を適応的に細分する。
class TrimmedSurfaceGraphics
    : public EntityGraphics<entities::TrimmedSurface, true> {
    /// @brief 面のVBO
    gl::Uint vbo_ = 0;
    /// @brief 面のEBO
    gl::Uint ebo_ = 0;

    /// @brief 面の頂点・法線・テクスチャ座標データ
    /// @note 各列が {x, y, z, nx, ny, nz, tu, tv} の形式
    MatrixXf vertices_;
    /// @brief 面のインデックスデータ
    std::vector<gl::Uint> indices_;
    /// @brief 境界エッジ (外周/内周トリム境界) の線分バッファ
    SurfaceEdgeBuffer edge_buffer_;

 public:
    /// @brief コンストラクタ
    /// @param entity 描画するエンティティのポインタ
    /// @param gl OpenGL関数のラッパー
    explicit TrimmedSurfaceGraphics(
            const std::shared_ptr<const entities::TrimmedSurface>,
            const std::shared_ptr<IOpenGL>);

    /// @brief デストラクタ
    ~TrimmedSurfaceGraphics() override;

    // コピーコンストラクタとコピー代入演算子を禁止
    TrimmedSurfaceGraphics(const TrimmedSurfaceGraphics&) = delete;
    TrimmedSurfaceGraphics& operator=(const TrimmedSurfaceGraphics&) = delete;

    /// @brief 描画可能な状態かどうかを確認する
    bool IsDrawable() const override {
        return EntityGraphics::IsDrawable() && vbo_ != 0 && ebo_ != 0;
    }

    // 基底のDrawオーバーロード (3引数版) を可視に保つ
    using EntityGraphics::Draw;

    /// @brief エンティティの描画を行う (シェーダー型で分岐)
    /// @note kSurfaceEdgeでは境界エッジを線描画し、それ以外は基底に委譲する
    void Draw(gl::Uint shader, const ShaderType shader_type,
              const std::pair<float, float>& viewport,
              const DrawContext& ctx) const override {
        if (shader_type == ShaderType::kSurfaceEdge) {
            if (edge_buffer_.IsEmpty()) return;
            const auto color = ctx.IsHighlighted(GetEntityID())
                    ? ctx.highlight_color : kSurfaceEdgeColor;
            edge_buffer_.DrawWithState(shader, GetWorldTransform(),
                                       color, GetLineWidth());
            return;
        }
        EntityGraphics::Draw(shader, shader_type, viewport, ctx);
    }

    /// @brief 全ての可能なシェーダータイプを取得する
    /// @note 面シェーダーに加え、エッジがあればkSurfaceEdgeを含める
    std::unordered_set<ShaderType> GetShaderTypes() const override {
        auto types = EntityGraphics::GetShaderTypes();
        if (!edge_buffer_.IsEmpty()) types.insert(ShaderType::kSurfaceEdge);
        return types;
    }

    /// @brief エンティティをセットアップする
    /// @note 内部で参照するエンティティの状態に基づいて、
    ///       描画用のリソースを再セットアップする
    void Synchronize() override;

    /// @brief OpenGLリソースを解放する
    void Cleanup() override;

    /// @brief 範囲選択用にトリム面をサンプリングする
    /// @param params サンプリング制御パラメータ
    /// @return トリム境界(外周/内周)の閉ループ折れ線と、トリム領域内の内部グリッド点
    /// @note 汎用の矩形境界では未トリム領域まで含み過剰選択となるため、
    ///       トリム境界曲線(Type 142)を ICurve としてサンプリングする
    SelectionSamples GetSelectionSamples(
            const SelectionSampleParams&) const override;

 protected:
    /// @brief エンティティの描画を行う
    /// @param shader プログラムシェーダーのID
    /// @param viewport ビューポートのサイズ (width, height)
    void DrawImpl(gl::Uint, const std::pair<float, float>&) const override;

 private:
    /// @brief 描画用の頂点/法線データとインデックスデータを生成する
    void GenerateSurfaceData();
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_SURFACES_TRIMMED_SURFACE_GRAPHICS_H_
