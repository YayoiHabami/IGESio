/**
 * @file graphics/curves/curve_on_a_parametric_surface_graphics.h
 * @brief CurveOnAParametricSurfaceの描画用クラス
 * @author Yayoi Habami
 * @date 2025-11-09
 * @copyright 2025 Yayoi Habami
 */
#ifndef IGESIO_GRAPHICS_CURVES_CURVE_ON_A_PARAMETRIC_SURFACE_GRAPHICS_H_
#define IGESIO_GRAPHICS_CURVES_CURVE_ON_A_PARAMETRIC_SURFACE_GRAPHICS_H_

#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "igesio/entities/curves/curve_on_a_parametric_surface.h"
#include "igesio/graphics/core/entity_graphics.h"



namespace igesio::graphics {

/// @brief CurveOnAParametricSurfaceエンティティの描画情報の管理クラス
class CurveOnAParametricSurfaceGraphics
    : public EntityGraphics<entities::CurveOnAParametricSurface> {
 private:
    /// @brief C(t) 用の描画オブジェクト
    std::unique_ptr<IEntityGraphics> curve_graphics_;

 public:
    /// @brief コンストラクタ
    /// @param entity 描画するエンティティのポインタ
    /// @param gl OpenGL関数のラッパー
    /// @throw std::invalid_argument entityがnullptrの場合、
    ////       entityがICurveを継承していない場合
    CurveOnAParametricSurfaceGraphics(
            const std::shared_ptr<const entities::CurveOnAParametricSurface>&,
            const std::shared_ptr<IOpenGL>&);

    /// @brief デストラクタ
    ~CurveOnAParametricSurfaceGraphics();

    // コピーコンストラクタとコピー代入演算子を禁止
    CurveOnAParametricSurfaceGraphics(
            const CurveOnAParametricSurfaceGraphics&) = delete;
    CurveOnAParametricSurfaceGraphics& operator=(
            const CurveOnAParametricSurfaceGraphics&) = delete;

    /// @brief エンティティをセットアップする
    /// @note 内部で参照するエンティティの状態に基づいて、
    ///       描画用のリソースを再セットアップする
    void DoSynchronize() override;

    /// @brief OpenGLリソースを解放する
    void Cleanup() override;

    /// @brief 描画可能な状態かどうかを確認する
    bool IsDrawable() const override;

    /// @brief 全ての可能なシェーダータイプを取得する
    /// @return 全ての可能なシェーダータイプのリスト
    /// @note 例えば子要素がある場合、`GetShaderType`のShaderTypeに加えて、
    ///       各子要素のShaderTypeも含まれる.
    std::unordered_set<ShaderType> GetShaderTypes() const override;

    /// @brief エンティティの描画を行う
    /// @param shader プログラムシェーダーのID
    /// @param shader_type 描画に使用するシェーダーのタイプ
    /// @param viewport ビューポートのサイズ (width, height)
    /// @param ctx 表示コンテキスト (選択ハイライト等をPULLする)
    /// @note shader_typeの子要素のみを描画する. 子要素の描画は
    ///       子要素に移譲する.
    void Draw(gl::Uint, const ShaderType, const std::pair<float, float>&,
              const DrawContext&) const override;

    /// @brief レイとの交差判定が可能か
    /// @return 生成済みの3D曲線C(t)が交差判定可能な場合はtrue
    bool CanIntersect() const override;

    /// @brief レイとエンティティの交差点を求める
    /// @param ray ワールド空間のレイ (kRayとして扱う)
    /// @param params 探索制御パラメータ
    /// @return 交差点のリスト (distance昇順)
    /// @note 142自身の評価は曲面の定義空間を経由し描画形状とずれるため、
    ///       描画と同一の生成済み3D曲線C(t) (子要素) へ判定を委譲する
    std::vector<RayHit> Intersect(
            const Ray&, const RayIntersectionParams&) const override;

    /// @brief メインの色を設定する
    /// @param color メインの色 (RGBA; [0, 1]の範囲)
    /// @note 描画は子要素C(t)に委譲されるため、色も子要素へ伝播させる
    void SetColor(const std::array<float, 4>&) override;

    /// @brief 色をデフォルトのエンティティの色に戻す
    /// @note 描画は子要素C(t)に委譲されるため、子要素の色もリセットする
    void ResetColor() override;



 protected:
    /// @brief エンティティの描画を行う
    /// @param shader プログラムシェーダーのID
    /// @param viewport ビューポートのサイズ (width, height)
    void DrawImpl(gl::Uint, const std::pair<float, float>&) const override {};
};

}  // namespace igesio::graphics

#endif  // IGESIO_GRAPHICS_CURVES_CURVE_ON_A_PARAMETRIC_SURFACE_GRAPHICS_H_
