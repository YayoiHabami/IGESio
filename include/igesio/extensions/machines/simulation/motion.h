/**
 * @file extensions/machines/simulation/motion.h
 * @brief 動作生成 (Animation用の、CLプログラムをもとに時間軸上で等間隔に
 *        補間・サンプリングした姿勢列)
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note `ClProgram`のレコードを順に`ClState::Apply`しながら、動作レコード
 *       (`ClGoto`/`ClArc`/`ClDwell`) ごとに機械座標系での通過点を計算して,
 *       逆運動学で軸変位量を求めて軸の動特性と送りから区間時間を決め,
 *       fpsで補間した`MotionTrack`を作る. 加減速は考慮しない.
 * @note 用語は以下のとおり.
 *       (1) 通過点: 1つの動作レコード (円弧の場合はその分割点ごと) の機械座標での
 *           到達先. 制御点と工具軸方向 (逆運動学で求める形式)、または各軸のNC指令値
 *           (直接指令の形式)
 *       (2) 直前の指令値`prev_nc`: 直前のサンプリング点の全軸のNC指令値.
 *           レコードで指定されなかった軸はこの値を保つ
 *       (3) 登録値相対の座標語: TCP無効時 (G43/G49) の座標語. 機械のNC指令値は
 *           座標語+ワークオフセットの登録値+工具長補正
 * @note 座標系はすべてゼロポーズ機械座標. ワーク座標の制御点と工具軸方向は
 *       `ClState::work_offset`のワーク座標→ゼロポーズ機械座標の同次変換W_0
 *       (`MachiningSetup::FindWorkFrame`) で変換する.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_SIMULATION_MOTION_H_
#define IGESIO_EXTENSIONS_MACHINES_SIMULATION_MOTION_H_

#include <cstddef>
#include <optional>
#include <vector>

#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/machine/axis_values.h"
#include "igesio/extensions/machines/machine/machine_definition.h"
#include "igesio/extensions/machines/machine/machine_model.h"
#include "igesio/extensions/machines/project/project_definition.h"
#include "igesio/extensions/machines/project/setup.h"
#include "igesio/extensions/machines/tools/tool_assembly.h"
#include "igesio/extensions/machines/toolpath/cl_program.h"

namespace igesio::extensions::machines {

/// @brief 1時刻の姿勢（機械動作の描画用サンプリング点）
/// @note 表示用のNC指令値は保持せず、`DisplayNc(model, q)`で求める
struct MotionSample {
    /// @brief 累積時刻 [s]
    double time = 0.0;
    /// @brief 全軸の軸変位量
    JointVector q;
    /// @brief 動作レコードのインデックス (`ClProgram::records`)
    std::size_t record_index = 0;
    /// @brief レコードの終点 (`true`) か補間点 (`false`) か
    /// @note 先頭の初期姿勢データでは`false`
    bool is_command_point = false;
    /// @brief 動作の種類
    /// @note 円弧の分割点は`kArcCw`/`kArcCcw`、ドウェルは`kLinear`.
    ///       先頭の初期姿勢サンプリング点は最初の通過点の種類
    MotionKind kind = MotionKind::kLinear;
    /// @brief 選択中の工具番号 (`ClState::tool`)
    int tool_number = kNoTool;
    /// @brief この通過点で発生した警告 (`MotionTrack::warnings`のインデックス)
    /// @note 同じ通過点の複数の警告は1件に連結する. 無ければ`std::nullopt`
    std::optional<std::size_t> warning;
};

/// @brief 動作生成の統計
struct MotionStats {
    /// @brief 動作レコード (`IsMotion`) の数
    std::size_t motion_record_count = 0;
    /// @brief サンプリング点の総数
    std::size_t sample_count = 0;
    /// @brief レコードの終点であるサンプリング点の数
    std::size_t command_point_count = 0;
    /// @brief 到達不能で直前の姿勢を保持した通過点の数 (補間点を含む)
    std::size_t unreachable_count = 0;
    /// @brief 可動範囲外の警告の数 (`OvertravelPolicy::kWarning`のみ)
    std::size_t overtravel_count = 0;
    /// @brief 順運動学による自己検証の回数
    std::size_t check_count = 0;
    /// @brief 自己検証での工具軸方向の最大誤差 [rad]
    double max_angle_error = 0.0;
    /// @brief 自己検証での制御点の最大誤差 [mm]
    double max_position_error = 0.0;
    /// @brief 実際に用いたfps
    double fps = 0.0;
    /// @brief 総時間 [s] (最終サンプリング点の`time`)
    double duration_sec = 0.0;
    /// @brief `max_samples`のためにfpsを下げた、または補間を無効にしたか
    bool fps_reduced = false;
};

/// @brief 動作のサンプリング点列
struct MotionTrack {
    /// @brief サンプリング点列 (時刻の昇順)
    /// @note 動作レコードがあれば、先頭は時刻0の初期姿勢 (`MachiningSetup::BaseQ`)
    ///       のサンプリング点. 動作レコードが無ければ空
    std::vector<MotionSample> samples;
    /// @brief レコード → そのレコード以降で最初の動作レコードの先頭サンプリング点
    /// @note `ClProgram::records`と同じ長さ. 末尾に動作レコードが無ければ
    ///       `samples.size()`、単調非減少
    std::vector<std::size_t> record_first_sample;
    /// @brief 統計
    MotionStats stats;
    /// @brief 警告と情報 (`Severity::kInfo`)
    /// @note `context`は`"record N"` (fpsの調整の情報は空)、`line`は`sources`の
    ///       行番号
    std::vector<Diagnostic> warnings;
};

/// @brief 動作生成の設定
struct MotionOptions {
    /// @brief 補間のサンプリングレート [1/s]
    double fps = 30.0;
    /// @brief サンプリング点総数の上限 (超える場合はfpsを下げる)
    std::size_t max_samples = 200000;
    /// @brief 区間を補間するか (`false`なら各通過点に1つのサンプリング点)
    bool interpolate = true;
    /// @brief 送りが不明な切削区間の送り [mm/s]
    double fallback_feed = 1000.0 / kSecondsPerMinute;
    /// @brief 円弧の分割の弦誤差 [mm]
    double arc_chord_tolerance = 0.05;
    /// @brief 回転角の解の選択方針 (省略時は機械定義の`branch`)
    std::optional<BranchPolicy> branch;
    /// @brief 可動範囲外の扱い (省略時はプロジェクトの`[run].overtravel`)
    std::optional<OvertravelPolicy> overtravel;
    /// @brief 通過点ごとの所要時間を固定する [s] (送りと軸の動特性を無視する)
    /// @note 指定時は各通過点 (円弧の分割点、ドウェルを含む) の区間時間をこの値に
    ///       する. 送りの無いCLデータの点列を一定の間隔で確認する用途.
    ///       `interpolate = true`との併用も可 (一定時間の区間をfpsで補間する)
    std::optional<double> fixed_record_seconds;
};

/// @brief CLプログラムから動作のサンプリング点列を作成する
/// @param setup 加工セットアップ (運動学モデル、工具、ワーク座標系、初期姿勢)
/// @param program CLプログラム
/// @param options 設定
/// @return サンプリング点列
/// @throw KinematicsError `overtravel == kError`で可動範囲外になった場合,
///        またはTCP無効の座標語が幾何形式のワークオフセットで指令された場合
/// @throw igesio::NotImplementedError 逆運動学が対応しない軸構成の場合
/// @throw std::invalid_argument `fps`/`fallback_feed`/`arc_chord_tolerance`/
///        `fixed_record_seconds`が正でない、または`max_samples`が0の場合
/// @note 到達不能な通過点は警告して直前の姿勢を保つ (例外にしない)
MotionTrack PlanMotion(const MachiningSetup& setup, const ClProgram& program,
                       const MotionOptions& options = {});

/// @brief 時刻に対応するサンプリング点のインデックスを取得する
/// @param track サンプリング点列
/// @param time_sec 時刻 [s]
/// @return `time <= time_sec`を満たす最大のインデックス. 先頭より前なら0,
///         `samples`が空なら0
std::size_t SampleIndexAtTime(const MotionTrack& track, double time_sec);

/// @brief レコードの終点のサンプリング点のインデックスを取得する
/// @param track サンプリング点列
/// @param record_index レコードのインデックス (`ClProgram::records`)
/// @return `record_index`の動作レコードの`is_command_point == true`の
///         サンプリング点. 動作レコードでない、サンプリング点が無い、または
///         `record_index`が`record_first_sample`の範囲外なら`std::nullopt`
/// @note `record_first_sample[r]`はrが状態レコードのときは次の動作レコードを
///       指し、`interpolate = true`では区間の先頭の補間点を指す. 終点の
///       サンプリング点 (工具軌跡の`skip_sample`、スライダーの同期に用いる) は
///       本関数で求めること
std::optional<std::size_t> CommandSampleOfRecord(const MotionTrack& track,
                                                 std::size_t record_index);

/// @brief 軸変位量を表示用のNC指令値に変換する
/// @param model 運動学モデル
/// @param q 全軸の軸変位量
/// @return 各軸のNC指令値. 無制限の回転軸で`wrap_start`があるものは
///         [wrap_start, wrap_start + 2π) に正規化し、それ以外の軸はそのまま
/// @throw std::invalid_argument `q`の長さが軸数と異なる場合
NcValues DisplayNc(const MachineModel& model, const JointVector& q);

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_SIMULATION_MOTION_H_
