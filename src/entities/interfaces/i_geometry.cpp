/**
 * @file entities/interfaces/i_geometry.cpp
 * @brief 曲線・曲面等のクラスのインターフェース定義
 * @author Yayoi Habami
 * @date 2025-11-03
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/interfaces/i_geometry.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using i_ent::IGeometry;

}  // namespace



i_num::BoundingBox IGeometry::GetBoundingBox() const {
    // 定義空間におけるバウンディングボックスを取得
    auto defined_bb = GetDefinedBoundingBox();
    auto p0 = defined_bb.GetControl();
    auto dirs = defined_bb.GetDirections();
    auto sizes = defined_bb.GetSizes();
    auto types = defined_bb.GetDirectionTypes();

    // 基点P0および延伸方向D0~D2を変換
    p0 = Transform(p0, true).value();
    for (size_t i = 0; i < 3; ++i) {
        dirs[i] = Transform(dirs[i], false).value();
    }
    std::array<bool, 3> is_line{};
    for (size_t i = 0; i < 3; ++i) {
        if (types[i] == i_num::BoundingBox::DirectionType::kLine) {
            is_line[i] = true;
        } else {
            is_line[i] = false;
        }
    }

    // 変換後のバウンディングボックスを作成して返す
    return i_num::BoundingBox(p0, dirs, sizes, is_line);
}
