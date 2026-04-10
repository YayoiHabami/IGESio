/**
 * @file tests/entities/curves/test_linear_path.cpp
 * @brief LinearPath (Type 106, forms 11-13) エンティティのテスト
 * @author Yayoi Habami
 * @date 2026-04-10
 * @copyright 2026 Yayoi Habami
 *
 * テスト対象:
 *   [LinearPath オーバライド]
 *     GetLinearSegments(), IsInLinearSegment(t, eps)
 *     GetCornerParams(),   IsCorner(t, eps)
 *     LeftTangentAt(t),    RightTangentAt(t)
 *   [ICurve デフォルト実装; LinearPath の挙動を通じて確認]
 *     CornerExteriorAngle(t, n), TryGetSignedCurvature(t, n)
 *
 * NOTE:
 *   - 1頂点のLinearPathはIGESの仕様上そもそも作成できないためテストケースは用意しない
 *
 * TODO:
 *   - コンストラクタ・ValidatePD のテストは別ファイルへ切り出すこと
 *   - GetLinearSegments: 3D コンストラクタ (vector<Vector3d>) での確認
 *   - 始点と同一座標の内部頂点を持つ非ループでの FindVertexIndex 先頭ヒット挙動
 *     (vertex_lengths[0] == vertex_lengths[1] == 0 のとき IsCorner(0.0)=true に
 *      なり、FindVertexIndex が idx=0 を返して nullopt が出る可能性がある)
 */
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

#include "igesio/numerics/tolerance.h"
#include "igesio/entities/curves/linear_path.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using igesio::Vector2d;
using igesio::Vector3d;
using i_ent::LinearPath;
constexpr double kPi = igesio::kPi;
/// @brief 浮動小数点比較の許容誤差
constexpr double kTol = 1e-9;
/// @brief IsInLinearSegment / IsCorner のデフォルト eps
constexpr double kEps = 1e-9;

/**
 * テスト用インスタンスのファクトリ関数
 * 各関数は呼び出しのたびに新しいインスタンスを返す
 */

/// @brief 2頂点非ループ: A(0,0) → B(3,4), length=5
std::shared_ptr<LinearPath> MakeTwoVertexOpen() {
    return std::make_shared<LinearPath>(
        std::vector<Vector2d>{{0.0, 0.0}, {3.0, 4.0}}, false);
}

/// @brief 3頂点非ループ（左直角折れ）: A(0,0) → B(1,0) → C(1,1), length=2
std::shared_ptr<LinearPath> MakeLeftTurnOpen() {
    return std::make_shared<LinearPath>(
        std::vector<Vector2d>{{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}}, false);
}

/// @brief 3頂点非ループ（右直角折れ）: A(0,0) → B(1,0) → C(1,-1), length=2
std::shared_ptr<LinearPath> MakeRightTurnOpen() {
    return std::make_shared<LinearPath>(
        std::vector<Vector2d>{{0.0, 0.0}, {1.0, 0.0}, {1.0, -1.0}}, false);
}

/// @brief 3頂点非ループ（直線上の角点）: A(0,0) → B(1,0) → C(2,0), length=2
std::shared_ptr<LinearPath> MakeStraightCorner() {
    return std::make_shared<LinearPath>(
        std::vector<Vector2d>{{0.0, 0.0}, {1.0, 0.0}, {2.0, 0.0}}, false);
}

/// @brief 3頂点非ループ（Uターン）: A(0,0) → B(1,0) → C(0,0), length=2
std::shared_ptr<LinearPath> MakeUTurnOpen() {
    return std::make_shared<LinearPath>(
        std::vector<Vector2d>{{0.0, 0.0}, {1.0, 0.0}, {0.0, 0.0}}, false);
}

/// @brief 4頂点正方形ループ (CCW): A(0,0)→B(1,0)→C(1,1)→D(0,1), length=4
std::shared_ptr<LinearPath> MakeSquareCCWLoop() {
    return std::make_shared<LinearPath>(
        std::vector<Vector2d>{{0.0, 0.0}, {1.0, 0.0},
                              {1.0, 1.0}, {0.0, 1.0}}, true);
}

/// @brief 3頂点（縮退：末尾辺の長さがゼロ）: A(0,0)→B(1,0)→C(1,0), length=1
/// @note  RightTangentAt が縮退辺（長さゼロ）に対して nullopt を返すか確認するため
std::shared_ptr<LinearPath> MakeDegenerateEndEdge() {
    return std::make_shared<LinearPath>(
        std::vector<Vector2d>{{0.0, 0.0}, {1.0, 0.0}, {1.0, 0.0}}, false);
}

}  // namespace



