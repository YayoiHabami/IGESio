/**
 * @file entities/curves/test_parametric_spline_curve.cpp
 * @brief ParametricSplineCurve の GetLinearSegments / GetCornerParams、
 *        およびファクトリ関数 (MakeParametricSplineCurve /
 *        MakeCubicSplineCurve) のテスト
 * @author Yayoi Habami
 * @date 2026-04-10
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/numerics/core/tolerance.h"
#include "igesio/entities/curves/parametric_spline_curve.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using i_ent::ParametricSplineCurve;
using i_ent::ParametricSplineCurveType;
using igesio::Matrix34d;
using igesio::Vector3d;

/// @brief テストの許容誤差
constexpr double kTol = 1e-9;

/// @brief CTYPE=1 (kLinear), H=1, NDIM=3, N=2 の折れ線スプライン
/// @details ブレークポイント [0, 1, 2]、幾何形状 (0,0,0)→(1,0,0)→(1,1,0).
///          GetLinearSegments は [{0,1},{1,2}]、GetCornerParams は [] を返すべき
std::shared_ptr<ParametricSplineCurve> MakeLinearSpline() {
    const auto param = igesio::IGESParameterVector{
        1, 1, 3, 2,          // CTYPE=1 (kLinear), H=1, NDIM=3, N=2
        0.0, 1.0, 2.0,       // T(1), T(2), T(3)
        // セグメント1: C(s)=(s, 0, 0)  (s = u - T(1) = u)
        0.0, 1.0, 0.0, 0.0,  // Ax, Bx, Cx, Dx
        0.0, 0.0, 0.0, 0.0,  // Ay, By, Cy, Dy
        0.0, 0.0, 0.0, 0.0,  // Az, Bz, Cz, Dz
        // セグメント2: C(s)=(1, s, 0)  (s = u - T(2) = u - 1)
        1.0, 0.0, 0.0, 0.0,  // Ax, Bx, Cx, Dx
        0.0, 1.0, 0.0, 0.0,  // Ay, By, Cy, Dy
        0.0, 0.0, 0.0, 0.0,  // Az, Bz, Cz, Dz
        // 末端点 u=T(3)=2, s=1: C=(1,1,0), C'=(0,1,0), C''=C'''=(0,0,0)
        1.0, 0.0, 0.0, 0.0,  // TPX0, TPX1, TPX2, TPX3
        1.0, 1.0, 0.0, 0.0,  // TPY0, TPY1, TPY2, TPY3
        0.0, 0.0, 0.0, 0.0   // TPZ0, TPZ1, TPZ2, TPZ3
    };
    return std::make_shared<ParametricSplineCurve>(param);
}

/// @brief CTYPE=3 (kCubic), H=3, NDIM=3, N=2 の3次スプライン
/// @details 幾何形状は MakeLinearSpline と同じだが CTYPE が kLinear でない.
///          GetLinearSegments は [] を返すべき
std::shared_ptr<ParametricSplineCurve> MakeCubicSpline() {
    const auto param = igesio::IGESParameterVector{
        3, 3, 3, 2,          // CTYPE=3 (kCubic), H=3, NDIM=3, N=2
        0.0, 1.0, 2.0,       // T(1), T(2), T(3)
        // セグメント1
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        // セグメント2
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        // 末端点
        1.0, 0.0, 0.0, 0.0,
        1.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0
    };
    return std::make_shared<ParametricSplineCurve>(param);
}

/// @brief CTYPE=3 (kCubic), H=0, NDIM=3, N=2 の区分定数スプライン
/// @details ブレークポイント [0, 1, 2]、B=C=D=0 のため C'=0 が常に成立.
///          GetCornerParams は内部ブレークポイント [1.0] を返すべき
std::shared_ptr<ParametricSplineCurve> MakeDegree0Spline() {
    const auto param = igesio::IGESParameterVector{
        3, 0, 3, 2,          // CTYPE=3 (kCubic), H=0, NDIM=3, N=2
        0.0, 1.0, 2.0,       // T(1), T(2), T(3)
        // セグメント1: 定数 (0, 0, 0)
        0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        // セグメント2: 定数 (1, 0, 0)
        1.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        // 末端点 u=T(3)=2, s=1: C=(1,0,0), C'=C''=C'''=(0,0,0)
        1.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0
    };
    return std::make_shared<ParametricSplineCurve>(param);
}

/// @brief MakeLinearSplineと同データのセグメント係数 (kLinear, N=2)
/// @return セグメント1: C(s)=(s,0,0)、セグメント2: C(s)=(1,s,0)
std::vector<Matrix34d> LinearSplineSegments() {
    Matrix34d seg1, seg2;
    seg1 << 0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0;
    seg2 << 1.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0;
    return {seg1, seg2};
}

}  // namespace



/// @brief `GetLinearSegments` / `IsInLinearSegment` のテスト
class ParametricSplineCurveLinearSegmentTest : public ::testing::Test {};

/// @brief kLinear 型 (CTYPE=1) は全ブレークポイント区間を返す
TEST(ParametricSplineCurveLinearSegmentTest, LinearType_TwoSegments) {
    auto spline = MakeLinearSpline();
    const auto segs = spline->GetLinearSegments();
    ASSERT_EQ(segs.size(), 2u);
    EXPECT_NEAR(segs[0][0], 0.0, kTol);
    EXPECT_NEAR(segs[0][1], 1.0, kTol);
    EXPECT_NEAR(segs[1][0], 1.0, kTol);
    EXPECT_NEAR(segs[1][1], 2.0, kTol);
}

/// @brief kLinear 以外 (CTYPE=3) は空リストを返す
TEST(ParametricSplineCurveLinearSegmentTest, CubicType_EmptySegments) {
    auto spline = MakeCubicSpline();
    EXPECT_TRUE(spline->GetLinearSegments().empty());
}

/// @brief セグメント内部の点は IsInLinearSegment が true
TEST(ParametricSplineCurveLinearSegmentTest, LinearType_IsInLinearSegment_Interior) {
    auto spline = MakeLinearSpline();
    EXPECT_TRUE(spline->IsInLinearSegment(0.5));
    EXPECT_TRUE(spline->IsInLinearSegment(1.5));
}

/// @brief 内部ブレークポイント (セグメント境界) は両セグメントの端点として true
TEST(ParametricSplineCurveLinearSegmentTest, LinearType_IsInLinearSegment_AtBreakpoint) {
    auto spline = MakeLinearSpline();
    // t=1.0 は Seg1 の終点かつ Seg2 の始点
    EXPECT_TRUE(spline->IsInLinearSegment(1.0));
}

/// @brief パラメータ範囲外は false
TEST(ParametricSplineCurveLinearSegmentTest, LinearType_IsInLinearSegment_OutOfRange) {
    auto spline = MakeLinearSpline();
    EXPECT_FALSE(spline->IsInLinearSegment(2.5));
    EXPECT_FALSE(spline->IsInLinearSegment(-0.1));
}



/// @brief `GetCornerParams` / `IsCorner` のテスト
class ParametricSplineCurveCornerTest : public ::testing::Test {};

/// @brief H=0 (degree=0) では内部ブレークポイントが角点
TEST(ParametricSplineCurveCornerTest, Degree0_OneInternalBreakpoint) {
    auto spline = MakeDegree0Spline();
    const auto corners = spline->GetCornerParams();
    ASSERT_EQ(corners.size(), 1u);
    EXPECT_NEAR(corners[0], 1.0, kTol);
}

/// @brief H=1 では角点なし
TEST(ParametricSplineCurveCornerTest, Degree1_Empty) {
    auto spline = MakeLinearSpline();
    EXPECT_TRUE(spline->GetCornerParams().empty());
}

/// @brief H=3 では角点なし
TEST(ParametricSplineCurveCornerTest, Degree3_Empty) {
    auto spline = MakeCubicSpline();
    EXPECT_TRUE(spline->GetCornerParams().empty());
}

/// @brief H=0 において始点・終点は角点でなく、内部ブレークポイントのみが角点
TEST(ParametricSplineCurveCornerTest, Degree0_EndpointKnots_NotCorners) {
    auto spline = MakeDegree0Spline();
    EXPECT_FALSE(spline->IsCorner(0.0));
    EXPECT_FALSE(spline->IsCorner(2.0));
    EXPECT_TRUE(spline->IsCorner(1.0));
}



/**
 * ValidatePDの重大度 (severity): セグメント間の不連続はkWarning
 */

