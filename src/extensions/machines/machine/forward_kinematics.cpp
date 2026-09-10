/**
 * @file extensions/machines/machine/forward_kinematics.cpp
 * @brief 順運動学・NC指令値の変換
 * @author Yayoi Habami
 * @date 2026-09-10
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/machines/machine/forward_kinematics.h"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "igesio/extensions/machines/core/rotation.h"

namespace igesio::extensions::machines {

namespace {

/// @brief 軸1つ分の変換J(θ)
/// @param axis 対象の軸
/// @param displacement 軸変位量θ [mm] or [rad].
///        直進軸では並進[mm]、回転軸では軸まわりの回転[rad]
/// @return 軸1つ分の剛体変換 (4x4同次行列)
igesio::Matrix4d JointTransform(const AxisInfo& axis, const double displacement) {
    // ゼロポーズの場合は無変換
    if (displacement == 0.0) return igesio::Matrix4d::Identity();

    if (axis.kind == AxisKind::kLinear) {
        return Translation(displacement * axis.direction_world);
    }
    return RotationAboutLine(axis.direction_world, axis.point_world, displacement);
}

/// @brief 軸の変位量数が稼働軸の数と一致することを確認する
/// @param what 文言の前置に使う関数名
/// @param expected 稼働軸の数
/// @param actual 与えられた軸の変位量の数
/// @throw std::invalid_argument 一致しない場合
void RequireJointCount(const char* what, const std::size_t expected,
                       const std::size_t actual) {
    if (expected != actual) {
        throw std::invalid_argument(
                std::string(what) + ": joint count mismatch (expected "
                + std::to_string(expected) + ", got " + std::to_string(actual) + ")");
    }
}

}  // namespace



JointVector InitialJoints(const MachineModel& model) {
    const std::vector<AxisInfo>& axes = model.Axes();
    JointVector q(axes.size());
    for (std::size_t i = 0; i < axes.size(); ++i) {
        q[i] = axes[i].sigma * axes[i].initial;
    }
    return q;
}

JointVector JointsFromNc(const MachineModel& model, const NcValues& nc,
                         const JointVector& base) {
    const std::vector<AxisInfo>& axes = model.Axes();
    RequireJointCount("JointsFromNc", axes.size(), base.Size());
    JointVector q = base;
    for (const NcEntry& entry : nc.Entries()) {
        const std::optional<std::size_t> index = model.FindAxis(entry.register_name);
        if (!index.has_value()) {
            throw std::invalid_argument("JointsFromNc: unknown register: "
                                        + entry.register_name);
        }
        q[*index] = axes[*index].sigma * entry.value;
    }
    return q;
}

NcValues NcFromJoints(const MachineModel& model, const JointVector& q) {
    const std::vector<AxisInfo>& axes = model.Axes();
    RequireJointCount("NcFromJoints", axes.size(), q.Size());
    NcValues nc;   // Axes()の順に入れるため、反復順は軸順
    for (std::size_t i = 0; i < axes.size(); ++i) {
        nc.Set(axes[i].register_name, axes[i].sigma * q[i]);
    }
    return nc;
}

std::vector<igesio::Matrix4d> Forward(const MachineModel& model, const JointVector& q) {
    std::vector<igesio::Matrix4d> placements;
    Forward(model, q, &placements);
    return placements;
}

void Forward(const MachineModel& model, const JointVector& q,
             std::vector<igesio::Matrix4d>* out) {
    if (out == nullptr) throw std::invalid_argument("Forward: output is null");
    const std::vector<AxisInfo>& axes = model.Axes();
    RequireJointCount("Forward", axes.size(), q.Size());
    const std::size_t count = model.ComponentCount();
    out->resize(count);
    (*out)[0] = igesio::Matrix4d::Identity();   // インデックス0はbase

    // コンポーネント列を先頭から走査し、親の同次変換行列に軸1つ分の変換J(θ)を
    // 掛けて子の同次変換行列を計算する. `MachineModel::Component(i)`において
    // 親要素は必ず子要素より前にあるため、1回の走査で済む.
    for (std::size_t i = 1; i < count; ++i) {
        const ComponentInfo& component = model.Component(i);
        const igesio::Matrix4d& parent = (*out)[*component.parent];
        if (component.axis.has_value()) {
            (*out)[i] = parent * JointTransform(axes[*component.axis],
                                                q[*component.axis]);
        } else {
            (*out)[i] = parent;
        }
    }
}

}  // namespace igesio::extensions::machines
