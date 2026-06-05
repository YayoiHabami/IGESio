/**
 * @file tests/entities/curves/test_rational_b_spline_curve_edit.cpp
 * @brief RationalBSplineCurve (Type 126) の編集機能のテスト
 * @author Yayoi Habami
 * @date 2026-06-05
 * @copyright 2026 Yayoi Habami
 *
 * テスト対象:
 *   [ファクトリ関数]
 *     MakeRationalBSplineCurve(), MakeClampedBSplineCurve(), MakeBezierCurve()
 *   [データ取得]
 *     NumControlPoints(), GetControlPointAt(i), GetWeightAt(i),
 *     IsPlanar(), TryGetDefinedPlaneNormal(), TryGetPlaneNormal()
 *   [データ変更]
 *     SetControlPointAt(i, p), SetControlPoints(cp),
 *     SetWeightAt(i, w), SetWeights(w), SetKnots(t),
 *     SetParameterRange(v), SetPeriodic(flag), SetCurveType(type),
 *     SetData(...)
 *
 * TODO:
 *   - GetMainPDParameters() 経由の書き出しラウンドトリップは protected の
 *     ため対象外 (test_igesio 側の責務)
 *   - 評価系 (TryGetDefinedDerivatives 等)・ValidatePD の網羅は既存
 *     test_rational_b_spline_curve.cpp の責務
 */
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <initializer_list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/common/iges_parameter_vector.h"
#include "igesio/numerics/core/tolerance.h"
#include "igesio/entities/curves/rational_b_spline_curve.h"
#include "igesio/entities/transformations/transformation_matrix.h"

namespace {

namespace i_ent = igesio::entities;
using igesio::Vector3d;
using i_ent::RationalBSplineCurve;
using i_ent::RationalBSplineCurveType;
/// @brief 浮動小数点比較の許容誤差
constexpr double kTol = 1e-9;



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

/// @brief std::vector<double>の全要素一致を検証する
void ExpectVectorsNear(const std::vector<double>& actual,
                       const std::vector<double>& expected) {
    ASSERT_EQ(actual.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_NEAR(actual[i], expected[i], kTol) << "index " << i;
    }
}

/// @brief 曲線上の点C(t)を評価する (TryGetDefinedDerivativesの0階)
Vector3d EvalAt(const RationalBSplineCurve& curve, const double t) {
    const auto derivs = curve.TryGetDefinedDerivatives(t, 0);
    EXPECT_TRUE(derivs.has_value()) << "evaluation failed at t = " << t;
    return derivs ? (*derivs)[0] : Vector3d::Zero();
}



/**
 * テスト用インスタンスのファクトリ関数
 *
 * IGES 126 パラメータ形式:
 *   K, M, PROP1-4(bool×4), ノット(K+M+2個), 重み(K+1個),
 *   制御点(3×(K+1)個), V(0), V(1), 法線(3個)
 *
 * NOTE: セッター/ゲッターのテストがファクトリ関数の正しさに依存しない
 *       よう、基準インスタンスはPDコンストラクタ経由で構築する
 */

/// @brief {x0,y0,z0, x1,y1,z1, ...} から Matrix3Xd を構築するヘルパー
/// @param coords 制御点座標の列（x, y, z の順）
igesio::Matrix3Xd MakeCPs(std::initializer_list<double> coords) {
    const int n = static_cast<int>(coords.size()) / 3;
    igesio::Matrix3Xd cp(3, n);
    auto it = coords.begin();
    for (int i = 0; i < n; ++i) {
        cp(0, i) = *it++;
        cp(1, i) = *it++;
        cp(2, i) = *it++;
    }
    return cp;
}

/// @brief degree=2 Bezier形状の基準NURBS (PDコンストラクタ経由)
/// K=2, M=2, ノット=[0,0,0,1,1,1], 重み全て1, V=[0,1]
/// P0=(0,0,0), P1=(1,1,0), P2=(2,0,0), 法線=(0,0,1) (XY平面)
std::shared_ptr<RationalBSplineCurve> MakeDeg2BezierPD() {
    const auto param = igesio::IGESParameterVector{
        2, 2,                          // K=2, M=2
        true, false, true, false,      // PROP1-4
        0.0, 0.0, 0.0, 1.0, 1.0, 1.0,  // ノット (K+M+2=6個)
        1.0, 1.0, 1.0,                 // 重み (K+1=3個)
        0.0, 0.0, 0.0,  // P0
        1.0, 1.0, 0.0,  // P1
        2.0, 0.0, 0.0,  // P2
        0.0, 1.0,       // V=[0,1]
        0.0, 0.0, 1.0   // 法線
    };
    return std::make_shared<RationalBSplineCurve>(param);
}

/// @brief degree=1 平面ポリライン状NURBS (XY平面上の正方形3辺)
/// K=3, M=1, ノット=[0,0,1,2,3,3], 重み全て1, V=[0,3]
/// P0=(0,0,0), P1=(1,0,0), P2=(1,1,0), P3=(0,1,0), 法線=(0,0,1)
/// @note 平面性テスト用: 3点では常に平面のため4制御点とし、
///       1点を面外に動かすと非平面になるようにする
std::shared_ptr<RationalBSplineCurve> MakeDeg1PlanarSquarePD() {
    const auto param = igesio::IGESParameterVector{
        3, 1,                          // K=3, M=1
        true, false, true, false,      // PROP1-4
        0.0, 0.0, 1.0, 2.0, 3.0, 3.0,  // ノット (K+M+2=6個)
        1.0, 1.0, 1.0, 1.0,            // 重み (K+1=4個)
        0.0, 0.0, 0.0,  // P0
        1.0, 0.0, 0.0,  // P1
        1.0, 1.0, 0.0,  // P2
        0.0, 1.0, 0.0,  // P3
        0.0, 3.0,       // V=[0,3]
        0.0, 0.0, 1.0   // 法線
    };
    return std::make_shared<RationalBSplineCurve>(param);
}

/// @brief degree=1 非平面NURBS (四面体状の4制御点)
/// K=3, M=1, ノット=[0,0,1,2,3,3], 重み全て1, V=[0,3], 法線=(0,0,0)
/// P0=(0,0,0), P1=(1,0,0), P2=(1,1,0), P3=(1,1,1)
std::shared_ptr<RationalBSplineCurve> MakeDeg1NonPlanarPD() {
    const auto param = igesio::IGESParameterVector{
        3, 1,                          // K=3, M=1
        false, false, true, false,     // PROP1-4
        0.0, 0.0, 1.0, 2.0, 3.0, 3.0,  // ノット (K+M+2=6個)
        1.0, 1.0, 1.0, 1.0,            // 重み (K+1=4個)
        0.0, 0.0, 0.0,  // P0
        1.0, 0.0, 0.0,  // P1
        1.0, 1.0, 0.0,  // P2
        1.0, 1.0, 1.0,  // P3
        0.0, 3.0,       // V=[0,3]
        0.0, 0.0, 0.0   // 法線 (非平面)
    };
    return std::make_shared<RationalBSplineCurve>(param);
}

}  // namespace



