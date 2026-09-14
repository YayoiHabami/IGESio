/**
 * @file extensions/machines/machine/toml_reading.cpp
 * @brief machines拡張のTOML読取ヘルパー (内部ヘッダ)
 * @author Yayoi Habami
 * @date 2026-09-08
 * @copyright 2026 Yayoi Habami
 */
#include "extensions/machines/machine/toml_reading.h"

#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/extensions/machines/core/formatting.h"
#include "igesio/extensions/machines/core/rotation.h"
#include "igesio/extensions/machines/core/tolerances.h"

namespace igesio::extensions::machines::detail {

namespace {

/// @brief プリミティブ形状を読む
/// @param geometry 形状テーブル (`primitive`を持つこと)
/// @param context 発生箇所
/// @param length_scale 長さの換算係数
/// @return 寸法換算済みのプリミティブ
/// @throw igesio::DataFormatError 未知のprimitive、寸法キーの欠落・非正値、
///        `unit`の指定
/// @note TOMLの`[[component.geometry]]`で`primitive`キーを指定した場合
PrimitiveSpec ReadPrimitive(const TomlValue& geometry, const std::string& context,
                            const double length_scale) {
    if (Find(geometry, "unit") != nullptr) {
        Fail(context, "unit cannot be specified for a primitive",
             LineOf(geometry));
    }
    const std::string kind = RequireString(geometry, "primitive", context);
    const std::optional<PrimitiveSpec::Kind> parsed = ParsePrimitiveKind(kind);
    if (!parsed.has_value()) {
        Fail(context, "unknown primitive: " + kind, LineOf(geometry));
    }
    PrimitiveSpec primitive;
    primitive.kind = *parsed;
    if (primitive.kind == PrimitiveSpec::Kind::kBox) {
        const TomlValue* size = Find(geometry, "size");
        if (size == nullptr) {
            Fail(context, "box requires size", LineOf(geometry));
        }
        const igesio::Vector3d raw = AsVec3(*size, context + ".size");
        if (!(raw.array() > 0.0).all()) {
            Fail(context + ".size",
                 "components are not all positive: " + FormatValue(*size),
                 LineOf(*size));
        }
        primitive.size = raw * length_scale;
        return primitive;
    }
    const TomlValue* radius = Find(geometry, "radius");
    const TomlValue* height = Find(geometry, "height");
    if (radius == nullptr || height == nullptr) {
        Fail(context, "cylinder requires radius and height", LineOf(geometry));
    }
    primitive.radius = AsPositive(*radius, context + ".radius") * length_scale;
    primitive.height = AsPositive(*height, context + ".height") * length_scale;
    return primitive;
}

/// @brief ファイル参照形式の`file`・`unit`を解釈する
/// @param geometry 形状テーブル (`file`を持つこと)
/// @param context 発生箇所
/// @param ctx 形状テーブルの解釈に必要な情報
/// @param warnings 警告の追記先
/// @param issues 絶対パスの集計先
/// @param spec 解決済みパス・生パス・単位係数の書き込み先
/// @return 読み込む形状なら`true`. 未対応形式でスキップするなら`false`
/// @throw igesio::DataFormatError 仕様違反
/// @note TOMLの`[[component.geometry]]`で`file`キーを指定した場合
bool ReadFileSource(const TomlValue& geometry, const std::string& context,
                    const GeometryContext& ctx, std::vector<Diagnostic>& warnings,
                    PathIssues& issues, GeometrySpec& spec) {
    for (const char* key : {"size", "radius", "height"}) {
        if (Find(geometry, key) != nullptr) {
            Fail(context,
                 std::string(key) + " is valid only for the primitive form",
                 LineOf(geometry));
        }
    }
    const TomlValue& file = *Find(geometry, "file");
    if (!file.is_string() || file.as_string().empty()) {
        Fail(context, "invalid file: " + FormatValue(file), LineOf(file));
    }
    spec.raw_path = file.as_string();
    CheckFilePath(spec.raw_path, context, issues);

    std::filesystem::path path(spec.raw_path);
    if (!IsAbsolutePathString(spec.raw_path)) {
        path = ctx.base_dir / path;
    }
    spec.source = path.lexically_normal();

    const GeometryFileFormat format = ClassifyGeometryFile(path);
    if (format == GeometryFileFormat::kIges ||
        format == GeometryFileFormat::kStep) {
        if (Find(geometry, "unit") != nullptr) {
            Fail(context, "unit cannot be specified for IGES/STEP",
                 LineOf(geometry));
        }
        if (format == GeometryFileFormat::kStep) {
            Warn(warnings, context,
                 "skipped because the format is unsupported: " + spec.raw_path,
                 LineOf(file));
            return false;
        }
        return true;
    }
    if (format == GeometryFileFormat::kUnknown) {
        Fail(context, "unsupported model format: " + spec.raw_path,
             LineOf(file));
    }
    const std::optional<std::string> unit =
            OptionalString(geometry, "unit", context);
    LengthUnit length_unit = ctx.scales.length_unit;
    if (unit.has_value()) {
        const auto parsed = ParseLengthUnit(*unit);
        if (!parsed.has_value()) {
            Fail(context, "unknown unit: " + *unit,
                 LineOf(*Find(geometry, "unit")));
        }
        length_unit = *parsed;
    }
    spec.file_unit_scale = LengthScale(length_unit);
    return true;
}

}  // namespace



/**
 * ---- TOMLファイルの読み込み ----
 */

TomlValue ParseTomlFile(const std::filesystem::path& path,
                        const std::string& source_name) {
    try {
        return toml::parse<toml::ordered_type_config>(path.string());
    } catch (const toml::exception& e) {
        throw igesio::DataFormatError(
                source_name + ": TOML parse error: " + e.what());
    }
}

TomlValue ParseTomlString(const std::string& text,
                          const std::string& source_name) {
    try {
        return toml::parse_str<toml::ordered_type_config>(text);
    } catch (const toml::exception& e) {
        throw igesio::DataFormatError(
                source_name + ": TOML parse error: " + e.what());
    }
}



/**
 * ---- 位置・診断 ----
 */

int LineOf(const TomlValue& value) {
    return static_cast<int>(value.location().first_line_number());
}

int LineOfTable(const TomlValue* table) {
    return table == nullptr ? 0 : LineOf(*table);
}

void Fail(const std::string& context, const std::string& message, const int line) {
    std::string text = context.empty() ? message : context + ": " + message;
    if (line > 0) text += " (line " + std::to_string(line) + ")";
    throw igesio::DataFormatError(text);
}

void Warn(std::vector<Diagnostic>& warnings, const std::string& context,
          const std::string& message, const int line) {
    warnings.push_back(Diagnostic{Severity::kWarning, context, message, line});
}

std::string FormatValue(const TomlValue& value) {
    return toml::format(value);
}

std::string FormatVersion(const std::array<int, 2>& version) {
    return "[" + std::to_string(version[0]) + ", " + std::to_string(version[1]) + "]";
}



/**
 * ---- 型検査つき取得 ----
 */

const TomlValue* Find(const TomlValue& table, const std::string& key) {
    if (!table.is_table()) return nullptr;
    const auto& entries = table.as_table();
    const auto it = entries.find(key);
    return it == entries.end() ? nullptr : &it->second;
}

void EnsureTable(const TomlValue& value, const std::string& message) {
    if (!value.is_table()) {
        throw igesio::DataFormatError(
                message + " (line " + std::to_string(LineOf(value)) + ")");
    }
}

std::string RequireString(const TomlValue& table, const std::string& key,
                          const std::string& context) {
    const TomlValue* value = Find(table, key);
    if (value == nullptr || !value->is_string() || value->as_string().empty()) {
        Fail(context, key + " is missing",
             value == nullptr ? LineOf(table) : LineOf(*value));
    }
    return value->as_string();
}

std::optional<std::string> OptionalString(const TomlValue& table,
                                          const std::string& key,
                                          const std::string& context) {
    const TomlValue* value = Find(table, key);
    if (value == nullptr) return std::nullopt;
    if (!value->is_string()) {
        Fail(context, key + " is not a string: " + FormatValue(*value),
             LineOf(*value));
    }
    return value->as_string();
}

bool OptionalBool(const TomlValue& table, const std::string& key,
                  const bool default_value, const std::string& context) {
    const TomlValue* value = Find(table, key);
    if (value == nullptr) return default_value;
    if (!value->is_boolean()) {
        Fail(context, key + " is not a boolean: " + FormatValue(*value),
             LineOf(*value));
    }
    return value->as_boolean();
}

std::vector<std::string> OptionalStringArray(const TomlValue& table,
                                             const std::string& key,
                                             const std::string& context) {
    std::vector<std::string> result;
    const TomlValue* value = Find(table, key);
    if (value == nullptr) return result;
    if (!value->is_array()) {
        Fail(context, key + " is not an array of strings: " + FormatValue(*value),
             LineOf(*value));
    }
    for (const TomlValue& element : value->as_array()) {
        if (!element.is_string()) {
            Fail(context,
                 key + " is not an array of strings: " + FormatValue(*value),
                 LineOf(*value));
        }
        result.push_back(element.as_string());
    }
    return result;
}

std::vector<std::string> PresentKeys(
        const TomlValue& table, const std::initializer_list<const char*> keys) {
    std::vector<std::string> present;
    for (const char* key : keys) {
        if (Find(table, key) != nullptr) present.emplace_back(key);
    }
    return present;
}

std::vector<const TomlValue*> TableArray(const TomlValue& root,
                                         const std::string& key,
                                         const std::string& context) {
    std::vector<const TomlValue*> tables;
    const TomlValue* value = Find(root, key);
    if (value == nullptr) return tables;
    if (!value->is_array()) {
        Fail(context, "not an array of tables", LineOf(*value));
    }
    for (const TomlValue& element : value->as_array()) {
        EnsureTable(element, context + ": not an array of tables");
        tables.push_back(&element);
    }
    return tables;
}



/**
 * ---- 実数互換 ----
 */

double AsReal(const TomlValue& value, const std::string& context) {
    if (value.is_integer()) return static_cast<double>(value.as_integer());
    if (value.is_floating()) return value.as_floating();
    Fail(context, "not a real number: " + FormatValue(value), LineOf(value));
}

double AsPositive(const TomlValue& value, const std::string& context,
                  const bool allow_zero) {
    const double real = AsReal(value, context);
    if (allow_zero) {
        if (real < 0.0) {
            Fail(context, "not >= 0: " + FormatValue(value), LineOf(value));
        }
    } else if (real <= 0.0) {
        Fail(context, "not positive: " + FormatValue(value), LineOf(value));
    }
    return real;
}

std::vector<double> AsRealArray(const TomlValue& value, const std::size_t size,
                                const std::string& context) {
    const std::string what = "not an array of " + std::to_string(size)
                             + " real numbers: " + FormatValue(value);
    if (!value.is_array() || value.as_array().size() != size) {
        Fail(context, what, LineOf(value));
    }
    std::vector<double> result;
    result.reserve(size);
    for (const TomlValue& element : value.as_array()) {
        if (!element.is_integer() && !element.is_floating()) {
            Fail(context, what, LineOf(value));
        }
        result.push_back(AsReal(element, context));
    }
    return result;
}

igesio::Vector3d AsVec3(const TomlValue& value, const std::string& context) {
    const std::vector<double> v = AsRealArray(value, 3, context);
    return igesio::Vector3d(v[0], v[1], v[2]);
}

igesio::Vector3d AsUnitVec3(const TomlValue& value, const std::string& context) {
    const igesio::Vector3d v = AsVec3(value, context);
    const double norm = v.norm();
    if (std::abs(norm - 1.0) > kUnitVectorTolerance) {
        Fail(context, "not a unit vector (norm " + FormatFixed(norm, 6) + ")",
             LineOf(value));
    }
    return v / norm;
}

double ReadReal(const TomlValue& table, const std::string& key,
                const std::string& context) {
    const TomlValue* value = Find(table, key);
    if (value == nullptr) Fail(context, key + " is missing", LineOf(table));
    return AsReal(*value, context + "." + key);
}

double ReadRealOr(const TomlValue& table, const std::string& key,
                  const double default_value, const std::string& context) {
    const TomlValue* value = Find(table, key);
    if (value == nullptr) return default_value;
    return AsReal(*value, context + "." + key);
}

igesio::Vector3d ReadVec3(const TomlValue& table, const std::string& key,
                          const std::string& context) {
    const TomlValue* value = Find(table, key);
    if (value == nullptr) {
        Fail(context + "." + key,
             "not an array of 3 real numbers: (missing)", LineOf(table));
    }
    return AsVec3(*value, context + "." + key);
}

igesio::Vector3d ReadVec3Or(const TomlValue& table, const std::string& key,
                            const igesio::Vector3d& default_value,
                            const std::string& context) {
    const TomlValue* value = Find(table, key);
    if (value == nullptr) return default_value;
    return AsVec3(*value, context + "." + key);
}



/**
 * ---- 整数 ----
 */

int AsInteger(const TomlValue& value, const std::string& context) {
    if (value.is_integer()) return static_cast<int>(value.as_integer());
    // 整数値の実数リテラル (`1.0`) は仕様の実数/整数互換により許容する
    if (value.is_floating()) {
        const double real = value.as_floating();
        if (std::isfinite(real) && std::floor(real) == real) {
            return static_cast<int>(real);
        }
    }
    Fail(context, "not an integer: " + FormatValue(value), LineOf(value));
}

int RequireInteger(const TomlValue& table, const std::string& key,
                   const std::string& context) {
    const TomlValue* value = Find(table, key);
    if (value == nullptr) Fail(context, key + " is missing", LineOf(table));
    return AsInteger(*value, context + "." + key);
}

std::optional<int> OptionalInteger(const TomlValue& table, const std::string& key,
                                   const std::string& context) {
    const TomlValue* value = Find(table, key);
    if (value == nullptr) return std::nullopt;
    return AsInteger(*value, context + "." + key);
}



/**
 * ---- 共通セクション ----
 */

std::array<int, 2> ReadFormat(
        const TomlValue& root, const std::string_view expected_name,
        const std::array<int, 2>& supported_version,
        std::vector<Diagnostic>& warnings) {
    const TomlValue* format = Find(root, "format");
    const TomlValue* name = format == nullptr ? nullptr : Find(*format, "name");
    if (name == nullptr || !name->is_string() || name->as_string() != expected_name) {
        Fail("", "[format].name must be \"" + std::string(expected_name) + "\"",
             LineOfTable(format));
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
                                       : FormatValue(*version)),
             LineOfTable(format));
    }
    const std::array<int, 2> result = {
            static_cast<int>(version->as_array()[0].as_integer()),
            static_cast<int>(version->as_array()[1].as_integer())};
    if (result[0] != supported_version[0]) {
        Fail("", "unsupported format version: " + FormatVersion(result)
                 + " (supported major " + std::to_string(supported_version[0]) + ")",
             LineOf(*version));
    }
    if (result[1] > supported_version[1]) {
        Warn(warnings, "[format]",
             "format version " + FormatVersion(result)
             + " has a newer minor than the supported "
             + FormatVersion(supported_version)
             + " (unsupported keys are ignored)",
             LineOf(*version));
    }
    return result;
}

