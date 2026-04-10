/**
 * @file tests/entities/curves/test_composite_curve.cpp
 * @brief CompositeCurve (Type 102) エンティティのテスト
 * @author Yayoi Habami
 * @date 2026-04-10
 * @copyright 2026 Yayoi Habami
 *
 * テスト対象:
 *   [CompositeCurve オーバライド]
 *     GetLinearSegments(), IsInLinearSegment(t, eps)
 *     GetCornerParams(),   IsCorner(t, eps)
 *     LeftTangentAt(t),    RightTangentAt(t)
 *   [ICurve デフォルト実装; CompositeCurve の挙動を通じて確認]
 *     CornerExteriorAngle(t, n), TryGetSignedCurvature(t, n)
 *
 * NOTE:
 *   - Line エンティティは GetLinearSegments() をオーバライドしないため、
 *     IsInLinearSegment のテストには LinearPath サブカーブを使用する。
 *   - 接合点は GetCornerParams() に常に角点として追加される（接線連続性は問わない）。
 *   - LeftTangentAt/RightTangentAt の接合点検出は内部で 1e-9 の固定誤差を使用する。
 *
 * TODO:
 *   - 接合点の固定誤差（hardcoded 1e-9）の境界挙動テストを追加する
 *   - 3つ以上の構成曲線を用いたパラメータ累積テストを追加する
 */
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

#include "igesio/common/iges_parameter_vector.h"
#include "igesio/numerics/tolerance.h"
#include "igesio/entities/curves/circular_arc.h"
#include "igesio/entities/curves/composite_curve.h"
#include "igesio/entities/curves/line.h"
#include "igesio/entities/curves/linear_path.h"
#include "igesio/entities/curves/rational_b_spline_curve.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using igesio::Vector2d;
using igesio::Vector3d;
using i_ent::CircularArc;
using i_ent::CompositeCurve;
using i_ent::Line;
using i_ent::LinearPath;
using i_ent::RationalBSplineCurve;
constexpr double kPi = igesio::kPi;
/// @brief 浮動小数点比較の許容誤差
constexpr double kTol = 1e-9;
/// @brief IsInLinearSegment / IsCorner のデフォルト eps
constexpr double kEps = 1e-9;



/**
 * テスト用インスタンスのファクトリ関数
 */

/// @brief 単一の Line からなる CompositeCurve
/// Line: (0,0,0)→(1,0,0), range=[0,1]
std::shared_ptr<CompositeCurve> MakeSingleLine() {
    auto cc = std::make_shared<CompositeCurve>();
    cc->AddCurve(std::make_shared<Line>(
        Vector3d{0.0, 0.0, 0.0}, Vector3d{1.0, 0.0, 0.0}));
    return cc;
}

/// @brief 2つの直線（左直角折れ）からなる CompositeCurve
/// Line1: (0,0,0)→(1,0,0), range=[0,1]
/// Line2: (1,0,0)→(1,1,0), range=[0,1]
/// 全体範囲 [0,2], 接合点 t=1
std::shared_ptr<CompositeCurve> MakeLineLine_LeftTurn() {
    auto cc = std::make_shared<CompositeCurve>();
    cc->AddCurve(std::make_shared<Line>(
        Vector3d{0.0, 0.0, 0.0}, Vector3d{1.0, 0.0, 0.0}));
    cc->AddCurve(std::make_shared<Line>(
        Vector3d{1.0, 0.0, 0.0}, Vector3d{1.0, 1.0, 0.0}));
    return cc;
}

/// @brief 2つの直線（右直角折れ）からなる CompositeCurve
/// Line1: (0,0,0)→(1,0,0), range=[0,1]
/// Line2: (1,0,0)→(1,-1,0), range=[0,1]
/// 全体範囲 [0,2], 接合点 t=1
std::shared_ptr<CompositeCurve> MakeLineLine_RightTurn() {
    auto cc = std::make_shared<CompositeCurve>();
    cc->AddCurve(std::make_shared<Line>(
        Vector3d{0.0, 0.0, 0.0}, Vector3d{1.0, 0.0, 0.0}));
    cc->AddCurve(std::make_shared<Line>(
        Vector3d{1.0, 0.0, 0.0}, Vector3d{1.0, -1.0, 0.0}));
    return cc;
}

