/**
 * @file tests/extensions/machines/simulation/motion_for_testing.h
 * @brief machines拡張のテストで共有する動作生成のフィクスチャ
 * @author Yayoi Habami
 * @date 2026-09-16
 * @copyright 2026 Yayoi Habami
 * @note 動作生成、アニメーション生成、シーン構築、表示オブジェクトのテストが
 *       同じセットアップとCLプログラムの組み立て関数を使えるようにする.
 * @note セットアップは3種類. (1) 実例機 (工具側XYZ・ワーク側AC. 工具取り付け点
 *       (0,-180,250.5)、各軸に動特性あり)、(2) `MinimalXyzAc` (幾何は実例機と
 *       同じで動特性なし. 区間時間が0になる)、(3) `ThreeAxis` (回転軸なし).
 *       いずれも`MinimalProject` (簡易ボール工具#1・G54登録値・G55幾何形式・
 *       boxストック・初期Z=100) と組み合わせる.
 */
#ifndef TESTS_EXTENSIONS_MACHINES_SIMULATION_MOTION_FOR_TESTING_H_
#define TESTS_EXTENSIONS_MACHINES_SIMULATION_MOTION_FOR_TESTING_H_

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/machine/axis_values.h"
#include "igesio/extensions/machines/project/setup.h"
#include "igesio/extensions/machines/simulation/motion.h"
#include "igesio/extensions/machines/toolpath/cl_program.h"
#include "../machine/machines_for_testing.h"
#include "../project/projects_for_testing.h"

namespace motion_test {

/// @brief 実例機 (動特性あり) の最小構成プロジェクトからセットアップを作る
inline igesio::extensions::machines::MachiningSetup MakeSetup(
        const std::string& toml = projects_test::MinimalProject()) {
    return igesio::extensions::machines::MachiningSetup(
            projects_test::ReadProjectText(toml));
}

/// @brief `MinimalProject`の`[machine]`を機械定義の文字列に差し替えて
///        セットアップを作る
/// @param machine_toml 機械定義のTOML
/// @param dir_name 一時ディレクトリ名 (テストごとに分ける)
/// @param project_toml プロジェクトのTOML (省略時は`MinimalProject`)
inline igesio::extensions::machines::MachiningSetup MakeSetupWithMachine(
        const std::string& machine_toml, const std::string& dir_name,
        const std::string& project_toml = projects_test::MinimalProject()) {
    const std::string body = projects_test::Replace(
            project_toml, "[machine]\nlibrary = \"t-ZYX-b-AC-w.toml\"\n", "");
    return igesio::extensions::machines::MachiningSetup(
            projects_test::ReadProjectWithMachine(machine_toml, body, dir_name));
}

/// @brief 動特性の無い機械 (`MinimalXyzAc`. 幾何は実例機と同じ) でセットアップを作る
inline igesio::extensions::machines::MachiningSetup MakeSetupWithoutDynamics() {
    return MakeSetupWithMachine(machines_test::MinimalXyzAc(),
                                "igesio_motion_no_dynamics");
}

/// @brief 3軸機 (`ThreeAxis`. 工具取り付け点 (0,0,100)) でセットアップを作る
/// @note G54の登録値は工具取り付け点を原点に置く {Z = -100} にする (W_0 = I)
inline igesio::extensions::machines::MachiningSetup MakeSetupThreeAxis() {
    const std::string body = projects_test::Replace(
            projects_test::MinimalProject(),
            "values = { X = 0.0, Y = 180.0, Z = -250.5 }",
            "values = { X = 0.0, Y = 0.0, Z = -100.0 }");
    return MakeSetupWithMachine(machines_test::ThreeAxis(),
                                "igesio_motion_three_axis", body);
}

/// @brief 制御点と工具軸方向を持つ移動を作る
inline igesio::extensions::machines::ClGoto Goto(
        const igesio::Vector3d& point,
        const std::optional<igesio::Vector3d>& axis = std::nullopt,
        const igesio::extensions::machines::MotionKind kind =
                igesio::extensions::machines::MotionKind::kLinear) {
    igesio::extensions::machines::ClGoto motion;
    motion.kind = kind;
    motion.point = point;
    motion.tool_axis = axis;
    return motion;
}

/// @brief 軸の指令のみの移動 (登録値相対の座標語、または機械座標) を作る
inline igesio::extensions::machines::ClGoto Words(
        const igesio::extensions::machines::NcValues& words,
        const igesio::extensions::machines::MotionFrame frame =
                igesio::extensions::machines::MotionFrame::kWork,
        const igesio::extensions::machines::MotionKind kind =
                igesio::extensions::machines::MotionKind::kLinear) {
    igesio::extensions::machines::ClGoto motion;
    motion.kind = kind;
    motion.axis_words = words;
    motion.frame = frame;
    return motion;
}

/// @brief レコード列からプログラムを作る (行番号は索引+1)
inline igesio::extensions::machines::ClProgram Program(
        std::vector<igesio::extensions::machines::ClRecord> records) {
    igesio::extensions::machines::ClProgram program;
    program.records = std::move(records);
    for (std::size_t i = 0; i < program.records.size(); ++i) {
        program.sources.push_back(igesio::extensions::machines::SourceLocation{
                0, static_cast<int>(i) + 1});
    }
    return program;
}

/// @brief 工具#1を選択してから始まるプログラムを作る
inline igesio::extensions::machines::ClProgram WithTool(
        std::vector<igesio::extensions::machines::ClRecord> records) {
    records.insert(records.begin(), igesio::extensions::machines::ClLoadTool{1});
    return Program(std::move(records));
}

/// @brief 可動範囲外を無視して動作を生成する
/// @note フィクスチャのZ軸は-90 mmが下限で、テーブル上面付近の制御点は
///       可動範囲外になる. 可動範囲外の扱い自体は動作生成のテストで検証する
inline igesio::extensions::machines::MotionTrack Plan(
        const igesio::extensions::machines::MachiningSetup& setup,
        const igesio::extensions::machines::ClProgram& program,
        igesio::extensions::machines::MotionOptions options = {}) {
    if (!options.overtravel.has_value()) {
        options.overtravel = igesio::extensions::machines::OvertravelPolicy::kIgnore;
    }
    return igesio::extensions::machines::PlanMotion(setup, program, options);
}

/// @brief 文言に部分文字列を含む警告の数
inline std::size_t CountWarnings(
        const std::vector<igesio::extensions::machines::Diagnostic>& warnings,
        const std::string& text) {
    std::size_t count = 0;
    for (const auto& warning : warnings) {
        if (warning.message.find(text) != std::string::npos) ++count;
    }
    return count;
}

}  // namespace motion_test

#endif  // TESTS_EXTENSIONS_MACHINES_SIMULATION_MOTION_FOR_TESTING_H_
