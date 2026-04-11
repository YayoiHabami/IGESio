/**
 * @file tests/entities/curves/test_rational_b_spline_curve.cpp
 * @brief RationalBSplineCurve (Type 126) エンティティの直線部・角点テスト
 * @author Yayoi Habami
 * @date 2026-04-10
 * @copyright 2026 Yayoi Habami
 *
 * テスト対象:
 *   [RationalBSplineCurve オーバライド]
 *     GetLinearSegments(), IsInLinearSegment(t, eps)
 *     GetCornerParams(),   IsCorner(t, eps)
 *   [ICurve デフォルト実装; RationalBSplineCurve の挙動を通じて確認]
 *     TryGetSignedCurvature(t, n)
 *
 * NOTE:
 *   - GetLinearSegments() は degree_ == 1 のときのみ非空を返す
 *   - GetCornerParams() は内部ノットの重複度 >= degree_ のものを角点とする
 *     (端点判定の eps = 1e-12 は GetCornerParams の内部固定値)
 *   - degree_ == 0 はコンストラクタが DataFormatError をスローするため
 *     Degree0_NoCorners テストは構成不可
 *
 * TODO:
 *   - TryGetSignedCurvature の通常点（circle NURBS での κ_s = +1/R）テスト
 *   - LeftTangentAt / RightTangentAt の解析値との数値比較テスト
 */
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

#include "igesio/common/iges_parameter_vector.h"
#include "igesio/numerics/tolerance.h"
#include "igesio/entities/curves/rational_b_spline_curve.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using igesio::Vector3d;
using i_ent::RationalBSplineCurve;
constexpr double kPi = igesio::kPi;
/// @brief 浮動小数点比較の許容誤差
constexpr double kTol = 1e-9;



/**
 * テスト用インスタンスのファクトリ関数
 *
 * IGES 126 パラメータ形式:
 *   K, M, PROP1-4(bool×4), ノット(K+M+2個), 重み(K+1個),
 *   制御点(3×(K+1)個), V(0), V(1), 法線(3個)
 */

/// @brief degree=1 NURBS（直線上）
/// K=2, M=1, ノット=[0,0,1,2,2], V=[0,2]
/// P0=(0,0,0), P1=(1,0,0), P2=(2,0,0) (共線)
/// GetLinearSegments: [{0,1},{1,2}]
/// GetCornerParams:   [1.0] (内部ノット t=1, mult=1 >= degree=1)
/// LeftTangentAt(1-h) = RightTangentAt(1+h) = (1,0,0) (直線上なので外角=0)
std::shared_ptr<RationalBSplineCurve> MakeNURBS_Deg1_Straight() {
    const auto param = igesio::IGESParameterVector{
        2, 1,                       // K=2, M=1
        false, false, true, false,  // PROP1-4
        0.0, 0.0, 1.0, 2.0, 2.0,    // ノット (K+M+2=5 個)
        1.0, 1.0, 1.0,              // 重み (K+1=3 個)
        0.0, 0.0, 0.0,              // P0 = (0,0,0)
        1.0, 0.0, 0.0,              // P1 = (1,0,0)
        2.0, 0.0, 0.0,              // P2 = (2,0,0)
        0.0, 2.0,                   // V(0)=0, V(1)=2
        0.0, 0.0, 1.0               // 法線
    };
    return std::make_shared<RationalBSplineCurve>(param);
}

/// @brief degree=1 NURBS（左直角折れ）
/// K=2, M=1, ノット=[0,0,1,2,2], V=[0,2]
/// P0=(0,0,0), P1=(1,0,0), P2=(1,1,0) (t=1で左90°折れ)
/// GetLinearSegments: [{0,1},{1,2}]
/// GetCornerParams:   [1.0]
/// CornerExteriorAngle(1.0, n=(0,0,1)) = +π/2 → TryGetSignedCurvature = +∞
std::shared_ptr<RationalBSplineCurve> MakeNURBS_Deg1_LeftTurn() {
    const auto param = igesio::IGESParameterVector{
        2, 1,                        // K=2, M=1
        false, false, true, false,   // PROP1-4
        0.0, 0.0, 1.0, 2.0, 2.0,     // ノット
        1.0, 1.0, 1.0,               // 重み
        0.0, 0.0, 0.0,               // P0 = (0,0,0)
        1.0, 0.0, 0.0,               // P1 = (1,0,0)
        1.0, 1.0, 0.0,               // P2 = (1,1,0)
        0.0, 2.0,                    // V=[0,2]
        0.0, 0.0, 1.0                // 法線
    };
    return std::make_shared<RationalBSplineCurve>(param);
}

