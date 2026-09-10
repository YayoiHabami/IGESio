/**
 * @file extensions/machines/machine/machine_model.cpp
 * @brief 機械定義から構築する運動学モデル (木構造・チェーン・軸一覧・派生量)
 * @author Yayoi Habami
 * @date 2026-09-09
 * @copyright 2026 Yayoi Habami
 * @note 構築時の検証対象は、モデルの構築に必要な構造に関するものに限定する.
 *       可動範囲、rank、右手系などの幾何的な検証は機械定義の読込側で行うこと.
 * @note 親先行順：`MachineModel::Component(i)`が提供する順序であり,
 *       baseコンポーネントを先頭として、深さ優先で親要素が子よりも前に来るように並ぶもの.
 */
#include "igesio/extensions/machines/machine/machine_model.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <tuple>
#include <utility>

#include "igesio/extensions/machines/core/rotation.h"
#include "igesio/extensions/machines/core/tolerances.h"
#include "igesio/extensions/machines/core/units.h"

namespace igesio::extensions::machines {

namespace {

/// @brief コンポーネント名をキー、`MachineDefinition::components`
///        のインデックスを値とするマップ
using IndexMap = std::unordered_map<std::string, std::size_t>;

/// @brief 機械構造の問題点を`std::invalid_argument`として投げる
/// @param message 内容 (先頭に`"MachineModel: "`を付ける)
/// @throw std::invalid_argument 常に投げる
[[noreturn]] void Fail(const std::string& message) {
    throw std::invalid_argument("MachineModel: " + message);
}

/// @brief 角度を[start, start + 2π)の範囲に収める
/// @param angle 対象の角度 [rad]
/// @param start 区間の下端 [rad]
/// @return [start, start + 2π)に入る角度 [rad]
double NormalizeToTurn(const double angle, const double start) {
    const double shifted = std::fmod(angle - start, kFullTurn);
    return start + (shifted < 0.0 ? shifted + kFullTurn : shifted);
}

/// @brief 名前列を`[a, b]`の形にする
/// @param names 列挙する名前
/// @return 角括弧で囲み、`", "`で連結した文字列
std::string JoinNames(const std::vector<std::string>& names) {
    std::string text = "[";
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i > 0) text += ", ";
        text += names[i];
    }
    return text + "]";
}

/// @brief コンポーネント名とインデックスの表を作り、名前の空・重複を検出する
/// @param definition 機械定義
/// @return コンポーネント名とインデックスの対応関係
/// @throw std::invalid_argument 名前が空、または重複する場合
IndexMap IndexByName(const MachineDefinition& definition) {
    IndexMap by_name;
    for (std::size_t i = 0; i < definition.components.size(); ++i) {
        const std::string& name = definition.components[i].name;
        if (name.empty()) {
            Fail("component name is empty (index " + std::to_string(i) + ")");
        }
        if (!by_name.emplace(name, i).second) {
            Fail("duplicate component name: " + name);
        }
    }
    return by_name;
}

/// @brief ルート (`base`) のインデックスを求め、baseの制約を検証する
/// @param definition 機械定義
/// @return definitionにおけるbaseコンポーネントのインデックス
/// @throw std::invalid_argument baseがちょうど1つでない、名前が予約名でない、
///        親を持つ、またはルート以外に`type = base`がある場合
std::size_t FindBase(const MachineDefinition& definition) {
    std::vector<std::size_t> bases;
    for (std::size_t i = 0; i < definition.components.size(); ++i) {
        if (definition.components[i].type == ComponentType::kBase) bases.push_back(i);
    }
    if (bases.size() != 1) {
        Fail("exactly one base is required (found " + std::to_string(bases.size())
             + ")");
    }
    const ComponentSpec& base = definition.components[bases[0]];
    if (base.name != kBaseComponentName) {
        Fail("type=base is reserved for the root \"" + std::string(kBaseComponentName)
             + "\": " + base.name);
    }
    if (!base.parent.empty()) Fail("base must not have a parent");
    return bases[0];
}

