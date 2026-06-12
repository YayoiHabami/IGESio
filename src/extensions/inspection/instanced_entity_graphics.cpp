/**
 * @file extensions/inspection/instanced_entity_graphics.cpp
 * @brief InstancedEntity (複製表示) の描画クラス
 * @author Yayoi Habami
 * @date 2026-06-12
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/inspection/instanced_entity_graphics.h"

#include <array>
#include <cstdint>
#include <memory>
#include <unordered_set>
#include <utility>

#include "igesio/graphics/factory.h"
#include "igesio/graphics/graphics_registry.h"

namespace igesio::extensions::inspection {

namespace {

namespace gl = igesio::graphics::gl;

/// @brief GraphicsRegistry用の描画オブジェクト作成関数
/// @param entity 描画する複製表示エンティティ
/// @param gl OpenGL関数のラッパー
/// @return 生成した描画オブジェクト
std::unique_ptr<graphics::IEntityGraphics> CreateGraphics(
        const std::shared_ptr<const InstancedEntity>& entity,
        const std::shared_ptr<graphics::IOpenGL>& gl) {
    return std::make_unique<InstancedEntityGraphics>(entity, gl);
}

}  // namespace



InstancedEntityGraphics::InstancedEntityGraphics(
        const std::shared_ptr<const InstancedEntity>& entity,
        const std::shared_ptr<graphics::IOpenGL>& gl)
        : EntityGraphics(entity, gl, graphics::ShaderId::kComposite, false) {
    // 基準エンティティの描画オブジェクトを1つだけ生成する (全複製で共有する単一メッシュ).
    // 同期 (CPU構築+GL転送) はレンダラのreconcile経路が駆動するため、生成時は同期しない.
    if (entity_ && entity_->Source()) {
        source_graphics_ = graphics::CreateEntityGraphics(
                entity_->Source(), gl, /*synchronize=*/false);
    }
}

InstancedEntityGraphics::~InstancedEntityGraphics() {
    Cleanup();
}

void InstancedEntityGraphics::Draw(
        gl::Uint shader, const graphics::ShaderId shader_id,
        const std::pair<float, float>& viewport,
        const graphics::DrawContext& ctx) const {
    if (!source_graphics_ || !entity_) return;

    // 複製表示自身が選択中なら、基準 (別ID) の描画にハイライトを強制する
    graphics::DrawContext child_ctx = ctx;
    if (ctx.IsHighlighted(GetEntityID())) child_ctx.force_highlight = true;

    // 各複製先行列について、基準のworld変換を差し替えてから描画を委譲する.
    // 同一のVAO/GPUバッファをmodel行列のみ変えて使い回すため、テッセレーション・
    // 転送は発生しない (描画コールのみが複製数ぶん走る).
    for (const auto& transform : entity_->Transforms()) {
        source_graphics_->SetWorldTransform(world_transform_ * transform);
        source_graphics_->Draw(shader, shader_id, viewport, child_ctx);
    }
}

std::unordered_set<igesio::graphics::ShaderId>
InstancedEntityGraphics::GetShaderIds() const {
    if (!source_graphics_) return {};
    return source_graphics_->GetShaderIds();
}

void InstancedEntityGraphics::PrewarmCpu() {
    if (source_graphics_) source_graphics_->PrewarmCpu();
}

void InstancedEntityGraphics::SyncTexture() {
    if (source_graphics_) source_graphics_->SyncTexture();
}

bool InstancedEntityGraphics::IsDrawable() const {
    if (!source_graphics_ || !entity_) return false;
    return source_graphics_->IsDrawable() && !entity_->Transforms().empty();
}

double InstancedEntityGraphics::GetLineWidth() const {
    if (source_graphics_) return source_graphics_->GetLineWidth();
    return EntityGraphics::GetLineWidth();
}

std::uint64_t InstancedEntityGraphics::CurrentGeometryKey() const {
    std::uint64_t key = 0;
    // 自身の(ID,リビジョン) — 複製先行列の編集 (リビジョン変化) を検知する
    if (entity_) key = graphics::CombineGeometryKey(key, *entity_);
    // 基準の再帰キー — 基準形状の変更で複製も再同期させる
    if (entity_ && entity_->Source()) {
        key = graphics::CombineGeometryKeyRecursive(key, *entity_->Source());
    }
    return key;
}

void InstancedEntityGraphics::SetColor(const std::array<float, 4>& color) {
    EntityGraphics::SetColor(color);
    if (source_graphics_) source_graphics_->SetColor(color);
}

void InstancedEntityGraphics::ResetColor() {
    EntityGraphics::ResetColor();
    if (source_graphics_) source_graphics_->ResetColor();
}

void InstancedEntityGraphics::DoSynchronize() {
    if (!source_graphics_) return;
    // 線幅・材質の既定に用いるグローバルパラメータを基準へ転送する
    // (基準は単独でレンダラに収集されないため、本クラスが肩代わりする)
    if (global_param_) source_graphics_->SetGlobalParam(*global_param_);
    // 基準を一度だけ同期する (CPU構築はPrewarmCpuで前倒し済みならGL転送のみ)
    source_graphics_->Synchronize();
}

void InstancedEntityGraphics::Cleanup() {
    if (source_graphics_) source_graphics_->Cleanup();
    EntityGraphics::Cleanup();
}

void RegisterInstancedEntityGraphics() {
    // 描画オブジェクト作成関数を登録する (冪等; 二重登録は無害).
    // 基準エンティティの既存シェーダーを利用するためカスタムシェーダーは不要.
    graphics::GraphicsRegistry::TryRegister<InstancedEntity>(&CreateGraphics);
}

}  // namespace igesio::extensions::inspection