/// @brief degree=1 NURBS（内部重複ノット; ゼロスパン区間あり）
/// K=3, M=1, ノット=[0,0,0.5,0.5,1,1], V=[0,1]
/// P0=(0,0,0), P1=(0.5,0,0), P2=(0.5,0.5,0), P3=(1,0.5,0)
/// GetLinearSegments: [{0,0.5},{0.5,1}] (ゼロスパン [0.5,0.5] は除外)
/// GetCornerParams:   [0.5] (mult=2 >= degree=1)
std::shared_ptr<RationalBSplineCurve> MakeNURBS_Deg1_RepeatedKnot() {
    const auto param = igesio::IGESParameterVector{
        3, 1,                          // K=3, M=1
        false, false, true, false,     // PROP1-4
        0.0, 0.0, 0.5, 0.5, 1.0, 1.0,  // ノット (K+M+2=6 個)
        1.0, 1.0, 1.0, 1.0,            // 重み (K+1=4 個)
        0.0, 0.0, 0.0,                 // P0
        0.5, 0.0, 0.0,                 // P1
        0.5, 0.5, 0.0,                 // P2
        1.0, 0.5, 0.0,                 // P3
        0.0, 1.0,                      // V=[0,1]
        0.0, 0.0, 1.0                  // 法線
    };
    return std::make_shared<RationalBSplineCurve>(param);
}

/// @brief degree=2 NURBS（単純 Bezier; 内部ノットなし）
/// K=2, M=2, ノット=[0,0,0,1,1,1], V=[0,1]
/// GetLinearSegments: [] (degree != 1)
/// GetCornerParams:   [] (内部ノットなし)
std::shared_ptr<RationalBSplineCurve> MakeNURBS_Deg2_Bezier() {
    const auto param = igesio::IGESParameterVector{
        2, 2,                          // K=2, M=2
        false, false, true, false,     // PROP1-4
        0.0, 0.0, 0.0, 1.0, 1.0, 1.0,  // ノット (K+M+2=6 個)
        1.0, 1.0, 1.0,                 // 重み (K+1=3 個)
        0.0, 0.0, 0.0,  // P0
        1.0, 1.0, 0.0,  // P1
        2.0, 0.0, 0.0,  // P2
        0.0, 1.0,       // V=[0,1]
        0.0, 0.0, 1.0   // 法線
    };
    return std::make_shared<RationalBSplineCurve>(param);
}

/// @brief degree=2 NURBS（内部ノット重複度=2; 角点あり）
/// K=4, M=2, ノット=[0,0,0,1,1,2,2,2], V=[0,2]
/// GetLinearSegments: [] (degree != 1)
/// GetCornerParams:   [1.0] (mult=2 >= degree=2)
std::shared_ptr<RationalBSplineCurve> MakeNURBS_Deg2_FullMult() {
    const auto param = igesio::IGESParameterVector{
        4, 2,                                    // K=4, M=2
        false, false, true, false,               // PROP1-4
        0.0, 0.0, 0.0, 1.0, 1.0, 2.0, 2.0, 2.0,  // ノット (K+M+2=8 個)
        1.0, 1.0, 1.0, 1.0, 1.0,                 // 重み (K+1=5 個)
        0.0, 0.0, 0.0,  // P0
        0.5, 0.5, 0.0,  // P1
        1.0, 0.0, 0.0,  // P2 (内部ノット t=1 に対応)
        1.5, 0.5, 0.0,  // P3
        2.0, 0.0, 0.0,  // P4
        0.0, 2.0,       // V=[0,2]
        0.0, 0.0, 1.0   // 法線
    };
    return std::make_shared<RationalBSplineCurve>(param);
}