/**
 * MakeRationalBSplineCurve (一般形) のテスト
 */

// 入力したパラメータがそのまま格納される
TEST(RationalBSplineCurveFactoryTest, MakeRationalBSplineCurve_StoredValues) {
    const auto cps = MakeCPs({0, 0, 0, 1, 1, 0, 2, 0, 0});
    const std::vector<double> knots{0.0, 0.0, 0.0, 1.0, 1.0, 1.0};
    const std::vector<double> weights{1.0, 2.0, 1.0};
    const auto curve = i_ent::MakeRationalBSplineCurve(
        2, cps, knots, weights, std::array<double, 2>{0.0, 1.0});

    EXPECT_EQ(curve->Degree(), 2);
    EXPECT_EQ(curve->NumControlPoints(), 3);
    ExpectVectorsNear(curve->Knots(), knots);
    ExpectVectorsNear(curve->Weights(), weights);
    ExpectVectorNear(curve->GetControlPointAt(1), Vector3d(1.0, 1.0, 0.0));
    EXPECT_NEAR(curve->GetParameterRange()[0], 0.0, kTol);
    EXPECT_NEAR(curve->GetParameterRange()[1], 1.0, kTol);
    EXPECT_EQ(curve->GetCurveType(), RationalBSplineCurveType::kUndetermined);
    EXPECT_TRUE(curve->ValidatePD().is_valid);
}

// 重み省略時は全て1.0 (polynomial形式) となる
TEST(RationalBSplineCurveFactoryTest,
     MakeRationalBSplineCurve_EmptyWeights_PolynomialAllOnes) {
    const auto curve = i_ent::MakeRationalBSplineCurve(
        2, MakeCPs({0, 0, 0, 1, 1, 0, 2, 0, 0}),
        {0.0, 0.0, 0.0, 1.0, 1.0, 1.0});

    ExpectVectorsNear(curve->Weights(), {1.0, 1.0, 1.0});
    EXPECT_TRUE(curve->IsPolynomial());
}

// 範囲省略時はノット定義域 [T(0), T(N)] となる (非クランプ一様ノットで
// インデックスを区別: T(0)=knots[M]=2, T(N)=knots[K+1]=5)
TEST(RationalBSplineCurveFactoryTest,
     MakeRationalBSplineCurve_OmittedRange_UsesKnotDomain) {
    const auto cps =
        MakeCPs({0, 0, 0, 1, 1, 0, 2, 0, 0, 3, 1, 0, 4, 0, 0});  // K=4
    const std::vector<double> knots{0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0};
    const auto curve = i_ent::MakeRationalBSplineCurve(2, cps, knots);

    EXPECT_NEAR(curve->GetParameterRange()[0], 2.0, kTol);
    EXPECT_NEAR(curve->GetParameterRange()[1], 5.0, kTol);
    EXPECT_TRUE(curve->ValidatePD().is_valid);
}

// 同一データのPDコンストラクタ品と評価結果が一致する
TEST(RationalBSplineCurveFactoryTest,
     MakeRationalBSplineCurve_EvalMatchesPDConstructor) {
    const auto factory_curve = i_ent::MakeRationalBSplineCurve(
        2, MakeCPs({0, 0, 0, 1, 1, 0, 2, 0, 0}),
        {0.0, 0.0, 0.0, 1.0, 1.0, 1.0});
    const auto pd_curve = MakeDeg2BezierPD();

    for (const double t : {0.0, 0.25, 0.5, 0.75, 1.0}) {
        ExpectVectorNear(EvalAt(*factory_curve, t), EvalAt(*pd_curve, t));
    }
}

// XY平面上の制御点 → 平面的と判定され法線は±(0,0,1)
TEST(RationalBSplineCurveFactoryTest,
     MakeRationalBSplineCurve_PlanarPoints_PlanarWithNormal) {
    const auto curve = i_ent::MakeRationalBSplineCurve(
        2, MakeCPs({0, 0, 0, 1, 1, 0, 2, 0, 0}),
        {0.0, 0.0, 0.0, 1.0, 1.0, 1.0});

    EXPECT_TRUE(curve->IsPlanar());
    const auto normal = curve->TryGetDefinedPlaneNormal();
    ASSERT_TRUE(normal.has_value());
    EXPECT_NEAR(std::abs(normal->z()), 1.0, kTol);
    EXPECT_NEAR(normal->x(), 0.0, kTol);
    EXPECT_NEAR(normal->y(), 0.0, kTol);
}

