/**
 * @file tests/extensions/machines/core/test_rotation.cpp
 * @brief machines拡張の回転3形式と剛体変換 (core/rotation) のテスト
 * @author Yayoi Habami
 * @date 2026-09-08
 * @copyright 2026 Yayoi Habami
 * @note 対象: RotationAboutAxis / RotationFromEulerIjk / RotationFromColumns /
 *       ResolveRotation / MakeRigid / Translation / RotationAboutLine / RigidInverse /
 *       RotationPart / TranslationPart / ApplyPoint / ApplyDirection
 *       - 正常系: 軸角・オイラー[I,J,K]・軸ベクトル指定の3形式が同じ姿勢で一致、
 *         Rodriguesの式の性質 (軸の不動・直交性・det=+1)、オイラー角の適用順
 *         (Rz(K) Ry(J) Rx(I))、`ResolveRotation`の分岐。角度はrad
 *         (`kQuarterTurn`・`ToRadians`で与える)
 *       - 正常系 (境界値): 単位長の許容誤差1e-3の内側 (1+0.9e-3) は受理して再正規化、
 *         オイラー角ゼロ・並進ゼロ
 *       - 正常系 (退化): 恒等回転、軸上の点 (回転で不動)
 *       - 異常系: 単位長の許容誤差の外側 (1+1.1e-3)、ゼロ軸、非直交、鏡映 (行列式が負)
 *       - 剛体変換: MakeRigid/Translation/RigidInverseの往復、RotationAboutLineが
 *         軸上の点を不動点にすること、ApplyPoint/ApplyDirectionの並進の扱い
 */
#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <string>

#include "igesio/numerics/core/matrix.h"
#include "igesio/extensions/machines/core/rotation.h"
#include "igesio/extensions/machines/core/units.h"

namespace {

namespace mc = igesio::extensions::machines;
using igesio::Matrix3d;
using igesio::Matrix4d;
using igesio::Vector3d;
using mc::ToRadians;

/// @brief 数値比較の許容誤差
constexpr double kTol = 1e-12;

/// @brief 行列が単位行列に近いかを検証する
/// @param m 検証する行列
void ExpectIdentity(const Matrix3d& m) {
    EXPECT_TRUE(m.isApprox(Matrix3d::Identity(), 1e-12))
            << "actual:\n" << m;
}

/// @brief 例外メッセージに指定語が含まれることを検証しつつ呼び出す
/// @param fn 例外を投げるはずの処理
/// @param keyword メッセージに含まれるべき語
template <class Fn>
void ExpectInvalidArgumentContaining(Fn fn, const std::string& keyword) {
    try {
        fn();
        FAIL() << "std::invalid_argument was not thrown";
    } catch (const std::invalid_argument& e) {
        EXPECT_NE(std::string(e.what()).find(keyword), std::string::npos)
                << "message: " << e.what();
    }
}

}  // namespace



// ---- 回転3形式 ----

TEST(MachinesRotationTest, RotationAboutAxis_KeepsAxisAndIsOrthonormal) {
    const Vector3d axis(1.0, 2.0, -0.5);
    const Matrix3d r = mc::RotationAboutAxis(axis, ToRadians(37.0));
    // 軸は不動 (内部で正規化されるため、非単位ベクトルを渡しても成り立つ)
    EXPECT_TRUE((r * axis).isApprox(axis, kTol));
    ExpectIdentity(r.transpose() * r);
    EXPECT_NEAR(r.determinant(), 1.0, kTol);
}

TEST(MachinesRotationTest, RotationAboutAxis_QuarterTurnAboutZMapsXToY) {
    const Matrix3d r = mc::RotationAboutAxis(Vector3d::UnitZ(), mc::kQuarterTurn);
    EXPECT_TRUE((r * Vector3d::UnitX()).isApprox(Vector3d::UnitY(), kTol));
    EXPECT_TRUE((r * Vector3d::UnitY()).isApprox(-Vector3d::UnitX(), kTol));
}