/// @brief degree=2 NURBS（内部ノット重複度=1; C^1連続, 角点なし）
/// K=3, M=2, ノット=[0,0,0,1,2,2,2], V=[0,2]
/// GetLinearSegments: []
/// GetCornerParams:   [] (mult=1 < degree=2 → C^1, 角点でない)
std::shared_ptr<RationalBSplineCurve> MakeNURBS_Deg2_PartialMult() {
    const auto param = igesio::IGESParameterVector{
        3, 2,                               // K=3, M=2
        false, false, true, false,          // PROP1-4
        0.0, 0.0, 0.0, 1.0, 2.0, 2.0, 2.0,  // ノット (K+M+2=7 個)
        1.0, 1.0, 1.0, 1.0,                 // 重み (K+1=4 個)
        0.0, 0.0, 0.0,  // P0
        0.5, 0.5, 0.0,  // P1
        1.5, 0.5, 0.0,  // P2
        2.0, 0.0, 0.0,  // P3
        0.0, 2.0,       // V=[0,2]
        0.0, 0.0, 1.0   // 法線
    };
    return std::make_shared<RationalBSplineCurve>(param);
}

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

}  // namespace



/**
 * GetLinearSegments / IsInLinearSegment のテスト
 */

// degree=1, 内部ノット 1 個: 2セグメントが返る
TEST(RationalBSplineCurveLinearSegmentTest, Degree1_TwoSegments) {
    auto nurbs = MakeNURBS_Deg1_Straight();
    const auto segs = nurbs->GetLinearSegments();

    // ノット [0,0,1,2,2] → 非ゼロスパン: {0,1}, {1,2}
    ASSERT_EQ(segs.size(), 2u);
    EXPECT_NEAR(segs[0][0], 0.0, kTol);
    EXPECT_NEAR(segs[0][1], 1.0, kTol);
    EXPECT_NEAR(segs[1][0], 1.0, kTol);
    EXPECT_NEAR(segs[1][1], 2.0, kTol);
}

// degree=2: 空リスト（直線部なし）
TEST(RationalBSplineCurveLinearSegmentTest, Degree2_EmptySegments) {
    auto nurbs = MakeNURBS_Deg2_Bezier();
    EXPECT_TRUE(nurbs->GetLinearSegments().empty());
}

// degree=1, 内部重複ノット: ゼロスパン区間 [0.5,0.5] が除外される
TEST(RationalBSplineCurveLinearSegmentTest, Degree1_ZeroSpanKnot_Excluded) {
    auto nurbs = MakeNURBS_Deg1_RepeatedKnot();
    const auto segs = nurbs->GetLinearSegments();

    // ノット [0,0,0.5,0.5,1,1] → 非ゼロスパン: {0,0.5}, {0.5,1}
    // スパン長0の [0.5,0.5] 対は除外される
    ASSERT_EQ(segs.size(), 2u);
    EXPECT_NEAR(segs[0][0], 0.0, kTol);
    EXPECT_NEAR(segs[0][1], 0.5, kTol);
    EXPECT_NEAR(segs[1][0], 0.5, kTol);
    EXPECT_NEAR(segs[1][1], 1.0, kTol);
}

// IsInLinearSegment: degree=1 の各種パラメータ値
TEST(RationalBSplineCurveLinearSegmentTest, Degree1_IsInLinearSegment) {
    auto nurbs = MakeNURBS_Deg1_Straight();  // V=[0,2], セグメント {0,1},{1,2}

    // セグメント内点 → true
    EXPECT_TRUE(nurbs->IsInLinearSegment(0.5));   // 第1セグメント中点
    EXPECT_TRUE(nurbs->IsInLinearSegment(1.5));   // 第2セグメント中点
    // 境界値（端点・接合点）→ true
    EXPECT_TRUE(nurbs->IsInLinearSegment(0.0));   // V(0)
    EXPECT_TRUE(nurbs->IsInLinearSegment(1.0));   // 内部ノット（セグメント接続点）
    EXPECT_TRUE(nurbs->IsInLinearSegment(2.0));   // V(1)
    // 範囲外 → false
    EXPECT_FALSE(nurbs->IsInLinearSegment(-0.5));
    EXPECT_FALSE(nurbs->IsInLinearSegment(2.5));
}

