/**
 * @file graphics/curves/composite_curve_graphics.cpp
 * @brief CompositeCurveの描画用クラス
 * @author Yayoi Habami
 * @date 2025-08-17
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/graphics/curves/composite_curve_graphics.h"

#include <memory>

namespace {

using igesio::graphics::CompositeCurveGraphics;

}  // namespace




CompositeCurveGraphics::CompositeCurveGraphics(
        const std::shared_ptr<const entities::CompositeCurve> entity,
        const std::shared_ptr<IOpenGL> gl)
        : CompositeEntityGraphics(entity, gl, true) {}

void CompositeCurveGraphics::Synchronize() {
    for (auto& [shader, graphics] : child_graphics_) {
        for (auto& graphic : graphics) {
            graphic->Synchronize();
        }
    }
}