TEST(MachinesRotationTest, RotationAboutAxis_ThrowsInvalidArgumentWhenAxisIsZero) {
    ExpectInvalidArgumentContaining(
            [] { mc::RotationAboutAxis(Vector3d::Zero(), ToRadians(10.0)); },
            "zero vector");
}

TEST(MachinesRotationTest, RotationFromEulerIjk_AppliesXThenYThenZ) {
    const Vector3d ijk(ToRadians(10.0), ToRadians(20.0), ToRadians(30.0));
    const Matrix3d expected =
            mc::RotationAboutAxis(Vector3d::UnitZ(), ToRadians(30.0))
            * mc::RotationAboutAxis(Vector3d::UnitY(), ToRadians(20.0))
            * mc::RotationAboutAxis(Vector3d::UnitX(), ToRadians(10.0));
    EXPECT_TRUE(mc::RotationFromEulerIjk(ijk).isApprox(expected, kTol));
}

TEST(MachinesRotationTest, RotationFromEulerIjk_ZeroIsIdentity) {
    ExpectIdentity(mc::RotationFromEulerIjk(Vector3d::Zero()));
}

TEST(MachinesRotationTest, ThreeForms_AgreeForRotationAboutZ) {
    const Matrix3d from_axis_angle =
            mc::RotationAboutAxis(Vector3d::UnitZ(), mc::kQuarterTurn);
    const Matrix3d from_euler =
            mc::RotationFromEulerIjk(Vector3d(0.0, 0.0, mc::kQuarterTurn));
    const Matrix3d from_columns = mc::RotationFromColumns(
            mc::ColumnsSpec{Vector3d(0.0, 1.0, 0.0), Vector3d(-1.0, 0.0, 0.0),
                            Vector3d(0.0, 0.0, 1.0)});
    EXPECT_TRUE(from_axis_angle.isApprox(from_euler, kTol));
    EXPECT_TRUE(from_axis_angle.isApprox(from_columns, kTol));
}

TEST(MachinesRotationTest, RotationFromColumns_RenormalizesJustInsideTolerance) {
    // ノルム1+0.9e-3は許容 (1e-3) の内側なので受理し、再正規化される
    const double s = 1.0 + 0.9e-3;
    const Matrix3d r = mc::RotationFromColumns(
            mc::ColumnsSpec{Vector3d(s, 0.0, 0.0), Vector3d(0.0, s, 0.0),
                            Vector3d(0.0, 0.0, s)});
    ExpectIdentity(r);
}

TEST(MachinesRotationTest,
     RotationFromColumns_ThrowsInvalidArgumentWhenColumnJustOutsideTolerance) {
    // ノルム1+1.1e-3は許容 (1e-3) の外側
    ExpectInvalidArgumentContaining(
            [] {
                mc::RotationFromColumns(mc::ColumnsSpec{
                        Vector3d(1.0 + 1.1e-3, 0.0, 0.0), Vector3d::UnitY(),
                        Vector3d::UnitZ()});
            },
            "x_axis");
}

TEST(MachinesRotationTest, RotationFromColumns_ThrowsInvalidArgumentWhenNotOrthogonal) {
    // 各列は単位長だが直交しない
    const Vector3d tilted = Vector3d(1.0, 0.1, 0.0).normalized();
    ExpectInvalidArgumentContaining(
            [tilted] {
                mc::RotationFromColumns(mc::ColumnsSpec{
                        Vector3d::UnitX(), tilted, Vector3d::UnitZ()});
            },
            "orthonormal");
}

TEST(MachinesRotationTest, RotationFromColumns_ThrowsInvalidArgumentWhenMirrored) {
    ExpectInvalidArgumentContaining(
            [] {
                mc::RotationFromColumns(mc::ColumnsSpec{
                        Vector3d::UnitX(), Vector3d::UnitY(),
                        -Vector3d::UnitZ()});
            },
            "mirrored");
}

