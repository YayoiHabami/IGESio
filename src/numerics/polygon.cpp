/**
 * @file numerics/polygon.cpp
 * @brief 多角形データ構造の実装
 * @author Yayoi Habami
 * @date 2026-04-10
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/numerics/polygon.h"

#include <stdexcept>
#include <string>
#include <utility>



namespace igesio::numerics {

int PolygonData::Count() const noexcept {
    return static_cast<int>(vertices.size());
}

std::pair<int, int> PolygonData::GetCurveParamIndex(int edge_index) const {
    const int m = Count();
    if (edge_index < 0 || edge_index >= m) {
        throw std::out_of_range(
            "edge_index must be in [0, " + std::to_string(m - 1) +
            "], got " + std::to_string(edge_index));
    }

    // 全頂点が on_curve=false の場合はパラメータ範囲を決定できない
    bool any_on_curve = false;
    for (bool flag : on_curve) {
        if (flag) {
            any_on_curve = true;
            break;
        }
    }
    if (!any_on_curve) {
        throw std::invalid_argument(
            "All vertices have on_curve=false. "
            "Cannot determine curve parameter range.");
    }

    // 始点側: on_curveがtrueになるまで遡る
    int start = edge_index;
    while (!on_curve[start]) start = (start - 1 + m) % m;

    // 終点側: on_curveがtrueになるまで進む
    int end = (edge_index + 1) % m;
    while (!on_curve[end]) end = (end + 1) % m;

    return {start, end};
}

}  // namespace igesio::numerics
