/**
 * @file extensions/machines/core/units.cpp
 * @brief machines拡張の単位系 (長さ・角度) と換算係数
 * @author Yayoi Habami
 * @date 2026-09-08
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/extensions/machines/core/units.h"

#include <optional>
#include <string_view>

namespace igesio::extensions::machines {

std::optional<LengthUnit> ParseLengthUnit(const std::string_view text) {
    if (text == "mm") return LengthUnit::kMillimeter;
    if (text == "inch") return LengthUnit::kInch;
    return std::nullopt;
}

std::optional<AngleUnit> ParseAngleUnit(const std::string_view text) {
    if (text == "deg") return AngleUnit::kDegree;
    if (text == "rad") return AngleUnit::kRadian;
    return std::nullopt;
}

std::string_view LengthUnitName(const LengthUnit unit) {
    return unit == LengthUnit::kInch ? "inch" : "mm";
}

std::string_view AngleUnitName(const AngleUnit unit) {
    return unit == AngleUnit::kRadian ? "rad" : "deg";
}

double LengthScale(const LengthUnit unit) {
    return unit == LengthUnit::kInch ? kInchToMillimeter : 1.0;
}

double AngleScale(const AngleUnit unit) {
    return unit == AngleUnit::kRadian ? 1.0 : kDegreeToRadian;
}

UnitScales MakeUnitScales(const LengthUnit length_unit,
                          const AngleUnit angle_unit) {
    UnitScales scales;
    scales.length = LengthScale(length_unit);
    scales.angle = AngleScale(angle_unit);
    scales.length_unit = length_unit;
    scales.angle_unit = angle_unit;
    return scales;
}

}  // namespace igesio::extensions::machines
