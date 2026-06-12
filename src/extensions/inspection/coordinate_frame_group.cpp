/**
 * @file extensions/inspection/coordinate_frame_group.cpp
 * @brief 座標系群を保持する非IGESエンティティ (CAM表示用)
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/inspection/coordinate_frame_group.h"

#include "igesio/numerics/core/tolerance.h"

namespace igesio::extensions::inspection {

void CoordinateFrameGroup::SetAxisSize(const double s) {
    if (!numerics::IsApproxEqual(AxisSize(), s)) {
        style_.axis_size = s;
        MarkGeometryModified();
    }
}

void CoordinateFrameGroup::SetStyle(const CoordinateFrameStyle& style) {
    SetPointColor(style.point_color);
    SetXColor(style.x_color);
    SetYColor(style.y_color);
    SetZColor(style.z_color);
    SetPointSize(style.point_size);
    SetAxisWidth(style.axis_width);
    SetAxisSize(style.axis_size);
}

numerics::BoundingBox CoordinateFrameGroup::GetDefinedBoundingBox() const {
    if (frames_.empty()) return numerics::BoundingBox();

    // 全座標系の原点と各軸の先端 (原点 + 軸·axis_size) を包含する軸平行AABBを構成する.
    // 描画される範囲と一致させるため、軸の先端まで含める.
    Vector3d lo = frames_.front().origin;
    Vector3d hi = frames_.front().origin;
    const auto extend = [&lo, &hi](const Vector3d& p) {
        lo = lo.cwiseMin(p);
        hi = hi.cwiseMax(p);
    };
    for (const auto& frame : frames_) {
        extend(frame.origin);
        extend(frame.origin + frame.x_axis * AxisSize());
        extend(frame.origin + frame.y_axis * AxisSize());
        extend(frame.origin + frame.z_axis * AxisSize());
    }
    return numerics::BoundingBox(lo, hi);
}

std::optional<Vector3d> CoordinateFrameGroup::Transform(
        const std::optional<Vector3d>& input,
        [[maybe_unused]] const bool is_point) const {
    // 変換行列を持たないため恒等 (配置はAssemblyの大域変換で行う)
    return input;
}



/**
 * ファクトリ関数
 */

std::shared_ptr<CoordinateFrameGroup> MakeCoordinateFrameGroup(
        std::vector<CoordinateFrame> frames,
        const CoordinateFrameStyle& style) {
    auto group = std::make_shared<CoordinateFrameGroup>(std::move(frames));
    group->SetStyle(style);
    return group;
}

}  // namespace igesio::extensions::inspection
