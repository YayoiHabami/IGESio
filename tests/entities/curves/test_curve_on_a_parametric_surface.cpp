/**
 * @file tests/entities/curves/test_curve_on_a_parametric_surface.cpp
 * @brief CurveOnAParametricSurface (Type 142) のテスト
 * @author Yayoi Habami
 * @date 2026-06-05
 * @copyright 2026 Yayoi Habami
 *
 * テスト対象:
 *   [コンストラクタ]
 *     CurveOnAParametricSurface(S, B, C) のnull検証
 *   [ファクトリ関数]
 *     MakeCurveOnAParametricSurface() (C自動生成版・C明示指定版),
 *     MakeIsoparametricCurve()
 *   [データ取得・変更]
 *     SetCreationType(), SetPreferredRepresentation(),
 *     HasBaseCurve(), IsBaseCurveOmitted(), SetCurves()
 *   [ベース曲線の再構築]
 *     ReconstructOmittedBaseCurve()
 *
 * NOTE:
 *   - 曲面フィクスチャは双一次B-Spline S(u,v) = (2u, v, 0) (定義域[0,1]²)
 *     を用いる。S(B(t))が解析的に厳密計算できるため、評価結果を直接検証できる
 *
 * TODO:
 *   - MakeIsoparametricCurveの非有界定義域での例外は、パラメータ定義域が
 *     有界でないISurface実装が現存しないためテスト不可
 *   - ICurve評価系 (Length, GetCornerParams等) の網羅は
 *     test_i_curve.cpp (curves_for_testing.h) の責務
 */
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/common/iges_parameter_vector.h"
#include "igesio/numerics/core/tolerance.h"
#include "igesio/entities/curves/curve_on_a_parametric_surface.h"
#include "igesio/entities/curves/line.h"
#include "igesio/entities/surfaces/rational_b_spline_surface.h"

