/**
 * @file numerics/test_polygon.cpp
 * @brief numerics/polygon.hのテスト
 * @author Yayoi Habami
 * @date 2026-04-12
 * @copyright 2026 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <vector>

#include "igesio/numerics/polygon.h"

namespace {

namespace i_num = igesio::numerics;
using igesio::Vector3d;

/// @brief 円周率
const double kPi = std::acos(-1.0);

// ===========================================================================
// テスト用多角形フィクスチャ
// ===========================================================================
//
// 単位円を近似する3種類の多角形を手動で構築する.
//
// 【外包多角形】
//   単位円の外接正方形（接線正方形）. 各辺の接点（on_curve=true）と
//   隣接する辺の交点（on_curve=false）が交互に並ぶ 8 頂点の多角形.
//   接点:   (±1,0), (0,±1)   (t = 0, π/2, π, 3π/2)
//   コーナー: (1,1), (-1,1), (-1,-1), (1,-1)  (curve_params = 0.0)
//
// 【内包多角形】
//   単位円に内接する正八角形. 全頂点が曲線上 (on_curve=true) にある.
//   各頂点は単位円上 (R=1.0) に等間隔に配置され、辺は円の内側を通る弦.
//   内接円半径 = cos(π/8) ≈ 0.924 → これより内側の点は Step 2 で判定可能.
//
// 【近似多角形】
//   単位円を 16 分割した正多角形. 全頂点が曲線上 (on_curve=true) にある.
//   内包多角形の各辺（パラメータ幅 π/4）の中間点を含む.

/// @brief 単位円の外包多角形（接線正方形、8頂点）を構築する
i_num::PolygonData MakeCircumscribed() {
    // 頂点順: 接点(on_curve=true) → コーナー(on_curve=false) が交互に並ぶ
    // 接点の curve_params は t = 0, π/2, π, 3π/2 (コーナーは 0.0)
    const std::vector<Vector3d> verts = {
        Vector3d(+1.0,  0.0, 0.0),  // 接点, t=0
        Vector3d(+1.0,  1.0, 0.0),  // コーナー
        Vector3d(+0.0,  1.0, 0.0),  // 接点, t=π/2
        Vector3d(-1.0,  1.0, 0.0),  // コーナー
        Vector3d(-1.0,  0.0, 0.0),  // 接点, t=π
        Vector3d(-1.0, -1.0, 0.0),  // コーナー
        Vector3d(+0.0, -1.0, 0.0),  // 接点, t=3π/2
        Vector3d(+1.0, -1.0, 0.0),  // コーナー
    };
    const std::vector<bool> on_curve = {
        true, false, true, false, true, false, true, false
    };
    const std::vector<double> params = {
        0.0, 0.0, kPi / 2.0, 0.0, kPi, 0.0, 3.0 * kPi / 2.0, 0.0
    };
    return {verts, on_curve, params};
}

/// @brief 単位円の内包多角形（内接正八角形、8頂点）を構築する
i_num::PolygonData MakeInscribed() {
    // 全頂点が単位円上 (on_curve=true)
    std::vector<Vector3d> verts;
    std::vector<bool> on_curve;
    std::vector<double> params;
    for (int k = 0; k < 8; ++k) {
        const double t = k * kPi / 4.0;
        verts.push_back(Vector3d(std::cos(t), std::sin(t), 0.0));
        on_curve.push_back(true);
        params.push_back(t);
    }
    return {verts, on_curve, params};
}

/// @brief 単位円の近似多角形（16分割、16頂点）を構築する
i_num::PolygonData MakeApproximate() {
    // 全頂点が単位円上 (on_curve=true)、内包多角形の各辺の中点を含む
    std::vector<Vector3d> verts;
    std::vector<bool> on_curve;
    std::vector<double> params;
    for (int k = 0; k < 16; ++k) {
        const double t = k * kPi / 8.0;
        verts.push_back(Vector3d(std::cos(t), std::sin(t), 0.0));
        on_curve.push_back(true);
        params.push_back(t);
    }
    return {verts, on_curve, params};
}

/// @brief テスト用の CurveContainmentPolygons を構築する
i_num::CurveContainmentPolygons MakeUnitCirclePolygons() {
    return {MakeCircumscribed(), MakeInscribed(), MakeApproximate()};
}

}  // namespace



/**
 * PolygonData::Count のテスト
 */

/// @brief 空の多角形は 0 を返す
TEST(PolygonDataCountTest, Empty) {
    i_num::PolygonData p;
    EXPECT_EQ(p.Count(), 0);
}

/// @brief 頂点数と等しい値を返す
TEST(PolygonDataCountTest, ReturnsVertexCount) {
    const auto circ = MakeCircumscribed();
    EXPECT_EQ(circ.Count(), 8);

    const auto inscribed = MakeInscribed();
    EXPECT_EQ(inscribed.Count(), 8);

    const auto approx = MakeApproximate();
    EXPECT_EQ(approx.Count(), 16);
}



/**
 * PolygonData::GetCurveParamIndex のテスト
 */

