/**
 * @file extensions/machines/core/formatting.h
 * @brief machines拡張におけるテキスト出力用のフォーマットおよび相互変換
 * @author Yayoi Habami
 * @date 2026-09-09
 * @copyright 2026 Yayoi Habami
 * @note 以下のフォーマット指定・変換関数を提供する
 *       - 診断・例外等のテキスト出力時に数値を含める際の整形 (固定小数・deg換算)
 *       - 機械定義・プロジェクト定義で共通の色表記`"#rrggbb"`の相互変換
 *         (ホスト側の短縮形・不透明度付きの表記は`ParseHexColorRgba`)
 *       - テキストファイル (NC/CL等) の改行・文字コード関連
 * @note TOML側では色を`#rrggbb`形式でのみ表現するため、`igesio::Color`との
 *       互換関数をここに記述する.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_CORE_FORMATTING_H_
#define IGESIO_EXTENSIONS_MACHINES_CORE_FORMATTING_H_

#include <optional>
#include <string>
#include <string_view>

#include "igesio/common/color.h"

namespace igesio::extensions::machines {

/// @brief 実数を固定小数として文字列化する
/// @param value 整形する値
/// @param digits 小数点以下の桁数
/// @return `std::fixed`で整形した文字列 (例: `FormatFixed(0.5, 3)` → `"0.500"`)
std::string FormatFixed(double value, int digits);

/// @brief 角度 [rad] をdegに換算して固定小数で文字列化する
/// @param radians 角度 [rad]
/// @param digits 小数点以下の桁数 (既定3)
/// @return deg値の文字列 (単位の記号は付けない)
std::string FormatDegrees(double radians, int digits = 3);

/// @brief `"#RRGGBB"`形式の色文字列を`Color`構造体に変換する
/// @param text 色文字列 (`#RRGGBB` (16進6桁). 大文字小文字を区別しない)
/// @return `Color`構造体 (a = 1.0). 形式が異なる場合は`std::nullopt`
/// @note 機械定義・プロジェクト定義の色表記は`#RRGGBB`に限定する.
///       `Color::TryParseHex`で指定可能な`#`の省略や8桁 (`#RRGGBBAA`) は対象外とする.
std::optional<Color> ParseHexColor(std::string_view text);

/// @brief 短縮形と不透明度を含む16進の色文字列を`Color`構造体に変換する
/// @param text 色文字列 (`#RGB`/`#RGBA`/`#RRGGBB`/`#RRGGBBAA`.
///        大文字小文字を区別しない)
/// @return `Color`構造体 (不透明度の無い形式はa = 1.0). 形式が異なる場合は
///         `std::nullopt`
/// @note 短縮形は各桁を2回繰り返して展開する (`#f80` → `#ff8800`). CSS等の
///       色表記をそのまま受け取るホスト用. 機械定義・プロジェクト定義の読込は
///       `ParseHexColor` (`#RRGGBB`のみ) を用いる
std::optional<Color> ParseHexColorRgba(std::string_view text);

/// @brief 色を`"#rrggbb"`形式の文字列に変換する
/// @param color 変換する色. RGBの0~1の範囲外はクランプし、α成分は無視する
/// @return 小文字16進の色文字列 (各成分を255倍して四捨五入)
std::string FormatHexColor(const Color& color);



/**
 * ---- テキストファイルの改行と文字コード ----
 * 主にTOMLの`[[program]]`で指定する改行・文字コードの規約に対応する.
 */

/// @brief テキストファイルの改行の種類
enum class NewlineStyle {
    /// @brief LF (`"\n"`)
    kLf,
    /// @brief CRLF (`"\r\n"`)
    kCrlf,
};

/// @brief テキストファイルの文字コード
/// @note 基本的に読込はバイト列のまま扱い、本列挙型は表示・書き出しの規約として保持する
enum class TextEncoding {
    /// @brief UTF-8
    kUtf8,
    /// @brief Shift_JIS
    kShiftJis,
};

/// @brief 改行の種類の文字列を`NewlineStyle`に変換する
/// @param text `"lf"` / `"crlf"` (大文字小文字を区別する)
/// @return 対応する種類. 未知の文字列なら`std::nullopt`
std::optional<NewlineStyle> ParseNewlineStyle(std::string_view text);

/// @brief 改行の種類の名称 (TOMLで用いる文字列) を返す
std::string_view NewlineStyleName(NewlineStyle style);

/// @brief 改行の文字列 (`"\n"` / `"\r\n"`) を返す
std::string_view NewlineText(NewlineStyle style);

/// @brief 文字コードの文字列を`TextEncoding`に変換する
/// @param text `"utf-8"` / `"shift_jis"` (大文字小文字を区別する)
/// @return 対応する文字コード. 未知の文字列なら`std::nullopt`
std::optional<TextEncoding> ParseTextEncoding(std::string_view text);

/// @brief 文字コードの名称 (TOMLで用いる文字列) を返す
std::string_view TextEncodingName(TextEncoding encoding);

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_CORE_FORMATTING_H_
