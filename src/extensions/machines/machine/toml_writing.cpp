/**
 * @file extensions/machines/machine/toml_writing.cpp
 * @brief machines拡張のTOML書き出しヘルパー (内部ヘッダ)
 * @author Yayoi Habami
 * @date 2026-09-12
 * @copyright 2026 Yayoi Habami
 */
#include "extensions/machines/machine/toml_writing.h"

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

#include "igesio/extensions/machines/core/formatting.h"
#include "igesio/extensions/machines/core/tolerances.h"

namespace igesio::extensions::machines::detail {

namespace {

/// @brief `file_unit_scale`が単位の係数と等しいとみなす許容誤差
constexpr double kScaleTolerance = 1e-9;
/// @brief 実数の最小有効桁数
/// @note この桁数を基準に、読み書きの際に同じ数値に戻る最短の有効桁数を返す
constexpr int kMinPrecision = 15;
/// @brief 実数の最大有効桁数 (doubleの往復に十分な桁数)
constexpr int kMaxPrecision = 17;
/// @brief 単精度の実数の最大有効桁数 (floatの往復に十分な桁数)
constexpr int kMaxFloatPrecision = 9;

/// @brief 日付・日時の表記を解析する
/// @param text 表記 (`2026-08-31`、`2026-09-02T10:00:00+09:00`等)
/// @return toml11の日付・日時の値. 日付・日時として解釈できなければ`std::nullopt`
/// @note `v = <text>`の1行文書として解析し、得た値の型で判定する
std::optional<TomlValue> ParseDateTime(const std::string& text) {
    try {
        const TomlValue root = toml::parse_str<toml::ordered_type_config>("v = " + text);
        const TomlValue& value = root.at("v");
        if (value.is_local_date() || value.is_local_datetime()
            || value.is_offset_datetime()) {
            return value;
        }
    } catch (const toml::exception&) {
        // 日付・日時の形式でない (文字列として書く)
    }
    return std::nullopt;
}

}  // namespace



WriteContext MakeContext(const std::filesystem::path& source_dir,
                         const UnitScales& units,
                         const std::filesystem::path& base_dir) {
    WriteContext ctx;
    ctx.base_dir = base_dir.lexically_normal();
    ctx.same_as_source = ctx.base_dir == source_dir.lexically_normal();
    ctx.length_unit = units.length_unit;
    ctx.length_scale = LengthScale(units.length_unit);
    ctx.angle_scale = AngleScale(units.angle_unit);
    return ctx;
}



/**
 * ---- 値の生成 ----
 */

int ShortestPrecision(const double value) {
    // 文字列化→`strtod`の結果が元の値と一致するかを桁数を増やして調べる.
    // 一致判定はビット単位の往復検査なので`==`で行う
    for (int precision = kMinPrecision; precision < kMaxPrecision; ++precision) {
        std::ostringstream stream;
        stream << std::setprecision(precision) << value;
        if (std::strtod(stream.str().c_str(), nullptr) == value) return precision;
    }
    return kMaxPrecision;
}

TomlValue Real(const double value) {
    // 90°回転の`cos`や、`local_frame`相対へ戻した取り付け点の原点の丸め誤差が
    // `6e-17`のような値で書かれることを避けるため、`kZeroTolerance`未満は0とする
    // (`-0.0`も0).
    // 単位ベクトルは読込側で再正規化されるため (許容1e-3)、長さ・角度で1e-12未満の
    // 値は意味を持たない
    const double written = std::abs(value) < kZeroTolerance ? 0.0 : value;
    TomlValue result(written);
    result.as_floating_fmt().prec = static_cast<std::size_t>(ShortestPrecision(written));
    return result;
}

TomlValue RealFromFloat(const float value) {
    const double written = std::abs(value) < kZeroTolerance ? 0.0
                                                            : static_cast<double>(value);
    int precision = kMaxFloatPrecision;
    for (int candidate = 1; candidate < kMaxFloatPrecision; ++candidate) {
        std::ostringstream stream;
        stream << std::setprecision(candidate) << written;
        if (std::strtof(stream.str().c_str(), nullptr) == value) {
            precision = candidate;
            break;
        }
    }
    TomlValue result(written);
    result.as_floating_fmt().prec = static_cast<std::size_t>(precision);
    return result;
}

TomlValue Table() {
    TomlValue result((TomlTable()));
    result.as_table_fmt().fmt = toml::table_format::multiline;
    return result;
}

TomlValue InlineTable() {
    TomlValue result((TomlTable()));
    result.as_table_fmt().fmt = toml::table_format::oneline;
    return result;
}

TomlValue TableArrayValue() {
    TomlValue result((TomlArray()));
    result.as_array_fmt().fmt = toml::array_format::array_of_tables;
    return result;
}

TomlValue OnelineArray(TomlArray elements) {
    TomlValue result(std::move(elements));
    result.as_array_fmt().fmt = toml::array_format::oneline;
    return result;
}

TomlValue MultilineArray() {
    TomlValue result((TomlArray()));
    result.as_array_fmt().fmt = toml::array_format::multiline;
    return result;
}

TomlValue Reals(const std::vector<double>& values) {
    TomlArray elements;
    for (const double value : values) elements.push_back(Real(value));
    return OnelineArray(std::move(elements));
}

TomlValue Vec3(const igesio::Vector3d& vector) {
    return Reals({vector.x(), vector.y(), vector.z()});
}

TomlValue NamePair(const std::array<std::string, 2>& names) {
    return OnelineArray({TomlValue(names[0]), TomlValue(names[1])});
}

TomlValue BoolPair(const std::array<bool, 2>& flags) {
    return OnelineArray({TomlValue(flags[0]), TomlValue(flags[1])});
}

void PutDateTime(TomlValue& table, const std::string& key, const std::string& text) {
    const std::optional<TomlValue> parsed = ParseDateTime(text);
    if (parsed.has_value()) {
        table[key] = *parsed;
    } else {
        table[key] = text;
    }
}

TomlValue FromOpaque(const std::string& key, const OpaqueToml& opaque) {
    TomlValue root;
    try {
        root = toml::parse_str<toml::ordered_type_config>(opaque.Text());
    } catch (const toml::exception& e) {
        throw std::invalid_argument("retained fragment \"" + key
                                    + "\" cannot be parsed: " + e.what());
    }
    const auto& entries = root.as_table();
    if (entries.size() != 1 || entries.begin()->first != key) {
        throw std::invalid_argument("retained fragment \"" + key + "\" "
                                    "must contain exactly that top-level key");
    }
    return entries.begin()->second;
}



/**
 * ---- 幾何要素 ----
 */

void PutRotation(TomlValue& table, const igesio::Matrix3d& rotation) {
    if (rotation.isIdentity(kZeroTolerance)) return;
    TomlValue spec = InlineTable();
    spec["x_axis"] = Vec3(rotation.col(0));
    spec["y_axis"] = Vec3(rotation.col(1));
    spec["z_axis"] = Vec3(rotation.col(2));
    table["rotation"] = spec;
}

void PutOrigin(TomlValue& table,
               const igesio::Vector3d& origin, const double length_scale) {
    if (origin.isZero(kZeroTolerance)) return;
    table["origin"] = Vec3(origin / length_scale);
}

std::optional<LengthUnit> UnitFromScale(const double scale) {
    for (const LengthUnit unit : {LengthUnit::kMillimeter, LengthUnit::kInch}) {
        if (std::abs(scale - LengthScale(unit)) <= kScaleTolerance) return unit;
    }
    return std::nullopt;
}

std::string PathText(const std::string& raw,
                     const std::filesystem::path& resolved,
                     const WriteContext& ctx) {
    if (ctx.same_as_source && !raw.empty()) return raw;
    const std::filesystem::path source = resolved.lexically_normal();
    const std::filesystem::path relative = source.lexically_relative(ctx.base_dir);
    return (relative.empty() ? source : relative).generic_string();
}

void PutFileSource(TomlValue& table,
                   const GeometrySpec& geometry, const std::string& context,
                   const WriteContext& ctx) {
    const std::string text = PathText(
            geometry.raw_path, std::get<std::filesystem::path>(geometry.source), ctx);
    table["file"] = text;
    const GeometryFileFormat format = ClassifyGeometryFile(std::filesystem::path(text));
    if (format != GeometryFileFormat::kStl && format != GeometryFileFormat::kObj) return;
    const std::optional<LengthUnit> unit = UnitFromScale(geometry.file_unit_scale);
    if (!unit.has_value()) {
        throw std::invalid_argument(
                context + ": file_unit_scale "
                + std::to_string(geometry.file_unit_scale)
                + " matches neither the mm nor the inch factor"
                  " (cannot be expressed by unit)");
    }
    if (*unit != ctx.length_unit) table["unit"] = std::string(LengthUnitName(*unit));
}

void PutPrimitive(TomlValue& table,
                  const PrimitiveSpec& primitive, const double length_scale) {
    table["primitive"] = std::string(PrimitiveKindName(primitive.kind));
    if (primitive.kind == PrimitiveSpec::Kind::kBox) {
        table["size"] = Vec3(primitive.size / length_scale);
        return;
    }
    table["radius"] = Real(primitive.radius / length_scale);
    table["height"] = Real(primitive.height / length_scale);
}

TomlValue MakeGeometry(
        const GeometryEntry& entry,
        const std::string& context, const WriteContext& ctx,
        const bool default_collision) {
    const GeometrySpec& geometry = entry.geometry;
    TomlValue table = Table();
    if (!geometry.name.empty()) table["name"] = geometry.name;
    if (std::holds_alternative<PrimitiveSpec>(geometry.source)) {
        PutPrimitive(table, std::get<PrimitiveSpec>(geometry.source),
                     ctx.length_scale);
    } else {
        PutFileSource(table, geometry, context, ctx);
    }

    PutOrigin(table, entry.placement.origin, ctx.length_scale);
    PutRotation(table, entry.placement.rotation);
    if (geometry.color.has_value()) table["color"] = FormatHexColor(*geometry.color);
    if (geometry.opacity != 1.0f) {   // 既定値 (省略時1) との一致検査
        table["opacity"] = RealFromFloat(geometry.opacity);
    }
    if (entry.collision != default_collision) {
        table["collision"] = entry.collision;
    }
    if (!entry.visible) table["visible"] = false;
    return table;
}



/**
 * ---- 共通セクション ----
 */

TomlValue MakeFormat(const std::string_view name,
                     const std::array<int, 2>& version) {
    TomlValue table = Table();
    table["name"] = std::string(name);
    table["version"] = OnelineArray({TomlValue(version[0]),
                                     TomlValue(version[1])});
    return table;
}

TomlValue MakeUnits(const UnitScales& units) {
    TomlValue table = Table();
    table["length"] = std::string(LengthUnitName(units.length_unit));
    table["angle"] = std::string(AngleUnitName(units.angle_unit));
    return table;
}

}  // namespace igesio::extensions::machines::detail