/// @brief 2つの直線（直線上・接線連続）からなる CompositeCurve
/// Line1: (0,0,0)→(1,0,0), range=[0,1]
/// Line2: (1,0,0)→(2,0,0), range=[0,1]
/// 全体範囲 [0,2], 接合点 t=1
/// NOTE: 接線連続だが接合点は角点として報告される（CompositeCurveの仕様）
std::shared_ptr<CompositeCurve> MakeLineLine_Straight() {
    auto cc = std::make_shared<CompositeCurve>();
    cc->AddCurve(std::make_shared<Line>(
        Vector3d{0.0, 0.0, 0.0}, Vector3d{1.0, 0.0, 0.0}));
    cc->AddCurve(std::make_shared<Line>(
        Vector3d{1.0, 0.0, 0.0}, Vector3d{2.0, 0.0, 0.0}));
    return cc;
}

/// @brief 直線 + LinearPath（左直角折れ内部角点あり）からなる CompositeCurve
/// Line: (0,0,0)→(1,0,0), range=[0,1]
/// LinearPath: (1,0,0)→(2,0,0)→(2,1,0) 非ループ, range=[0,2]
/// 全体範囲 [0,3], 接合点 t=1, サブカーブ角点 t=2
std::shared_ptr<CompositeCurve> MakeLineLinearPath_LeftTurn() {
    auto cc = std::make_shared<CompositeCurve>();
    cc->AddCurve(std::make_shared<Line>(
        Vector3d{0.0, 0.0, 0.0}, Vector3d{1.0, 0.0, 0.0}));
    cc->AddCurve(std::make_shared<LinearPath>(
        std::vector<Vector2d>{{1.0, 0.0}, {2.0, 0.0}, {2.0, 1.0}}, false));
    return cc;
}

/// @brief 2頂点 LinearPath ×2（左直角折れ）からなる CompositeCurve
/// Path1: (0,0,0)→(1,0,0) 非ループ, range=[0,1]
/// Path2: (1,0,0)→(1,1,0) 非ループ, range=[0,1]
/// 全体範囲 [0,2], 接合点 t=1
/// NOTE: Line の代わりに LinearPath を使用（GetLinearSegments テスト用）
std::shared_ptr<CompositeCurve> MakePathPath_LeftTurn() {
    auto cc = std::make_shared<CompositeCurve>();
    cc->AddCurve(std::make_shared<LinearPath>(
        std::vector<Vector2d>{{0.0, 0.0}, {1.0, 0.0}}, false));
    cc->AddCurve(std::make_shared<LinearPath>(
        std::vector<Vector2d>{{1.0, 0.0}, {1.0, 1.0}}, false));
    return cc;
}

/// @brief 四分円弧 + 2頂点 LinearPath からなる CompositeCurve
/// Arc: center=(0,0), r=1, 始点(1,0)→終点(0,1) CCW, range=[0,π/2]
/// Path: (0,1,0)→(0,2,0) 非ループ, range=[0,1]
/// 全体範囲 [0, π/2+1], 接合点 t=π/2
std::shared_ptr<CompositeCurve> MakeArcPath_Quarter() {
    auto cc = std::make_shared<CompositeCurve>();
    cc->AddCurve(std::make_shared<CircularArc>(
        Vector2d{0.0, 0.0},    // 中心
        Vector2d{1.0, 0.0},    // 始点 (角度 0)
        Vector2d{0.0, 1.0}));  // 終点 (角度 π/2, CCW)
    cc->AddCurve(std::make_shared<LinearPath>(
        std::vector<Vector2d>{{0.0, 1.0}, {0.0, 2.0}}, false));
    return cc;
}