/**
 * GetLinearSegments / IsInLinearSegment のテスト
 */

// 2頂点非ループは単一の直線セグメントを返す
TEST(LinearPathLinearSegmentTest, TwoVertex_SingleSegment) {
    auto lp = MakeTwoVertexOpen();
    const auto segs = lp->GetLinearSegments();

    ASSERT_EQ(segs.size(), 1u);
    EXPECT_NEAR(segs[0][0], 0.0, kTol);
    EXPECT_NEAR(segs[0][1], 5.0, kTol);
}

// 3頂点非ループは2つのセグメントを返す
TEST(LinearPathLinearSegmentTest, ThreeVertex_TwoSegments) {
    auto lp = MakeLeftTurnOpen();
    const auto segs = lp->GetLinearSegments();

    // セグメント: {0,1}, {1,2}
    ASSERT_EQ(segs.size(), 2u);
    EXPECT_NEAR(segs[0][0], 0.0, kTol);
    EXPECT_NEAR(segs[0][1], 1.0, kTol);
    EXPECT_NEAR(segs[1][0], 1.0, kTol);
    EXPECT_NEAR(segs[1][1], 2.0, kTol);
}

// 4頂点ループは閉鎖辺を含む4セグメントを返す
TEST(LinearPathLinearSegmentTest, FourVertexLoop_WithClosingSegment) {
    auto lp = MakeSquareCCWLoop();
    const auto segs = lp->GetLinearSegments();

    // セグメント: {0,1}, {1,2}, {2,3}, {3,4}
    ASSERT_EQ(segs.size(), 4u);
    for (size_t i = 0; i < 4; ++i) {
        SCOPED_TRACE("segment " + std::to_string(i));
        EXPECT_NEAR(segs[i][0], static_cast<double>(i),   kTol);
        EXPECT_NEAR(segs[i][1], static_cast<double>(i+1), kTol);
    }
    // 末尾セグメントがパラメータ範囲の終端と一致することを確認
    const auto [t0, t1] = lp->GetParameterRange();
    EXPECT_NEAR(segs.back()[1], t1, kTol);
}

// セグメント内点で IsInLinearSegment が true を返す
TEST(LinearPathLinearSegmentTest, IsInLinearSegment_Midpoint) {
    auto lp = MakeLeftTurnOpen();  // セグメント {0,1}, {1,2}

    EXPECT_TRUE(lp->IsInLinearSegment(0.5));   // 1つ目の辺の中点
    EXPECT_TRUE(lp->IsInLinearSegment(1.5));   // 2つ目の辺の中点
}

// セグメント端点で IsInLinearSegment が true を返す
TEST(LinearPathLinearSegmentTest, IsInLinearSegment_Endpoints) {
    auto lp = MakeLeftTurnOpen();

    EXPECT_TRUE(lp->IsInLinearSegment(0.0));   // 全体の始点
    EXPECT_TRUE(lp->IsInLinearSegment(1.0));   // 内部頂点（セグメント境界）
    EXPECT_TRUE(lp->IsInLinearSegment(2.0));   // 全体の終点
}

// パラメータ範囲外で IsInLinearSegment が false を返す
TEST(LinearPathLinearSegmentTest, IsInLinearSegment_OutOfRange) {
    auto lp = MakeTwoVertexOpen();  // range=[0, 5]

    EXPECT_FALSE(lp->IsInLinearSegment(-0.1));
    EXPECT_FALSE(lp->IsInLinearSegment(5.1));
}

// eps 内の点は true (境界内側)
TEST(LinearPathLinearSegmentTest, IsInLinearSegment_EpsBoundaryInside) {
    auto lp = MakeTwoVertexOpen();  // セグメント {0, 5}
    const double custom_eps = 1e-6;

    // セグメント始端の eps/2 外側（絶対値 < eps → true）
    EXPECT_TRUE(lp->IsInLinearSegment(-custom_eps / 2.0, custom_eps));
    // セグメント終端の eps/2 外側（同様に true）
    EXPECT_TRUE(lp->IsInLinearSegment(5.0 + custom_eps / 2.0, custom_eps));
}

