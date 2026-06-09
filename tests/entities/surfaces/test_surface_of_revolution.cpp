/**
 * @file tests/entities/surfaces/test_surface_of_revolution.cpp
 * @brief SurfaceOfRevolution (Type 120) のテスト
 * @author Yayoi Habami
 * @date 2026-06-05
 * @copyright 2026 Yayoi Habami
 *
 * テスト対象:
 *   [コンストラクタ]
 *     SurfaceOfRevolution(axis, generatrix, SA, TA) のnull検証
 *   [ファクトリ関数]
 *     MakeSurfaceOfRevolution() (既存Line版・軸自動生成版)
 *   [データ取得・変更]
 *     SetAngleRange(), SetAxis(), SetGeneratrix(),
 *     HasAxis(), HasGeneratrix(), GetAngleRange(),
 *     GetChildIDs(), GetChildEntity()
 *   [ISurface実装]
 *     IsUClosed(), IsVClosed(), GetParameterRange(),
 *     TryGetDefinedDerivatives(), GetDefinedBoundingBox()
 *   [検証]
 *     ValidatePD() の角度制約 (0 < TA-SA <= 2π) と警告セマンティクス
 *
 * NOTE:
 *   - 幾何検証には単位円筒 (z軸周りに母線 (1,0,0)-(1,0,1) を回転;
 *     S(u,θ) = (cosθ, sinθ, u)) を用いる。解析的に厳密計算できるため、
 *     評価結果を直接検証できる
 *
 * TODO:
 *   - 数値微分との整合などISurface共通の網羅的検証は
 *     test_i_surface.cpp (surfaces_for_testing.h) の責務
 *   - 変換行列適用時 (Transform) の評価検証は対象外 (test_i_surface側)
 *   - GetDefinedBoundingBoxの部分回転ケース (主要角度クランプ) は未検証
 *     (全周回転の代表値のみ)
 */
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/common/iges_parameter_vector.h"
#include "igesio/common/validation_result.h"
#include "igesio/entities/curves/circular_arc.h"
#include "igesio/entities/curves/line.h"
#include "igesio/entities/curves/linear_path.h"
#include "igesio/entities/surfaces/surface_of_revolution.h"

namespace {

namespace i_ent = igesio::entities;
using igesio::Vector3d;
using igesio::kPi;
using igesio::ValidationSeverity;
using i_ent::SurfaceOfRevolution;
/// @brief 浮動小数点比較の許容誤差
constexpr double kTol = 1e-9;
/// @brief 2πの十進近似値 (真の2πをわずかに上回る; CAD出力で頻出する値)
constexpr double kTwoPiDecimal = 6.28318530717959;



/**
 * 検証ヘルパー
 */

/// @brief Vector3dの成分一致を検証する
void ExpectVectorNear(const Vector3d& actual, const Vector3d& expected,
                      const double tol = kTol) {
    EXPECT_NEAR(actual.x(), expected.x(), tol);
    EXPECT_NEAR(actual.y(), expected.y(), tol);
    EXPECT_NEAR(actual.z(), expected.z(), tol);
}

/// @brief 曲面上の点S(u,v)を評価する (TryGetDefinedDerivativesの0階)
Vector3d EvalAt(const i_ent::ISurface& surface,
                const double u, const double v) {
    const auto derivs = surface.TryGetDefinedDerivatives(u, v, 0);
    EXPECT_TRUE(derivs.has_value())
        << "evaluation failed at (u, v) = (" << u << ", " << v << ")";
    return derivs ? (*derivs)(0, 0) : Vector3d::Zero();
}



/**
 * テスト用インスタンスのファクトリ関数
 */

/// @brief 回転軸: z軸 (原点から+z方向) のLineを作成する
std::shared_ptr<i_ent::Line> MakeZAxis() {
    return i_ent::MakeLine(Vector3d{0., 0., 0.}, Vector3d{0., 0., 1.});
}

/// @brief 母線: 線分 (1,0,0)-(1,0,1) を作成する
/// @note z軸周りの回転で単位円筒 S(u,θ) = (cosθ, sinθ, u) となる
///       (u∈[0,1]は母線のパラメータ、θは回転角)
std::shared_ptr<i_ent::Line> MakeCylinderGeneratrix() {
    return i_ent::MakeLine(Vector3d{1., 0., 0.}, Vector3d{1., 0., 1.});
}

/// @brief 単位円筒 (z軸周りに母線 (1,0,0)-(1,0,1) を回転) を作成する
/// @param start_angle 回転の開始角度SA [rad]
/// @param end_angle 回転の終了角度TA [rad]
std::shared_ptr<SurfaceOfRevolution> MakeUnitCylinder(
        const double start_angle = 0.0, const double end_angle = 2.0 * kPi) {
    return i_ent::MakeSurfaceOfRevolution(
        MakeZAxis(), MakeCylinderGeneratrix(), start_angle, end_angle);
}

/// @brief ポインタ未解決のSurfaceOfRevolutionを作成する
/// @note PDパラメータにIDのみを渡し、参照解決を行わない
///       (ファイル読込中の中間状態に相当)
std::shared_ptr<SurfaceOfRevolution> MakeUnresolvedSurface() {
    const auto axis = MakeZAxis();
    const auto generatrix = MakeCylinderGeneratrix();
    return std::make_shared<SurfaceOfRevolution>(
        igesio::IGESParameterVector{
            axis->GetID(), generatrix->GetID(), 0.0, 2.0 * kPi});
}

}  // namespace



