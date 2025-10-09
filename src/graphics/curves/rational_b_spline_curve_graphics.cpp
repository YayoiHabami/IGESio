/**
 * @file graphics/curves/rational_b_spline_curve_graphics.cpp
 * @brief RationalBSplineCurveの描画用クラス
 * @author Yayoi Habami
 * @date 2025-08-15
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/graphics/curves/rational_b_spline_curve_graphics.h"

#include <memory>
#include <utility>
#include <vector>

#include "./../core/type_conversion_utils.h"

namespace {

using RationalBSplineCurveGraphics = igesio::graphics::RationalBSplineCurveGraphics;

/// @brief MAX_DEGREEの値 (kRationalBSplineCurveShader)
constexpr size_t kMaxDegree = 5;

}  // namespace



RationalBSplineCurveGraphics::RationalBSplineCurveGraphics(
        const std::shared_ptr<const entities::RationalBSplineCurve> entity,
        const std::shared_ptr<IOpenGL> gl)
        : EntityGraphics(entity, gl, true) {
    Synchronize();
}

RationalBSplineCurveGraphics::~RationalBSplineCurveGraphics() {
    Cleanup();
}



/**
 * protected
 */

void RationalBSplineCurveGraphics::DrawImpl(
        GLuint shader, const std::pair<float, float>& viewport) const {
    // シェーダーのuniform変数を設定
    gl_->Uniform1i(gl_->GetUniformLocation(shader, "numRefPoints"),
                   reference_points_.cols());

    gl_->Uniform1i(gl_->GetUniformLocation(shader, "degreeT"), entity_->Degree());
    gl_->Uniform1i(gl_->GetUniformLocation(shader, "numCtrlPointsT"),
                   entity_->ControlPoints().cols());

    // SSBOをバインド
    gl_->BindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, knots_ssbo_);
    gl_->BindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, control_with_weights_ssbo_);
    gl_->BindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, reference_points_ssbo_);

    gl_->Uniform2f(gl_->GetUniformLocation(shader, "paramRangeT"),
                   entity_->GetParameterRange()[0], entity_->GetParameterRange()[1]);

    gl_->Uniform2f(gl_->GetUniformLocation(shader, "viewportSize"),
                   viewport.first, viewport.second);

    // 描画
    gl_->BindVertexArray(vao_);
    // 1つの頂点からなるパッチを1つ描画する
    gl_->DrawArrays(GL_PATCHES, 0, 1);
    gl_->BindVertexArray(0);
}

void RationalBSplineCurveGraphics::Synchronize() {
    // 既存のリソースを開放
    Cleanup();

    // 参照点の作成
    // TODO: 曲線の種類によって参照点の数を変更する
    int num_ref_points = 4;
    auto [start, end] = entity_->GetParameterRange();
    reference_points_.resize(4, num_ref_points);
    for (int i = 0; i < num_ref_points; ++i) {
        auto t = start + (end - start) * i / (num_ref_points - 1);
        auto point = entity_->TryGetDefinedPointAt(t);
        if (!point) {
            // 曲線上の点が取得できない場合はエラー
            reference_points_.resize(NoChange, 0);
            break;
        }
        reference_points_.block<3, 1>(0, i) = (*point).cast<float>();
    }
    // 参照点が0の場合は描画しない
    if (reference_points_.cols() == 0) return;

    // 参照点のSSBOを作成
    gl_->GenBuffers(1, &reference_points_ssbo_);
    gl_->BindBuffer(GL_SHADER_STORAGE_BUFFER, reference_points_ssbo_);
    gl_->BufferData(GL_SHADER_STORAGE_BUFFER,
                    sizeof(float) * reference_points_.size(),
                    reference_points_.data(), GL_STATIC_DRAW);

    // ノットのSSBOを作成
    gl_->GenBuffers(1, &knots_ssbo_);
    gl_->BindBuffer(GL_SHADER_STORAGE_BUFFER, knots_ssbo_);
    gl_->BufferData(GL_SHADER_STORAGE_BUFFER,
                    sizeof(float) * entity_->Knots().size(),
                    ConvertToFloatVector(entity_->Knots()).data(),
                    GL_STATIC_DRAW);

    // 制御点と重みの転送
    MatrixXf control_with_weights(4, entity_->ControlPoints().cols());
    control_with_weights.block(0, 0, 3, entity_->ControlPoints().cols()) =
            entity_->ControlPoints().cast<float>();
    for (int i = 0; i < entity_->Weights().size(); ++i) {
        control_with_weights(3, i) = static_cast<float>(entity_->Weights()[i]);
    }
    // SSBOを作成してデータを転送
    gl_->GenBuffers(1, &control_with_weights_ssbo_);
    gl_->BindBuffer(GL_SHADER_STORAGE_BUFFER, control_with_weights_ssbo_);
    gl_->BufferData(GL_SHADER_STORAGE_BUFFER,
                    sizeof(float) * control_with_weights.size(),
                    control_with_weights.data(), GL_STATIC_DRAW);
    gl_->BindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

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

void RationalBSplineCurveGraphics::Cleanup() {
    EntityGraphics::Cleanup();

    // SSBOを解放
    if (knots_ssbo_ != 0) {
        gl_->DeleteBuffers(1, &knots_ssbo_);
        knots_ssbo_ = 0;
    }
    if (control_with_weights_ssbo_ != 0) {
        gl_->DeleteBuffers(1, &control_with_weights_ssbo_);
        control_with_weights_ssbo_ = 0;
    }

    if (reference_points_.cols() > 0) {
        reference_points_.resize(NoChange, 0);
    }
    if (reference_points_ssbo_ != 0) {
        gl_->DeleteBuffers(1, &reference_points_ssbo_);
        reference_points_ssbo_ = 0;
    }
}