namespace {

namespace i_ent = igesio::entities;
using igesio::Vector3d;
using i_ent::CurveOnSurface;
using i_ent::CurveCreationType;
using i_ent::PreferredRepresentation;
using i_ent::IsoparametricDirection;
/// @brief 浮動小数点比較の許容誤差
constexpr double kTol = 1e-9;
/// @brief 自動生成C・逆射影Bの近似誤差用の許容誤差
/// @note C生成側の近似許容 (1e-3) に余裕を持たせた値
constexpr double kApproxTol = 1e-2;



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

/// @brief 曲線上の点C(t)を評価する (TryGetDefinedDerivativesの0階)
Vector3d EvalAt(const i_ent::ICurve& curve, const double t) {
    const auto derivs = curve.TryGetDefinedDerivatives(t, 0);
    EXPECT_TRUE(derivs.has_value()) << "evaluation failed at t = " << t;
    return derivs ? (*derivs)[0] : Vector3d::Zero();
}



/**
 * テスト用インスタンスのファクトリ関数
 */

/// @brief 双一次B-Splineサーフェス S(u,v) = (2u, v, 0) を構築する
/// @note K1=K2=M1=M2=1、パラメータ定義域 [0,1]×[0,1].
///       S(B(t))が解析的に計算できるため、生成された曲線の検証に用いる
std::shared_ptr<i_ent::RationalBSplineSurface> MakeBilinearSurfacePD() {
    const auto param = igesio::IGESParameterVector{
        1, 1,                              // K1, K2
        1, 1,                              // M1, M2
        false, false, true, false, false,  // PROP1-5
        0.0, 0.0, 1.0, 1.0,                // Uノット (K1+M1+2=4個)
        0.0, 0.0, 1.0, 1.0,                // Vノット (K2+M2+2=4個)
        1.0, 1.0, 1.0, 1.0,                // 重み (4個)
        // 制御点 (PDの並びはuインデックスが先に変わる)
        0.0, 0.0, 0.0,      // P(0,0)
        2.0, 0.0, 0.0,      // P(1,0)
        0.0, 1.0, 0.0,      // P(0,1)
        2.0, 1.0, 0.0,      // P(1,1)
        0.0, 1.0, 0.0, 1.0  // U(0), U(1), V(0), V(1)
    };
    return std::make_shared<i_ent::RationalBSplineSurface>(param);
}

/// @brief パラメータ空間上の線分B(t)を作成する (z=0)
/// @note (u0, v0) から (u1, v1) への線分. Lineのパラメータ範囲は[0,1]
std::shared_ptr<i_ent::Line> MakeParamLine(
        const double u0, const double v0, const double u1, const double v1) {
    return std::make_shared<i_ent::Line>(
        Vector3d(u0, v0, 0.0), Vector3d(u1, v1, 0.0));
}

/// @brief B(t): (0.2,0.2)→(0.8,0.5) に対応するモデル空間曲線C(t)を作成する
/// @note C(t) = S(B(t)) = (0.4,0.2,0)→(1.6,0.5,0) の線分
std::shared_ptr<i_ent::Line> MakeModelSpaceLine() {
    return std::make_shared<i_ent::Line>(
        Vector3d(0.4, 0.2, 0.0), Vector3d(1.6, 0.5, 0.0));
}

/// @brief BPTR=0 (B省略) フィクスチャの構成要素
struct OmittedBParts {
    /// @brief 曲面 S(u,v) (双一次)
    std::shared_ptr<i_ent::RationalBSplineSurface> surface;
    /// @brief モデル空間曲線 C(t) (S上の線分)
    std::shared_ptr<i_ent::Line> curve;
    /// @brief 本体 (Type 142; BPTR=0)
    std::shared_ptr<CurveOnSurface> cos;
};

/// @brief BPTR=0 (ベース曲線省略; CATIA等の出力相当) のType 142を構築する
/// @param resolve trueの場合、S・Cのポインタを解決する
/// @note S: 双一次曲面、C: S上の線分 (0.4,0.2,0)→(1.6,0.5,0)
///       (= S(B), B: (0.2,0.2)→(0.8,0.5) に相当)
OmittedBParts MakeCosWithOmittedB(const bool resolve) {
    OmittedBParts parts;
    parts.surface = MakeBilinearSurfacePD();
    parts.curve = MakeModelSpaceLine();

    const igesio::IGESParameterVector params{
        static_cast<int>(CurveCreationType::kUnspecified),
        parts.surface->GetID(),
        igesio::IDGenerator::UnsetID(),  // BPTR=0 (省略)
        parts.curve->GetID(),
        static_cast<int>(PreferredRepresentation::kUnspecified)};
    parts.cos = std::make_shared<CurveOnSurface>(
        i_ent::RawEntityDE::ByDefault(
            i_ent::EntityType::kCurveOnAParametricSurface),
        params);

    if (resolve) {
        EXPECT_TRUE(parts.cos->SetUnresolvedReference(parts.surface));
        EXPECT_TRUE(parts.cos->SetUnresolvedReference(parts.curve));
    }
    return parts;
}

}  // namespace



/**
 * コンストラクタのテスト (null検証の回帰; 修正前は委譲初期化式での
 * GetID()参照により未定義動作だった)
 */

// S/B/Cのいずれかがnullptrの場合は例外
TEST(CurveOnSurfaceCtorTest, Constructor_ThrowsInvalidArgumentWhenAnyPointerNull) {
    const auto surface = MakeBilinearSurfacePD();
    const auto base = MakeParamLine(0.2, 0.2, 0.8, 0.5);
    const auto curve = MakeModelSpaceLine();

    EXPECT_THROW(CurveOnSurface(nullptr, base, curve), std::invalid_argument);
    EXPECT_THROW(CurveOnSurface(surface, nullptr, curve), std::invalid_argument);
    EXPECT_THROW(CurveOnSurface(surface, base, nullptr), std::invalid_argument);
}



/**
 * MakeCurveOnAParametricSurface (C自動生成版) のテスト
 */