/// @brief on_curve=false 頂点を跨いで on_curve=true の頂点を返す (通常ケース)
///
/// 外包多角形は on_curve: [true, false, true, false, ...] のパターン.
/// edge_index=0 の辺は頂点 0 (on_curve=true) から頂点 1 (on_curve=false) への辺.
/// → 始点側は頂点 0、終点側は頂点 2 (次の on_curve=true) が返る.
TEST(PolygonDataGetCurveParamIndexTest, Normal_SkipsOffCurveVertex) {
    const auto circ = MakeCircumscribed();

    const auto [i_start, i_end] = circ.GetCurveParamIndex(0);
    EXPECT_EQ(i_start, 0);
    EXPECT_EQ(i_end, 2);
}

/// @brief 全頂点 on_curve=true の多角形では隣接頂点インデックスを返す
TEST(PolygonDataGetCurveParamIndexTest, Normal_AllOnCurve) {
    const auto inscribed = MakeInscribed();

    const auto [i_start, i_end] = inscribed.GetCurveParamIndex(3);
    EXPECT_EQ(i_start, 3);
    EXPECT_EQ(i_end, 4);
}

/// @brief off_curve 頂点を始点とする辺では遡って on_curve 頂点を返す
TEST(PolygonDataGetCurveParamIndexTest, Normal_StartsFromOffCurveEdge) {
    const auto circ = MakeCircumscribed();

    // edge_index=1: 頂点 1 (off_curve) → 頂点 2 (on_curve=true)
    // 始点を遡ると頂点 0 (on_curve=true)
    const auto [i_start, i_end] = circ.GetCurveParamIndex(1);
    EXPECT_EQ(i_start, 0);
    EXPECT_EQ(i_end, 2);
}

/// @brief 末尾辺で折り返しが発生する (i_start > i_end)
///
/// 外包多角形の edge_index=7: 頂点 7 (off_curve) → 頂点 0 (on_curve=true)
/// → 始点側を遡ると頂点 6 (on_curve=true), 終点側は頂点 0
/// → i_start=6 > i_end=0 となる.
TEST(PolygonDataGetCurveParamIndexTest, Wrap_IsStartGreaterThanEnd) {
    const auto circ = MakeCircumscribed();

    const auto [i_start, i_end] = circ.GetCurveParamIndex(7);
    EXPECT_EQ(i_start, 6);
    EXPECT_EQ(i_end, 0);
    EXPECT_GT(i_start, i_end);
}

/// @brief 内包多角形（全 on_curve=true）の最終辺でも折り返しが発生する
TEST(PolygonDataGetCurveParamIndexTest, Wrap_AllOnCurve_LastEdge) {
    const auto inscribed = MakeInscribed();

    // edge_index=7 (頂点 7 → 頂点 0): i_start=7 > i_end=0
    const auto [i_start, i_end] = inscribed.GetCurveParamIndex(7);
    EXPECT_EQ(i_start, 7);
    EXPECT_EQ(i_end, 0);
    EXPECT_GT(i_start, i_end);
}

/// @brief 範囲外の edge_index は out_of_range 例外を投げる
TEST(PolygonDataGetCurveParamIndexTest, OutOfRange_Throws) {
    const auto inscribed = MakeInscribed();

    EXPECT_THROW(inscribed.GetCurveParamIndex(-1), std::out_of_range);
    EXPECT_THROW(inscribed.GetCurveParamIndex(8), std::out_of_range);
}

/// @brief 全頂点 on_curve=false の場合は invalid_argument 例外を投げる
TEST(PolygonDataGetCurveParamIndexTest, AllOffCurve_ThrowsInvalidArgument) {
    i_num::PolygonData p;
    p.vertices = {Vector3d(1, 0, 0), Vector3d(0, 1, 0), Vector3d(-1, 0, 0)};
    p.on_curve = {false, false, false};
    p.curve_params = {0.0, 0.0, 0.0};

    EXPECT_THROW(p.GetCurveParamIndex(0), std::invalid_argument);
}



/**
 * IsPointInPolygon のテスト
 *
 * 単位円の外包多角形・内包多角形・近似多角形を用いて内外判定を行う.
 *
 * Step 1: 外包多角形の外部 → 確実に false
 * Step 2: 内包多角形の内部 → 確実に true
 * Step 3: 境界域（外包内 かつ 内包外）→ 再構成多角形による精密判定
 *
 * 【Step 3 の構造】
 *   再構成多角形 = 最近傍辺の頂点列（末端除く）
 *                + 近似多角形の対応パラメータ範囲（逆順、先頭除く）
 *
 *   内包辺使用時 (use_inscribed=true):
 *     再構成多角形 CW → 内部点の winding = -1 ≠ 0 → inside=true → true を返す
 *   外包辺使用時 (use_inscribed=false):
 *     再構成多角形 CCW → 内部点の winding = 1 ≠ 0 → inside=true → !true=false を返す
 */

/// @brief 外包多角形の外部の点は false を返す (Step 1)
TEST(IsPointInPolygonTest, OutsideCircumscribed_ReturnsFalse) {
    const auto polygons = MakeUnitCirclePolygons();
    EXPECT_FALSE(i_num::IsPointInPolygon(Vector3d(2.0, 0.0, 0.0), polygons));
    EXPECT_FALSE(i_num::IsPointInPolygon(Vector3d(0.0, 2.0, 0.0), polygons));
    EXPECT_FALSE(i_num::IsPointInPolygon(Vector3d(-1.5, -1.5, 0.0), polygons));
}

