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
#include <vector>

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
        const std::shared_ptr<IOpenGL>& gl)
        : EntityGraphics(entity, gl, ShaderId::kComposite, true) {
    // 同期 (子曲線の生成+CPU構築+GL転送) はレンダラのreconcile経路が駆動する
    // (ctorでは行わない)。DoSynchronizeが子を生成・同期する。
}

CurveOnSurfaceGraphics::~CurveOnAParametricSurfaceGraphics() {
    Cleanup();
}



void CurveOnSurfaceGraphics::PrewarmCpu() {
    std::shared_ptr<const i_ent::ICurve> curve;
    try {
        // 曲線を取得
        curve = entity_->GetCurve();
        if (curve == nullptr) return;
    } catch (const std::runtime_error&) {
        // 曲線が取得できない場合はスキップ (DoSynchronizeでも再試行される)
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
        // 親のフレーム状態 (変換・色) を新しい子へ伝播する.
        // WalkAssemblyでの適用時点では子が未生成のため、ここで反映する.
        SetWorldTransform(world_transform_);
        if (is_color_overridden_) {
            curve_graphics_->SetColor(
                    {color_[0], color_[1], color_[2], color_[3]});
        } else {
            curve_graphics_->ResetColor();
        }
    }

    // 子のCPU側準備 (契約の伝播; 現状ICurve系はno-op)
    curve_graphics_->PrewarmCpu();
}

void CurveOnSurfaceGraphics::DoSynchronize() {
    // 子の生成/再生成はCPU相で行う (未prewarm時の直列フォールバックを兼ねる).
    // 冪等: 同一曲線では再生成せず、変更時のみ作り直す
    PrewarmCpu();
    if (curve_graphics_ == nullptr) return;

    // 子C(t)のGPUリソースを同期する
    curve_graphics_->Synchronize();
}

void CurveOnSurfaceGraphics::SetWorldTransform(const igesio::Matrix4d& matrix) {
    EntityGraphics::SetWorldTransform(matrix);
    if (curve_graphics_ == nullptr) return;
    // 子C(t)はchild_graphics_でなく専用メンバのため基底のカスケードが届かない.
    // 基底と同一規則で matrix·M_entity を子へ伝播する
    igesio::Matrix4d child_transform = matrix;
    if (entity_ != nullptr) {
        child_transform = matrix
                * entity_->GetTransformationMatrix().GetTransformation();
    }
    curve_graphics_->SetWorldTransform(child_transform);
}

void CurveOnSurfaceGraphics::Cleanup() {
    if (curve_graphics_ == nullptr) return;
    curve_graphics_->Cleanup();
}

bool CurveOnSurfaceGraphics::IsDrawable() const {
    return curve_graphics_ != nullptr && curve_graphics_->IsDrawable();
}

std::unordered_set<i_graph::ShaderId> CurveOnSurfaceGraphics::GetShaderIds() const {
    std::unordered_set<ShaderId> shader_ids = {GetShaderId()};

    if (curve_graphics_ != nullptr) {
        // 曲線のシェーダータイプを追加
        shader_ids.insert(curve_graphics_->GetShaderId());
    }

    return shader_ids;
}

void CurveOnSurfaceGraphics::Draw(
        gl::Uint shader, const ShaderId shader_id,
        const std::pair<float, float>& viewport,
        const DrawContext& ctx) const {
    if (curve_graphics_ == nullptr) return;

    if (shader_id == curve_graphics_->GetShaderId()) {
        // 142自身が選択中なら、委譲先の子(別ID)へハイライトを強制する
        DrawContext child_ctx = ctx;
        if (ctx.IsHighlighted(GetEntityID())) child_ctx.force_highlight = true;
        // 指定されたシェーダータイプに合致する場合、描画を行う
        curve_graphics_->Draw(shader, viewport, child_ctx);
    }
}

bool CurveOnSurfaceGraphics::CanIntersect() const {
    return curve_graphics_ != nullptr && curve_graphics_->CanIntersect();
}

std::vector<i_graph::RayHit> CurveOnSurfaceGraphics::Intersect(
        const Ray& ray, const RayIntersectionParams& params) const {
    // 描画と同一の生成済み3D曲線C(t)へ委譲する
    if (curve_graphics_ == nullptr) return {};
    return curve_graphics_->Intersect(ray, params);
}

void CurveOnSurfaceGraphics::SetColor(const std::array<float, 4>& color) {
    EntityGraphics::SetColor(color);
    // 描画は子要素C(t)に委譲されるため、色も子要素へ伝播させる
    if (curve_graphics_ != nullptr) curve_graphics_->SetColor(color);
}

void CurveOnSurfaceGraphics::ResetColor() {
    EntityGraphics::ResetColor();
    // 子要素C(t)の色もデフォルトに戻す
    if (curve_graphics_ != nullptr) curve_graphics_->ResetColor();
}