/**
 * コンストラクタのテスト (null検証の回帰; 修正前は委譲初期化式での
 * GetID()参照により未定義動作だった)
 */

TEST(SurfaceOfRevolutionCtorTest,
     Constructor_ThrowsInvalidArgumentWhenAxisIsNull) {
    EXPECT_THROW(SurfaceOfRevolution(nullptr, MakeCylinderGeneratrix()),
                 std::invalid_argument);
}

TEST(SurfaceOfRevolutionCtorTest,
     Constructor_ThrowsInvalidArgumentWhenGeneratrixIsNull) {
    EXPECT_THROW(SurfaceOfRevolution(MakeZAxis(), nullptr),
                 std::invalid_argument);
}

// コンストラクタは角度を検証しない (ファイル読込経路では不正値も
// 受け入れ、検証はValidatePDが警告として行う)
TEST(SurfaceOfRevolutionCtorTest, Constructor_AcceptsInvalidAngles) {
    EXPECT_NO_THROW(SurfaceOfRevolution(
        MakeZAxis(), MakeCylinderGeneratrix(), kPi, kPi));
}



/**
 * MakeSurfaceOfRevolution (既存Line版) のテスト
 */

TEST(SurfaceOfRevolutionFactoryTest, MakeSurfaceOfRevolution_ResolvedReferences) {
    const auto axis = MakeZAxis();
    const auto generatrix = MakeCylinderGeneratrix();
    const auto surface =
        i_ent::MakeSurfaceOfRevolution(axis, generatrix, 0.0, kPi);

    ASSERT_NE(surface, nullptr);
    EXPECT_TRUE(surface->HasAxis());
    EXPECT_TRUE(surface->HasGeneratrix());
    EXPECT_EQ(surface->GetAxis()->GetID(), axis->GetID());
    EXPECT_EQ(surface->GetGeneratrix()->GetID(), generatrix->GetID());
}

TEST(SurfaceOfRevolutionFactoryTest,
     MakeSurfaceOfRevolution_DefaultAnglesAreFullRotation) {
    const auto surface = i_ent::MakeSurfaceOfRevolution(
        MakeZAxis(), MakeCylinderGeneratrix());

    const auto range = surface->GetAngleRange();
    EXPECT_NEAR(range[0], 0.0, kTol);
    EXPECT_NEAR(range[1], 2.0 * kPi, kTol);
    EXPECT_TRUE(surface->IsVClosed());
}

TEST(SurfaceOfRevolutionFactoryTest,
     MakeSurfaceOfRevolution_SetsPhysicallyDependentOnChildren) {
    const auto axis = MakeZAxis();
    const auto generatrix = MakeCylinderGeneratrix();
    const auto surface = i_ent::MakeSurfaceOfRevolution(axis, generatrix);

    EXPECT_EQ(axis->GetSubordinateEntitySwitch(),
              i_ent::SubordinateEntitySwitch::kPhysicallyDependent);
    EXPECT_EQ(generatrix->GetSubordinateEntitySwitch(),
              i_ent::SubordinateEntitySwitch::kPhysicallyDependent);
}

