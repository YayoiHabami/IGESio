/**
 * @file tests/extensions/machines/machine/machines_for_testing.h
 * @brief machines拡張のテストで共有する機械定義のフィクスチャ
 * @author Yayoi Habami
 * @date 2026-09-09
 * @copyright 2026 Yayoi Habami
 * @note TOMLの入出力、運動学モデル、逆運動学のテストが同じTOML文字列と
 *       補助関数を使えるようにする. 各関数のTOMLは現バージョンの最小構成で、
 *       形状ファイルは参照しない (読込時にファイルの存在は確認されない).
 */
#ifndef TESTS_EXTENSIONS_MACHINES_MACHINE_MACHINES_FOR_TESTING_H_
#define TESTS_EXTENSIONS_MACHINES_MACHINE_MACHINES_FOR_TESTING_H_

#include <filesystem>
#include <stdexcept>
#include <string>

#include "igesio/extensions/machines/machine/machine_definition.h"
#include "igesio/extensions/machines/machine/machine_io.h"
#include "igesio/extensions/machines/machine/machine_model.h"

namespace machines_test {

/// @brief 実例TOMLのパス (tests/test_data/machines/t-ZYX-b-AC-w.toml)
/// @note `./`付きのインクルードでは、clangの`__FILE__`に`.`がパス要素として残る
///       (gccでは除去される). 親を遡る段数が処理系で変わらないよう、正規化してから遡る.
inline const std::filesystem::path kFixturePath =
        std::filesystem::path(__FILE__).lexically_normal()
                .parent_path().parent_path().parent_path().parent_path()
                / "test_data" / "machines" / "t-ZYX-b-AC-w.toml";

/// @brief 文字列入力の相対パス基準ディレクトリ
inline const std::filesystem::path kBaseDir = std::filesystem::path("C:/machines");

/// @brief 最小構成の機械定義 (工具側XYZ・ワーク側AC、プリミティブのみ、暗黙base)
/// @note 実例機と同じ幾何 (A軸は(0,0,60)を通るx軸、Toolは(0,-180,250.5)).
///       Aは`limits = [-90, 90]`、Cは無制限・`wrap_start = 0`
inline std::string MinimalXyzAc() {
    return R"([format]
name = "machine-definition"
version = [2, 0]

[machine]
name = "minimal-xyz-ac"

[[component]]
name = "A"
type = "rotary"
parent = "base"

[component.axis]
register = "A"
direction = [1, 0, 0]
point = [0, 0, 60]
limits = [-90, 90]

[[component]]
name = "C"
type = "rotary"
parent = "A"

[component.axis]
register = "C"
direction = [0, 0, 1]
point = [0, 0, 0]
unlimited = true
wrap_start = 0

[[component]]
name = "Table"
type = "work_mount"
parent = "C"

[component.frame]
origin = [0, 0, 0]

[[component]]
name = "X"
type = "linear"
parent = "base"

[component.axis]
register = "X"
direction = [1, 0, 0]
limits = [-400, 400]

[[component.geometry]]
name = "x-box"
primitive = "box"
size = [10, 20, 30]

[[component]]
name = "Y"
type = "linear"
parent = "X"

[component.axis]
register = "Y"
direction = [0, 1, 0]
limits = [0, 300]

[[component]]
name = "Z"
type = "linear"
parent = "Y"

[component.axis]
register = "Z"
direction = [0, 0, 1]
limits = [-90, 300]

[[component]]
name = "Spindle"
type = "spindle"
parent = "Z"

[[component]]
name = "Tool"
type = "tool_mount"
parent = "Spindle"

[component.frame]
origin = [0, -180, 250.5]
z_axis = [0, 0, 1]
)";
}

/// @brief 3軸のみ (XYZ・回転軸なし) の最小構成
/// @note Toolは(0,0,100)、Tableは原点. 各軸`limits = [-100, 100]`
inline std::string ThreeAxis() {
    return R"([format]
name = "machine-definition"
version = [2, 0]

[machine]
name = "three-axis"

[[component]]
name = "Table"
type = "work_mount"
parent = "base"

[component.frame]
origin = [0, 0, 0]

[[component]]
name = "X"
type = "linear"
parent = "base"

[component.axis]
register = "X"
direction = [1, 0, 0]
limits = [-100, 100]

[[component]]
name = "Y"
type = "linear"
parent = "X"

[component.axis]
register = "Y"
direction = [0, 1, 0]
limits = [-100, 100]

[[component]]
name = "Z"
type = "linear"
parent = "Y"

[component.axis]
register = "Z"
direction = [0, 0, 1]
limits = [-100, 100]

[[component]]
name = "Tool"
type = "tool_mount"
parent = "Z"

[component.frame]
origin = [0, 0, 100]
)";
}