/// @brief 2頂点 LinearPath + degree-1 NURBS（パラメータ範囲 [2,5]）の CompositeCurve
/// Path: (0,0,0)→(1,0,0) 非ループ, range=[0,1]
/// NURBS: degree=1, K=2 (3制御点), range=[2,5], 内部ノット=3.5
///        P0=(1,0,0), P1=(1,1,0), P2=(1,2,0)
/// 全体範囲 [0,4], 接合点 t=1.0, NURBSの角点 t=2.5 (=1.0+(3.5-2.0))
/// NOTE: local_start=2.0 の非ゼロ開始パラメータを持つサブカーブの
///       オフセット計算を検証するためのファクトリ
std::shared_ptr<CompositeCurve> MakePathNURBS_NonZeroRange() {
    auto cc = std::make_shared<CompositeCurve>();
    cc->AddCurve(std::make_shared<LinearPath>(
        std::vector<Vector2d>{{0.0, 0.0}, {1.0, 0.0}}, false));
    // degree-1 NURBS: K=2, M=1, ノット=[2,2,3.5,5,5], range=[2,5]
    // ノットベクトルサイズ = K+M+2 = 5, 重みサイズ = K+1 = 3
    const auto param = igesio::IGESParameterVector{
        2,                         // K (制御点数 - 1)
        1,                         // M (次数)
        false, false, true, false,  // PROP1-4 (非周期, 非閉, 多項式, 非平面外)
        2.0, 2.0, 3.5, 5.0, 5.0,    // ノットベクトル (K+M+2=5 個)
        1.0, 1.0, 1.0,             // 重み (K+1=3 個)
        1.0, 0.0, 0.0,             // P0 = (1,0,0)
        1.0, 1.0, 0.0,             // P1 = (1,1,0) (内部ノット t=3.5 に対応)
        1.0, 2.0, 0.0,             // P2 = (1,2,0)
        2.0, 5.0,                  // V(0)=2, V(1)=5
        0.0, 0.0, 1.0              // 法線ベクトル
    };
    cc->AddCurve(std::make_shared<RationalBSplineCurve>(param));
    return cc;
}

}  // namespace



/**
 * GetLinearSegments / IsInLinearSegment のテスト
 * NOTE: Line は GetLinearSegments() を未オーバライドのため LinearPath 系を使用する
 */

// 2つの LinearPath（左折れ）: 2つのセグメントが返る
TEST(CompositeCurveLinearSegmentTest, PathPath_TwoSegments) {
    auto cc = MakePathPath_LeftTurn();
    const auto segs = cc->GetLinearSegments();

    ASSERT_EQ(segs.size(), 2u);
    EXPECT_NEAR(segs[0][0], 0.0, kTol);
    EXPECT_NEAR(segs[0][1], 1.0, kTol);
    EXPECT_NEAR(segs[1][0], 1.0, kTol);
    EXPECT_NEAR(segs[1][1], 2.0, kTol);
}

// 四分円弧 + LinearPath: 円弧部分は非線形、Path のみセグメントとして返る
TEST(CompositeCurveLinearSegmentTest, ArcPath_OnlyPathIsLinear) {
    auto cc = MakeArcPath_Quarter();
    const auto segs = cc->GetLinearSegments();

    // Arc: セグメントなし (offset=0, 円弧は非線形)
    // Path: {0,1} → グローバル {π/2, π/2+1}
    ASSERT_EQ(segs.size(), 1u);
    EXPECT_NEAR(segs[0][0], kPi / 2.0, kTol);
    EXPECT_NEAR(segs[0][1], kPi / 2.0 + 1.0, kTol);
}

