/**
 * @file extensions/inspection/coordinate_frame_group_graphics.h
 * @brief CoordinateFrameGroup (座標系群) の描画クラス
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 * @note 軸は専用のカスタムシェーダー (CamFrameLine; ジオメトリシェーダーで線分を
 *       スクリーン空間のpx幅クアッドへ展開) で色分けした太線として、点は専用の
 *       カスタムシェーダー (CamFramePoint; gl_PointSizeをuniform指定) で描画する.
 *       Core ProfileではglLineWidthによる太線が無効なため、GSで太さを生成する.
 *       IGESIO_ENABLE_GRAPHICS有効時にのみビルドされる.
 * @note 描画を配線するには `RegisterCoordinateFrameGroupGraphics()` を一度呼ぶこと
 *       (GraphicsRegistryへの作成関数登録とカスタムシェーダー登録を行う).
 */
#ifndef IGESIO_EXTENSIONS_INSPECTION_COORDINATE_FRAME_GROUP_GRAPHICS_H_
#define IGESIO_EXTENSIONS_INSPECTION_COORDINATE_FRAME_GROUP_GRAPHICS_H_

#include <cstdint>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "igesio/graphics/core/entity_graphics.h"
#include "igesio/graphics/core/surface_edge_buffer.h"
#include "igesio/extensions/inspection/coordinate_frame_group.h"

namespace igesio::extensions::inspection {

/// @brief CoordinateFrameGroup (座標系群) の描画クラス
/// @note GraphicsRegistry経由で生成される (登録は`RegisterCoordinateFrameGroupGraphics`).
///       軸はX/Y/Z色別の3本の線分バッファ、点は専用シェーダーで描画する.
/// @note 表示モードに依らず常時表示する (軸はCamFrameLine=kAlways、点も
///       CamFramePoint=kAlways). 面塗り・面エッジは持たない.
class CoordinateFrameGroupGraphics
        : public graphics::EntityGraphics<CoordinateFrameGroup, false> {
 public:
    /// @brief コンストラクタ
    /// @param entity 描画する座標系群エンティティ
    /// @param gl OpenGL関数のラッパー
    /// @throw std::invalid_argument entityがnullptrの場合
    CoordinateFrameGroupGraphics(
            const std::shared_ptr<const CoordinateFrameGroup>&,
            const std::shared_ptr<graphics::IOpenGL>&);

    /// @brief デストラクタ
    ~CoordinateFrameGroupGraphics() override;

    // 基底のDrawオーバーロード (3引数版) を可視に保つ
    using EntityGraphics::Draw;

    /// @brief エンティティの描画を行う (シェーダー型で分岐)
    /// @note CamFrameLineでは色分けした3軸の太線を、点用シェーダーでは原点の点群を
    ///       描画する. それ以外のシェーダー型では何もしない.
    void Draw(graphics::gl::Uint shader, graphics::ShaderId shader_id,
              const std::pair<float, float>& viewport,
              const graphics::DrawContext& ctx) const override;

    /// @brief 全ての可能なシェーダータイプを取得する
    /// @return { CamFrameLine, CamFramePoint }
    std::unordered_set<graphics::ShaderId> GetShaderIds() const override;

    /// @brief 描画用CPUデータ (線分・点のステージング) を事前構築する
    /// @note GLを呼ばない. 同期キーで冪等化する
    void PrewarmCpu() override;

    /// @brief 描画可能な状態かどうかを確認する
    /// @return 点または軸の少なくとも一方の描画リソースを保持する場合はtrue
    bool IsDrawable() const override;

    /// @brief 線 (軸) の表示太さを取得する
    /// @return スタイルの軸線幅 (entity_->AxisWidth()) [px].
    ///         entityが無い場合は基底の既定値にフォールバックする
    /// @note 基底はIGES線幅番号から算出するが、本エンティティは非IGESのため
    ///       スタイルで指定した軸線幅を用いる
    double GetLineWidth() const override;

    /// @brief OpenGLリソースを解放する
    /// @note CPUステージングは破棄しない (DoSynchronizeが先頭で本関数を呼ぶため)
    void Cleanup() override;

 protected:
    /// @brief GPUリソースを構築・転送する
    void DoSynchronize() override;

    /// @brief 葉ノードの描画実体 (本クラスではDrawで完結するため未使用)
    void DrawImpl(graphics::gl::Uint,
                  const std::pair<float, float>&) const override {}

 private:
    /// @brief 点群を描画する (CamFramePointシェーダー)
    /// @param shader 使用中のシェーダープログラムID
    void DrawPoints(graphics::gl::Uint shader) const;

    /// @brief X軸の線分バッファ (origin → origin + x·axis_size)
    graphics::SurfaceEdgeBuffer x_axis_buffer_;
    /// @brief Y軸の線分バッファ
    graphics::SurfaceEdgeBuffer y_axis_buffer_;
    /// @brief Z軸の線分バッファ
    graphics::SurfaceEdgeBuffer z_axis_buffer_;

    /// @brief 点群の頂点配列オブジェクト
    graphics::gl::Uint point_vao_ = 0;
    /// @brief 点群の頂点バッファオブジェクト
    graphics::gl::Uint point_vbo_ = 0;
    /// @brief 点群の頂点数 (= 座標系の本数)
    int point_count_ = 0;

    /// @brief CPUステージング: X軸線分頂点列 (Cleanupでは破棄しない)
    std::vector<float> staging_x_;
    /// @brief CPUステージング: Y軸線分頂点列
    std::vector<float> staging_y_;
    /// @brief CPUステージング: Z軸線分頂点列
    std::vector<float> staging_z_;
    /// @brief CPUステージング: 点群頂点列 (x,y,z/点)
    std::vector<float> staging_points_;
    /// @brief ステージングを構築した時点の同期キー (冪等化用)
    std::uint64_t staged_geometry_key_ = 0;
    /// @brief ステージングが構築済みか
    bool staged_ = false;
};

/// @brief CoordinateFrameGroupの描画をGraphicsRegistry/ShaderRegistryへ登録する
/// @note 冪等. GraphicsRegistryへの作成関数登録 (TryRegister) と、点描画用の
///       カスタムシェーダー (CamFramePoint) の登録を行う. 描画を行うアプリケーション
///       (ビューワー等) の初期化時に一度呼ぶこと.
/// @note 本関数が拡張ライブラリ内のこの翻訳単位を参照させるため、静的ライブラリでも
///       描画クラスがリンクされる (自動登録のみに頼らない明示登録経路).
void RegisterCoordinateFrameGroupGraphics();

}  // namespace igesio::extensions::inspection

#endif  // IGESIO_EXTENSIONS_INSPECTION_COORDINATE_FRAME_GROUP_GRAPHICS_H_