/// @brief baseを起点とする、親先行順の定義インデックス列を作る
/// @param definition 機械定義
/// @param by_name コンポーネント名とインデックスの対応関係
/// @param base definitionにおけるbaseコンポーネントのインデックス
/// @return `MachineDefinition::components`を作る,
///         definitionにおけるコンポーネントのインデックス列
/// @throw std::invalid_argument 親が存在しない,
///        またはbaseから到達できないコンポーネント (閉路など) がある場合
std::vector<std::size_t> ParentFirstOrder(
        const MachineDefinition& definition,
        const IndexMap& by_name, const std::size_t base) {
    std::vector<std::vector<std::size_t>> children(definition.components.size());
    for (std::size_t i = 0; i < definition.components.size(); ++i) {
        if (i == base) continue;
        const ComponentSpec& spec = definition.components[i];
        const auto it = by_name.find(spec.parent);
        if (it == by_name.end()) {
            Fail(spec.name + ": parent does not exist: " + spec.parent);
        }
        children[it->second].push_back(i);
    }
    std::vector<std::size_t> order;
    std::vector<std::size_t> stack = {base};
    while (!stack.empty()) {
        const std::size_t index = stack.back();
        stack.pop_back();
        order.push_back(index);
        for (auto child = children[index].rbegin(); child != children[index].rend();
             ++child) {
            stack.push_back(*child);
        }
    }
    if (order.size() != definition.components.size()) {
        std::vector<std::string> rest;
        for (std::size_t i = 0; i < definition.components.size(); ++i) {
            if (std::find(order.begin(), order.end(), i) == order.end()) {
                rest.push_back(definition.components[i].name);
            }
        }
        std::sort(rest.begin(), rest.end());
        Fail("component not in the tree (cycle or similar): " + JoinNames(rest));
    }
    return order;
}

/// @brief コンポーネントのtypeとサブ要素 (`axis`/`point`/`frame_placement`)
///        の整合を検証する
/// @param spec 検証するコンポーネントの定義
/// @throw std::invalid_argument typeと`axis`/`frame_placement`の有無が不整合,
///        rotaryの`point`が欠落、registerが空、または`direction`がゼロの場合
void CheckComponentShape(const ComponentSpec& spec) {
    const bool is_axis = spec.type == ComponentType::kLinear
                         || spec.type == ComponentType::kRotary;
    const bool is_mount = spec.type == ComponentType::kToolMount
                          || spec.type == ComponentType::kWorkMount;
    const std::string type_name(ComponentTypeName(spec.type));
    if (is_axis != spec.axis.has_value()) {
        Fail(spec.name + ": type=" + type_name
             + " is inconsistent with the presence of axis");
    }
    if (is_mount != spec.frame_placement.has_value()) {
        Fail(spec.name + ": type=" + type_name
             + " is inconsistent with the presence of frame_placement");
    }
    if (!is_axis) return;
    if (spec.type == ComponentType::kRotary && !spec.axis->point.has_value()) {
        Fail(spec.name + ": rotary axis has no point");
    }
    if (spec.axis->register_name.empty()) Fail(spec.name + ": register is empty");
    if (spec.axis->direction.norm() < kDegenerateTolerance) {
        Fail(spec.name + ": direction is a zero vector");
    }
}

/// @brief 親先行順の`ComponentInfo`列を作る (軸のインデックスは後で埋める)
/// @param definition 機械定義
/// @param by_name コンポーネント名とインデックスの対応関係
/// @param order 親先行順に並べた、definitionにおけるコンポーネントのインデックス列
/// @return 親先行順に並べたコンポーネント
std::vector<ComponentInfo> BuildComponents(
        const MachineDefinition& definition,
        const IndexMap& by_name, const std::vector<std::size_t>& order) {
    std::vector<std::size_t> position(definition.components.size());
    for (std::size_t i = 0; i < order.size(); ++i) position[order[i]] = i;
    std::vector<ComponentInfo> components;
    components.reserve(order.size());
    for (const std::size_t definition_index : order) {
        const ComponentSpec& spec = definition.components[definition_index];
        ComponentInfo info;
        info.name = spec.name;
        info.type = spec.type;
        info.definition_index = definition_index;
        info.local_frame = spec.local_frame;
        info.mount_placement = spec.frame_placement;
        if (spec.type != ComponentType::kBase) {
            info.parent = position[by_name.at(spec.parent)];
            components[*info.parent].children.push_back(components.size());
        }
        components.push_back(std::move(info));
    }
    return components;
}

