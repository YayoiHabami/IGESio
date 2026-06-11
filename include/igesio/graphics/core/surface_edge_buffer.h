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

#include "igesio/numerics/core/matrix.h"
#include "igesio/graphics/core/gl_types.h"
#include "igesio/graphics/core/i_open_gl.h"



namespace igesio::graphics {

/// @brief サーフェス境界エッジ描画時のデフォルト色 (RGBA) [0.0 - 1.0]
/// @note shadedモードで面と区別できるよう、やや暗い灰色とする
constexpr std::array<float, 4> kSurfaceEdgeColor = {0.1f, 0.1f, 0.1f, 1.0f};

/// @brief ハイライト中のエッジへ適用するウィンドウ深度の圧縮率
/// @note 隣接面が共有する辺は両面から1本ずつ (別テッセレーションで) 描かれ、
///       ほぼ同一深度でZファイトして選択色と通常色の縞になる. ハイライト側を
///       glDepthRange(0, 1-この値)で僅かに手前へ寄せ、描画順に依存せず
///       選択色を勝たせる. 大きくしすぎると選択エッジが手前の面を透過するため、
///       テッセレーション差 (窓深度で~1e-5) に勝つ最小限の桁とする
constexpr double kHighlightDepthShrink = 1e-4;

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

    /// @brief 平坦化済みの線分頂点列からGPUバッファを構築する
    /// @param segment_vertices 線分頂点列 (頂点毎にx,y,z; 2頂点で1線分.
    ///        要素数は6の倍数であること)
    /// @note 既存リソースを解放してから再構築する. 独立した線分の集合
    ///       (メッシュエッジ等) をループ点列へ詰め替えずに転送するための入口
    void BuildFromSegments(const std::vector<float>&);

    /// @brief 表示状態 (model, 色, 線幅) を設定して線分を描画する
    /// @param shader 使用中のシェーダープログラムID (呼び出し側でbind済み)
    /// @param model モデル変換行列
    /// @param color 線の色 (RGBA)
    /// @param line_width 線幅
    /// @param highlighted ハイライト中か. trueの場合は深度を僅かに手前へ寄せ、
    ///        隣接面の同一位置のエッジに対して決定的に勝たせる
    ///        (kHighlightDepthShrink参照)
    /// @note view/projection uniformは呼び出し側で設定済みであること
    void DrawWithState(gl::Uint, const igesio::Matrix4f&,
                       const std::array<float, 4>&, double,
                       bool highlighted = false) const;

    /// @brief GPUリソースを解放する
    void Cleanup();

    /// @brief 描画可能な頂点を保持していないか
    bool IsEmpty() const { return vertex_count_ == 0; }
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CORE_SURFACE_EDGE_BUFFER_H_