TEST(SurfaceOfRevolutionFactoryTest,
     MakeSurfaceOfRevolution_ThrowsInvalidArgumentWhenAxisIsNull) {
    EXPECT_THROW(
        i_ent::MakeSurfaceOfRevolution(nullptr, MakeCylinderGeneratrix()),
        std::invalid_argument);
}

TEST(SurfaceOfRevolutionFactoryTest,
     MakeSurfaceOfRevolution_ThrowsInvalidArgumentWhenGeneratrixIsNull) {
    EXPECT_THROW(i_ent::MakeSurfaceOfRevolution(MakeZAxis(), nullptr),
                 std::invalid_argument);
}

// 規格 (0 < TA-SA <= 2π) に適合する角度範囲はすべて受理される
TEST(SurfaceOfRevolutionFactoryTest,
     MakeSurfaceOfRevolution_AcceptsSpecConformantAngles) {
    const auto axis = MakeZAxis();
    const auto generatrix = MakeCylinderGeneratrix();

    // 負の開始角 (規格はSA・TAの絶対値を制約しない)
    EXPECT_NO_THROW(i_ent::MakeSurfaceOfRevolution(
        axis, generatrix, -kPi / 2, kPi / 2));
    // 2πを超える終了角 (差は2π以下)
    EXPECT_NO_THROW(i_ent::MakeSurfaceOfRevolution(
        axis, generatrix, 3 * kPi / 2, 3 * kPi));
    // 差がちょうど2π (上限境界の内側)
    EXPECT_NO_THROW(i_ent::MakeSurfaceOfRevolution(
        axis, generatrix, kPi / 4, kPi / 4 + 2.0 * kPi));
    // 2πの十進近似 (真の2πをわずかに超えるが許容誤差内)
    EXPECT_NO_THROW(i_ent::MakeSurfaceOfRevolution(
        axis, generatrix, 0.0, kTwoPiDecimal));
    // 微小な正の差 (下限境界の内側)
    EXPECT_NO_THROW(i_ent::MakeSurfaceOfRevolution(
        axis, generatrix, 0.0, 1e-6));
}

TEST(SurfaceOfRevolutionFactoryTest,
     MakeSurfaceOfRevolution_ThrowsEntityValueErrorWhenSweepNotPositive) {
    const auto axis = MakeZAxis();
    const auto generatrix = MakeCylinderGeneratrix();

    // 差がゼロ (下限境界の外側)
    EXPECT_THROW(i_ent::MakeSurfaceOfRevolution(axis, generatrix, kPi, kPi),
                 igesio::EntityValueError);
    // 開始角 > 終了角
    EXPECT_THROW(i_ent::MakeSurfaceOfRevolution(axis, generatrix, 1.0, 0.5),
                 igesio::EntityValueError);
}

TEST(SurfaceOfRevolutionFactoryTest,
     MakeSurfaceOfRevolution_ThrowsEntityValueErrorWhenSweepExceedsTwoPi) {
    // 差が2π + 1e-9 (上限境界の外側; 許容誤差を超える)
    EXPECT_THROW(
        i_ent::MakeSurfaceOfRevolution(
            MakeZAxis(), MakeCylinderGeneratrix(), 0.0, 2.0 * kPi + 1e-9),
        igesio::EntityValueError);
}

// 単位円筒の評価: S(u,θ) = (cosθ, sinθ, u)
TEST(SurfaceOfRevolutionFactoryTest, MakeSurfaceOfRevolution_EvalMatchesCylinder) {
    const auto surface = MakeUnitCylinder();

    ExpectVectorNear(EvalAt(*surface, 0.0, 0.0), Vector3d(1.0, 0.0, 0.0));
    ExpectVectorNear(EvalAt(*surface, 0.5, kPi / 2), Vector3d(0.0, 1.0, 0.5));
    ExpectVectorNear(EvalAt(*surface, 1.0, kPi), Vector3d(-1.0, 0.0, 1.0));
}



/**
 * MakeSurfaceOfRevolution (軸自動生成版) のテスト
 */

