/**
 * @file graphics/copious_data_graphics.cpp
 * @brief CopiousDataの描画用クラス
 * @author Yayoi Habami
 * @date 2025-09-12
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/graphics/curves/copious_data_graphics.h"

#include <memory>
#include <utility>
#include <vector>

namespace {

using CopiousDataGraphics = igesio::graphics::CopiousDataGraphics;
using CDType = igesio::entities::CopiousDataType;

}  // namespace

CopiousDataGraphics::CopiousDataGraphics(
        const std::shared_ptr<const entities::CopiousDataBase> entity,
        const std::shared_ptr<IOpenGL> gl)
        : EntityGraphics(entity, gl, false) {
    Synchronize();
}

CopiousDataGraphics::~CopiousDataGraphics() {
    Cleanup();
}

CopiousDataGraphics::CopiousDataGraphics(CopiousDataGraphics&& other) noexcept
        : EntityGraphics(std::move(other)),
          vbo_(other.vbo_),
          vertex_count_(other.vertex_count_) {
    other.vbo_ = 0;
    other.vertex_count_ = 0;
}

CopiousDataGraphics& CopiousDataGraphics::operator=(
        CopiousDataGraphics&& other) noexcept {
    if (this != &other) {
        EntityGraphics::operator=(std::move(other));
        vbo_ = other.vbo_;
        vertex_count_ = other.vertex_count_;
        other.vbo_ = 0;
        other.vertex_count_ = 0;
    }
    return *this;
}

void CopiousDataGraphics::Cleanup() {
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

bool CopiousDataGraphics::IsDrawable() const {
    if (!EntityGraphics::IsDrawable()) return false;
    return vbo_ != 0 && vertex_count_ > 0;
}

void CopiousDataGraphics::Synchronize() {
    Cleanup();

    const auto& coords = entity_->Coordinates();
    const auto count = entity_->GetCount();

    if (count == 0) return;

    std::vector<float> vertices;
    vertices.reserve(count * 3);
    for (int i = 0; i < count; ++i) {
        vertices.push_back(static_cast<float>(coords.col(i).x()));
        vertices.push_back(static_cast<float>(coords.col(i).y()));
        vertices.push_back(static_cast<float>(coords.col(i).z()));
    }

    auto type = entity_->GetDataType();
    if (CDType::kPlanarPoints <= type && type <= CDType::kSextuples) {
        // formが1-3の場合は、点群 (Copious Data) として描画
        draw_mode_ = GL_POINTS;
        // 点のサイズをシェーダーで制御可能にする
        gl_->Enable(GL_PROGRAM_POINT_SIZE);
    } else {
        // それ以外の場合は折れ線として描画
        draw_mode_ = GL_LINE_STRIP;
    }

    vertex_count_ = count;
    gl_->GenVertexArrays(1, &vao_);
    gl_->GenBuffers(1, &vbo_);

    gl_->BindVertexArray(vao_);
    gl_->BindBuffer(GL_ARRAY_BUFFER, vbo_);
    gl_->BufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
                    vertices.data(), GL_STATIC_DRAW);

    gl_->VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    gl_->EnableVertexAttribArray(0);

    gl_->BindBuffer(GL_ARRAY_BUFFER, 0);
    gl_->BindVertexArray(0);
}

void CopiousDataGraphics::DrawImpl(
        GLuint, const std::pair<float, float>&) const {
    if (draw_mode_ == GL_POINTS) {
        // 点のサイズを5ピクセルに設定
        gl_->PointSize(5.0f);
    }

    gl_->BindVertexArray(vao_);
    gl_->DrawArrays(draw_mode_, 0, vertex_count_);
    gl_->BindVertexArray(0);
}