// セグメント間に関数値の不連続 (seg0終点(1,0,0) != seg1始点(5,0,0)) がある → 折れ線として
// 描画可能なためValidatePDは警告 (kWarning) を出すがis_valid=true (描画ブロックしない)。
TEST(ParametricSplineCurveValidateTest, Discontinuity_IsValidWithWarning) {
    const auto param = igesio::IGESParameterVector{
        1, 1, 3, 2,          // CTYPE=1, H=1, NDIM=3, N=2
        0.0, 1.0, 2.0,       // T(1), T(2), T(3)
        // セグメント1: C(s)=(s, 0, 0) → 終点s=1で (1,0,0)
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        // セグメント2: C(s)=(5, s, 0) → 始点s=0で (5,0,0) ← 前セグメント終点と不連続
        5.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        // 末端点 u=T(3)=2, s=1: C=(5,1,0)
        5.0, 0.0, 0.0, 0.0,
        1.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0
    };
    const ParametricSplineCurve spline(param);
    const auto result = spline.ValidatePD();
    EXPECT_TRUE(result.is_valid);  // 描画はブロックしない
    bool has_warning = false;
    for (const auto& e : result.errors) {
        if (e.severity == igesio::ValidationSeverity::kWarning) has_warning = true;
    }
    EXPECT_TRUE(has_warning);
}