TEST(SurfaceOfRevolutionFactoryTest,
     MakeSurfaceOfRevolution_AutoAxis_ReturnsBodyAndGeneratedAxis) {
    const auto [surface, axis] = i_ent::MakeSurfaceOfRevolution(
        Vector3d{0., 0., 0.}, Vector3d{0., 0., 1.}, MakeCylinderGeneratrix());

    ASSERT_NE(surface, nullptr);
    ASSERT_NE(axis, nullptr);
    EXPECT_EQ(surface->GetAxis()->GetID(), axis->GetID());
    EXPECT_TRUE(surface->HasGeneratrix());
}

TEST(SurfaceOfRevolutionFactoryTest,
     MakeSurfaceOfRevolution_AutoAxis_AxisAnchorPointsMatchInput) {
    const Vector3d point{1., 2., 3.};
    const Vector3d direction{0., 1., 1.};
    const auto [surface, axis] = i_ent::MakeSurfaceOfRevolution(
        point, direction, MakeCylinderGeneratrix());

    const auto [start, end] = axis->GetAnchorPoints();
    ExpectVectorNear(start, point);
    ExpectVectorNear(end, point + direction);
}

TEST(SurfaceOfRevolutionFactoryTest,
     MakeSurfaceOfRevolution_AutoAxis_SetsPhysicallyDependentOnAxis) {
    const auto [surface, axis] = i_ent::MakeSurfaceOfRevolution(
        Vector3d{0., 0., 0.}, Vector3d{0., 0., 1.}, MakeCylinderGeneratrix());

    EXPECT_EQ(axis->GetSubordinateEntitySwitch(),
              i_ent::SubordinateEntitySwitch::kPhysicallyDependent);
}

TEST(SurfaceOfRevolutionFactoryTest,
     MakeSurfaceOfRevolution_AutoAxis_ThrowsEntityValueErrorWhenDirectionZero) {
    // ゼロベクトル
    EXPECT_THROW(
        i_ent::MakeSurfaceOfRevolution(
            Vector3d{0., 0., 0.}, Vector3d::Zero(), MakeCylinderGeneratrix()),
        igesio::EntityValueError);
    // ほぼゼロ (kGeometryTolerance = 1e-9 以下; 境界の外側)
    EXPECT_THROW(
        i_ent::MakeSurfaceOfRevolution(
            Vector3d{0., 0., 0.}, Vector3d{0., 0., 1e-12},
            MakeCylinderGeneratrix()),
        igesio::EntityValueError);
}

TEST(SurfaceOfRevolutionFactoryTest,
     MakeSurfaceOfRevolution_AutoAxis_AcceptsSmallButNonZeroDirection) {
    // 許容誤差より大きい微小方向ベクトル (境界の内側)
    EXPECT_NO_THROW(i_ent::MakeSurfaceOfRevolution(
        Vector3d{0., 0., 0.}, Vector3d{0., 0., 1e-6},
        MakeCylinderGeneratrix()));
}

TEST(SurfaceOfRevolutionFactoryTest,
     MakeSurfaceOfRevolution_AutoAxis_ThrowsInvalidArgumentWhenGeneratrixNull) {
    EXPECT_THROW(
        i_ent::MakeSurfaceOfRevolution(
            Vector3d{0., 0., 0.}, Vector3d{0., 0., 1.}, nullptr),
        std::invalid_argument);
}

TEST(SurfaceOfRevolutionFactoryTest,
     MakeSurfaceOfRevolution_AutoAxis_ThrowsEntityValueErrorWhenAnglesInvalid) {
    EXPECT_THROW(
        i_ent::MakeSurfaceOfRevolution(
            Vector3d{0., 0., 0.}, Vector3d{0., 0., 1.},
            MakeCylinderGeneratrix(), kPi, kPi),
        igesio::EntityValueError);
}

// 軸自動生成版は、同じ軸を明示的に渡した場合と同一の曲面を生成する
TEST(SurfaceOfRevolutionFactoryTest,
     MakeSurfaceOfRevolution_AutoAxis_GeometryMatchesExplicitAxisVersion) {
    const auto explicit_version = MakeUnitCylinder();
    const auto [auto_version, axis] = i_ent::MakeSurfaceOfRevolution(
        Vector3d{0., 0., 0.}, Vector3d{0., 0., 1.}, MakeCylinderGeneratrix());

    ExpectVectorNear(EvalAt(*auto_version, 0.5, kPi / 3),
                     EvalAt(*explicit_version, 0.5, kPi / 3));
}



