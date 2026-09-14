/**
 * @file extensions/machines/machine/toml_writing.h
 * @brief machines拡張のTOML書き出しヘルパー (内部ヘッダ)
 * @author Yayoi Habami
 * @date 2026-09-12
 * @copyright 2026 Yayoi Habami
 * @note toml11に依存する値の生成 (実数の桁数・配列・テーブルの書式)、幾何要素
 *       (回転・原点・形状・パス) の書き出し、`[format]`・`[units]`の生成を,
 *       機械定義とプロジェクト定義の両書き出しで共有するための内部ヘッダ.
 *       `toml_reading.h`と対になり、値型も同じ`toml::ordered_value`を用いる.
 * @note 実数は往復で同じ値に戻る最短の桁数 (15〜17桁) で書き、`kZeroTolerance`
 *       未満の微小値は0とする. 単位は`WriteContext`で宣言する単位に戻す.
 */
#ifndef SRC_EXTENSIONS_MACHINES_MACHINE_TOML_WRITING_H_
#define SRC_EXTENSIONS_MACHINES_MACHINE_TOML_WRITING_H_

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <toml.hpp>

#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/opaque_toml.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/machine/machine_definition.h"
#include "extensions/machines/machine/toml_reading.h"

namespace igesio::extensions::machines::detail {

/// @brief `TomlValue`のテーブル型
using TomlTable = toml::ordered_table;
/// @brief `TomlValue`の配列型
using TomlArray = toml::ordered_array;

/// @brief TOML出力全体で共有する内容
struct WriteContext {
    /// @brief 参照ファイルの相対パスの基準ディレクトリ (正規化済)
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

/// @brief `WriteContext`を作る
/// @param source_dir 読込時の基準ディレクトリ (`source_dir`)
/// @param units 出力する宣言単位
/// @param base_dir 出力先の基準ディレクトリ
WriteContext MakeContext(const std::filesystem::path& source_dir,
                         const UnitScales& units,
                         const std::filesystem::path& base_dir);



/**
 * ---- 値の生成 ----
 */

/// @brief 読み書きの際に同じdoubleに戻る最短の有効桁数を返す
int ShortestPrecision(double value);

/// @brief 出力用の実数値を作る
/// @note 微小値は0、それ以外は往復で同じ値に戻る最短の桁数で書く
TomlValue Real(double value);

/// @brief 単精度の値を作る (`float`として往復する最短の桁数)
/// @note `opacity`のように`float`で保持する値を`double`経由で書くと
///       `0.800000011920929`のような表記になるため、`float`で往復する桁数を探す
TomlValue RealFromFloat(float value);

/// @brief 複数行形式のテーブル (`[a.b]`) を作る
TomlValue Table();

/// @brief インライン形式のテーブル (`a = { ... }`) を作る
TomlValue InlineTable();

/// @brief テーブル配列 (`[[a]]`) を作る
TomlValue TableArrayValue();

/// @brief 1行の配列を作る
TomlValue OnelineArray(TomlArray elements);

/// @brief 要素ごとに改行する配列を作る
TomlValue MultilineArray();

/// @brief 実数の配列を作る
TomlValue Reals(const std::vector<double>& values);

/// @brief 3成分ベクトルを作る
TomlValue Vec3(const igesio::Vector3d& vector);

/// @brief 文字列2要素の配列を作る
/// @note 干渉ペアの対象等
TomlValue NamePair(const std::array<std::string, 2>& names);

/// @brief 真偽値2要素の配列を作る
/// @note `subtree`等
TomlValue BoolPair(const std::array<bool, 2>& flags);

/// @brief 日付・日時として解釈できればTOMLの日付・日時型、できなければ文字列で書く
/// @param table 書き込み先
/// @param key キー名 (`"date"`・`"modified"`)
/// @param text 表記 (`ReadDateTimeText`で読んだ文字列)
void PutDateTime(TomlValue& table,
                 const std::string& key, const std::string& text);

/// @brief machine拡張側で読み込まないTOML要素をtoml11の値へ戻す
/// @param key opaqueのトップレベルキー (記述内容と一致すること)
/// @param opaque TOML要素
/// @return キーに対応する値 (書式情報つき)
/// @throw std::invalid_argument `opaque`をTOMLとして解析できない,
///        トップレベルのキーが1つでない、または`key`と一致しない場合
TomlValue FromOpaque(const std::string& key, const OpaqueToml& opaque);



/**
 * ---- 幾何要素 ----
 */

/// @brief 回転が単位行列でなければ`rotation`を書き込む
/// @param table 書き込み先
/// @param rotation 回転行列
/// @note `rotation = { x_axis, y_axis, z_axis }`
void PutRotation(TomlValue& table, const igesio::Matrix3d& rotation);

/// @brief 原点が零ベクトルでなければ`origin`を書き込む
/// @param table 書き込み先
/// @param origin 原点
/// @param length_scale 内部値 (mm) をファイル値にする除数
void PutOrigin(TomlValue& table,
               const igesio::Vector3d& origin, double length_scale);

/// @brief 換算係数に対応する長さ単位を返す
/// @return mm (1.0) またはinch (25.4). どちらでもなければ`std::nullopt`
std::optional<LengthUnit> UnitFromScale(double scale);

/// @brief 参照ファイルのパス文字列を決める
/// @param raw TOMLに記載されていたパス (空なら解決済みパスから作る)
/// @param resolved 解決済みパス
/// @param ctx TOML出力に関する設定
/// @return 基準ディレクトリが読込時と同じであれば記載どおりの`raw`.
///         異なれば解決済みパスを`base_dir`からの相対にし、相対化できなければ
///         (ドライブが異なる等) そのままとする. 区切りは`/`に統一する
std::string PathText(const std::string& raw, const std::filesystem::path& resolved,
                     const WriteContext& ctx);

/// @brief ファイル参照形式 (`[[component.geometry]].file`) の`file`,`unit`を書き込む
/// @param table 書き込み先
/// @param geometry 形状に関するデータ
/// @param context 例外の文言に用いる読込箇所
/// @param ctx TOML出力に関する設定
/// @note `unit`はSTL/OBJのみ. 出力の長さ単位と同じなら省略する.
///       IGES/STEPの場合は`unit`を指定しないため、`file_unit_scale`を書かない
/// @throw std::invalid_argument `file_unit_scale`がmm・inchのいずれでもない場合
void PutFileSource(TomlValue& table,
                   const GeometrySpec& geometry, const std::string& context,
                   const WriteContext& ctx);

/// @brief プリミティブ形式 (`[[component.geometry]].primitive`) を書き込む
/// @param table 書き込み先
/// @param primitive プリミティブに関するデータ
/// @param length_scale 内部値 (mm) をファイル値にする除数
void PutPrimitive(TomlValue& table,
                  const PrimitiveSpec& primitive, double length_scale);

/// @brief `[[component.geometry]]`相当のテーブルを書き出す
/// @param entry 形状と、その座標系→親フレームの剛体変換
///        (`origin`・`rotation`は保持している値をそのまま書く)
/// @param context 例外の文言に用いる読込箇所
/// @param ctx TOML出力に関する設定
/// @param default_collision `collision`の既定値 (一致した場合は省略)
/// @note 既定値 (`opacity = 1`・`visible = true`) のキーは書かない
TomlValue MakeGeometry(const GeometryEntry& entry,
                       const std::string& context, const WriteContext& ctx,
                       bool default_collision = true);



/**
 * ---- 共通セクション ----
 */

/// @brief `[format]`を書き出す
/// @param name フォーマット名
/// @param version 書き出すバージョン
TomlValue MakeFormat(std::string_view name,
                     const std::array<int, 2>& version);

/// @brief `[units]`を書き出す
TomlValue MakeUnits(const UnitScales& units);

}  // namespace igesio::extensions::machines::detail

#endif  // SRC_EXTENSIONS_MACHINES_MACHINE_TOML_WRITING_H_
