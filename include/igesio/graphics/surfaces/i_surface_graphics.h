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
#include <utility>
#include <vector>

#include "igesio/numerics/matrix.h"
#include "igesio/entities/interfaces/i_surface.h"
#include "igesio/graphics/core/entity_graphics.h"


namespace igesio::graphics {

/// @brief ISurfaceクラス用の、曲面全般の描画情報の管理クラス
class ISurfaceGraphics
    : public EntityGraphics<entities::ISurface,
                            ShaderType::kGeneralSurface,
                            true> {
    /// @brief 面のVBO
    GLuint vbo_ = 0;
    /// @brief 面のEBO
    GLuint ebo_ = 0;

    /// @brief 面の頂点・法線データ
    /// @note 各列が {x, y, z, nx, ny, nz} の形式
    MatrixXf vertices_;
    /// @brief 面のインデックスデータ
    std::vector<GLuint> indices_;

 public:
    /// @brief コンストラクタ
    /// @param entity 描画するエンティティのポインタ
    /// @param gl OpenGL関数のラッパー
    /// @throw std::invalid_argument entityがnullptrの場合、
    ////       entityがISurfaceを継承していない場合
    explicit ISurfaceGraphics(const std::shared_ptr<const entities::ISurface>,
                              const std::shared_ptr<IOpenGL>);

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

    /// @brief エンティティをセットアップする
    /// @note 内部で参照するエンティティの状態に基づいて、
    ///       描画用のリソースを再セットアップする
    void Synchronize() override;

    /// @brief OpenGLリソースを解放する
    /// @note SSBO等のOpenGLリソースを解放する
    void Cleanup() override;

 protected:
    /// @brief エンティティの描画を行う
    /// @param shader プログラムシェーダーのID
    /// @param viewport ビューポートのサイズ (width, height)
    void DrawImpl(GLuint, const std::pair<float, float>&) const override;

    /// @brief 描画用の頂点/法線データとインデックスデータを生成する
    void GenerateSurfaceData();
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_SURFACES_I_SURFACE_GRAPHICS_H_
