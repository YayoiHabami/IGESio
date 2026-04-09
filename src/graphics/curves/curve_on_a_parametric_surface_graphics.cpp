/**
 * @file graphics/curves/curve_on_a_parametric_surface_graphics.cpp
 * @brief CurveOnAParametricSurfaceの描画用クラス
 * @author Yayoi Habami
 * @date 2025-11-09
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/graphics/curves/curve_on_a_parametric_surface_graphics.h"

#include <memory>
#include <unordered_set>
#include <utility>

#include "igesio/entities/curves/copious_data_base.h"
#include "igesio/entities/curves/rational_b_spline_curve.h"
#include "igesio/graphics/curves/i_curve_graphics.h"
#include "igesio/graphics/curves/copious_data_graphics.h"
#include "igesio/graphics/curves/rational_b_spline_curve_graphics.h"

namespace {

namespace i_ent = igesio::entities;
namespace i_graph = igesio::graphics;

using CurveOnSurfaceGraphics = i_graph::CurveOnAParametricSurfaceGraphics;

}  // namespace



CurveOnSurfaceGraphics::CurveOnAParametricSurfaceGraphics(
        const std::shared_ptr<const i_ent::CurveOnAParametricSurface>& entity,
        const std::shared_ptr<IOpenGL> gl)
        : EntityGraphics(entity, gl, true) {
    Synchronize();
}

CurveOnSurfaceGraphics::~CurveOnAParametricSurfaceGraphics() {
    Cleanup();
}



void CurveOnSurfaceGraphics::Synchronize() {
    std::shared_ptr<const i_ent::ICurve> curve;
    try {
        // 曲線を取得
        curve = entity_->GetCurve();
        if (curve == nullptr) return;
    } catch (const std::runtime_error&) {
        // 曲線が取得できない場合、同期処理を中止
        return;
    }

    if (curve_graphics_ == nullptr ||
        curve_graphics_->GetEntityID() != curve->GetID()) {
        // 描画用オブジェクトが未設定、またはエンティティが変更された場合、新規作成
        // 折れ線とNURBS曲線のみ特殊化、そうでなければ一般曲線用の描画オブジェクトを作成
        if (auto copious = std::dynamic_pointer_cast<
                           const i_ent::CopiousDataBase>(curve)) {
            curve_graphics_ = std::make_unique<CopiousDataGraphics>(copious, gl_);
        } else if (auto nurbs = std::dynamic_pointer_cast<
                                const i_ent::RationalBSplineCurve>(curve)) {
            curve_graphics_ = std::make_unique<RationalBSplineCurveGraphics>(nurbs, gl_);
        } else {
            curve_graphics_ = std::make_unique<ICurveGraphics>(curve, gl_);
        }
    }

    // 曲線の描画用データを同期
    curve_graphics_->Synchronize();
}

void CurveOnSurfaceGraphics::Cleanup() {
    if (curve_graphics_ == nullptr) return;
    curve_graphics_->Cleanup();
}

bool CurveOnSurfaceGraphics::IsDrawable() const {
    return curve_graphics_ != nullptr && curve_graphics_->IsDrawable();
}

std::unordered_set<i_graph::ShaderType> CurveOnSurfaceGraphics::GetShaderTypes() const {
    std::unordered_set<ShaderType> shader_types = {GetShaderType()};

    if (curve_graphics_ != nullptr) {
        // 曲線のシェーダータイプを追加
        shader_types.insert(curve_graphics_->GetShaderType());
    }

    return shader_types;
}

void CurveOnSurfaceGraphics::Draw(
        GLuint shader, const ShaderType shader_type,
        const std::pair<float, float>& viewport) const {
    if (curve_graphics_ == nullptr) return;

    if (shader_type == curve_graphics_->GetShaderType()) {
        // 指定されたシェーダータイプに合致する場合、描画を行う
        curve_graphics_->Draw(shader, viewport);
    }
}

