/**
 * @file extensions/machines/machine/forward_kinematics.h
 * @brief 順運動学・NC指令値の変換
 * @author Yayoi Habami
 * @date 2026-09-10
 * @copyright 2026 Yayoi Habami
 * @note 仕様の順運動学の式`F_c(q) = J_1(σ_1 q_1) ... J_n(σ_n q_n)`を評価する.
 *       ゼロポーズ (全軸の変位量が0) で全コンポーネントの位置姿勢は単位行列になる.
 * @note 機械の軸の値の表現 (`JointVector`/`NcValues`) およびσの設定については
 *       `axis_values.h`を参照のこと.
 *       両者の相互変換は本ヘッダの`JointsFromNc`/`NcFromJoints`のみが行う.
 * @note いずれも`MachineModel`を読むだけの非メンバ関数で、姿勢は引数で渡す.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_MACHINE_FORWARD_KINEMATICS_H_
#define IGESIO_EXTENSIONS_MACHINES_MACHINE_FORWARD_KINEMATICS_H_

#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/machine/machine_model.h"

namespace igesio::extensions::machines {

/// @brief 既定姿勢の軸変位量 (各軸の`initial`にσを掛けたもの)
/// @param model 運動学モデル
/// @return 全軸の変位量 (軸のインデックス順)
JointVector InitialJoints(const MachineModel& model);

/// @brief NC指令値を軸変位量へ変換する
/// @param model 運動学モデル
/// @param nc 各軸のNC指令値. 含まれない軸は`base`の値を使う
/// @param base 全軸の変位量の基準
/// @return `base`に`nc`で指定された軸の変位量を上書きしたもの
/// @throw std::invalid_argument `base`の長さが軸数と異なる,
///        または`nc`に未知の軸名が含まれる場合
JointVector JointsFromNc(const MachineModel& model, const NcValues& nc,
                         const JointVector& base);

/// @brief 全軸の変位量をNC指令値へ変換する
/// @param model 運動学モデル
/// @param q 全軸の変位量
/// @return 各軸のNC指令値
/// @throw std::invalid_argument `q`の長さが軸数と異なる場合
NcValues NcFromJoints(const MachineModel& model, const JointVector& q);

/// @brief 全コンポーネントの同次変換行列F_c(q)を計算する (順運動学)
/// @param model 運動学モデル
/// @param q 全軸の変位量
/// @return 全コンポーネントの同次変換行列 (`MachineModel::Component()`の順)
///         (ゼロポーズ機械座標系から、軸変位量qにおける機械座標系への剛体変換)
/// @throw std::invalid_argument `q`の長さが軸数と異なる場合
std::vector<igesio::Matrix4d> Forward(const MachineModel& model, const JointVector& q);

/// @brief 全コンポーネントの同次変換行列F_c(q)を計算する (順運動学)
/// @param model 運動学モデル
/// @param q 全軸の変位量
/// @param[out] out 同次変換行列 (`model.ComponentCount()`にリサイズされる)
///             (ゼロポーズ機械座標系から、軸変位量qにおける機械座標系への剛体変換)
/// @throw std::invalid_argument `q`の長さが軸数と異なる,
///        または`out`が`nullptr`の場合
/// @note 出力引数版. 毎フレーム呼ぶ際に再確保を避けるために使用する
void Forward(const MachineModel& model, const JointVector& q,
             std::vector<igesio::Matrix4d>* out);

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_MACHINE_FORWARD_KINEMATICS_H_