/// @brief `tool_mount`/`work_mount`のインデックスを求める (それぞれちょうど1つ)
/// @param components 親先行順のコンポーネント
/// @return `tool_mount`と`work_mount`のインデックスの組
/// @throw std::invalid_argument いずれかがちょうど1つでない場合
std::pair<std::size_t, std::size_t> FindMounts(
        const std::vector<ComponentInfo>& components) {
    std::vector<std::size_t> tool_mounts, work_mounts;
    for (std::size_t i = 0; i < components.size(); ++i) {
        if (components[i].type == ComponentType::kToolMount) tool_mounts.push_back(i);
        if (components[i].type == ComponentType::kWorkMount) work_mounts.push_back(i);
    }
    if (tool_mounts.size() != 1) {
        Fail("exactly one tool_mount is required (found "
             + std::to_string(tool_mounts.size()) + ")");
    }
    if (work_mounts.size() != 1) {
        Fail("exactly one work_mount is required (found "
             + std::to_string(work_mounts.size()) + ")");
    }
    return {tool_mounts[0], work_mounts[0]};
}

/// @brief baseからmountまでのインデックス列 (両端を含む)
/// @param components 親先行順のコンポーネント
/// @param mount 末端とするコンポーネントのインデックス
/// @return baseから`mount`までのインデックス列
std::vector<std::size_t> BuildChain(const std::vector<ComponentInfo>& components,
                                    const std::size_t mount) {
    std::vector<std::size_t> chain;
    std::optional<std::size_t> current = mount;
    while (current.has_value()) {
        chain.push_back(*current);
        current = components[*current].parent;
    }
    std::reverse(chain.begin(), chain.end());
    return chain;
}

/// @brief NC値の正規化区間を求める (`limits` or `[wrap_start, wrap_start + 2π)`)
/// @param axis 対象の軸の定義
/// @return 正規化区間. `limits`も`wrap_start`も無ければ`std::nullopt`
std::optional<std::array<double, 2>> NcRangeOf(const AxisSpec& axis) {
    if (axis.limits.has_value()) return axis.limits;
    if (axis.wrap_start.has_value()) {
        return std::array<double, 2>{*axis.wrap_start, *axis.wrap_start + kFullTurn};
    }
    return std::nullopt;
}

/// @brief 軸一覧を作り、各コンポーネントの軸インデックスと
///        どちらのチェーンに属しているか、およびσを設定する
/// @param definition 機械定義
/// @param[out] components 親先行順のコンポーネント (軸のインデックスを書き込む)
/// @param tool_chain 工具側チェーンのコンポーネントインデックス
/// @param work_chain ワーク側チェーンのコンポーネントインデックス
/// @return ゼロポーズ機械座標での軸一覧 (出現順)
/// @throw std::invalid_argument registerが重複する場合
std::vector<AxisInfo> BuildAxes(
        const MachineDefinition& definition,
        std::vector<ComponentInfo>& components,
        const std::vector<std::size_t>& tool_chain,
        const std::vector<std::size_t>& work_chain) {
    std::vector<bool> on_tool(components.size(), false), on_work(components.size(), false);
    for (const std::size_t index : tool_chain) on_tool[index] = true;
    for (const std::size_t index : work_chain) on_work[index] = true;
    std::vector<AxisInfo> axes;
    std::unordered_map<std::string, std::size_t> seen;
    for (std::size_t i = 0; i < components.size(); ++i) {
        const ComponentSpec& spec = definition.components[components[i].definition_index];
        if (!spec.axis.has_value()) continue;
        const AxisSpec& source = *spec.axis;
        if (!seen.emplace(source.register_name, i).second) {
            Fail("duplicate register: " + source.register_name);
        }
        AxisInfo axis;
        axis.register_name = source.register_name;
        axis.component_index = i;
        axis.kind = spec.type == ComponentType::kRotary ? AxisKind::kRotary
                                                        : AxisKind::kLinear;
        axis.direction_world =
                (RotationPart(spec.local_frame) * source.direction).normalized();
        if (source.point.has_value()) {
            axis.point_world = ApplyPoint(spec.local_frame, *source.point);
        }
        axis.on_tool_chain = on_tool[i];
        axis.on_work_chain = on_work[i];
        axis.sigma = on_work[i] ? -1.0 : 1.0;   // 仕様に基づき、共通軸も-1
        axis.limits = source.limits;
        axis.unlimited = source.unlimited;
        axis.wrap_start = source.wrap_start;
        axis.initial = source.initial;
        axis.dynamics = source.dynamics;
        axis.nc_range = NcRangeOf(source);
        components[i].axis = axes.size();
        axes.push_back(std::move(axis));
    }
    return axes;
}