// 非平面の制御点 → 非平面と判定され法線はnullopt
TEST(RationalBSplineCurveFactoryTest,
     MakeRationalBSplineCurve_NonPlanarPoints_NormalNullopt) {
    const auto curve = i_ent::MakeRationalBSplineCurve(
        1, MakeCPs({0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1}),
        {0.0, 0.0, 1.0, 2.0, 3.0, 3.0});

    EXPECT_FALSE(curve->IsPlanar());
    EXPECT_FALSE(curve->TryGetDefinedPlaneNormal().has_value());
}

// 境界: 制御点数 = M+1 (最小構成; degree=1の線分) で構築できる
TEST(RationalBSplineCurveFactoryTest,
     MakeRationalBSplineCurve_MinimumControlPoints) {
    const auto curve = i_ent::MakeRationalBSplineCurve(
        1, MakeCPs({0, 0, 0, 1, 0, 0}), {0.0, 0.0, 1.0, 1.0});

    EXPECT_NEAR(curve->GetParameterRange()[0], 0.0, kTol);
    EXPECT_NEAR(curve->GetParameterRange()[1], 1.0, kTol);
    ExpectVectorNear(EvalAt(*curve, 0.5), Vector3d(0.5, 0.0, 0.0));
}

// 次数0は構築不可
TEST(RationalBSplineCurveFactoryTest,
     MakeRationalBSplineCurve_ThrowsEntityValueErrorWhenDegreeZero) {
    EXPECT_THROW(
        i_ent::MakeRationalBSplineCurve(
            0, MakeCPs({0, 0, 0, 1, 0, 0}), {0.0, 1.0}),
        igesio::EntityValueError);
}

// 制御点数 < M+1 は構築不可 (M点で例外、M+1点で成功)
TEST(RationalBSplineCurveFactoryTest,
     MakeRationalBSplineCurve_ThrowsEntityValueErrorWhenTooFewControlPoints) {
    const std::vector<double> knots{0.0, 0.0, 0.0, 1.0, 1.0, 1.0};
    EXPECT_THROW(
        i_ent::MakeRationalBSplineCurve(
            2, MakeCPs({0, 0, 0, 1, 0, 0}), knots),
        igesio::EntityValueError);
    EXPECT_NO_THROW(i_ent::MakeRationalBSplineCurve(
        2, MakeCPs({0, 0, 0, 1, 1, 0, 2, 0, 0}), knots));
}

// ノット数 != K+M+2 は構築不可 (±1個の両側)
TEST(RationalBSplineCurveFactoryTest,
     MakeRationalBSplineCurve_ThrowsEntityValueErrorWhenKnotCountWrong) {
    const auto cps = MakeCPs({0, 0, 0, 1, 1, 0, 2, 0, 0});
    EXPECT_THROW(
        i_ent::MakeRationalBSplineCurve(
            2, cps, {0.0, 0.0, 0.0, 1.0, 1.0}),  // 5個 (1個不足)
        igesio::EntityValueError);
    EXPECT_THROW(
        i_ent::MakeRationalBSplineCurve(
            2, cps, {0.0, 0.0, 0.0, 0.5, 1.0, 1.0, 1.0}),  // 7個 (1個超過)
        igesio::EntityValueError);
}

// 非減少でないノットは構築不可 (隣接同値の重複ノットは許容)
TEST(RationalBSplineCurveFactoryTest,
     MakeRationalBSplineCurve_ThrowsEntityValueErrorWhenKnotsDecreasing) {
    const auto cps = MakeCPs({0, 0, 0, 1, 1, 0, 2, 0, 0});
    EXPECT_THROW(
        i_ent::MakeRationalBSplineCurve(
            2, cps, {0.0, 0.0, 0.0, 1.0, 0.5, 1.0}),  // 1.0 → 0.5 で逆転
        igesio::EntityValueError);
    // 隣接同値 (クランプノット) は非減少なので許容
    EXPECT_NO_THROW(i_ent::MakeRationalBSplineCurve(
        2, cps, {0.0, 0.0, 0.0, 1.0, 1.0, 1.0}));
}

// 重み数 != K+1 は構築不可 (±1個の両側)
TEST(RationalBSplineCurveFactoryTest,
     MakeRationalBSplineCurve_ThrowsEntityValueErrorWhenWeightCountWrong) {
    const auto cps = MakeCPs({0, 0, 0, 1, 1, 0, 2, 0, 0});
    const std::vector<double> knots{0.0, 0.0, 0.0, 1.0, 1.0, 1.0};
    EXPECT_THROW(
        i_ent::MakeRationalBSplineCurve(2, cps, knots, {1.0, 1.0}),
        igesio::EntityValueError);
    EXPECT_THROW(
        i_ent::MakeRationalBSplineCurve(2, cps, knots, {1.0, 1.0, 1.0, 1.0}),
        igesio::EntityValueError);
}

// 正でない重みは構築不可 (w=0は境界外、微小な正値は境界内)
TEST(RationalBSplineCurveFactoryTest,
     MakeRationalBSplineCurve_ThrowsEntityValueErrorWhenWeightNotPositive) {
    const auto cps = MakeCPs({0, 0, 0, 1, 1, 0, 2, 0, 0});
    const std::vector<double> knots{0.0, 0.0, 0.0, 1.0, 1.0, 1.0};
    EXPECT_THROW(
        i_ent::MakeRationalBSplineCurve(2, cps, knots, {1.0, 0.0, 1.0}),
        igesio::EntityValueError);
    EXPECT_THROW(
        i_ent::MakeRationalBSplineCurve(2, cps, knots, {1.0, -1.0, 1.0}),
        igesio::EntityValueError);
    EXPECT_NO_THROW(
        i_ent::MakeRationalBSplineCurve(2, cps, knots, {1.0, 1e-12, 1.0}));
}