// eps の2倍外側の点は false (境界外側)
TEST(LinearPathLinearSegmentTest, IsInLinearSegment_EpsBoundaryOutside) {
    auto lp = MakeTwoVertexOpen();  // セグメント {0, 5}
    const double custom_eps = 1e-6;

    EXPECT_FALSE(lp->IsInLinearSegment(-custom_eps * 2.0, custom_eps));
    EXPECT_FALSE(lp->IsInLinearSegment(5.0 + custom_eps * 2.0, custom_eps));
}



/**
 * GetCornerParams / IsCorner のテスト
 */

// 2頂点非ループ：内部頂点なし → 空リスト
TEST(LinearPathCornerTest, TwoVertex_NoCorners) {
    auto lp = MakeTwoVertexOpen();

    EXPECT_TRUE(lp->GetCornerParams().empty());
    EXPECT_FALSE(lp->IsCorner(0.0));
    EXPECT_FALSE(lp->IsCorner(5.0));
}

// 3頂点非ループ：内部頂点1個（始点・終点は除外）
TEST(LinearPathCornerTest, ThreeVertex_OneCorner) {
    auto lp = MakeLeftTurnOpen();
    const auto corners = lp->GetCornerParams();

    ASSERT_EQ(corners.size(), 1u);
    EXPECT_NEAR(corners[0], 1.0, kTol);  // B の弧長パラメータ

    EXPECT_TRUE(lp->IsCorner(1.0));
    // 始点・終点は角点でない
    EXPECT_FALSE(lp->IsCorner(0.0));
    EXPECT_FALSE(lp->IsCorner(2.0));
}

// 4頂点非ループ：内部頂点2個
TEST(LinearPathCornerTest, FourVertex_TwoCorners) {
    // A(0,0)→B(1,0)→C(1,1)→D(0,1), 各辺の長さは1
    auto lp = std::make_shared<LinearPath>(
        std::vector<Vector2d>{{0.0, 0.0}, {1.0, 0.0},
                              {1.0, 1.0}, {0.0, 1.0}}, false);
    const auto corners = lp->GetCornerParams();

    ASSERT_EQ(corners.size(), 2u);
    EXPECT_NEAR(corners[0], 1.0, kTol);  // B
    EXPECT_NEAR(corners[1], 2.0, kTol);  // C

    EXPECT_FALSE(lp->IsCorner(0.0));   // 始点は角点でない
    EXPECT_FALSE(lp->IsCorner(3.0));   // 終点は角点でない
}

// 4頂点ループ：全頂点が角点（t=0 の頂点0も含む）
TEST(LinearPathCornerTest, Loop_AllVerticesAreCorners) {
    auto lp = MakeSquareCCWLoop();
    const auto corners = lp->GetCornerParams();

    ASSERT_EQ(corners.size(), 4u);
    EXPECT_NEAR(corners[0], 0.0, kTol);  // 頂点0 (t=0)
    EXPECT_NEAR(corners[1], 1.0, kTol);  // 頂点1
    EXPECT_NEAR(corners[2], 2.0, kTol);  // 頂点2
    EXPECT_NEAR(corners[3], 3.0, kTol);  // 頂点3
}

// ループの t=0 が IsCorner で true になる
TEST(LinearPathCornerTest, Loop_t0_IsCorner) {
    auto lp = MakeSquareCCWLoop();

    EXPECT_TRUE(lp->IsCorner(0.0));
}

// IsCorner の eps 境界テスト
TEST(LinearPathCornerTest, IsCorner_EpsBoundary) {
    auto lp = MakeLeftTurnOpen();  // 角点は t=1.0
    const double custom_eps = 1e-6;

    // eps/2 以内は true
    EXPECT_TRUE(lp->IsCorner(1.0 + custom_eps / 2.0, custom_eps));
    EXPECT_TRUE(lp->IsCorner(1.0 - custom_eps / 2.0, custom_eps));
    // eps*2 以遠は false
    EXPECT_FALSE(lp->IsCorner(1.0 + custom_eps * 2.0, custom_eps));
    EXPECT_FALSE(lp->IsCorner(1.0 - custom_eps * 2.0, custom_eps));
}



/**
 * LeftTangentAt / RightTangentAt のテスト
 */

// 直角左折れの角点：左接線が入射辺方向
TEST(LinearPathTangentAtTest, LeftTurn_Corner_LeftTangent) {
    auto lp = MakeLeftTurnOpen();  // A(0,0)→B(1,0)→C(1,1)
    // B での入射辺 AB の方向は (1,0,0)
    auto result = lp->LeftTangentAt(1.0);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(*result, Vector3d(1.0, 0.0, 0.0), kTol));
}

