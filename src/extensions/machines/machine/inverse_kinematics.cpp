/**
 * @file extensions/machines/machine/inverse_kinematics.cpp
 * @brief 逆運動学 (姿勢IK・位置IK・回転角候補からの選択・自己検証)
 * @author Yayoi Habami
 * @date 2026-09-09
 * @copyright 2026 Yayoi Habami
 * @note 姿勢IKは統一形 T = R(v_t, θ_w) R(u_t, θ_t) z_s（v_t: 工作物側回転軸,
 *       u_t: 工具側回転軸, z_s: 基準コンフィギュレーションの工具軸）を閉形式で解く.
 *       工具側回転軸の回転角θ_t = δ ± Δの符号を可動範囲と回転角の選択方針で選び,
 *       工作物側回転軸の回転角θ_wを求める. 特異コンフィギュレーション（θ_wが不定）
 *       では`kContinuous`で直前の指令値を保ち、他の方針では0にする.
 */
#include "igesio/extensions/machines/machine/inverse_kinematics.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>

#include <Eigen/LU>

#include "igesio/common/errors.h"
#include "igesio/extensions/machines/core/formatting.h"
#include "igesio/extensions/machines/core/rotation.h"
#include "igesio/extensions/machines/core/tolerances.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/machine/forward_kinematics.h"