// 非ゼロ開始パラメータの NURBS: local_start が正しく減算されるか
// NOTE: バグ（offset + seg[0]）では {3, 4.5} になるが、
//       正しくは {1+(2-2), 1+(3.5-2)} = {1, 2.5}
TEST(CompositeCurveLinearSegmentTest, NonZeroRangeStart_CorrectMapping) {
    auto cc = MakePathNURBS_NonZeroRange();
    const auto segs = cc->GetLinearSegments();

    // Path: {0,1} → グローバル {0,1}
    // NURBS (range=[2,5], 内部ノット 3.5):
    //   ローカル {2,3.5},{3.5,5} → グローバル {1+(2-2),1+(3.5-2)},{1+(3.5-2),1+(5-2)}
    //                                        = {1,2.5},{2.5,4}
    ASSERT_EQ(segs.size(), 3u);
    EXPECT_NEAR(segs[0][0], 0.0, kTol);
    EXPECT_NEAR(segs[0][1], 1.0, kTol);
    EXPECT_NEAR(segs[1][0], 1.0, kTol);
    EXPECT_NEAR(segs[1][1], 2.5, kTol);
    EXPECT_NEAR(segs[2][0], 2.5, kTol);
    EXPECT_NEAR(segs[2][1], 4.0, kTol);
}

// セグメント内点 → true
TEST(CompositeCurveLinearSegmentTest, IsInLinearSegment_Interior) {
    auto cc = MakePathPath_LeftTurn();  // セグメント {0,1}, {1,2}

    EXPECT_TRUE(cc->IsInLinearSegment(0.5));   // Path1の中点
    EXPECT_TRUE(cc->IsInLinearSegment(1.5));   // Path2の中点
}

// 接合点 t=1 はセグメント端点として true
TEST(CompositeCurveLinearSegmentTest, IsInLinearSegment_AtJunction) {
    auto cc = MakePathPath_LeftTurn();

    // t=1 は Path1 終端かつ Path2 始端（両セグメントの端点）
    EXPECT_TRUE(cc->IsInLinearSegment(1.0));
}

// 円弧パラメータ範囲内点 → false（円弧は非線形）
TEST(CompositeCurveLinearSegmentTest, IsInLinearSegment_ArcRange_False) {
    auto cc = MakeArcPath_Quarter();  // Arc: [0, π/2], Path: [π/2, π/2+1]

    // 円弧の中点 t = π/4
    EXPECT_FALSE(cc->IsInLinearSegment(kPi / 4.0));
}

// パラメータ範囲外 → false
TEST(CompositeCurveLinearSegmentTest, IsInLinearSegment_OutOfRange) {
    auto cc = MakePathPath_LeftTurn();  // range [0, 2]

    EXPECT_FALSE(cc->IsInLinearSegment(-0.5));
    EXPECT_FALSE(cc->IsInLinearSegment(2.5));
}



/**
 * GetCornerParams / IsCorner のテスト
 */

// 単一 Line: 接合点なし、Line に角点なし → 空リスト
TEST(CompositeCurveCornerTest, SingleLine_NoCorners) {
    auto cc = MakeSingleLine();

    EXPECT_TRUE(cc->GetCornerParams().empty());
    EXPECT_FALSE(cc->IsCorner(0.0));
    EXPECT_FALSE(cc->IsCorner(1.0));
}

// 2直線（左折れ）: 接合点のみが角点
TEST(CompositeCurveCornerTest, TwoLines_JunctionIsCorner) {
    auto cc = MakeLineLine_LeftTurn();
    const auto corners = cc->GetCornerParams();

    ASSERT_EQ(corners.size(), 1u);
    EXPECT_NEAR(corners[0], 1.0, kTol);
    EXPECT_TRUE(cc->IsCorner(1.0));
}

// 2直線（直線上・接線連続）: 接合点はそれでも角点として報告される
// NOTE: CompositeCurve は接線連続性を判断しない。全接合点を角点扱いする。
TEST(CompositeCurveCornerTest, TwoLines_Straight_JunctionStillCorner) {
    auto cc = MakeLineLine_Straight();
    const auto corners = cc->GetCornerParams();

    ASSERT_EQ(corners.size(), 1u);
    EXPECT_NEAR(corners[0], 1.0, kTol);
    EXPECT_TRUE(cc->IsCorner(1.0));
}

