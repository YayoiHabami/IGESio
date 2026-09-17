/**
 * @file extensions/machines/machine/virtual_machines.cpp
 * @brief 形状を持たない仮想機械の機械定義
 * @author Yayoi Habami
 * @date 2026-09-17
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/machines/machine/virtual_machines.h"

#include <array>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "igesio/numerics/core/matrix.h"

namespace igesio::extensions::machines {

namespace {

/// @brief 工具取り付け点のコンポーネント名
constexpr std::string_view kToolMountName = "Tool";
/// @brief ワーク取り付け点のコンポーネント名
constexpr std::string_view kWorkMountName = "Table";

/// @brief 可動軸のコンポーネントを作る
/// @param name コンポーネント名 (軸名にも用いる)
/// @param parent 親コンポーネント名
/// @param type `kLinear`または`kRotary`
/// @param direction 軸方向 (コンポーネント座標 = ゼロポーズ機械座標)
/// @param limit 可動範囲 ±値 [mm] or [rad] (無ければ無制限)
/// @return 回転軸は原点を通る
ComponentSpec AxisComponent(const std::string_view name,
                            const std::string_view parent,
                            const ComponentType type,
                            const igesio::Vector3d& direction,
                            const std::optional<double> limit) {
    ComponentSpec component;
    component.name = std::string(name);
    component.parent = std::string(parent);
    component.type = type;
    AxisSpec axis;
    axis.register_name = std::string(name);
    axis.direction = direction;
    if (type == ComponentType::kRotary) axis.point = igesio::Vector3d::Zero();
    if (limit.has_value()) {
        axis.limits = std::array<double, 2>{-*limit, *limit};
    } else {
        axis.unlimited = true;
    }
    component.axis = std::move(axis);
    return component;
}

/// @brief 旋回軸C (軸+z、無制限、`wrap_start = 0`) のコンポーネントを作る
/// @param parent 親コンポーネント名
/// @note 傾斜軸は無制限でも`wrap_start`を持たず、NC指令値を正規化しない
///       (傾斜-30°を330°にしない)
ComponentSpec SwivelComponent(const std::string_view parent) {
    ComponentSpec component = AxisComponent("C", parent, ComponentType::kRotary,
                                            igesio::Vector3d::UnitZ(), std::nullopt);
    component.axis->wrap_start = 0.0;
    return component;
}

/// @brief 取り付け点のコンポーネントを作る (取り付けフレームは原点)
/// @param name コンポーネント名
/// @param parent 親コンポーネント名
/// @param type `kToolMount`または`kWorkMount`
ComponentSpec MountComponent(const std::string_view name,
                             const std::string_view parent,
                             const ComponentType type) {
    ComponentSpec component;
    component.name = std::string(name);
    component.parent = std::string(parent);
    component.type = type;
    component.frame_placement = igesio::Matrix4d::Identity();
    return component;
}

/// @brief 工具側の直進3軸 (base→X→Y→Z) を追加する
/// @param[out] components 追加先
void AppendLinearAxes(std::vector<ComponentSpec>& components) {
    components.push_back(AxisComponent("X", kBaseComponentName, ComponentType::kLinear,
                                       igesio::Vector3d::UnitX(), std::nullopt));
    components.push_back(AxisComponent("Y", "X", ComponentType::kLinear,
                                       igesio::Vector3d::UnitY(), std::nullopt));
    components.push_back(AxisComponent("Z", "Y", ComponentType::kLinear,
                                       igesio::Vector3d::UnitZ(), std::nullopt));
}

/// @brief 種類ごとの運動学ツリーを作る
/// @param kind 仮想機械の種類
/// @param tilt_limit 傾斜軸の可動範囲 ±値 [rad] (無ければ無制限)
/// @return コンポーネント (`base`は末尾)
std::vector<ComponentSpec> BuildComponents(const VirtualMachineKind kind,
                                           const std::optional<double> tilt_limit) {
    std::vector<ComponentSpec> components;
    AppendLinearAxes(components);
    switch (kind) {
        case VirtualMachineKind::kThreeAxis:
            components.push_back(
                    MountComponent(kToolMountName, "Z", ComponentType::kToolMount));
            components.push_back(MountComponent(kWorkMountName, kBaseComponentName,
                                                ComponentType::kWorkMount));
            break;
        case VirtualMachineKind::kHeadBc:
            components.push_back(SwivelComponent("Z"));
            components.push_back(AxisComponent("B", "C", ComponentType::kRotary,
                                               igesio::Vector3d::UnitY(), tilt_limit));
            components.push_back(
                    MountComponent(kToolMountName, "B", ComponentType::kToolMount));
            components.push_back(MountComponent(kWorkMountName, kBaseComponentName,
                                                ComponentType::kWorkMount));
            break;
        case VirtualMachineKind::kTableAc:
            components.push_back(
                    MountComponent(kToolMountName, "Z", ComponentType::kToolMount));
            components.push_back(AxisComponent("A", kBaseComponentName,
                                               ComponentType::kRotary,
                                               igesio::Vector3d::UnitX(), tilt_limit));
            components.push_back(SwivelComponent("A"));
            components.push_back(
                    MountComponent(kWorkMountName, "C", ComponentType::kWorkMount));
            break;
    }
    ComponentSpec base;
    base.name = std::string(kBaseComponentName);
    base.type = ComponentType::kBase;
    components.push_back(std::move(base));
    return components;
}

/// @brief 種類の説明文を取得する
/// @param kind 仮想機械の種類
/// @return `MachineDefinition::description`に設定する英語の説明
std::string DescriptionOf(const VirtualMachineKind kind) {
    switch (kind) {
        case VirtualMachineKind::kThreeAxis: return "virtual 3-axis machine (X-Y-Z)";
        case VirtualMachineKind::kHeadBc:
            return "virtual 5-axis machine (head X-Y-Z-C-B)";
        case VirtualMachineKind::kTableAc:
            return "virtual 5-axis machine (X-Y-Z, table A-C)";
    }
    return "virtual machine";
}

}  // namespace



MachineDefinition MakeVirtualMachineDefinition(const VirtualMachineKind kind,
                                               const VirtualMachineOptions& options) {
    if (options.name.empty()) {
        throw std::invalid_argument("MakeVirtualMachineDefinition: name is empty");
    }
    if (options.tilt_limit_rad.has_value() && !(*options.tilt_limit_rad > 0.0)) {
        throw std::invalid_argument(
                "MakeVirtualMachineDefinition: tilt_limit_rad must be positive");
    }
    MachineDefinition definition;
    definition.format_version = kMachineFormatVersion;
    definition.name = options.name;
    definition.description = DescriptionOf(kind);
    definition.components = BuildComponents(kind, options.tilt_limit_rad);
    definition.branch = options.branch;
    definition.source_name = options.name;
    return definition;
}

}  // namespace igesio::extensions::machines
