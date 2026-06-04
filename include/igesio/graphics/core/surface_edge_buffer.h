/**
 * @file graphics/core/surface_edge_buffer.h
 * @brief サーフェス境界エッジを線分として描画するGPUバッファ
 * @author Yayoi Habami
 * @date 2026-06-03
 * @copyright 2026 Yayoi Habami
 * @note 境界エッジ (折れ線ループ群) を線分(kLines)頂点列へ平坦化し、単一のVAO/VBOで
 *       保持する. 頂点入力は汎用曲線シェーダー (kGeneralCurve相当; location 0 = vec3) に
 *       一致する. 各サーフェスグラフィックスがコンポジションで保持して利用する.
 */
#ifndef IGESIO_GRAPHICS_CORE_SURFACE_EDGE_BUFFER_H_
#define IGESIO_GRAPHICS_CORE_SURFACE_EDGE_BUFFER_H_

#include <array>
#include <memory>
#include <utility>
#include <vector>

#include "igesio/numerics/matrix.h"
#include "igesio/graphics/core/gl_types.h"
#include "igesio/graphics/core/i_open_gl.h"



namespace igesio::graphics {

/// @brief サーフェス境界エッジ描画時のデフォルト色 (RGBA) [0.0 - 1.0]
/// @note shadedモードで面と区別できるよう、やや暗い灰色とする
constexpr std::array<float, 4> kSurfaceEdgeColor = {0.1f, 0.1f, 0.1f, 1.0f};

/// @brief サーフェス境界エッジを線分(kLines)として保持・描画するGPUバッファ
/// @note 折れ線ループ群を線分ペアに平坦化して保持する. 描画時のシェーダープログラムの
///       bindとview/projection uniformの設定は呼び出し側 (レンダラ) の責務とする.
class SurfaceEdgeBuffer {
    /// @brief OpenGLラッパー (非所有; 生成元のグラフィックスと共有)
    std::shared_ptr<IOpenGL> gl_;
    /// @brief 頂点配列オブジェクト
    gl::Uint vao_ = 0;
    /// @brief 頂点バッファオブジェクト
    gl::Uint vbo_ = 0;
    /// @brief 線分頂点数 (= 線分数 x 2)
    gl::Sizei vertex_count_ = 0;

 public:
    /// @brief コンストラクタ
    /// @param gl OpenGLラッパー
    explicit SurfaceEdgeBuffer(std::shared_ptr<IOpenGL> gl)
            : gl_(std::move(gl)) {}

    /// @brief デストラクタ
    ~SurfaceEdgeBuffer() { Cleanup(); }

    // コピーは禁止 (GPUリソースの二重解放を避ける)
    SurfaceEdgeBuffer(const SurfaceEdgeBuffer&) = delete;
    SurfaceEdgeBuffer& operator=(const SurfaceEdgeBuffer&) = delete;

    /// @brief 境界エッジ (折れ線ループ群) からGPUバッファを構築する
    /// @param loops 各ループの連続点列 (定義/モデル空間)
    /// @note 既存リソースを解放してから再構築する. 各ループは隣接点を結ぶ線分へ
    ///       平坦化される (N点のループ → N-1本の線分).
    void Build(const std::vector<std::vector<Vector3d>>&);

    /// @brief 表示状態 (model, 色, 線幅) を設定して線分を描画する
    /// @param shader 使用中のシェーダープログラムID (呼び出し側でbind済み)
    /// @param model モデル変換行列
    /// @param color 線の色 (RGBA)
    /// @param line_width 線幅
    /// @note view/projection uniformは呼び出し側で設定済みであること
    void DrawWithState(gl::Uint, const igesio::Matrix4f&,
                       const std::array<float, 4>&, double) const;

    /// @brief GPUリソースを解放する
    void Cleanup();

    /// @brief 描画可能な頂点を保持していないか
    bool IsEmpty() const { return vertex_count_ == 0; }
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CORE_SURFACE_EDGE_BUFFER_H_