// 四分円弧 + LinearPath: 接合点のみが角点
TEST(CompositeCurveCornerTest, ArcPath_JunctionIsCorner) {
    auto cc = MakeArcPath_Quarter();
    const auto corners = cc->GetCornerParams();

    ASSERT_EQ(corners.size(), 1u);
    EXPECT_NEAR(corners[0], kPi / 2.0, kTol);
    EXPECT_TRUE(cc->IsCorner(kPi / 2.0));
}

// 直線 + LinearPath: 接合点とサブカーブ角点の両方が角点
TEST(CompositeCurveCornerTest, LineLinearPath_JunctionPlusSubcurveCorner) {
    auto cc = MakeLineLinearPath_LeftTurn();
    const auto corners = cc->GetCornerParams();

    // 接合点 t=1 (Line→Path の接合)
    // Path 内部角点 t=2 (t_local=1 → グローバル = 1+(1-0) = 2)
    ASSERT_EQ(corners.size(), 2u);
    EXPECT_NEAR(corners[0], 1.0, kTol);
    EXPECT_NEAR(corners[1], 2.0, kTol);

    EXPECT_TRUE(cc->IsCorner(1.0));
    EXPECT_TRUE(cc->IsCorner(2.0));
    EXPECT_FALSE(cc->IsCorner(0.0));   // 始点は角点でない
    EXPECT_FALSE(cc->IsCorner(3.0));   // 終点は角点でない
}

// 非ゼロ開始パラメータの NURBS: local_start が正しく減算されるか
// NOTE: バグ（offset + tc）では [1.0, 4.5] になるが、
//       正しくは [1.0, 1+(3.5-2.0)] = [1.0, 2.5]
TEST(CompositeCurveCornerTest, NonZeroRangeStart_CorrectMapping) {
    auto cc = MakePathNURBS_NonZeroRange();
    const auto corners = cc->GetCornerParams();

    ASSERT_EQ(corners.size(), 2u);
    EXPECT_NEAR(corners[0], 1.0, kTol);  // 接合点
    EXPECT_NEAR(corners[1], 2.5, kTol);  // NURBS 内部ノット (3.5), グローバル=1+(3.5-2)=2.5

    // グローバル曲率パラメータとして正しく機能するか
    EXPECT_TRUE(cc->IsCorner(2.5));
    EXPECT_FALSE(cc->IsCorner(4.5));  // バグ値は false
}

// IsCorner の eps 境界テスト
TEST(CompositeCurveCornerTest, IsCorner_EpsBoundary) {
    auto cc = MakeLineLine_LeftTurn();  // 角点 t=1.0
    const double custom_eps = 1e-6;

    EXPECT_TRUE(cc->IsCorner(1.0 + custom_eps / 2.0, custom_eps));
    EXPECT_TRUE(cc->IsCorner(1.0 - custom_eps / 2.0, custom_eps));
    EXPECT_FALSE(cc->IsCorner(1.0 + custom_eps * 2.0, custom_eps));
    EXPECT_FALSE(cc->IsCorner(1.0 - custom_eps * 2.0, custom_eps));
}

// 接合点以外の点は角点でない
TEST(CompositeCurveCornerTest, NonCornerPoints_NotCorner) {
    auto cc = MakeLineLine_LeftTurn();

    EXPECT_FALSE(cc->IsCorner(0.0));   // 始点
    EXPECT_FALSE(cc->IsCorner(0.5));   // Line1 内部
    EXPECT_FALSE(cc->IsCorner(1.5));   // Line2 内部
    EXPECT_FALSE(cc->IsCorner(2.0));   // 終点
}



/**
 * LeftTangentAt / RightTangentAt のテスト
 */

// 接合点での左接線: 直前の曲線の終端方向
TEST(CompositeCurveTangentAtTest, Junction_LeftTangent_FromPrevCurve) {
    auto cc = MakeLineLine_LeftTurn();
    // 接合点 t=1: Line1(→(1,0)) の終端接線 = (1,0,0)
    const auto result = cc->LeftTangentAt(1.0);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(*result, Vector3d(1.0, 0.0, 0.0), kTol));
}

