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
#include <utility>
#include <vector>

#include "igesio/numerics/matrix.h"
#include "igesio/entities/surfaces/trimmed_surface.h"
#include "igesio/graphics/core/entity_graphics.h"



namespace igesio::graphics {

/// @brief TrimmedSurfaceエンティティの描画情報の管理クラス
/// @note 均一グリッドにエッジ交差点を加えたMarching Squares的な三角分割により、
///       トリム境界近傍の段差を抑制する。
///       各グリッドエッジでの有効/無効の遷移点を2分探索で求め、
///       境界セルを凸多角形にクリッピングしてファン三角分割する。
class TrimmedSurfaceGraphics
    : public EntityGraphics<entities::TrimmedSurface,
                            ShaderType::kGeneralSurface,
                            true> {
    /// @brief 面のVBO
    GLuint vbo_ = 0;
    /// @brief 面のEBO
    GLuint ebo_ = 0;

    /// @brief 面の頂点・法線・テクスチャ座標データ
    /// @note 各列が {x, y, z, nx, ny, nz, tu, tv} の形式
    MatrixXf vertices_;
    /// @brief 面のインデックスデータ
    std::vector<GLuint> indices_;

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

    /// @brief エンティティをセットアップする
    /// @note 内部で参照するエンティティの状態に基づいて、
    ///       描画用のリソースを再セットアップする
    void Synchronize() override;

    /// @brief OpenGLリソースを解放する
    void Cleanup() override;

 protected:
    /// @brief エンティティの描画を行う
    /// @param shader プログラムシェーダーのID
    /// @param viewport ビューポートのサイズ (width, height)
    void DrawImpl(GLuint, const std::pair<float, float>&) const override;

 private:
    /// @brief 描画用の頂点/法線データとインデックスデータを生成する
    void GenerateSurfaceData();
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_SURFACES_TRIMMED_SURFACE_GRAPHICS_H_