// 直角左折れの角点：右接線が出射辺方向
TEST(LinearPathTangentAtTest, LeftTurn_Corner_RightTangent) {
    auto lp = MakeLeftTurnOpen();  // A(0,0)→B(1,0)→C(1,1)
    // B での出射辺 BC の方向は (0,1,0)
    auto result = lp->RightTangentAt(1.0);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(*result, Vector3d(0.0, 1.0, 0.0), kTol));
}

// 非ループの始点は角点でないためデフォルト実装に委譲される
// (数値近似; クランプにより始点での接線方向と一致することを確認)
TEST(LinearPathTangentAtTest, NonLoopStart_LeftTangent_DefaultImpl) {
    auto lp = MakeTwoVertexOpen();  // A(0,0)→B(3,4), length=5
    // t=0 は角点でないためデフォルト実装 → 始点の接線 = AB 方向 = (3,4,0)/5
    auto result = lp->LeftTangentAt(0.0);

    ASSERT_TRUE(result.has_value());
    const Vector3d expected(3.0 / 5.0, 4.0 / 5.0, 0.0);
    EXPECT_TRUE(i_num::IsApproxEqual(*result, expected, kTol));
}

// 非ループの終点は角点でないためデフォルト実装に委譲される
TEST(LinearPathTangentAtTest, NonLoopEnd_RightTangent_DefaultImpl) {
    auto lp = MakeTwoVertexOpen();  // A(0,0)→B(3,4), length=5
    // t=5 は角点でないためデフォルト実装 → 終点の接線 = AB 方向 = (3,4,0)/5
    auto result = lp->RightTangentAt(5.0);

    ASSERT_TRUE(result.has_value());
    const Vector3d expected(3.0 / 5.0, 4.0 / 5.0, 0.0);
    EXPECT_TRUE(i_num::IsApproxEqual(*result, expected, kTol));
}

// ループの t=0 (頂点0) における左接線は閉鎖辺 D→A の方向
TEST(LinearPathTangentAtTest, Loop_t0_LeftTangent) {
    auto lp = MakeSquareCCWLoop();  // A(0,0)→B(1,0)→C(1,1)→D(0,1)
    // 閉鎖辺 D(0,1)→A(0,0): 方向 = (0,-1,0)
    auto result = lp->LeftTangentAt(0.0);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(*result, Vector3d(0.0, -1.0, 0.0), kTol));
}

// ループの t=0 (頂点0) における右接線は最初の辺 A→B の方向
TEST(LinearPathTangentAtTest, Loop_t0_RightTangent) {
    auto lp = MakeSquareCCWLoop();  // A(0,0)→B(1,0)→C(1,1)→D(0,1)
    // 辺 A(0,0)→B(1,0): 方向 = (1,0,0)
    auto result = lp->RightTangentAt(0.0);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(*result, Vector3d(1.0, 0.0, 0.0), kTol));
}

// 縮退辺（長さゼロ）の右接線は nullopt
TEST(LinearPathTangentAtTest, DegenerateOutgoingEdge_RightTangent_Nullopt) {
    // A(0,0)→B(1,0)→C(1,0): B→C が縮退辺
    auto lp = MakeDegenerateEndEdge();
    // B の右接線（出射辺 B→C; 長さ 0）は nullopt
    auto result = lp->RightTangentAt(1.0);

    EXPECT_FALSE(result.has_value());
}



/**
 * CornerExteriorAngle のテスト
 * ICurve のデフォルト実装を LinearPath の LeftTangentAt / RightTangentAt を
 * 通じて確認する
 */

// 左直角折れの角点: 外角 = +π/2 (反時計回り)
TEST(LinearPathCornerAngleTest, LeftTurn_PositivePiHalf) {
    auto lp = MakeLeftTurnOpen();  // A(0,0)→B(1,0)→C(1,1)
    const Vector3d normal(0.0, 0.0, 1.0);
    auto angle = lp->CornerExteriorAngle(1.0, normal);

    ASSERT_TRUE(angle.has_value());
    EXPECT_NEAR(*angle, kPi / 2.0, kTol);
}

// 右直角折れの角点: 外角 = -π/2 (時計回り)
TEST(LinearPathCornerAngleTest, RightTurn_NegativePiHalf) {
    auto lp = MakeRightTurnOpen();  // A(0,0)→B(1,0)→C(1,-1)
    const Vector3d normal(0.0, 0.0, 1.0);
    auto angle = lp->CornerExteriorAngle(1.0, normal);

    ASSERT_TRUE(angle.has_value());
    EXPECT_NEAR(*angle, -kPi / 2.0, kTol);
}