// V(0) >= V(1) は構築不可 (同値は境界外、微小な増加は境界内)
TEST(RationalBSplineCurveFactoryTest,
     MakeRationalBSplineCurve_ThrowsEntityValueErrorWhenRangeNotIncreasing) {
    const auto cps = MakeCPs({0, 0, 0, 1, 1, 0, 2, 0, 0});
    const std::vector<double> knots{0.0, 0.0, 0.0, 1.0, 1.0, 1.0};
    EXPECT_THROW(
        i_ent::MakeRationalBSplineCurve(
            2, cps, knots, {}, std::array<double, 2>{0.5, 0.5}),
        igesio::EntityValueError);
    EXPECT_NO_THROW(i_ent::MakeRationalBSplineCurve(
        2, cps, knots, {}, std::array<double, 2>{0.5, 0.5 + 1e-9}));
}



/**
 * MakeClampedBSplineCurve のテスト
 */

// クランプ一様ノットが自動生成される (deg3・6点 → 内部ノット 1/3, 2/3)
TEST(RationalBSplineCurveFactoryTest, MakeClampedBSplineCurve_KnotStructure) {
    const auto cps = MakeCPs(
        {0, 0, 0, 1, 2, 0, 2, 2, 0, 3, 0, 0, 4, 1, 0, 5, 0, 0});
    const auto curve = i_ent::MakeClampedBSplineCurve(3, cps);

    ExpectVectorsNear(curve->Knots(),
                      {0.0, 0.0, 0.0, 0.0, 1.0 / 3.0, 2.0 / 3.0,
                       1.0, 1.0, 1.0, 1.0});
    EXPECT_NEAR(curve->GetParameterRange()[0], 0.0, kTol);
    EXPECT_NEAR(curve->GetParameterRange()[1], 1.0, kTol);
    EXPECT_TRUE(curve->ValidatePD().is_valid);
}

// 縮退: K=M (内部ノットなし) → Bezierと同じノットになる
TEST(RationalBSplineCurveFactoryTest, MakeClampedBSplineCurve_NoInteriorKnots) {
    const auto curve = i_ent::MakeClampedBSplineCurve(
        2, MakeCPs({0, 0, 0, 1, 1, 0, 2, 0, 0}));

    ExpectVectorsNear(curve->Knots(), {0.0, 0.0, 0.0, 1.0, 1.0, 1.0});
}

// クランプ性: 曲線は両端の制御点を通る
TEST(RationalBSplineCurveFactoryTest,
     MakeClampedBSplineCurve_InterpolatesEndpoints) {
    const auto cps = MakeCPs(
        {0, 0, 0, 1, 2, 0, 2, 2, 0, 3, 0, 0, 4, 1, 0, 5, 0, 0});
    const auto curve = i_ent::MakeClampedBSplineCurve(3, cps);

    ExpectVectorNear(EvalAt(*curve, 0.0), Vector3d(0.0, 0.0, 0.0));
    ExpectVectorNear(EvalAt(*curve, 1.0), Vector3d(5.0, 0.0, 0.0));
}

// 制御点数 < M+1 は構築不可 (M点で例外、M+1点で成功)
TEST(RationalBSplineCurveFactoryTest,
     MakeClampedBSplineCurve_ThrowsEntityValueErrorWhenTooFewControlPoints) {
    EXPECT_THROW(
        i_ent::MakeClampedBSplineCurve(
            3, MakeCPs({0, 0, 0, 1, 1, 0, 2, 0, 0})),
        igesio::EntityValueError);
    EXPECT_NO_THROW(i_ent::MakeClampedBSplineCurve(
        3, MakeCPs({0, 0, 0, 1, 1, 0, 2, 0, 0, 3, 1, 0})));
}

// 次数0は構築不可 (制御点数が十分でも)
TEST(RationalBSplineCurveFactoryTest,
     MakeClampedBSplineCurve_ThrowsEntityValueErrorWhenDegreeZero) {
    EXPECT_THROW(
        i_ent::MakeClampedBSplineCurve(0, MakeCPs({0, 0, 0, 1, 0, 0})),
        igesio::EntityValueError);
}



/**
 * MakeBezierCurve のテスト
 */

// 次数とノット構造 (4点 → deg3, ノット{0×4, 1×4})
TEST(RationalBSplineCurveFactoryTest, MakeBezierCurve_DegreeAndKnots) {
    const auto curve = i_ent::MakeBezierCurve(
        MakeCPs({0, 0, 0, 1, 1, 0, 2, 1, 0, 3, 0, 0}));

    EXPECT_EQ(curve->Degree(), 3);
    ExpectVectorsNear(curve->Knots(),
                      {0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0});
    EXPECT_NEAR(curve->GetParameterRange()[0], 0.0, kTol);
    EXPECT_NEAR(curve->GetParameterRange()[1], 1.0, kTol);
    EXPECT_TRUE(curve->ValidatePD().is_valid);
}

