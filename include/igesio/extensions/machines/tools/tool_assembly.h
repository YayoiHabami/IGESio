/**
 * @file extensions/machines/tools/tool_assembly.h
 * @brief 工具・ホルダのアセンブリ定義
 * @author Yayoi Habami
 * @date 2026-09-11
 * @copyright 2026 Yayoi Habami
 * @note 工具座標系 (原点=先端、軸=+z) と工具取り付け点 (`tool_mount`) の
 *       フレームの関係は、先端からゲージラインまでの距離 (`ToolProfile::GaugeLength`)
 *       で定める. 取り付けフレームの原点はゲージラインと工具軸の交点であり,
 *       工具座標の点は`ToolMountOffset` = T(0, 0, -GaugeLength) で
 *       取り付けフレームに移す.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_TOOLS_TOOL_ASSEMBLY_H_
#define IGESIO_EXTENSIONS_MACHINES_TOOLS_TOOL_ASSEMBLY_H_

#include <optional>
#include <string>
#include <string_view>

#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/tools/tool_profile.h"

namespace igesio::extensions::machines {

/// @brief 位置IKで目標点に一致させる制御点
enum class ControlPoint {
    /// @brief 工具の指令点 (`ToolProfile::command_point_z`)
    /// @note G43は無視する.
    kTip,
    /// @brief ゲージラインの位置 (取り付けフレームの原点)
    /// @note G43有効時は工具長補正を加える.
    kGauge,
};

/// @brief 制御点の名称を`ControlPoint`に変換する
/// @param text `"tip"` / `"gauge"` (大文字小文字を区別する)
/// @return 対応する制御点. 未知の文字列なら`std::nullopt`
std::optional<ControlPoint> ParseControlPoint(std::string_view text);

/// @brief 制御点の名称 (TOMLで用いる文字列) を取得する
std::string_view ControlPointName(ControlPoint point);

/// @brief 工具アセンブリの定義
/// @note 実体化 (`MakeToolAssembly`) の際、ゲージラインに基づく取り付けオフセットは
///       設定せず、最上位アセンブリの変換は単位行列のまま返す. 実際のオフセットは
///       `ToolMountOffset`で得ること
struct ToolAssemblySpec {
    /// @brief 工具番号 (NCのTで指定する番号に対応)
    /// @note 工具なしの場合は0とする.
    int number = 0;
    /// @brief 工具名
    std::string name;
    /// @brief 工具輪郭
    ToolProfile profile;
    /// @brief 位置IKの制御点
    ControlPoint control_point = ControlPoint::kTip;
};

/// @brief 制御点の`tool_mount`フレーム座標 (位置IKの`control_local`) を計算する
/// @param spec 工具アセンブリ定義
/// @param g43_length G43で有効な工具長補正量 [mm] (無効なら`std::nullopt`)
/// @return `kGauge`: (0, 0, 0)、G43有効時は (0, 0, -g43_length).
///         `kTip`: (0, 0, command_point_z - GaugeLength) (G43は無視する)
/// @note 座標系は先端を原点とする工具座標ではなく、取り付け点 (`tool_mount`) の
///       座標系であり、`SolvePosition`の`control_local`へ直接渡せる.
igesio::Vector3d ControlLocal(const ToolAssemblySpec& spec,
                              std::optional<double> g43_length = std::nullopt);

/// @brief 工具先端を原点とする工具座標系から、`tool_mount`フレームへの変換を返す
/// @return T(0, 0, -GaugeLength).
///         シーン構築時に工具・ホルダアセンブリの最上位に設定すること.
igesio::Matrix4d ToolMountOffset(const ToolAssemblySpec& spec);

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_TOOLS_TOOL_ASSEMBLY_H_
