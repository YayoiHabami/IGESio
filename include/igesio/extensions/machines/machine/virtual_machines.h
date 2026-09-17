/**
 * @file extensions/machines/machine/virtual_machines.h
 * @brief 形状を持たない仮想機械の機械定義
 * @author Yayoi Habami
 * @date 2026-09-17
 * @copyright 2026 Yayoi Habami
 * @note 機械定義ファイルを持たないホスト (ワーク座標で工具を直接置くCAM等)、
 *       単体テスト、ヘッドレスな使用例が`MachiningSetup`/`MachineScene`/
 *       `PlanMotion`をそのまま使えるように、代表的な軸構成の機械定義を
 *       構造体として直接組み立てる (TOMLを経由しない).
 * @note 全軸は可動範囲無制限 (`unlimited = true`) で軸の動特性を持たない
 *       (動作生成では`fallback_feed`を用い、情報を1件のみ報告する). 旋回軸Cは
 *       `wrap_start = 0`で、傾斜軸 (B/A) は`wrap_start`を持たない (NC指令値を
 *       0〜2πに正規化しない). 取り付けフレームは`tool_mount`/`work_mount`とも
 *       ゼロポーズ機械座標の原点で、工具軸方向は+Z. 暗黙のG54では
 *       ワーク座標→ゼロポーズ機械座標の同次変換がW_0 = I
 *       (ワーク座標 = ゼロポーズ機械座標) になる.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_MACHINE_VIRTUAL_MACHINES_H_
#define IGESIO_EXTENSIONS_MACHINES_MACHINE_VIRTUAL_MACHINES_H_

#include <optional>
#include <string>

#include "igesio/extensions/machines/machine/machine_definition.h"

namespace igesio::extensions::machines {

/// @brief 仮想機械の種類
/// @note 括弧内は運動学ツリー. 軸名はコンポーネント名と同じ
enum class VirtualMachineKind {
    /// @brief 工具側X-Y-Z. 回転軸なし
    /// @note base→X→Y→Z→Tool、base→Table
    kThreeAxis,
    /// @brief 工具側X-Y-Z-C-B (ヘッド・ヘッド型). ワーク側はTableのみ
    /// @note base→X→Y→Z→C (軸+z)→B (軸+y)→Tool、base→Table.
    ///       旋回軸はC (外側)、傾斜軸はB (内側)
    kHeadBc,
    /// @brief 工具側X-Y-Z、ワーク側A-C (テーブル・テーブル型)
    /// @note base→X→Y→Z→Tool、base→A (軸+x)→C (軸+z)→Table.
    ///       旋回軸はC (外側)、傾斜軸はA (内側)
    kTableAc,
};

/// @brief 仮想機械の設定
struct VirtualMachineOptions {
    /// @brief 機械名 (`MachineDefinition::name`. シーンでは`machine:<name>`)
    std::string name = "virtual";
    /// @brief 逆運動学における回転角の解の選択方針
    BranchPolicy branch = BranchPolicy::kContinuous;
    /// @brief 傾斜軸 (B/A) の可動範囲 ±値 [rad]
    /// @note 省略時は無制限. 回転軸の無い`kThreeAxis`では使わない
    std::optional<double> tilt_limit_rad;
};

/// @brief 形状を持たない仮想機械の機械定義を作る
/// @param kind 仮想機械の種類
/// @param options 設定
/// @return 機械定義 (`format_version`は`kMachineFormatVersion`、単位は
///         デフォルト (mm/deg)、`components`は暗黙のbaseと同様に
///         `base`を末尾に置く. 形状、干渉チェック設定、警告は無し)
/// @throw std::invalid_argument `name`が空、または`tilt_limit_rad`が正でない場合
MachineDefinition MakeVirtualMachineDefinition(
        VirtualMachineKind kind, const VirtualMachineOptions& options = {});

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_MACHINE_VIRTUAL_MACHINES_H_
