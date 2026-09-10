/**
 * @file extensions/machines/machine/inverse_kinematics.h
 * @brief 逆運動学 (姿勢IK・位置IK・回転角候補からの選択・自己検証)
 * @author Yayoi Habami
 * @date 2026-09-09
 * @copyright 2026 Yayoi Habami
 * @note 工具軸方向から回転軸のNC指令値を解く姿勢IKと、回転軸の計算結果を用いて直進軸のNC指令値を
 *       解く位置IKを提供する. `MachineModel`のIK対象軸 (`AxisInfo::IsIkTarget()`)
 *       のみで、以下を満たさない構成では`igesio::NotImplementedError`を投げる.
 *       (i) 回転軸は0〜2本
 *       (ii) 直進軸はちょうど3本
 * @note 入力の座標系はすべてゼロポーズ機械座標である. ワーク座標系で与えられた
 *       工具軸ベクトル・目標点は、呼び出し側がワーク座標系の配置W_0で写してから渡す.
 * @note 警告 (`Diagnostic`) の`context`は空で返す.
 *       動作生成などでブロック番号等を指定する場合は呼び出し側が与えること.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_MACHINE_INVERSE_KINEMATICS_H_
#define IGESIO_EXTENSIONS_MACHINES_MACHINE_INVERSE_KINEMATICS_H_

#include <optional>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/machine/machine_definition.h"
#include "igesio/extensions/machines/machine/machine_model.h"

namespace igesio::extensions::machines {

/// @brief 順運動学による解の自己検証の誤差 (`CheckSolution`の戻り値)
struct SolutionError {
    /// @brief 工具軸方向のなす角 [rad]
    double angle = 0.0;
    /// @brief 制御点と目標点の距離 [mm]
    double position = 0.0;
};

/// @brief IKの解 (姿勢IK、位置IK、両者の合成で共通)
struct IkSolution {
    /// @brief 解いた軸のNC指令値 [mm] or [rad]
    /// @note `SolveOrientation`では回転軸 (0〜2本),
    ///       `SolvePosition`では直進軸 (3本)、`Solve`ではその両方
    NcValues nc;
    /// @brief 全軸の変位量 (変位量の基準`base_q`に`nc`による変位量を加えたもの)
    /// @note `base_q`を取らない`SolveOrientation`では`std::nullopt`.
    ///       指令値から再度求める場合は`JointsFromNc`を用いる
    std::optional<JointVector> q;
    /// @brief 特異姿勢 (旋回角が不定) で旋回角を0としたか
    /// @note 回転軸を解かない`SolvePosition`では常に`false`
    bool singular = false;
    /// @brief 順運動学による解の自己検証の誤差
    /// @note 検証を行わない`SolveOrientation`/`SolvePosition`では`std::nullopt`
    std::optional<SolutionError> error;
    /// @brief 警告 (可動範囲外、特異姿勢、傾斜角不定、ストローク外)
    /// @note `Solve`では姿勢IK→位置IKの順に並ぶ
    std::vector<Diagnostic> warnings;
};

/// @brief 工具軸方向を実現する回転軸の指令値を計算する (姿勢IK)
/// @param model 運動学モデル
/// @param tool_axis_home 目標の工具軸方向 (ゼロポーズ機械座標. 内部で正規化する)
/// @param prev_nc 直前の指令値 (`BranchPolicy::kContinuous`での符号決定に用いる.
///        該当軸が無ければ0として比較し、両軸とも無ければ`kPositive`と同じとする)
/// @param policy 回転角の解の選択方針 (通常は`model.Definition().branch`)
/// @return 解 (回転軸の指令値`nc`と警告. `q`・`error`は設定しない).
///         回転軸が1本以下なら`policy`・`prev_nc`は使わない
/// @throw std::invalid_argument `tool_axis_home`がゼロベクトルの場合
/// @throw igesio::NotImplementedError IK対象の回転軸が3本以上の場合
/// @throw KinematicsError 到達不能な工具姿勢の場合
IkSolution SolveOrientation(const MachineModel& model,
                            const igesio::Vector3d& tool_axis_home,
                            const NcValues& prev_nc, BranchPolicy policy);

/// @brief 回転軸の計算結果を用いて、制御点を目標点に一致させる直進軸の指令値を計算する (位置IK)
/// @param model 運動学モデル
/// @param target_home 目標点 (ゼロポーズ機械座標)
/// @param control_local 制御点の`tool_mount`フレームでの座標
/// @param rotary_nc 回転軸の指令値
///        (`SolveOrientation`の`nc`、またはNCでの回転軸指令値)
/// @param base_q 全軸の変位量の基準 (チェーン外の軸・未指定の軸の値に用いる)
/// @return 解 (直進3軸の指令値`nc`、`base_q`に回転軸と直進軸の変位量を重ねた`q`、警告.
///         `error`は設定しない)
/// @throw std::invalid_argument `base_q`の長さが軸数と異なる,
///        または`rotary_nc`に未知の軸名が含まれる場合
/// @throw igesio::NotImplementedError IK対象の直進軸が計3本でない場合
/// @throw KinematicsError 方程式が退化している (rank < 3) 場合
IkSolution SolvePosition(const MachineModel& model,
                         const igesio::Vector3d& target_home,
                         const igesio::Vector3d& control_local,
                         const NcValues& rotary_nc, const JointVector& base_q);

/// @brief 工具軸方向と制御点を同時に満たす軸の指令値を計算する (姿勢IK→位置IK)
/// @param model 運動学モデル
/// @param tool_axis_home 目標の工具軸方向 (ゼロポーズ機械座標)
/// @param target_home 目標点 (ゼロポーズ機械座標)
/// @param control_local 制御点の`tool_mount`フレームでの座標
/// @param base_q 全軸の変位量の基準
/// @param prev_nc 直前の指令値 (`BranchPolicy::kContinuous`での符号決定に用いる.
///        該当軸が無ければ0として比較し、両軸とも無ければ`kPositive`と同じとする)
/// @param policy 回転角の解の選択方針 (通常は`model.Definition().branch`)
/// @return 解 (全フィールドを設定する). `q`は`base_q`のコピーに回転軸・直進軸の
///         変位量を書き込んだもの、`error`は`CheckSolution`の結果
/// @throw std::invalid_argument `tool_axis_home`がゼロベクトル,
///        または`base_q`の長さが軸数と異なる場合
/// @throw igesio::NotImplementedError IK対象の回転軸が3本以上,
///        または直進軸の数が3つではない場合
/// @throw KinematicsError 到達不能な工具姿勢の場合,
///        または方程式が退化している (rank < 3) 場合
IkSolution Solve(const MachineModel& model, const igesio::Vector3d& tool_axis_home,
                 const igesio::Vector3d& target_home,
                 const igesio::Vector3d& control_local, const JointVector& base_q,
                 const NcValues& prev_nc, BranchPolicy policy);

/// @brief 順運動学で解を検証する
/// @param model 運動学モデル
/// @param q 全軸の変位量
/// @param tool_axis_home 目標の工具軸方向 (ゼロポーズ機械座標)
/// @param target_home 目標点 (ゼロポーズ機械座標)
/// @param control_local 制御点の`tool_mount`フレームでの座標
/// @return 現在姿勢で以下の誤差を返す：
///         工具軸 F[tm].R·z_tool とワークに固定された目標方向F[wm].R·t のなす角,
///         および制御点 F[tm]·x_control と目標点 F[wm]·x_target の距離.
///         (F[tm]はゼロポーズ機械座標系からtool_mountのへの剛体変換行列,
///         F[wm]はゼロポーズ機械座標系からwork_mountのへの剛体変換行列)
/// @throw std::invalid_argument `q`の長さが軸数と異なる,
///        または`tool_axis_home`がゼロベクトルの場合
SolutionError CheckSolution(const MachineModel& model, const JointVector& q,
                            const igesio::Vector3d& tool_axis_home,
                            const igesio::Vector3d& target_home,
                            const igesio::Vector3d& control_local);

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_MACHINE_INVERSE_KINEMATICS_H_
