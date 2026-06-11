/**
 * @file graphics/core/i_entity_graphics.cpp
 * @brief 描画クラスのインターフェース定義 (EntityGraphicsの型消去クラス)
 * @author Yayoi Habami
 * @date 2025-08-07
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/graphics/core/i_entity_graphics.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "igesio/graphics/shader_registry.h"

namespace {

namespace i_graph = igesio::graphics;
using IEntityGraphics = igesio::graphics::IEntityGraphics;
using ShaderId = igesio::graphics::ShaderId;
using ShaderRegistry = igesio::graphics::ShaderRegistry;
using ShaderDrawCategory = igesio::graphics::ShaderDrawCategory;

}  // namespace



/**
 * コンストラクタ
 */

IEntityGraphics::IEntityGraphics(const std::shared_ptr<i_graph::IOpenGL>& gl,
                                 bool use_entity_transform)
        : gl_(gl), use_entity_transform_(use_entity_transform) {}

IEntityGraphics::IEntityGraphics(IEntityGraphics&& other) noexcept
        : color_{other.color_[0], other.color_[1], other.color_[2], other.color_[3]},
          is_color_overridden_(other.is_color_overridden_),
          world_transform_(std::move(other.world_transform_)),
          use_entity_transform_(other.use_entity_transform_),
          gl_(std::move(other.gl_)),
          synced_geometry_key_(other.synced_geometry_key_) {
    other.is_color_overridden_ = false;
}

IEntityGraphics& IEntityGraphics::operator=(IEntityGraphics&& other) noexcept {
    if (this != &other) {
        // メンバをムーブ
        std::copy(std::begin(other.color_), std::end(other.color_), color_);
        is_color_overridden_ = other.is_color_overridden_;
        world_transform_ = std::move(other.world_transform_);
        use_entity_transform_ = other.use_entity_transform_;
        gl_ = std::move(other.gl_);
        synced_geometry_key_ = other.synced_geometry_key_;

        // ムーブ元のフラグをリセット
        other.is_color_overridden_ = false;
        other.gl_ = nullptr;
    }
    return *this;
}



/**
 * パラメータの取得・設定
 */

void IEntityGraphics::SetGlobalParam(
        const models::GraphicsGlobalParam& global_param) {
    // 編集のためにコピーを作成
    auto copy = std::make_shared<models::GraphicsGlobalParam>(global_param);

    if (copy->max_line_weight <= 1) {
        // 線の最大太さが1(多くのデフォルト)の場合、本ライブラリの仕様に合わせて拡張する
        copy->max_line_weight = models::kDefaultMaxLineWeight;
    }
    if (copy->line_weight_gradations <= 1) {
        // 線の太さの最大数が1(IGES仕様のデフォルト)の場合、本ライブラリの仕様に合わせて拡張する
        copy->line_weight_gradations = models::kDefaultLineWeightGradations;
    }

    global_param_ = copy;
}

void IEntityGraphics::SetWorldTransform(const igesio::Matrix4d& matrix) {
    world_transform_ = matrix;
}

void IEntityGraphics::SetColor(const std::array<float, 4>&color) {
    std::copy(color.begin(), color.end(), color_);
    is_color_overridden_ = true;
}



/**
 * その他
 */

bool i_graph::HasSpecificShaderCode(const ShaderId shader_id) {
    const auto* info = ShaderRegistry::Get(shader_id);
    return info != nullptr && !info->code.IsIncomplete();
}

bool i_graph::IsSurfaceFill(const ShaderId shader_id) {
    const auto* info = ShaderRegistry::Get(shader_id);
    return info != nullptr &&
           info->category == ShaderDrawCategory::kSurfaceFill;
}

std::string i_graph::ToString(const ShaderId shader_id) {
    const auto* info = ShaderRegistry::Get(shader_id);
    return (info != nullptr) ? info->name : "Unknown";
}

bool i_graph::UsesLighting(const ShaderId shader_id) {
    const auto* info = ShaderRegistry::Get(shader_id);
    return info != nullptr && info->uses_lighting;
}