/// @brief チェーン上のIK対象の回転軸のインデックスを、チェーンの並び順で集める
/// @param components 親先行順のコンポーネント
/// @param axes 軸一覧
/// @param chain 対象のチェーンのコンポーネントインデックス
/// @return `axes`におけるインデックス列
std::vector<std::size_t> IkRotaries(
        const std::vector<ComponentInfo>& components,
        const std::vector<AxisInfo>& axes, const std::vector<std::size_t>& chain) {
    std::vector<std::size_t> result;
    for (const std::size_t index : chain) {
        const std::optional<std::size_t> axis = components[index].axis;
        if (!axis.has_value()) continue;
        if (axes[*axis].kind == AxisKind::kRotary && axes[*axis].IsIkTarget()) {
            result.push_back(*axis);
        }
    }
    return result;
}

/// @brief 工具の向きを決める回転軸をまとめる
/// @param components 親先行順のコンポーネント
/// @param axes 軸一覧
/// @param tool_chain 工具側チェーンのコンポーネントインデックス
/// @param work_chain ワーク側チェーンのコンポーネントインデックス
/// @return `axes`におけるインデックス列
///         (先頭が外側. ワーク側を末端から並べ、続けて工具側を根元から並べる)
std::vector<std::size_t> BuildOrientationAxes(
        const std::vector<ComponentInfo>& components, const std::vector<AxisInfo>& axes,
        const std::vector<std::size_t>& tool_chain,
        const std::vector<std::size_t>& work_chain) {
    std::vector<std::size_t> indices = IkRotaries(components, axes, work_chain);
    std::reverse(indices.begin(), indices.end());
    const std::vector<std::size_t> tool = IkRotaries(components, axes, tool_chain);
    indices.insert(indices.end(), tool.begin(), tool.end());
    return indices;
}

/// @brief チェーン上の可動軸の軸名を並べる
/// @param components 親先行順のコンポーネント
/// @param axes 軸一覧
/// @param tool_chain 工具側チェーンのコンポーネントインデックス
/// @param work_chain ワーク側チェーンのコンポーネントインデックス
/// @return 軸名の列 (工具側を根元から並べ、続いてワーク側を末端から並べる)
/// @note 両チェーンに共通する軸が重複しないよう、既出の名前は飛ばす
std::vector<std::string> BuildChainRegisters(
        const std::vector<ComponentInfo>& components,
        const std::vector<AxisInfo>& axes,
        const std::vector<std::size_t>& tool_chain,
        const std::vector<std::size_t>& work_chain) {
    std::vector<std::string> registers;
    for (const std::vector<std::size_t>* chain : {&tool_chain, &work_chain}) {
        for (const std::size_t index : *chain) {
            const std::optional<std::size_t> axis = components[index].axis;
            if (!axis.has_value()) continue;
            const std::string& name = axes[*axis].register_name;
            if (std::find(registers.begin(), registers.end(), name) == registers.end()) {
                registers.push_back(name);
            }
        }
    }
    return registers;
}

/// @brief ゼロポーズでの工具軸方向 (`tool_mount`フレームのz軸. 機械座標)
/// @param components 親先行順のコンポーネント
/// @param tool_mount `tool_mount`コンポーネントのインデックス
/// @return 単位ベクトル
igesio::Vector3d ToolAxisOf(const std::vector<ComponentInfo>& components,
                            const std::size_t tool_mount) {
    return RotationPart(*components[tool_mount].mount_placement).col(2);
}

