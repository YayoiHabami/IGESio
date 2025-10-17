/**
 * @file graphics/curves/point_graphics.cpp
 * @brief 点の描画用クラス
 * @author Yayoi Habami
 * @date 2025-10-15
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/graphics/curves/point_graphics.h"

#include <memory>
#include <utility>
#include <vector>

namespace {

using igesio::graphics::PointGraphics;

}  // namespace



PointGraphics::PointGraphics(
        const std::shared_ptr<const entities::Point> entity,
        const std::shared_ptr<IOpenGL> gl)
        : EntityGraphics(entity, gl, false) {
    Synchronize();
}

PointGraphics::~PointGraphics() {
    Cleanup();
}

PointGraphics::PointGraphics(PointGraphics&& other) noexcept
        : EntityGraphics(std::move(other)),
          vbo_(other.vbo_) {
    // ムーブ元のリソース所有権を放棄させ、デストラクタで解放されないようにする
    other.vbo_ = 0;
}

PointGraphics& PointGraphics::operator=(PointGraphics&& other) noexcept {
    if (this != &other) {
        // 基底クラスのムーブ代入演算子を呼び出す
        EntityGraphics::operator=(std::move(other));

        // メンバをムーブ
        vbo_ = other.vbo_;

        // ムーブ元のリソース所有権を放棄
        other.vbo_ = 0;
    }
    return *this;
}

void PointGraphics::Cleanup() {
    EntityGraphics::Cleanup();
    if (vbo_ != 0) {
        gl_->DeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
}

bool PointGraphics::IsDrawable() const {
    if (!EntityGraphics::IsDrawable()) return false;

    // 追加の条件をチェック
    return vbo_ != 0;
}



/**
 * protected
 */

void PointGraphics::DrawImpl(
        GLuint shader, const std::pair<float, float>& viewport) const {
    gl_->BindVertexArray(vao_);
    gl_->DrawArrays(draw_mode_, 0, 1);
    gl_->BindVertexArray(0);
}

void PointGraphics::Synchronize() {
    // 既存のリソースを開放
    Cleanup();

    // OpenGLバッファのセットアップ
    // TODO: Subfigure Definitionが設定されている場合の処理
    gl_->GenVertexArrays(1, &vao_);
    gl_->GenBuffers(1, &vbo_);

    gl_->BindVertexArray(vao_);
    gl_->BindBuffer(GL_ARRAY_BUFFER, vbo_);
    gl_->BufferData(GL_ARRAY_BUFFER, sizeof(float) * 3,
                    entity_->GetPosition().cast<float>().data(), GL_STATIC_DRAW);
    draw_mode_ = GL_POINTS;

    gl_->VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    gl_->EnableVertexAttribArray(0);
    gl_->BindBuffer(GL_ARRAY_BUFFER, 0);
    gl_->BindVertexArray(0);
}
