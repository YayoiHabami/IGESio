/**
 * @file extensions/inspection/instanced_entity_graphics.cpp
 * @brief InstancedEntity (複製表示) の描画クラス
 * @author Yayoi Habami
 * @date 2026-06-12
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/inspection/instanced_entity_graphics.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

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
    // 各メンバの描画オブジェクトを1つずつ生成する (全複製で共有する単一メッシュ).
    // 同期 (CPU構築+GL転送) はレンダラのreconcile経路が駆動するため、生成時は同期しない.
    if (!entity_) return;
    member_graphics_.reserve(entity_->Members().size());
    for (const auto& member : entity_->Members()) {
        member_graphics_.push_back(graphics::CreateEntityGraphics(
                member.source, gl, /*synchronize=*/false));
    }
}

InstancedEntityGraphics::~InstancedEntityGraphics() {
    Cleanup();
}

void InstancedEntityGraphics::RebuildInstanceTransforms() {
    instance_world_.clear();
    if (!entity_) return;
    const auto& members = entity_->Members();
    const auto& transforms = entity_->Transforms();
    instance_world_.resize(members.size());
    for (std::size_t i = 0; i < members.size(); ++i) {
        auto& worlds = instance_world_[i];
        worlds.reserve(transforms.size());
        for (const auto& t : transforms) {
            // world · 複製先行列 · 局所配置 を1回だけ合成する (M_entityはメンバ側が適用)
            worlds.push_back(world_transform_ * t * members[i].local_placement);
        }
    }
}

void InstancedEntityGraphics::SetWorldTransform(const igesio::Matrix4d& matrix) {
    world_transform_ = matrix;
    RebuildInstanceTransforms();
}

void InstancedEntityGraphics::Draw(
        gl::Uint shader, const graphics::ShaderId shader_id,
        const std::pair<float, float>& viewport,
        const graphics::DrawContext& ctx) const {
    if (member_graphics_.empty() || !entity_) return;

    // 複製表示自身が選択中なら、各メンバ (別ID) の描画にハイライトを強制する
    graphics::DrawContext child_ctx = ctx;
    if (ctx.IsHighlighted(GetEntityID())) child_ctx.force_highlight = true;

    // 各メンバを、事前合成済みのworld行列 (instance_world_) で複製数ぶん描き分ける.
    // 同一のVAO/GPUバッファをmodel行列だけ差し替えて使い回すため、テッセレーション・
    // 転送は発生せず、描画フェーズでの行列乗算も無い (キャッシュを流すのみ).
    for (std::size_t i = 0; i < member_graphics_.size(); ++i) {
        const auto& g = member_graphics_[i];
        if (!g || i >= instance_world_.size()) continue;
        for (const auto& w : instance_world_[i]) {
            g->SetWorldTransform(w);
            g->Draw(shader, shader_id, viewport, child_ctx);
        }
    }
}

std::unordered_set<igesio::graphics::ShaderId>
InstancedEntityGraphics::GetShaderIds() const {
    std::unordered_set<graphics::ShaderId> ids;
    for (const auto& g : member_graphics_) {
        if (!g) continue;
        auto s = g->GetShaderIds();
        ids.merge(s);
    }
    return ids;
}

void InstancedEntityGraphics::PrewarmCpu() {
    for (auto& g : member_graphics_) {
        if (g) g->PrewarmCpu();
    }
}

void InstancedEntityGraphics::SyncTexture() {
    // 材質 (metallic/roughness/ao/opacity/テクスチャ) は描画時に各メンバ自身の
    // material_property_ が読まれるため、本クラスへ設定された材質をメンバへ複写する.
    // レンダラは材質オーバーライドの適用直後に本関数を呼ぶ (材質適用のチョークポイント)
    // ため、ここで複写すればテクスチャのGL転送も同一呼び出し内で完結する.
    for (auto& g : member_graphics_) {
        if (!g) continue;
        g->MaterialProperty() = material_property_;
        g->SyncTexture();
    }
}

bool InstancedEntityGraphics::IsDrawable() const {
    if (!entity_ || entity_->Transforms().empty()) return false;
    // 描画可能なメンバが1つでもあれば描画可 (描画不能な型のメンバはスキップして描く)
    for (const auto& g : member_graphics_) {
        if (g && g->IsDrawable()) return true;
    }
    return false;
}

double InstancedEntityGraphics::GetLineWidth() const {
    for (const auto& g : member_graphics_) {
        if (g) return g->GetLineWidth();
    }
    return EntityGraphics::GetLineWidth();
}

std::uint64_t InstancedEntityGraphics::CurrentGeometryKey() const {
    std::uint64_t key = 0;
    if (!entity_) return key;
    // 自身の(ID,リビジョン) — 複製先行列の編集 (リビジョン変化) を検知する
    key = graphics::CombineGeometryKey(key, *entity_);
    // 各メンバの再帰キー — メンバ形状の変更で複製も再同期させる
    for (const auto& member : entity_->Members()) {
        if (member.source) {
            key = graphics::CombineGeometryKeyRecursive(key, *member.source);
        }
    }
    return key;
}

void InstancedEntityGraphics::SetColor(const std::array<float, 4>& color) {
    EntityGraphics::SetColor(color);
    for (auto& g : member_graphics_) {
        if (g) g->SetColor(color);
    }
}

void InstancedEntityGraphics::ResetColor() {
    EntityGraphics::ResetColor();
    for (auto& g : member_graphics_) {
        if (g) g->ResetColor();
    }
}

void InstancedEntityGraphics::DoSynchronize() {
    for (auto& g : member_graphics_) {
        if (!g) continue;
        // 線幅・材質の既定に用いるグローバルパラメータをメンバへ転送する
        // (メンバは単独でレンダラに収集されないため、本クラスが肩代わりする)
        if (global_param_) g->SetGlobalParam(*global_param_);
        // 各メンバを一度だけ同期する (CPU構築はPrewarmCpuで前倒し済みならGL転送のみ)
        g->Synchronize();
    }
    // 複製先行列の編集等を合成world行列キャッシュへ反映する
    RebuildInstanceTransforms();
}

void InstancedEntityGraphics::Cleanup() {
    for (auto& g : member_graphics_) {
        if (g) g->Cleanup();
    }
    EntityGraphics::Cleanup();
}

void RegisterInstancedEntityGraphics() {
    // 描画オブジェクト作成関数を登録する (冪等; 二重登録は無害).
    // 各メンバの既存シェーダーを利用するためカスタムシェーダーは不要.
    graphics::GraphicsRegistry::TryRegister<InstancedEntity>(&CreateGraphics);
}

}  // namespace igesio::extensions::inspection
