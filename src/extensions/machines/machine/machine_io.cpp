/**
 * @file extensions/machines/machine/machine_io.cpp
 * @brief 機械定義 (TOML) の読込・検証と書き出し
 * @author Yayoi Habami
 * @date 2026-09-08
 * @copyright 2026 Yayoi Habami
 * @note 読込順・検証項目・文言は仕様に基づく. 未知キーの検出は行わない.
 *       書き出しの本体は`machine_writing.h`.
 */
#include "igesio/extensions/machines/machine/machine_io.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include <Eigen/SVD>
#include <toml.hpp>

#include "igesio/common/errors.h"
#include "igesio/extensions/machines/core/rotation.h"
#include "igesio/extensions/machines/core/units.h"
#include "extensions/machines/machine/machine_writing.h"
#include "extensions/machines/machine/toml_reading.h"

namespace igesio::extensions::machines {

namespace {

using detail::TomlValue;
using detail::Fail;
using detail::Find;
using detail::LineOf;
using detail::Warn;

/// @brief ルートコンポーネントの予約名
constexpr const char* kBaseName = "base";
/// @brief 直進軸の実効方向のrank判定に用いる特異値の許容誤差
constexpr double kRankTolerance = 1e-6;
/// @brief `initial`と`limits`の範囲検査の許容誤差 (mmまたはrad)
constexpr double kLimitTolerance = 1e-9;

/// @brief `[[component]]`の表 (名前→テーブル). 暗黙のbaseはテーブルを持たない
struct ComponentTables {
    /// @brief 名前のファイル順 (暗黙のbaseは末尾)
    std::vector<std::string> file_order;
    /// @brief 名前→テーブル (暗黙のbaseは`nullptr`)
    std::unordered_map<std::string, const TomlValue*> by_name;
    /// @brief 名前→親の名前 (baseは空)
    std::unordered_map<std::string, std::string> parent;
    /// @brief 名前→子の名前列 (ファイル順)
    std::unordered_map<std::string, std::vector<std::string>> children;

    /// @brief 名前が存在するか
    bool Has(const std::string& name) const { return by_name.count(name) != 0; }
};

/// @brief 構造検証の結果
struct Structure {
    /// @brief 名前→種別
    std::unordered_map<std::string, ComponentType> types;
    /// @brief tool_mountの名前
    std::string tool_mount;
    /// @brief work_mountの名前
    std::string work_mount;
};

/// @brief 干渉設定の予約名 (`tool`・`work`) か
bool IsReservedTarget(const std::string& name) {
    return name == "tool" || name == "work";
}

/// @brief コンポーネントの文脈文字列 `component[{name}]`
std::string ContextOf(const std::string& name) {
    return "component[" + name + "]";
}

/// @brief テーブルの行番号 (暗黙のbaseは0)
int LineOfTable(const TomlValue* table) {
    return table == nullptr ? 0 : LineOf(*table);
}

/// @brief 名前列を`[a, b]`の形にする
std::string JoinNames(const std::vector<std::string>& names) {
    std::string text = "[";
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i > 0) text += ", ";
        text += names[i];
    }
    return text + "]";
}

/// @brief `[a, b]`形式のバージョン表記
std::string FormatVersion(const std::array<int, 2>& version) {
    return "[" + std::to_string(version[0]) + ", " + std::to_string(version[1]) + "]";
}

/// @brief 文字列2要素の配列を読む (干渉ペアの対象)
/// @throw igesio::DataFormatError 形式が異なる場合 (`message`を用いる)
std::array<std::string, 2> ReadNamePair(const TomlValue& value,
                                        const std::string& context,
                                        const std::string& message) {
    if (!value.is_array() || value.as_array().size() != 2
        || !value.as_array()[0].is_string() || !value.as_array()[1].is_string()) {
        Fail(context, message, LineOf(value));
    }
    return {value.as_array()[0].as_string(), value.as_array()[1].as_string()};
}



/**
 * ---- TOML本体の解析 ----
 */

/// @brief toml11の例外を`DataFormatError`に変換してファイルを解析する
TomlValue ParseTomlFile(const std::filesystem::path& path,
                        const std::string& source_name) {
    try {
        return toml::parse(path.string());
    } catch (const toml::exception& e) {
        throw igesio::DataFormatError(
                source_name + ": TOML parse error: " + e.what());
    }
}

/// @brief toml11の例外を`DataFormatError`に変換して文字列を解析する
TomlValue ParseTomlString(const std::string& text, const std::string& source_name) {
    try {
        return toml::parse_str(text);
    } catch (const toml::exception& e) {
        throw igesio::DataFormatError(
                source_name + ": TOML parse error: " + e.what());
    }
}



/**
 * ---- セクション ----
 */