/**
 * データ取得・変更のテスト
 */

TEST(SurfaceOfRevolutionSetterTest, SetAngleRange_UpdatesAngleRange) {
    const auto surface = MakeUnitCylinder();
    surface->SetAngleRange(0.1, 1.2);

    const auto range = surface->GetAngleRange();
    EXPECT_NEAR(range[0], 0.1, kTol);
    EXPECT_NEAR(range[1], 1.2, kTol);
}

// 規格 (0 < TA-SA <= 2π) に適合する角度範囲はすべて受理される
TEST(SurfaceOfRevolutionSetterTest, SetAngleRange_AcceptsSpecConformantRanges) {
    const auto surface = MakeUnitCylinder();

    // 負の開始角
    EXPECT_NO_THROW(surface->SetAngleRange(-kPi, kPi));
    // 2πを超える終了角 (差は2π以下)
    EXPECT_NO_THROW(surface->SetAngleRange(3 * kPi / 2, 3 * kPi));
    // 差がちょうど2π (上限境界の内側)
    EXPECT_NO_THROW(surface->SetAngleRange(kPi / 4, kPi / 4 + 2.0 * kPi));
    // 2πの十進近似 (許容誤差内)
    EXPECT_NO_THROW(surface->SetAngleRange(0.0, kTwoPiDecimal));
    // 微小な正の差 (下限境界の内側)
    EXPECT_NO_THROW(surface->SetAngleRange(0.0, 1e-6));
}

TEST(SurfaceOfRevolutionSetterTest,
     SetAngleRange_ThrowsEntityValueErrorWhenSweepNotPositive) {
    const auto surface = MakeUnitCylinder();

    // 差がゼロ (下限境界の外側)
    EXPECT_THROW(surface->SetAngleRange(kPi, kPi), igesio::EntityValueError);
    // 開始角 > 終了角
    EXPECT_THROW(surface->SetAngleRange(1.0, 0.5), igesio::EntityValueError);
}

TEST(SurfaceOfRevolutionSetterTest,
     SetAngleRange_ThrowsEntityValueErrorWhenSweepExceedsTwoPi) {
    const auto surface = MakeUnitCylinder();
    // 差が2π + 1e-9 (上限境界の外側; 許容誤差を超える)
    EXPECT_THROW(surface->SetAngleRange(0.0, 2.0 * kPi + 1e-9),
                 igesio::EntityValueError);
}

TEST(SurfaceOfRevolutionSetterTest, SetAngleRange_StateUnchangedOnFailure) {
    const auto surface = MakeUnitCylinder();
    surface->SetAngleRange(0.1, 0.9);

    EXPECT_THROW(surface->SetAngleRange(2.0, 1.0), igesio::EntityValueError);

    // 失敗時に角度範囲は変更されない
    const auto range = surface->GetAngleRange();
    EXPECT_NEAR(range[0], 0.1, kTol);
    EXPECT_NEAR(range[1], 0.9, kTol);
}

TEST(SurfaceOfRevolutionSetterTest, SetAxis_ThrowsInvalidArgumentWhenNull) {
    const auto surface = MakeUnitCylinder();
    EXPECT_THROW(surface->SetAxis(nullptr), std::invalid_argument);
}

TEST(SurfaceOfRevolutionSetterTest,
     SetGeneratrix_ThrowsInvalidArgumentWhenNull) {
    const auto surface = MakeUnitCylinder();
    EXPECT_THROW(surface->SetGeneratrix(nullptr), std::invalid_argument);
}

TEST(SurfaceOfRevolutionSetterTest, SetAxis_ReplacesAxisAndSetsDependentFlag) {
    const auto surface = MakeUnitCylinder();
    const auto new_axis = i_ent::MakeLine(
        Vector3d{0., 0., 0.}, Vector3d{1., 0., 0.});
    surface->SetAxis(new_axis);

    EXPECT_EQ(surface->GetAxis()->GetID(), new_axis->GetID());
    EXPECT_EQ(new_axis->GetSubordinateEntitySwitch(),
              i_ent::SubordinateEntitySwitch::kPhysicallyDependent);
}