// 参照が解決された状態で構築される
TEST(CurveOnSurfaceFactoryTest, MakeCurveOnSurface_AutoC_ResolvedReferences) {
    const auto surface = MakeBilinearSurfacePD();
    const auto base = MakeParamLine(0.2, 0.2, 0.8, 0.5);
    const auto [entity, generated] =
        i_ent::MakeCurveOnAParametricSurface(surface, base);

    ASSERT_NE(entity, nullptr);
    ASSERT_NE(generated, nullptr);
    EXPECT_NO_THROW(entity->GetSurface());
    EXPECT_NO_THROW(entity->GetBaseCurve());
    EXPECT_NO_THROW(entity->GetCurve());
    EXPECT_TRUE(entity->GetCurve()->GetID() == generated->GetID());
    EXPECT_TRUE(entity->ValidatePD().is_valid);
}

// 属性省略時: CRTN=kUnspecified、PREF=kSofB (C(t)をS(B(t))から生成するため)
TEST(CurveOnSurfaceFactoryTest, MakeCurveOnSurface_AutoC_DefaultCrtnAndSofBPref) {
    const auto [entity, generated] = i_ent::MakeCurveOnAParametricSurface(
        MakeBilinearSurfacePD(), MakeParamLine(0.2, 0.2, 0.8, 0.5));

    EXPECT_EQ(entity->GetCreationType(), CurveCreationType::kUnspecified);
    EXPECT_EQ(entity->GetPreferredRepresentation(),
              PreferredRepresentation::kSofB);
}

// creation_type指定が反映される
TEST(CurveOnSurfaceFactoryTest, MakeCurveOnSurface_AutoC_CreationTypeApplied) {
    const auto [entity, generated] = i_ent::MakeCurveOnAParametricSurface(
        MakeBilinearSurfacePD(), MakeParamLine(0.2, 0.2, 0.8, 0.5),
        CurveCreationType::kProjection);

    EXPECT_EQ(entity->GetCreationType(), CurveCreationType::kProjection);
}

// 実体の評価がS(B(t))の解析値と一致し、生成Cの端点も一致する
TEST(CurveOnSurfaceFactoryTest, MakeCurveOnSurface_AutoC_EvalMatchesSofB) {
    const auto [entity, generated] = i_ent::MakeCurveOnAParametricSurface(
        MakeBilinearSurfacePD(), MakeParamLine(0.2, 0.2, 0.8, 0.5));

    // B(t) = (0.2+0.6t, 0.2+0.3t) → S(B(t)) = (0.4+1.2t, 0.2+0.3t, 0)
    // (実体の評価はSとBを直接用いるため厳密)
    for (const double t : {0.0, 0.25, 0.5, 0.75, 1.0}) {
        ExpectVectorNear(EvalAt(*entity, t),
                         Vector3d(0.4 + 1.2 * t, 0.2 + 0.3 * t, 0.0));
    }
    // 生成されたC(t)は近似曲線のため、端点を近似許容で確認
    const auto [c0, c1] = generated->GetParameterRange();
    ExpectVectorNear(EvalAt(*generated, c0), Vector3d(0.4, 0.2, 0.0), kApproxTol);
    ExpectVectorNear(EvalAt(*generated, c1), Vector3d(1.6, 0.5, 0.0), kApproxTol);
}

// B・Cに従属フラグが設定される (B: EUF=05+物理従属、C: 物理従属)
TEST(CurveOnSurfaceFactoryTest, MakeCurveOnSurface_AutoC_DependentFlagsSet) {
    const auto base = MakeParamLine(0.2, 0.2, 0.8, 0.5);
    const auto [entity, generated] = i_ent::MakeCurveOnAParametricSurface(
        MakeBilinearSurfacePD(), base);

    EXPECT_EQ(base->GetEntityUseFlag(), i_ent::EntityUseFlag::k2DParametric);
    EXPECT_EQ(base->GetSubordinateEntitySwitch(),
              i_ent::SubordinateEntitySwitch::kPhysicallyDependent);

    const auto gen_entity =
        std::dynamic_pointer_cast<i_ent::EntityBase>(generated);
    ASSERT_NE(gen_entity, nullptr);
    EXPECT_EQ(gen_entity->GetSubordinateEntitySwitch(),
              i_ent::SubordinateEntitySwitch::kPhysicallyDependent);
}