/// @brief `[format]`を検証する
/// @return フォーマットバージョン
std::array<int, 2> ReadFormat(const TomlValue& root,
                              std::vector<Diagnostic>& warnings) {
    const TomlValue* format = Find(root, "format");
    const TomlValue* name = format == nullptr ? nullptr : Find(*format, "name");
    if (name == nullptr || !name->is_string()
        || name->as_string() != kMachineFormatName) {
        Fail("", "[format].name must be \"" + std::string(kMachineFormatName)
                 + "\"", LineOfTable(format));
    }
    const TomlValue* version = Find(*format, "version");
    const bool well_formed =
            version != nullptr && version->is_array()
            && version->as_array().size() == 2
            && version->as_array()[0].is_integer()
            && version->as_array()[1].is_integer();
    if (!well_formed) {
        Fail("", "[format].version must be two integers [major, minor]: "
                 + (version == nullptr ? std::string("(missing)")
                                       : detail::FormatValue(*version)),
             LineOfTable(format));
    }
    const std::array<int, 2> result = {
            static_cast<int>(version->as_array()[0].as_integer()),
            static_cast<int>(version->as_array()[1].as_integer())};
    if (result[0] != kMachineFormatVersion[0]) {
        Fail("", "unsupported format version: " + FormatVersion(result)
                 + " (supported major "
                 + std::to_string(kMachineFormatVersion[0]) + ")",
             LineOf(*version));
    }
    if (result[1] > kMachineFormatVersion[1]) {
        Warn(warnings, "[format]",
             "format version " + FormatVersion(result)
             + " has a newer minor than the supported "
             + FormatVersion(kMachineFormatVersion)
             + " (unsupported keys are ignored)",
             LineOf(*version));
    }
    return result;
}

/// @brief `[machine]`を読み込む
void ReadMachineMeta(const TomlValue& root, MachineDefinition& definition) {
    const TomlValue* machine = Find(root, "machine");
    if (machine == nullptr) Fail("", "[machine].name is missing");
    detail::EnsureTable(*machine, "[machine] is not a table");
    const TomlValue* name = Find(*machine, "name");
    if (name == nullptr || !name->is_string() || name->as_string().empty()) {
        Fail("", "[machine].name is missing", LineOf(*machine));
    }
    definition.name = name->as_string();
    definition.description =
            detail::OptionalString(*machine, "description", "[machine]").value_or("");
    definition.author =
            detail::OptionalString(*machine, "author", "[machine]").value_or("");
    if (const TomlValue* date = Find(*machine, "date"); date != nullptr) {
        if (date->is_string()) {
            definition.date = date->as_string();
        } else if (date->is_local_date()) {
            definition.date = toml::format(*date);
        } else {
            Fail("[machine]", "date is neither a date nor a string: "
                              + detail::FormatValue(*date), LineOf(*date));
        }
    }
}

/// @brief `[units]`を読み込む. 既定はmm・deg
UnitScales ReadUnits(const TomlValue& root) {
    const TomlValue* units = Find(root, "units");
    if (units == nullptr) {
        return MakeUnitScales(LengthUnit::kMillimeter, AngleUnit::kDegree);
    }
    detail::EnsureTable(*units, "[units] is not a table");
    const std::string length =
            detail::OptionalString(*units, "length", "[units]").value_or("mm");
    const std::string angle =
            detail::OptionalString(*units, "angle", "[units]").value_or("deg");
    const auto length_unit = ParseLengthUnit(length);
    if (!length_unit.has_value()) {
        Fail("", "invalid [units].length: " + length, LineOf(*units));
    }
    const auto angle_unit = ParseAngleUnit(angle);
    if (!angle_unit.has_value()) {
        Fail("", "invalid [units].angle: " + angle, LineOf(*units));
    }
    return MakeUnitScales(*length_unit, *angle_unit);
}

/// @brief `[kinematics]`を読み込む. 既定は`positive`
BranchPolicy ReadKinematics(const TomlValue& root) {
    const TomlValue* kinematics = Find(root, "kinematics");
    if (kinematics == nullptr) return BranchPolicy::kPositive;
    detail::EnsureTable(*kinematics, "[kinematics] is not a table");
    const std::string branch =
            detail::OptionalString(*kinematics, "branch", "[kinematics]")
                    .value_or("positive");
    const auto policy = ParseBranchPolicy(branch);
    if (!policy.has_value()) {
        Fail("", "invalid [kinematics].branch: " + branch,
             LineOf(*kinematics));
    }
    return *policy;
}



/**
 * ---- コンポーネント表 ----
 */

/// @brief `[[component]]`の各要素の`name`を読み込む
/// @throw igesio::DataFormatError 欠落・空・予約名・重複
std::string ReadComponentName(const TomlValue& table, const std::size_t index,
                              const ComponentTables& tables) {
    const std::string context = "component[" + std::to_string(index) + "]";
    detail::EnsureTable(table, context + ": not a table");
    const TomlValue* name = Find(table, "name");
    if (name == nullptr || !name->is_string() || name->as_string().empty()) {
        Fail(context, "name is missing", LineOf(table));
    }
    const std::string text = name->as_string();
    if (IsReservedTarget(text)) {
        Fail("", "reserved name cannot be used as a component name: " + text,
             LineOf(table));
    }
    if (tables.Has(text)) {
        Fail("", "duplicate component name: " + text, LineOf(table));
    }
    return text;
}

/// @brief 明示された`base`の制約 (`[[component]].base`) を検証する
void ValidateExplicitBase(const TomlValue& base) {
    const auto type = detail::OptionalString(base, "type", ContextOf(kBaseName));
    if (!type.has_value() || *type != "base") {
        Fail("", "root \"base\" must have type \"base\"", LineOf(base));
    }
    for (const char* key : {"parent", "axis", "frame", "spindle"}) {
        if (Find(base, key) != nullptr) {
            Fail("", std::string("\"base\" cannot have ") + key, LineOf(base));
        }
    }
}

