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
        2, 1,                          // K=2, M=1
        false, false, true, false,     // PROP1-4
        0.0, 0.0, 1.0, 2.0, 2.0,     // ノット (K+M+2=5 個)
        1.0, 1.0, 1.0,                // 重み (K+1=3 個)
        0.0, 0.0, 0.0,                // P0 = (0,0,0)
        1.0, 0.0, 0.0,                // P1 = (1,0,0)
        2.0, 0.0, 0.0,                // P2 = (2,0,0)
        0.0, 2.0,                     // V(0)=0, V(1)=2
        0.0, 0.0, 1.0                 // 法線
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
        2, 1,                          // K=2, M=1
        false, false, true, false,     // PROP1-4
        0.0, 0.0, 1.0, 2.0, 2.0,     // ノット
        1.0, 1.0, 1.0,                // 重み
        0.0, 0.0, 0.0,                // P0 = (0,0,0)
        1.0, 0.0, 0.0,                // P1 = (1,0,0)
        1.0, 1.0, 0.0,                // P2 = (1,1,0)
        0.0, 2.0,                     // V=[0,2]
        0.0, 0.0, 1.0                 // 法線
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
        3, 1,                              // K=3, M=1
        false, false, true, false,         // PROP1-4
        0.0, 0.0, 0.5, 0.5, 1.0, 1.0,   // ノット (K+M+2=6 個)
        1.0, 1.0, 1.0, 1.0,               // 重み (K+1=4 個)
        0.0, 0.0, 0.0,                    // P0
        0.5, 0.0, 0.0,                    // P1
        0.5, 0.5, 0.0,                    // P2
        1.0, 0.5, 0.0,                    // P3
        0.0, 1.0,                         // V=[0,1]
        0.0, 0.0, 1.0                     // 法線
    };
    return std::make_shared<RationalBSplineCurve>(param);
}

/// @brief degree=2 NURBS（単純 Bezier; 内部ノットなし）
/// K=2, M=2, ノット=[0,0,0,1,1,1], V=[0,1]
/// GetLinearSegments: [] (degree != 1)
/// GetCornerParams:   [] (内部ノットなし)
std::shared_ptr<RationalBSplineCurve> MakeNURBS_Deg2_Bezier() {
    const auto param = igesio::IGESParameterVector{
        2, 2,                              // K=2, M=2
        false, false, true, false,         // PROP1-4
        0.0, 0.0, 0.0, 1.0, 1.0, 1.0,   // ノット (K+M+2=6 個)
        1.0, 1.0, 1.0,                    // 重み (K+1=3 個)
        0.0, 0.0, 0.0,                    // P0
        1.0, 1.0, 0.0,                    // P1
        2.0, 0.0, 0.0,                    // P2
        0.0, 1.0,                         // V=[0,1]
        0.0, 0.0, 1.0                     // 法線
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
        0.0, 0.0, 0.0, 1.0, 1.0, 2.0, 2.0, 2.0, // ノット (K+M+2=8 個)
        1.0, 1.0, 1.0, 1.0, 1.0,               // 重み (K+1=5 個)
        0.0, 0.0, 0.0,                          // P0
        0.5, 0.5, 0.0,                          // P1
        1.0, 0.0, 0.0,                          // P2 (内部ノット t=1 に対応)
        1.5, 0.5, 0.0,                          // P3
        2.0, 0.0, 0.0,                          // P4
        0.0, 2.0,                               // V=[0,2]
        0.0, 0.0, 1.0                           // 法線
    };
    return std::make_shared<RationalBSplineCurve>(param);
}

/// @brief degree=2 NURBS（内部ノット重複度=1; C^1連続, 角点なし）
/// K=3, M=2, ノット=[0,0,0,1,2,2,2], V=[0,2]
/// GetLinearSegments: []
/// GetCornerParams:   [] (mult=1 < degree=2 → C^1, 角点でない)
std::shared_ptr<RationalBSplineCurve> MakeNURBS_Deg2_PartialMult() {
    const auto param = igesio::IGESParameterVector{
        3, 2,                                    // K=3, M=2
        false, false, true, false,               // PROP1-4
        0.0, 0.0, 0.0, 1.0, 2.0, 2.0, 2.0,    // ノット (K+M+2=7 個)
        1.0, 1.0, 1.0, 1.0,                    // 重み (K+1=4 個)
        0.0, 0.0, 0.0,                          // P0
        0.5, 0.5, 0.0,                          // P1
        1.5, 0.5, 0.0,                          // P2
        2.0, 0.0, 0.0,                          // P3
        0.0, 2.0,                               // V=[0,2]
        0.0, 0.0, 1.0                           // 法線
    };
    return std::make_shared<RationalBSplineCurve>(param);
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