// S/Bのnullptrは例外 (ファクトリ側の検証)
TEST(CurveOnSurfaceFactoryTest,
     MakeCurveOnSurface_AutoC_ThrowsInvalidArgumentWhenNull) {
    const auto surface = MakeBilinearSurfacePD();
    const auto base = MakeParamLine(0.2, 0.2, 0.8, 0.5);

    EXPECT_THROW(i_ent::MakeCurveOnAParametricSurface(nullptr, base),
                 std::invalid_argument);
    EXPECT_THROW(i_ent::MakeCurveOnAParametricSurface(surface, nullptr),
                 std::invalid_argument);
}

// BがSの定義域D外にはみ出す場合は例外 (境界上はちょうど許容)
TEST(CurveOnSurfaceFactoryTest,
     MakeCurveOnSurface_AutoC_ThrowsEntityValueErrorWhenBOutsideDomain) {
    const auto surface = MakeBilinearSurfacePD();

    // u = 1.5 まで伸びる線分は定義域 [0,1]² 外
    EXPECT_THROW(
        i_ent::MakeCurveOnAParametricSurface(
            surface, MakeParamLine(0.5, 0.5, 1.5, 0.5)),
        igesio::EntityValueError);
    // 端点が定義域境界上 (u=1.0) は許容
    EXPECT_NO_THROW(i_ent::MakeCurveOnAParametricSurface(
        surface, MakeParamLine(0.5, 0.5, 1.0, 0.5)));
}



/**
 * MakeCurveOnAParametricSurface (C明示指定版) のテスト
 */

// 渡した参照と属性が反映される
TEST(CurveOnSurfaceFactoryTest,
     MakeCurveOnSurface_ExplicitC_StoredReferencesAndAttrs) {
    const auto surface = MakeBilinearSurfacePD();
    const auto base = MakeParamLine(0.2, 0.2, 0.8, 0.5);
    const auto curve = MakeModelSpaceLine();
    const auto entity = i_ent::MakeCurveOnAParametricSurface(
        surface, base, curve,
        CurveCreationType::kIntersection, PreferredRepresentation::kC);

    ASSERT_NE(entity, nullptr);
    EXPECT_TRUE(entity->GetCurve()->GetID() == curve->GetID());
    EXPECT_EQ(entity->GetCreationType(), CurveCreationType::kIntersection);
    EXPECT_EQ(entity->GetPreferredRepresentation(),
              PreferredRepresentation::kC);
    EXPECT_TRUE(entity->ValidatePD().is_valid);
}

// 属性省略時はCRTN/PREFともkUnspecified (自動kSofBにはならない)
TEST(CurveOnSurfaceFactoryTest,
     MakeCurveOnSurface_ExplicitC_DefaultAttrsUnspecified) {
    const auto entity = i_ent::MakeCurveOnAParametricSurface(
        MakeBilinearSurfacePD(), MakeParamLine(0.2, 0.2, 0.8, 0.5),
        MakeModelSpaceLine());

    EXPECT_EQ(entity->GetCreationType(), CurveCreationType::kUnspecified);
    EXPECT_EQ(entity->GetPreferredRepresentation(),
              PreferredRepresentation::kUnspecified);
}

// S/B/Cのいずれかがnullptrの場合は例外 (コンストラクタ経由の検証)
TEST(CurveOnSurfaceFactoryTest,
     MakeCurveOnSurface_ExplicitC_ThrowsInvalidArgumentWhenAnyNull) {
    const auto surface = MakeBilinearSurfacePD();
    const auto base = MakeParamLine(0.2, 0.2, 0.8, 0.5);
    const auto curve = MakeModelSpaceLine();

    EXPECT_THROW(i_ent::MakeCurveOnAParametricSurface(nullptr, base, curve),
                 std::invalid_argument);
    EXPECT_THROW(i_ent::MakeCurveOnAParametricSurface(surface, nullptr, curve),
                 std::invalid_argument);
    EXPECT_THROW(i_ent::MakeCurveOnAParametricSurface(surface, base, nullptr),
                 std::invalid_argument);
}



/**
 * MakeIsoparametricCurve のテスト
 */

