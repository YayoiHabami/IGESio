/**
 * @file graphics/copious_data_graphics.h
 * @brief CopiousDataの描画用クラス
 * @author Yayoi Habami
 * @date 2025-09-12
 * @copyright 2025 Yayoi Habami
 * @brief このクラスは、Copious Data (Type 106, Forms 1-3, 11-13)
 *        エンティティの描画を行う。
 */
#ifndef IGESIO_GRAPHICS_CURVES_COPIOUS_DATA_GRAPHICS_H_
#define IGESIO_GRAPHICS_CURVES_COPIOUS_DATA_GRAPHICS_H_

#include <memory>
#include <utility>
#include <vector>

#include "igesio/entities/curves/copious_data_base.h"
#include "igesio/graphics/core/entity_graphics.h"

namespace igesio::graphics {

/// @brief CopiousDataエンティティの描画情報の管理クラス
class CopiousDataGraphics
    : public EntityGraphics<entities::CopiousDataBase> {
 private:
    /// @brief エンティティの描画用の頂点バッファオブジェクト (VBO) のID
    gl::Uint vbo_ = 0;
    /// @brief エンティティの頂点数
    gl::Sizei vertex_count_ = 0;

 public:
    /// @brief コンストラクタ
    /// @param entity 描画するエンティティのポインタ
    /// @param gl OpenGL関数のラッパー
    /// @throw std::invalid_argument entityがnullptrの場合
    explicit CopiousDataGraphics(const std::shared_ptr<const entities::CopiousDataBase>&,
                                 const std::shared_ptr<IOpenGL>&);

    /// @brief デストラクタ
    ~CopiousDataGraphics();

    // コピーコンストラクタとコピー代入演算子を禁止
    CopiousDataGraphics(const CopiousDataGraphics&) = delete;
    CopiousDataGraphics& operator=(const CopiousDataGraphics&) = delete;

    /// @brief ムーブコンストラクタ
    /// @param other ムーブ元のEntityGraphics
    CopiousDataGraphics(CopiousDataGraphics&&) noexcept;

    /// @brief ムーブ代入演算子
    /// @param other ムーブ元のEntityGraphics
    /// @return *this
    CopiousDataGraphics& operator=(CopiousDataGraphics&&) noexcept;

    /// @brief OpenGLリソースを解放する
    void Cleanup() override;

    /// @brief 描画可能な状態かどうかを確認する
    bool IsDrawable() const override;

    /// @brief エンティティをセットアップする
    void Synchronize() override;

    /// @brief レイと各頂点の近接判定を行う
    /// @param ray ワールド空間のレイ (kRayとして扱う)
    /// @param params 探索制御パラメータ
    /// @return 点列 (form 1-3) の場合は各頂点とレイの近接判定結果 (distance昇順)、
    ///         折れ線 (form 11-13) の場合は基底の曲線判定に委譲した結果
    /// @note 点列は接線を持たず IntersectCurveWithLine が適用できないため、
    ///       各頂点とレイの距離で判定する
    std::vector<RayHit> Intersect(
            const Ray&, const RayIntersectionParams&) const override;

    /// @brief 範囲選択用にサンプリングする
    /// @param params サンプリング制御パラメータ
    /// @return 点列 (form 1-3) は各頂点をpointsに格納したサンプル、
    ///         折れ線 (form 11-13) は基底の曲線サンプリングに委譲した結果
    SelectionSamples GetSelectionSamples(
            const SelectionSampleParams&) const override;

 protected:
    /// @brief エンティティの描画を行う
    /// @param shader プログラムシェーダーのID
    /// @param viewport ビューポートのサイズ (width, height)
    void DrawImpl(
            gl::Uint, [[maybe_unused]] const std::pair<float, float>&) const override;
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CURVES_COPIOUS_DATA_GRAPHICS_H_