/// @brief 傾斜B軸上にC軸を持つテーブル・テーブル型 (工具側XZ・ワーク側Y-B-C)
/// @note B軸の方向は(cos45°, 0, sin45°)で原点を通り`limits = [0, 180]`、
///       C軸はz軸で原点を通り無制限 (`wrap_start = 0`). Toolは(0,0,200).
///       Bを回すとCの実効軸方向がR_B·d_Cになる
inline std::string TiltedBc() {
    return R"([format]
name = "machine-definition"
version = [2, 0]

[machine]
name = "tilted-bc"

[[component]]
name = "X"
type = "linear"
parent = "base"

[component.axis]
register = "X"
direction = [1, 0, 0]
limits = [-500, 500]

[[component]]
name = "Z"
type = "linear"
parent = "X"

[component.axis]
register = "Z"
direction = [0, 0, 1]
limits = [-500, 500]

[[component]]
name = "Spindle"
type = "spindle"
parent = "Z"

[[component]]
name = "Tool"
type = "tool_mount"
parent = "Spindle"

[component.frame]
origin = [0, 0, 200]

[[component]]
name = "Y"
type = "linear"
parent = "base"

[component.axis]
register = "Y"
direction = [0, 1, 0]
limits = [-500, 500]

[[component]]
name = "B"
type = "rotary"
parent = "Y"

[component.axis]
register = "B"
direction = [0.70710678, 0, 0.70710678]
point = [0, 0, 0]
limits = [0, 180]

[[component]]
name = "C"
type = "rotary"
parent = "B"

[component.axis]
register = "C"
direction = [0, 0, 1]
point = [0, 0, 0]
unlimited = true
wrap_start = 0

[[component]]
name = "Attach"
type = "work_mount"
parent = "C"

[component.frame]
origin = [0, 0, 0]
)";
}

/// @brief ヘッド・ヘッド型 (工具側X-Y-Z-C-B、ワーク側はTableのみ)
/// @note C軸はz軸で(0,0,300)を通り無制限 (`wrap_start = 0`)、B軸はy軸で
///       (0,0,300)を通り`limits = [-120, 120]`. Toolは(0,0,200)で工具軸は+z.
///       工具の向きを決める回転軸は根元側のCが外側、Bが内側 (いずれもσ=+1)
inline std::string HeadBc() {
    return R"([format]
name = "machine-definition"
version = [2, 0]

[machine]
name = "head-bc"

[[component]]
name = "X"
type = "linear"
parent = "base"

[component.axis]
register = "X"
direction = [1, 0, 0]
limits = [-500, 500]

[[component]]
name = "Y"
type = "linear"
parent = "X"

[component.axis]
register = "Y"
direction = [0, 1, 0]
limits = [-500, 500]

[[component]]
name = "Z"
type = "linear"
parent = "Y"

[component.axis]
register = "Z"
direction = [0, 0, 1]
limits = [-500, 500]

[[component]]
name = "C"
type = "rotary"
parent = "Z"

[component.axis]
register = "C"
direction = [0, 0, 1]
point = [0, 0, 300]
unlimited = true
wrap_start = 0

[[component]]
name = "B"
type = "rotary"
parent = "C"

[component.axis]
register = "B"
direction = [0, 1, 0]
point = [0, 0, 300]
limits = [-120, 120]

[[component]]
name = "Spindle"
type = "spindle"
parent = "B"

[[component]]
name = "Tool"
type = "tool_mount"
parent = "Spindle"

[component.frame]
origin = [0, 0, 200]

[[component]]
name = "Table"
type = "work_mount"
parent = "base"

[component.frame]
origin = [0, 0, 0]
)";
}

/// @brief 文字列の最初の出現を置換する (異常系・派生構成の作成用)
/// @throw std::logic_error 置換元が見つからない場合 (テストの記述ミス)
inline std::string Replace(std::string base, const std::string& from,
                           const std::string& to) {
    const auto pos = base.find(from);
    if (pos == std::string::npos) {
        throw std::logic_error("Replace: anchor not found: " + from);
    }
    return base.replace(pos, from.size(), to);
}

/// @brief 文字列入力で機械定義を読み込む
inline igesio::extensions::machines::MachineDefinition ReadDefinition(
        const std::string& toml) {
    return igesio::extensions::machines::ReadMachineDefinitionFromString(
            toml, kBaseDir, "<test>");
}

/// @brief 文字列入力で機械定義を読み込み、運動学モデルを作る
inline igesio::extensions::machines::MachineModel ReadModel(const std::string& toml) {
    return igesio::extensions::machines::MachineModel(ReadDefinition(toml));
}

/// @brief 名前でコンポーネントを引く
/// @throw std::logic_error 見つからない場合
inline const igesio::extensions::machines::ComponentSpec& FindComponent(
        const igesio::extensions::machines::MachineDefinition& definition,
        const std::string& name) {
    for (const auto& component : definition.components) {
        if (component.name == name) return component;
    }
    throw std::logic_error("component not found: " + name);
}

}  // namespace machines_test

#endif  // TESTS_EXTENSIONS_MACHINES_MACHINE_MACHINES_FOR_TESTING_H_