// IsInLinearSegment: degree=2 は常に false
TEST(RationalBSplineCurveLinearSegmentTest, Degree2_IsInLinearSegment_AlwaysFalse) {
    auto nurbs = MakeNURBS_Deg2_Bezier();  // V=[0,1]

    EXPECT_FALSE(nurbs->IsInLinearSegment(0.0));
    EXPECT_FALSE(nurbs->IsInLinearSegment(0.5));
    EXPECT_FALSE(nurbs->IsInLinearSegment(1.0));
}



/**
 * GetCornerParams / IsCorner のテスト
 */

// degree=1, 単純内部ノット: 内部ノットが角点
TEST(RationalBSplineCurveCornerTest, Degree1_InternalKnot) {
    auto nurbs = MakeNURBS_Deg1_Straight();
    const auto corners = nurbs->GetCornerParams();

    ASSERT_EQ(corners.size(), 1u);
    EXPECT_NEAR(corners[0], 1.0, kTol);
    EXPECT_TRUE(nurbs->IsCorner(1.0));
}

// degree=2, 内部ノット重複度=2 (= degree): 角点として検出
TEST(RationalBSplineCurveCornerTest, Degree2_FullMultiplicity) {
    auto nurbs = MakeNURBS_Deg2_FullMult();
    const auto corners = nurbs->GetCornerParams();

    ASSERT_EQ(corners.size(), 1u);
    EXPECT_NEAR(corners[0], 1.0, kTol);
    EXPECT_TRUE(nurbs->IsCorner(1.0));
}

// degree=2, 内部ノット重複度=1 (< degree): 角点として検出されない (C^1 連続)
TEST(RationalBSplineCurveCornerTest, Degree2_PartialMultiplicity_NotCorner) {
    auto nurbs = MakeNURBS_Deg2_PartialMult();
    const auto corners = nurbs->GetCornerParams();

    EXPECT_TRUE(corners.empty());
    // t=1.0 は内部ノットだが mult=1 < degree=2 → 角点でない
    EXPECT_FALSE(nurbs->IsCorner(1.0));
}

// 端点ノット V(0), V(1) は角点から除外される
TEST(RationalBSplineCurveCornerTest, EndpointKnots_NotCorners) {
    auto nurbs = MakeNURBS_Deg1_Straight();  // V=[0,2], 角点=[1.0]

    // V(0)=0, V(1)=2 はノットベクトルに含まれるが角点ではない
    EXPECT_FALSE(nurbs->IsCorner(0.0));
    EXPECT_FALSE(nurbs->IsCorner(2.0));
    // 角点は内部ノット t=1.0 のみ
    EXPECT_TRUE(nurbs->IsCorner(1.0));
}

// degree=1, 内部重複ノット: mult=2 >= degree=1 → 角点
TEST(RationalBSplineCurveCornerTest, Degree1_RepeatedKnot_IsCorner) {
    auto nurbs = MakeNURBS_Deg1_RepeatedKnot();  // 内部ノット t=0.5, mult=2
    const auto corners = nurbs->GetCornerParams();

    ASSERT_EQ(corners.size(), 1u);
    EXPECT_NEAR(corners[0], 0.5, kTol);
    EXPECT_TRUE(nurbs->IsCorner(0.5));
}

// IsCorner の eps 境界テスト
TEST(RationalBSplineCurveCornerTest, IsCorner_EpsBoundary) {
    auto nurbs = MakeNURBS_Deg1_Straight();  // 角点 t=1.0
    const double custom_eps = 1e-6;

    EXPECT_TRUE(nurbs->IsCorner(1.0 + custom_eps / 2.0, custom_eps));
    EXPECT_TRUE(nurbs->IsCorner(1.0 - custom_eps / 2.0, custom_eps));
    EXPECT_FALSE(nurbs->IsCorner(1.0 + custom_eps * 2.0, custom_eps));
    EXPECT_FALSE(nurbs->IsCorner(1.0 - custom_eps * 2.0, custom_eps));
}

