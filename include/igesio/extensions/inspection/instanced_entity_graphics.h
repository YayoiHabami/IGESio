/**
 * @file extensions/inspection/instanced_entity_graphics.h
 * @brief InstancedEntity (複製表示) の描画クラス
 * @author Yayoi Habami
 * @date 2026-06-12
 * @copyright 2026 Yayoi Habami
 * @note 各メンバの描画オブジェクト (テッセレーション結果のGPUバッファ) を
 *       メンバごとに1つだけ生成・同期し、その同一メッシュを各複製先行列の位置へ
 *       繰り返し描画する. 描画時に各メンバ描画オブジェクトのworld_transform_を
 *       複製ごとに差し替えるため、N個複製してもCPUテッセレーション・GPUバッファは
 *       メンバ数ぶん (複製数には非依存) で済む.
 *       IGESIO_ENABLE_GRAPHICS有効時にのみビルドされる.
 * @note 複製先行列 · 局所配置を畳んだworld行列 (`world_transform_ · 複製先行列 ·
 *       local_placement`) は reconcile時 (SetWorldTransform/DoSynchronize) にのみ
 *       前計算してキャッシュし、毎フレームのDrawではこれを流すだけにする. これにより
 *       カメラ回転等で行列乗算がメンバ数・複製数に比例して増えるのを避ける.
 * @note 描画を配線するには `RegisterInstancedEntityGraphics()` を一度呼ぶこと
 *       (GraphicsRegistryへの作成関数登録を行う. 各メンバの既存シェーダーを
 *       そのまま使うため、固有のカスタムシェーダーは持たない).
 */
#ifndef IGESIO_EXTENSIONS_INSPECTION_INSTANCED_ENTITY_GRAPHICS_H_
#define IGESIO_EXTENSIONS_INSPECTION_INSTANCED_ENTITY_GRAPHICS_H_

#include <array>
#include <cstdint>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/graphics/core/entity_graphics.h"
#include "igesio/extensions/inspection/instanced_entity.h"

