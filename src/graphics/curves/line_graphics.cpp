/**
 * @file graphics/curves/line_graphics.cpp
 * @brief 線分/半直線/直線の描画用クラス
 * @author Yayoi Habami
 * @date 2025-08-10
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/graphics/curves/line_graphics.h"

#include <memory>
#include <utility>
#include <vector>

namespace {

using LineGraphics = igesio::graphics::LineGraphics;
namespace i_ent = igesio::entities;

}  // namespace



LineGraphics::LineGraphics(
        const std::shared_ptr<const entities::Line> entity,
        const std::shared_ptr<IOpenGL> gl)
        : EntityGraphics(entity, gl, true) {
    Synchronize();
}

LineGraphics::~LineGraphics() {
    Cleanup();
}

LineGraphics::LineGraphics(LineGraphics&& other) noexcept
        : EntityGraphics(std::move(other)),
          vbo_(other.vbo_),
          vertex_count_(other.vertex_count_) {
    other.vbo_ = 0;
    other.vertex_count_ = 0;
}

LineGraphics& LineGraphics::operator=(LineGraphics&& other) noexcept {
    if (this != &other) {
        // 基底クラスのムーブ代入演算子を呼び出す
        EntityGraphics::operator=(std::move(other));

        // メンバをムーブ
        vbo_ = other.vbo_;
        vertex_count_ = other.vertex_count_;

        other.vbo_ = 0;
        other.vertex_count_ = 0;
    }
    return *this;
}

void LineGraphics::Cleanup() {
    if (vbo_ != 0) {
        gl_->DeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (vao_ != 0) {
        gl_->DeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    vertex_count_ = 0;
}

bool LineGraphics::IsDrawable() const {
    if (!EntityGraphics::IsDrawable()) return false;

    // 追加の条件をチェック
    return vbo_ != 0 && vertex_count_ > 0;
}



/**
 * protected
 */

void LineGraphics::DrawImpl(
        GLuint shader, const std::pair<float, float>& viewport) const {
    gl_->BindVertexArray(vao_);
    gl_->DrawArrays(draw_mode_, 0, vertex_count_);
    gl_->BindVertexArray(0);
}

void LineGraphics::Synchronize() {
    // 既存のリソースを開放
    Cleanup();

    // 端点を計算
    // 直線/半直線の場合は、無限遠点側を`far_length_`だけ延長する
    auto start = entity_->GetAnchorPoints().first;
    auto end = entity_->GetAnchorPoints().second;

    auto dir = (end - start).normalized();
    switch (entity_->GetLineType()) {
        case i_ent::LineType::kSegment:
            break;
        case i_ent::LineType::kRay:
            end = end + dir * far_length_;
            break;
        case i_ent::LineType::kLine:
            start = start - dir * far_length_;
            end = end + dir * far_length_;
            break;
    }

    // 頂点を設定
    std::vector<float> vertices = {
            static_cast<float>(start.x()), static_cast<float>(start.y()),
            static_cast<float>(start.z()), static_cast<float>(end.x()),
            static_cast<float>(end.y()), static_cast<float>(end.z())};
    vertex_count_ = 2;
    draw_mode_ = GL_LINES;

    // OpenGLバッファのセットアップ
    gl_->GenVertexArrays(1, &vao_);
    gl_->GenBuffers(1, &vbo_);

    gl_->BindVertexArray(vao_);
    gl_->BindBuffer(GL_ARRAY_BUFFER, vbo_);
    gl_->BufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
                    vertices.data(), GL_STATIC_DRAW);

    gl_->VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                             3 * sizeof(float), (void*)0);
    gl_->EnableVertexAttribArray(0);

    gl_->BindBuffer(GL_ARRAY_BUFFER, 0);
    gl_->BindVertexArray(0);
}