/**
 * エラーケース: コンストラクタ・ValidatePDの例外型
 */

// 境界: パラメータ数が最小値17のすぐ外 (16個) はEntityParameterError
TEST(ParametricSplineCurveErrorTest,
     Constructor_ThrowsEntityParameterErrorWhenTooFewParams) {
    const auto param = igesio::IGESParameterVector{
        1, 1, 3, 1,                                      // CTYPE, H, NDIM, N=1
        0.0, 1.0,                                        // T(1), T(2)
        0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0  // 計16個で打ち切り
    };
    EXPECT_THROW(ParametricSplineCurve spline(param),
                 igesio::EntityParameterError);
}

// セグメント数Nに対してパラメータ数が不足する場合もEntityParameterError
TEST(ParametricSplineCurveErrorTest,
     Constructor_ThrowsEntityParameterErrorWhenInsufficientForSegments) {
    // N=2の必要数 17+13*2=43 に対して17個のみ
    const auto param = igesio::IGESParameterVector{
        1, 1, 3, 2,          // CTYPE, H, NDIM, N=2
        0.0, 1.0, 2.0,       // T(1)-T(3)
        0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0  // 計17個
    };
    EXPECT_THROW(ParametricSplineCurve spline(param),
                 igesio::EntityParameterError);
}

// セグメント数Nが1未満の場合はEntityValueError (数の不足ではなく値の制約違反)
TEST(ParametricSplineCurveErrorTest,
     Constructor_ThrowsEntityValueErrorWhenSegmentCountIsZero) {
    const auto param = igesio::IGESParameterVector{
        1, 1, 3, 0,          // N=0 (1未満)
        0.0, 1.0, 2.0,
        0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0  // 計17個
    };
    EXPECT_THROW(ParametricSplineCurve spline(param), igesio::EntityValueError);
}