// 2次Bezierの評価値が de Casteljau の解析値と一致する
TEST(RationalBSplineCurveFactoryTest,
     MakeBezierCurve_QuadraticEvalMatchesDeCasteljau) {
    const auto curve = i_ent::MakeBezierCurve(
        MakeCPs({0, 0, 0, 1, 1, 0, 2, 0, 0}));

    // C(0.5) = (P0 + 2*P1 + P2) / 4 = (1, 0.5, 0)
    ExpectVectorNear(EvalAt(*curve, 0.5), Vector3d(1.0, 0.5, 0.0));
    ExpectVectorNear(EvalAt(*curve, 0.0), Vector3d(0.0, 0.0, 0.0));
    ExpectVectorNear(EvalAt(*curve, 1.0), Vector3d(2.0, 0.0, 0.0));
}

// 有理ケース: 重み{1, √2/2, 1}の2次Bezierは単位円の1/4円弧
TEST(RationalBSplineCurveFactoryTest, MakeBezierCurve_RationalQuarterCircle) {
    const auto curve = i_ent::MakeBezierCurve(
        MakeCPs({1, 0, 0, 1, 1, 0, 0, 1, 0}),
        {1.0, std::sqrt(0.5), 1.0});

    EXPECT_FALSE(curve->IsPolynomial());
    for (const double t : {0.0, 0.25, 0.5, 0.75, 1.0}) {
        const auto point = EvalAt(*curve, t);
        EXPECT_NEAR(point.norm(), 1.0, kTol) << "t = " << t;
        EXPECT_NEAR(point.z(), 0.0, kTol) << "t = " << t;
    }
}

// 制御点数2未満は構築不可 (1点で例外、2点で成功)
TEST(RationalBSplineCurveFactoryTest,
     MakeBezierCurve_ThrowsEntityValueErrorWhenLessThanTwoControlPoints) {
    EXPECT_THROW(i_ent::MakeBezierCurve(MakeCPs({0, 0, 0})),
                 igesio::EntityValueError);
    EXPECT_NO_THROW(i_ent::MakeBezierCurve(MakeCPs({0, 0, 0, 1, 0, 0})));
}



/**
 * データ取得のテスト
 */

// NumControlPoints は K+1 を返す
TEST(RationalBSplineCurveGetterTest, NumControlPoints_MatchesControlPointCount) {
    EXPECT_EQ(MakeDeg2BezierPD()->NumControlPoints(), 3);
    EXPECT_EQ(MakeDeg1PlanarSquarePD()->NumControlPoints(), 4);
}

// GetControlPointAt は格納された各制御点を返す
TEST(RationalBSplineCurveGetterTest, GetControlPointAt_ReturnsStoredPoints) {
    const auto curve = MakeDeg2BezierPD();
    ExpectVectorNear(curve->GetControlPointAt(0), Vector3d(0.0, 0.0, 0.0));
    ExpectVectorNear(curve->GetControlPointAt(1), Vector3d(1.0, 1.0, 0.0));
    ExpectVectorNear(curve->GetControlPointAt(2), Vector3d(2.0, 0.0, 0.0));
}

// 範囲外インデックスは例外 (i=K+1で例外、i=Kで成功)
TEST(RationalBSplineCurveGetterTest,
     GetControlPointAt_ThrowsOutOfRangeWhenIndexTooLarge) {
    const auto curve = MakeDeg2BezierPD();
    EXPECT_THROW(curve->GetControlPointAt(3), std::out_of_range);
    EXPECT_NO_THROW(curve->GetControlPointAt(2));
}

// GetWeightAt は格納された各重みを返す
TEST(RationalBSplineCurveGetterTest, GetWeightAt_ReturnsStoredWeights) {
    const auto curve = MakeDeg2BezierPD();
    for (size_t i = 0; i < 3; ++i) {
        EXPECT_NEAR(curve->GetWeightAt(i), 1.0, kTol) << "index " << i;
    }
}

// 範囲外インデックスは例外 (i=K+1で例外、i=Kで成功)
TEST(RationalBSplineCurveGetterTest,
     GetWeightAt_ThrowsOutOfRangeWhenIndexTooLarge) {
    const auto curve = MakeDeg2BezierPD();
    EXPECT_THROW(curve->GetWeightAt(3), std::out_of_range);
    EXPECT_NO_THROW(curve->GetWeightAt(2));
}

// PDで指定した法線がそのまま取得できる
TEST(RationalBSplineCurveGetterTest, TryGetDefinedPlaneNormal_FromPDNormal) {
    const auto curve = MakeDeg2BezierPD();
    EXPECT_TRUE(curve->IsPlanar());
    const auto normal = curve->TryGetDefinedPlaneNormal();
    ASSERT_TRUE(normal.has_value());
    ExpectVectorNear(*normal, Vector3d(0.0, 0.0, 1.0));
}

// 変換行列なしの場合、親空間の法線は定義空間と一致する
TEST(RationalBSplineCurveGetterTest, TryGetPlaneNormal_NoTransform_SameAsDefined) {
    const auto curve = MakeDeg2BezierPD();
    const auto normal = curve->TryGetPlaneNormal();
    ASSERT_TRUE(normal.has_value());
    ExpectVectorNear(*normal, Vector3d(0.0, 0.0, 1.0));
}

// 変換行列 (X軸周り+90°回転) が法線にベクトルとして適用される
TEST(RationalBSplineCurveGetterTest, TryGetPlaneNormal_RotationApplied) {
    const auto curve = MakeDeg2BezierPD();
    const auto rotation =
        i_ent::MakeRotation(igesio::kPi / 2.0, Vector3d(1.0, 0.0, 0.0));
    ASSERT_TRUE(curve->OverwriteTransformationMatrix(rotation));

    // R_x(90°) * (0,0,1) = (0,-1,0); 定義空間側は不変
    const auto normal = curve->TryGetPlaneNormal();
    ASSERT_TRUE(normal.has_value());
    ExpectVectorNear(*normal, Vector3d(0.0, -1.0, 0.0));
    ExpectVectorNear(*curve->TryGetDefinedPlaneNormal(),
                     Vector3d(0.0, 0.0, 1.0));
}