// 直線上の角点: 外角 = 0
TEST(LinearPathCornerAngleTest, StraightThrough_Zero) {
    auto lp = MakeStraightCorner();  // A(0,0)→B(1,0)→C(2,0)
    const Vector3d normal(0.0, 0.0, 1.0);
    auto angle = lp->CornerExteriorAngle(1.0, normal);

    ASSERT_TRUE(angle.has_value());
    EXPECT_NEAR(*angle, 0.0, kTol);
}

// Uターン: 外角 = +π (atan2(0, -1) = π で正値)
TEST(LinearPathCornerAngleTest, UTurn_Pi) {
    auto lp = MakeUTurnOpen();  // A(0,0)→B(1,0)→C(0,0)
    const Vector3d normal(0.0, 0.0, 1.0);
    auto angle = lp->CornerExteriorAngle(1.0, normal);

    ASSERT_TRUE(angle.has_value());
    EXPECT_NEAR(*angle, kPi, kTol);
}

// 法線を反転すると外角の符号が逆転する
TEST(LinearPathCornerAngleTest, NormalFlipped_SignFlipped) {
    auto lp = MakeLeftTurnOpen();
    const Vector3d normal_pos(0.0, 0.0,  1.0);
    const Vector3d normal_neg(0.0, 0.0, -1.0);

    auto angle_pos = lp->CornerExteriorAngle(1.0, normal_pos);
    auto angle_neg = lp->CornerExteriorAngle(1.0, normal_neg);

    ASSERT_TRUE(angle_pos.has_value());
    ASSERT_TRUE(angle_neg.has_value());
    EXPECT_NEAR(*angle_neg, -(*angle_pos), kTol);
}

// 非単位法線ベクトルでも単位法線と同じ結果
TEST(LinearPathCornerAngleTest, NonUnitNormal_SameResult) {
    auto lp = MakeLeftTurnOpen();
    const Vector3d unit_normal(0.0, 0.0, 1.0);
    const Vector3d scaled_normal(0.0, 0.0, 3.0);  // スケール違い

    auto angle_unit   = lp->CornerExteriorAngle(1.0, unit_normal);
    auto angle_scaled = lp->CornerExteriorAngle(1.0, scaled_normal);

    ASSERT_TRUE(angle_unit.has_value());
    ASSERT_TRUE(angle_scaled.has_value());
    EXPECT_NEAR(*angle_unit, *angle_scaled, kTol);
}

// ゼロ法線ベクトル: nullopt を返す
TEST(LinearPathCornerAngleTest, ZeroNormal_Nullopt) {
    auto lp = MakeLeftTurnOpen();
    const Vector3d zero_normal(0.0, 0.0, 0.0);

    EXPECT_FALSE(lp->CornerExteriorAngle(1.0, zero_normal).has_value());
}



/**
 * TryGetSignedCurvature のテスト
 * 分岐: (1) ゼロ法線 → nullopt
 *       (2) 角点かつ外角>0 → +∞
 *       (3) 角点かつ外角<0 → -∞
 *       (4) 角点かつ外角=0 → 0.0
 *       (5) 直線部内 (非角点) → 0.0
 */

// ゼロ法線 → nullopt
TEST(LinearPathSignedCurvatureTest, ZeroNormal_Nullopt) {
    auto lp = MakeLeftTurnOpen();
    const Vector3d zero(0.0, 0.0, 0.0);

    EXPECT_FALSE(lp->TryGetSignedCurvature(0.5, zero).has_value());
    EXPECT_FALSE(lp->TryGetSignedCurvature(1.0, zero).has_value());
}

// 直線部中点 → 0.0
TEST(LinearPathSignedCurvatureTest, LinearSegment_Zero) {
    auto lp = MakeLeftTurnOpen();  // セグメント {0,1}, {1,2}
    const Vector3d normal(0.0, 0.0, 1.0);

    const auto kappa_mid1 = lp->TryGetSignedCurvature(0.5, normal);
    const auto kappa_mid2 = lp->TryGetSignedCurvature(1.5, normal);

    ASSERT_TRUE(kappa_mid1.has_value());
    ASSERT_TRUE(kappa_mid2.has_value());
    EXPECT_NEAR(*kappa_mid1, 0.0, kTol);
    EXPECT_NEAR(*kappa_mid2, 0.0, kTol);
}