// ブレークポイントが狭義単調増加でない場合、ValidatePDはEntityValueErrorを投げる
TEST(ParametricSplineCurveErrorTest,
     ValidatePD_ThrowsEntityValueErrorWhenBreakpointsNotIncreasing) {
    // MakeLinearSplineと同構成だが T = [0, 1, 0.5] (T(3) < T(2))
    const auto param = igesio::IGESParameterVector{
        1, 1, 3, 2,
        0.0, 1.0, 0.5,       // T(3)=0.5 < T(2)=1.0 (非単調)
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0,
        1.0, 0.0, 0.0, 0.0,
        1.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0
    };
    const ParametricSplineCurve spline(param);
    EXPECT_THROW(spline.ValidatePD(), igesio::EntityValueError);
}


/**
 * ファクトリ関数: MakeParametricSplineCurve (型付きコンストラクタ込み)
 */

// 代表値: 型付きパラメータが各アクセサに格納される
TEST(ParametricSplineCurveFactoryTest,
     MakeParametricSplineCurve_StoresTypedParameters) {
    const std::vector<double> breakpoints{0.0, 1.0, 2.0};
    const auto segments = LinearSplineSegments();
    const auto spline = i_ent::MakeParametricSplineCurve(
            ParametricSplineCurveType::kLinear, 1, breakpoints, segments);

    ASSERT_NE(spline, nullptr);
    EXPECT_EQ(spline->GetCurveType(), ParametricSplineCurveType::kLinear);
    EXPECT_EQ(spline->Degree(), 1u);
    EXPECT_EQ(spline->NumberOfSegments(), 2u);
    ASSERT_EQ(spline->Breakpoints().size(), breakpoints.size());
    for (size_t i = 0; i < breakpoints.size(); ++i) {
        EXPECT_DOUBLE_EQ(spline->Breakpoints()[i], breakpoints[i]);
    }
    for (unsigned int i = 0; i < 2u; ++i) {
        EXPECT_TRUE(i_num::IsApproxEqual(
                spline->Coefficients(i), Matrix34d(segments[i]), kTol));
    }
    EXPECT_TRUE(spline->IsValid());
}

// 代表値: 末端のTP値が最終セグメントの係数から自動計算される。
// s=T(2)-T(1)=2 (≠1) のデータを使い、sの掛け忘れを検出できるようにする
TEST(ParametricSplineCurveFactoryTest,
     MakeParametricSplineCurve_ComputesEndDerivatives) {
    Matrix34d seg;
    seg << 1.0,  2.0,  3.0,  4.0,    // x: A, B, C, D
          -1.0,  0.5, -2.0,  1.0,    // y
           0.0,  1.0,  0.0, -0.5;    // z
    const auto spline = i_ent::MakeParametricSplineCurve(
            ParametricSplineCurveType::kCubic, 3, {1.0, 3.0}, {seg});

    // s=2における解析値 (TP0=A+Bs+Cs^2+Ds^3, TP1=B+2Cs+3Ds^2,
    //                    TP2=C+3Ds, TP3=D)
    const std::array<double, 12> expected{
            49.0, 62.0, 27.0,  4.0,   // TPX0..TPX3
             0.0,  4.5,  4.0,  1.0,   // TPY0..TPY3
            -2.0, -5.0, -3.0, -0.5};  // TPZ0..TPZ3
    auto params = spline->GetParameters();
    ASSERT_EQ(params.size(), 30u);  // 17 + 13*1
    for (size_t k = 0; k < expected.size(); ++k) {
        // TP値はPDパラメータの末尾12個 (インデックス18~29)
        EXPECT_DOUBLE_EQ(params.access_as<double>(18 + k), expected[k]);
    }
}

