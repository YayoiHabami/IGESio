/**
 * @file extensions/machines/tools/tool_assembly.cpp
 * @brief 工具・ホルダのアセンブリ定義
 * @author Yayoi Habami
 * @date 2026-09-11
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/machines/tools/tool_assembly.h"

#include <optional>
#include <string_view>

#include "igesio/extensions/machines/core/rotation.h"

namespace igesio::extensions::machines {

std::optional<ControlPoint> ParseControlPoint(const std::string_view text) {
    if (text == "tip") return ControlPoint::kTip;
    if (text == "gauge") return ControlPoint::kGauge;
    return std::nullopt;
}

std::string_view ControlPointName(const ControlPoint point) {
    return point == ControlPoint::kGauge ? "gauge" : "tip";
}

igesio::Vector3d ControlLocal(const ToolAssemblySpec& spec,
                              const std::optional<double> g43_length) {
    if (spec.control_point == ControlPoint::kGauge) {
        // ゲージラインは取り付けフレームの原点. G43はそこから先端側へ補正する
        return igesio::Vector3d(0.0, 0.0, g43_length.has_value() ? -*g43_length : 0.0);
    }
    return igesio::Vector3d(
            0.0, 0.0, spec.profile.command_point_z - spec.profile.GaugeLength(nullptr));
}

igesio::Matrix4d ToolMountOffset(const ToolAssemblySpec& spec) {
    return Translation(igesio::Vector3d(0.0, 0.0, -spec.profile.GaugeLength(nullptr)));
}

}  // namespace igesio::extensions::machines
