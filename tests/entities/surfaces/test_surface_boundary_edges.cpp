/**
 * @file tests/entities/surfaces/test_surface_boundary_edges.cpp
 * @brief ComputeParametricSurfaceEdges / ComputeRestrictedSurfaceEdges のテスト
 * @author Yayoi Habami
 * @date 2026-06-03
 * @copyright 2026 Yayoi Habami
 * @note 対象は公開関数:
 *       - entities::ComputeParametricSurfaceEdges(const ISurface&, params)
 *       - entities::ComputeRestrictedSurfaceEdges(const IRestrictedSurface&, params)
 *       検証する不変量:
 *       - 一般曲面: パラメータ矩形の4アイソ辺 / 端点が4隅に一致 / 分割数の反映 /
 *         閉方向の継ぎ目辺の除外 (include_seamsで切替)
 *       - 制限付き曲面: 未トリム外周=矩形4辺 / 穴で内周ループ追加 / 明示外周は単一ループ /
 *         閉基底での継ぎ目除外
 *       - 幾何整合: S(B(t))が基底曲面 (平面y=5) 上に乗る
 *
 * TODO: 無限パラメータ範囲のクランプ (テスト用曲面に無限域の曲面が無いため未カバー)。
 * TODO: GetBaseSurfaceがnullptrを返す制限付き曲面 (TrimmedSurfaceでは構築不能) は未カバー。
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/entities/curves/circular_arc.h"
#include "igesio/entities/curves/linear_path.h"
#include "igesio/entities/curves/curve_on_a_parametric_surface.h"
#include "igesio/entities/surfaces/trimmed_surface.h"
#include "igesio/entities/surfaces/algorithms/surface_boundary_edges.h"
#include "./surfaces_for_testing.h"

namespace {

namespace i_ent = igesio::entities;
namespace i_test = igesio::tests;
using igesio::Vector2d;
using igesio::Vector3d;
using i_ent::TrimmedSurface;
using i_ent::CurveOnAParametricSurface;
using i_ent::SurfaceBoundaryEdges;
using i_ent::SurfaceBoundaryEdgeParams;

/// @brief 位置の許容誤差
constexpr double kPosTol = 1e-6;



/**
 * テスト対象を構築するファクトリ (test_restricted_surface_mesh.cppと同方針)
 */

/// @brief 基底NURBS平面 S(u,v)=(-5+10u, 5, 5-10v), D=[0,1]² を作成する
std::shared_ptr<i_ent::ISurface> MakePlane() {
    return i_test::CreateRationalBSplineSurfaces()[0].surface;
}

/// @brief 一方向に閉じた全回転面 (0〜2π) を作成する
std::shared_ptr<i_ent::ISurface> MakeClosedBaseSurface() {
    return i_test::CreateSurfaceOfRevolutions()[0].surface;
}

/// @brief z=0平面上の軸平行矩形ループ (UV境界曲線) を作成する
std::shared_ptr<i_ent::ICurve> MakeUvRectLoop(
        const double umin, const double vmin,
        const double umax, const double vmax) {
    return i_ent::MakeLinearPath(
        std::vector<Vector2d>{
            {umin, vmin}, {umax, vmin}, {umax, vmax}, {umin, vmax}},
        true);
}

/// @brief z=0平面上の全円ループ (UV境界曲線) を作成する
std::shared_ptr<i_ent::ICurve> MakeUvCircle(
        const Vector2d& center, const double radius) {
    return i_ent::MakeCircle(center, radius);
}

/// @brief UV曲線をトリム境界 (Type142) に変換する
std::shared_ptr<CurveOnAParametricSurface> MakeBoundary142(
        const std::shared_ptr<i_ent::ISurface>& surface,
        const std::shared_ptr<i_ent::ICurve>& uv_loop) {
    return i_ent::MakeCurveOnAParametricSurface(surface, uv_loop).first;
}