// 等価性: 生PDを手組みした場合 (MakeLinearSpline) と同じ曲線になる
TEST(ParametricSplineCurveFactoryTest,
     MakeParametricSplineCurve_MatchesHandBuiltPDEntity) {
    const auto by_factory = i_ent::MakeParametricSplineCurve(
            ParametricSplineCurveType::kLinear, 1,
            {0.0, 1.0, 2.0}, LinearSplineSegments());
    const auto by_pd = MakeLinearSpline();

    const auto range_f = by_factory->GetParameterRange();
    const auto range_p = by_pd->GetParameterRange();
    EXPECT_DOUBLE_EQ(range_f[0], range_p[0]);
    EXPECT_DOUBLE_EQ(range_f[1], range_p[1]);

    for (const double t : {0.0, 0.5, 1.0, 1.5, 2.0}) {
        const auto d_f = by_factory->TryGetDefinedDerivatives(t, 1);
        const auto d_p = by_pd->TryGetDefinedDerivatives(t, 1);
        ASSERT_TRUE(d_f.has_value() && d_p.has_value());
        EXPECT_TRUE(i_num::IsApproxEqual((*d_f)[0], (*d_p)[0], kTol));
        EXPECT_TRUE(i_num::IsApproxEqual((*d_f)[1], (*d_p)[1], kTol));
    }
}

// 代表値: n_dim=2 (平面上の曲線) が受理され、PDのNDIMに反映される
TEST(ParametricSplineCurveFactoryTest,
     MakeParametricSplineCurve_AcceptsPlanarDimension) {
    const auto spline = i_ent::MakeParametricSplineCurve(
            ParametricSplineCurveType::kLinear, 1,
            {0.0, 1.0, 2.0}, LinearSplineSegments(), 2);

    auto params = spline->GetParameters();
    EXPECT_EQ(params.access_as<int>(2), 2);  // NDIM
    EXPECT_TRUE(spline->IsValid());
}

// エラー+境界: セグメント0個→throw、1個 (最小) →受理
TEST(ParametricSplineCurveFactoryTest,
     MakeParametricSplineCurve_ThrowsEntityValueErrorWhenNoSegments) {
    EXPECT_THROW(i_ent::MakeParametricSplineCurve(
            ParametricSplineCurveType::kCubic, 0, {0.0, 1.0}, {}),
            igesio::EntityValueError);
    EXPECT_NO_THROW(i_ent::MakeParametricSplineCurve(
            ParametricSplineCurveType::kCubic, 0, {0.0, 1.0},
            {Matrix34d::Zero()}));
}

// エラー: ブレークポイント数がN+1でない場合は棄却される
TEST(ParametricSplineCurveFactoryTest,
     MakeParametricSplineCurve_ThrowsEntityValueErrorWhenBreakpointCountMismatch) {
    EXPECT_THROW(i_ent::MakeParametricSplineCurve(
            ParametricSplineCurveType::kCubic, 0, {0.0, 1.0, 2.0},
            {Matrix34d::Zero()}),
            igesio::EntityValueError);
    EXPECT_THROW(i_ent::MakeParametricSplineCurve(
            ParametricSplineCurveType::kCubic, 0, {0.0},
            {Matrix34d::Zero()}),
            igesio::EntityValueError);
}

// エラー+境界精度: 単調性チェックは厳密比較 (同値→棄却、わずかでも増加→受理)
TEST(ParametricSplineCurveFactoryTest,
     MakeParametricSplineCurve_ThrowsEntityValueErrorWhenBreakpointsNotIncreasing) {
    const auto segments = LinearSplineSegments();
    EXPECT_THROW(i_ent::MakeParametricSplineCurve(
            ParametricSplineCurveType::kLinear, 1, {0.0, 1.0, 1.0}, segments),
            igesio::EntityValueError);
    EXPECT_THROW(i_ent::MakeParametricSplineCurve(
            ParametricSplineCurveType::kLinear, 1, {0.0, 1.0, 0.5}, segments),
            igesio::EntityValueError);
    EXPECT_NO_THROW(i_ent::MakeParametricSplineCurve(
            ParametricSplineCurveType::kLinear, 1,
            {0.0, 1.0, 1.0 + 1e-12}, segments));
}