TEST(SurfaceOfRevolutionSetterTest,
     SetGeneratrix_ReplacesGeneratrixAndSetsDependentFlag) {
    const auto surface = MakeUnitCylinder();
    const auto new_generatrix = i_ent::MakeLine(
        Vector3d{2., 0., 0.}, Vector3d{2., 0., 1.});
    surface->SetGeneratrix(new_generatrix);

    EXPECT_EQ(surface->GetGeneratrix()->GetID(), new_generatrix->GetID());
    EXPECT_EQ(new_generatrix->GetSubordinateEntitySwitch(),
              i_ent::SubordinateEntitySwitch::kPhysicallyDependent);
}

TEST(SurfaceOfRevolutionChildTest, GetChildIDs_ContainsAxisAndGeneratrix) {
    const auto axis = MakeZAxis();
    const auto generatrix = MakeCylinderGeneratrix();
    const auto surface = i_ent::MakeSurfaceOfRevolution(axis, generatrix);

    const auto ids = surface->GetChildIDs();
    ASSERT_EQ(ids.size(), 2u);
    EXPECT_EQ(ids[0], axis->GetID());
    EXPECT_EQ(ids[1], generatrix->GetID());
}

TEST(SurfaceOfRevolutionChildTest, GetChildEntity_ReturnsCorrespondingEntity) {
    const auto axis = MakeZAxis();
    const auto generatrix = MakeCylinderGeneratrix();
    const auto surface = i_ent::MakeSurfaceOfRevolution(axis, generatrix);

    const auto axis_child = surface->GetChildEntity(axis->GetID());
    ASSERT_NE(axis_child, nullptr);
    EXPECT_EQ(axis_child->GetID(), axis->GetID());

    const auto gen_child = surface->GetChildEntity(generatrix->GetID());
    ASSERT_NE(gen_child, nullptr);
    EXPECT_EQ(gen_child->GetID(), generatrix->GetID());
}

TEST(SurfaceOfRevolutionChildTest, GetChildEntity_ReturnsNullptrForUnknownID) {
    const auto surface = MakeUnitCylinder();
    EXPECT_EQ(surface->GetChildEntity(surface->GetID()), nullptr);
}

TEST(SurfaceOfRevolutionAccessorTest,
     HasAxisAndHasGeneratrix_FalseWhenUnresolved) {
    const auto surface = MakeUnresolvedSurface();

    EXPECT_FALSE(surface->HasAxis());
    EXPECT_FALSE(surface->HasGeneratrix());
    EXPECT_THROW(surface->GetAxis(), igesio::ReferenceError);
    EXPECT_THROW(surface->GetGeneratrix(), igesio::ReferenceError);
}



/**
 * 閉性・パラメータ範囲のテスト
 */

// 全周回転の判定は開始角によらず差 (TA-SA = 2π) のみで決まる
TEST(SurfaceOfRevolutionClosureTest,
     IsVClosed_TrueWhenSweepIsFullRotationFromNonZeroStart) {
    const auto surface = MakeUnitCylinder(kPi / 4, kPi / 4 + 2.0 * kPi);
    EXPECT_TRUE(surface->IsVClosed());
}

TEST(SurfaceOfRevolutionClosureTest,
     IsVClosed_TrueWhenSweepIsFullRotationFromZero) {
    const auto surface = MakeUnitCylinder(0.0, 2.0 * kPi);
    EXPECT_TRUE(surface->IsVClosed());
}

TEST(SurfaceOfRevolutionClosureTest, IsVClosed_FalseWhenPartialRotation) {
    const auto surface = MakeUnitCylinder(0.0, kPi);
    EXPECT_FALSE(surface->IsVClosed());
}

TEST(SurfaceOfRevolutionClosureTest, IsUClosed_FalseWhenGeneratrixIsOpen) {
    const auto surface = MakeUnitCylinder();
    EXPECT_FALSE(surface->IsUClosed());
}

TEST(SurfaceOfRevolutionClosureTest, IsUClosed_TrueWhenGeneratrixIsClosed) {
    // 母線: z軸から離れた位置の閉円 (z=0平面、中心(2,0)、半径0.5)
    const auto circle = i_ent::MakeCircle(igesio::Vector2d{2., 0.}, 0.5);
    const auto surface =
        i_ent::MakeSurfaceOfRevolution(MakeZAxis(), circle, 0.0, kPi);
    EXPECT_TRUE(surface->IsUClosed());
}

