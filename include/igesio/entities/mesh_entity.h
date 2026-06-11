/**
 * @file entities/mesh_entity.h
 * @brief 三角形メッシュを保持する非IGESエンティティ
 * @author Yayoi Habami
 * @date 2026-06-10
 * @copyright 2026 Yayoi Habami
 * @note IGESへはシリアライズされない計算・描画用のメッシュエンティティ.
 *       STL等の外部フォーマット読み込み結果やプログラム生成メッシュを
 *       Assemblyツリー・描画パイプラインへ載せるために用いる.
 */
#ifndef IGESIO_ENTITIES_MESH_ENTITY_H_
#define IGESIO_ENTITIES_MESH_ENTITY_H_

#include <optional>
#include <utility>

#include "igesio/numerics/core/matrix.h"
#include "igesio/numerics/geometric/bounding_box.h"
#include "igesio/numerics/mesh/triangle_mesh.h"
#include "igesio/entities/non_iges_entity_base.h"
#include "igesio/entities/interfaces/i_geometry.h"



namespace igesio::entities {

/// @brief 三角形メッシュを保持する非IGESエンティティ
/// @note `EntityType::kNonIges`としてAssemblyへ保存でき、IGES出力 (WriteIges)
///       からはスキップされる. 描画は組み込みの`TriangleMeshGraphics`が
///       GraphicsRegistry経由で担う (IGESIO_ENABLE_GRAPHICS時).
/// @note メッシュはCPU正準値の方針に従い倍精度 (TriangleMeshd) で保持する.
///       GPUへは描画側が転送時に単精度へ変換する.
/// @note 変換行列は持たない (恒等). 配置はAssemblyの大域変換で行う.
/// @note ピッキング (レイ交差) と範囲選択は、graphics側のPickRegistryの
///       組み込みseedにより対応する (IGESIO_ENABLE_GRAPHICS時).
class MeshEntity : public NonIgesEntityBase, public IGeometry {
 public:
    /// @brief コンストラクタ
    /// @param mesh 保持する三角形メッシュ
    /// @throw std::invalid_argument meshの構造が不正な場合
    ///        (numerics::Validateに失敗する場合)
    explicit MeshEntity(numerics::TriangleMeshd mesh);

    /// @brief メッシュを取得する
    /// @return 保持している三角形メッシュ
    const numerics::TriangleMeshd& Mesh() const { return mesh_; }

    /// @brief メッシュを差し替える
    /// @param mesh 新しい三角形メッシュ
    /// @throw std::invalid_argument meshの構造が不正な場合
    /// @note 形状編集としてジオメトリリビジョンをバンプする
    ///       (描画は次回Drawで自動的に再同期される)
    void SetMesh(numerics::TriangleMeshd mesh);

    /// @brief 定義空間におけるバウンディングボックスを取得する
    /// @return 全頂点を包含する軸平行バウンディングボックス.
    ///         頂点が無い場合は空のBoundingBox
    numerics::BoundingBox GetDefinedBoundingBox() const override;

 protected:
    /// @brief 座標orベクトルを変換する (恒等)
    /// @param input 変換前の座標orベクトル
    /// @param is_point 座標を変換する場合はtrue
    /// @return 入力をそのまま返す (変換行列を持たないため)
    std::optional<Vector3d> Transform(
            const std::optional<Vector3d>& input, bool is_point) const override;

 private:
    /// @brief 保持する三角形メッシュ
    numerics::TriangleMeshd mesh_;
};

}  // namespace igesio::entities

#endif  // IGESIO_ENTITIES_MESH_ENTITY_H_