namespace igesio::extensions::inspection {

/// @brief InstancedEntity (複製表示) の描画クラス
/// @note GraphicsRegistry経由で生成される (登録は`RegisterInstancedEntityGraphics`).
///       各メンバの描画オブジェクトを専用メンバ列で保持し、描画・同期・テクスチャ転送・
///       色設定を委譲する. 子は`child_graphics_`ではなく専用メンバで保持し、
///       複製描画 (複数行列での描き分け) を本クラスで制御する.
/// @note シェーダー型は各メンバ描画オブジェクトのものをそのまま公開する (固有の
///       シェーダーは持たない). レンダラのバケットには各メンバの各シェーダー型で収集される.
class InstancedEntityGraphics
        : public graphics::EntityGraphics<InstancedEntity, false> {
 public:
    /// @brief コンストラクタ
    /// @param entity 描画する複製表示エンティティ
    /// @param gl OpenGL関数のラッパー
    /// @throw std::invalid_argument entityがnullptrの場合
    /// @note 各メンバの描画オブジェクトを生成する (同期はreconcile経路が駆動).
    ///       描画不能な型のメンバはnullptrとなり、そのメンバは描画されない.
    InstancedEntityGraphics(
            const std::shared_ptr<const InstancedEntity>&,
            const std::shared_ptr<graphics::IOpenGL>&);

    /// @brief デストラクタ
    ~InstancedEntityGraphics() override;

    // 基底のDrawオーバーロード (3引数版) を可視に保つ
    using EntityGraphics::Draw;

    /// @brief エンティティの描画を行う (各複製先行列で各メンバメッシュを描き分ける)
    /// @note 各メンバについて、事前合成済みのworld行列 (instance_world_) を
    ///       複製数ぶん差し替えて描画を委譲する. 同一のVAO/GPUバッファをmodel行列のみ
    ///       差し替えて使い回すため、テッセレーション・転送・行列乗算は発生しない.
    void Draw(graphics::gl::Uint shader, graphics::ShaderId shader_id,
              const std::pair<float, float>& viewport,
              const graphics::DrawContext& ctx) const override;

    /// @brief 全ての可能なシェーダータイプを取得する
    /// @return 各メンバ描画オブジェクトのシェーダー型の和集合 (メンバが無い場合は空)
    std::unordered_set<graphics::ShaderId> GetShaderIds() const override;

    /// @brief 描画用CPUデータ (各メンバのテッセレーション) を事前構築する
    /// @note 各メンバ描画オブジェクトのPrewarmCpuへ委譲する (メンバ数ぶんのみ)
    void PrewarmCpu() override;

    /// @brief テクスチャ用の描画リソースを同期する (各メンバへ委譲)
    void SyncTexture() override;

    /// @brief 描画可能な状態かどうかを確認する
    /// @return 複製先行列が1つ以上あり、描画可能なメンバが1つ以上ある場合はtrue
    bool IsDrawable() const override;

    /// @brief 線の太さを取得する (先頭の描画可能メンバへ委譲)
    /// @return メンバ描画オブジェクトの線幅. メンバが無い場合は基底の既定値
    double GetLineWidth() const override;

    /// @brief 現在の同期キーを計算する
    /// @return 本エンティティ自身の(ID,リビジョン)と、各メンバの再帰キーを
    ///         結合した値. メンバの形状変更で複製も再同期し、複製先行列の編集
    ///         (本エンティティのリビジョン変化) でバウンディングボックスが追従する
    std::uint64_t CurrentGeometryKey() const override;

    /// @brief グローバル座標系への変換行列を設定する (合成world行列キャッシュを再構築)
    /// @param matrix このエンティティのworld変換 (親→モデル空間)
    /// @note 子伝播は行わず (メンバは専用に保持する)、world_transform_を保存後に
    ///       instance_world_ を作り直す. reconcile時にのみ呼ばれる.
    void SetWorldTransform(const igesio::Matrix4d& matrix) override;

    /// @brief メインの色を設定する (各メンバへ委譲)
    void SetColor(const std::array<float, 4>& color) override;
    /// @brief 色をデフォルトのエンティティの色に戻す (各メンバへ委譲)
    void ResetColor() override;

    /// @brief OpenGLリソースを解放する (各メンバへ委譲)
    void Cleanup() override;

 protected:
    /// @brief GPUリソースを構築・転送する (各メンバへ委譲; メンバ数ぶんのみ)
    /// @note 末尾で合成world行列キャッシュ (instance_world_) を再構築する
    ///       (複製先行列の編集を反映するため)
    void DoSynchronize() override;

    /// @brief 葉ノードの描画実体 (本クラスではDrawで完結するため未使用)
    void DrawImpl(graphics::gl::Uint,
                  const std::pair<float, float>&) const override {}

 private:
    /// @brief 合成world行列キャッシュを再構築する
    /// @note world_transform_ と entity_->Members()/Transforms() から
    ///       instance_world_ を作り直す. メンバ数×複製数の行列をここで1回だけ合成する.
    void RebuildInstanceTransforms();

    /// @brief 各メンバの描画オブジェクト (メンバごとに1つ; 全複製で共有する単一メッシュ)
    /// @note 生成失敗 (描画不能なメンバ型) の要素はnullptr. entity_->Members()と並列.
    std::vector<std::unique_ptr<graphics::IEntityGraphics>> member_graphics_;

    /// @brief 複製ごとの合成world行列キャッシュ
    /// @note instance_world_[member][replica] =
    ///       world_transform_ · 複製先行列 · local_placement (M_entityはメンバ側が適用).
    ///       reconcile時 (SetWorldTransform/DoSynchronize) にのみ再構築し、Drawは
    ///       これを流すだけにすることで、毎フレーム描画でのCPU行列乗算を排する.
    std::vector<std::vector<igesio::Matrix4d>> instance_world_;
};

/// @brief InstancedEntityの描画をGraphicsRegistryへ登録する
/// @note 冪等. 描画を行うアプリケーション (ビューワー等) の初期化時に一度呼ぶこと.
///       各メンバの既存シェーダーを利用するため、カスタムシェーダーの登録は不要
///       (作成関数の登録のみ行う).
/// @note 本関数が拡張ライブラリ内のこの翻訳単位を参照させるため、静的ライブラリでも
///       描画クラスがリンクされる (明示登録経路).
void RegisterInstancedEntityGraphics();

}  // namespace igesio::extensions::inspection

#endif  // IGESIO_EXTENSIONS_INSPECTION_INSTANCED_ENTITY_GRAPHICS_H_