/// @brief 基底曲面・外側境界・内側境界からTrimmedSurfaceを構築する
/// @param outer_uv 外側境界のUVループ。nullptrの場合はN1=0 (未トリム外周)
/// @param inner_uv 内側境界 (穴) のUVループ群
std::shared_ptr<TrimmedSurface> BuildTrimmed(
        const std::shared_ptr<i_ent::ISurface>& base,
        const std::shared_ptr<i_ent::ICurve>& outer_uv,
        const std::vector<std::shared_ptr<i_ent::ICurve>>& inner_uv = {}) {
    std::shared_ptr<CurveOnAParametricSurface> outer142;
    if (outer_uv) outer142 = MakeBoundary142(base, outer_uv);
    auto ts = std::make_shared<TrimmedSurface>(base, outer142);
    for (const auto& iv : inner_uv) {
        ts->AddInnerBoundary(MakeBoundary142(base, iv));
    }
    return ts;
}



/**
 * 検査ユーティリティ
 */

/// @brief 全ループの全頂点がy=5平面上にあることを検査する
void AssertAllOnPlaneY5(const SurfaceBoundaryEdges& edges) {
    for (const auto& loop : edges.loops) {
        for (const auto& p : loop) {
            EXPECT_NEAR(p.y(), 5.0, kPosTol);
        }
    }
}

/// @brief 点が4隅のいずれかに一致するか
bool MatchesAnyCorner(const Vector3d& p,
                      const std::vector<Vector3d>& corners) {
    return std::any_of(corners.begin(), corners.end(),
                       [&](const Vector3d& c) { return (p - c).norm() < kPosTol; });
}

}  // namespace



/**
 * ComputeParametricSurfaceEdges: 一般曲面
 */

TEST(ComputeParametricSurfaceEdges, Plane_ProducesFourEdgeLoops) {
    auto plane = MakePlane();

    const auto edges = i_ent::ComputeParametricSurfaceEdges(*plane);

    // 開いた平面 → 4本のアイソ辺、各 divisions+1 点、全点がy=5平面上
    EXPECT_EQ(edges.loops.size(), 4u);
    for (const auto& loop : edges.loops) {
        EXPECT_EQ(loop.size(), 65u);  // 既定 divisions=64
    }
    AssertAllOnPlaneY5(edges);
}

TEST(ComputeParametricSurfaceEdges, Plane_EdgeEndpointsMatchCorners) {
    auto plane = MakePlane();
    // S(u,v)=(-5+10u, 5, 5-10v): 4隅
    const std::vector<Vector3d> corners = {
        {-5., 5.,  5.},  // (u,v)=(0,0)
        { 5., 5.,  5.},  // (1,0)
        {-5., 5., -5.},  // (0,1)
        { 5., 5., -5.},  // (1,1)
    };

    const auto edges = i_ent::ComputeParametricSurfaceEdges(*plane);

    ASSERT_EQ(edges.loops.size(), 4u);
    for (const auto& loop : edges.loops) {
        ASSERT_GE(loop.size(), 2u);
        EXPECT_TRUE(MatchesAnyCorner(loop.front(), corners));
        EXPECT_TRUE(MatchesAnyCorner(loop.back(), corners));
    }
}

TEST(ComputeParametricSurfaceEdges, Divisions_ControlsSampleCount) {
    auto plane = MakePlane();

    const auto e8 = i_ent::ComputeParametricSurfaceEdges(*plane, {8, false});
    const auto e1 = i_ent::ComputeParametricSurfaceEdges(*plane, {1, false});

    ASSERT_EQ(e8.loops.size(), 4u);
    ASSERT_EQ(e1.loops.size(), 4u);
    for (const auto& loop : e8.loops) EXPECT_EQ(loop.size(), 9u);
    for (const auto& loop : e1.loops) EXPECT_EQ(loop.size(), 2u);
}

TEST(ComputeParametricSurfaceEdges, DivisionsZero_ProducesNoLoops) {
    auto plane = MakePlane();

    const auto edges = i_ent::ComputeParametricSurfaceEdges(*plane, {0, false});

    EXPECT_TRUE(edges.loops.empty());
}

