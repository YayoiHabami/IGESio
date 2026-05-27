/**
 * @file graphics/curves/point_graphics.h
 * @brief 点の描画用クラス
 * @author Yayoi Habami
 * @date 2025-10-15
 * @copyright 2025 Yayoi Habami
 * @note Pointエンティティ (Type 116) の描画を行う. Pointエンティティは
 *       Subfigure Definition (Type 308) への参照を持つことがあり、この場合
 *       点をSubfigure Definitionで定義された図形で描画する必要があるため、
 *       独立した描画クラスを用意する.
 */
#ifndef IGESIO_GRAPHICS_CURVES_POINT_GRAPHICS_H_
#define IGESIO_GRAPHICS_CURVES_POINT_GRAPHICS_H_

#include <memory>
#include <utility>
#include <vector>

#include "igesio/entities/curves/point.h"
#include "igesio/graphics/core/entity_graphics.h"



namespace igesio::graphics {

/// @brief 点の描画用クラス
/// @note Pointエンティティ (Type 116) の描画を行う. Pointエンティティは
///       Subfigure Definition (Type 308) への参照を持つことがあり、この場合
///       点をSubfigure Definitionで定義された図形で描画する必要があるため、
///       独立した描画クラスを用意する.
class PointGraphics
    : public EntityGraphics<entities::Point, ShaderType::kPoint> {
 private:
    /// @brief エンティティの描画用の頂点バッファオブジェクト (VBO) のID
    GLuint vbo_ = 0;

 public:
    /// @brief コンストラクタ
    /// @param entity 描画するエンティティのポインタ
    /// @param gl OpenGL関数のラッパー
    /// @throw std::invalid_argument entityがnullptrの場合
    explicit PointGraphics(const std::shared_ptr<const entities::Point>,
                           const std::shared_ptr<IOpenGL>);

    /// @brief デストラクタ
    ~PointGraphics();

    // コピーコンストラクタとコピー代入演算子を禁止
    PointGraphics(const PointGraphics&) = delete;
    PointGraphics& operator=(const PointGraphics&) = delete;

    /// @brief ムーブコンストラクタ
    /// @param other ムーブ元のEntityGraphics
    PointGraphics(PointGraphics&&) noexcept;

    /// @brief ムーブ代入演算子
    /// @param other ムーブ元のEntityGraphics
    /// @return *this
    PointGraphics& operator=(PointGraphics&&) noexcept;

    /// @brief OpenGLリソースを解放する
    void Cleanup() override;

    /// @brief 描画可能な状態かどうかを確認する
    bool IsDrawable() const override;

    /// @brief エンティティをセットアップする
    void Synchronize() override;

    /// @brief レイとの交差判定が可能か
    /// @return 常にtrue (点とレイの近接判定を行う)
    /// @note PointはICurveを継承しないため、基底の判定ではfalseとなる.
    ///       点状形状として独自に近接判定を行うためtrueを返す
    bool CanIntersect() const override;

    /// @brief レイと点の近接判定を行う
    /// @param ray ワールド空間のレイ (kRayとして扱う)
    /// @param params 探索制御パラメータ
    /// @return 点とレイの3D距離がcurve_hit_tolerance以下のとき1件、
    ///         そうでなければ空リスト
    std::vector<RayHit> Intersect(
            const Ray&, const RayIntersectionParams&) const override;

    /// @brief 範囲選択用に点をサンプリングする
    /// @param params サンプリング制御パラメータ (未使用)
    /// @return 点1つをpointsに格納したサンプル (描画位置・ピッキング位置に一致)
    /// @note PointはICurveを継承しないため基底の汎用サンプリングでは拾えない.
    ///       点状形状として独自にサンプルを返す
    SelectionSamples GetSelectionSamples(
            const SelectionSampleParams&) const override;

 protected:
    /// @brief エンティティの描画を行う
    /// @param shader プログラムシェーダーのID
    /// @param viewport ビューポートのサイズ (width, height)
    void DrawImpl(GLuint, const std::pair<float, float>&) const override;
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CURVES_POINT_GRAPHICS_H_
