/**
 * @file graphics/curves/circular_arc_graphics.cpp
 * @brief  CircularArcの描画用クラス
 * @author Yayoi Habami
 * @date 2025-08-08
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/graphics/curves/circular_arc_graphics.h"

#include <memory>
#include <utility>

namespace {

using igesio::graphics::CircularArcGraphics;

}  // namespace



CircularArcGraphics::CircularArcGraphics(
        const std::shared_ptr<const entities::CircularArc> entity,
        const std::shared_ptr<IOpenGL> gl)
        : EntityGraphics(entity, gl, true) {
    Synchronize();
}

CircularArcGraphics::~CircularArcGraphics() {
    Cleanup();
}



/**
 * protected
 */

void CircularArcGraphics::DrawImpl(
        GLuint shader, const std::pair<float, float>& viewport) const {
    // シェーダーのuniform変数を設定
    auto center = entity_->Center();
    gl_->Uniform3f(gl_->GetUniformLocation(shader, "center"),
                   static_cast<float>(center[0]),
                   static_cast<float>(center[1]),
                   static_cast<float>(center[2]));

    gl_->Uniform1f(gl_->GetUniformLocation(shader, "radius"),
                   static_cast<float>(entity_->Radius()));
    gl_->Uniform1f(gl_->GetUniformLocation(shader, "startAngle"),
                   static_cast<float>(entity_->StartAngle()));
    gl_->Uniform1f(gl_->GetUniformLocation(shader, "endAngle"),
                   static_cast<float>(entity_->EndAngle()));

    gl_->Uniform2f(gl_->GetUniformLocation(shader, "viewportSize"),
                   viewport.first, viewport.second);

    // 描画
    gl_->BindVertexArray(vao_);
    // 1つの頂点からなるパッチを1つ描画する
    gl_->DrawArrays(GL_PATCHES, 0, 1);
    gl_->BindVertexArray(0);
}

void CircularArcGraphics::Synchronize() {
    // 既存のリソースを解放
    Cleanup();

    // テッセレーションシェーダーを使用するため、空のVAOを生成する
    gl_->GenVertexArrays(1, &vao_);

    // パッチの頂点数を1に設定
    // この設定はVAOに保存されるため、描画のたびに呼び出す必要はない
    gl_->PatchParameteri(GL_PATCH_VERTICES, 1);

    if (entity_->IsClosed()) {
        draw_mode_ = GL_LINE_LOOP;
    } else {
        draw_mode_ = GL_LINE_STRIP;
    }
}
