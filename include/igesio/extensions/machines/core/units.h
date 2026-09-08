/**
 * @file extensions/machines/core/units.h
 * @brief machines拡張の単位系 (長さ・角度) と換算係数
 * @author Yayoi Habami
 * @date 2026-09-08
 * @copyright 2026 Yayoi Habami
 * @note 本拡張の内部単位はmmとradで固定する. TOMLの`[units]`で宣言された単位は
 *       読込時に`UnitScales`の係数を乗じて内部単位へ換算し、以降の処理は
 *       単位を意識しない. C++が保持する角度はradであり,
 *       degはファイル上の表現と人が読む出力 (診断文言・GUI表示) でのみ使用する.
 */
#ifndef IGESIO_EXTENSIONS_MACHINES_CORE_UNITS_H_
#define IGESIO_EXTENSIONS_MACHINES_CORE_UNITS_H_

#include <optional>
#include <string_view>

#include "igesio/numerics/core/matrix.h"

namespace igesio::extensions::machines {

/// @brief 長さの単位
enum class LengthUnit {
    /// @brief ミリメートル (内部単位)
    kMillimeter,
    /// @brief インチ
    kInch,
};

/// @brief 角度の単位
enum class AngleUnit {
    /// @brief ラジアン (内部単位)
    kRadian,
    /// @brief 度
    kDegree,
};

/// @brief inchからmmへの換算係数
constexpr double kInchToMillimeter = 25.4;

/// @brief degからradへの換算係数 (π/180)
constexpr double kDegreeToRadian = igesio::kPi / 180.0;
/// @brief radからdegへの換算係数 (180/π)
constexpr double kRadianToDegree = 180.0 / igesio::kPi;

/// @brief 1回転 (2π rad)
/// @note 回転軸の巻き戻し・正規化区間の幅に用いる
constexpr double kFullTurn = 2.0 * igesio::kPi;
/// @brief 半回転 (π rad)
constexpr double kHalfTurn = igesio::kPi;
/// @brief 1/4回転 (π/2 rad)
constexpr double kQuarterTurn = igesio::kPi / 2.0;

/// @brief degをradへ換算する
/// @param degrees 角度 [deg]
/// @return 角度 [rad]
constexpr double ToRadians(const double degrees) {
    return degrees * kDegreeToRadian;
}

/// @brief radをdegへ換算する
/// @param radians 角度 [rad]
/// @return 角度 [deg]
constexpr double ToDegrees(const double radians) {
    return radians * kRadianToDegree;
}

/// @brief ファイル値から内部単位 (mm・rad) への換算係数
/// @note `length`・`angle`はファイル値に乗じる係数.
///       元の単位も保持し、形状ファイルの`unit`キーの既定値等に用いる.
///       既定の角度単位はdegなので、既定値でも`angle`は恒等でない
struct UnitScales {
    /// @brief 長さの換算係数 (ファイル値 × length = mm)
    double length = 1.0;
    /// @brief 角度の換算係数 (ファイル値 × angle = rad)
    double angle = kDegreeToRadian;
    /// @brief 宣言された長さ単位
    LengthUnit length_unit = LengthUnit::kMillimeter;
    /// @brief 宣言された角度単位
    AngleUnit angle_unit = AngleUnit::kDegree;
};

/// @brief 長さ単位の文字列を変換する
/// @param text `"mm"`または`"inch"` (大文字小文字を区別する)
/// @return 対応する単位. 未知の文字列なら`std::nullopt`
std::optional<LengthUnit> ParseLengthUnit(std::string_view text);

/// @brief 角度単位の文字列を変換する
/// @param text `"deg"`または`"rad"` (大文字小文字を区別する)
/// @return 対応する単位. 未知の文字列なら`std::nullopt`
std::optional<AngleUnit> ParseAngleUnit(std::string_view text);

/// @brief 長さ単位の名称 (TOMLで用いる文字列) を返す
std::string_view LengthUnitName(LengthUnit unit);

/// @brief 角度単位の名称 (TOMLで用いる文字列) を返す
std::string_view AngleUnitName(AngleUnit unit);

/// @brief 長さ単位からmmへの換算係数を返す
/// @return mmなら1.0、inchなら25.4
double LengthScale(LengthUnit unit);

/// @brief 角度単位からradへの換算係数を返す
/// @return degならπ/180、radなら1.0
double AngleScale(AngleUnit unit);

/// @brief 宣言された単位の組から換算係数を作る
UnitScales MakeUnitScales(LengthUnit length_unit, AngleUnit angle_unit);

}  // namespace igesio::extensions::machines

#endif  // IGESIO_EXTENSIONS_MACHINES_CORE_UNITS_H_
