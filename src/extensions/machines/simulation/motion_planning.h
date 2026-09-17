/**
 * @file extensions/machines/simulation/motion_planning.h
 * @brief 動作生成の内部構造
 *        (IKの計算が容易な形式への動作レコードの変換、逆運動学、直前の指令値の管理)
 * @author Yayoi Habami
 * @date 2026-09-15
 * @copyright 2026 Yayoi Habami
 * @note `motion.cpp` (時間軸/補間/サンプリング点列) と`axis_resolution.cpp`
 *       (工具軸方向→回転軸の指令) が共有する内部ヘッダ. 公開ヘッダには公開しない.
 * @note 用語と座標系は`motion.h`に従う. 本ヘッダの関数では、レコード1つを,
 *       1つまたは複数の通過点 (基本は1つ、円弧の場合は分割点を生成しそれぞれを
 *       通過点に変換する. いずれも各移動での機械座標系における到達先に対応.
 *       通過点は逆運動学の計算が容易な形式の`detail::Target`として保持) に変換し,
 *       逆運動学で軸変位量を求めるところまでを行う. 時間軸と補間は`motion.cpp`側.
 * @note 警告の`context`は`WarnRecord`で`"record N"`にする. 逆運動学の警告は
 *       `context`に種別 (`"limits"` / `"singular"`) を付けたまま`PoseResult`に
 *       追加し、可動範囲外について (`OvertravelPolicy`) の判定は呼び出し側が行う.
 */
#ifndef SRC_EXTENSIONS_MACHINES_SIMULATION_MOTION_PLANNING_H_
#define SRC_EXTENSIONS_MACHINES_SIMULATION_MOTION_PLANNING_H_

#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/machine/axis_values.h"
#include "igesio/extensions/machines/machine/machine_definition.h"
#include "igesio/extensions/machines/machine/machine_model.h"
#include "igesio/extensions/machines/project/setup.h"
#include "igesio/extensions/machines/tools/tool_assembly.h"
#include "igesio/extensions/machines/toolpath/cl_program.h"

