/**
 * @file tests/numerics/test_mesh_line_intersection.cpp
 * @brief 三角形メッシュと直線/半直線/線分の交差判定の検証
 * @author Yayoi Habami
 * @date 2026-06-11
 * @copyright 2026 Yayoi Habami
 * @note テスト対象:
 *       - `IntersectMeshWithLine` (Möller–Trumbore法・線形走査)
 *         - 正常系 (代表値): 三角形内部のヒット・裏面ヒット・線種別の範囲・
 *           t昇順ソート・三角形番号・重心座標
 *         - 正常系 (境界値): 線分の端点 (t=1) ちょうど・共有エッジ上の重複除去
 *         - 正常系 (退化): 空メッシュ・退化三角形・長さゼロの方向ベクトル
 * @note TODO: 異常系 (例外) は対象外. 本関数は前提条件違反 (不正メッシュ) を
 *       検査せずValidate済みメッシュを前提とし、例外を送出しない設計のため.
 */
#include <gtest/gtest.h>

#include <vector>

#include "igesio/numerics/meshes/triangle_mesh.h"
#include "igesio/numerics/meshes/algorithms.h"

namespace {

namespace i_num = igesio::numerics;
using igesio::Vector3d;
using i_num::BoundingBox;
using i_num::IntersectMeshWithLine;
using i_num::TriangleMeshd;

/// @brief 数値比較の許容誤差
constexpr double kTol = 1e-12;

/// @brief z=0平面上の単位直角三角形メッシュを作成する
/// @return 頂点 (0,0,0), (1,0,0), (0,1,0) の1枚の三角形.
///         重心座標は交点(x,y,0)に対しu=x, v=yとなる配置
TriangleMeshd MakeSingleTriangle() {
    TriangleMeshd mesh;
    mesh.positions.resize(3, 3);
    mesh.positions.col(0) = Vector3d(0.0, 0.0, 0.0);
    mesh.positions.col(1) = Vector3d(1.0, 0.0, 0.0);
    mesh.positions.col(2) = Vector3d(0.0, 1.0, 0.0);
    mesh.indices = {0, 1, 2};
    return mesh;
}

/// @brief z=0平面上の単位正方形メッシュ (2三角形) を作成する
/// @return 頂点 (0,0), (1,0), (1,1), (0,1). 対角線 (0,0)-(1,1) を共有する
///         三角形0 = {0,1,2} (y<=x側)、三角形1 = {0,2,3} (y>=x側)
TriangleMeshd MakeUnitQuad() {
    TriangleMeshd mesh;
    mesh.positions.resize(3, 4);
    mesh.positions.col(0) = Vector3d(0.0, 0.0, 0.0);
    mesh.positions.col(1) = Vector3d(1.0, 0.0, 0.0);
    mesh.positions.col(2) = Vector3d(1.0, 1.0, 0.0);
    mesh.positions.col(3) = Vector3d(0.0, 1.0, 0.0);
    mesh.indices = {0, 1, 2, 0, 2, 3};
    return mesh;
}

/// @brief z=0とz=2の平面に1枚ずつ三角形を持つメッシュを作成する
/// @return xy配置は同一 (単位直角三角形). 三角形0がz=0、三角形1がz=2
TriangleMeshd MakeStackedTriangles() {
    TriangleMeshd mesh;
    mesh.positions.resize(3, 6);
    mesh.positions.col(0) = Vector3d(0.0, 0.0, 0.0);
    mesh.positions.col(1) = Vector3d(1.0, 0.0, 0.0);
    mesh.positions.col(2) = Vector3d(0.0, 1.0, 0.0);
    mesh.positions.col(3) = Vector3d(0.0, 0.0, 2.0);
    mesh.positions.col(4) = Vector3d(1.0, 0.0, 2.0);
    mesh.positions.col(5) = Vector3d(0.0, 1.0, 2.0);
    mesh.indices = {0, 1, 2, 3, 4, 5};
    return mesh;
}

/// @brief 3頂点が同一直線上にある退化三角形のみのメッシュを作成する
TriangleMeshd MakeDegenerateTriangle() {
    TriangleMeshd mesh;
    mesh.positions.resize(3, 3);
    mesh.positions.col(0) = Vector3d(0.0, 0.0, 0.0);
    mesh.positions.col(1) = Vector3d(1.0, 0.0, 0.0);
    mesh.positions.col(2) = Vector3d(2.0, 0.0, 0.0);
    mesh.indices = {0, 1, 2};
    return mesh;
}

}  // namespace



/**
 * 正常系 (代表値)
 */

