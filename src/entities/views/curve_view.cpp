/**
 * @file entities/views/curve_view.cpp
 * @brief 曲線エンティティの変換ビュー(CurveView)
 * @author Yayoi Habami
 * @date 2026-05-28
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/entities/views/curve_view.h"

#include <cstdint>
#include <stdexcept>
#include <utility>



namespace {

namespace i_num = igesio::numerics;

}  // namespace



namespace igesio::entities {

CurveView::CurveView(std::shared_ptr<const ICurve> base,
                     const Matrix4d& placement)
        : placement_(placement) {
    if (!base) {
        throw std::invalid_argument("CurveView: base curve cannot be null");
    }

    // baseが既にCurveViewの場合は変換を畳み込み、元のICurveへ繋ぎ直す
    if (const auto* inner = dynamic_cast<const CurveView*>(base.get())) {
        base_ = inner->base_;
        placement_ = placement * inner->placement_;
    } else {
        base_ = std::move(base);
    }

    // ビュー固有のIDを採番する(型は元エンティティに揃える)
    id_ = IDGenerator::Generate(ObjectType::kEntityView,
                                static_cast<uint16_t>(base_->GetType()));
}

CurveView::~CurveView() {
    // 固有のint型IDを解放する
    IDGenerator::Release(id_.ToInt());
}

std::optional<Vector3d> CurveView::Transform(
        const std::optional<Vector3d>& input, const bool is_point) const {
    return i_num::ApplyTransform(placement_, input, is_point);
}

}  // namespace igesio::entities