namespace igesio::extensions::machines {

namespace {

/// @brief 文言中の角度・長さの小数桁数
constexpr int kMessageDigits = 3;
/// @brief 文言中の無次元量 (内積の差等) の小数桁数
constexpr int kRatioDigits = 6;

/// @brief 姿勢IKの候補（+Δ/-Δ）
struct Branch {
    /// @brief 工具側回転軸の回転角θ_t [rad]（工具側回転軸のNC指令値）
    double tool_side_angle = 0.0;
    /// @brief 工作物側回転軸の回転角θ_w [rad]（工作物側回転軸のNC指令値）
    double work_side_angle = 0.0;
    /// @brief 工作物側回転軸の回転角が不定（0とした）か
    bool singular = false;
};

/// @brief 位置IKの連立方程式 A q = b
struct LinearSystem {
    /// @brief 係数行列 (列は各直進軸の実効方向に符号を付けたもの)
    igesio::Matrix3d coefficients = igesio::Matrix3d::Zero();
    /// @brief 右辺
    igesio::Vector3d rhs = igesio::Vector3d::Zero();
    /// @brief 列に対応する軸のインデックス
    std::vector<std::size_t> axes;
};

/// @brief 可動範囲外・ストローク外の警告の`context`
constexpr const char* kLimitsContext = "limits";
/// @brief 特異コンフィギュレーション・工具側回転軸の回転角の不定の警告の`context`
constexpr const char* kSingularContext = "singular";

/// @brief 警告を追加する
/// @param[out] warnings 警告の追記先
/// @param context 警告の種別 (`kLimitsContext` / `kSingularContext`)
/// @param message 内容
/// @param severity 重大度 (省略時は警告)
void Warn(std::vector<Diagnostic>& warnings, const char* context,
          const std::string& message,
          const Severity severity = Severity::kWarning) {
    warnings.push_back(Diagnostic{severity, context, message, 0});
}

/// @brief ベクトルを正規化する
/// @param vector 対象のベクトル
/// @param what 例外メッセージに用いる対象名
/// @return 単位ベクトル
/// @throw std::invalid_argument 零ベクトルの場合
igesio::Vector3d NormalizedOrThrow(const igesio::Vector3d& vector, const char* what) {
    const double norm = vector.norm();
    if (norm < kDegenerateTolerance) {
        throw std::invalid_argument(std::string(what) + " is a zero vector");
    }
    return vector / norm;
}

/// @brief 角度を[start, start + 2π)に正規化する
/// @param angle 対象の角度 [rad]
/// @param start 区間の下端 [rad]
/// @return 正規化した角度 [rad]
double NormalizeToTurn(const double angle, const double start) {
    const double shifted = std::fmod(angle - start, kFullTurn);
    return start + (shifted < 0.0 ? shifted + kFullTurn : shifted);
}

/// @brief 差を(-π, π]に正規化する (無制限回転軸の連続性の距離)
/// @param a 引かれる角度 [rad]
/// @param b 引く角度 [rad]
/// @return a - bを(-π, π]に正規化した値 [rad]
double FoldedDifference(const double a, const double b) {
    const double folded = NormalizeToTurn(a - b, -kHalfTurn);
    return folded == -kHalfTurn ? kHalfTurn : folded;
}

/// @brief 工作物側回転軸の回転角θ_wを求める（R(v_t, θ_w) z_1 = t を満たす角）
/// @param v_t 工作物側回転軸の方向（回転軸が1つの場合はその軸の方向）
/// @param z_1 工具側回転軸で回転した後の工具軸
/// @param t 目標の工具軸
/// @param[out] singular 特異（z_1 ∥ v_t で工作物側回転軸の回転角が不定）ならtrue
/// @return θ_w [rad]. 特異なら0
double WorkSideAngle(const igesio::Vector3d& v_t, const igesio::Vector3d& z_1,
                     const igesio::Vector3d& t, bool* singular) {
    const double p = v_t.dot(z_1.cross(t));
    const double q = z_1.dot(t) - v_t.dot(z_1) * v_t.dot(t);
    *singular = std::hypot(p, q) < kSingularTolerance;
    return *singular ? 0.0 : std::atan2(p, q);
}

/// @brief 特異コンフィギュレーション（工作物側回転軸の回転角が不定）での
///        工作物側回転軸の回転角を、回転角の解の選択方針に従って決める
/// @param work_side_axis 工作物側回転軸（回転軸が1つの場合はその軸）
/// @param prev_nc 直前の指令値
/// @param policy 回転角の解の選択方針
/// @param[out] warnings `kContinuous`以外で追加する情報の追記先
/// @return `kContinuous`なら直前の指令値の工作物側回転軸の値（無ければ0）.
///         それ以外は0で、`Severity::kInfo`の情報を追加する
/// @note 制御装置は不定の軸を動かさないため、`kContinuous`では直前の指令値を保つ.
///       工具軸方向が工作物側回転軸と平行な経路（3軸経路等）では通過点ごとに
///       特異コンフィギュレーションになるため、警告ではなく情報にする
double SingularWorkSideAngle(
        const AxisInfo& work_side_axis, const NcValues& prev_nc,
        const BranchPolicy policy, std::vector<Diagnostic>& warnings) {
    if (policy == BranchPolicy::kContinuous) {
        return prev_nc.GetOr(work_side_axis.register_name, 0.0);
    }
    Warn(warnings, kSingularContext,
         "singular orientation (work-side rotary angle is indeterminate); set to 0",
         Severity::kInfo);
    return 0.0;
}

/// @brief 候補の工具側回転軸の回転角と工作物側回転軸の回転角を求める（回転軸2つ）
/// @param v_t 工作物側回転軸の方向
/// @param u_t 工具側回転軸の方向
/// @param z_s 基準コンフィギュレーションの工具軸
/// @param t 目標の工具軸（単位ベクトル）
/// @param[out] warnings 工具側回転軸の回転角が不定のときの警告の追記先
/// @return 回転角候補（通常2つ. 工具側回転軸の回転角が不定なら1つ）
/// @throw KinematicsError 到達不能な場合
std::vector<Branch> BranchCandidates(
        const igesio::Vector3d& v_t, const igesio::Vector3d& u_t,
        const igesio::Vector3d& z_s, const igesio::Vector3d& t,
        std::vector<Diagnostic>& warnings) {
    const double c = v_t.dot(u_t) * u_t.dot(z_s);
    const double a = v_t.dot(z_s) - c;
    const double b = v_t.dot(u_t.cross(z_s));
    const double d = v_t.dot(t);
    const double rho = std::hypot(a, b);
    if (std::abs(d - c) > rho + kReachTolerance) {
        throw KinematicsError("unreachable tool orientation: |d-c|="
                              + FormatFixed(std::abs(d - c), kRatioDigits)
                              + " > rho=" + FormatFixed(rho, kRatioDigits));
    }
    std::vector<double> tool_side_angles;
    if (rho < kZeroTolerance) {
        // 工具側回転軸が工具軸に平行（または両軸が平行）の場合は
        // 工具側回転軸の回転角が寄与しない
        Warn(warnings, kSingularContext,
             "tool-side rotary angle is indeterminate; set to 0");
        tool_side_angles.push_back(0.0);
    } else {
        const double delta = std::atan2(b, a);
        const double half = std::acos(std::clamp((d - c) / rho, -1.0, 1.0));
        tool_side_angles = {delta + half, delta - half};
    }
    std::vector<Branch> branches;
    for (const double tool_side_angle : tool_side_angles) {
        Branch branch;
        branch.tool_side_angle = tool_side_angle;
        const igesio::Vector3d z_1 = RotationAboutAxis(u_t, tool_side_angle) * z_s;
        branch.work_side_angle = WorkSideAngle(v_t, z_1, t, &branch.singular);
        branches.push_back(branch);
    }
    return branches;
}

/// @brief 直前の指令値との二乗距離（無制限の回転軸は差を(-π, π]に正規化）
/// @param branch 評価対象の回転角候補
/// @param prev_nc 直前の指令値
/// @param tool_side_axis 工具側回転軸
/// @param work_side_axis 工作物側回転軸
/// @return 工具側回転軸の回転角・工作物側回転軸の回転角それぞれの差の二乗和
double ContinuityDistance(const Branch& branch, const NcValues& prev_nc,
                          const AxisInfo& tool_side_axis,
                          const AxisInfo& work_side_axis) {
    double distance = 0.0;
    for (const auto& [axis, value] :
         {std::pair(&tool_side_axis, branch.tool_side_angle),
          std::pair(&work_side_axis, branch.work_side_angle)}) {
        const double previous = prev_nc.GetOr(axis->register_name, 0.0);
        const double difference = axis->unlimited ? FoldedDifference(value, previous)
                                                  : value - previous;
        distance += difference * difference;
    }
    return distance;
}

/// @brief 可動範囲と回転角の選択方針に基づいて符号を決める
/// @param candidates 回転角の候補（先頭がs=+1）
/// @param tool_side_axis 工具側回転軸
/// @param work_side_axis 工作物側回転軸
/// @param prev_nc 直前の指令値
/// @param policy 選択方針
/// @param[out] warnings どの候補も範囲に入らないときの警告の追記先
/// @return 選ばれた回転角
Branch SelectBranch(
        const std::vector<Branch>& candidates, const AxisInfo& tool_side_axis,
        const AxisInfo& work_side_axis, const NcValues& prev_nc,
        const BranchPolicy policy, std::vector<Diagnostic>& warnings) {
    std::vector<Branch> valid;
    for (const Branch& candidate : candidates) {
        const std::optional<double> tool_side_angle =
                WrapAngleIntoLimits(candidate.tool_side_angle, tool_side_axis);
        const std::optional<double> work_side_angle =
                WrapAngleIntoLimits(candidate.work_side_angle, work_side_axis);
        if (tool_side_angle.has_value() && work_side_angle.has_value()) {
            valid.push_back(Branch{*tool_side_angle, *work_side_angle,
                                   candidate.singular});
        }
    }
    if (valid.empty()) {
        Branch fallback = candidates.front();
        fallback.work_side_angle = NormalizeToTurn(fallback.work_side_angle, 0.0);
        Warn(warnings, kLimitsContext,
            "rotary axes out of range: " + tool_side_axis.register_name + "="
                       + FormatDegrees(fallback.tool_side_angle, kMessageDigits)
                       + ", " + work_side_axis.register_name + "="
                       + FormatDegrees(fallback.work_side_angle, kMessageDigits));
        return fallback;
    }
    if (valid.size() == 1) return valid.front();
    const bool has_previous = prev_nc.Contains(tool_side_axis.register_name)
                              || prev_nc.Contains(work_side_axis.register_name);
    if (policy == BranchPolicy::kNegative) return valid.back();
    if (policy == BranchPolicy::kContinuous && has_previous) {
        return ContinuityDistance(valid.front(), prev_nc, tool_side_axis,
                                  work_side_axis)
                       <= ContinuityDistance(valid.back(), prev_nc, tool_side_axis,
                                             work_side_axis)
               ? valid.front() : valid.back();
    }
    return valid.front();
}

/// @brief 1つの回転軸の姿勢IK（その回転軸の回転角のみ）
/// @param model 運動学モデル
/// @param t 目標の工具軸（単位ベクトル）
/// @param prev_nc 直前の指令値（特異コンフィギュレーションの回転角に用いる）
/// @param policy 回転角の解の選択方針（特異コンフィギュレーションの回転角に用いる）
/// @return 解（回転軸1つの指令値と警告）
/// @throw KinematicsError 到達不能な工具姿勢の場合
IkSolution SolveSingleRotary(const MachineModel& model,
                             const igesio::Vector3d& t,
                             const NcValues& prev_nc, const BranchPolicy policy) {
    const AxisInfo& axis = model.Axes()[model.OrientationAxes().front()];
    const igesio::Vector3d v = axis.direction_world;
    const igesio::Vector3d z_s = model.ToolAxisHome();
    const double cone_error = std::abs(v.dot(t) - v.dot(z_s));
    if (cone_error > kReachTolerance) {
        throw KinematicsError("unreachable tool orientation: |v.t - v.z_s|="
                              + FormatFixed(cone_error, kRatioDigits));
    }
    IkSolution solution;
    double angle = WorkSideAngle(v, z_s, t, &solution.singular);
    if (solution.singular) {
        angle = SingularWorkSideAngle(axis, prev_nc, policy, solution.warnings);
    }
    const std::optional<double> wrapped = WrapAngleIntoLimits(angle, axis);
    if (wrapped.has_value()) {
        angle = *wrapped;
    } else {
        angle = NormalizeToTurn(angle, 0.0);
        Warn(solution.warnings, kLimitsContext,
             "rotary axis out of range: " + axis.register_name
             + "=" + FormatDegrees(angle, kMessageDigits));
    }
    solution.nc.Set(axis.register_name, angle);
    return solution;
}

/// @brief 回転軸2つの姿勢IK
/// @param model 運動学モデル
/// @param t 目標の工具軸 (単位ベクトル)
/// @param prev_nc 直前の指令値 (回転角の選択の連続性に用いる)
/// @param policy 回転軸の解の選択方針
/// @return 解 (回転軸2つの指令値と警告)
/// @throw KinematicsError 到達不能な工具姿勢の場合
IkSolution SolveTwoRotaries(
        const MachineModel& model, const igesio::Vector3d& t,
        const NcValues& prev_nc, const BranchPolicy policy) {
    const std::vector<std::size_t>& orientation = model.OrientationAxes();
    const AxisInfo& work_side_axis = model.Axes()[orientation[0]];
    const AxisInfo& tool_side_axis = model.Axes()[orientation[1]];
    IkSolution solution;
    std::vector<Branch> candidates = BranchCandidates(
            work_side_axis.direction_world, tool_side_axis.direction_world,
            model.ToolAxisHome(), t, solution.warnings);
    // 特異コンフィギュレーションは目標の工具軸が工作物側回転軸と平行な場合であり,
    // 全候補で同時に起きる. 工作物側回転軸の回転角を可動範囲の検査と候補の選択の
    // 前に決め、情報は1件だけ追加する
    std::optional<double> singular_work_side_angle;
    for (Branch& candidate : candidates) {
        if (!candidate.singular) continue;
        if (!singular_work_side_angle.has_value()) {
            singular_work_side_angle = SingularWorkSideAngle(
                    work_side_axis, prev_nc, policy, solution.warnings);
        }
        candidate.work_side_angle = *singular_work_side_angle;
    }
    const Branch chosen = SelectBranch(candidates, tool_side_axis, work_side_axis,
                                       prev_nc, policy, solution.warnings);
    solution.singular = chosen.singular;
    solution.nc.Set(tool_side_axis.register_name, chosen.tool_side_angle);
    solution.nc.Set(work_side_axis.register_name, chosen.work_side_angle);
    return solution;
}

/// @brief IK対象の直進軸を工具側連鎖、工作物側連鎖の順に集める
/// @param model 運動学モデル
/// @return `MachineModel::Axes()`におけるインデックス列
std::vector<std::size_t> IkLinearAxes(const MachineModel& model) {
    std::vector<std::size_t> axes;
    for (const std::vector<std::size_t>* chain : {&model.ToolChain(), &model.WorkChain()}) {
        for (const std::size_t component : *chain) {
            const std::optional<std::size_t> axis = model.Component(component).axis;
            if (!axis.has_value()) continue;
            const AxisInfo& info = model.Axes()[*axis];
            if (info.kind == AxisKind::kLinear && info.IsIkTarget()) axes.push_back(*axis);
        }
    }
    return axes;
}

/// @brief 位置IKの連立方程式を組み立てる
/// @param model 運動学モデル
/// @param q0 回転軸を入れてIK対象の直進軸を0にした軸変位量
/// @param axes IK対象の直進軸 (3つ)
/// @param target_home 目標点 (基準機械座標)
/// @param control_local 制御点の工具取り付け部座標系での座標
/// @return 係数行列・右辺・列に対応する軸のインデックス
LinearSystem BuildLinearSystem(
        const MachineModel& model, const JointVector& q0,
        const std::vector<std::size_t>& axes,
        const igesio::Vector3d& target_home,
        const igesio::Vector3d& control_local) {
    const std::vector<igesio::Matrix4d> f0 = Forward(model, q0);
    LinearSystem system;
    system.axes = axes;
    for (std::size_t k = 0; k < axes.size(); ++k) {
        const AxisInfo& axis = model.Axes()[axes[k]];
        const double sign = axis.on_tool_chain ? 1.0 : -1.0;
        system.coefficients.col(static_cast<Eigen::Index>(k)) =
                sign * ApplyDirection(f0[axis.component_index], axis.direction_world);
    }
    system.rhs = ApplyPoint(f0[model.WorkMountIndex()], target_home)
                 - ApplyPoint(f0[model.ToolMountIndex()], control_local);
    return system;
}

/// @brief 直進軸の指令値がストローク内かを検査し、外なら警告する
/// @param axis 対象の軸
/// @param nc 検査するNC指令値 [mm]
/// @param[out] warnings 警告の追記先
void CheckStroke(const AxisInfo& axis, const double nc,
                 std::vector<Diagnostic>& warnings) {
    if (IsWithinLimits(axis, nc)) return;
    // 範囲内として返らなかった以上、`limits`は必ず値を持つ
    const double lo = (*axis.limits)[0];
    const double hi = (*axis.limits)[1];
    Warn(warnings, kLimitsContext,
         "linear axis out of stroke: " + axis.register_name + "="
                   + FormatFixed(nc, kMessageDigits) + " (range ["
                   + FormatFixed(lo, kMessageDigits) + ", "
                   + FormatFixed(hi, kMessageDigits) + "])");
}

}  // namespace



IkSolution SolveOrientation(const MachineModel& model,
                            const igesio::Vector3d& tool_axis_home,
                            const NcValues& prev_nc, const BranchPolicy policy) {
    const igesio::Vector3d t = NormalizedOrThrow(tool_axis_home, "tool_axis_home");
    const std::size_t count = model.OrientationAxes().size();
    if (count > 2) {
        throw igesio::NotImplementedError(
                "orientation IK with " + std::to_string(count)
                + " rotary axes is not supported (up to 2)");
    }
    if (count == 2) return SolveTwoRotaries(model, t, prev_nc, policy);
    if (count == 1) return SolveSingleRotary(model, t, prev_nc, policy);
    const igesio::Vector3d z_s = model.ToolAxisHome();
    if ((t - z_s).norm() > kReachTolerance) {
        const double angle = std::acos(std::clamp(t.dot(z_s), -1.0, 1.0));
        throw KinematicsError("unreachable tool orientation: no rotary axis to tilt "
                              "the tool (angle " + FormatDegrees(angle, kMessageDigits)
                              + " deg)");
    }
    return IkSolution{};
}

IkSolution SolvePosition(const MachineModel& model,
                         const igesio::Vector3d& target_home,
                         const igesio::Vector3d& control_local,
                         const NcValues& rotary_nc, const JointVector& base_q) {
    const std::vector<std::size_t> axes = IkLinearAxes(model);
    if (axes.size() != 3) {
        throw igesio::NotImplementedError(
                "exactly 3 linear axes are supported (found "
                + std::to_string(axes.size()) + ")");
    }
    JointVector q0 = JointsFromNc(model, rotary_nc, base_q);
    for (const std::size_t axis : axes) q0[axis] = 0.0;
    const LinearSystem system =
            BuildLinearSystem(model, q0, axes, target_home, control_local);
    Eigen::FullPivLU<igesio::Matrix3d> lu(system.coefficients);
    lu.setThreshold(kRankTolerance);
    if (lu.rank() < 3) {
        throw KinematicsError("linear axis equations are degenerate (rank "
                              + std::to_string(lu.rank()) + ")");
    }
    const igesio::Vector3d displacement = lu.solve(system.rhs);
    IkSolution solution;
    for (std::size_t k = 0; k < axes.size(); ++k) {
        const AxisInfo& axis = model.Axes()[axes[k]];
        const double q_axis = displacement(static_cast<Eigen::Index>(k));
        const double nc = axis.sigma * q_axis;
        solution.nc.Set(axis.register_name, nc);
        // q0は回転軸を入れ直進軸を0にしたものなので、書き直すと全軸の変位量になる
        q0[axes[k]] = q_axis;
        CheckStroke(axis, nc, solution.warnings);
    }
    solution.q = std::move(q0);
    return solution;
}

IkSolution Solve(
        const MachineModel& model,
        const igesio::Vector3d& tool_axis_home, const igesio::Vector3d& target_home,
        const igesio::Vector3d& control_local, const JointVector& base_q,
        const NcValues& prev_nc, const BranchPolicy policy) {
    const IkSolution orientation =
            SolveOrientation(model, tool_axis_home, prev_nc, policy);
    // 位置IKの解が全軸の変位量を持つため、これに姿勢IKの結果を加える
    IkSolution solution =
            SolvePosition(model, target_home, control_local, orientation.nc, base_q);
    solution.nc.Merge(orientation.nc);
    solution.singular = orientation.singular;
    solution.warnings.insert(solution.warnings.begin(), orientation.warnings.begin(),
                             orientation.warnings.end());
    solution.error = CheckSolution(model, *solution.q, tool_axis_home, target_home,
                                   control_local);
    return solution;
}

SolutionError CheckSolution(
        const MachineModel& model, const JointVector& q,
        const igesio::Vector3d& tool_axis_home,
        const igesio::Vector3d& target_home,
        const igesio::Vector3d& control_local) {
    const igesio::Vector3d t = NormalizedOrThrow(tool_axis_home, "tool_axis_home");
    const std::vector<igesio::Matrix4d> f = Forward(model, q);
    const igesio::Matrix4d& tool = f[model.ToolMountIndex()];
    const igesio::Matrix4d& work = f[model.WorkMountIndex()];
    // 工具側の量はF[tm]、ワークに固定された量はF[wm]で現在のコンフィギュレーションへ移して比べる
    const igesio::Vector3d tool_axis = ApplyDirection(tool, model.ToolAxisHome());
    const igesio::Vector3d target_axis = ApplyDirection(work, t);
    SolutionError error;

    // acosは1付近で条件が悪く (acos(1-ε)≈√(2ε)) 厳密解でも1e-8程度を返すため、
    // 外積と内積のatan2で角度を求める
    error.angle = std::atan2(tool_axis.cross(target_axis).norm(),
                             tool_axis.dot(target_axis));
    error.position = (ApplyPoint(tool, control_local)
                      - ApplyPoint(work, target_home)).norm();
    return error;
}

}  // namespace igesio::extensions::machines
