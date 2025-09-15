/**
 * @file graphics/curves/i_curve_graphics.cpp
 * @brief 曲線全般の描画用クラス
 * @author Yayoi Habami
 * @date 2025-08-06
 * @copyright 2025 Yayoi Habami
 * @brief このクラスは、ICurveを継承した全エンティティの描画を行う。
 *        曲線のパラメータ範囲で離散化を行い、OpenGLのVAOとVBOを使用して描画する。
 */
#include "igesio/graphics/curves/i_curve_graphics.h"

#include <memory>
#include <utility>
#include <vector>

namespace {

using ICurveGraphics = igesio::graphics::ICurveGraphics;

}  // namespace



ICurveGraphics::ICurveGraphics(
        const std::shared_ptr<const entities::ICurve> entity,
        const std::shared_ptr<IOpenGL> gl)
        : EntityGraphics(entity, gl, false) {
    Synchronize();
}

ICurveGraphics::~ICurveGraphics() {
    Cleanup();
}

ICurveGraphics::ICurveGraphics(ICurveGraphics&& other) noexcept
        : EntityGraphics(std::move(other)),
          vbo_(other.vbo_),
          vertex_count_(other.vertex_count_) {
    // ムーブ元のリソース所有権を放棄させ、デストラクタで解放されないようにする
    other.vbo_ = 0;
    other.vertex_count_ = 0;
}

ICurveGraphics& ICurveGraphics::operator=(ICurveGraphics&& other) noexcept {
    if (this != &other) {
        // 基底クラスのムーブ代入演算子を呼び出す
        EntityGraphics::operator=(std::move(other));

        // メンバをムーブ
        vbo_ = other.vbo_;
        vertex_count_ = other.vertex_count_;

        // ムーブ元のリソース所有権を放棄
        other.vbo_ = 0;
        other.vertex_count_ = 0;
    }
    return *this;
}

void ICurveGraphics::Cleanup() {
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

bool ICurveGraphics::IsDrawable() const {
    if (!EntityGraphics::IsDrawable()) return false;

    // 追加の条件をチェック
    return vbo_ != 0 && vertex_count_ > 0;
}



/**
 * protected
 */

void ICurveGraphics::DrawImpl(
        GLuint shader, const std::pair<float, float>& viewport) const {
    gl_->BindVertexArray(vao_);
    gl_->DrawArrays(draw_mode_, 0, vertex_count_);
    gl_->BindVertexArray(0);
}

void ICurveGraphics::Synchronize() {
    // 既存のリソースを解放
    Cleanup();

    // 頂点データを生成
    std::vector<float> vertices;
    auto [t_start, t_end] = entity_->GetParameterRange();

    // 無限のパラメータ範囲をクリップ
    if (std::isinf(t_start)) t_start = -100.0;
    if (std::isinf(t_end))   t_end = 100.0;

    unsigned int segments = 50;  // TODO: GUIによるtessellationを設定可能にする
    if (segments == 0) segments = 1;

    for (unsigned int i = 0; i <= segments; ++i) {
        double t = t_start + (t_end - t_start) * static_cast<double>(i) / segments;
        auto point_opt = entity_->TryGetPointAt(t);

        if (point_opt) {
            vertices.push_back(static_cast<float>((*point_opt)[0]));
            vertices.push_back(static_cast<float>((*point_opt)[1]));
            vertices.push_back(static_cast<float>((*point_opt)[2]));
        }
    }

    if (vertices.empty()) return;

    vertex_count_ = vertices.size() / 3;
    draw_mode_ = entity_->IsClosed() ? GL_LINE_LOOP : GL_LINE_STRIP;

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