// u=一定: B端点・評価値・CRTN/PREFを確認
TEST(CurveOnSurfaceFactoryTest, MakeIsoparametricCurve_UConstant_Geometry) {
    const auto parts = i_ent::MakeIsoparametricCurve(
        MakeBilinearSurfacePD(), IsoparametricDirection::kUConstant, 0.5);

    ASSERT_NE(parts.curve_on_surface, nullptr);
    ASSERT_NE(parts.base_curve, nullptr);
    ASSERT_NE(parts.curve, nullptr);
    EXPECT_EQ(parts.curve_on_surface->GetCreationType(),
              CurveCreationType::kIsoparametric);
    EXPECT_EQ(parts.curve_on_surface->GetPreferredRepresentation(),
              PreferredRepresentation::kSofB);

    // B: (0.5, 0, 0) → (0.5, 1, 0)
    const auto base_line = std::dynamic_pointer_cast<i_ent::Line>(parts.base_curve);
    ASSERT_NE(base_line, nullptr);
    const auto [p1, p2] = base_line->GetDefinedAnchorPoints();
    ExpectVectorNear(p1, Vector3d(0.5, 0.0, 0.0));
    ExpectVectorNear(p2, Vector3d(0.5, 1.0, 0.0));

    // C(t) = S(0.5, t) = (1.0, t, 0)
    for (const double t : {0.0, 0.5, 1.0}) {
        ExpectVectorNear(EvalAt(*parts.curve_on_surface, t),
                         Vector3d(1.0, t, 0.0));
    }
}

// v=一定: 評価値を確認
TEST(CurveOnSurfaceFactoryTest, MakeIsoparametricCurve_VConstant_Geometry) {
    const auto parts = i_ent::MakeIsoparametricCurve(
        MakeBilinearSurfacePD(), IsoparametricDirection::kVConstant, 0.25);

    EXPECT_EQ(parts.curve_on_surface->GetCreationType(),
              CurveCreationType::kIsoparametric);
    // C(t) = S(t, 0.25) = (2t, 0.25, 0)
    for (const double t : {0.0, 0.5, 1.0}) {
        ExpectVectorNear(EvalAt(*parts.curve_on_surface, t),
                         Vector3d(2.0 * t, 0.25, 0.0));
    }
}

// 境界: 定義域端ちょうどの値は許容される
TEST(CurveOnSurfaceFactoryTest, MakeIsoparametricCurve_BoundaryValue_NoThrow) {
    const auto surface = MakeBilinearSurfacePD();
    EXPECT_NO_THROW(i_ent::MakeIsoparametricCurve(
        surface, IsoparametricDirection::kUConstant, 0.0));
    EXPECT_NO_THROW(i_ent::MakeIsoparametricCurve(
        surface, IsoparametricDirection::kUConstant, 1.0));
}

// 定義域外の値は例外 (両端の境界外側)
TEST(CurveOnSurfaceFactoryTest,
     MakeIsoparametricCurve_ThrowsEntityValueErrorWhenValueOutside) {
    const auto surface = MakeBilinearSurfacePD();
    EXPECT_THROW(
        i_ent::MakeIsoparametricCurve(
            surface, IsoparametricDirection::kUConstant, -1e-6),
        igesio::EntityValueError);
    EXPECT_THROW(
        i_ent::MakeIsoparametricCurve(
            surface, IsoparametricDirection::kUConstant, 1.0 + 1e-6),
        igesio::EntityValueError);
    EXPECT_THROW(
        i_ent::MakeIsoparametricCurve(
            surface, IsoparametricDirection::kVConstant, 1.0 + 1e-6),
        igesio::EntityValueError);
}

// 曲面がnullptrの場合は例外
TEST(CurveOnSurfaceFactoryTest,
     MakeIsoparametricCurve_ThrowsInvalidArgumentWhenSurfaceNull) {
    EXPECT_THROW(
        i_ent::MakeIsoparametricCurve(
            nullptr, IsoparametricDirection::kUConstant, 0.5),
        std::invalid_argument);
}



/**
 * データ取得・変更のテスト
 */

namespace {

/// @brief セッターテスト用の基準インスタンスを作成する
std::shared_ptr<CurveOnSurface> MakeDefaultCos() {
    auto [entity, generated] = i_ent::MakeCurveOnAParametricSurface(
        MakeBilinearSurfacePD(), MakeParamLine(0.2, 0.2, 0.8, 0.5));
    return entity;
}

}  // namespace

