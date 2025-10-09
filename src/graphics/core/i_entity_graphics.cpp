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

namespace {

namespace i_graph = igesio::graphics;
using IEntityGraphics = igesio::graphics::IEntityGraphics;
using ShaderType = igesio::graphics::ShaderType;

}  // namespace



/**
 * コンストラクタ
 */

IEntityGraphics::IEntityGraphics(const std::shared_ptr<i_graph::IOpenGL> gl,
                                 bool use_entity_transform)
        : gl_(gl), use_entity_transform_(use_entity_transform) {}

IEntityGraphics::IEntityGraphics(IEntityGraphics&& other) noexcept
        : color_{other.color_[0], other.color_[1], other.color_[2], other.color_[3]},
          is_color_overridden_(other.is_color_overridden_),
          world_transform_(std::move(other.world_transform_)),
          use_entity_transform_(other.use_entity_transform_),
          gl_(std::move(other.gl_)) {
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
        const std::shared_ptr<const models::GraphicsGlobalParam> global_param) {
    global_param_ = global_param;
}

void IEntityGraphics::SetWorldTransform(const igesio::Matrix4f& matrix) {
    world_transform_ = matrix;
}

void IEntityGraphics::SetColor(const std::array<GLfloat, 4>&color) {
    std::copy(color.begin(), color.end(), color_);
    is_color_overridden_ = true;
}



/**
 * その他
 */

std::string i_graph::ToString(const ShaderType shader_type) {
    switch (shader_type) {
        case ShaderType::kNone:
            return "None";
        case ShaderType::kGeneralCurve:
            return "GeneralCurve";
        case ShaderType::kCircularArc:
            return "CircularArc";
        case ShaderType::kEllipse:
            return "Ellipse";
        case ShaderType::kCopiousData:
            return "CopiousData";
        case ShaderType::kSegment:
            return "Segment";
        case ShaderType::kLine:
            return "Line";
        case ShaderType::kRationalBSplineCurve:
            return "RationalBSplineCurve";
        case ShaderType::kComposite:
            return "Composite";
        default:
            return "Unknown";
    }
}