// エラー+境界: degree=4→throw、degree=3 (最大) →受理
TEST(ParametricSplineCurveFactoryTest,
     MakeParametricSplineCurve_ThrowsEntityValueErrorWhenDegreeExceedsThree) {
    EXPECT_THROW(i_ent::MakeParametricSplineCurve(
            ParametricSplineCurveType::kCubic, 4, {0.0, 1.0},
            {Matrix34d::Zero()}),
            igesio::EntityValueError);
    EXPECT_NO_THROW(i_ent::MakeParametricSplineCurve(
            ParametricSplineCurveType::kCubic, 3, {0.0, 1.0},
            {Matrix34d::Zero()}));
}

// エラー: n_dimが2でも3でもない場合は棄却される
TEST(ParametricSplineCurveFactoryTest,
     MakeParametricSplineCurve_ThrowsEntityValueErrorWhenDimensionInvalid) {
    EXPECT_THROW(i_ent::MakeParametricSplineCurve(
            ParametricSplineCurveType::kCubic, 0, {0.0, 1.0},
            {Matrix34d::Zero()}, 1),
            igesio::EntityValueError);
    EXPECT_THROW(i_ent::MakeParametricSplineCurve(
            ParametricSplineCurveType::kCubic, 0, {0.0, 1.0},
            {Matrix34d::Zero()}, 4),
            igesio::EntityValueError);
}



/**
 * ファクトリ関数: MakeCubicSplineCurve
 */

// 代表値: 非一様ブレークポイントで通過点と接線が補間される
// (内部ブレークポイントでの位置・勾配の連続性確認を含む)
TEST(ParametricSplineCurveFactoryTest,
     MakeCubicSplineCurve_InterpolatesPointsAndTangents) {
    const std::vector<Vector3d> points{
            {0.0, 0.0, 0.0}, {1.0, 2.0, 0.0}, {4.0, 2.0, -2.0}};
    const std::vector<Vector3d> tangents{
            {1.0, 0.0, 0.0}, {1.0, 1.0, 0.0}, {2.0, 0.0, -1.0}};
    const std::vector<double> breakpoints{0.0, 1.0, 3.0};
    const auto spline = i_ent::MakeCubicSplineCurve(
            points, tangents, breakpoints);

    ASSERT_NE(spline, nullptr);
    EXPECT_EQ(spline->GetCurveType(), ParametricSplineCurveType::kCubic);
    EXPECT_EQ(spline->Degree(), 1u);
    EXPECT_EQ(spline->NumberOfSegments(), 2u);
    EXPECT_TRUE(spline->IsValid());

    for (size_t i = 0; i < points.size(); ++i) {
        const auto derivs =
                spline->TryGetDefinedDerivatives(breakpoints[i], 1);
        ASSERT_TRUE(derivs.has_value()) << "at T(" << i << ")";
        EXPECT_TRUE(i_num::IsApproxEqual((*derivs)[0], points[i], kTol))
                << "point at T(" << i << ")";
        EXPECT_TRUE(i_num::IsApproxEqual((*derivs)[1], tangents[i], kTol))
                << "tangent at T(" << i << ")";
    }
}