TEST(MachinesRotationTest, ResolveRotation_DispatchesEachForm) {
    const double angle = ToRadians(45.0);
    const Matrix3d expected = mc::RotationAboutAxis(Vector3d::UnitX(), angle);
    const mc::RotationSpec axis_angle = mc::AxisAngleSpec{Vector3d::UnitX(), angle};
    const mc::RotationSpec euler = mc::EulerIjkSpec{Vector3d(angle, 0.0, 0.0)};
    const mc::RotationSpec columns = mc::ColumnsSpec{
            expected.col(0), expected.col(1), expected.col(2)};
    EXPECT_TRUE(mc::ResolveRotation(axis_angle).isApprox(expected, kTol));
    EXPECT_TRUE(mc::ResolveRotation(euler).isApprox(expected, kTol));
    EXPECT_TRUE(mc::ResolveRotation(columns).isApprox(expected, kTol));
}

// ---- 剛体変換 ----

TEST(MachinesRotationTest, MakeRigid_PlacesRotationAndTranslation) {
    const Matrix3d r = mc::RotationAboutAxis(Vector3d::UnitZ(), ToRadians(30.0));
    const Vector3d t(1.0, 2.0, 3.0);
    const Matrix4d m = mc::MakeRigid(r, t);
    EXPECT_TRUE(mc::RotationPart(m).isApprox(r, kTol));
    EXPECT_TRUE(mc::TranslationPart(m).isApprox(t, kTol));
    EXPECT_NEAR(m(3, 3), 1.0, kTol);
    EXPECT_TRUE(m.row(3).head<3>().isZero(kTol));
}

TEST(MachinesRotationTest, Translation_HasIdentityRotation) {
    const Matrix4d m = mc::Translation(Vector3d(4.0, -1.0, 0.5));
    ExpectIdentity(mc::RotationPart(m));
    EXPECT_TRUE(mc::TranslationPart(m).isApprox(Vector3d(4.0, -1.0, 0.5), kTol));
}

TEST(MachinesRotationTest, RigidInverse_RoundTripsToIdentity) {
    const Matrix4d m = mc::MakeRigid(
            mc::RotationAboutAxis(Vector3d(1.0, 1.0, 0.0), ToRadians(60.0)),
            Vector3d(10.0, -20.0, 30.0));
    EXPECT_TRUE((m * mc::RigidInverse(m)).isApprox(Matrix4d::Identity(), 1e-12));
    EXPECT_TRUE((mc::RigidInverse(m) * m).isApprox(Matrix4d::Identity(), 1e-12));
}

TEST(MachinesRotationTest, RotationAboutLine_FixesPointOnAxis) {
    const Vector3d direction(1.0, 0.0, 0.0);
    const Vector3d point(0.0, 0.0, 60.0);
    const Matrix4d m = mc::RotationAboutLine(direction, point, mc::kQuarterTurn);
    // 軸上の点は不動
    EXPECT_TRUE(mc::ApplyPoint(m, point).isApprox(point, kTol));
    EXPECT_TRUE(mc::ApplyPoint(m, point + 5.0 * direction)
                        .isApprox(point + 5.0 * direction, kTol));
    // 軸から離れた点は軸まわりに回る: (0, 1, 60) → (0, 0, 61)
    EXPECT_TRUE(mc::ApplyPoint(m, Vector3d(0.0, 1.0, 60.0))
                        .isApprox(Vector3d(0.0, 0.0, 61.0), kTol));
}

TEST(MachinesRotationTest, RotationAboutLine_ThrowsInvalidArgumentWhenDirectionIsZero) {
    ExpectInvalidArgumentContaining(
            [] {
                mc::RotationAboutLine(Vector3d::Zero(), Vector3d::Zero(),
                                      ToRadians(1.0));
            },
            "zero vector");
}

TEST(MachinesRotationTest, ApplyDirection_IgnoresTranslation) {
    const Matrix4d m = mc::MakeRigid(
            mc::RotationAboutAxis(Vector3d::UnitZ(), mc::kQuarterTurn),
            Vector3d(7.0, 8.0, 9.0));
    EXPECT_TRUE(mc::ApplyDirection(m, Vector3d::UnitX()).isApprox(Vector3d::UnitY(), kTol));
    EXPECT_TRUE(mc::ApplyPoint(m, Vector3d::UnitX())
                        .isApprox(Vector3d(7.0, 9.0, 9.0), kTol));
}
