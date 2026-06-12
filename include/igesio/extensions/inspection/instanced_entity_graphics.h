/**
 * @file extensions/inspection/instanced_entity_graphics.h
 * @brief InstancedEntity (複製表示) の描画クラス
 * @author Yayoi Habami
 * @date 2026-06-12
 * @copyright 2026 Yayoi Habami
 * @note 基準エンティティの描画オブジェクト (テッセレーション結果のGPUバッファ) を
 *       1つだけ生成・同期し、その同一メッシュを各複製先行列の位置へ繰り返し描画する.
 *       描画時に基準描画オブジェクトのworld_transform_を複製ごとに差し替えるため、
 *       N個複製してもCPUテッセレーション・GPUバッファは1回ぶんで済む.
 *       IGESIO_ENABLE_GRAPHICS有効時にのみビルドされる.
 * @note 描画を配線するには `RegisterInstancedEntityGraphics()` を一度呼ぶこと
 *       (GraphicsRegistryへの作成関数登録を行う. 基準エンティティの既存シェーダーを
 *       そのまま使うため、固有のカスタムシェーダーは持たない).
 */
#ifndef IGESIO_EXTENSIONS_INSPECTION_INSTANCED_ENTITY_GRAPHICS_H_
#define IGESIO_EXTENSIONS_INSPECTION_INSTANCED_ENTITY_GRAPHICS_H_

#include <array>
#include <cstdint>
#include <memory>
#include <unordered_set>
#include <utility>

#include "igesio/graphics/core/entity_graphics.h"
#include "igesio/extensions/inspection/instanced_entity.h"

namespace igesio::extensions::inspection {

/// @brief InstancedEntity (複製表示) の描画クラス
/// @note GraphicsRegistry経由で生成される (登録は`RegisterInstancedEntityGraphics`).
///       基準エンティティの描画オブジェクトを単一の子として保持し、描画・同期・
///       テクスチャ転送・色設定を委譲する. 子は`child_graphics_`ではなく専用メンバで
///       保持し、複製描画 (複数行列での描き分け) を本クラスで制御する.
/// @note シェーダー型は基準描画オブジェクトのものをそのまま公開する (固有の
///       シェーダーは持たない). レンダラのバケットには基準の各シェーダー型で収集される.
class InstancedEntityGraphics
        : public graphics::EntityGraphics<InstancedEntity, false> {
 public:
    /// @brief コンストラクタ
    /// @param entity 描画する複製表示エンティティ
    /// @param gl OpenGL関数のラッパー
    /// @throw std::invalid_argument entityがnullptrの場合
    /// @note 基準エンティティの描画オブジェクトを生成する (同期はreconcile経路が駆動).
    ///       基準が描画不能な型の場合は子はnullptrとなり、本オブジェクトは描画されない.
    InstancedEntityGraphics(
            const std::shared_ptr<const InstancedEntity>&,
            const std::shared_ptr<graphics::IOpenGL>&);

    /// @brief デストラクタ
    ~InstancedEntityGraphics() override;

    // 基底のDrawオーバーロード (3引数版) を可視に保つ
    using EntityGraphics::Draw;

    /// @brief エンティティの描画を行う (各複製先行列で基準メッシュを描き分ける)
    /// @note 各複製先行列Tについて、基準描画オブジェクトのworld変換を
    ///       `(本オブジェクトのworld変換) · T` に設定してから描画を委譲する.
    ///       同一のVAO/GPUバッファをmodel行列のみ差し替えて使い回す.
    void Draw(graphics::gl::Uint shader, graphics::ShaderId shader_id,
              const std::pair<float, float>& viewport,
              const graphics::DrawContext& ctx) const override;

    /// @brief 全ての可能なシェーダータイプを取得する
    /// @return 基準描画オブジェクトのシェーダー型の集合 (基準が無い場合は空)
    std::unordered_set<graphics::ShaderId> GetShaderIds() const override;

    /// @brief 描画用CPUデータ (基準のテッセレーション) を事前構築する
    /// @note 基準描画オブジェクトのPrewarmCpuへ委譲する (1回ぶんのみ)
    void PrewarmCpu() override;

    /// @brief テクスチャ用の描画リソースを同期する (基準へ委譲)
    void SyncTexture() override;

    /// @brief 描画可能な状態かどうかを確認する
    /// @return 基準描画オブジェクトが描画可能、かつ複製先行列が1つ以上ある場合はtrue
    bool IsDrawable() const override;

    /// @brief 線の太さを取得する (基準へ委譲)
    /// @return 基準描画オブジェクトの線幅. 基準が無い場合は基底の既定値
    double GetLineWidth() const override;

    /// @brief 現在の同期キーを計算する
    /// @return 本エンティティ自身の(ID,リビジョン)と、基準エンティティの再帰キーを
    ///         結合した値. 基準の形状変更で複製も再同期し、複製先行列の編集
    ///         (本エンティティのリビジョン変化) でバウンディングボックスが追従する
    std::uint64_t CurrentGeometryKey() const override;

    /// @brief メインの色を設定する (基準へ委譲)
    void SetColor(const std::array<float, 4>& color) override;
    /// @brief 色をデフォルトのエンティティの色に戻す (基準へ委譲)
    void ResetColor() override;

    /// @brief OpenGLリソースを解放する (基準へ委譲)
    void Cleanup() override;

 protected:
    /// @brief GPUリソースを構築・転送する (基準へ委譲; 1回ぶんのみ)
    void DoSynchronize() override;

    /// @brief 葉ノードの描画実体 (本クラスではDrawで完結するため未使用)
    void DrawImpl(graphics::gl::Uint,
                  const std::pair<float, float>&) const override {}

 private:
    /// @brief 基準エンティティの描画オブジェクト (全複製で共有する単一メッシュ)
    /// @note 生成失敗 (描画不能な基準型) の場合はnullptr
    std::unique_ptr<graphics::IEntityGraphics> source_graphics_;
};

/// @brief InstancedEntityの描画をGraphicsRegistryへ登録する
/// @note 冪等. 描画を行うアプリケーション (ビューワー等) の初期化時に一度呼ぶこと.
///       基準エンティティの既存シェーダーを利用するため、カスタムシェーダーの
///       登録は不要 (作成関数の登録のみ行う).
/// @note 本関数が拡張ライブラリ内のこの翻訳単位を参照させるため、静的ライブラリでも
///       描画クラスがリンクされる (明示登録経路).
void RegisterInstancedEntityGraphics();

}  // namespace igesio::extensions::inspection

#endif  // IGESIO_EXTENSIONS_INSPECTION_INSTANCED_ENTITY_GRAPHICS_H_