// 有効な作成方法 (CRTN) はすべて設定できる
TEST(CurveOnSurfaceSetterTest, SetCreationType_UpdatesValue) {
    const auto entity = MakeDefaultCos();
    for (const auto type : {CurveCreationType::kUnspecified,
                            CurveCreationType::kProjection,
                            CurveCreationType::kIntersection,
                            CurveCreationType::kIsoparametric}) {
        EXPECT_TRUE(entity->SetCreationType(type));
        EXPECT_EQ(entity->GetCreationType(), type);
    }
}

// 列挙範囲外の値はfalseを返し値を変更しない
TEST(CurveOnSurfaceSetterTest, SetCreationType_InvalidValue_ReturnsFalse) {
    const auto entity = MakeDefaultCos();
    ASSERT_TRUE(entity->SetCreationType(CurveCreationType::kProjection));

    EXPECT_FALSE(entity->SetCreationType(static_cast<CurveCreationType>(4)));
    EXPECT_EQ(entity->GetCreationType(), CurveCreationType::kProjection);
}

// 有効な優先表現 (PREF) はすべて設定できる
TEST(CurveOnSurfaceSetterTest, SetPreferredRepresentation_UpdatesValue) {
    const auto entity = MakeDefaultCos();
    for (const auto pref : {PreferredRepresentation::kUnspecified,
                            PreferredRepresentation::kSofB,
                            PreferredRepresentation::kC,
                            PreferredRepresentation::kEquallyPreferred}) {
        EXPECT_TRUE(entity->SetPreferredRepresentation(pref));
        EXPECT_EQ(entity->GetPreferredRepresentation(), pref);
    }
}

// 列挙範囲外の値はfalseを返し値を変更しない
TEST(CurveOnSurfaceSetterTest,
     SetPreferredRepresentation_InvalidValue_ReturnsFalse) {
    const auto entity = MakeDefaultCos();
    ASSERT_TRUE(entity->SetPreferredRepresentation(PreferredRepresentation::kC));

    EXPECT_FALSE(entity->SetPreferredRepresentation(
        static_cast<PreferredRepresentation>(4)));
    EXPECT_EQ(entity->GetPreferredRepresentation(),
              PreferredRepresentation::kC);
}

// ファクトリ生成品はBが解決済み
TEST(CurveOnSurfaceSetterTest, HasBaseCurve_TrueWhenResolved) {
    const auto entity = MakeDefaultCos();
    EXPECT_TRUE(entity->HasBaseCurve());
    EXPECT_FALSE(entity->IsBaseCurveOmitted());
}

// BPTR=0で構築した場合はB省略状態
TEST(CurveOnSurfaceSetterTest, IsBaseCurveOmitted_TrueWhenBptrZero) {
    const auto parts = MakeCosWithOmittedB(/*resolve=*/false);
    EXPECT_FALSE(parts.cos->HasBaseCurve());
    EXPECT_TRUE(parts.cos->IsBaseCurveOmitted());
}

// C省略時は自動生成したCを返す (修正前はnull参照でクラッシュしていた経路)
TEST(CurveOnSurfaceSetterTest, SetCurves_OmittedCurve_GeneratesC) {
    const auto entity = MakeDefaultCos();
    const auto new_base = MakeParamLine(0.3, 0.3, 0.7, 0.7);

    const auto generated = entity->SetCurves(new_base);
    ASSERT_NE(generated, nullptr);
    EXPECT_TRUE(entity->GetCurve()->GetID() == generated->GetID());
    EXPECT_EQ(entity->GetPreferredRepresentation(),
              PreferredRepresentation::kSofB);
    // 新しいB由来の評価: B(0.5) = (0.5, 0.5) → S(B) = (1.0, 0.5, 0)
    ExpectVectorNear(EvalAt(*entity, 0.5), Vector3d(1.0, 0.5, 0.0));
}