TEST(SurfaceOfRevolutionClosureTest,
     GetParameterRange_CombinesGeneratrixRangeAndAngles) {
    const auto surface = MakeUnitCylinder(kPi / 2, kPi);

    // u: 母線 (Line; [0,1])、v: 回転角度範囲
    const auto range = surface->GetParameterRange();
    EXPECT_NEAR(range[0], 0.0, kTol);
    EXPECT_NEAR(range[1], 1.0, kTol);
    EXPECT_NEAR(range[2], kPi / 2, kTol);
    EXPECT_NEAR(range[3], kPi, kTol);
}



/**
 * ValidatePDのテスト (角度制約 0 < TA-SA <= 2π)
 */

TEST(SurfaceOfRevolutionValidateTest, Validate_ValidSurface_NoErrors) {
    const auto surface = MakeUnitCylinder();
    const auto result = surface->Validate();

    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.errors.empty()) << result.Message();
}

// 負の開始角は規格適合であり、警告も出さない (読込経路の回帰)
TEST(SurfaceOfRevolutionValidateTest,
     Validate_NoWarningWhenStartAngleNegative) {
    const auto surface = SurfaceOfRevolution(
        MakeZAxis(), MakeCylinderGeneratrix(), -kPi / 2, kPi / 2);
    const auto result = surface.Validate();

    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.errors.empty()) << result.Message();
}

// 2πの十進近似による全周回転に警告を出さない (CAD出力の回帰)
TEST(SurfaceOfRevolutionValidateTest,
     Validate_NoWarningWhenDecimalTwoPiSweep) {
    const auto surface = SurfaceOfRevolution(
        MakeZAxis(), MakeCylinderGeneratrix(), 0.0, kTwoPiDecimal);
    const auto result = surface.Validate();

    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.errors.empty()) << result.Message();
}

// 退化した角度範囲 (TA == SA) は警告のみで、使用はブロックしない
TEST(SurfaceOfRevolutionValidateTest, Validate_WarnsWhenSweepDegenerate) {
    const auto surface = SurfaceOfRevolution(
        MakeZAxis(), MakeCylinderGeneratrix(), kPi, kPi);
    const auto result = surface.Validate();

    EXPECT_TRUE(result.is_valid);
    ASSERT_EQ(result.errors.size(), 1u) << result.Message();
    EXPECT_EQ(result.errors[0].severity, ValidationSeverity::kWarning);
}

// 過回転 (TA-SA > 2π) は警告のみで、使用はブロックしない
TEST(SurfaceOfRevolutionValidateTest, Validate_WarnsWhenSweepExceedsTwoPi) {
    const auto surface = SurfaceOfRevolution(
        MakeZAxis(), MakeCylinderGeneratrix(), 0.0, 3.0 * kPi);
    const auto result = surface.Validate();

    EXPECT_TRUE(result.is_valid);
    ASSERT_EQ(result.errors.size(), 1u) << result.Message();
    EXPECT_EQ(result.errors[0].severity, ValidationSeverity::kWarning);
}

TEST(SurfaceOfRevolutionValidateTest, Validate_FailsWhenPointersUnresolved) {
    const auto surface = MakeUnresolvedSurface();
    const auto result = surface->Validate();

    EXPECT_FALSE(result.is_valid);
}



/**
 * 幾何のテスト (単位円筒 S(u,θ) = (cosθ, sinθ, u) による解析検証)
 */

TEST(SurfaceOfRevolutionGeometryTest,
     TryGetDefinedDerivatives_FirstOrderMatchesCylinder) {
    const auto surface = MakeUnitCylinder();
    const double theta = kPi / 3;

    const auto derivs = surface->TryGetDefinedDerivatives(0.5, theta, 1);
    ASSERT_TRUE(derivs.has_value());
    // S(u,θ) = (cosθ, sinθ, u)
    ExpectVectorNear((*derivs)(0, 0),
                     Vector3d(std::cos(theta), std::sin(theta), 0.5));
    // Su = (0, 0, 1)
    ExpectVectorNear((*derivs)(1, 0), Vector3d(0.0, 0.0, 1.0));
    // Sθ = (-sinθ, cosθ, 0)
    ExpectVectorNear((*derivs)(0, 1),
                     Vector3d(-std::sin(theta), std::cos(theta), 0.0));
}