/// @brief 内包多角形の内部の点は true を返す (Step 2)
///
/// 内接正八角形の内接円半径は cos(π/8) ≈ 0.924.
/// これより十分内側の点は Step 2 で捕捉される.
TEST(IsPointInPolygonTest, InsideInscribed_ReturnsTrue) {
    const auto polygons = MakeUnitCirclePolygons();
    EXPECT_TRUE(i_num::IsPointInPolygon(Vector3d(0.0, 0.0, 0.0), polygons));
    EXPECT_TRUE(i_num::IsPointInPolygon(Vector3d(0.5, 0.3, 0.0), polygons));
    EXPECT_TRUE(i_num::IsPointInPolygon(Vector3d(-0.6, 0.5, 0.0), polygons));
}

/// @brief z 成分は判定に影響しない
TEST(IsPointInPolygonTest, ZComponentIsIgnored) {
    const auto polygons = MakeUnitCirclePolygons();
    // z=0 と z=99 で同一の判定結果になることを確認
    EXPECT_FALSE(i_num::IsPointInPolygon(Vector3d(2.0, 0.0, 0.0), polygons));
    EXPECT_FALSE(i_num::IsPointInPolygon(Vector3d(2.0, 0.0, 99.0), polygons));
    EXPECT_TRUE(i_num::IsPointInPolygon(Vector3d(0.5, 0.3, 0.0), polygons));
    EXPECT_TRUE(i_num::IsPointInPolygon(Vector3d(0.5, 0.3, -50.0), polygons));
}

/// @brief 境界域の点: 外包辺使用, 曲線の外側 → false (Step 3)
///
/// (0.95, 0.4): dist≈1.031 > 1 (曲線外), 内包八角形外.
/// 外包辺 0 ((1,0)→(1,1)) への距離² = 0.0025 < 内包辺 0 への距離² ≈ 0.012
/// → use_inscribed=false, 再構成多角形内 → !inside=false.
TEST(IsPointInPolygonTest, BoundaryZone_OutsideCurve_CircumscribedPath_ReturnsFalse) {
    const auto polygons = MakeUnitCirclePolygons();
    EXPECT_FALSE(i_num::IsPointInPolygon(Vector3d(0.95, 0.4, 0.0), polygons));
}

/// @brief 境界域の点: 内包辺使用, 曲線の内側 (edge 0 の cap 内) → true (Step 3)
///
/// (0.891, 0.369): dist≈0.964 < 1 (曲線内), 内包八角形 edge 0 の弓形領域.
/// 内包辺 0 ((1,0)→(0.707,0.707)) への距離² ≈ 0.0016 < 外包辺への距離²
/// → use_inscribed=true, 再構成三角形 [(1,0),(0.707,0.707),(0.924,0.383)] CW
/// → winding=-1 → inside=true.
TEST(IsPointInPolygonTest, BoundaryZone_InsideCurve_InscribedPath_ReturnsTrue) {
    const auto polygons = MakeUnitCirclePolygons();
    EXPECT_TRUE(i_num::IsPointInPolygon(Vector3d(0.891, 0.369, 0.0), polygons));
}

/// @brief 境界域の点: 内包辺使用, 曲線の外側 → false (Step 3)
///
/// (0.8, 0.7): dist≈1.063 > 1 (曲線外), 内包八角形 edge 1 に最近傍.
/// 内包辺 1 ((0.707,0.707)→(0,1)) への距離² ≈ 0.0087 < 外包辺への距離² ≈ 0.04
/// → use_inscribed=true, 再構成三角形 [(0.707,0.707),(0,1),(0.383,0.924)] CW
/// → winding=0 → inside=false.
TEST(IsPointInPolygonTest, BoundaryZone_OutsideCurve_InscribedPath_ReturnsFalse) {
    const auto polygons = MakeUnitCirclePolygons();
    EXPECT_FALSE(i_num::IsPointInPolygon(Vector3d(0.8, 0.7, 0.0), polygons));
}

/// @brief 境界域の点: 内包辺使用, wrap-around (edge 7 の cap 内) → true (Step 3)
///
/// (0.91, -0.37): dist≈0.982 < 1 (曲線内), 内包八角形 edge 7 の弓形領域.
/// 内包辺 7 ((0.707,-0.707)→(1,0)) が最近傍 → GetCurveParamIndex(7) で
/// i_start=7 > i_end=0 (is_wrap=true).
/// 再構成三角形 [(0.707,-0.707),(1,0),(0.924,-0.383)] CW → winding=-1 → inside=true.
TEST(IsPointInPolygonTest, BoundaryZone_InsideCurve_WrapAround_ReturnsTrue) {
    const auto polygons = MakeUnitCirclePolygons();
    EXPECT_TRUE(i_num::IsPointInPolygon(Vector3d(0.91, -0.37, 0.0), polygons));
}