/// @brief `[[component]]`の一覧を検証し、名前→テーブルの表を作る
ComponentTables CollectComponentTables(const TomlValue& root) {
    const TomlValue* raw = Find(root, "component");
    if (raw == nullptr || !raw->is_array() || raw->as_array().empty()) {
        Fail("", "no [[component]] is defined");
    }
    ComponentTables tables;
    const auto& array = raw->as_array();
    for (std::size_t i = 0; i < array.size(); ++i) {
        const std::string name = ReadComponentName(array[i], i, tables);
        tables.file_order.push_back(name);
        tables.by_name[name] = &array[i];
    }
    if (!tables.Has(kBaseName)) {
        tables.file_order.emplace_back(kBaseName);
        tables.by_name[kBaseName] = nullptr;
    }
    if (const TomlValue* base = tables.by_name[kBaseName]; base != nullptr) {
        ValidateExplicitBase(*base);
    }
    tables.parent[kBaseName] = "";
    for (const std::string& name : tables.file_order) {
        if (name == kBaseName) continue;
        const TomlValue& table = *tables.by_name[name];
        const auto type = detail::OptionalString(table, "type", ContextOf(name));
        if (type.has_value() && *type == "base") {
            Fail("", "type=\"base\" is reserved for the root \"base\": " + name,
                 LineOf(table));
        }
        const TomlValue* parent = Find(table, "parent");
        if (parent == nullptr) {
            Fail(name, "parent is missing (required except for base)",
                 LineOf(table));
        }
        if (!parent->is_string()) {
            Fail(name, "parent is not a string: " + detail::FormatValue(*parent),
                 LineOf(*parent));
        }
        if (!tables.Has(parent->as_string())) {
            Fail(name, "parent does not exist: " + parent->as_string(),
                 LineOf(*parent));
        }
        tables.parent[name] = parent->as_string();
        tables.children[parent->as_string()].push_back(name);
    }
    return tables;
}

/// @brief 親が先に現れる名前列を作る (baseから宣言順の深さ優先)
/// @throw igesio::DataFormatError baseから到達できない (閉路) コンポーネントがある場合
std::vector<std::string> TopologicalOrder(const ComponentTables& tables) {
    std::vector<std::string> order;
    std::vector<std::string> stack = {kBaseName};
    while (!stack.empty()) {
        const std::string name = stack.back();
        stack.pop_back();
        order.push_back(name);
        const auto it = tables.children.find(name);
        if (it == tables.children.end()) continue;
        for (auto child = it->second.rbegin(); child != it->second.rend(); ++child) {
            stack.push_back(*child);
        }
    }
    if (order.size() != tables.file_order.size()) {
        std::vector<std::string> rest;
        for (const std::string& name : tables.file_order) {
            if (std::find(order.begin(), order.end(), name) == order.end()) {
                rest.push_back(name);
            }
        }
        std::sort(rest.begin(), rest.end());
        Fail("", "component not in the tree (cycle or similar): "
                 + JoinNames(rest));
    }
    return order;
}

/// @brief 末端からbaseへ向かう経路上の名前列 (baseを含む)
std::vector<std::string> ChainNames(const ComponentTables& tables,
                                    const std::string& leaf) {
    std::vector<std::string> path;
    std::string current = leaf;
    while (current != kBaseName) {
        path.push_back(current);
        current = tables.parent.at(current);
    }
    path.emplace_back(kBaseName);
    return path;
}

/// @brief コンポーネントの`type`を読む
ComponentType ReadComponentType(const std::string& name, const TomlValue* table) {
    if (table == nullptr) return ComponentType::kBase;
    const auto text = detail::OptionalString(*table, "type", ContextOf(name));
    if (!text.has_value()) Fail(name, "type is missing", LineOf(*table));
    const auto type = ParseComponentType(*text);
    if (!type.has_value()) Fail(name, "unknown type: " + *text, LineOf(*table));
    return *type;
}

/// @brief typeとサブテーブル・マウント・spindleの整合性を検証する
Structure ValidateStructure(const ComponentTables& tables) {
    Structure structure;
    std::vector<std::string> tool_mounts, work_mounts, spindles;
    for (const std::string& name : tables.file_order) {
        const TomlValue* table = tables.by_name.at(name);
        const ComponentType type = ReadComponentType(name, table);
        structure.types[name] = type;
        if (table == nullptr) continue;
        const bool is_axis = type == ComponentType::kLinear
                             || type == ComponentType::kRotary;
        const bool is_mount = type == ComponentType::kToolMount
                              || type == ComponentType::kWorkMount;
        const std::string type_name(ComponentTypeName(type));
        if (is_axis != (Find(*table, "axis") != nullptr)) {
            Fail(name, "type=" + type_name
                       + " is inconsistent with the presence of axis",
                 LineOf(*table));
        }
        if (is_mount != (Find(*table, "frame") != nullptr)) {
            Fail(name, "type=" + type_name
                       + " is inconsistent with the presence of frame",
                 LineOf(*table));
        }
        if (Find(*table, "spindle") != nullptr && type != ComponentType::kSpindle) {
            Fail(name, "the spindle table is valid only for type=spindle",
                 LineOf(*table));
        }
        if (type == ComponentType::kSpindle) spindles.push_back(name);
        if (type == ComponentType::kToolMount) tool_mounts.push_back(name);
        if (type == ComponentType::kWorkMount) work_mounts.push_back(name);
    }
    if (tool_mounts.size() != 1) {
        Fail("", "exactly one tool_mount is required (found "
                 + std::to_string(tool_mounts.size()) + ")");
    }
    if (work_mounts.size() != 1) {
        Fail("", "exactly one work_mount is required (found "
                 + std::to_string(work_mounts.size()) + ")");
    }
    structure.tool_mount = tool_mounts[0];
    structure.work_mount = work_mounts[0];
    for (const std::string& name : tables.file_order) {
        if (name == kBaseName) continue;
        const std::string& parent = tables.parent.at(name);
        if (parent == structure.tool_mount || parent == structure.work_mount) {
            Fail(name, "mount (" + parent + ") cannot have children",
                 LineOfTable(tables.by_name.at(name)));
        }
    }
    if (spindles.size() > 1) {
        Fail("", "at most one spindle is allowed (found "
                 + std::to_string(spindles.size()) + ")");
    }
    if (!spindles.empty()) {
        const std::vector<std::string> chain = ChainNames(tables, structure.tool_mount);
        if (std::find(chain.begin(), chain.end(), spindles[0]) == chain.end()) {
            Fail("", "spindle (" + spindles[0]
                     + ") must be an ancestor of the tool_mount");
        }
    }
    return structure;
}



