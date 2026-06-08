/**
 * @file numerics/test_tolerance.cpp
 * @brief numerics/core/tolerance.hのテスト
 * @author Yayoi Habami
 * @date 2025-08-11
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "igesio/numerics/core/tolerance.h"

namespace {

namespace i_num = igesio::numerics;

}  // namespace



TEST(ToleranceTest, IsApproxZero) {
    EXPECT_TRUE(i_num::IsApproxZero(0.0));
    EXPECT_TRUE(i_num::IsApproxZero(1e-14));
    EXPECT_FALSE(i_num::IsApproxZero(1.0));
    EXPECT_TRUE(i_num::IsApproxZero(-1e-14));
    EXPECT_FALSE(i_num::IsApproxZero(-1.0));
    EXPECT_TRUE(i_num::IsApproxZero(1e-6, 1e-5));
    EXPECT_FALSE(i_num::IsApproxZero(1e-4, 1e-5));
}

TEST(ToleranceTest, IsApproxOne) {
    EXPECT_TRUE(i_num::IsApproxOne(1.0));
    EXPECT_TRUE(i_num::IsApproxOne(1.0 + 1e-14));
    EXPECT_FALSE(i_num::IsApproxOne(0.0));
    EXPECT_TRUE(i_num::IsApproxOne(1.0 - 1e-14));
    EXPECT_FALSE(i_num::IsApproxOne(2.0));
    EXPECT_TRUE(i_num::IsApproxOne(1.0 + 1e-6, 1e-5));
    EXPECT_FALSE(i_num::IsApproxOne(1.0 + 1e-4, 1e-5));
}

TEST(ToleranceTest, IsApproxEqual) {
    EXPECT_TRUE(i_num::IsApproxEqual(1.0, 1.0));
    EXPECT_TRUE(i_num::IsApproxEqual(1.0, 1.0 + 1e-14));
    EXPECT_FALSE(i_num::IsApproxEqual(1.0, 0.0));
    EXPECT_TRUE(i_num::IsApproxEqual(1.0, 1.0 - 1e-14));
    EXPECT_FALSE(i_num::IsApproxEqual(1.0, 2.0));
    EXPECT_TRUE(i_num::IsApproxEqual(1.0, 1.0 + 1e-6, 1e-5));
    EXPECT_FALSE(i_num::IsApproxEqual(1.0, 1.0 + 1e-4, 1e-5));
}

TEST(ToleranceTest, IsApproxEqualMatrix) {
    igesio::Matrix3d a;
    a << 1.0, 2.0, 3.0,
         4.0, 5.0, 6.0,
         7.0, 8.0, 9.0;
    igesio::Matrix3d b = a;
    EXPECT_TRUE(i_num::IsApproxEqual(a, b));
    b(0, 0) += 1e-14;
    EXPECT_TRUE(i_num::IsApproxEqual(a, b));
    b(0, 0) += 1e-4;
    EXPECT_FALSE(i_num::IsApproxEqual(a, b));

    igesio::Matrix<double, 2, 3> c;
    c << 1.0, 2.0, 3.0,
         4.0, 5.0, 6.0;
    igesio::Matrix<double, 2, 3> d = c;
    EXPECT_TRUE(i_num::IsApproxEqual(c, d));
    d(0, 0) += 1e-14;
    EXPECT_TRUE(i_num::IsApproxEqual(c, d));
    d(0, 0) += 1e-4;
    EXPECT_FALSE(i_num::IsApproxEqual(c, d));
}

TEST(ToleranceTest, IsRotation_ValidRotations) {
    // 単位行列・軸平行回転・任意軸回転はいずれも回転行列
    EXPECT_TRUE(i_num::IsRotation(igesio::Matrix3d::Identity()));
    const igesio::Matrix3d rz = igesio::AngleAxisd(
            igesio::kPi / 2.0, igesio::Vector3d::UnitZ()).toRotationMatrix();
    EXPECT_TRUE(i_num::IsRotation(rz));
    const igesio::Matrix3d r = igesio::AngleAxisd(
            0.9, igesio::Vector3d(1.0, 2.0, 3.0).normalized()).toRotationMatrix();
    EXPECT_TRUE(i_num::IsRotation(r));
}

TEST(ToleranceTest, IsRotation_FloatRoundTripRegression) {
    // 報告バグの核心: doubleの回転行列を単精度へ丸めて戻すと R·Rᵀ−I に約1e-7の
    // 残差が出る. double機械ε尺度では非直交と誤判定されるが、float尺度では回転と
    // 認める. (この行列でピック時にBoundingBox::Rotateが例外化していた)
    const igesio::Matrix3d r = igesio::AngleAxisd(
            0.9, igesio::Vector3d(1.0, 2.0, 3.0).normalized()).toRotationMatrix();
    const igesio::Matrix3d r_f2d = r.cast<float>().cast<double>();

    // 旧来の厳密許容誤差(kParameterTolerance)では直交性検証に失敗する
    const igesio::Matrix3d gram = r_f2d * r_f2d.transpose();
    EXPECT_FALSE(i_num::IsApproxIdentity(gram));
    // float尺度のIsRotationは回転行列と認める
    EXPECT_TRUE(i_num::IsRotation(r_f2d));
}

TEST(ToleranceTest, IsRotation_NonRotations) {
    // 一様スケール(直交だがdet≠1)
    EXPECT_FALSE(i_num::IsRotation(igesio::Matrix3d::Identity() * 2.0));
    // 左手系(直交だがdet=-1): float往復でも符号は保たれるため確実に弾く
    igesio::Matrix3d left = igesio::Matrix3d::Identity();
    left(2, 2) = -1.0;
    EXPECT_FALSE(i_num::IsRotation(left));
    // せん断(det=1だが非直交)
    igesio::Matrix3d shear = igesio::Matrix3d::Identity();
    shear(0, 1) = 0.1;
    EXPECT_FALSE(i_num::IsRotation(shear));
    // ゼロ行列
    EXPECT_FALSE(i_num::IsRotation(igesio::Matrix3d::Zero()));
}

TEST(ToleranceTest, IsRotation_ToleranceBoundary) {
    // 非直交量(R·Rᵀの非対角成分)を許容誤差の直前/直後に置いて境界を検証する.
    // (0,1)成分にδを与えると R·Rᵀ の非対角がδになり、detは1のまま
    constexpr double kTol = 1e-5;
    igesio::Matrix3d inside = igesio::Matrix3d::Identity();
    inside(0, 1) = kTol;          // ちょうど境界 (比較は<=なので回転とみなす)
    EXPECT_TRUE(i_num::IsRotation(inside, kTol));

    igesio::Matrix3d outside = igesio::Matrix3d::Identity();
    outside(0, 1) = kTol * 2.0;   // 境界の外
    EXPECT_FALSE(i_num::IsRotation(outside, kTol));
}

TEST(ToleranceTest, NearestRotation_OrthonormalizesNearRotation) {
    // float往復で微小に非直交化した回転を、厳密な回転行列へ正規化する
    const igesio::Matrix3d r = igesio::AngleAxisd(
            0.9, igesio::Vector3d(1.0, 2.0, 3.0).normalized()).toRotationMatrix();
    const igesio::Matrix3d r_f2d = r.cast<float>().cast<double>();
    const igesio::Matrix3d snapped = i_num::NearestRotation(r_f2d);

    // 出力は厳密な許容誤差でも回転行列(直交かつdet=+1)
    EXPECT_TRUE(i_num::IsRotation(snapped, 1e-12));
    // 元の意図した回転に十分近い(補正量はfloat残差オーダー)
    EXPECT_TRUE(i_num::IsApproxEqual(snapped, r, 1e-5));
}

TEST(ToleranceTest, NearestRotation_IdempotentOnExactRotation) {
    // 厳密な回転行列はほぼ変化しない
    const igesio::Matrix3d r = igesio::AngleAxisd(
            0.7, igesio::Vector3d::UnitX()).toRotationMatrix();
    const igesio::Matrix3d snapped = i_num::NearestRotation(r);
    EXPECT_TRUE(i_num::IsApproxEqual(snapped, r, 1e-12));
}

TEST(ToleranceTest, NearestRotation_DegenerateZeroColumnReturnsInput) {
    // 補正不能な退化入力(ゼロ列)は入力をそのまま返す(契約)
    const igesio::Matrix3d zero = igesio::Matrix3d::Zero();
    EXPECT_TRUE(i_num::IsApproxEqual(i_num::NearestRotation(zero), zero));
}

TEST(ToleranceTest, TryClampToRange_WithinRange) {
    // 範囲内の値はそのまま返る
    auto r = i_num::TryClampToRange(0.5, 0.0, 1.0);
    ASSERT_TRUE(r.has_value());
    EXPECT_DOUBLE_EQ(*r, 0.5);
}

TEST(ToleranceTest, TryClampToRange_AtBoundary) {
    // 下端・上端ちょうどの値はその端点が返る
    auto lo = i_num::TryClampToRange(0.0, 0.0, 1.0);
    auto hi = i_num::TryClampToRange(1.0, 0.0, 1.0);
    ASSERT_TRUE(lo.has_value());
    ASSERT_TRUE(hi.has_value());
    EXPECT_DOUBLE_EQ(*lo, 0.0);
    EXPECT_DOUBLE_EQ(*hi, 1.0);
}

TEST(ToleranceTest, TryClampToRange_OvershootWithinToleranceIsClamped) {
    // 許容誤差内で僅かに域外へ出た境界値は端点へ丸められる
    // (報告事例: 代表v値が上端を1 ULP超過するケースに相当)
    const double hi = 1.0;
    const double over = std::nextafter(hi, 2.0);  // hi + 1 ULP
    auto ro = i_num::TryClampToRange(over, 0.0, hi);
    ASSERT_TRUE(ro.has_value());
    EXPECT_DOUBLE_EQ(*ro, hi);

    const double lo = 0.0;
    const double under = std::nextafter(lo, -1.0);  // lo - 1 ULP
    auto ru = i_num::TryClampToRange(under, lo, 1.0);
    ASSERT_TRUE(ru.has_value());
    EXPECT_DOUBLE_EQ(*ru, lo);
}

TEST(ToleranceTest, TryClampToRange_OutOfRangeReturnsNullopt) {
    // 許容誤差を明確に超える域外値はstd::nulloptを返す
    EXPECT_FALSE(i_num::TryClampToRange(1.0 + 1e-9, 0.0, 1.0).has_value());
    EXPECT_FALSE(i_num::TryClampToRange(-1e-9, 0.0, 1.0).has_value());
    EXPECT_FALSE(i_num::TryClampToRange(2.0, 0.0, 1.0).has_value());
}

TEST(ToleranceTest, TryClampToRange_InfiniteBoundsPassThrough) {
    // 無限端の範囲は任意の有限値を素通しする
    constexpr double kInf = std::numeric_limits<double>::infinity();
    auto r = i_num::TryClampToRange(1e10, -kInf, kInf);
    ASSERT_TRUE(r.has_value());
    EXPECT_DOUBLE_EQ(*r, 1e10);
}

TEST(ToleranceTest, TryClampToRange_CustomTolerance) {
    // 許容誤差を指定した場合の境界判定
    EXPECT_TRUE(i_num::TryClampToRange(1.0 + 1e-6, 0.0, 1.0, 1e-5).has_value());
    EXPECT_FALSE(i_num::TryClampToRange(1.0 + 1e-4, 0.0, 1.0, 1e-5).has_value());
}