// 接合点での右接線: 直後の曲線の始端方向
TEST(CompositeCurveTangentAtTest, Junction_RightTangent_FromNextCurve) {
    auto cc = MakeLineLine_LeftTurn();
    // 接合点 t=1: Line2(→(0,1)) の始端接線 = (0,1,0)
    const auto result = cc->RightTangentAt(1.0);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(*result, Vector3d(0.0, 1.0, 0.0), kTol));
}

// 直線上接合点: Left=Right (同方向)
TEST(CompositeCurveTangentAtTest, Junction_Straight_BothTangentsSameDirection) {
    auto cc = MakeLineLine_Straight();
    const auto left  = cc->LeftTangentAt(1.0);
    const auto right = cc->RightTangentAt(1.0);

    ASSERT_TRUE(left.has_value());
    ASSERT_TRUE(right.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(*left,  Vector3d(1.0, 0.0, 0.0), kTol));
    EXPECT_TRUE(i_num::IsApproxEqual(*right, Vector3d(1.0, 0.0, 0.0), kTol));
}

// サブカーブ角点（接合点でない）の左接線: LinearPath に委譲
TEST(CompositeCurveTangentAtTest, SubcurveCorner_LeftTangent_Delegated) {
    auto cc = MakeLineLinearPath_LeftTurn();
    // t=2: 接合点は t=1, Path の角点 t=2 はサブカーブに委譲される
    // Path.LeftTangentAt(t_local=1): 入射辺 (1,0,0)→(2,0,0) 方向 = (1,0,0)
    const auto result = cc->LeftTangentAt(2.0);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(*result, Vector3d(1.0, 0.0, 0.0), kTol));
}

// サブカーブ角点の右接線: LinearPath に委譲
TEST(CompositeCurveTangentAtTest, SubcurveCorner_RightTangent_Delegated) {
    auto cc = MakeLineLinearPath_LeftTurn();
    // Path.RightTangentAt(t_local=1): 出射辺 (2,0,0)→(2,1,0) 方向 = (0,1,0)
    const auto result = cc->RightTangentAt(2.0);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(*result, Vector3d(0.0, 1.0, 0.0), kTol));
}

// 非角点の内部点: サブカーブに委譲される
TEST(CompositeCurveTangentAtTest, Interior_Delegated) {
    auto cc = MakeLineLine_LeftTurn();
    // t=0.5 (Line1 内部): Line1 の接線 = (1,0,0)
    const auto result = cc->LeftTangentAt(0.5);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(i_num::IsApproxEqual(*result, Vector3d(1.0, 0.0, 0.0), kTol));
}



/**
 * CornerExteriorAngle のテスト
 * ICurve デフォルト実装を CompositeCurve の LeftTangentAt / RightTangentAt を
 * 通じて確認する
 */

// 左直角折れ接合点: 外角 = +π/2
TEST(CompositeCurveCornerAngleTest, Junction_LeftTurn_PiHalf) {
    auto cc = MakeLineLine_LeftTurn();
    const Vector3d normal(0.0, 0.0, 1.0);

    const auto angle = cc->CornerExteriorAngle(1.0, normal);

    ASSERT_TRUE(angle.has_value());
    EXPECT_NEAR(*angle, kPi / 2.0, kTol);
}

// 右直角折れ接合点: 外角 = -π/2
TEST(CompositeCurveCornerAngleTest, Junction_RightTurn_NegPiHalf) {
    auto cc = MakeLineLine_RightTurn();
    const Vector3d normal(0.0, 0.0, 1.0);

    const auto angle = cc->CornerExteriorAngle(1.0, normal);

    ASSERT_TRUE(angle.has_value());
    EXPECT_NEAR(*angle, -kPi / 2.0, kTol);
}

