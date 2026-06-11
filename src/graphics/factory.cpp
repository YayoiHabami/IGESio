/**
 * @file graphics/factory.cpp
 * @brief エンティティクラスから描画オブジェクトを作成する
 * @author Yayoi Habami
 * @date 2025-08-08
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/graphics/factory.h"

#include <memory>
#include <typeindex>
#include <utility>

#include "igesio/graphics/graphics_registry.h"

#include "igesio/graphics/curves/i_curve_graphics.h"
#include "igesio/graphics/curves/circular_arc_graphics.h"
#include "igesio/graphics/curves/composite_curve_graphics.h"
#include "igesio/graphics/curves/conic_arc_graphics.h"
#include "igesio/graphics/curves/copious_data_graphics.h"
#include "igesio/graphics/curves/line_graphics.h"
#include "igesio/graphics/curves/point_graphics.h"
#include "igesio/graphics/curves/rational_b_spline_curve_graphics.h"
#include "igesio/graphics/curves/curve_on_a_parametric_surface_graphics.h"

#include "igesio/graphics/surfaces/i_surface_graphics.h"
#include "igesio/graphics/surfaces/rational_b_spline_surface_graphics.h"
#include "igesio/graphics/surfaces/restricted_surface_graphics.h"

namespace {

namespace i_ent = igesio::entities;
namespace i_graph = igesio::graphics;

/// @brief CompositeCurveの描画オブジェクトを作成する
std::unique_ptr<i_graph::IEntityGraphics> CreateCompositeCurveGraphics(
        const std::shared_ptr<const i_ent::CompositeCurve>& entity,
        const std::shared_ptr<i_graph::IOpenGL>& gl) {
    // entityがnullptrの場合は無効なポインタを返す
    if (!entity) return nullptr;

    // CompositeCurveの描画オブジェクトを作成
    auto graphics = std::make_unique<i_graph::CompositeCurveGraphics>(entity, gl);

    // 子要素の描画オブジェクトを追加
    auto ids = entity->GetChildIDs();
    for (const auto& id : ids) {
        auto child_entity = entity->GetChildEntity(id);
        if (!child_entity) continue;

        // 曲線の子要素を追加
        auto curve_ptr = std::dynamic_pointer_cast<const i_ent::ICurve>(child_entity);
        if (curve_ptr) {
            // 子要素の描画オブジェクトを作成
            auto child_graphics = i_graph::CreateCurveGraphics(curve_ptr, gl);
            if (child_graphics) {
                graphics->AddChildGraphics(std::move(child_graphics));
            }
        }
    }

    return graphics;
}

}  // namespace


std::unique_ptr<i_graph::IEntityGraphics> i_graph::CreateCurveGraphics(
        const std::shared_ptr<const i_ent::ICurve>& entity,
        const std::shared_ptr<i_graph::IOpenGL>& gl) {
    // entityがnullptrの場合は無効なポインタを返す
    if (!entity) return nullptr;

    if (auto cir = std::dynamic_pointer_cast<const i_ent::CircularArc>(entity)) {
        // Type 100: CircularArcの描画オブジェクトを作成
        return std::make_unique<i_graph::CircularArcGraphics>(cir, gl);
    } else if (auto comp_curve = std::dynamic_pointer_cast<const i_ent::CompositeCurve>(entity)) {
        // Type 102: CompositeCurveの描画オブジェクトを作成
        return CreateCompositeCurveGraphics(comp_curve, gl);
    } else if (auto conic_arc = std::dynamic_pointer_cast<const i_ent::ConicArc>(entity)) {
        // Type 104: 楕円の描画オブジェクトを作成
        if (conic_arc->GetConicType() == i_ent::ConicType::kEllipse) {
            return std::make_unique<i_graph::EllipseGraphics>(conic_arc, gl);
        }
        // それ以外の円錐曲線は非対応 (ICurveGraphicsを返す)
    } else if (auto copious_data = std::dynamic_pointer_cast<
               const i_ent::CopiousDataBase>(entity)) {
        // Type 106: CopiousDataの描画オブジェクトを作成
        return std::make_unique<i_graph::CopiousDataGraphics>(copious_data, gl);
    } else if (auto line = std::dynamic_pointer_cast<const i_ent::Line>(entity)) {
        // Type 110: Lineの描画オブジェクトを作成
        if (line->GetLineType() == i_ent::LineType::kSegment ||
            line->GetLineType() == i_ent::LineType::kRay) {
            return std::make_unique<i_graph::SegmentGraphics>(line, gl);
        }
        return std::make_unique<i_graph::LineGraphics>(line, gl);
    } else if (auto r_bspline = std::dynamic_pointer_cast<
               const i_ent::RationalBSplineCurve>(entity)) {
        // Type 126: RationalBSplineCurveの描画オブジェクトを作成
        return std::make_unique<i_graph::RationalBSplineCurveGraphics>(r_bspline, gl);
    } else if (auto curve_on_surf = std::dynamic_pointer_cast<
               const i_ent::CurveOnAParametricSurface>(entity)) {
        // Type 142: CurveOnAParametricSurfaceの描画オブジェクトを作成
        return std::make_unique<i_graph::CurveOnAParametricSurfaceGraphics>(curve_on_surf, gl);
    }

    // いずれにも当てはまらない場合はICurveGraphicsを作成
    return std::make_unique<i_graph::ICurveGraphics>(entity, gl);
}

std::unique_ptr<i_graph::IEntityGraphics> i_graph::CreateSurfaceGraphics(
        const std::shared_ptr<const i_ent::ISurface>& entity,
        const std::shared_ptr<IOpenGL>& gl) {
    // entityがnullptrの場合は無効なポインタを返す
    if (!entity) return nullptr;

    if (auto r_bspline_surface = std::dynamic_pointer_cast<
                const i_ent::RationalBSplineSurface>(entity)) {
        // Type 128: RationalBSplineSurfaceの描画オブジェクトを作成
        return std::make_unique<i_graph::RationalBSplineSurfaceGraphics>(r_bspline_surface, gl);
    } else if (auto restricted = std::dynamic_pointer_cast<
                const i_ent::IRestrictedSurface>(entity)) {
        // Type 143/144/108(有界): 制限付き曲面の描画オブジェクトを作成
        return std::make_unique<i_graph::RestrictedSurfaceGraphics>(restricted, gl);
    }

    // いずれにも当てはまらない場合は無効なポインタを返す
    return std::make_unique<i_graph::ISurfaceGraphics>(entity, gl);
}

std::unique_ptr<i_graph::IEntityGraphics> i_graph::CreateEntityGraphics(
        const std::shared_ptr<const i_ent::IEntityIdentifier>& entity,
        const std::shared_ptr<i_graph::IOpenGL>& gl,
        const bool synchronize) {
    // entityがnullptrの場合は無効なポインタを返す
    if (!entity) return nullptr;
    // UnsupportedEntityの場合は無効なポインタを返す
    if (!entity->IsSupported()) return nullptr;

    // レジストリ登録 (GraphicsRegistry) を動的型の完全一致で最優先する.
    // 登録がある型はその結果を最終とする (nullptrの返却は「型起因の恒久的失敗」
    // として負キャッシュされる. 下の不変条件と同じ扱い)
    // (typeidのオペランドに関数呼び出し (operator*) を直接置くと
    //  -Wpotentially-evaluated-expression となるため、参照へ束縛してから渡す)
    const i_ent::IEntityIdentifier& entity_ref = *entity;
    if (auto creator = i_graph::GraphicsRegistry::FindCreator(
            std::type_index(typeid(entity_ref)))) {
        auto registered = creator(entity, gl);
        if (registered && synchronize) registered->Synchronize();
        return registered;
    }

    std::unique_ptr<i_graph::IEntityGraphics> graphics;
    if (auto curve = std::dynamic_pointer_cast<const i_ent::ICurve>(entity)) {
        // 曲線の描画オブジェクトを作成
        graphics = CreateCurveGraphics(curve, gl);
    } else if (auto surface = std::dynamic_pointer_cast<const i_ent::ISurface>(entity)) {
        // 曲面の描画オブジェクトを作成
        graphics = CreateSurfaceGraphics(surface, gl);
    } else if (auto point = std::dynamic_pointer_cast<const i_ent::Point>(entity)) {
        // Type 116: Pointの描画オブジェクトを作成
        // NOTE: curvesの下に配置してはいるが、ICurveを継承していないため、ここで処理する
        graphics = std::make_unique<i_graph::PointGraphics>(point, gl);
    } else {
        // いずれにも当てはまらない場合は無効なポインタを返す
        return nullptr;
    }

    // 既定では生成時に同期 (CPU構築+GL転送) まで行い即描画可能な状態で返す.
    // 複合系は自身のSynchronizeが子孫へ伝播する。レンダラはsynchronize=falseで
    // 遅延し、reconcile経路で並列前倒し+直列GL転送を行う。
    if (graphics && synchronize) graphics->Synchronize();
    return graphics;
}
