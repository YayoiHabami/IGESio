/**
 * @file entities/views/surface_view.cpp
 * @brief 曲面エンティティの変換ビュー(SurfaceView)
 * @author Yayoi Habami
 * @date 2026-05-28
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/entities/views/surface_view.h"

#include <cstdint>
#include <stdexcept>
#include <utility>



namespace {

namespace i_num = igesio::numerics;

}  // namespace



namespace igesio::entities {

SurfaceView::SurfaceView(std::shared_ptr<const ISurface> base,
                         const Matrix4d& placement)
        : placement_(placement) {
    if (!base) {
        throw std::invalid_argument("SurfaceView: base surface cannot be null");
    }

    // baseが既にSurfaceViewの場合は変換を畳み込み、元のISurfaceへ繋ぎ直す
    if (const auto* inner = dynamic_cast<const SurfaceView*>(base.get())) {
        base_ = inner->base_;
        placement_ = placement * inner->placement_;
    } else {
        base_ = std::move(base);
    }

    // ビュー固有のIDを採番する(型は元エンティティに揃える)
    id_ = IDGenerator::Generate(ObjectType::kEntityView,
                                static_cast<uint16_t>(base_->GetType()));
}

SurfaceView::~SurfaceView() {
    // 固有のint型IDを解放する
    IDGenerator::Release(id_.ToInt());
}

std::optional<Vector3d> SurfaceView::Transform(
        const std::optional<Vector3d>& input, const bool is_point) const {
    return i_num::ApplyTransform(placement_, input, is_point);
}

}  // namespace igesio::entities