// 非平面曲線では両方とも nullopt
TEST(RationalBSplineCurveGetterTest, TryGetDefinedPlaneNormal_NonPlanar_Nullopt) {
    const auto curve = MakeDeg1NonPlanarPD();
    EXPECT_FALSE(curve->IsPlanar());
    EXPECT_FALSE(curve->TryGetDefinedPlaneNormal().has_value());
    EXPECT_FALSE(curve->TryGetPlaneNormal().has_value());
}



/**
 * データ変更 (構造保存セッター) のテスト
 */

// 制御点の変更が格納値と評価に反映される
TEST(RationalBSplineCurveSetterTest, SetControlPointAt_UpdatesStoredPoint) {
    const auto curve = MakeDeg2BezierPD();
    curve->SetControlPointAt(1, Vector3d(1.0, 2.0, 0.0));

    ExpectVectorNear(curve->GetControlPointAt(1), Vector3d(1.0, 2.0, 0.0));
    // C(0.5) = (P0 + 2*P1 + P2) / 4 = (1, 1, 0)
    ExpectVectorNear(EvalAt(*curve, 0.5), Vector3d(1.0, 1.0, 0.0));
}

// 制御点変更で平面性が再計算される (面外→非平面、面内に戻す→平面)
TEST(RationalBSplineCurveSetterTest, SetControlPointAt_RecomputesPlanarity) {
    const auto curve = MakeDeg1PlanarSquarePD();
    ASSERT_TRUE(curve->IsPlanar());

    curve->SetControlPointAt(3, Vector3d(0.0, 1.0, 1.0));
    EXPECT_FALSE(curve->IsPlanar());
    EXPECT_FALSE(curve->TryGetDefinedPlaneNormal().has_value());

    curve->SetControlPointAt(3, Vector3d(0.0, 1.0, 0.0));
    EXPECT_TRUE(curve->IsPlanar());
    const auto normal = curve->TryGetDefinedPlaneNormal();
    ASSERT_TRUE(normal.has_value());
    EXPECT_NEAR(std::abs(normal->z()), 1.0, kTol);
}

// 縮退: 終点制御点を始点に一致させると閉曲線になる
TEST(RationalBSplineCurveSetterTest, SetControlPointAt_ClosesCurve_IsClosedTrue) {
    const auto curve = MakeDeg2BezierPD();
    ASSERT_FALSE(curve->IsClosed());

    curve->SetControlPointAt(2, Vector3d(0.0, 0.0, 0.0));
    EXPECT_TRUE(curve->IsClosed());
}

// 範囲外インデックスは例外 (i=K+1で例外、i=Kで成功)
TEST(RationalBSplineCurveSetterTest,
     SetControlPointAt_ThrowsOutOfRangeWhenIndexTooLarge) {
    const auto curve = MakeDeg2BezierPD();
    EXPECT_THROW(curve->SetControlPointAt(3, Vector3d(0.0, 0.0, 0.0)),
                 std::out_of_range);
    EXPECT_NO_THROW(curve->SetControlPointAt(2, Vector3d(2.0, 0.0, 0.0)));
}

// 同数の制御点全体の差し替えと平面性の再計算
TEST(RationalBSplineCurveSetterTest,
     SetControlPoints_ReplacesAllAndRecomputesPlanarity) {
    const auto curve = MakeDeg1PlanarSquarePD();
    ASSERT_TRUE(curve->IsPlanar());

    // 非平面な4点に差し替え
    curve->SetControlPoints(
        MakeCPs({0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1}));
    ExpectVectorNear(curve->GetControlPointAt(3), Vector3d(1.0, 1.0, 1.0));
    EXPECT_FALSE(curve->IsPlanar());
}

// 列数 (制御点数) が異なる差し替えは例外 (±1列の両側、同数で成功)
TEST(RationalBSplineCurveSetterTest,
     SetControlPoints_ThrowsEntityValueErrorWhenCountDiffers) {
    const auto curve = MakeDeg1PlanarSquarePD();
    EXPECT_THROW(
        curve->SetControlPoints(MakeCPs({0, 0, 0, 1, 0, 0, 1, 1, 0})),
        igesio::EntityValueError);
    EXPECT_THROW(
        curve->SetControlPoints(
            MakeCPs({0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 2, 0})),
        igesio::EntityValueError);
    EXPECT_NO_THROW(curve->SetControlPoints(
        MakeCPs({0, 0, 0, 2, 0, 0, 2, 2, 0, 0, 2, 0})));
}

// 重みの変更が格納値とIsPolynomialに反映される
TEST(RationalBSplineCurveSetterTest, SetWeightAt_UpdatesWeightAndPolynomialFlag) {
    const auto curve = MakeDeg2BezierPD();
    ASSERT_TRUE(curve->IsPolynomial());

    curve->SetWeightAt(1, 2.0);
    EXPECT_NEAR(curve->GetWeightAt(1), 2.0, kTol);
    EXPECT_FALSE(curve->IsPolynomial());
}

// 範囲外インデックスは例外 (i=K+1で例外、i=Kで成功)
TEST(RationalBSplineCurveSetterTest,
     SetWeightAt_ThrowsOutOfRangeWhenIndexTooLarge) {
    const auto curve = MakeDeg2BezierPD();
    EXPECT_THROW(curve->SetWeightAt(3, 1.0), std::out_of_range);
    EXPECT_NO_THROW(curve->SetWeightAt(2, 1.0));
}