namespace igesio::extensions::machines::detail {

/// @brief 可動範囲外の警告の`context` (`inverse_kinematics.cpp`と同じ)
constexpr const char* kLimitsContext = "limits";
/// @brief 特異姿勢の警告の`context` (`inverse_kinematics.cpp`と同じ)
constexpr const char* kSingularContext = "singular";

/// @brief 通過点の種別
enum class TargetForm {
    /// @brief 制御点+工具軸方向 (姿勢IK+位置IK)
    kToolAxis,
    /// @brief 制御点+回転軸の指令 (位置IKのみ)
    kRotaryWords,
    /// @brief 各軸のNC指令値の直接指令 (IKなし. ドウェルも含む)
    kAxisWords,
};

/// @brief 1つの通過点
/// @note 動作レコード（またはレコードが円弧の場合はその分割点ごと）に対応する
///       機械座標系における到達先
/// @note `NormalizeRecord`が設定する. `point_home`/`tool_axis_home`は
///       通過点の種別に応じて設定し、それ以外は`std::nullopt`
struct Target {
    /// @brief 通過点の種別
    TargetForm form = TargetForm::kAxisWords;
    /// @brief 制御点 (ゼロポーズ機械座標. `kToolAxis`/`kRotaryWords`)
    std::optional<igesio::Vector3d> point_home;
    /// @brief 工具軸方向 (ゼロポーズ機械座標. `kToolAxis`)
    std::optional<igesio::Vector3d> tool_axis_home;
    /// @brief 直接指令する軸のNC指令値
    /// @note `kRotaryWords`では回転軸、`kAxisWords`では直進軸と回転軸
    ///       (登録値と工具長補正を加えた値). 未指定の軸は直前の指令値を保つ
    NcValues nc_words;
    /// @brief 制御点のゼロポーズ機械座標 (`tool_mount`コンポーネントに固定)
    /// @note `tool_mount`フレーム座標cに、`tool_mount`フレーム座標→ゼロポーズ
    ///       機械座標の同次変換H_tm (`MountPlacement(kToolMount)`) を掛けたもの.
    ///       位置IKの`control_local`にそのまま渡す
    igesio::Vector3d control_home = igesio::Vector3d::Zero();
    /// @brief 動作の種類
    MotionKind kind = MotionKind::kLinear;
    /// @brief 送り [mm/s] (無ければ`std::nullopt`)
    std::optional<double> feed;
    /// @brief ドウェル時間 [s] (ドウェル以外は0)
    double dwell_sec = 0.0;
    /// @brief 動作レコードのインデックス
    std::size_t record_index = 0;
    /// @brief レコードの終点か
    bool is_command_point = true;
    /// @brief 元ファイルの行番号 (不明なら0)
    int line = 0;
    /// @brief 選択中の工具番号
    int tool_number = kNoTool;
};

/// @brief 警告の内容 (プログラムにつき1回だけ報告するもの)
struct OnceFlags {
    /// @brief 工具軸方向が定まらず+Zを仮定した
    bool assumed_tool_axis = false;
    /// @brief 工具軸に平行な直進軸が無く工具長補正を加えなかった
    bool no_parallel_axis = false;
    /// @brief 送りが不明で`fallback_feed`を用いた
    bool fallback_feed = false;
    /// @brief 未定義のワークオフセットid
    std::set<std::string> unknown_work_offsets;
    /// @brief 未定義の工具長補正番号
    std::set<int> unknown_length_offsets;
    /// @brief 未解決または未選択の工具番号
    std::set<int> unresolved_tools;
    /// @brief 機械定義に無い軸名
    std::set<std::string> unknown_axes;
    /// @brief 回転方向の正規化で指令値を変えた回転軸
    std::set<std::string> normalized_rotaries;
};

/// @brief 動作生成の作業状態
/// @note レコードを順に処理しながら更新する
struct PlannerState {
    /// @brief 出力済みレコードを適用したモーダル状態
    ClState cl;
    /// @brief 直前の指令値 (全軸)
    NcValues prev_nc;
    /// @brief 直前の軸変位量 (全軸)
    JointVector prev_q;
    /// @brief 現在のワーク座標系 (未解決なら`nullptr`)
    const WorkFrame* frame = nullptr;
    /// @brief 警告の内容 (プログラムにつき1回だけ報告するもの)
    OnceFlags once;
    /// @brief 警告の格納先 (`nullptr`なら報告しない)
    std::vector<Diagnostic>* warnings = nullptr;
};

/// @brief 工具軸方向から求めた回転軸の指令値 (`SolveToolAxis`の戻り値)
struct OrientationResult {
    /// @brief 回転軸の指令値 (到達不能なら直前の指令値の回転軸)
    NcValues rotary;
    /// @brief 特異姿勢 (旋回角が不定) か (`IkSolution::singular`)
    bool singular = false;
    /// @brief 逆運動学の警告 (`context`は種別)
    /// @note 特異姿勢 (`"singular"`) の診断は含めない. 工具軸方向が+Zの
    ///       通常の姿勢で毎回発生するため、動作生成と、工具軸方向から回転軸指令値
    ///       を計算する処理ではいずれも無視する
    std::vector<Diagnostic> warnings;
    /// @brief 到達不能の理由 (到達可能なら`std::nullopt`)
    std::optional<std::string> unreachable;
};

/// @brief 目標の軸変位量と全軸の指令値 (`SolveTarget`の戻り値)
struct PoseResult {
    /// @brief 全軸の軸変位量
    JointVector q;
    /// @brief 全軸のNC指令値
    NcValues nc;
    /// @brief 警告 (逆運動学の警告は`context`が種別、到達不能は空)
    std::vector<Diagnostic> warnings;
    /// @brief 到達不能で直前の姿勢を保持したか
    bool unreachable = false;
};

/// @brief 作業状態を初期化する
/// @param setup 加工セットアップ
/// @param[out] warnings 警告の格納先 (`nullptr`なら報告しない)
/// @return 初期工具/初期ワークオフセット/初期姿勢 (`BaseQ`) に基づく初期状態
PlannerState MakePlannerState(const MachiningSetup& setup,
                              std::vector<Diagnostic>* warnings);

/// @brief レコードと対応付けた警告を追加する
/// @param state 作業状態 (`warnings`が`nullptr`なら何もしない)
/// @param message 内容
/// @param index レコードのインデックス (`context = "record N"`)
/// @param line 元ファイルの行番号 (不明なら0)
void WarnRecord(PlannerState& state, const std::string& message,
                std::size_t index, int line);

/// @brief レコードの元ファイルの行番号を取得する
/// @param program プログラム
/// @param index レコードのインデックス
/// @return 行番号. `sources`が無ければ0
int LineOf(const ClProgram& program, std::size_t index);

/// @brief 現在のワーク座標系を取得する
/// @param setup 加工セットアップ
/// @param state 作業状態 (`frame`を更新する)
/// @param index レコードのインデックス (警告用)
/// @param line 行番号 (警告用)
/// @return `ClState::work_offset` (空なら初期ワークオフセット) のワーク座標系.
///         未定義ならidごとに1件警告して直前の座標系 (無ければ先頭)
const WorkFrame& CurrentWorkFrame(const MachiningSetup& setup,
                                  PlannerState& state,
                                  std::size_t index, int line);

/// @brief 工具が無いときの制御点 (ゲージライン) の`tool_mount`フレーム座標を計算する
/// @param g43_length 有効な工具長補正 [mm] (無ければ`std::nullopt`)
/// @return (0, 0, -g43_length). 工具長補正が無ければ原点
igesio::Vector3d GaugeControlLocal(std::optional<double> g43_length);

/// @brief 工具表の工具から制御点の`tool_mount`フレーム座標を計算する
/// @param setup 加工セットアップ
/// @param tool 工具番号
/// @param g43_length 有効な工具長補正 [mm] (無ければ`std::nullopt`)
/// @return `ControlLocal(spec, g43_length)`. 工具表に無い番号 (`kNoTool`を含む)
///         なら`std::nullopt`
std::optional<igesio::Vector3d> ToolControlLocal(const MachiningSetup& setup,
                                                 int tool,
                                                 std::optional<double> g43_length);

/// @brief 現在の工具と工具長補正から制御点の`tool_mount`フレーム座標を計算する
/// @param setup 加工セットアップ
/// @param state 作業状態
/// @param index レコードのインデックス (警告用)
/// @param line 行番号 (警告用)
/// @return `ControlLocal(spec, g43)` (`tool_mount`フレーム座標).
///         工具なし/未解決工具なら`kGauge`相当、未定義の補正番号なら補正なし
/// @note ゼロポーズ機械座標にするには`MountPlacement(kToolMount)`を掛けること
///       (`ControlHomeFor`). 工具なし/未解決工具は工具番号ごとに1件、未定義の
///       補正番号は番号ごとに1件警告する
igesio::Vector3d ControlLocalFor(const MachiningSetup& setup,
                                 PlannerState& state,
                                 std::size_t index, int line);

/// @brief 回転軸の指令を含むか
/// @param model 運動学モデル
/// @param words 軸の指令
/// @return 機械定義の回転軸の名前が1つでもあれば`true`
bool HasRotaryWords(const MachineModel& model, const NcValues& words);

/// @brief 軸の指令から回転軸の指令値を取り出す
/// @param model 運動学モデル
/// @param words 軸の指令値
/// @return 機械定義の回転軸の名前を持つ項目のみ
NcValues RotaryWordsOf(const MachineModel& model, const NcValues& words);

/// @brief 直前の指令値から姿勢IKの対象軸の値を取り出す
/// @param model 運動学モデル
/// @param prev_nc 直前の指令値
/// @return `OrientationAxes()`の軸のうち`prev_nc`にあるものの値
NcValues PrevRotaryWords(const MachineModel& model, const NcValues& prev_nc);

/// @brief 工具軸方向 (ワーク座標) を決める
/// @param model 運動学モデル
/// @param state 作業状態
/// @param explicit_axis レコードの工具軸方向 (無ければ`std::nullopt`)
/// @param record_words レコードの軸の指令
/// @param index レコードのインデックス (警告用)
/// @param line 行番号 (警告用)
/// @return 以下の順で最初に該当したもの.
///         (1) `explicit_axis`
///         (2) `record_words`に回転軸の指令があれば`std::nullopt` (回転軸の指令の形式)
///         (3) 直前の工具軸方向 (`ClState::tool_axis`)
///         (4) 直前の回転軸の指令 (`ClState::axis_words`) があれば`std::nullopt`
///         (5) +Zを仮定し、プログラムにつき1件警告する
std::optional<igesio::Vector3d> ToolAxisOrFallback(
        const MachineModel& model, PlannerState& state,
        const std::optional<igesio::Vector3d>& explicit_axis,
        const NcValues& record_words, std::size_t index, int line);

/// @brief 工具軸方向から回転軸の指令を計算する (姿勢IK)
/// @param model 運動学モデル
/// @param prev_nc 直前の指令値
/// @param axis_home 工具軸方向 (ゼロポーズ機械座標)
/// @param policy 回転角の解の選択方針
/// @return 回転軸の指令 (無制限軸は直前の指令値に近い回転方向に正規化)、特異姿勢か,
///         および警告. 到達不能なら`unreachable`に理由を設定し、`prev_nc`の
///         回転軸の値を返す
/// @throw igesio::NotImplementedError 対応しない軸構成の場合
OrientationResult SolveToolAxis(const MachineModel& model, const NcValues& prev_nc,
                                const igesio::Vector3d& axis_home,
                                BranchPolicy policy);

/// @brief 無制限の回転軸の指令値を直前の値に近い回転方向に正規化する
/// @param axis 対象の軸
/// @param raw 指令値 [rad]
/// @param base 直前の指令値 [rad]
/// @return 無制限 (または可動範囲の幅が2π以上) の回転軸なら
///         `raw - 2π round((raw - base) / 2π)`. それ以外は`raw`
/// @note 制限軸では正規化後の値が可動範囲外になる場合も`raw`を返す
///       (指令値そのものは範囲内でありうるため)
double ShortestTurn(const AxisInfo& axis, double raw, double base);

/// @brief 動作レコードを通過点 (または通過点列) に変換する
/// @param setup 加工セットアップ
/// @param state 作業状態 (`ClState`は適用前の状態であること)
/// @param program プログラム
/// @param index レコードのインデックス
/// @param chord_tolerance 円弧の分割の弦誤差 [mm]
/// @return 通過点の列 (円弧は分割点ごと). 動作レコードでなければ空.
///         始点の分からない円弧は警告して空
/// @throw KinematicsError TCP無効の座標語が幾何形式のワークオフセットで
///        指令された場合
std::vector<Target> NormalizeRecord(const MachiningSetup& setup, PlannerState& state,
                                    const ClProgram& program, std::size_t index,
                                    double chord_tolerance);

/// @brief 通過点での軸変位量を計算する
/// @param setup 加工セットアップ
/// @param state 作業状態 (直前の指令値と軸変位量は参照のみで更新しない)
/// @param target 通過点
/// @param policy 回転角の解の選択方針
/// @return 解. 到達不能なら直前の姿勢をそのまま返し`unreachable`を`true`にする.
///         直接指令した軸の可動範囲外は`context = "limits"`の警告にする
/// @throw igesio::NotImplementedError 対応しない軸構成の場合
PoseResult SolveTarget(const MachiningSetup& setup, PlannerState& state,
                       const Target& target, BranchPolicy policy);

}  // namespace igesio::extensions::machines::detail

#endif  // SRC_EXTENSIONS_MACHINES_SIMULATION_MOTION_PLANNING_H_
