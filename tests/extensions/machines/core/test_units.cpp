/**
 * @file tests/extensions/machines/core/test_units.cpp
 * @brief machines拡張の単位系 (core/units) のテスト
 * @author Yayoi Habami
 * @date 2026-09-08
 * @copyright 2026 Yayoi Habami
 * @note 対象: ParseLengthUnit / ParseAngleUnit / LengthUnitName / AngleUnitName /
 *       LengthScale / AngleScale / MakeUnitScales / UnitScalesの既定値 /
 *       角度定数 (kDegreeToRadian等) / ToRadians / ToDegrees
 *       - 正常系 (代表値): 4種の単位文字列の解釈、名称との往復、
 *         換算係数 (25.4・π/180)、`MakeUnitScales`の合成、deg↔radの往復
 *       - 正常系 (境界値): 空文字列 (未知扱い)、角度0の換算
 *       - 異常系: 未知の文字列・大文字小文字違いで`std::nullopt` (例外は投げない)
 *       TODO: 退化ケースは列挙型と線形換算のみのAPIのため該当なし
 */
#include <gtest/gtest.h>

#include <cmath>
#include <string>

#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/units.h"

namespace {

namespace mc = igesio::extensions::machines;

/// @brief 数値比較の許容誤差
constexpr double kTol = 1e-12;

}  // namespace



TEST(MachinesUnitsTest, ParseLengthUnit_AcceptsMillimeterAndInch) {
    EXPECT_EQ(mc::ParseLengthUnit("mm"), mc::LengthUnit::kMillimeter);
    EXPECT_EQ(mc::ParseLengthUnit("inch"), mc::LengthUnit::kInch);
}

TEST(MachinesUnitsTest, ParseLengthUnit_RejectsUnknownAndCaseMismatch) {
    EXPECT_FALSE(mc::ParseLengthUnit("in").has_value());
    EXPECT_FALSE(mc::ParseLengthUnit("MM").has_value());
    EXPECT_FALSE(mc::ParseLengthUnit("").has_value());
}

TEST(MachinesUnitsTest, ParseAngleUnit_AcceptsDegreeAndRadian) {
    EXPECT_EQ(mc::ParseAngleUnit("deg"), mc::AngleUnit::kDegree);
    EXPECT_EQ(mc::ParseAngleUnit("rad"), mc::AngleUnit::kRadian);
}

TEST(MachinesUnitsTest, ParseAngleUnit_RejectsUnknownAndCaseMismatch) {
    EXPECT_FALSE(mc::ParseAngleUnit("degree").has_value());
    EXPECT_FALSE(mc::ParseAngleUnit("Rad").has_value());
    EXPECT_FALSE(mc::ParseAngleUnit("").has_value());
}

TEST(MachinesUnitsTest, UnitName_RoundTripsWithParse) {
    for (const auto unit : {mc::LengthUnit::kMillimeter, mc::LengthUnit::kInch}) {
        EXPECT_EQ(mc::ParseLengthUnit(mc::LengthUnitName(unit)), unit);
    }
    for (const auto unit : {mc::AngleUnit::kDegree, mc::AngleUnit::kRadian}) {
        EXPECT_EQ(mc::ParseAngleUnit(mc::AngleUnitName(unit)), unit);
    }
}

TEST(MachinesUnitsTest, LengthScale_IsIdentityForMillimeterAndInchFactorForInch) {
    EXPECT_NEAR(mc::LengthScale(mc::LengthUnit::kMillimeter), 1.0, kTol);
    EXPECT_NEAR(mc::LengthScale(mc::LengthUnit::kInch), 25.4, kTol);
    EXPECT_NEAR(mc::kInchToMillimeter, 25.4, kTol);
}

TEST(MachinesUnitsTest, AngleScale_ConvertsDegreeAndIsIdentityForRadian) {
    // 180 deg が π rad になること (内部単位はrad)
    EXPECT_NEAR(mc::AngleScale(mc::AngleUnit::kDegree) * 180.0, igesio::kPi,
                kTol);
    EXPECT_NEAR(mc::AngleScale(mc::AngleUnit::kRadian), 1.0, kTol);
}