// セグメント内点（非角点）は IsCorner が false
TEST(RationalBSplineCurveCornerTest, NonCornerInterior_NotCorner) {
    auto nurbs = MakeNURBS_Deg1_Straight();

    EXPECT_FALSE(nurbs->IsCorner(0.5));   // 第1セグメント内部
    EXPECT_FALSE(nurbs->IsCorner(1.5));   // 第2セグメント内部
}

// degree=2, 内部ノットなし: 空リスト
TEST(RationalBSplineCurveCornerTest, Degree2_NoInternalKnots_Empty) {
    auto nurbs = MakeNURBS_Deg2_Bezier();

    EXPECT_TRUE(nurbs->GetCornerParams().empty());
    EXPECT_FALSE(nurbs->IsCorner(0.0));
    EXPECT_FALSE(nurbs->IsCorner(0.5));
    EXPECT_FALSE(nurbs->IsCorner(1.0));
}



/**
 * TryGetSignedCurvature のテスト
 * 分岐: (1) ゼロ法線   → nullopt
 *       (2) IsCorner かつ外角>0 → +∞
 *       (3) IsCorner かつ外角<0 → -∞
 *       (4) IsCorner かつ外角=0 → 0.0
 *       (5) IsInLinearSegment  → 0.0
 *       (6) その他（滑らか）   → 導関数から計算
 */

// ゼロ法線 → nullopt
TEST(RationalBSplineCurveSignedCurvatureTest, ZeroNormal_Nullopt) {
    auto nurbs = MakeNURBS_Deg1_Straight();
    const Vector3d zero(0.0, 0.0, 0.0);

    EXPECT_FALSE(nurbs->TryGetSignedCurvature(0.5, zero).has_value());
    EXPECT_FALSE(nurbs->TryGetSignedCurvature(1.0, zero).has_value());
}

// degree=1 直線セグメント内点 → 0.0 (IsInLinearSegment=true 経路)
TEST(RationalBSplineCurveSignedCurvatureTest, LinearSegment_Zero) {
    auto nurbs = MakeNURBS_Deg1_Straight();
    const Vector3d normal(0.0, 0.0, 1.0);

    const auto kappa1 = nurbs->TryGetSignedCurvature(0.5, normal);
    const auto kappa2 = nurbs->TryGetSignedCurvature(1.5, normal);

    ASSERT_TRUE(kappa1.has_value());
    ASSERT_TRUE(kappa2.has_value());
    EXPECT_NEAR(*kappa1, 0.0, kTol);
    EXPECT_NEAR(*kappa2, 0.0, kTol);
}

// degree=1 直線上コーナー（外角=0） → 0.0
// NOTE: IsCorner=true だが LeftTangent=RightTangent=(1,0,0) → 外角=0 → κ_s=0
TEST(RationalBSplineCurveSignedCurvatureTest, StraightCorner_Zero) {
    auto nurbs = MakeNURBS_Deg1_Straight();  // P0,P1,P2 共線
    const Vector3d normal(0.0, 0.0, 1.0);

    const auto kappa = nurbs->TryGetSignedCurvature(1.0, normal);

    ASSERT_TRUE(kappa.has_value());
    EXPECT_NEAR(*kappa, 0.0, kTol);
}

// degree=1 左折コーナー（外角=+π/2） → +∞
TEST(RationalBSplineCurveSignedCurvatureTest, LeftTurnCorner_PlusInf) {
    auto nurbs = MakeNURBS_Deg1_LeftTurn();
    const Vector3d normal(0.0, 0.0, 1.0);

    const auto kappa = nurbs->TryGetSignedCurvature(1.0, normal);

    ASSERT_TRUE(kappa.has_value());
    EXPECT_TRUE(std::isinf(*kappa) && *kappa > 0.0);
}