/**
 * ---- コンポーネントの要素 ----
 */

/// @brief `component.local_frame`からローカル座標系の配置C_cを作る
igesio::Matrix4d ReadLocalFrame(const TomlValue& table, const std::string& name,
                                const UnitScales& scales) {
    const TomlValue* frame = Find(table, "local_frame");
    if (frame == nullptr) return igesio::Matrix4d::Identity();
    const std::string context = ContextOf(name) + ".local_frame";
    detail::EnsureTable(*frame, context + ": not a table");
    const igesio::Vector3d origin =
            detail::ReadVec3Or(*frame, "origin", igesio::Vector3d::Zero(), context)
            * scales.length;
    return MakeRigid(detail::ReadRotation(*frame, context, scales.angle), origin);
}

/// @brief マウントの`frame` (`component.frame`) からゼロポーズ機械座標での配置Hを作る
igesio::Matrix4d ReadFrame(const TomlValue& frame, const igesio::Matrix4d& local_frame,
                           const UnitScales& scales, const std::string& context) {
    detail::EnsureTable(frame, context + ": not a table");
    const igesio::Matrix3d r_lf = RotationPart(local_frame);
    const igesio::Vector3d origin =
            detail::ReadVec3(frame, "origin", context) * scales.length;
    const std::vector<std::string> rotation_keys = detail::PresentKeys(
            frame, {detail::kRotationKeys[0], detail::kRotationKeys[1],
                    detail::kRotationKeys[2]});
    const std::vector<std::string> axis_keys =
            detail::PresentKeys(frame, {"z_axis", "x_axis"});
    if (!rotation_keys.empty() && !axis_keys.empty()) {
        Fail(context, "rotation form (" + rotation_keys[0]
                      + ") and z_axis/x_axis are exclusive",
             LineOf(frame));
    }
    igesio::Matrix3d rotation;
    if (!rotation_keys.empty()) {
        rotation = r_lf * detail::ReadRotation(frame, context, scales.angle);
    } else {
        const TomlValue* z_axis = Find(frame, "z_axis");
        const igesio::Vector3d z =
                r_lf * (z_axis == nullptr ? igesio::Vector3d::UnitZ()
                                          : detail::AsUnitVec3(*z_axis, context + ".z_axis"));
        igesio::Vector3d x = r_lf * igesio::Vector3d::UnitX();
        if (const TomlValue* x_axis = Find(frame, "x_axis"); x_axis != nullptr) {
            x = r_lf * detail::AsUnitVec3(*x_axis, context + ".x_axis");
            if (std::abs(x.dot(z)) > kUnitVectorTolerance) {
                Fail(context, "x_axis is not orthogonal to z_axis",
                     LineOf(*x_axis));
            }
        }
        x -= x.dot(z) * z;
        if (x.norm() < kDegenerateTolerance) {
            Fail(context, "cannot determine the x axis (z_axis is parallel to "
                          "the local x axis; specify x_axis explicitly)",
                 LineOf(frame));
        }
        x.normalize();
        rotation.col(0) = x;
        rotation.col(1) = z.cross(x);
        rotation.col(2) = z;
    }
    return MakeRigid(rotation, r_lf * origin + TranslationPart(local_frame));
}

/// @brief `component.axis.dynamics`を読み込み、単位を換算する
/// @note 送り3種はファイル上で毎分なので、軸種別スケールと1/60を合成した
///       1つの係数を掛ける (書き出し側は同じ係数で割り、往復を対称にする).
///       `accel`・`decel`は毎秒基準、`resolution`は長さ/角度のみで軸種別スケールのみ
AxisDynamics ReadDynamics(const TomlValue& axis, const double scale,
                          const std::string& context) {
    AxisDynamics dynamics;
    const TomlValue* table = Find(axis, "dynamics");
    if (table == nullptr) return dynamics;
    const std::string ctx = context + ".dynamics";
    detail::EnsureTable(*table, ctx + ": not a table");
    const double feed_scale = scale / kSecondsPerMinute;
    const std::array<std::tuple<const char*, std::optional<double>*, double>, 6> keys = {{
            {"rapid_feed", &dynamics.rapid_feed, feed_scale},
            {"max_feed", &dynamics.max_feed, feed_scale},
            {"min_feed", &dynamics.min_feed, feed_scale},
            {"accel", &dynamics.accel, scale},
            {"decel", &dynamics.decel, scale},
            {"resolution", &dynamics.resolution, scale}}};
    for (const auto& [key, target, factor] : keys) {
        const TomlValue* value = Find(*table, key);
        if (value == nullptr) continue;
        const bool allow_zero = std::string(key) == "min_feed";
        *target = detail::AsPositive(*value, ctx + "." + key, allow_zero) * factor;
    }
    if (dynamics.min_feed.has_value() && dynamics.max_feed.has_value()
        && *dynamics.min_feed > *dynamics.max_feed) {
        Fail(ctx, "min_feed exceeds max_feed: "
                  + detail::FormatValue(*Find(*table, "min_feed")) + " > "
                  + detail::FormatValue(*Find(*table, "max_feed")), LineOf(*table));
    }
    return dynamics;
}

