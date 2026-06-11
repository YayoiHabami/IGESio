/**
 * @file graphics/meshes/triangle_mesh_graphics.h
 * @brief MeshEntity (三角形メッシュ) の描画クラス
 * @author Yayoi Habami
 * @date 2026-06-10
 * @copyright 2026 Yayoi Habami
 * @note 既存の汎用曲面シェーダー (ShaderId::kGeneralSurface) を再利用して
 *       三角形メッシュを描画する. 独自シェーダーは要求しない.
 */
#ifndef IGESIO_GRAPHICS_MESHES_TRIANGLE_MESH_GRAPHICS_H_
#define IGESIO_GRAPHICS_MESHES_TRIANGLE_MESH_GRAPHICS_H_

#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "igesio/entities/meshes/mesh_entity.h"
#include "igesio/graphics/core/entity_graphics.h"
#include "igesio/graphics/core/surface_edge_buffer.h"



namespace igesio::graphics {

/// @brief MeshEntity (三角形メッシュ) の描画クラス
/// @note GraphicsRegistryの組み込みseedとして登録され、MeshEntityを
///       Assemblyへ追加するだけで描画される (ユーザーの登録操作は不要).
/// @note 頂点属性は汎用曲面シェーダーのレイアウト (interleaved 8 float:
///       位置3+法線3+UV2) に合わせる. メッシュが法線を持たない場合は
///       面積重み平均で補い、UVを持たない場合はゼロを書き込む.
/// @note エッジ描画 (kSurfaceEdge) に対応する. 表示モードに応じて
///       kWireFrameでは全ユニークエッジ、kShadedでは特徴エッジ
///       (境界・非多様体・折り目) を線分として描画する.
/// @note ピッキング (レイ交差) と範囲選択はPickRegistryの組み込みseed
///       (MeshEntity→メッシュ用ピック関数) が担うため、本クラスでの
///       オーバーライドは不要 (EntityGraphicsの既定実装がレジストリを引く).
class TriangleMeshGraphics
        : public EntityGraphics<entities::MeshEntity, true> {
 public:
    /// @brief コンストラクタ
    /// @param entity 描画するメッシュエンティティ
    /// @param gl OpenGL関数のラッパー
    /// @throw std::invalid_argument entityがnullptrの場合
    TriangleMeshGraphics(const std::shared_ptr<const entities::MeshEntity>&,
                         const std::shared_ptr<IOpenGL>&);

    /// @brief デストラクタ
    ~TriangleMeshGraphics() override;

    // 基底のDrawオーバーロード (3引数版) を可視に保つ
    using EntityGraphics::Draw;

    /// @brief エンティティの描画を行う (シェーダー型で分岐)
    /// @note kSurfaceEdgeでは表示モードに応じたエッジバッファ
    ///       (kWireFrame=全エッジ、それ以外=特徴エッジ) を線描画し、
    ///       他は基底に委譲する. kNoEdgeはレンダラ側でkSurfaceEdgeバケット
    ///       ごとスキップされるためここへは到達しない.
    void Draw(gl::Uint shader, const ShaderId shader_id,
              const std::pair<float, float>& viewport,
              const DrawContext& ctx) const override;

    /// @brief 全ての可能なシェーダータイプを取得する
    /// @note 面シェーダーに加え、エッジ用のkSurfaceEdgeを常に含める。
    ///       描画バケットは同期前 (エッジバッファ未構築) のreconcile段で
    ///       構築されるため、構築状態に依存させてはならない
    ///       (実際に空ならDrawがIsEmptyで早期return)。
    std::unordered_set<ShaderId> GetShaderIds() const override;

    /// @brief 描画用CPUデータ (interleavedステージング) を事前構築する
    /// @note GLを呼ばない. 同期キーで冪等化されており、レンダラの並列
    ///       前倒し経路から安全に呼べる
    void PrewarmCpu() override;

    /// @brief OpenGLリソースを解放する
    /// @note CPUステージングは破棄しない (DoSynchronizeが先頭で本関数を
    ///       呼ぶため. IEntityGraphicsの規約)
    void Cleanup() override;

 protected:
    /// @brief GPUリソースを構築・転送する
    void DoSynchronize() override;

    /// @brief 三角形メッシュを描画する
    void DrawImpl(gl::Uint, const std::pair<float, float>&) const override;

 private:
    /// @brief 頂点バッファ (VBO) のID
    gl::Uint vbo_ = 0;
    /// @brief インデックスバッファ (EBO) のID
    gl::Uint ebo_ = 0;
    /// @brief 転送済みのインデックス数 (DrawElements用)
    int index_count_ = 0;

    /// @brief 全ユニークエッジの線分バッファ (kWireFrame表示用)
    SurfaceEdgeBuffer all_edge_buffer_;
    /// @brief 特徴エッジ (境界・非多様体・折り目) の線分バッファ (kShaded表示用)
    SurfaceEdgeBuffer feature_edge_buffer_;

    /// @brief CPUステージング: interleaved頂点 (8つのfloat/頂点)
    /// @note Cleanupでは破棄しない (PrewarmCpuの結果を保持する)
    std::vector<float> staging_vertices_;
    /// @brief CPUステージング: 三角形インデックス
    std::vector<gl::Uint> staging_indices_;
    /// @brief CPUステージング: 全エッジの線分頂点列 (Cleanupでは破棄しない)
    std::vector<float> staging_all_edges_;
    /// @brief CPUステージング: 特徴エッジの線分頂点列 (Cleanupでは破棄しない)
    std::vector<float> staging_feature_edges_;
    /// @brief ステージングを構築した時点の同期キー (PrewarmCpuの冪等化用)
    std::uint64_t staged_geometry_key_ = 0;
    /// @brief ステージングが構築済みか
    bool staged_ = false;
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_MESHES_TRIANGLE_MESH_GRAPHICS_H_