// 三角形内部を貫くレイは1点でヒットし、t・交点・三角形番号・重心座標が正しい
TEST(IntersectMeshWithLineTest, HitsTriangleInterior) {
    const auto mesh = MakeSingleTriangle();
    const Vector3d p0(0.25, 0.25, 5.0);
    const Vector3d p1 = p0 + Vector3d(0.0, 0.0, -1.0);

    const auto hits = IntersectMeshWithLine(
            mesh, p0, p1, BoundingBox::DirectionType::kRay);

    ASSERT_EQ(hits.size(), 1u);
    EXPECT_NEAR(hits[0].t, 5.0, kTol);
    EXPECT_NEAR(hits[0].position.x(), 0.25, kTol);
    EXPECT_NEAR(hits[0].position.y(), 0.25, kTol);
    EXPECT_NEAR(hits[0].position.z(), 0.0, kTol);
    EXPECT_EQ(hits[0].triangle_index, 0u);
    // この頂点配置では交点(x,y,0)に対しu=x, v=y
    EXPECT_NEAR(hits[0].barycentric_u, 0.25, kTol);
    EXPECT_NEAR(hits[0].barycentric_v, 0.25, kTol);
}

// 三角形の外側 (u+v>1の領域) を通るレイはヒットしない
TEST(IntersectMeshWithLineTest, MissesOutsideTriangle) {
    const auto mesh = MakeSingleTriangle();
    const Vector3d p0(0.9, 0.9, 5.0);
    const Vector3d p1 = p0 + Vector3d(0.0, 0.0, -1.0);

    const auto hits = IntersectMeshWithLine(
            mesh, p0, p1, BoundingBox::DirectionType::kRay);
    EXPECT_TRUE(hits.empty());
}

// 裏面側 (法線と同方向) から入射するレイもヒットする (表裏は区別しない)
TEST(IntersectMeshWithLineTest, HitsBackFace) {
    const auto mesh = MakeSingleTriangle();
    const Vector3d p0(0.25, 0.25, -5.0);
    const Vector3d p1 = p0 + Vector3d(0.0, 0.0, 1.0);

    const auto hits = IntersectMeshWithLine(
            mesh, p0, p1, BoundingBox::DirectionType::kRay);
    ASSERT_EQ(hits.size(), 1u);
    EXPECT_NEAR(hits[0].t, 5.0, kTol);
}

// 始点より後方の交点はkRayでは除外され、kLineでは負のtでヒットする
TEST(IntersectMeshWithLineTest, RayExcludesBehindOriginButLineDoesNot) {
    const auto mesh = MakeSingleTriangle();
    // 三角形はレイの後方 (始点z=-5・方向-z) にある
    const Vector3d p0(0.25, 0.25, -5.0);
    const Vector3d p1 = p0 + Vector3d(0.0, 0.0, -1.0);

    EXPECT_TRUE(IntersectMeshWithLine(
            mesh, p0, p1, BoundingBox::DirectionType::kRay).empty());

    const auto line_hits = IntersectMeshWithLine(
            mesh, p0, p1, BoundingBox::DirectionType::kLine);
    ASSERT_EQ(line_hits.size(), 1u);
    EXPECT_NEAR(line_hits[0].t, -5.0, kTol);
}

// 複数の交点はt昇順で返される
TEST(IntersectMeshWithLineTest, HitsSortedByDistance) {
    const auto mesh = MakeStackedTriangles();
    const Vector3d p0(0.25, 0.25, 5.0);
    const Vector3d p1 = p0 + Vector3d(0.0, 0.0, -1.0);

    const auto hits = IntersectMeshWithLine(
            mesh, p0, p1, BoundingBox::DirectionType::kRay);

    ASSERT_EQ(hits.size(), 2u);
    EXPECT_NEAR(hits[0].t, 3.0, kTol);  // z=2の三角形が先
    EXPECT_EQ(hits[0].triangle_index, 1u);
    EXPECT_NEAR(hits[1].t, 5.0, kTol);  // z=0の三角形が後
    EXPECT_EQ(hits[1].triangle_index, 0u);
}

