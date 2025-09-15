/**
 * @file graphics/curves/rational_b_spline_curve_graphics.cpp
 * @brief RationalBSplineCurveの描画用クラス
 * @author Yayoi Habami
 * @date 2025-08-15
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/graphics/curves/rational_b_spline_curve_graphics.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

namespace {

using RationalBSplineCurveGraphics = igesio::graphics::RationalBSplineCurveGraphics;

/// @brief MAX_REF_POINTSの値 (kRationalBSplineCurveShader)
constexpr size_t kMaxRefPoints = 10;
/// @brief MAX_DEGREEの値 (kRationalBSplineCurveShader)
constexpr size_t kMaxDegree = 5;
/// @brief MAX_CTRL_POINTSの値 (kRationalBSplineCurveShader)
constexpr size_t kMaxCtrlPoints = 128;

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

namespace {

/// @brief std::vector<double> -> std::vector<float> の変換
std::vector<float> ConvertToFloatVector(
        const std::vector<double>& vec) {
    std::vector<float> result(vec.size());
    std::transform(vec.begin(), vec.end(), result.begin(),
                   [](double val) { return static_cast<float>(val); });
    return result;
}

}  // namespace

void RationalBSplineCurveGraphics::DrawImpl(
        GLuint shader, const std::pair<float, float>& viewport) const {
    // シェーダーのuniform変数を設定
    gl_->Uniform1i(gl_->GetUniformLocation(shader, "numRefPoints"),
                   reference_points_.cols());
    gl_->Uniform3fv(gl_->GetUniformLocation(shader, "refPoints"),
                    reference_points_.cols(), reference_points_.data());

    gl_->Uniform1i(gl_->GetUniformLocation(shader, "degree"), entity_->Degree());
    gl_->Uniform1i(gl_->GetUniformLocation(shader, "numCtrlPoints"),
                   entity_->ControlPoints().cols());
    gl_->Uniform1fv(gl_->GetUniformLocation(shader, "knots"),
                    entity_->Knots().size(),
                    ConvertToFloatVector(entity_->Knots()).data());
    gl_->Uniform1fv(gl_->GetUniformLocation(shader, "weights"),
                    entity_->Weights().size(),
                    ConvertToFloatVector(entity_->Weights()).data());
    gl_->Uniform3fv(gl_->GetUniformLocation(shader, "ctrlPoints"),
                    entity_->ControlPoints().cols(),
                    entity_->ControlPoints().cast<float>().data());
    gl_->Uniform2f(gl_->GetUniformLocation(shader, "paramRange"),
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
    reference_points_.resize(NoChange, num_ref_points);
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
