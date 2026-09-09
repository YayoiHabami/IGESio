/**
 * @file extensions/machines/machine/machine_writing.cpp
 * @brief 機械定義のTOML書き出し (内部ヘッダ)
 * @author Yayoi Habami
 * @date 2026-09-09
 * @copyright 2026 Yayoi Habami
 * @note `toml::ordered_value`で文書を組み立て、toml11のシリアライザに
 *       `[[component]]`, `[component.axis]`等の配置を任せる. 実数は往復で
 *       同じ値に戻る最短の桁数 (15〜17桁) で書く.
 */
#include "extensions/machines/machine/machine_writing.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include <toml.hpp>

#include "igesio/extensions/machines/core/rotation.h"
#include "igesio/extensions/machines/core/units.h"

namespace igesio::extensions::machines::detail {

namespace {

/// @brief 挿入順を保持するtoml11の値型 (書き出し順を制御するため)
using TomlValue = toml::ordered_value;
/// @brief `TomlValue`のテーブル型
using TomlTable = toml::ordered_table;
/// @brief `TomlValue`の配列型
using TomlArray = toml::ordered_array;

/// @brief 出力の先頭に置く見出しコメント
constexpr const char* kHeaderComment =
        "# machine-definition 2.0 "
        "(written by the IGESio machines extension)\n\n";
/// @brief 実数をこの大きさ未満なら0として書く
/// @note 90°回転の`cos`や、`local_frame`相対へ戻した原点の丸め誤差が
///       `6e-17`のような値で書かれるのを避ける. 単位ベクトルは読込側で
///       再正規化され (許容1e-3)、長さ・角度で1e-12未満の値は意味を持たない
constexpr double kZeroSnapTolerance = 1e-12;
/// @brief 単位行列・零ベクトルとみなす許容誤差 (省略可能なキーの判定)
constexpr double kIdentityTolerance = 1e-12;
/// @brief `clearance`が`default_clearance`と等しいとみなす許容誤差 [mm]
constexpr double kClearanceTolerance = 1e-9;
/// @brief `file_unit_scale`が単位の係数と等しいとみなす許容誤差
constexpr double kScaleTolerance = 1e-9;
/// @brief 実数の最小有効桁数 (この桁数から往復一致する桁数を探す)
constexpr int kMinPrecision = 15;
/// @brief 実数の最大有効桁数 (doubleの往復に十分な桁数)
constexpr int kMaxPrecision = 17;

/// @brief 書き出し全体で共有する内容
struct WriteContext {
    /// @brief 形状ファイルの相対パスの基準ディレクトリ (正規化済)
    std::filesystem::path base_dir;
    /// @brief `base_dir`が読込時の基準ディレクトリと一致するか
    bool same_as_source = false;
    /// @brief 出力する長さ単位
    LengthUnit length_unit = LengthUnit::kMillimeter;
    /// @brief 内部値 (mm) をファイル値にする除数
    double length_scale = 1.0;
    /// @brief 内部値 (rad) をファイル値にする除数
    double angle_scale = 1.0;
};



/**
 * ---- 値の生成 ----
 */

/// @brief 往復で同じdoubleに戻る最短の有効桁数を返す
/// @note 文字列化→`strtod`の結果が元の値と一致するかを桁数を増やして調べる.
///       一致判定はビット単位の往復検査なので`==`で行う
int ShortestPrecision(const double value) {
    for (int precision = kMinPrecision; precision < kMaxPrecision; ++precision) {
        std::ostringstream stream;
        stream << std::setprecision(precision) << value;
        if (std::strtod(stream.str().c_str(), nullptr) == value) return precision;
    }
    return kMaxPrecision;
}

/// @brief 実数の値を作る (微小値は0、それ以外は最短の往復桁数)
/// @note `-0.0`も0として書く
TomlValue Real(const double value) {
    const double written = std::abs(value) < kZeroSnapTolerance ? 0.0 : value;
    TomlValue result(written);
    result.as_floating_fmt().prec = static_cast<std::size_t>(ShortestPrecision(written));
    return result;
}

/// @brief 複数行形式のテーブル (`[a.b]`) を作る
TomlValue Table() {
    TomlValue result((TomlTable()));
    result.as_table_fmt().fmt = toml::table_format::multiline;
    return result;
}

/// @brief インライン形式のテーブル (`a = { ... }`) を作る
TomlValue InlineTable() {
    TomlValue result((TomlTable()));
    result.as_table_fmt().fmt = toml::table_format::oneline;
    return result;
}

/// @brief テーブル配列 (`[[a]]`) を作る
TomlValue TableArray() {
    TomlValue result((TomlArray()));
    result.as_array_fmt().fmt = toml::array_format::array_of_tables;
    return result;
}

/// @brief 1行の配列を作る
TomlValue OnelineArray(TomlArray elements) {
    TomlValue result(std::move(elements));
    result.as_array_fmt().fmt = toml::array_format::oneline;
    return result;
}

/// @brief 要素ごとに改行する配列を作る (`pairs`・`exclude`)
TomlValue MultilineArray() {
    TomlValue result((TomlArray()));
    result.as_array_fmt().fmt = toml::array_format::multiline;
    return result;
}

/// @brief 実数の配列を作る
TomlValue Reals(const std::vector<double>& values) {
    TomlArray elements;
    for (const double value : values) elements.push_back(Real(value));
    return OnelineArray(std::move(elements));
}

/// @brief 3成分ベクトルを作る
TomlValue Vec3(const igesio::Vector3d& vector) {
    return Reals({vector.x(), vector.y(), vector.z()});
}

/// @brief 文字列2要素の配列を作る (干渉ペアの対象)
TomlValue NamePair(const std::array<std::string, 2>& names) {
    return OnelineArray({TomlValue(names[0]), TomlValue(names[1])});
}

/// @brief 真偽値2要素の配列を作る (`subtree`)
TomlValue BoolPair(const std::array<bool, 2>& flags) {
    return OnelineArray({TomlValue(flags[0]), TomlValue(flags[1])});
}

/// @brief 色を`"#RRGGBB"`にする (各成分を0..1にクランプして255倍・四捨五入)
std::string ColorText(const std::array<float, 3>& rgb) {
    std::ostringstream stream;
    stream << '#' << std::hex << std::setfill('0');
    for (const float channel : rgb) {
        const double clamped = std::clamp(static_cast<double>(channel), 0.0, 1.0);
        stream << std::setw(2) << std::lround(clamped * 255.0);
    }
    return stream.str();
}



/**
 * ---- 幾何要素 ----
 */

/// @brief 回転が単位行列でなければ`rotation = { x_axis, y_axis, z_axis }`を書く
void PutRotation(TomlValue& table, const igesio::Matrix3d& rotation) {
    if (rotation.isIdentity(kIdentityTolerance)) return;
    TomlValue spec = InlineTable();
    spec["x_axis"] = Vec3(rotation.col(0));
    spec["y_axis"] = Vec3(rotation.col(1));
    spec["z_axis"] = Vec3(rotation.col(2));
    table["rotation"] = spec;
}

/// @brief 原点が零ベクトルでなければ`origin`を書く
void PutOrigin(TomlValue& table, const igesio::Vector3d& origin,
               const double length_scale) {
    if (origin.isZero(kIdentityTolerance)) return;
    table["origin"] = Vec3(origin / length_scale);
}

/// @brief `local_frame` (`[[component]].local_frame`) が単位行列でなければ書く
void PutLocalFrame(TomlValue& component, const igesio::Matrix4d& local_frame,
                   const WriteContext& ctx) {
    if (local_frame.isIdentity(kIdentityTolerance)) return;
    TomlValue frame = Table();
    PutOrigin(frame, TranslationPart(local_frame), ctx.length_scale);
    PutRotation(frame, RotationPart(local_frame));
    component["local_frame"] = frame;
}

/// @brief マウントの`frame` (`[[component]].frame`) を
///        `origin`+`z_axis`(+`x_axis`) で書く
/// @note 保持している配置Hは`local_frame`込みなので、`C_c⁻¹·H`で相対に戻す.
///       回転が単位行列なら`z_axis`のみ (`x_axis`は既定のローカルx軸)
TomlValue MakeFrame(const igesio::Matrix4d& local_frame,
                    const igesio::Matrix4d& placement, const double length_scale) {
    const igesio::Matrix4d relative = RigidInverse(local_frame) * placement;
    const igesio::Matrix3d rotation = RotationPart(relative);
    TomlValue frame = Table();
    frame["origin"] = Vec3(TranslationPart(relative) / length_scale);
    frame["z_axis"] = Vec3(rotation.col(2));
    if (!rotation.isIdentity(kIdentityTolerance)) {
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
 * ---- 形状 ----
 */

/// @brief 拡張子を小文字にして返す (`".STL"` → `".stl"`)
std::string LowerExtension(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](const unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                   });
    return ext;
}

/// @brief 換算係数に対応する長さ単位を返す
/// @return mm (1.0) またはinch (25.4). どちらでもなければ`std::nullopt`
std::optional<LengthUnit> UnitFromScale(const double scale) {
    for (const LengthUnit unit : {LengthUnit::kMillimeter, LengthUnit::kInch}) {
        if (std::abs(scale - LengthScale(unit)) <= kScaleTolerance) return unit;
    }
    return std::nullopt;
}

/// @brief 形状ファイルのパス文字列を決める
/// @note 基準ディレクトリが読込時と同じなら記載どおりの`raw_path`. 異なれば
///       解決済みパスを`base_dir`からの相対にし、相対化できなければ
///       (ドライブが異なる等) そのまま書く. 区切りは`/`に統一する
std::string PathText(const GeometrySpec& geometry, const WriteContext& ctx) {
    if (ctx.same_as_source && !geometry.raw_path.empty()) return geometry.raw_path;
    const std::filesystem::path source =
            std::get<std::filesystem::path>(geometry.source).lexically_normal();
    const std::filesystem::path relative = source.lexically_relative(ctx.base_dir);
    return (relative.empty() ? source : relative).generic_string();
}

/// @brief ファイル参照形式 (`[[component.geometry]].file`) の
///       `file`・`unit`を書き出す
/// @note `unit`はSTL/OBJのみ. 出力の長さ単位と同じなら省略する.
///       IGES/STEPは`unit`を持てないため`file_unit_scale`を書かない
/// @throw std::invalid_argument `file_unit_scale`がmm・inchのいずれでもない場合
void PutFileSource(TomlValue& table, const GeometrySpec& geometry,
                   const std::string& context, const WriteContext& ctx) {
    const std::string text = PathText(geometry, ctx);
    table["file"] = text;
    const std::string ext = LowerExtension(std::filesystem::path(text));
    if (ext != ".stl" && ext != ".obj") return;
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

/// @brief プリミティブ形式 (`[[component.geometry]].primitive`) を書き出す
void PutPrimitive(TomlValue& table, const PrimitiveSpec& primitive,
                  const double length_scale) {
    if (primitive.kind == PrimitiveSpec::Kind::kBox) {
        table["primitive"] = "box";
        table["size"] = Vec3(primitive.size / length_scale);
        return;
    }
    table["primitive"] = "cylinder";
    table["radius"] = Real(primitive.radius / length_scale);
    table["height"] = Real(primitive.height / length_scale);
}

/// @brief `[[component.geometry]]`の1要素を書き出す
/// @note 保持している`placement`は`C_c`込みなので、`C_c⁻¹·placement`で
///       `origin`・`rotation`に戻す. 既定値 (`opacity = 1`・`collision`・
///       `visible`が`true`) のキーは書かない
TomlValue MakeGeometry(const GeometrySpec& geometry, const igesio::Matrix4d& local_frame,
                       const std::string& context, const WriteContext& ctx) {
    TomlValue table = Table();
    if (!geometry.name.empty()) table["name"] = geometry.name;
    if (std::holds_alternative<PrimitiveSpec>(geometry.source)) {
        PutPrimitive(table, std::get<PrimitiveSpec>(geometry.source), ctx.length_scale);
    } else {
        PutFileSource(table, geometry, context, ctx);
    }
    const igesio::Matrix4d relative = RigidInverse(local_frame) * geometry.placement;
    PutOrigin(table, TranslationPart(relative), ctx.length_scale);
    PutRotation(table, RotationPart(relative));
    if (geometry.color.has_value()) table["color"] = ColorText(*geometry.color);
    if (geometry.opacity != 1.0f) {   // 既定値 (省略時1) との一致検査
        table["opacity"] = Real(static_cast<double>(geometry.opacity));
    }
    if (!geometry.collision) table["collision"] = false;
    if (!geometry.visible) table["visible"] = false;
    return table;
}



/**
 * ---- コンポーネント ----
 */

/// @brief 暗黙のbaseとして省略できるか (形状も`local_frame`も持たないbase)
bool IsImplicitBase(const ComponentSpec& component) {
    return component.type == ComponentType::kBase && component.geometries.empty()
           && component.local_frame.isIdentity(kIdentityTolerance);
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
    TomlValue geometries = TableArray();
    for (std::size_t i = 0; i < component.geometries.size(); ++i) {
        const std::string context =
                "component[" + component.name + "].geometry[" + std::to_string(i) + "]";
        geometries.push_back(MakeGeometry(component.geometries[i],
                                          component.local_frame, context, ctx));
    }
    table["geometry"] = geometries;
    return table;
}



/**
 * ---- セクション ----
 */

/// @brief `[format]`を書き出す. 常に対応バージョンを書く
TomlValue MakeFormat() {
    TomlValue table = Table();
    table["name"] = std::string(kMachineFormatName);
    table["version"] = OnelineArray({TomlValue(kMachineFormatVersion[0]),
                                     TomlValue(kMachineFormatVersion[1])});
    return table;
}

/// @brief `[machine]`を書き出す. 空の任意キーは省略する
TomlValue MakeMachineMeta(const MachineDefinition& definition) {
    TomlValue table = Table();
    table["name"] = definition.name;
    if (!definition.description.empty()) table["description"] = definition.description;
    if (!definition.author.empty()) table["author"] = definition.author;
    if (!definition.date.empty()) table["date"] = definition.date;
    return table;
}

/// @brief `[units]`を書き出す
TomlValue MakeUnits(const UnitScales& units) {
    TomlValue table = Table();
    table["length"] = std::string(LengthUnitName(units.length_unit));
    table["angle"] = std::string(AngleUnitName(units.angle_unit));
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
    TomlValue detailed = TableArray();
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

/// @brief 書き出しの文脈を作る
WriteContext MakeContext(const MachineDefinition& definition,
                         const std::filesystem::path& base_dir) {
    WriteContext ctx;
    ctx.base_dir = base_dir.lexically_normal();
    ctx.same_as_source = ctx.base_dir == definition.source_dir.lexically_normal();
    ctx.length_unit = definition.units.length_unit;
    ctx.length_scale = LengthScale(definition.units.length_unit);
    ctx.angle_scale = AngleScale(definition.units.angle_unit);
    return ctx;
}

}  // namespace



std::string FormatMachineDefinition(const MachineDefinition& definition,
                                    const std::filesystem::path& base_dir) {
    const WriteContext ctx = MakeContext(definition, base_dir);
    TomlValue root = Table();
    root["format"] = MakeFormat();
    root["machine"] = MakeMachineMeta(definition);
    root["units"] = MakeUnits(definition.units);
    TomlValue components = TableArray();
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
