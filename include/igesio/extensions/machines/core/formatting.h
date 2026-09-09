/**
 * @file extensions/machines/core/formatting.h
 * @brief machines拡張におけるテキスト出力用のフォーマットおよび相互変換
 * @author Yayoi Habami
 * @date 2026-09-09
 * @copyright 2026 Yayoi Habami
 * @note 診断・例外等のテキスト出力時に数値を含める際の整形 (固定小数・deg換算) と、
 *       機械定義・プロジェクト定義で共通の色表記`"#RRGGBB"`の相互変換をまとめる.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_CORE_FORMATTING_H_
#define IGESIO_EXTENSIONS_MACHINES_CORE_FORMATTING_H_

#include <array>
#include <optional>
#include <string>
#include <string_view>

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

/// @brief `"#RRGGBB"`形式の色文字列をRGBの配列 (0~1) に変換する
/// @param text 色文字列 (`#RRGGBB` (16進6桁). 大文字小文字を区別しない)
/// @return RGB各成分 (0~1). 形式が異なる場合は`std::nullopt`
std::optional<std::array<float, 3>> ParseHexColor(std::string_view text);

/// @brief RGB各成分 (0~1) を`"#rrggbb"`形式の文字列に変換する
/// @param rgb RGBの各成分. 0~1の範囲外はクランプする
/// @return 小文字16進の色文字列 (各成分を255倍して四捨五入)
std::string FormatHexColor(const std::array<float, 3>& rgb);

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_CORE_FORMATTING_H_