// degree=1 直線セグメントの両端点（非角点かつ直線部内） → 0.0
TEST(RationalBSplineCurveSignedCurvatureTest, LinearSegment_StartEnd_Zero) {
    auto nurbs = MakeNURBS_Deg1_Straight();  // V=[0,2]
    const Vector3d normal(0.0, 0.0, 1.0);

    // t=0: IsCorner(0)=false, IsInLinearSegment(0)=true → 0.0
    const auto kappa_start = nurbs->TryGetSignedCurvature(0.0, normal);
    // t=2: IsCorner(2)=false, IsInLinearSegment(2)=true → 0.0
    const auto kappa_end = nurbs->TryGetSignedCurvature(2.0, normal);

    ASSERT_TRUE(kappa_start.has_value());
    ASSERT_TRUE(kappa_end.has_value());
    EXPECT_NEAR(*kappa_start, 0.0, kTol);
    EXPECT_NEAR(*kappa_end,   0.0, kTol);
}



/**
 * NURBSパラメータコンストラクタのテスト
 *
 * RationalBSplineCurve(k, m, knots, weights, control_points,
 *                      parameter_range, is_periodic) の検証.
 *
 * 確認項目:
 *   1. 保存値 — Degree, Knots, Weights, ControlPoints, GetParameterRange
 *   2. 点評価の等価性 — IGESParameterVector コンストラクタと同一結果
 *   3. 平面性の整合 — ValidatePD() を通じた間接確認
 *   4. IsPolynomial / IsPeriodic
 *   5. GetLinearSegments / GetCornerParams との統合
 */

// 保存値が正しく設定されているか確認
// K=2, M=2 のクランプ Bezier で全フィールドを検証する
TEST(RationalBSplineCurveNurbsCtorTest, StoredValues_Degree2Bezier) {
    // MakeNURBS_Deg2_Bezier と同一パラメータ
    const unsigned int k = 2, m = 2;
    const std::vector<double> knots   = {0.0, 0.0, 0.0, 1.0, 1.0, 1.0};
    const std::vector<double> weights = {1.0, 1.0, 1.0};
    const auto cp = MakeCPs({
        0.0, 0.0, 0.0,   // P0
        1.0, 1.0, 0.0,   // P1
        2.0, 0.0, 0.0,   // P2
    });
    const std::array<double, 2> param_range = {0.0, 1.0};

    const RationalBSplineCurve nurbs(k, m, knots, weights, cp, param_range);

    EXPECT_TRUE(nurbs.ValidatePD().is_valid);
    EXPECT_EQ(nurbs.Degree(), static_cast<int>(m));

    ASSERT_EQ(nurbs.Knots().size(), knots.size());
    for (size_t i = 0; i < knots.size(); ++i) {
        EXPECT_NEAR(nurbs.Knots()[i], knots[i], kTol);
    }

    ASSERT_EQ(nurbs.Weights().size(), weights.size());
    for (size_t i = 0; i < weights.size(); ++i) {
        EXPECT_NEAR(nurbs.Weights()[i], weights[i], kTol);
    }

    // 制御点の各列を確認
    ASSERT_EQ(nurbs.ControlPoints().cols(), static_cast<int>(k + 1));
    for (int i = 0; i <= static_cast<int>(k); ++i) {
        EXPECT_NEAR(nurbs.ControlPoints()(0, i), cp(0, i), kTol);
        EXPECT_NEAR(nurbs.ControlPoints()(1, i), cp(1, i), kTol);
        EXPECT_NEAR(nurbs.ControlPoints()(2, i), cp(2, i), kTol);
    }

    const auto range = nurbs.GetParameterRange();
    EXPECT_NEAR(range[0], param_range[0], kTol);
    EXPECT_NEAR(range[1], param_range[1], kTol);
}