// 左折り角点 → +∞
TEST(LinearPathSignedCurvatureTest, LeftTurnCorner_PlusInf) {
    auto lp = MakeLeftTurnOpen();
    const Vector3d normal(0.0, 0.0, 1.0);

    const auto kappa = lp->TryGetSignedCurvature(1.0, normal);

    ASSERT_TRUE(kappa.has_value());
    EXPECT_TRUE(std::isinf(*kappa) && *kappa > 0.0);
}

// 右折り角点 → -∞
TEST(LinearPathSignedCurvatureTest, RightTurnCorner_MinusInf) {
    auto lp = MakeRightTurnOpen();
    const Vector3d normal(0.0, 0.0, 1.0);

    const auto kappa = lp->TryGetSignedCurvature(1.0, normal);

    ASSERT_TRUE(kappa.has_value());
    EXPECT_TRUE(std::isinf(*kappa) && *kappa < 0.0);
}

// 直線上の角点（外角=0） → 0.0
TEST(LinearPathSignedCurvatureTest, StraightCorner_Zero) {
    auto lp = MakeStraightCorner();
    const Vector3d normal(0.0, 0.0, 1.0);

    const auto kappa = lp->TryGetSignedCurvature(1.0, normal);

    ASSERT_TRUE(kappa.has_value());
    EXPECT_NEAR(*kappa, 0.0, kTol);
}

// Uターン角点（外角=+π） → +∞
TEST(LinearPathSignedCurvatureTest, UTurnCorner_PlusInf) {
    auto lp = MakeUTurnOpen();
    const Vector3d normal(0.0, 0.0, 1.0);

    const auto kappa = lp->TryGetSignedCurvature(1.0, normal);

    ASSERT_TRUE(kappa.has_value());
    EXPECT_TRUE(std::isinf(*kappa) && *kappa > 0.0);
}

// ループの t=0 角点 (CCW正方形; 外角=+π/2) → +∞
TEST(LinearPathSignedCurvatureTest, Loop_t0Corner_PlusInf) {
    auto lp = MakeSquareCCWLoop();
    const Vector3d normal(0.0, 0.0, 1.0);

    const auto kappa = lp->TryGetSignedCurvature(0.0, normal);

    ASSERT_TRUE(kappa.has_value());
    EXPECT_TRUE(std::isinf(*kappa) && *kappa > 0.0);
}

// 非ループの始点（非角点; 直線部内） → 0.0
TEST(LinearPathSignedCurvatureTest, NonLoopStart_LinearPart_Zero) {
    auto lp = MakeLeftTurnOpen();
    const Vector3d normal(0.0, 0.0, 1.0);

    // t=0 は角点でなく直線部内 → IsInLinearSegment(0) = true → κ_s = 0.0
    const auto kappa = lp->TryGetSignedCurvature(0.0, normal);

    ASSERT_TRUE(kappa.has_value());
    EXPECT_NEAR(*kappa, 0.0, kTol);
}

// 非ループの終点（非角点; 直線部内） → 0.0
TEST(LinearPathSignedCurvatureTest, NonLoopEnd_LinearPart_Zero) {
    auto lp = MakeLeftTurnOpen();
    const Vector3d normal(0.0, 0.0, 1.0);

    // t=2 は角点でなく直線部内 → IsInLinearSegment(2) = true → κ_s = 0.0
    const auto kappa = lp->TryGetSignedCurvature(2.0, normal);

    ASSERT_TRUE(kappa.has_value());
    EXPECT_NEAR(*kappa, 0.0, kTol);
}

// 法線を反転すると符号が反転する (左折り角点の場合)
TEST(LinearPathSignedCurvatureTest, NormalFlipped_SignFlipped) {
    auto lp = MakeLeftTurnOpen();

    const auto kappa_pos =
        lp->TryGetSignedCurvature(1.0, Vector3d(0.0, 0.0,  1.0));
    const auto kappa_neg =
        lp->TryGetSignedCurvature(1.0, Vector3d(0.0, 0.0, -1.0));

    // 両者は無限大で符号が逆になるはず
    ASSERT_TRUE(kappa_pos.has_value());
    ASSERT_TRUE(kappa_neg.has_value());
    EXPECT_TRUE(std::isinf(*kappa_pos) && *kappa_pos > 0.0);
    EXPECT_TRUE(std::isinf(*kappa_neg) && *kappa_neg < 0.0);
}