// 正でない重みは例外 (w=0は境界外、微小な正値は境界内; 失敗時は値不変)
TEST(RationalBSplineCurveSetterTest,
     SetWeightAt_ThrowsEntityValueErrorWhenNotPositive) {
    const auto curve = MakeDeg2BezierPD();
    EXPECT_THROW(curve->SetWeightAt(1, 0.0), igesio::EntityValueError);
    EXPECT_THROW(curve->SetWeightAt(1, -1.0), igesio::EntityValueError);
    EXPECT_NEAR(curve->GetWeightAt(1), 1.0, kTol);  // 失敗時は変更されない
    EXPECT_NO_THROW(curve->SetWeightAt(1, 1e-12));
}

// 重み全体の差し替え
TEST(RationalBSplineCurveSetterTest, SetWeights_ReplacesAll) {
    const auto curve = MakeDeg2BezierPD();
    curve->SetWeights({1.0, 2.0, 3.0});
    ExpectVectorsNear(curve->Weights(), {1.0, 2.0, 3.0});
}

// サイズが異なる差し替えは例外 (±1個の両側)
TEST(RationalBSplineCurveSetterTest,
     SetWeights_ThrowsEntityValueErrorWhenSizeDiffers) {
    const auto curve = MakeDeg2BezierPD();
    EXPECT_THROW(curve->SetWeights({1.0, 1.0}), igesio::EntityValueError);
    EXPECT_THROW(curve->SetWeights({1.0, 1.0, 1.0, 1.0}),
                 igesio::EntityValueError);
}

// 正でない重みを含む差し替えは例外
TEST(RationalBSplineCurveSetterTest,
     SetWeights_ThrowsEntityValueErrorWhenContainsNonPositive) {
    const auto curve = MakeDeg2BezierPD();
    EXPECT_THROW(curve->SetWeights({1.0, 0.0, 1.0}), igesio::EntityValueError);
    ExpectVectorsNear(curve->Weights(), {1.0, 1.0, 1.0});  // 失敗時は変更されない
}

// 同サイズのノット差し替え (再パラメータ化)
TEST(RationalBSplineCurveSetterTest, SetKnots_ReplacesAll) {
    const auto curve = MakeDeg2BezierPD();
    curve->SetKnots({0.0, 0.0, 0.0, 2.0, 2.0, 2.0});
    ExpectVectorsNear(curve->Knots(), {0.0, 0.0, 0.0, 2.0, 2.0, 2.0});
}

// 境界: 隣接同値 (内部重複ノット) は非減少なので許容
TEST(RationalBSplineCurveSetterTest, SetKnots_EqualAdjacentKnots_NoThrow) {
    const auto curve = MakeDeg2BezierPD();
    EXPECT_NO_THROW(curve->SetKnots({0.0, 0.0, 0.5, 0.5, 1.0, 1.0}));
}

// サイズが異なる差し替えは例外 (±1個の両側)
TEST(RationalBSplineCurveSetterTest,
     SetKnots_ThrowsEntityValueErrorWhenSizeDiffers) {
    const auto curve = MakeDeg2BezierPD();
    EXPECT_THROW(curve->SetKnots({0.0, 0.0, 0.0, 1.0, 1.0}),
                 igesio::EntityValueError);
    EXPECT_THROW(curve->SetKnots({0.0, 0.0, 0.0, 0.5, 1.0, 1.0, 1.0}),
                 igesio::EntityValueError);
}

// 非減少でないノットへの差し替えは例外
TEST(RationalBSplineCurveSetterTest,
     SetKnots_ThrowsEntityValueErrorWhenDecreasing) {
    const auto curve = MakeDeg2BezierPD();
    EXPECT_THROW(curve->SetKnots({0.0, 0.0, 0.0, 1.0, 0.5, 1.0}),
                 igesio::EntityValueError);
    ExpectVectorsNear(curve->Knots(),
                      {0.0, 0.0, 0.0, 1.0, 1.0, 1.0});  // 失敗時は変更されない
}

// パラメータ範囲の変更が反映される
TEST(RationalBSplineCurveSetterTest, SetParameterRange_UpdatesRange) {
    const auto curve = MakeDeg2BezierPD();
    curve->SetParameterRange({0.25, 0.75});
    EXPECT_NEAR(curve->GetParameterRange()[0], 0.25, kTol);
    EXPECT_NEAR(curve->GetParameterRange()[1], 0.75, kTol);
}

// V(0) >= V(1) は例外 (同値は境界外、微小な増加は境界内; 失敗時は値不変)
TEST(RationalBSplineCurveSetterTest,
     SetParameterRange_ThrowsEntityValueErrorWhenStartNotLessThanEnd) {
    const auto curve = MakeDeg2BezierPD();
    EXPECT_THROW(curve->SetParameterRange({0.5, 0.5}),
                 igesio::EntityValueError);
    EXPECT_NEAR(curve->GetParameterRange()[0], 0.0, kTol);  // 失敗時は変更されない
    EXPECT_NEAR(curve->GetParameterRange()[1], 1.0, kTol);
    EXPECT_NO_THROW(curve->SetParameterRange({0.25, 0.25 + 1e-9}));
}

// 周期フラグの設定
TEST(RationalBSplineCurveSetterTest, SetPeriodic_TogglesFlag) {
    const auto curve = MakeDeg2BezierPD();
    ASSERT_FALSE(curve->IsPeriodic());
    curve->SetPeriodic(true);
    EXPECT_TRUE(curve->IsPeriodic());
    curve->SetPeriodic(false);
    EXPECT_FALSE(curve->IsPeriodic());
}