TEST(MachinesUnitsTest, MakeUnitScales_CombinesFactorsAndKeepsUnits) {
    const auto scales = mc::MakeUnitScales(mc::LengthUnit::kInch,
                                           mc::AngleUnit::kRadian);
    EXPECT_NEAR(scales.length, 25.4, kTol);
    EXPECT_NEAR(scales.angle, 1.0, kTol);
    EXPECT_EQ(scales.length_unit, mc::LengthUnit::kInch);
    EXPECT_EQ(scales.angle_unit, mc::AngleUnit::kRadian);
}

TEST(MachinesUnitsTest, MakeUnitScales_DegreeYieldsRadianFactor) {
    const auto scales = mc::MakeUnitScales(mc::LengthUnit::kMillimeter,
                                           mc::AngleUnit::kDegree);
    EXPECT_NEAR(scales.length, 1.0, kTol);
    EXPECT_NEAR(scales.angle, igesio::kPi / 180.0, kTol);
}

TEST(MachinesUnitsTest, UnitScales_DefaultIsMillimeterDegree) {
    // 既定の角度単位はdegなので、angleは恒等でなくπ/180
    const mc::UnitScales scales;
    EXPECT_NEAR(scales.length, 1.0, kTol);
    EXPECT_NEAR(scales.angle, mc::kDegreeToRadian, kTol);
    EXPECT_EQ(scales.length_unit, mc::LengthUnit::kMillimeter);
    EXPECT_EQ(scales.angle_unit, mc::AngleUnit::kDegree);
}

// ---- 角度定数と換算関数 ----

TEST(MachinesUnitsTest, AngleConstants_MatchPi) {
    EXPECT_NEAR(mc::kDegreeToRadian * 180.0, igesio::kPi, kTol);
    EXPECT_NEAR(mc::kRadianToDegree * igesio::kPi, 180.0, kTol);
    EXPECT_NEAR(mc::kDegreeToRadian * mc::kRadianToDegree, 1.0, kTol);
    EXPECT_NEAR(mc::kFullTurn, 2.0 * igesio::kPi, kTol);
    EXPECT_NEAR(mc::kHalfTurn, igesio::kPi, kTol);
    EXPECT_NEAR(mc::kQuarterTurn, igesio::kPi / 2.0, kTol);
}

TEST(MachinesUnitsTest, ToRadians_ConvertsRepresentativeAngles) {
    EXPECT_NEAR(mc::ToRadians(180.0), igesio::kPi, kTol);
    EXPECT_NEAR(mc::ToRadians(90.0), mc::kQuarterTurn, kTol);
    EXPECT_NEAR(mc::ToRadians(-360.0), -mc::kFullTurn, kTol);
    EXPECT_NEAR(mc::ToRadians(0.0), 0.0, kTol);
}

TEST(MachinesUnitsTest, ToDegrees_ConvertsRepresentativeAngles) {
    EXPECT_NEAR(mc::ToDegrees(igesio::kPi), 180.0, kTol);
    EXPECT_NEAR(mc::ToDegrees(mc::kQuarterTurn), 90.0, kTol);
    EXPECT_NEAR(mc::ToDegrees(0.0), 0.0, kTol);
}

TEST(MachinesUnitsTest, ToDegrees_RoundTripsWithToRadians) {
    for (const double deg : {-370.0, -90.0, 0.0, 37.5, 90.0, 359.999}) {
        EXPECT_NEAR(mc::ToDegrees(mc::ToRadians(deg)), deg, 1e-9) << deg;
    }
}

TEST(MachinesUnitsTest, AngleConversion_IsConstexpr) {
    // コンパイル時定数として使えること (テンプレート引数等での利用を保証)
    constexpr double quarter = mc::ToRadians(90.0);
    static_assert(quarter > 1.5 && quarter < 1.6, "ToRadians must be constexpr");
    constexpr double straight = mc::ToDegrees(mc::kHalfTurn);
    static_assert(straight > 179.9 && straight < 180.1,
                  "ToDegrees must be constexpr");
    EXPECT_NEAR(quarter, mc::kQuarterTurn, kTol);
}