// 直線上接合点: 外角 = 0
// NOTE: GetCornerParams は角点として報告するが、接線連続のため外角は 0
TEST(CompositeCurveCornerAngleTest, Junction_Straight_ZeroAngle) {
    auto cc = MakeLineLine_Straight();
    const Vector3d normal(0.0, 0.0, 1.0);

    const auto angle = cc->CornerExteriorAngle(1.0, normal);

    ASSERT_TRUE(angle.has_value());
    EXPECT_NEAR(*angle, 0.0, kTol);
}

// サブカーブ角点（グローバル t=2）: Path の左折れ角点 → +π/2
TEST(CompositeCurveCornerAngleTest, SubcurveCorner_LeftTurn_PiHalf) {
    auto cc = MakeLineLinearPath_LeftTurn();
    const Vector3d normal(0.0, 0.0, 1.0);

    const auto angle = cc->CornerExteriorAngle(2.0, normal);

    ASSERT_TRUE(angle.has_value());
    EXPECT_NEAR(*angle, kPi / 2.0, kTol);
}

// ゼロ法線: nullopt を返す
TEST(CompositeCurveCornerAngleTest, ZeroNormal_Nullopt) {
    auto cc = MakeLineLine_LeftTurn();
    const Vector3d zero(0.0, 0.0, 0.0);

    EXPECT_FALSE(cc->CornerExteriorAngle(1.0, zero).has_value());
}

// 法線を反転すると外角の符号が逆転する
TEST(CompositeCurveCornerAngleTest, NormalFlipped_SignFlipped) {
    auto cc = MakeLineLine_LeftTurn();
    const Vector3d normal_pos(0.0, 0.0,  1.0);
    const Vector3d normal_neg(0.0, 0.0, -1.0);

    const auto angle_pos = cc->CornerExteriorAngle(1.0, normal_pos);
    const auto angle_neg = cc->CornerExteriorAngle(1.0, normal_neg);

    ASSERT_TRUE(angle_pos.has_value());
    ASSERT_TRUE(angle_neg.has_value());
    EXPECT_NEAR(*angle_neg, -(*angle_pos), kTol);
}



/**
 * TryGetSignedCurvature のテスト
 * 分岐: (1) ゼロ法線 → nullopt
 *       (2) 角点かつ外角>0 → +∞
 *       (3) 角点かつ外角<0 → -∞
 *       (4) 角点かつ外角=0 → 0.0
 *       (5) 直線部内 (非角点) → 0.0
 *       (6) それ以外 (滑らか) → 導関数から計算
 * NOTE: Line はGetLinearSegments をオーバライドしないため、Line 内部点では
 *       分岐(5) ではなく分岐(6) 経由で 0.0 が返される (C''=0 のため)
 */

// ゼロ法線 → nullopt
TEST(CompositeCurveSignedCurvatureTest, ZeroNormal_Nullopt) {
    auto cc = MakeLineLine_LeftTurn();
    const Vector3d zero(0.0, 0.0, 0.0);

    EXPECT_FALSE(cc->TryGetSignedCurvature(0.5, zero).has_value());
    EXPECT_FALSE(cc->TryGetSignedCurvature(1.0, zero).has_value());
}

// Line 内部点 → 0.0 (C''=0 のため導関数計算経路でも 0)
TEST(CompositeCurveSignedCurvatureTest, LineInterior_Zero) {
    auto cc = MakeLineLine_LeftTurn();
    const Vector3d normal(0.0, 0.0, 1.0);

    const auto kappa1 = cc->TryGetSignedCurvature(0.5, normal);
    const auto kappa2 = cc->TryGetSignedCurvature(1.5, normal);

    ASSERT_TRUE(kappa1.has_value());
    ASSERT_TRUE(kappa2.has_value());
    EXPECT_NEAR(*kappa1, 0.0, kTol);
    EXPECT_NEAR(*kappa2, 0.0, kTol);
}