UnitScales ReadUnits(const TomlValue& root) {
    const UnitScales defaults;
    const TomlValue* units = Find(root, "units");
    if (units == nullptr) return defaults;
    EnsureTable(*units, "[units] is not a table");
    const std::string length =
            OptionalString(*units, "length", "[units]")
                    .value_or(std::string(LengthUnitName(defaults.length_unit)));
    const std::string angle =
            OptionalString(*units, "angle", "[units]")
                    .value_or(std::string(AngleUnitName(defaults.angle_unit)));
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

std::optional<std::string> ReadDateTimeText(const TomlValue& table,
                                            const std::string& key,
                                            const std::string& context) {
    const TomlValue* value = Find(table, key);
    if (value == nullptr) return std::nullopt;
    if (value->is_string()) return value->as_string();
    if (value->is_local_date() || value->is_local_datetime()
        || value->is_offset_datetime()) {
        return toml::format(*value);
    }
    Fail(context, key + " is neither a date-time nor a string: " + FormatValue(*value),
         LineOf(*value));
}

OpaqueToml ToOpaque(const std::string& key, const TomlValue& value) {
    // キー付きの文書として整形する (テーブルは`[key]`、テーブル配列は`[[key]]`、
    // それ以外は`key = value`の形になる). キーの引用はtoml11に任せる
    TomlValue root((toml::ordered_table()));
    root.as_table_fmt().fmt = toml::table_format::multiline;
    root[key] = value;
    std::string text = toml::format(root);
    // テーブルの後に付く空行を落とし、末尾の改行を1つにそろえる
    while (text.size() >= 2 && text[text.size() - 1] == '\n'
           && text[text.size() - 2] == '\n') {
        text.pop_back();
    }
    return OpaqueToml(std::move(text));
}



/**
 * ---- 幾何要素 ----
 */

igesio::Matrix3d ReadRotation(const TomlValue& table, const std::string& context,
                              const double angle_scale) {
    const std::vector<std::string> keys =
            PresentKeys(table, {kRotationKeys[0], kRotationKeys[1], kRotationKeys[2]});
    if (keys.size() > 1) {
        std::string listed;
        for (const auto& key : keys) listed += (listed.empty() ? "" : ", ") + key;
        Fail(context, "multiple rotation forms at once: [" + listed + "]",
             LineOf(table));
    }
    if (keys.empty()) return igesio::Matrix3d::Identity();

    const TomlValue& value = *Find(table, keys[0]);
    const std::string ctx = context + "." + keys[0];
    try {
        if (keys[0] == "rotation") {
            EnsureTable(value,
                        ctx + ": not a table of x_axis, y_axis and z_axis");
            const std::array<const char*, 3> names = {"x_axis", "y_axis", "z_axis"};
            std::array<igesio::Vector3d, 3> columns;
            for (std::size_t i = 0; i < names.size(); ++i) {
                const TomlValue* column = Find(value, names[i]);
                if (column == nullptr) {
                    Fail(ctx, std::string(names[i]) + " is missing",
                         LineOf(value));
                }
                columns[i] = AsUnitVec3(*column, ctx + "." + names[i]);
            }
            return RotationFromColumns(
                    ColumnsSpec{columns[0], columns[1], columns[2]});
        }
        if (keys[0] == "rotation_axis_angle") {
            const TomlValue* axis = value.is_table() ? Find(value, "axis") : nullptr;
            const TomlValue* angle = value.is_table() ? Find(value, "angle") : nullptr;
            if (axis == nullptr || angle == nullptr) {
                Fail(ctx, "axis and angle are required", LineOf(value));
            }
            const igesio::Vector3d unit = AsUnitVec3(*axis, ctx + ".axis");
            const double angle_rad = AsReal(*angle, ctx + ".angle") * angle_scale;
            return RotationAboutAxis(unit, angle_rad);
        }
        return RotationFromEulerIjk(AsVec3(value, ctx) * angle_scale);
    } catch (const std::invalid_argument& e) {
        // 列の再正規化後の直交性・鏡映検査 (RotationFromColumns) の失敗
        Fail(ctx, e.what(), LineOf(value));
    }
}

std::optional<Color> ReadColor(const TomlValue& table, const std::string& context) {
    const std::optional<std::string> text = OptionalString(table, "color", context);
    if (!text.has_value()) return std::nullopt;
    const std::optional<Color> rgb = ParseHexColor(*text);
    if (!rgb.has_value()) {
        Fail(context + ".color", "not in \"#RRGGBB\" form: " + *text,
             LineOf(*Find(table, "color")));
    }
    return rgb;
}

bool IsAbsolutePathString(const std::string& raw) {
    if (raw.empty()) return false;
    if (raw[0] == '/') return true;
    return raw.size() >= 2 && raw[1] == ':'
           && std::isalpha(static_cast<unsigned char>(raw[0])) != 0;
}

void CheckFilePath(const std::string& raw, const std::string& context,
                   PathIssues& issues) {
    if (raw.find('\\') != std::string::npos) {
        Fail(context, "use only '/' as the path separator: " + raw);
    }
    if (IsAbsolutePathString(raw)) {
        ++issues.absolute;
        return;
    }
    const std::filesystem::path normalized =
            std::filesystem::path(raw).lexically_normal();
    if (normalized.begin() != normalized.end() && *normalized.begin() == "..") {
        ++issues.escape;
    }
}

std::optional<GeometryEntry> ReadGeometry(
        const TomlValue& geometry,
        const std::string& context, const GeometryContext& ctx,
        std::vector<Diagnostic>& warnings, PathIssues& issues,
        const bool keep_unsupported) {
    EnsureTable(geometry, context + ": not a table");
    const bool has_file = Find(geometry, "file") != nullptr;
    if (has_file == (Find(geometry, "primitive") != nullptr)) {
        Fail(context, "specify exactly one of file and primitive",
             LineOf(geometry));
    }
    GeometryEntry entry;
    GeometrySpec& spec = entry.geometry;
    spec.line = LineOf(geometry);
    if (const TomlValue* opacity = Find(geometry, "opacity"); opacity != nullptr) {
        const double value = AsReal(*opacity, context + ".opacity");
        if (value < 0.0 || value > 1.0) {
            Fail(context, "opacity is not in 0..1: " + FormatValue(*opacity),
                 LineOf(*opacity));
        }
        spec.opacity = static_cast<float>(value);
    }
    entry.placement.origin =
            ReadVec3Or(geometry, "origin", igesio::Vector3d::Zero(), context)
            * ctx.scales.length;
    entry.placement.rotation = ReadRotation(geometry, context, ctx.scales.angle);

    if (!has_file) {
        spec.source = ReadPrimitive(geometry, context, ctx.scales.length);
    } else if (!ReadFileSource(geometry, context, ctx, warnings, issues, spec)
               && !keep_unsupported) {
        return std::nullopt;
    }
    spec.name = OptionalString(geometry, "name", context).value_or("");
    spec.color = ReadColor(geometry, context);
    entry.collision = OptionalBool(geometry, "collision", true, context);
    entry.visible = OptionalBool(geometry, "visible", true, context);
    return entry;
}

}  // namespace igesio::extensions::machines::detail
