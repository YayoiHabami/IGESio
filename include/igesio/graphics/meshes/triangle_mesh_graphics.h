/**
 * @file graphics/meshes/triangle_mesh_graphics.h
 * @brief MeshEntity (三角形メッシュ) の描画クラス
 * @author Yayoi Habami
 * @date 2026-06-10
 * @copyright 2026 Yayoi Habami
 * @note 既存の汎用曲面シェーダー (ShaderType::kGeneralSurface) を再利用して
 *       三角形メッシュを描画する. 独自シェーダーは要求しない.
 */
#ifndef IGESIO_GRAPHICS_MESHES_TRIANGLE_MESH_GRAPHICS_H_
#define IGESIO_GRAPHICS_MESHES_TRIANGLE_MESH_GRAPHICS_H_

#include <memory>
#include <utility>
#include <vector>

#include "igesio/entities/mesh_entity.h"
#include "igesio/graphics/core/entity_graphics.h"



namespace igesio::graphics {

/// @brief MeshEntity (三角形メッシュ) の描画クラス
/// @note GraphicsRegistryの組み込みseedとして登録され、MeshEntityを
///       Assemblyへ追加するだけで描画される (ユーザーの登録操作は不要).
/// @note 頂点属性は汎用曲面シェーダーのレイアウト (interleaved 8 float:
///       位置3+法線3+UV2) に合わせる. メッシュが法線を持たない場合は
///       面積重み平均で補い、UVを持たない場合はゼロを書き込む.
/// @note ピッキング (レイ交差) は現状非対応 (EntityGraphicsの既定動作.
///       MeshEntityはICurve/ISurfaceでないためCanIntersect=false).
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

    /// @brief CPUステージング: interleaved頂点 (8つのfloat/頂点)
    /// @note Cleanupでは破棄しない (PrewarmCpuの結果を保持する)
    std::vector<float> staging_vertices_;
    /// @brief CPUステージング: 三角形インデックス
    std::vector<gl::Uint> staging_indices_;
    /// @brief ステージングを構築した時点の同期キー (PrewarmCpuの冪等化用)
    std::uint64_t staged_geometry_key_ = 0;
    /// @brief ステージングが構築済みか
    bool staged_ = false;
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_MESHES_TRIANGLE_MESH_GRAPHICS_H_