TEST(ComputeParametricSurfaceEdges, ClosedSurface_SkipsSeamEdgesByDefault) {
    auto closed = MakeClosedBaseSurface();  // 一方向に閉じた全回転面

    const auto edges = i_ent::ComputeParametricSurfaceEdges(*closed);

    // 閉方向の2本 (継ぎ目) が除外され、開方向の2本のみ残る
    EXPECT_EQ(edges.loops.size(), 2u);
}

TEST(ComputeParametricSurfaceEdges, ClosedSurface_IncludeSeams_KeepsAllEdges) {
    auto closed = MakeClosedBaseSurface();

    const auto edges = i_ent::ComputeParametricSurfaceEdges(*closed, {64, true});

    // include_seams=true なら閉方向の継ぎ目辺も含め4本
    EXPECT_EQ(edges.loops.size(), 4u);
}



/**
 * ComputeRestrictedSurfaceEdges: 制限付き曲面
 */

TEST(ComputeRestrictedSurfaceEdges, UntrimmedOuter_EqualsParametricRectangle) {
    auto plane = MakePlane();
    auto ts = BuildTrimmed(plane, nullptr);  // N1=0, 穴なし

    const auto edges = i_ent::ComputeRestrictedSurfaceEdges(*ts);
    const auto base_edges = i_ent::ComputeParametricSurfaceEdges(*plane);

    // 未トリム外周は基底曲面のパラメータ矩形と一致する
    EXPECT_EQ(edges.loops.size(), 4u);
    EXPECT_EQ(edges.loops.size(), base_edges.loops.size());
    AssertAllOnPlaneY5(edges);
}

TEST(ComputeRestrictedSurfaceEdges, RectHole_AddsInnerLoop) {
    auto plane = MakePlane();
    // N1=0 (外周は矩形) + 中央の円穴
    auto ts = BuildTrimmed(plane, nullptr, {MakeUvCircle({0.5, 0.5}, 0.2)});

    const auto edges = i_ent::ComputeRestrictedSurfaceEdges(*ts);

    // 外周4辺 + 内周(穴)1ループ = 5ループ
    EXPECT_EQ(edges.loops.size(), 5u);
    AssertAllOnPlaneY5(edges);
}

TEST(ComputeRestrictedSurfaceEdges, ExplicitOuter_ProducesSingleOuterLoop) {
    auto plane = MakePlane();
    auto ts = BuildTrimmed(plane, MakeUvRectLoop(0.1, 0.1, 0.9, 0.9));

    const auto edges = i_ent::ComputeRestrictedSurfaceEdges(*ts);

    // 明示外周 (Type142) は S(B(t)) の単一閉ループ、穴なし
    EXPECT_EQ(edges.loops.size(), 1u);
    AssertAllOnPlaneY5(edges);
}

TEST(ComputeRestrictedSurfaceEdges, ExplicitOuterWithHole_ProducesTwoLoops) {
    auto plane = MakePlane();
    auto ts = BuildTrimmed(plane, MakeUvRectLoop(0.1, 0.1, 0.9, 0.9),
                           {MakeUvCircle({0.5, 0.5}, 0.15)});

    const auto edges = i_ent::ComputeRestrictedSurfaceEdges(*ts);

    // 明示外周1ループ + 内周1ループ
    EXPECT_EQ(edges.loops.size(), 2u);
    AssertAllOnPlaneY5(edges);
}

TEST(ComputeRestrictedSurfaceEdges, ClosedBaseUntrimmed_SkipsSeam) {
    auto closed = MakeClosedBaseSurface();
    auto ts = BuildTrimmed(closed, nullptr);  // N1=0, 穴なし

    const auto edges = i_ent::ComputeRestrictedSurfaceEdges(*ts);

    // 閉基底の未トリム外周は継ぎ目を除外し、開方向の2辺のみ
    EXPECT_EQ(edges.loops.size(), 2u);
}
