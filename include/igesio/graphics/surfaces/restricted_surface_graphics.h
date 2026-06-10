/**
 * @file graphics/surfaces/restricted_surface_graphics.h
 * @brief 制限付き曲面 (IRestrictedSurface: Type 143/144/108有界) の描画用クラス
 * @author Yayoi Habami
 * @date 2026-06-09
 * @copyright 2026 Yayoi Habami
 */
#ifndef IGESIO_GRAPHICS_SURFACES_RESTRICTED_SURFACE_GRAPHICS_H_
#define IGESIO_GRAPHICS_SURFACES_RESTRICTED_SURFACE_GRAPHICS_H_

#include <cstdint>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/entities/interfaces/i_restricted_surface.h"
#include "igesio/entities/surfaces/algorithms/restricted_surface_mesh.h"
#include "igesio/graphics/core/entity_graphics.h"
#include "igesio/graphics/core/surface_edge_buffer.h"



namespace igesio::graphics {

/// @brief 制限付き曲面エンティティ (IRestrictedSurface) の描画情報の管理クラス
/// @note Type 144 (TrimmedSurface)、Type 143 (BoundedSurface)、Type 108の有界平面
///       (BoundedPlane) を共通に扱う。メッシュ生成は制限付き曲面共通のテッセレーション
///       (entities::TessellateRestrictedSurface) に委譲する。境界駆動の制限付き
///       四分木により、トリム境界近傍を適応的に細分する。
class RestrictedSurfaceGraphics
    : public EntityGraphics<entities::IRestrictedSurface, true> {
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

    /// @brief 描画用CPUデータのステージング (PrewarmCpuが構築、DoSynchronizeが消費)
    /// @note GL転送前のCPU側メッシュ。前倒しテッセレーション結果をここへ置き、
    ///       DoSynchronize (GLスレッド) がvertices_/indices_へ移してGPUへ転送する。
    entities::RestrictedSurfaceMesh pending_mesh_;
    /// @brief 境界エッジループ (モデル空間の折れ線群) のステージング
    std::vector<std::vector<Vector3d>> pending_edge_loops_;
    /// @brief pending_*が現在の幾何キーで構築済みか (冪等判定用)
    bool cpu_ready_ = false;
    /// @brief pending_*構築時の同期キー (CurrentGeometryKeyと比較)
    std::uint64_t cpu_key_ = 0;

 public:
    /// @brief コンストラクタ
    /// @param entity 描画するエンティティのポインタ
    /// @param gl OpenGL関数のラッパー
    explicit RestrictedSurfaceGraphics(
            const std::shared_ptr<const entities::IRestrictedSurface>&,
            const std::shared_ptr<IOpenGL>&);

    /// @brief デストラクタ
    ~RestrictedSurfaceGraphics() override;

    // コピーコンストラクタとコピー代入演算子を禁止
    RestrictedSurfaceGraphics(const RestrictedSurfaceGraphics&) = delete;
    RestrictedSurfaceGraphics& operator=(const RestrictedSurfaceGraphics&) = delete;

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
            const bool highlighted = ctx.IsHighlighted(GetEntityID());
            const auto& color = highlighted
                    ? ctx.highlight_color : kSurfaceEdgeColor;
            edge_buffer_.DrawWithState(shader, GetWorldTransform(),
                                       color, GetLineWidth(), highlighted);
            return;
        }
        EntityGraphics::Draw(shader, shader_type, viewport, ctx);
    }

    /// @brief 全ての可能なシェーダータイプを取得する
    /// @note 面シェーダーに加え、境界エッジ用のkSurfaceEdgeを常に含める。
    ///       描画バケットは同期前 (edge_buffer_未構築) のreconcile段で構築されるため、
    ///       構築状態に依存させてはならない (実際に空ならDrawがIsEmptyで早期return)。
    std::unordered_set<ShaderType> GetShaderTypes() const override {
        auto types = EntityGraphics::GetShaderTypes();
        types.insert(ShaderType::kSurfaceEdge);
        return types;
    }

    /// @brief 描画用CPUデータ (テッセレーション・境界エッジ) を事前構築する
    /// @note GL呼び出しを含まないため、レンダラから並列に呼べる。結果は
    ///       pending_mesh_/pending_edge_loops_へ格納する。同一同期キーでは再構築しない。
    void PrewarmCpu() override;

    /// @brief エンティティをセットアップする (GL転送)
    /// @note PrewarmCpuで前倒し済みならステージングをGPUへ転送するのみ。未構築なら
    ///       自前でPrewarmCpuを呼んでから転送する (直列フォールバック)。
    void DoSynchronize() override;

    /// @brief OpenGLリソースを解放する
    void Cleanup() override;

    /// @brief 範囲選択用に制限付き曲面をサンプリングする
    /// @param params サンプリング制御パラメータ
    /// @return トリム境界(外周/内周)の閉ループ折れ線と、トリム領域内の内部グリッド点
    /// @note 汎用の矩形境界では未トリム領域まで含み過剰選択となるため、
    ///       IRestrictedSurfaceのUV境界曲線を ICurve としてサンプリングする
    SelectionSamples GetSelectionSamples(
            const SelectionSampleParams&) const override;

 protected:
    /// @brief エンティティの描画を行う
    /// @param shader プログラムシェーダーのID
    /// @param viewport ビューポートのサイズ (width, height)
    void DrawImpl(gl::Uint, const std::pair<float, float>&) const override;

 private:
    /// @brief UV境界曲線を基底曲面S(u,v)で3D化したワールド折れ線をresultへ追加する
    /// @param uv_boundary UV空間の境界曲線 (TryGetPointAtが(u,v,0)を返す)
    /// @param n_samples サンプル分割数
    /// @param result 追加先 (polylinesへ折れ線を追加)
    /// @note GetOuterUVBoundary()等はUV空間の曲線を返すため、選択用の3D点を得るには
    ///       基底曲面で評価する必要がある。トリム境界はドメイン端のため、ドメイン制限を
    ///       受けない基底S(u,v)で評価し、配置(M_entity込みのワールド変換)を適用する
    void AppendBoundaryWorldPolyline(
            const entities::ICurve& uv_boundary, int n_samples,
            SelectionSamples& result) const;
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_SURFACES_RESTRICTED_SURFACE_GRAPHICS_H_
