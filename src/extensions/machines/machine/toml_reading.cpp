/**
 * @file extensions/machines/machine/toml_reading.cpp
 * @brief machines拡張のTOML読取ヘルパー (内部ヘッダ)
 * @author Yayoi Habami
 * @date 2026-09-08
 * @copyright 2026 Yayoi Habami
 */
#include "extensions/machines/machine/toml_reading.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/extensions/machines/core/rotation.h"

namespace igesio::extensions::machines::detail {

namespace {

/// @brief 拡張子を小文字にして返す (`".STL"` → `".stl"`)
/// @param path 対象のパス
/// @return 小文字の拡張子 (無ければ空)
std::string LowerExtension(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](const unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                   });
    return ext;
}

/// @brief 16進2桁を0~1の値にする
/// @param text `"#RRGGBB"`の文字列
/// @param offset 読み取る位置 (1・3・5)
/// @return 0~1の値. 16進でなければ`std::nullopt`
std::optional<float> HexPair(const std::string& text, const std::size_t offset) {
    int value = 0;
    for (std::size_t i = offset; i < offset + 2; ++i) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        int digit = 0;
        if (std::isdigit(c)) {
            digit = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            digit = 10 + (c - 'a');
        } else if (c >= 'A' && c <= 'F') {
            digit = 10 + (c - 'A');
        } else {
            return std::nullopt;
        }
        value = value * 16 + digit;
    }
    return static_cast<float>(value) / 255.0f;
}

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
    PrimitiveSpec primitive;
    if (kind == "box") {
        primitive.kind = PrimitiveSpec::Kind::kBox;
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
    if (kind == "cylinder") {
        primitive.kind = PrimitiveSpec::Kind::kCylinder;
        const TomlValue* radius = Find(geometry, "radius");
        const TomlValue* height = Find(geometry, "height");
        if (radius == nullptr || height == nullptr) {
            Fail(context, "cylinder requires radius and height",
                 LineOf(geometry));
        }
        primitive.radius = AsPositive(*radius, context + ".radius") * length_scale;
        primitive.height = AsPositive(*height, context + ".height") * length_scale;
        return primitive;
    }
    Fail(context, "unknown primitive: " + kind, LineOf(geometry));
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

    const std::string ext = LowerExtension(path);
    const bool is_iges = (ext == ".igs" || ext == ".iges");
    const bool is_step = (ext == ".stp" || ext == ".step");
    if (is_iges || is_step) {
        if (Find(geometry, "unit") != nullptr) {
            Fail(context, "unit cannot be specified for IGES/STEP",
                 LineOf(geometry));
        }
        if (is_step) {
            Warn(warnings, context,
                 "skipped because the format is unsupported: " + spec.raw_path,
                 LineOf(file));
            return false;
        }
        return true;
    }
    if (ext != ".stl" && ext != ".obj") {
        Fail(context, "unsupported model format: " + spec.raw_path,
             LineOf(file));
    }
    const std::optional<std::string> unit = OptionalString(geometry, "unit", context);
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
 * ---- 位置・診断 ----
 */

int LineOf(const TomlValue& value) {
    return static_cast<int>(value.location().first_line_number());
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

std::string FormatReal(const double value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(6) << value;
    return stream.str();
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

std::vector<std::string> PresentKeys(const TomlValue& table,
                                     const std::initializer_list<const char*> keys) {
    std::vector<std::string> present;
    for (const char* key : keys) {
        if (Find(table, key) != nullptr) present.emplace_back(key);
    }
    return present;
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
        Fail(context, "not a unit vector (norm " + FormatReal(norm) + ")",
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

std::optional<std::array<float, 3>> ReadColor(const TomlValue& table,
                                              const std::string& context) {
    const std::optional<std::string> text = OptionalString(table, "color", context);
    if (!text.has_value()) return std::nullopt;
    const int line = LineOf(*Find(table, "color"));
    if (text->size() != 7 || (*text)[0] != '#') {
        Fail(context + ".color", "not in \"#RRGGBB\" form: " + *text, line);
    }
    std::array<float, 3> rgb{};
    for (std::size_t i = 0; i < 3; ++i) {
        const std::optional<float> channel = HexPair(*text, 1 + 2 * i);
        if (!channel.has_value()) {
            Fail(context + ".color", "not in \"#RRGGBB\" form: " + *text, line);
        }
        rgb[i] = *channel;
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

std::optional<GeometrySpec> ReadGeometry(
        const TomlValue& geometry,
        const std::string& context, const GeometryContext& ctx,
        std::vector<Diagnostic>& warnings, PathIssues& issues) {
    EnsureTable(geometry, context + ": not a table");
    const bool has_file = Find(geometry, "file") != nullptr;
    if (has_file == (Find(geometry, "primitive") != nullptr)) {
        Fail(context, "specify exactly one of file and primitive",
             LineOf(geometry));
    }
    GeometrySpec spec;
    spec.line = LineOf(geometry);
    if (const TomlValue* opacity = Find(geometry, "opacity"); opacity != nullptr) {
        const double value = AsReal(*opacity, context + ".opacity");
        if (value < 0.0 || value > 1.0) {
            Fail(context, "opacity is not in 0..1: " + FormatValue(*opacity),
                 LineOf(*opacity));
        }
        spec.opacity = static_cast<float>(value);
    }
    const igesio::Vector3d origin =
            ReadVec3Or(geometry, "origin", igesio::Vector3d::Zero(), context)
            * ctx.scales.length;
    const igesio::Matrix3d rotation = ReadRotation(geometry, context, ctx.scales.angle);
    spec.placement = ctx.local_frame * MakeRigid(rotation, origin);

    if (!has_file) {
        spec.source = ReadPrimitive(geometry, context, ctx.scales.length);
    } else if (!ReadFileSource(geometry, context, ctx, warnings, issues, spec)) {
        return std::nullopt;
    }
    spec.name = OptionalString(geometry, "name", context).value_or("");
    spec.color = ReadColor(geometry, context);
    spec.collision = OptionalBool(geometry, "collision", true, context);
    spec.visible = OptionalBool(geometry, "visible", true, context);
    return spec;
}

}  // namespace igesio::extensions::machines::detail
