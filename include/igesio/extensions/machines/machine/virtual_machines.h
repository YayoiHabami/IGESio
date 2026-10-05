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
 * @note 各軸は可動範囲無制限（`unlimited = true`）で軸の動特性を持たない
 *       （動作生成では`fallback_feed`を用い、infoを1つだけ報告）. 工作物側回転軸C
 *       は`wrap_start = 0`で、工具側回転軸は`wrap_start`を持たない（NC指令値は
 *       0〜2πに正規化しない）. 取り付け部座標系は`tool_mount`/`work_mount`とも
 *       基準機械座標の原点で、工具軸方向は+Z. 暗黙のG54ではワーク座標→基準機械座標の
 *       同次変換がW_0 = I（ワーク座標 = 基準機械座標）になる.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_MACHINE_VIRTUAL_MACHINES_H_
#define IGESIO_EXTENSIONS_MACHINES_MACHINE_VIRTUAL_MACHINES_H_

#include <optional>
#include <string>
#include <string_view>

#include "igesio/extensions/machines/machine/machine_definition.h"

namespace igesio::extensions::machines {

/// @brief 仮想機械の種類
/// @note 括弧内は機械構造ツリー. 軸名はコンポーネント名と同じ
enum class VirtualMachineKind {
    /// @brief 工具側X-Y-Z. 回転軸なし
    /// @note base→X→Y→Z→Tool、base→Table
    kThreeAxis,
    /// @brief 工具側X-Y-Z-C-B（ヘッド・ヘッド型）. 工作物側はTableのみ
    /// @note base→X→Y→Z→C (軸+z)→B (軸+y)→Tool、base→Table.
    ///       工作物側回転軸はC、工具側回転軸はB
    kHeadBc,
    /// @brief 工具側X-Y-Z、工作物側A-C（テーブル・テーブル型）
    /// @note base→X→Y→Z→Tool、base→A (軸+x)→C (軸+z)→Table.
    ///       工作物側回転軸はC、工具側回転軸はA
    kTableAc,
};

/// @brief 仮想機械の設定
struct VirtualMachineOptions {
    /// @brief 機械名（`MachineDefinition::name`. シーンでは`machine:<name>`）
    std::string name = "virtual";
    /// @brief 逆運動学における回転角の解の選択方針
    BranchPolicy branch = BranchPolicy::kContinuous;
    /// @brief 工具側回転軸の可動範囲 ±値 [rad]
    /// @note 省略時は無制限. 回転軸の無い`kThreeAxis`では使わない
    std::optional<double> tool_side_limit_rad;
};

/// @brief 仮想機械の指定 (種類と設定の組)
/// @note プロジェクト定義の`[machine].virtual`等に対応する
struct VirtualMachineSpec {
    /// @brief 仮想機械の種類
    VirtualMachineKind kind = VirtualMachineKind::kThreeAxis;
    /// @brief 設定
    VirtualMachineOptions options;
};

/// @brief 仮想機械の種類の文字列を`VirtualMachineKind`に変換する
/// @param text `"three_axis"` / `"head_bc"` / `"table_ac"`
///        (大文字小文字を区別する)
/// @return 対応する種類. 未知の文字列なら`std::nullopt`
std::optional<VirtualMachineKind> ParseVirtualMachineKind(std::string_view text);

/// @brief 仮想機械の種類を名称 (TOMLで用いる文字列) に変換する
/// @param kind 仮想機械の種類
/// @return `"three_axis"` / `"head_bc"` / `"table_ac"`
std::string_view VirtualMachineKindName(VirtualMachineKind kind);

/// @brief 形状を持たない仮想機械の機械定義を作る
/// @param kind 仮想機械の種類
/// @param options 設定
/// @return 機械定義 (`format_version`は`kMachineFormatVersion`、単位は
///         デフォルト (mm/deg)、`components`は暗黙のbaseと同様に
///         `base`を末尾に置く. 形状、干渉チェック設定、警告は無し)
/// @throw std::invalid_argument `name`が空、または`tool_side_limit_rad`が正でない場合
MachineDefinition MakeVirtualMachineDefinition(
        VirtualMachineKind kind, const VirtualMachineOptions& options = {});

/// @brief 仮想機械の指定から機械定義を作る
/// @param spec 仮想機械の種類と設定
/// @return `MakeVirtualMachineDefinition(spec.kind, spec.options)`と同じ
/// @throw std::invalid_argument `name`が空、または`tool_side_limit_rad`が正でない場合
MachineDefinition MakeVirtualMachineDefinition(const VirtualMachineSpec& spec);

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_MACHINE_VIRTUAL_MACHINES_H_