/// @brief `limits`を読み単位換算する
std::array<double, 2> ReadLimits(const TomlValue& raw, const double scale,
                                 const std::string& context) {
    if (!raw.is_array() || raw.as_array().size() != 2) {
        Fail(context, "limits must be two real numbers", LineOf(raw));
    }
    const std::array<double, 2> limits = {
            detail::AsReal(raw.as_array()[0], context + ".limits[0]") * scale,
            detail::AsReal(raw.as_array()[1], context + ".limits[1]") * scale};
    if (!(limits[0] < limits[1])) {
        Fail(context, "limits is not min < max: " + detail::FormatValue(raw),
             LineOf(raw));
    }
    return limits;
}

/// @brief `initial`を読み、`limits`との整合を検証する
double ReadInitial(const TomlValue& axis, const double scale,
                   const std::optional<std::array<double, 2>>& limits,
                   const std::string& context) {
    const TomlValue* raw = Find(axis, "initial");
    const double initial =
            raw == nullptr ? 0.0 : detail::AsReal(*raw, context + ".initial") * scale;
    if (!limits.has_value()) return initial;
    const bool inside = (*limits)[0] - kLimitTolerance <= initial
                        && initial <= (*limits)[1] + kLimitTolerance;
    if (inside) return initial;
    if (raw != nullptr) {
        Fail(context, "initial is outside limits: " + detail::FormatValue(*raw),
             LineOf(*raw));
    }
    Fail(context,
         "initial cannot be omitted because limits does not contain 0",
         LineOf(axis));
}

/// @brief `component.axis`を読み込み、単位を換算する
AxisSpec ReadAxis(const TomlValue& table, const std::string& name,
                  const bool is_rotary, const UnitScales& scales) {
    const TomlValue& axis = *Find(table, "axis");
    const std::string context = ContextOf(name) + ".axis";
    detail::EnsureTable(axis, context + ": not a table");
    AxisSpec spec;
    spec.register_name = detail::RequireString(axis, "register", context);
    const TomlValue* direction = Find(axis, "direction");
    if (direction == nullptr) {
        Fail(context + ".direction",
             "not an array of 3 real numbers: (missing)", LineOf(axis));
    }
    spec.direction = detail::AsUnitVec3(*direction, context + ".direction");
    if (is_rotary) {
        spec.point = detail::ReadVec3(axis, "point", context) * scales.length;
    } else if (Find(axis, "point") != nullptr) {
        Fail(context, "point can be specified only for rotary", LineOf(axis));
    }
    const double scale = is_rotary ? scales.angle : scales.length;
    spec.unlimited = detail::OptionalBool(axis, "unlimited", false, context);
    const TomlValue* limits = Find(axis, "limits");
    if ((limits != nullptr) == spec.unlimited) {
        Fail(context, "specify exactly one of limits and unlimited = true",
             LineOf(axis));
    }
    if (limits != nullptr) spec.limits = ReadLimits(*limits, scale, context);
    if (const TomlValue* wrap = Find(axis, "wrap_start"); wrap != nullptr) {
        if (!(is_rotary && spec.unlimited)) {
            Fail(context,
                 "wrap_start is allowed only for rotary with unlimited = true",
                 LineOf(*wrap));
        }
        spec.wrap_start = detail::AsReal(*wrap, context + ".wrap_start") * scales.angle;
    }
    spec.initial = ReadInitial(axis, scale, spec.limits, context);
    spec.dynamics = ReadDynamics(axis, scale, context);
    return spec;
}

/// @brief `component.spindle`を読む
/// @return テーブルがあれば`SpindleSpec` (各キーは省略可). 無ければ`std::nullopt`
/// @note `max_rpm`の単位は換算しない ([min⁻¹] のまま保持)
std::optional<SpindleSpec> ReadSpindle(const TomlValue& table,
                                       const std::string& name) {
    const TomlValue* spindle = Find(table, "spindle");
    if (spindle == nullptr) return std::nullopt;
    const std::string context = ContextOf(name) + ".spindle";
    detail::EnsureTable(*spindle, context + ": not a table");
    SpindleSpec spec;
    if (const TomlValue* max_rpm = Find(*spindle, "max_rpm"); max_rpm != nullptr) {
        spec.max_rpm = detail::AsPositive(*max_rpm, context + ".max_rpm");
    }
    return spec;
}