// LinearPath セグメント内点 → 0.0 (IsInLinearSegment=true 経路)
TEST(CompositeCurveSignedCurvatureTest, LinearPathInterior_Zero) {
    auto cc = MakeLineLinearPath_LeftTurn();
    const Vector3d normal(0.0, 0.0, 1.0);

    // t=1.5: Path の最初のセグメント中点 (IsInLinearSegment=true)
    const auto kappa = cc->TryGetSignedCurvature(1.5, normal);

    ASSERT_TRUE(kappa.has_value());
    EXPECT_NEAR(*kappa, 0.0, kTol);
}

// 左折れ接合点（外角 > 0） → +∞
TEST(CompositeCurveSignedCurvatureTest, Junction_LeftTurn_PlusInf) {
    auto cc = MakeLineLine_LeftTurn();
    const Vector3d normal(0.0, 0.0, 1.0);

    const auto kappa = cc->TryGetSignedCurvature(1.0, normal);

    ASSERT_TRUE(kappa.has_value());
    EXPECT_TRUE(std::isinf(*kappa) && *kappa > 0.0);
}

// 右折れ接合点（外角 < 0） → -∞
TEST(CompositeCurveSignedCurvatureTest, Junction_RightTurn_MinusInf) {
    auto cc = MakeLineLine_RightTurn();
    const Vector3d normal(0.0, 0.0, 1.0);

    const auto kappa = cc->TryGetSignedCurvature(1.0, normal);

    ASSERT_TRUE(kappa.has_value());
    EXPECT_TRUE(std::isinf(*kappa) && *kappa < 0.0);
}

// 直線上接合点（外角 = 0） → 0.0
// NOTE: IsCorner=true だが CornerExteriorAngle=0 → 0.0 を返す
TEST(CompositeCurveSignedCurvatureTest, Junction_Straight_Zero) {
    auto cc = MakeLineLine_Straight();
    const Vector3d normal(0.0, 0.0, 1.0);

    const auto kappa = cc->TryGetSignedCurvature(1.0, normal);

    ASSERT_TRUE(kappa.has_value());
    EXPECT_NEAR(*kappa, 0.0, kTol);
}

// サブカーブ角点（左折れ Path 内部角点） → +∞
TEST(CompositeCurveSignedCurvatureTest, SubcurveCorner_LeftTurn_PlusInf) {
    auto cc = MakeLineLinearPath_LeftTurn();
    const Vector3d normal(0.0, 0.0, 1.0);

    const auto kappa = cc->TryGetSignedCurvature(2.0, normal);

    ASSERT_TRUE(kappa.has_value());
    EXPECT_TRUE(std::isinf(*kappa) && *kappa > 0.0);
}

// 始点・終点（角点でない; Line C''=0 経路） → 0.0
TEST(CompositeCurveSignedCurvatureTest, StartEnd_Zero) {
    auto cc = MakeLineLine_LeftTurn();
    const Vector3d normal(0.0, 0.0, 1.0);

    const auto kappa_start = cc->TryGetSignedCurvature(0.0, normal);
    const auto kappa_end   = cc->TryGetSignedCurvature(2.0, normal);

    ASSERT_TRUE(kappa_start.has_value());
    ASSERT_TRUE(kappa_end.has_value());
    EXPECT_NEAR(*kappa_start, 0.0, kTol);
    EXPECT_NEAR(*kappa_end,   0.0, kTol);
}

// 法線を反転すると符号が逆転する（左折れ接合点）
TEST(CompositeCurveSignedCurvatureTest, NormalFlipped_SignFlipped) {
    auto cc = MakeLineLine_LeftTurn();

    const auto kappa_pos =
        cc->TryGetSignedCurvature(1.0, Vector3d(0.0, 0.0,  1.0));
    const auto kappa_neg =
        cc->TryGetSignedCurvature(1.0, Vector3d(0.0, 0.0, -1.0));

    ASSERT_TRUE(kappa_pos.has_value());
    ASSERT_TRUE(kappa_neg.has_value());
    EXPECT_TRUE(std::isinf(*kappa_pos) && *kappa_pos > 0.0);
    EXPECT_TRUE(std::isinf(*kappa_neg) && *kappa_neg < 0.0);
}