// 曲線の種類 (フォーム番号) の設定
TEST(RationalBSplineCurveSetterTest, SetCurveType_UpdatesFormNumber) {
    const auto curve = MakeDeg2BezierPD();
    ASSERT_EQ(curve->GetCurveType(), RationalBSplineCurveType::kUndetermined);

    EXPECT_TRUE(curve->SetCurveType(RationalBSplineCurveType::kCircularArc));
    EXPECT_EQ(curve->GetCurveType(), RationalBSplineCurveType::kCircularArc);
}

// 列挙範囲外の値はfalseを返しフォーム番号を変更しない
TEST(RationalBSplineCurveSetterTest, SetCurveType_InvalidValue_ReturnsFalse) {
    const auto curve = MakeDeg2BezierPD();
    EXPECT_FALSE(
        curve->SetCurveType(static_cast<RationalBSplineCurveType>(7)));
    EXPECT_EQ(curve->GetCurveType(), RationalBSplineCurveType::kUndetermined);
}



/**
 * SetData (一括差し替え) のテスト
 */

// 構造全体の差し替え (deg2/3点 → deg1/4点); IDとフォーム番号は保持される
TEST(RationalBSplineCurveSetDataTest, SetData_ReplacesWholeStructure_KeepsID) {
    const auto curve = MakeDeg2BezierPD();
    const auto id_before = curve->id_;

    const std::vector<double> knots{0.0, 0.0, 1.0, 2.0, 3.0, 3.0};
    const std::vector<double> weights{1.0, 1.0, 1.0, 1.0};
    const auto cps = MakeCPs({0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0});
    curve->SetData(1, knots, weights, cps, std::array<double, 2>{0.0, 3.0});

    EXPECT_EQ(curve->Degree(), 1);
    EXPECT_EQ(curve->NumControlPoints(), 4);
    ExpectVectorsNear(curve->Knots(), knots);
    ExpectVectorsNear(curve->Weights(), weights);
    ExpectVectorNear(curve->GetControlPointAt(3), Vector3d(0.0, 1.0, 0.0));
    EXPECT_NEAR(curve->GetParameterRange()[0], 0.0, kTol);
    EXPECT_NEAR(curve->GetParameterRange()[1], 3.0, kTol);
    EXPECT_TRUE(curve->ValidatePD().is_valid);
    EXPECT_TRUE(curve->id_ == id_before);
    EXPECT_EQ(curve->GetCurveType(), RationalBSplineCurveType::kUndetermined);
}

// 範囲省略時はノット定義域 [T(0), T(N)] となる (非クランプ一様ノット)
TEST(RationalBSplineCurveSetDataTest, SetData_OmittedRange_UsesKnotDomain) {
    const auto curve = MakeDeg2BezierPD();
    curve->SetData(
        2, {0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0}, {},
        MakeCPs({0, 0, 0, 1, 1, 0, 2, 0, 0, 3, 1, 0, 4, 0, 0}));

    EXPECT_NEAR(curve->GetParameterRange()[0], 2.0, kTol);
    EXPECT_NEAR(curve->GetParameterRange()[1], 5.0, kTol);
}

// 重み省略時は全て1.0 (polynomial形式) となる
TEST(RationalBSplineCurveSetDataTest, SetData_EmptyWeights_AllOnes) {
    const auto curve = MakeDeg2BezierPD();
    curve->SetWeights({1.0, 2.0, 1.0});  // 一旦rationalにしてから差し替え
    curve->SetData(2, {0.0, 0.0, 0.0, 1.0, 1.0, 1.0}, {},
                   MakeCPs({0, 0, 0, 1, 1, 0, 2, 0, 0}));

    ExpectVectorsNear(curve->Weights(), {1.0, 1.0, 1.0});
    EXPECT_TRUE(curve->IsPolynomial());
}

// 差し替え後の制御点から平面性が再計算される
TEST(RationalBSplineCurveSetDataTest, SetData_RecomputesPlanarity) {
    const auto curve = MakeDeg1PlanarSquarePD();
    ASSERT_TRUE(curve->IsPlanar());

    curve->SetData(1, {0.0, 0.0, 1.0, 2.0, 3.0, 3.0}, {},
                   MakeCPs({0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1}));  // 非平面
    EXPECT_FALSE(curve->IsPlanar());

    curve->SetData(1, {0.0, 0.0, 1.0, 2.0, 3.0, 3.0}, {},
                   MakeCPs({0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0}));  // XY平面
    EXPECT_TRUE(curve->IsPlanar());
}

// 不正データでは例外を送出し、状態は一切変更されない (強い例外保証)
TEST(RationalBSplineCurveSetDataTest,
     SetData_ThrowsEntityValueErrorWhenInvalid_StateUnchanged) {
    const auto curve = MakeDeg2BezierPD();

    // ノット数不正 (deg1/4点ならK+M+2=6個必要だが5個)
    EXPECT_THROW(
        curve->SetData(1, {0.0, 0.0, 1.0, 2.0, 2.0}, {},
                       MakeCPs({0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0})),
        igesio::EntityValueError);

    // 全メンバが差し替え前のまま
    EXPECT_EQ(curve->Degree(), 2);
    EXPECT_EQ(curve->NumControlPoints(), 3);
    ExpectVectorsNear(curve->Knots(), {0.0, 0.0, 0.0, 1.0, 1.0, 1.0});
    ExpectVectorsNear(curve->Weights(), {1.0, 1.0, 1.0});
    ExpectVectorNear(curve->GetControlPointAt(1), Vector3d(1.0, 1.0, 0.0));
    EXPECT_NEAR(curve->GetParameterRange()[0], 0.0, kTol);
    EXPECT_NEAR(curve->GetParameterRange()[1], 1.0, kTol);
}