// 負の角度範囲の曲面も評価できる (角度制約緩和後の回帰)
TEST(SurfaceOfRevolutionGeometryTest,
     TryGetDefinedDerivatives_EvaluatesAtNegativeAngle) {
    const auto surface = MakeUnitCylinder(-kPi / 2, kPi / 2);
    ExpectVectorNear(EvalAt(*surface, 0.5, -kPi / 2),
                     Vector3d(0.0, -1.0, 0.5));
}

TEST(SurfaceOfRevolutionGeometryTest,
     TryGetDefinedDerivatives_ReturnsNulloptOutsideAngleRange) {
    const auto surface = MakeUnitCylinder(0.0, kPi);

    EXPECT_FALSE(surface->TryGetDefinedDerivatives(0.5, kPi + 0.1, 0)
                     .has_value());
    EXPECT_FALSE(surface->TryGetDefinedDerivatives(0.5, -0.1, 0).has_value());
}

TEST(SurfaceOfRevolutionGeometryTest,
     TryGetDefinedDerivatives_ReturnsNulloptOutsideGeneratrixRange) {
    const auto surface = MakeUnitCylinder();
    EXPECT_FALSE(surface->TryGetDefinedDerivatives(1.5, kPi, 0).has_value());
}

TEST(SurfaceOfRevolutionGeometryTest, GetDefinedBoundingBox_FullRotation) {
    const auto surface = MakeUnitCylinder();

    // 単位円筒のバウンディングボックスは [-1,1] x [-1,1] x [0,1]
    const auto vertices = surface->GetDefinedBoundingBox().GetVertices();
    ASSERT_FALSE(vertices.empty());
    Vector3d min_v =
        Vector3d::Constant(std::numeric_limits<double>::infinity());
    Vector3d max_v =
        Vector3d::Constant(-std::numeric_limits<double>::infinity());
    for (const auto& v : vertices) {
        min_v = min_v.cwiseMin(v);
        max_v = max_v.cwiseMax(v);
    }
    ExpectVectorNear(min_v, Vector3d(-1.0, -1.0, 0.0));
    ExpectVectorNear(max_v, Vector3d(1.0, 1.0, 1.0));
}



/**
 * GetUCreaseParameters() のテスト
 *
 * u方向は母線のパラメータに一致するため、母線の角点をそのまま返す.
 */

// 母線が角を持つ折れ線の場合、その角点パラメータ (母線のGetCornerParams) を返す
TEST(SurfaceOfRevolutionCreaseTest,
     GetUCreaseParameters_ReturnsGeneratrixCornersWhenPolyline) {
    // 母線: 折れ線 (1,0,0)-(1,0,2)-(2,0,2) (xz平面内, z軸を含む平面)
    // 角は中央頂点; LinearPathは弧長パラメータのため累積長 = 2.0
    auto generatrix = i_ent::MakeLinearPath(std::vector<Vector3d>{
        Vector3d{1., 0., 0.}, Vector3d{1., 0., 2.}, Vector3d{2., 0., 2.}});
    auto surf = i_ent::MakeSurfaceOfRevolution(
        MakeZAxis(), generatrix, 0., 2. * kPi);

    const auto creases = surf->GetUCreaseParameters();
    const auto expected = generatrix->GetCornerParams();
    ASSERT_EQ(creases.size(), 1u);
    ASSERT_EQ(expected.size(), 1u);
    EXPECT_NEAR(creases[0], expected[0], kTol);  // 母線の角点を素通し
    EXPECT_NEAR(creases[0], 2.0, kTol);          // 累積長 (1,0,0)->(1,0,2)
}

// 滑らかな母線 (直線) の場合は空 (角なし)
TEST(SurfaceOfRevolutionCreaseTest,
     GetUCreaseParameters_EmptyWhenSmoothGeneratrix) {
    EXPECT_TRUE(MakeUnitCylinder()->GetUCreaseParameters().empty());
}

// 母線ポインタ未解決の場合は空 (throwしない)
TEST(SurfaceOfRevolutionCreaseTest,
     GetUCreaseParameters_EmptyWhenGeneratrixUnset) {
    EXPECT_TRUE(MakeUnresolvedSurface()->GetUCreaseParameters().empty());
}