/// @brief `[[component.geometry]]`の列を読む
void ReadGeometries(const TomlValue& table, ComponentSpec& spec,
                    const detail::GeometryContext& ctx,
                    std::vector<Diagnostic>& warnings, detail::PathIssues& issues) {
    const TomlValue* geometries = Find(table, "geometry");
    if (geometries == nullptr) return;
    if (!geometries->is_array()) {
        Fail(ContextOf(spec.name), "geometry is not an array",
             LineOf(*geometries));
    }
    const auto& array = geometries->as_array();
    for (std::size_t i = 0; i < array.size(); ++i) {
        const std::string context =
                ContextOf(spec.name) + ".geometry[" + std::to_string(i) + "]";
        auto geometry = detail::ReadGeometry(array[i], context, ctx, warnings, issues);
        if (geometry.has_value()) spec.geometries.push_back(std::move(*geometry));
    }
}

/// @brief コンポーネント本体を読む
ComponentSpec ReadComponent(
        const std::string& name, const ComponentTables& tables,
        const Structure& structure, const UnitScales& scales,
        const std::filesystem::path& base_dir,
        std::vector<Diagnostic>& warnings, detail::PathIssues& issues) {
    ComponentSpec spec;
    spec.name = name;
    spec.parent = tables.parent.at(name);
    spec.type = structure.types.at(name);
    const TomlValue* table = tables.by_name.at(name);
    if (table == nullptr) return spec;   // 暗黙のbase
    spec.line = LineOf(*table);
    spec.local_frame = ReadLocalFrame(*table, name, scales);
    spec.spindle = ReadSpindle(*table, name);
    if (spec.type == ComponentType::kLinear || spec.type == ComponentType::kRotary) {
        spec.axis = ReadAxis(*table, name, spec.type == ComponentType::kRotary, scales);
    }
    if (spec.type == ComponentType::kToolMount || spec.type == ComponentType::kWorkMount) {
        spec.frame_placement = ReadFrame(*Find(*table, "frame"), spec.local_frame,
                                         scales, ContextOf(name) + ".frame");
    }
    ReadGeometries(*table, spec,
                   detail::GeometryContext{base_dir, scales, spec.local_frame},
                   warnings, issues);
    return spec;
}



/**
 * ---- チェーン検証 ----
 */

/// @brief 直進軸の実効方向が3次元を張るか (特異値がrank許容誤差を超える数が3)
bool SpansThreeDimensions(const std::map<std::string, igesio::Vector3d>& linears) {
    if (linears.empty()) return false;
    igesio::MatrixXd matrix(3, static_cast<Eigen::Index>(linears.size()));
    Eigen::Index column = 0;
    for (const auto& [register_name, direction] : linears) {
        matrix.col(column++) = direction;
    }
    const Eigen::JacobiSVD<igesio::MatrixXd> svd(matrix);
    return (svd.singularValues().array() > kRankTolerance).count() >= 3;
}

/// @brief チェーン上の軸構成を検証する
void ValidateChains(const std::map<std::string, ComponentSpec>& specs,
                    const ComponentTables& tables, const Structure& structure,
                    std::vector<Diagnostic>& warnings) {
    const std::vector<std::string> tool_chain = ChainNames(tables, structure.tool_mount);
    const std::vector<std::string> work_chain = ChainNames(tables, structure.work_mount);
    const std::set<std::string> tool_set(tool_chain.begin(), tool_chain.end());
    std::vector<std::string> common;
    for (const std::string& name : work_chain) {
        if (name != kBaseName && tool_set.count(name) != 0) common.push_back(name);
    }
    std::sort(common.begin(), common.end());
    if (!common.empty()) {
        Warn(warnings, "[[component]]",
             "component common to both chains (moving axes cancel out in "
             "relative motion): " + JoinNames(common));
    }
    std::set<std::string> members(tool_chain.begin(), tool_chain.end());
    members.insert(work_chain.begin(), work_chain.end());
    std::map<std::string, igesio::Vector3d> linears;
    int rotary_count = 0;
    for (const std::string& name : members) {
        const ComponentSpec& spec = specs.at(name);
        if (!spec.axis.has_value()) continue;
        if (spec.type == ComponentType::kLinear) {
            linears[spec.axis->register_name] =
                    RotationPart(spec.local_frame) * spec.axis->direction;
        } else {
            ++rotary_count;
        }
    }
    if (rotary_count > 2) {
        Fail("", "more than 2 rotary axes on the chain: "
                 + std::to_string(rotary_count));
    }
    if (!SpansThreeDimensions(linears)) {
        Fail("", "effective directions of the linear axes on the chain do "
                 "not span 3 dimensions");
    }
    if (linears.count("X") && linears.count("Y") && linears.count("Z")) {
        igesio::Matrix3d xyz;
        xyz.col(0) = linears.at("X");
        xyz.col(1) = linears.at("Y");
        xyz.col(2) = linears.at("Z");
        const double det = xyz.determinant();
        if (det <= 0.0) {
            Fail("", "effective directions of linear axes X, Y and Z are "
                     "not right-handed (determinant "
                     + detail::FormatReal(det) + ")");
        }
    } else {
        Warn(warnings, "[[component]]",
             "right-handedness check skipped: the three linear axes with "
             "register names X, Y and Z are not all present");
    }
}



/**
 * ---- 干渉チェック関連の設定 ----
 */