// 新コンストラクタによる点評価が IGESParameterVector 版と一致するか
// t = 0, 0.25, 0.5, 0.75, 1.0 の各点で比較する
TEST(RationalBSplineCurveNurbsCtorTest, PointEval_MatchesIGESVectorCtor) {
    const RationalBSplineCurve nurbs_new(
        2u, 2u,
        std::vector<double>{0.0, 0.0, 0.0, 1.0, 1.0, 1.0},
        std::vector<double>{1.0, 1.0, 1.0},
        MakeCPs({0.0, 0.0, 0.0,   1.0, 1.0, 0.0,   2.0, 0.0, 0.0}),
        std::array<double, 2>{0.0, 1.0});

    const auto ref = MakeNURBS_Deg2_Bezier();

    for (double t : {0.0, 0.25, 0.5, 0.75, 1.0}) {
        const auto d_new = nurbs_new.TryGetDerivatives(t, 0);
        const auto d_ref = ref->TryGetDerivatives(t, 0);
        ASSERT_TRUE(d_new.has_value()) << "t=" << t;
        ASSERT_TRUE(d_ref.has_value()) << "t=" << t;
        EXPECT_NEAR((*d_new)[0].x(), (*d_ref)[0].x(), kTol) << "t=" << t;
        EXPECT_NEAR((*d_new)[0].y(), (*d_ref)[0].y(), kTol) << "t=" << t;
        EXPECT_NEAR((*d_new)[0].z(), (*d_ref)[0].z(), kTol) << "t=" << t;
    }
}

// XY 平面上の非共線3点 → ComputePlaneNormal が法線を返し ValidatePD が通る
TEST(RationalBSplineCurveNurbsCtorTest, PlanarXYPlane_IsValid) {
    // P0,P1,P2 が z=0 かつ非共線
    const RationalBSplineCurve nurbs(
        2u, 2u,
        std::vector<double>{0.0, 0.0, 0.0, 1.0, 1.0, 1.0},
        std::vector<double>{1.0, 1.0, 1.0},
        MakeCPs({0.0, 0.0, 0.0,   1.0, 0.0, 0.0,   1.0, 1.0, 0.0}),
        std::array<double, 2>{0.0, 1.0});

    EXPECT_TRUE(nurbs.ValidatePD().is_valid);
}

// 非平面の制御点 → is_planar=false でも ValidatePD が通る
TEST(RationalBSplineCurveNurbsCtorTest, NonPlanar_IsValid) {
    // P3=(0,1,1) が XY 平面から外れており4点は非共面
    const RationalBSplineCurve nurbs(
        3u, 2u,
        std::vector<double>{0.0, 0.0, 0.0, 0.5, 1.0, 1.0, 1.0},
        std::vector<double>{1.0, 1.0, 1.0, 1.0},
        MakeCPs({0.0, 0.0, 0.0,   1.0, 0.0, 0.0,
                 1.0, 1.0, 0.0,   0.0, 1.0, 1.0}),
        std::array<double, 2>{0.0, 1.0});

    EXPECT_TRUE(nurbs.ValidatePD().is_valid);
}

// 全制御点が一直線上 → 法線が定まらず non-planar 扱いでも ValidatePD が通る
TEST(RationalBSplineCurveNurbsCtorTest, Collinear_IsValid) {
    // MakeNURBS_Deg1_Straight と同一の共線制御点
    const RationalBSplineCurve nurbs(
        2u, 1u,
        std::vector<double>{0.0, 0.0, 1.0, 2.0, 2.0},
        std::vector<double>{1.0, 1.0, 1.0},
        MakeCPs({0.0, 0.0, 0.0,   1.0, 0.0, 0.0,   2.0, 0.0, 0.0}),
        std::array<double, 2>{0.0, 2.0});

    EXPECT_TRUE(nurbs.ValidatePD().is_valid);
}

// k=1（制御点2点）→ ComputePlaneNormal が即 nullopt を返し ValidatePD が通る
TEST(RationalBSplineCurveNurbsCtorTest, TwoControlPoints_IsValid) {
    const RationalBSplineCurve nurbs(
        1u, 1u,
        std::vector<double>{0.0, 0.0, 1.0, 1.0},
        std::vector<double>{1.0, 1.0},
        MakeCPs({0.0, 0.0, 0.0,   1.0, 0.0, 0.0}),
        std::array<double, 2>{0.0, 1.0});

    EXPECT_TRUE(nurbs.ValidatePD().is_valid);
}

// 全重みが等しい → IsPolynomial() == true
TEST(RationalBSplineCurveNurbsCtorTest, IsPolynomial_EqualWeights) {
    const RationalBSplineCurve nurbs(
        2u, 2u,
        std::vector<double>{0.0, 0.0, 0.0, 1.0, 1.0, 1.0},
        std::vector<double>{1.0, 1.0, 1.0},
        MakeCPs({0.0, 0.0, 0.0,   1.0, 1.0, 0.0,   2.0, 0.0, 0.0}),
        std::array<double, 2>{0.0, 1.0});

    EXPECT_TRUE(nurbs.IsPolynomial());
}

