/**
 * @file extensions/machines/machine/machine_writing.cpp
 * @brief 機械定義のTOML書き出し (内部ヘッダ)
 * @author Yayoi Habami
 * @date 2026-09-09
 * @copyright 2026 Yayoi Habami
 * @note `toml::ordered_value`で文書を組み立て、toml11のシリアライザに
 *       `[[component]]`, `[component.axis]`等の配置を任せる. 値の生成・幾何要素・
 *       `[format]`・`[units]`はプロジェクト定義と共有する`toml_writing.h`に置く.
 */
#include "extensions/machines/machine/machine_writing.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include <toml.hpp>

#include "igesio/extensions/machines/core/rotation.h"
#include "igesio/extensions/machines/core/tolerances.h"
#include "igesio/extensions/machines/core/units.h"
#include "extensions/machines/machine/toml_writing.h"

namespace igesio::extensions::machines::detail {

namespace {

/// @brief 出力の先頭に置く見出しコメント
constexpr const char* kHeaderComment =
        "# machine-definition 2.0 "
        "(written by the IGESio machines extension)\n\n";
/// @brief `clearance`が`default_clearance`と等しいとみなす許容誤差 [mm]
constexpr double kClearanceTolerance = 1e-9;



/**
 * ---- 幾何要素 ----
 */

/// @brief `local_frame` (`[[component]].local_frame`) が単位行列でなければ書く
void PutLocalFrame(TomlValue& component, const igesio::Matrix4d& local_frame,
                   const WriteContext& ctx) {
    if (local_frame.isIdentity(kZeroTolerance)) return;
    TomlValue frame = Table();
    PutOrigin(frame, TranslationPart(local_frame), ctx.length_scale);
    PutRotation(frame, RotationPart(local_frame));
    component["local_frame"] = frame;
}

/// @brief マウントの`frame` (`[[component]].frame`) を
///        `origin`+`z_axis`(+`x_axis`) で書く
/// @note 保持しているH (取り付け先座標系→ゼロポーズ機械座標) は`local_frame`込み
///       なので、`C_c⁻¹·H`でコンポーネント座標相対に戻す.
///       回転が単位行列なら`z_axis`のみ (`x_axis`は既定のコンポーネント座標x軸)
TomlValue MakeFrame(const igesio::Matrix4d& local_frame,
                    const igesio::Matrix4d& placement, const double length_scale) {
    const igesio::Matrix4d relative = RigidInverse(local_frame) * placement;
    const igesio::Matrix3d rotation = RotationPart(relative);
    TomlValue frame = Table();
    frame["origin"] = Vec3(TranslationPart(relative) / length_scale);
    frame["z_axis"] = Vec3(rotation.col(2));
    if (!rotation.isIdentity(kZeroTolerance)) {
        frame["x_axis"] = Vec3(rotation.col(0));
    }
    return frame;
}



/**
 * ---- 軸 ----
 */

/// @brief `initial`を書く必要があるか (0以外、またはlimitsが0を含まない)
/// @note 読込側はlimitsが0を含まない軸で`initial`の省略を許さない
bool NeedsInitial(const AxisSpec& axis) {
    if (axis.initial != 0.0) return true;   // 既定値 (省略時0) との一致検査
    if (!axis.limits.has_value()) return false;
    return !((*axis.limits)[0] <= 0.0 && 0.0 <= (*axis.limits)[1]);
}

/// @brief `axis.dynamics` (`[[component]].axis.dynamics`) の指定された値のみ書く
/// @note 送り3種は内部単位の毎秒からファイルの毎分へ戻す.
void PutDynamics(TomlValue& axis, const AxisDynamics& dynamics, const double scale) {
    const double feed_scale = scale / kSecondsPerMinute;
    const std::array<std::tuple<const char*, const std::optional<double>*, double>, 6>
            keys = {{{"rapid_feed", &dynamics.rapid_feed, feed_scale},
                     {"max_feed", &dynamics.max_feed, feed_scale},
                     {"min_feed", &dynamics.min_feed, feed_scale},
                     {"accel", &dynamics.accel, scale},
                     {"decel", &dynamics.decel, scale},
                     {"resolution", &dynamics.resolution, scale}}};
    TomlValue table = Table();
    bool any = false;
    for (const auto& [key, value, factor] : keys) {
        if (!value->has_value()) continue;
        table[key] = Real(**value / factor);
        any = true;
    }
    if (any) axis["dynamics"] = table;
}

/// @brief `component.spindle`を書き出す
/// @note テーブルの有無は`ComponentSpec::spindle`の有無に対応し、
///       指定された値のみ書く. `max_rpm`は単位換算しない
TomlValue MakeSpindle(const SpindleSpec& spindle) {
    TomlValue table = Table();
    if (spindle.max_rpm.has_value()) table["max_rpm"] = Real(*spindle.max_rpm);
    return table;
}

/// @brief `component.axis`を書き出す
/// @param is_rotary 回転軸なら`true` (limits等の換算に角度単位を使う)
TomlValue MakeAxis(const AxisSpec& axis, const bool is_rotary,
                   const WriteContext& ctx) {
    const double scale = is_rotary ? ctx.angle_scale : ctx.length_scale;
    TomlValue table = Table();
    table["register"] = axis.register_name;
    table["direction"] = Vec3(axis.direction);
    if (axis.point.has_value()) {
        table["point"] = Vec3(*axis.point / ctx.length_scale);
    }
    if (axis.limits.has_value()) {
        table["limits"] = Reals({(*axis.limits)[0] / scale, (*axis.limits)[1] / scale});
    }
    if (axis.unlimited) table["unlimited"] = true;
    if (axis.wrap_start.has_value()) {
        table["wrap_start"] = Real(*axis.wrap_start / ctx.angle_scale);
    }
    if (NeedsInitial(axis)) table["initial"] = Real(axis.initial / scale);
    PutDynamics(table, axis.dynamics, scale);
    return table;
}



/**
 * ---- コンポーネント ----
 */

/// @brief 暗黙のbaseとして省略できるか (形状も`local_frame`も持たないbase)
bool IsImplicitBase(const ComponentSpec& component) {
    return component.type == ComponentType::kBase && component.geometries.empty()
           && component.local_frame.isIdentity(kZeroTolerance);
}

/// @brief `[[component]]`の1要素を書き出す. 省略可能なキーは省略する
TomlValue MakeComponent(const ComponentSpec& component, const WriteContext& ctx) {
    TomlValue table = Table();
    table["name"] = component.name;
    table["type"] = std::string(ComponentTypeName(component.type));
    if (component.type != ComponentType::kBase) table["parent"] = component.parent;
    PutLocalFrame(table, component.local_frame, ctx);
    if (component.axis.has_value()) {
        table["axis"] = MakeAxis(*component.axis,
                                 component.type == ComponentType::kRotary, ctx);
    }
    if (component.frame_placement.has_value()) {
        table["frame"] = MakeFrame(component.local_frame, *component.frame_placement,
                                   ctx.length_scale);
    }
    if (component.spindle.has_value()) {
        table["spindle"] = MakeSpindle(*component.spindle);
    }
    if (component.geometries.empty()) return table;
    TomlValue geometries = TableArrayValue();
    for (std::size_t i = 0; i < component.geometries.size(); ++i) {
        const std::string context =
                "component[" + component.name + "].geometry[" + std::to_string(i) + "]";
        geometries.push_back(MakeGeometry(component.geometries[i], context, ctx));
    }
    table["geometry"] = geometries;
    return table;
}



/**
 * ---- セクション ----
 */

/// @brief `[machine]`を書き出す. 空の任意キーは省略する
/// @note `date`は日付として解釈できればTOML形式の日付として出力する
TomlValue MakeMachineMeta(const MachineDefinition& definition) {
    TomlValue table = Table();
    table["name"] = definition.name;
    if (!definition.description.empty()) table["description"] = definition.description;
    if (!definition.author.empty()) table["author"] = definition.author;
    if (!definition.date.empty()) PutDateTime(table, "date", definition.date);
    return table;
}

/// @brief `[kinematics]`を書き出す
TomlValue MakeKinematics(const BranchPolicy branch) {
    TomlValue table = Table();
    table["branch"] = std::string(BranchPolicyName(branch));
    return table;
}

/// @brief 簡易形`pairs`で書けるペアか (subtreeなし・有効・既定クリアランス)
bool IsSimplePair(const CollisionPair& pair, const double default_clearance) {
    return !pair.subtree[0] && !pair.subtree[1] && pair.enabled
           && std::abs(pair.clearance - default_clearance) <= kClearanceTolerance;
}

/// @brief `[[collision.pair]]`の1要素を書き出す. 既定値は省略する
TomlValue MakeDetailedPair(const CollisionPair& pair, const double default_clearance,
                           const double length_scale) {
    TomlValue table = Table();
    table["targets"] = NamePair(pair.targets);
    if (pair.subtree[0] || pair.subtree[1]) table["subtree"] = BoolPair(pair.subtree);
    if (std::abs(pair.clearance - default_clearance) > kClearanceTolerance) {
        table["clearance"] = Real(pair.clearance / length_scale);
    }
    if (!pair.enabled) table["enabled"] = false;
    return table;
}

/// @brief `[collision]`を書き出す
/// @note 簡易形で書けるペアは`pairs`に、それ以外は`[[collision.pair]]`に分ける
///       (読込側もこの順で正規化する)
TomlValue MakeCollision(const CollisionSettings& settings, const WriteContext& ctx) {
    TomlValue table = Table();
    table["mode"] = std::string(CollisionModeName(settings.mode));
    table["default_clearance"] = Real(settings.default_clearance / ctx.length_scale);
    if (!settings.exclude.empty()) {
        TomlValue exclude = MultilineArray();
        for (const auto& pair : settings.exclude) exclude.push_back(NamePair(pair));
        table["exclude"] = exclude;
    }
    TomlValue simple = MultilineArray();
    TomlValue detailed = TableArrayValue();
    for (const CollisionPair& pair : settings.pairs) {
        if (IsSimplePair(pair, settings.default_clearance)) {
            simple.push_back(NamePair(pair.targets));
        } else {
            detailed.push_back(MakeDetailedPair(pair, settings.default_clearance,
                                                ctx.length_scale));
        }
    }
    if (!simple.as_array().empty()) table["pairs"] = simple;
    if (!detailed.as_array().empty()) table["pair"] = detailed;
    return table;
}

}  // namespace



std::string FormatMachineDefinition(const MachineDefinition& definition,
                                    const std::filesystem::path& base_dir) {
    const WriteContext ctx = MakeContext(definition.source_dir, definition.units,
                                         base_dir);
    TomlValue root = Table();
    root["format"] = MakeFormat(kMachineFormatName, kMachineFormatVersion);
    root["machine"] = MakeMachineMeta(definition);
    root["units"] = MakeUnits(definition.units);
    TomlValue components = TableArrayValue();
    for (const ComponentSpec& component : definition.components) {
        if (IsImplicitBase(component)) continue;
        components.push_back(MakeComponent(component, ctx));
    }
    root["component"] = components;
    root["kinematics"] = MakeKinematics(definition.branch);
    if (definition.collision.has_value()) {
        root["collision"] = MakeCollision(*definition.collision, ctx);
    }
    return kHeaderComment + toml::format(root);
}

}  // namespace igesio::extensions::machines::detail