// 三角形番号はヒットした三角形を正しく識別する
TEST(IntersectMeshWithLineTest, TriangleIndexIdentifiesHitTriangle) {
    const auto mesh = MakeUnitQuad();
    const Vector3d dir(0.0, 0.0, -1.0);

    // (0.75, 0.25) はy<=x側 = 三角形0
    const Vector3d a0(0.75, 0.25, 5.0);
    const auto hits0 = IntersectMeshWithLine(
            mesh, a0, a0 + dir, BoundingBox::DirectionType::kRay);
    ASSERT_EQ(hits0.size(), 1u);
    EXPECT_EQ(hits0[0].triangle_index, 0u);

    // (0.25, 0.75) はy>=x側 = 三角形1
    const Vector3d a1(0.25, 0.75, 5.0);
    const auto hits1 = IntersectMeshWithLine(
            mesh, a1, a1 + dir, BoundingBox::DirectionType::kRay);
    ASSERT_EQ(hits1.size(), 1u);
    EXPECT_EQ(hits1[0].triangle_index, 1u);
}

// floatメッシュでも計算はdoubleで行われ、同じ交点が得られる
TEST(IntersectMeshWithLineTest, FloatMeshComputesInDouble) {
    const auto mesh = i_num::CastScalar<float>(MakeSingleTriangle());
    const Vector3d p0(0.25, 0.25, 5.0);
    const Vector3d p1 = p0 + Vector3d(0.0, 0.0, -1.0);

    const auto hits = IntersectMeshWithLine(
            mesh, p0, p1, BoundingBox::DirectionType::kRay);
    ASSERT_EQ(hits.size(), 1u);
    EXPECT_NEAR(hits[0].t, 5.0, 1e-6);  // 頂点がfloat精度のため許容を緩める
}



/**
 * 正常系 (境界値)
 */

// 線分の終端 (t=1) ちょうどの交点はヒットし、僅かに届かない線分はヒットしない
TEST(IntersectMeshWithLineTest, SegmentEndpointBoundary) {
    const auto mesh = MakeSingleTriangle();
    const Vector3d p0(0.25, 0.25, 1.0);

    // 終端がちょうど三角形上 (t=1)
    const auto on_hits = IntersectMeshWithLine(
            mesh, p0, Vector3d(0.25, 0.25, 0.0),
            BoundingBox::DirectionType::kSegment);
    ASSERT_EQ(on_hits.size(), 1u);
    EXPECT_NEAR(on_hits[0].t, 1.0, kTol);

    // 終端が三角形の手前 (交点はt=2で範囲外)
    const auto short_hits = IntersectMeshWithLine(
            mesh, p0, Vector3d(0.25, 0.25, 0.5),
            BoundingBox::DirectionType::kSegment);
    EXPECT_TRUE(short_hits.empty());
}

// 共有エッジ上を貫くレイの重複ヒットは1つに除去される
TEST(IntersectMeshWithLineTest, SharedEdgeHitDeduplicated) {
    const auto mesh = MakeUnitQuad();
    // 対角線 (0,0)-(1,1) の中点を貫く
    const Vector3d p0(0.5, 0.5, 5.0);
    const Vector3d p1 = p0 + Vector3d(0.0, 0.0, -1.0);

    const auto hits = IntersectMeshWithLine(
            mesh, p0, p1, BoundingBox::DirectionType::kRay);
    ASSERT_EQ(hits.size(), 1u);
    EXPECT_NEAR(hits[0].t, 5.0, kTol);
}



/**
 * 正常系 (退化)
 */

// 空メッシュは空リストを返す
TEST(IntersectMeshWithLineTest, EmptyMeshReturnsEmpty) {
    const TriangleMeshd mesh;
    const auto hits = IntersectMeshWithLine(
            mesh, Vector3d(0.0, 0.0, 1.0), Vector3d(0.0, 0.0, 0.0),
            BoundingBox::DirectionType::kRay);
    EXPECT_TRUE(hits.empty());
}

// 退化三角形 (3頂点が同一直線上) はスキップされ、クラッシュしない
TEST(IntersectMeshWithLineTest, DegenerateTriangleIgnored) {
    const auto mesh = MakeDegenerateTriangle();
    // 退化三角形の「上」を貫くレイ
    const Vector3d p0(1.0, 0.0, 5.0);
    const Vector3d p1 = p0 + Vector3d(0.0, 0.0, -1.0);

    const auto hits = IntersectMeshWithLine(
            mesh, p0, p1, BoundingBox::DirectionType::kRay);
    EXPECT_TRUE(hits.empty());
}

// p0とp1が一致する場合 (方向が定義できない) は空リストを返す
TEST(IntersectMeshWithLineTest, ZeroLengthDirectionReturnsEmpty) {
    const auto mesh = MakeSingleTriangle();
    const Vector3d p0(0.25, 0.25, 0.0);  // 三角形上の点でも交差は判定しない

    const auto hits = IntersectMeshWithLine(
            mesh, p0, p0, BoundingBox::DirectionType::kLine);
    EXPECT_TRUE(hits.empty());
}