/// @brief 干渉ペアの対象1つをコンポーネント集合へ展開する
/// @note 予約名は仮想メンバ`@tool`・`@work`で表し、グループにマウントが
///       含まれる場合も対応する仮想メンバを加える
std::set<std::string> CollisionGroup(const std::string& target, const bool subtree,
                                     const ComponentTables& tables,
                                     const Structure& structure) {
    if (target == "tool") return {"@tool"};
    if (target == "work") return {"@work"};
    std::set<std::string> group = {target};
    if (subtree) {
        std::vector<std::string> stack = {target};
        while (!stack.empty()) {
            const std::string name = stack.back();
            stack.pop_back();
            const auto it = tables.children.find(name);
            if (it == tables.children.end()) continue;
            for (const std::string& child : it->second) {
                group.insert(child);
                stack.push_back(child);
            }
        }
    }
    if (group.count(structure.tool_mount)) group.insert("@tool");
    if (group.count(structure.work_mount)) group.insert("@work");
    return group;
}

/// @brief 干渉ペアの名前存在・重複・グループ交差を検証する
void ValidateCollisionPairs(const std::vector<CollisionPair>& pairs,
                            const ComponentTables& tables,
                            const Structure& structure) {
    std::set<std::set<std::pair<std::string, bool>>> seen;
    for (const CollisionPair& pair : pairs) {
        std::array<std::set<std::string>, 2> groups;
        for (std::size_t k = 0; k < 2; ++k) {
            const std::string& target = pair.targets[k];
            if (!tables.Has(target) && !IsReservedTarget(target)) {
                Fail("", "collision pair target does not exist: " + target);
            }
            if (IsReservedTarget(target) && pair.subtree[k]) {
                Fail("", "subtree = true cannot be specified for the "
                         "reserved name " + target);
            }
            groups[k] = CollisionGroup(target, pair.subtree[k], tables, structure);
        }
        const std::vector<std::string> targets = {pair.targets[0], pair.targets[1]};
        const std::set<std::pair<std::string, bool>> key = {
                {pair.targets[0], pair.subtree[0]}, {pair.targets[1], pair.subtree[1]}};
        if (!seen.insert(key).second) {
            Fail("", "duplicate collision pair: " + JoinNames(targets));
        }
        std::vector<std::string> intersection;
        std::set_intersection(groups[0].begin(), groups[0].end(),
                              groups[1].begin(), groups[1].end(),
                              std::back_inserter(intersection));
        if (!intersection.empty()) {
            Fail("", "the two groups of the collision pair intersect: "
                     + JoinNames(targets)
                     + " common=" + JoinNames(intersection));
        }
    }
}

/// @brief `[[collision.pair]]`の1要素を読む
CollisionPair ReadCollisionPair(const TomlValue& value, const std::size_t index,
                                const double default_clearance,
                                const double length_scale) {
    const std::string context = "[[collision.pair]][" + std::to_string(index) + "]";
    detail::EnsureTable(value, context + ": not a table");
    CollisionPair pair;
    const TomlValue* targets = Find(value, "targets");
    if (targets == nullptr) {
        Fail(context, "targets is not two strings", LineOf(value));
    }
    pair.targets = ReadNamePair(*targets, context, "targets is not two strings");
    if (const TomlValue* subtree = Find(value, "subtree"); subtree != nullptr) {
        const bool well_formed =
                subtree->is_array() && subtree->as_array().size() == 2
                && subtree->as_array()[0].is_boolean()
                && subtree->as_array()[1].is_boolean();
        if (!well_formed) {
            Fail(context, "subtree is not two booleans", LineOf(*subtree));
        }
        pair.subtree = {subtree->as_array()[0].as_boolean(),
                        subtree->as_array()[1].as_boolean()};
    }
    const TomlValue* clearance = Find(value, "clearance");
    pair.clearance = clearance == nullptr
            ? default_clearance
            : detail::AsPositive(*clearance, context + ".clearance", true) * length_scale;
    pair.enabled = detail::OptionalBool(value, "enabled", true, context);
    return pair;
}

/// @brief `[collision].exclude`を読む
std::vector<std::array<std::string, 2>> ReadExclude(const TomlValue& collision,
                                                    const ComponentTables& tables) {
    std::vector<std::array<std::string, 2>> exclude;
    const TomlValue* raw = Find(collision, "exclude");
    if (raw == nullptr) return exclude;
    if (!raw->is_array()) {
        Fail("[collision].exclude", "not an array", LineOf(*raw));
    }
    const auto& array = raw->as_array();
    for (std::size_t i = 0; i < array.size(); ++i) {
        const std::string context = "[collision].exclude[" + std::to_string(i) + "]";
        const auto pair = ReadNamePair(array[i], context, "not two strings");
        for (const std::string& target : pair) {
            if (!tables.Has(target) && !IsReservedTarget(target)) {
                Fail(context, "target does not exist: " + target,
                     LineOf(array[i]));
            }
        }
        exclude.push_back(pair);
    }
    return exclude;
}

