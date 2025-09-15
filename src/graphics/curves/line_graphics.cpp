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

using igesio::graphics::SegmentGraphics;
using igesio::graphics::LineGraphics;
namespace i_ent = igesio::entities;

}  // namespace




/**
 * SegmentGraphics
 */

SegmentGraphics::SegmentGraphics(
        const std::shared_ptr<const entities::Line> entity,
        const std::shared_ptr<IOpenGL> gl)
        : EntityGraphics(entity, gl, true) {
    Synchronize();
}

SegmentGraphics::~SegmentGraphics() {
    Cleanup();
}

SegmentGraphics::SegmentGraphics(SegmentGraphics&& other) noexcept
        : EntityGraphics(std::move(other)),
          vbo_(other.vbo_),
          vertex_count_(other.vertex_count_) {
    other.vbo_ = 0;
    other.vertex_count_ = 0;
}

SegmentGraphics& SegmentGraphics::operator=(SegmentGraphics&& other) noexcept {
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

void SegmentGraphics::Cleanup() {
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

bool SegmentGraphics::IsDrawable() const {
    if (!EntityGraphics::IsDrawable()) return false;

    // 追加の条件をチェック
    return vbo_ != 0 && vertex_count_ > 0;
}



/**
 * SegmentGraphics protected
 */

void SegmentGraphics::DrawImpl(
        GLuint shader, const std::pair<float, float>& viewport) const {
    gl_->BindVertexArray(vao_);
    gl_->DrawArrays(draw_mode_, 0, vertex_count_);
    gl_->BindVertexArray(0);
}

void SegmentGraphics::Synchronize() {
    // 既存のリソースを開放
    Cleanup();

    // 端点を計算
    // 直線/半直線の場合は、無限遠点側を`far_length_`だけ延長する
    auto start = entity_->GetAnchorPoints().first;
    auto end = entity_->GetAnchorPoints().second;
    if (entity_->GetLineType() == i_ent::LineType::kRay) {
        end = end + (end - start).normalized() * far_length_;
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




/**
 * LineGraphics
 */

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
    gl_->UseProgram(shader);

    auto line_type = entity_->GetLineType();
    gl_->Uniform1i(gl_->GetUniformLocation(shader, "lineType"), static_cast<int>(line_type));

    // kRayまたはkLineの場合、ジオメトリシェーダーにuniform変数を渡す
    gl_->Uniform1f(gl_->GetUniformLocation(shader, "farLength"), far_length_);

    gl_->BindVertexArray(vao_);
    gl_->DrawArrays(draw_mode_, 0, vertex_count_);
    gl_->BindVertexArray(0);

    gl_->UseProgram(0);
}

void LineGraphics::Synchronize() {
    // 既存のリソースを開放
    Cleanup();

    auto start = entity_->GetAnchorPoints().first;
    auto end = entity_->GetAnchorPoints().second;
    auto dir = (end - start).normalized();

    std::vector<float> vertices;
    auto line_type = entity_->GetLineType();

    // 始点と方向ベクトルを渡す
    vertices = {
        static_cast<float>(start.x()), static_cast<float>(start.y()),
        static_cast<float>(start.z()), static_cast<float>(dir.x()),
        static_cast<float>(dir.y()), static_cast<float>(dir.z())
    };
    vertex_count_ = 1;
    draw_mode_ = GL_POINTS;


    // OpenGLバッファのセットアップ
    gl_->GenVertexArrays(1, &vao_);
    gl_->GenBuffers(1, &vbo_);

    gl_->BindVertexArray(vao_);
    gl_->BindBuffer(GL_ARRAY_BUFFER, vbo_);
    gl_->BufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
                    vertices.data(), GL_STATIC_DRAW);

    // 全てのlineTypeで共通の頂点属性設定を使用
    // 位置属性 (aPos)
    gl_->VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    gl_->EnableVertexAttribArray(0);
    // 方向属性 (aDir)
    gl_->VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                             (void*)(3 * sizeof(float)));
    gl_->EnableVertexAttribArray(1);

    gl_->BindBuffer(GL_ARRAY_BUFFER, 0);
    gl_->BindVertexArray(0);
}