// 精度: 1セグメント (最小構成、h=2) の係数がエルミート変換の解析値と一致する
TEST(ParametricSplineCurveFactoryTest,
     MakeCubicSplineCurve_SingleSegmentMatchesHermiteFormula) {
    // P0=(0,0,0), P1=(2,2,0), M0=(1,0,1), M1=(3,2,0), h=2
    const auto spline = i_ent::MakeCubicSplineCurve(
            {{0.0, 0.0, 0.0}, {2.0, 2.0, 0.0}},
            {{1.0, 0.0, 1.0}, {3.0, 2.0, 0.0}},
            {0.0, 2.0});

    // A=P0, B=M0, C=3dP/h^2-(2M0+M1)/h, D=-2dP/h^3+(M0+M1)/h^2
    Matrix34d expected;
    expected << 0.0, 1.0, -1.0,  0.5,
                0.0, 0.0,  0.5,  0.0,
                0.0, 1.0, -1.0,  0.25;
    EXPECT_TRUE(i_num::IsApproxEqual(
            spline->Coefficients(0), expected, kTol));

    // 終端 u=h での位置・接線が P1, M1 に一致する (エンドツーエンド確認)
    const auto derivs = spline->TryGetDefinedDerivatives(2.0, 1);
    ASSERT_TRUE(derivs.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(
            (*derivs)[0], Vector3d(2.0, 2.0, 0.0), kTol));
    EXPECT_TRUE(i_num::IsApproxEqual(
            (*derivs)[1], Vector3d(3.0, 2.0, 0.0), kTol));
}

// 縮退: 全接線をコード方向に揃えると3次が直線に退化する (C=D=0)
TEST(ParametricSplineCurveFactoryTest,
     MakeCubicSplineCurve_StraightLineWhenTangentsMatchChord) {
    // コード (2,4,6)、h=2 → 接線 M = dP/h = (1,2,3)
    const auto spline = i_ent::MakeCubicSplineCurve(
            {{0.0, 0.0, 0.0}, {2.0, 4.0, 6.0}},
            {{1.0, 2.0, 3.0}, {1.0, 2.0, 3.0}},
            {0.0, 2.0});

    const auto coeffs = spline->Coefficients(0);
    EXPECT_NEAR(coeffs.col(2).norm(), 0.0, kTol);  // C
    EXPECT_NEAR(coeffs.col(3).norm(), 0.0, kTol);  // D
}

// エラー+境界: 1点→throw、2点 (最小) →受理
TEST(ParametricSplineCurveFactoryTest,
     MakeCubicSplineCurve_ThrowsEntityValueErrorWhenFewerThanTwoPoints) {
    EXPECT_THROW(i_ent::MakeCubicSplineCurve(
            {{0.0, 0.0, 0.0}}, {{1.0, 0.0, 0.0}}, {0.0}),
            igesio::EntityValueError);
    EXPECT_NO_THROW(i_ent::MakeCubicSplineCurve(
            {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}},
            {{1.0, 0.0, 0.0}, {1.0, 0.0, 0.0}},
            {0.0, 1.0}));
}

// エラー: points/tangents/breakpointsのサイズ不一致は棄却される
TEST(ParametricSplineCurveFactoryTest,
     MakeCubicSplineCurve_ThrowsEntityValueErrorWhenSizesMismatch) {
    const std::vector<Vector3d> points{
            {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {2.0, 1.0, 0.0}};
    // tangentsが2個
    EXPECT_THROW(i_ent::MakeCubicSplineCurve(
            points, {{1.0, 0.0, 0.0}, {1.0, 0.0, 0.0}}, {0.0, 1.0, 2.0}),
            igesio::EntityValueError);
    // breakpointsが2個
    EXPECT_THROW(i_ent::MakeCubicSplineCurve(
            points,
            {{1.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {1.0, 0.0, 0.0}},
            {0.0, 1.0}),
            igesio::EntityValueError);
}

// エラー: ブレークポイントが狭義単調増加でない場合は棄却される
TEST(ParametricSplineCurveFactoryTest,
     MakeCubicSplineCurve_ThrowsEntityValueErrorWhenBreakpointsNotIncreasing) {
    const std::vector<Vector3d> points{{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}};
    const std::vector<Vector3d> tangents{{1.0, 0.0, 0.0}, {1.0, 0.0, 0.0}};
    EXPECT_THROW(i_ent::MakeCubicSplineCurve(points, tangents, {0.0, 0.0}),
                 igesio::EntityValueError);
    EXPECT_THROW(i_ent::MakeCubicSplineCurve(points, tangents, {1.0, 0.0}),
                 igesio::EntityValueError);
}