/// @brief `[collision]`を検証し、詳細形へ正規化して返す
std::optional<CollisionSettings> ReadCollision(
        const TomlValue& root, const ComponentTables& tables,
        const Structure& structure, const UnitScales& scales,
        std::vector<Diagnostic>& warnings) {
    const TomlValue* collision = Find(root, "collision");
    if (collision == nullptr) return std::nullopt;
    detail::EnsureTable(*collision, "[collision] is not a table");
    CollisionSettings settings;
    const std::string mode =
            detail::OptionalString(*collision, "mode", "[collision]").value_or("pairs");
    const auto parsed_mode = ParseCollisionMode(mode);
    if (!parsed_mode.has_value()) {
        Fail("", "invalid [collision].mode: " + mode, LineOf(*collision));
    }
    settings.mode = *parsed_mode;
    if (const TomlValue* value = Find(*collision, "default_clearance"); value != nullptr) {
        settings.default_clearance =
                detail::AsPositive(*value, "[collision].default_clearance", true)
                * scales.length;
    }
    settings.exclude = ReadExclude(*collision, tables);
    if (!settings.exclude.empty() && settings.mode == CollisionMode::kPairs) {
        Warn(warnings, "[collision]",
             "[collision].exclude is valid only when "
             "mode = \"all_except_adjacent\"",
             LineOf(*Find(*collision, "exclude")));
    }
    if (const TomlValue* pairs = Find(*collision, "pairs"); pairs != nullptr) {
        if (!pairs->is_array()) {
            Fail("[collision].pairs", "not an array", LineOf(*pairs));
        }
        const auto& array = pairs->as_array();
        for (std::size_t i = 0; i < array.size(); ++i) {
            const std::string context = "[collision].pairs[" + std::to_string(i) + "]";
            CollisionPair pair;
            pair.targets = ReadNamePair(array[i], context, "not two strings");
            pair.clearance = settings.default_clearance;
            settings.pairs.push_back(pair);
        }
    }
    if (const TomlValue* detailed = Find(*collision, "pair"); detailed != nullptr) {
        if (!detailed->is_array()) {
            Fail("[[collision.pair]]", "not an array", LineOf(*detailed));
        }
        const auto& array = detailed->as_array();
        for (std::size_t i = 0; i < array.size(); ++i) {
            settings.pairs.push_back(ReadCollisionPair(
                    array[i], i, settings.default_clearance, scales.length));
        }
    }
    ValidateCollisionPairs(settings.pairs, tables, structure);
    return settings;
}



/**
 * ---- 全コンポーネントの読み込み ----
 */

/// @brief 全コンポーネントを親先行順に読み、レジスタの一意性と絶対パスを検査する
std::map<std::string, ComponentSpec> ReadAllComponents(
        const ComponentTables& tables, const std::vector<std::string>& order,
        const Structure& structure, const UnitScales& scales,
        const std::filesystem::path& base_dir, std::vector<Diagnostic>& warnings) {
    std::map<std::string, ComponentSpec> specs;
    std::set<std::string> registers;
    detail::PathIssues issues;
    for (const std::string& name : order) {
        ComponentSpec spec = ReadComponent(name, tables, structure, scales, base_dir,
                                           warnings, issues);
        if (spec.axis.has_value() && !registers.insert(spec.axis->register_name).second) {
            Fail("", "duplicate register: " + spec.axis->register_name,
                 spec.line);
        }
        specs[name] = std::move(spec);
    }
    const int total = issues.absolute + issues.escape;
    if (total > 0) {
        Warn(warnings, "[[component]]",
             std::to_string(total)
             + " non-portable geometry path(s) (absolute: "
             + std::to_string(issues.absolute) + ", above base directory: "
             + std::to_string(issues.escape) + ")");
    }
    return specs;
}

/// @brief TOML全体を読み込み、検証済みの機械定義を作る
MachineDefinition ParseDocument(const TomlValue& root,
                                const std::filesystem::path& base_dir,
                                const std::string& source_name) {
    MachineDefinition definition;
    definition.source_dir = base_dir;
    definition.source_name = source_name;
    definition.format_version = ReadFormat(root, definition.warnings);
    ReadMachineMeta(root, definition);
    definition.units = ReadUnits(root);
    const ComponentTables tables = CollectComponentTables(root);
    const std::vector<std::string> order = TopologicalOrder(tables);
    const Structure structure = ValidateStructure(tables);
    std::map<std::string, ComponentSpec> specs = ReadAllComponents(
            tables, order, structure, definition.units, base_dir, definition.warnings);
    ValidateChains(specs, tables, structure, definition.warnings);
    definition.branch = ReadKinematics(root);
    definition.collision = ReadCollision(root, tables, structure, definition.units,
                                         definition.warnings);
    definition.components.reserve(tables.file_order.size());
    for (const std::string& name : tables.file_order) {
        definition.components.push_back(std::move(specs.at(name)));
    }
    return definition;
}

}  // namespace



MachineDefinition ReadMachineDefinition(const std::filesystem::path& path) {
    if (!std::filesystem::is_regular_file(path)) {
        throw igesio::FileOpenError(path.string());
    }
    const std::string source_name = path.filename().string();
    const TomlValue root = ParseTomlFile(path, source_name);
    return ParseDocument(root, path.parent_path(), source_name);
}

MachineDefinition ReadMachineDefinitionFromString(
        const std::string& toml, const std::filesystem::path& base_dir,
        const std::string& source_name) {
    const TomlValue root = ParseTomlString(toml, source_name);
    return ParseDocument(root, base_dir, source_name);
}

void WriteMachineDefinition(const MachineDefinition& definition,
                            const std::filesystem::path& path) {
    // 文字列化の例外 (invalid_argument) はファイルを作る前に出す
    const std::string text =
            detail::FormatMachineDefinition(definition, path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        throw igesio::FileOpenError("Failed to open machine definition file: "
                                    + path.string());
    }
    stream << text;
    if (!stream) {
        throw igesio::FileOpenError("Failed to write machine definition file: "
                                    + path.string());
    }
}

std::string WriteMachineDefinitionToString(
        const MachineDefinition& definition, const std::filesystem::path& base_dir) {
    return detail::FormatMachineDefinition(definition, base_dir);
}

}  // namespace igesio::extensions::machines
