/**
 * @file extensions/machines/machine/toml_reading.h
 * @brief machines拡張のTOML読取ヘルパー (内部ヘッダ)
 * @author Yayoi Habami
 * @date 2026-09-08
 * @copyright 2026 Yayoi Habami
 * @note toml11に依存する読取・型検査・幾何要素 (回転・色・形状・パス) の処理を,
 *       機械定義とプロジェクト定義の両読込で共有するための内部ヘッダ.
 *       公開ヘッダにはtoml11の型を出さないようにするため、`src/`にのみ置く.
 * @note 値の読取は`contains`/`is_*`で型検査してから`as_*`を呼ぶため、
 *       toml11の`type_error`/`std::out_of_range`は発生しない.
 *       フォーマットに対しての仕様違反は`igesio::DataFormatError`で報告する.
 */
#ifndef SRC_EXTENSIONS_MACHINES_MACHINE_TOML_READING_H_
#define SRC_EXTENSIONS_MACHINES_MACHINE_TOML_READING_H_

#include <array>
#include <cstddef>
#include <filesystem>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

#include <toml.hpp>

#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/diagnostics.h"
#include "igesio/extensions/machines/core/units.h"
#include "igesio/extensions/machines/machine/machine_definition.h"

namespace igesio::extensions::machines::detail {

/// @brief toml11の値型
using TomlValue = toml::value;

/// @brief 回転3形式のキー名
/// @note TOMLの`[[component.geometry]]`内にて指定可能な回転の形式のキー名
constexpr std::array<const char*, 3> kRotationKeys = {
        "rotation", "rotation_axis_angle", "rotation_euler_ijk"};

/// @brief 絶対パスの件数 (警告を1件にまとめるための集計)
struct PathIssues {
    /// @brief 絶対パスの件数
    int absolute = 0;
    /// @brief `..`による親ディレクトリ越えの件数
    int escape = 0;
};

/// @brief 形状テーブルの解釈に必要な情報
struct GeometryContext {
    /// @brief 相対パス解決の基準ディレクトリ
    std::filesystem::path base_dir;
    /// @brief 単位換算係数
    UnitScales scales;
    /// @brief 所属コンポーネントのローカル座標系の配置C_c
    igesio::Matrix4d local_frame = igesio::Matrix4d::Identity();
};



/**
 * ---- 位置・診断 ----
 **/

/// @brief 値のTOML行番号を返す
int LineOf(const TomlValue& value);

/// @brief 仕様違反を`DataFormatError`として投げる
/// @param context 発生箇所 (`"component[A].axis"`等)
/// @param message 内容
/// @param line TOMLの行番号 (0なら省略)
[[noreturn]] void Fail(const std::string& context, const std::string& message,
                       int line = 0);

/// @brief 警告を追加する
void Warn(std::vector<Diagnostic>& warnings, const std::string& context,
          const std::string& message, int line = 0);

/// @brief 値をTOML表記の文字列にする (診断文言用)
std::string FormatValue(const TomlValue& value);



/**
 * ---- 型検査つきでの取得 ----
 */

/// @brief テーブルのキーを引く
/// @return 値. `table`がテーブルでない、またはキーが無ければ`nullptr`
const TomlValue* Find(const TomlValue& table, const std::string& key);

/// @brief 値がテーブルであることを検査する
/// @throw igesio::DataFormatError テーブルでない場合 (`message`をそのまま用いる)
void EnsureTable(const TomlValue& value, const std::string& message);

/// @brief 必須の文字列キーを読む
/// @throw igesio::DataFormatError 欠落・文字列でない・空の場合
/// @param context 読込箇所 (`"component[A].axis"`等)
std::string RequireString(const TomlValue& table, const std::string& key,
                          const std::string& context);

/// @brief 任意の文字列キーを読む
/// @param context 読込箇所 (`"component[A].axis"`等)
/// @return 値. 欠落なら`std::nullopt`
/// @throw igesio::DataFormatError 文字列でない場合
std::optional<std::string>
OptionalString(const TomlValue& table, const std::string& key,
               const std::string& context);

/// @brief 任意の真偽値キーを読む
/// @param context 読込箇所 (`"component[A].axis"`等)
/// @throw igesio::DataFormatError 真偽値でない場合
bool OptionalBool(const TomlValue& table, const std::string& key,
                  bool default_value, const std::string& context);

/// @brief 指定したキーのうちテーブルに存在するものを列挙する (順序保持)
std::vector<std::string>
PresentKeys(const TomlValue& table, std::initializer_list<const char*> keys);



/**
 * ---- 実数互換 (整数リテラルを実数として受理する) ----
 */

/// @brief 値を実数として読む
/// @param context 読込箇所 (`"component[A].axis"`等)
/// @throw igesio::DataFormatError 整数・実数以外 (真偽値を含む) の場合
double AsReal(const TomlValue& value, const std::string& context);

/// @brief 正 (または0以上) の実数として読む
/// @param context 読込箇所 (`"component[A].axis"`等)
/// @param allow_zero `true`なら0を許す
/// @throw igesio::DataFormatError 実数でない、または符号条件を満たさない場合
double AsPositive(const TomlValue& value, const std::string& context,
                  bool allow_zero = false);

/// @brief size成分の実数配列として読む
/// @param context 読込箇所 (`"component[A].axis"`等)
/// @throw igesio::DataFormatError 配列でない・長さ不一致・非実数の場合
std::vector<double> AsRealArray(const TomlValue& value, std::size_t size,
                                const std::string& context);

/// @brief 実数3成分のベクトルとして読む
/// @param context 読込箇所 (`"component[A].axis"`等)
igesio::Vector3d AsVec3(const TomlValue& value, const std::string& context);

/// @brief 単位ベクトルとして読み、再正規化する
/// @throw igesio::DataFormatError ノルムが1から許容誤差 (1e-3) を超えて外れる場合
igesio::Vector3d AsUnitVec3(const TomlValue& value, const std::string& context);

/// @brief 必須の実数キーを読む
/// @param context 読込箇所 (`"component[A].axis"`等)
/// @throw igesio::DataFormatError 欠落または実数でない場合
double ReadReal(const TomlValue& table, const std::string& key,
                const std::string& context);

/// @brief 任意の実数キーを読む (欠落時はdefault_value)
/// @param context 読込箇所 (`"component[A].axis"`等)
double ReadRealOr(const TomlValue& table, const std::string& key,
                  double default_value, const std::string& context);

/// @brief 必須の実数3成分キーを読む
/// @param context 読込箇所 (`"component[A].axis"`等)
/// @throw igesio::DataFormatError 欠落または形式不正の場合
igesio::Vector3d ReadVec3(const TomlValue& table, const std::string& key,
                          const std::string& context);

/// @brief 任意の実数3成分キーを読む (欠落時はdefault_value)
/// @param context 読込箇所 (`"component[A].axis"`等)
igesio::Vector3d ReadVec3Or(const TomlValue& table, const std::string& key,
                            const igesio::Vector3d& default_value,
                            const std::string& context);



/**
 * ---- 幾何要素 ----
 */

/// @brief テーブル中の回転指定 (3形式・排他) を回転行列にする
/// @param table `rotation`等を持ち得るテーブル
/// @param context 読込箇所 (`"component[A].axis"`等)
/// @param angle_scale 角度の換算係数 (ファイル値→rad)
/// @return 回転行列. 指定が無ければ単位行列
/// @throw igesio::DataFormatError 複数形式の同時指定、鏡映、値が正しくない場合
igesio::Matrix3d ReadRotation(const TomlValue& table, const std::string& context,
                              double angle_scale);

/// @brief 任意の`color`キー (`"#RRGGBB"`) を読む
/// @return RGB各0..1. 欠落なら`std::nullopt`
/// @throw igesio::DataFormatError 形式が不正な場合
std::optional<std::array<float, 3>> ReadColor(const TomlValue& table,
                                              const std::string& context);

/// @brief 形状パスの規約を検査し、絶対パスを集計する
/// @note 絶対パスは先頭`/`またはドライブ文字 (`X:`) で判定する (OS非依存).
///       親ディレクトリ越えは正規化後の先頭要素が`..`のときのみ数える
/// @throw igesio::DataFormatError `\`を含む場合
void CheckFilePath(const std::string& raw, const std::string& context,
                   PathIssues& issues);

/// @brief パス文字列が絶対パスか (`CheckFilePath`と同じ判定)
bool IsAbsolutePathString(const std::string& raw);

/// @brief `[[component.geometry]]`相当のテーブルを読む (仕様§4.8)
/// @param geometry 形状テーブル
/// @param context 読込箇所 (`"component[A].axis"`等)
/// @param ctx 形状テーブルの解釈に必要な情報
/// @param warnings 警告の追記先
/// @param issues 絶対パスの集計先
/// @return 形状. 未対応形式 (STEP) は警告して`std::nullopt`
/// @throw igesio::DataFormatError 仕様違反
std::optional<GeometrySpec>
ReadGeometry(const TomlValue& geometry,
             const std::string& context, const GeometryContext& ctx,
             std::vector<Diagnostic>& warnings, PathIssues& issues);

}  // namespace igesio::extensions::machines::detail

#endif  // SRC_EXTENSIONS_MACHINES_MACHINE_TOML_READING_H_