// C明示指定時はnullptrを返し、指定したCが設定される
TEST(CurveOnSurfaceSetterTest, SetCurves_ExplicitCurve_ReturnsNullptr) {
    const auto entity = MakeDefaultCos();
    const auto new_base = MakeParamLine(0.3, 0.3, 0.7, 0.7);
    const auto new_curve = std::make_shared<i_ent::Line>(
        Vector3d(0.6, 0.3, 0.0), Vector3d(1.4, 0.7, 0.0));

    const auto generated = entity->SetCurves(new_base, new_curve);
    EXPECT_EQ(generated, nullptr);
    EXPECT_TRUE(entity->GetCurve()->GetID() == new_curve->GetID());
}

// Bがnullptrの場合は例外
TEST(CurveOnSurfaceSetterTest,
     SetCurves_ThrowsInvalidArgumentWhenBaseCurveNull) {
    const auto entity = MakeDefaultCos();
    EXPECT_THROW(entity->SetCurves(nullptr), std::invalid_argument);
}



/**
 * ReconstructOmittedBaseCurve のテスト
 */

// BPTR=0かつS・C解決済みの場合、B = S^{-1}∘C を再構築する
TEST(CurveOnSurfaceReconstructTest, ReconstructOmittedBaseCurve_RebuildsBFromSAndC) {
    const auto parts = MakeCosWithOmittedB(/*resolve=*/true);
    ASSERT_TRUE(parts.cos->IsBaseCurveOmitted());
    ASSERT_FALSE(parts.cos->HasBaseCurve());

    const auto base = parts.cos->ReconstructOmittedBaseCurve();
    ASSERT_NE(base, nullptr);
    EXPECT_TRUE(parts.cos->HasBaseCurve());
    EXPECT_FALSE(parts.cos->IsBaseCurveOmitted());
    EXPECT_TRUE(parts.cos->GetBaseCurve()->GetID() == base->GetID());

    // 再構築後のS(B(t))がCの端点 (0.4,0.2,0)/(1.6,0.5,0) を再現する
    const auto [t0, t1] = parts.cos->GetParameterRange();
    ExpectVectorNear(EvalAt(*parts.cos, t0), Vector3d(0.4, 0.2, 0.0), kApproxTol);
    ExpectVectorNear(EvalAt(*parts.cos, t1), Vector3d(1.6, 0.5, 0.0), kApproxTol);
    EXPECT_TRUE(parts.cos->ValidatePD().is_valid);
}

// 再構築されたBにはEUF=05 (2D Parametric)・物理従属が設定される
TEST(CurveOnSurfaceReconstructTest, ReconstructOmittedBaseCurve_SetsDependentFlags) {
    const auto parts = MakeCosWithOmittedB(/*resolve=*/true);
    const auto base = parts.cos->ReconstructOmittedBaseCurve();
    ASSERT_NE(base, nullptr);

    const auto base_entity = std::dynamic_pointer_cast<i_ent::EntityBase>(base);
    ASSERT_NE(base_entity, nullptr);
    EXPECT_EQ(base_entity->GetEntityUseFlag(),
              i_ent::EntityUseFlag::k2DParametric);
    EXPECT_EQ(base_entity->GetSubordinateEntitySwitch(),
              i_ent::SubordinateEntitySwitch::kPhysicallyDependent);
}

// Bが既に設定済みの場合はnullptrを返す
TEST(CurveOnSurfaceReconstructTest,
     ReconstructOmittedBaseCurve_ReturnsNullptrWhenBAlreadySet) {
    const auto entity = MakeDefaultCos();
    EXPECT_EQ(entity->ReconstructOmittedBaseCurve(), nullptr);
}

// S・Cのポインタが未解決の場合はnullptrを返す (状態も変化しない)
TEST(CurveOnSurfaceReconstructTest,
     ReconstructOmittedBaseCurve_ReturnsNullptrWhenReferencesUnresolved) {
    const auto parts = MakeCosWithOmittedB(/*resolve=*/false);
    EXPECT_EQ(parts.cos->ReconstructOmittedBaseCurve(), nullptr);
    EXPECT_FALSE(parts.cos->HasBaseCurve());
    EXPECT_TRUE(parts.cos->IsBaseCurveOmitted());
}
