/**
 * @file graphics/curves/rational_b_spline_surface_graphics.cpp
 * @brief RationalBSplineSurfaceの描画用クラス
 * @author Yayoi Habami
 * @date 2025-09-26
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/graphics/surfaces/rational_b_spline_surface_graphics.h"

#include <memory>
#include <utility>

#include "./../core/type_conversion_utils.h"

namespace {

using igesio::graphics::RationalBSplineSurfaceGraphics;

}  // namespace



RationalBSplineSurfaceGraphics::RationalBSplineSurfaceGraphics(
        const std::shared_ptr<const entities::RationalBSplineSurface> entity,
        const std::shared_ptr<IOpenGL> gl)
        : EntityGraphics(entity, gl, true) {
    Synchronize();
}

RationalBSplineSurfaceGraphics::~RationalBSplineSurfaceGraphics() {
    Cleanup();
}



/**
 * protected
 */

void RationalBSplineSurfaceGraphics::DrawImpl(
        GLuint shader, const std::pair<float, float>& viewport) const {
    // テッセレーションのため、参照点をシェーダーに渡す
    gl_->Uniform1i(gl_->GetUniformLocation(shader, "numRefPointsU"), 4);
    gl_->Uniform1i(gl_->GetUniformLocation(shader, "numRefPointsV"), 4);

    // NURBSのパラメータをシェーダーに渡す
    auto [degree_u, degree_v] = entity_->Degrees();
    gl_->Uniform1i(gl_->GetUniformLocation(shader, "degreeU"), degree_u);
    gl_->Uniform1i(gl_->GetUniformLocation(shader, "degreeV"), degree_v);
    auto [num_ctrl_u, num_ctrl_v] = entity_->NumControlPoints();
    gl_->Uniform1i(gl_->GetUniformLocation(shader, "numCtrlPointsU"), num_ctrl_u);
    gl_->Uniform1i(gl_->GetUniformLocation(shader, "numCtrlPointsV"), num_ctrl_v);

    // SSBOをバインドする
    gl_->BindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, knots_u_ssbo_);
    gl_->BindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, knots_v_ssbo_);
    gl_->BindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, control_with_weights_ssbo_);
    gl_->BindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, reference_points_ssbo_);

    gl_->Uniform2f(gl_->GetUniformLocation(shader, "paramRangeU"),
                   entity_->GetURange()[0], entity_->GetURange()[1]);
    gl_->Uniform2f(gl_->GetUniformLocation(shader, "paramRangeV"),
                   entity_->GetVRange()[0], entity_->GetVRange()[1]);

    gl_->Uniform2f(gl_->GetUniformLocation(shader, "viewportSize"),
                   viewport.first, viewport.second);

    gl_->BindVertexArray(vao_);
    gl_->DrawArrays(GL_PATCHES, 0, 1);
    gl_->BindVertexArray(0);
}

void RationalBSplineSurfaceGraphics::Synchronize() {
    Cleanup();
    SyncTexture();

    // TCSのため、グリッド状の参照点を用意する
    // TODO: 曲面の種類によって参照点の数を変更する
    int num_ref_points_u = 4;
    int num_ref_points_v = 4;
    reference_points_.resize(4, num_ref_points_u * num_ref_points_v);

    auto [u_start, u_end] = entity_->GetURange();
    auto [v_start, v_end] = entity_->GetVRange();

    bool success = true;
    for (int i = 0; i < num_ref_points_u; ++i) {
        float u = u_start + (u_end - u_start) * i / (num_ref_points_u - 1);
        for (int j = 0; j < num_ref_points_v; ++j) {
            float v = v_start + (v_end - v_start) * j / (num_ref_points_v - 1);
            auto point = entity_->TryGetDefinedPointAt(u, v);
            if (!point) {
                success = false;
                break;
            }
            reference_points_.block<3, 1>(0, i * num_ref_points_v + j) = (*point).cast<float>();
        }
        if (!success) break;
    }

    // 参照点の取得/生成に失敗した場合は描画しない
    if (!success) {
        reference_points_.resize(NoChange, 0);
        return;
    } else if (reference_points_.cols() == 0) {
        return;
    }
    // 参照点のSSBOの生成とデータ転送
    gl_->GenBuffers(1, &reference_points_ssbo_);
    gl_->BindBuffer(GL_SHADER_STORAGE_BUFFER, reference_points_ssbo_);
    gl_->BufferData(GL_SHADER_STORAGE_BUFFER,
                    reference_points_.size() * sizeof(float),
                    reference_points_.data(), GL_STATIC_DRAW);

    // ノットベクトルのSSBOの生成とデータ転送
    auto u_knots_float = ConvertToFloatVector(entity_->UKnots());
    gl_->GenBuffers(1, &knots_u_ssbo_);
    gl_->BindBuffer(GL_SHADER_STORAGE_BUFFER, knots_u_ssbo_);
    gl_->BufferData(GL_SHADER_STORAGE_BUFFER, u_knots_float.size() * sizeof(float),
                    u_knots_float.data(), GL_STATIC_DRAW);

    auto v_knots_float = ConvertToFloatVector(entity_->VKnots());
    gl_->GenBuffers(1, &knots_v_ssbo_);
    gl_->BindBuffer(GL_SHADER_STORAGE_BUFFER, knots_v_ssbo_);
    gl_->BufferData(GL_SHADER_STORAGE_BUFFER, v_knots_float.size() * sizeof(float),
                    v_knots_float.data(), GL_STATIC_DRAW);

    // 制御点と重みの転送
    // std430のvec4配列として転送するため (vec3配列も4byteアライメントが必要なため)、
    // 4xNの行列に変換して転送する
    MatrixXf control_with_weights(4, entity_->ControlPoints().cols());
    control_with_weights.block(0, 0, 3, control_with_weights.cols()) =
            entity_->ControlPoints().cast<float>();
    control_with_weights.conservativeResize(4, NoChange);

    // 重みの並びと制御点の並びが異なるため、重みを正しい位置にコピーする
    for (int u = 0; u < entity_->NumControlPoints().first; ++u) {
        for (int v = 0; v < entity_->NumControlPoints().second; ++v) {
            auto ctrl_idx = u * entity_->NumControlPoints().second + v;
            control_with_weights(3, ctrl_idx) =
                    static_cast<float>(entity_->WeightAt(u, v));
        }
    }

    // SSBOを生成してデータを転送
    gl_->GenBuffers(1, &control_with_weights_ssbo_);
    gl_->BindBuffer(GL_SHADER_STORAGE_BUFFER, control_with_weights_ssbo_);
    gl_->BufferData(GL_SHADER_STORAGE_BUFFER,
                    control_with_weights.size() * sizeof(float),
                    control_with_weights.data(), GL_STATIC_DRAW);

    gl_->BindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // テッセレーションシェーダーを使用するため、空のVAOを生成する
    gl_->GenVertexArrays(1, &vao_);

    // パッチの頂点数を1に設定
    // この設定はVAOに保存されるため、描画のたびに設定する必要はない
    gl_->PatchParameteri(GL_PATCH_VERTICES, 1);
}

void RationalBSplineSurfaceGraphics::Cleanup() {
    EntityGraphics::Cleanup();

    // SSBOを解放
    if (knots_u_ssbo_ != 0) {
        gl_->DeleteBuffers(1, &knots_u_ssbo_);
        knots_u_ssbo_ = 0;
    }
    if (knots_v_ssbo_ != 0) {
        gl_->DeleteBuffers(1, &knots_v_ssbo_);
        knots_v_ssbo_ = 0;
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