// 重みが不均等 → IsPolynomial() == false
TEST(RationalBSplineCurveNurbsCtorTest, IsPolynomial_UnequalWeights) {
    const RationalBSplineCurve nurbs(
        2u, 2u,
        std::vector<double>{0.0, 0.0, 0.0, 1.0, 1.0, 1.0},
        std::vector<double>{1.0, 0.7, 1.0},
        MakeCPs({0.0, 0.0, 0.0,   1.0, 1.0, 0.0,   2.0, 0.0, 0.0}),
        std::array<double, 2>{0.0, 1.0});

    EXPECT_FALSE(nurbs.IsPolynomial());
}

// degree=1, 内部ノット1個 → GetLinearSegments が2セグメントを返す
TEST(RationalBSplineCurveNurbsCtorTest, LinearSegments_Degree1) {
    // MakeNURBS_Deg1_Straight と同一パラメータ
    const RationalBSplineCurve nurbs(
        2u, 1u,
        std::vector<double>{0.0, 0.0, 1.0, 2.0, 2.0},
        std::vector<double>{1.0, 1.0, 1.0},
        MakeCPs({0.0, 0.0, 0.0,   1.0, 0.0, 0.0,   2.0, 0.0, 0.0}),
        std::array<double, 2>{0.0, 2.0});

    const auto segs = nurbs.GetLinearSegments();
    ASSERT_EQ(segs.size(), 2u);
    EXPECT_NEAR(segs[0][0], 0.0, kTol);
    EXPECT_NEAR(segs[0][1], 1.0, kTol);
    EXPECT_NEAR(segs[1][0], 1.0, kTol);
    EXPECT_NEAR(segs[1][1], 2.0, kTol);
}

// degree=2, 内部ノット重複度=2 → GetCornerParams が {1.0} を返す
TEST(RationalBSplineCurveNurbsCtorTest, CornerParams_Degree2FullMult) {
    // MakeNURBS_Deg2_FullMult と同一パラメータ
    const RationalBSplineCurve nurbs(
        4u, 2u,
        std::vector<double>{0.0, 0.0, 0.0, 1.0, 1.0, 2.0, 2.0, 2.0},
        std::vector<double>{1.0, 1.0, 1.0, 1.0, 1.0},
        MakeCPs({0.0, 0.0, 0.0,   0.5, 0.5, 0.0,   1.0, 0.0, 0.0,
                 1.5, 0.5, 0.0,   2.0, 0.0, 0.0}),
        std::array<double, 2>{0.0, 2.0});

    const auto corners = nurbs.GetCornerParams();
    ASSERT_EQ(corners.size(), 1u);
    EXPECT_NEAR(corners[0], 1.0, kTol);
}

// is_periodic のデフォルトは false
TEST(RationalBSplineCurveNurbsCtorTest, IsPeriodic_Default_False) {
    const RationalBSplineCurve nurbs(
        1u, 1u,
        std::vector<double>{0.0, 0.0, 1.0, 1.0},
        std::vector<double>{1.0, 1.0},
        MakeCPs({0.0, 0.0, 0.0,   1.0, 0.0, 0.0}),
        std::array<double, 2>{0.0, 1.0});

    EXPECT_FALSE(nurbs.IsPeriodic());
}

// is_periodic=true を指定した場合に正しく反映される
TEST(RationalBSplineCurveNurbsCtorTest, IsPeriodic_True) {
    const RationalBSplineCurve nurbs(
        1u, 1u,
        std::vector<double>{0.0, 0.0, 1.0, 1.0},
        std::vector<double>{1.0, 1.0},
        MakeCPs({0.0, 0.0, 0.0,   1.0, 0.0, 0.0}),
        std::array<double, 2>{0.0, 1.0},
        /*is_periodic=*/true);

    EXPECT_TRUE(nurbs.IsPeriodic());
}
