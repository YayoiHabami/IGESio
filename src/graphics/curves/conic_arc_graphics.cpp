/**
 * @file graphics/curves/conic_arc_graphics.cpp
 * @brief ConicArcの描画用クラス
 * @author Yayoi Habami
 * @date 2025-08-09
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/graphics/curves/conic_arc_graphics.h"

#include <memory>
#include <utility>

namespace {

using EllipseGraphics = igesio::graphics::EllipseGraphics;

}  // namespace



/**
 * コンストラクタ
 */

EllipseGraphics::EllipseGraphics(
        const std::shared_ptr<const entities::ConicArc> entity,
        const std::shared_ptr<IOpenGL> gl)
        : EntityGraphics(entity, gl, true) {
    if (entity->GetConicType() != entities::ConicType::kEllipse) {
        throw std::invalid_argument("Entity is not an ellipse.");
    }
    Synchronize();
}

EllipseGraphics::~EllipseGraphics() {
    Cleanup();
}



/**
 * protected
 */

void EllipseGraphics::DrawImpl(
        GLuint shader, const std::pair<float, float>& viewport) const {
    // シェーダーのuniform変数を設定
    auto center = entity_->EllipseCenter();
    gl_->Uniform3f(gl_->GetUniformLocation(shader, "center"),
                   static_cast<float>(center[0]),
                   static_cast<float>(center[1]),
                   static_cast<float>(center[2]));

    int segments = 50;  // TODO: 自動で計算できるようにする
    auto radius = entity_->EllipseRadii();
    gl_->Uniform1f(gl_->GetUniformLocation(shader, "radiusX"),
                   static_cast<float>(radius.first));
    gl_->Uniform1f(gl_->GetUniformLocation(shader, "radiusY"),
                   static_cast<float>(radius.second));
    gl_->Uniform1f(gl_->GetUniformLocation(shader, "startAngle"),
                   static_cast<float>(entity_->EllipseStartAngle()));
    gl_->Uniform1f(gl_->GetUniformLocation(shader, "endAngle"),
                   static_cast<float>(entity_->EllipseEndAngle()));
    gl_->Uniform1i(gl_->GetUniformLocation(shader, "segments"), segments);

    // 描画
    gl_->BindVertexArray(vao_);
    // 頂点数はセグメント数+1
    gl_->DrawArrays(draw_mode_, 0, segments + 1);
    gl_->BindVertexArray(0);
}

void EllipseGraphics::Synchronize() {
    // 既存のリソースを解放
    Cleanup();

    // CircularArcのシェーダーはGPUで頂点を生成するため、
    // VBOへのデータ転送は不要。空のVAOを生成するだけでよい。
    gl_->GenVertexArrays(1, &vao_);

    if (entity_->IsClosed()) {
        draw_mode_ = GL_LINE_LOOP;
    } else {
        draw_mode_ = GL_LINE_STRIP;
    }
}