/// @brief ワーク側回転軸の回転中心の参照点を計算する (ゼロポーズ機械座標)
/// @param components 親先行順のコンポーネント
/// @param axes 軸一覧
/// @param work_chain ワーク側チェーンのコンポーネントインデックス
/// @return 回転軸が1本ならその軸上の点. 2本以上なら末端側の軸上でもう一方の
///         軸線に最も近い点 (両軸が交わる理想機では交点).
///         IK対象の回転軸が無ければ`std::nullopt`
std::optional<igesio::Vector3d> WorkPivotOf(
        const std::vector<ComponentInfo>& components,
        const std::vector<AxisInfo>& axes,
        const std::vector<std::size_t>& work_chain) {
    const std::vector<std::size_t> rotaries = IkRotaries(components, axes, work_chain);
    if (rotaries.empty()) return std::nullopt;
    if (rotaries.size() == 1) return axes[rotaries[0]].point_world;

    // 末端側2本について、末端側の軸上でもう一方の軸線に最も近い点
    const AxisInfo& root_side = axes[rotaries[rotaries.size() - 2]];
    const AxisInfo& tip_side = axes[rotaries.back()];
    const igesio::Vector3d d1 = root_side.direction_world;
    const igesio::Vector3d d2 = tip_side.direction_world;
    const double b = d1.dot(d2);
    const igesio::Vector3d w = tip_side.point_world - root_side.point_world;
    const double denominator = 1.0 - b * b;
    const double t2 = std::abs(denominator) < kZeroTolerance
            ? 0.0
            : (b * w.dot(d1) - w.dot(d2)) / denominator;
    return igesio::Vector3d(tip_side.point_world + t2 * d2);
}

}  // namespace



bool IsWithinLimits(const AxisInfo& axis, const double nc) {
    if (!axis.limits.has_value()) return true;
    return (*axis.limits)[0] - kLimitTolerance <= nc
           && nc <= (*axis.limits)[1] + kLimitTolerance;
}

std::optional<double> WrapAngleIntoLimits(const double nc_rad,
                                          const AxisInfo& axis) {
    if (axis.kind != AxisKind::kRotary) {
        throw std::invalid_argument(
                "WrapAngleIntoLimits: axis '" + axis.register_name
                + "' is not a rotary axis");
    }
    if (axis.unlimited) {
        // 可動範囲に制限がない場合は、`wrap_start`がある場合のみ正規化する
        if (!axis.nc_range.has_value()) return nc_rad;
        return NormalizeToTurn(nc_rad, (*axis.nc_range)[0]);
    }
    if (!axis.limits.has_value()) return nc_rad;
    for (const double shift : {0.0, kFullTurn, -kFullTurn}) {
        const double candidate = nc_rad + shift;
        if (IsWithinLimits(axis, candidate)) return candidate;
    }
    return std::nullopt;
}

MachineModel::MachineModel(MachineDefinition definition)
    : definition_(std::move(definition)) {
    const IndexMap by_name = IndexByName(definition_);
    const std::size_t base = FindBase(definition_);
    const std::vector<std::size_t> order = ParentFirstOrder(definition_, by_name, base);
    for (const ComponentSpec& spec : definition_.components) CheckComponentShape(spec);
    components_ = BuildComponents(definition_, by_name, order);
    for (std::size_t i = 0; i < components_.size(); ++i) {
        by_name_[components_[i].name] = i;
    }
    std::tie(tool_mount_, work_mount_) = FindMounts(components_);
    tool_chain_ = BuildChain(components_, tool_mount_);
    work_chain_ = BuildChain(components_, work_mount_);
    axes_ = BuildAxes(definition_, components_, tool_chain_, work_chain_);
    for (std::size_t i = 0; i < axes_.size(); ++i) {
        by_register_[axes_[i].register_name] = i;
    }
    orientation_axes_ =
            BuildOrientationAxes(components_, axes_, tool_chain_, work_chain_);
    chain_registers_ = BuildChainRegisters(components_, axes_, tool_chain_, work_chain_);
    tool_axis_home_ = ToolAxisOf(components_, tool_mount_);
    work_pivot_reference_ = WorkPivotOf(components_, axes_, work_chain_);
}

const ComponentInfo& MachineModel::Component(const std::size_t index) const {
    return components_.at(index);
}

const ComponentSpec& MachineModel::Spec(const std::size_t index) const {
    return definition_.components.at(components_.at(index).definition_index);
}

std::optional<std::size_t> MachineModel::FindComponent(const std::string_view name) const {
    const auto it = by_name_.find(std::string(name));
    if (it == by_name_.end()) return std::nullopt;
    return it->second;
}

std::optional<std::size_t> MachineModel::FindAxis(
        const std::string_view register_name) const {
    const auto it = by_register_.find(std::string(register_name));
    if (it == by_register_.end()) return std::nullopt;
    return it->second;
}

igesio::Matrix4d MachineModel::MountPlacement(const MountKind kind) const {
    const std::size_t index = kind == MountKind::kToolMount ? tool_mount_ : work_mount_;
    return *components_[index].mount_placement;
}

}  // namespace igesio::extensions::machines
