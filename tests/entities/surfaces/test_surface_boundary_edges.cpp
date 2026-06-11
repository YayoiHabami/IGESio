/**
 * @file tests/entities/surfaces/test_surface_boundary_edges.cpp
 * @brief ComputeParametricSurfaceEdges / ComputeRestrictedSurfaceEdges のテスト
 * @author Yayoi Habami
 * @date 2026-06-03
 * @copyright 2026 Yayoi Habami
 * @note 対象は公開関数:
 *       - entities::ComputeParametricSurfaceEdges(const ISurface&, params)
 *       - entities::ComputeRestrictedSurfaceEdges(const IRestrictedSurface&, params)
 *       - entities::ComputeSurfaceCreaseEdges(const ISurface&, params)
 *       検証する不変量:
 *       - 一般曲面: パラメータ矩形の4アイソ辺 / 端点が4隅に一致 / 分割数の反映 /
 *         閉方向の継ぎ目辺の除外 (include_seamsで切替)
 *       - 制限付き曲面: 未トリム外周=矩形4辺 / 穴で内周ループ追加 / 明示外周は単一ループ /
 *         閉基底での継ぎ目除外
 *       - 折り目稜線: 内部の折り目1つにつき1ループ / S(u_c, v)アイソ曲線に一致 /
 *         滑らかな曲面では空 / 分割数の反映
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
#include "igesio/entities/curves/line.h"
#include "igesio/entities/curves/linear_path.h"
#include "igesio/entities/curves/curve_on_a_parametric_surface.h"
#include "igesio/entities/surfaces/surface_of_revolution.h"
#include "igesio/entities/surfaces/tabulated_cylinder.h"
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
using igesio::kPi;

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
    auto ts = i_ent::MakeTrimmedSurface(base, outer142, {});
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

/// @brief 折り目(角)を2つ持つ段付き回転面を作成する (Y軸回り; 母線はXY平面内)
/// @note 母線(1,0,0)-(1,2,0)-(2,2,0)-(2,4,0): 折り目は弧長2.0(半径1,高さ2)と
///       3.0(半径2,高さ2)。全周回転のためv方向に閉じる。
std::shared_ptr<i_ent::ISurface> MakeSteppedRevolution() {
    auto axis = i_ent::MakeLine(Vector3d{0., 0., 0.}, Vector3d{0., 1., 0.});
    auto gen = i_ent::MakeLinearPath(std::vector<Vector3d>{
        Vector3d{1., 0., 0.}, Vector3d{1., 2., 0.},
        Vector3d{2., 2., 0.}, Vector3d{2., 4., 0.}});
    return i_ent::MakeSurfaceOfRevolution(axis, gen, 0., 2. * kPi);
}

/// @brief 折り目(角)を1つ持つL字準線の平行曲面を作成する
/// @note 準線(-3,0,0)-(0,0,0)-(0,3,0)を+Zへ押し出す。折り目はu=0.5。
std::shared_ptr<i_ent::ISurface> MakeLShapedExtrusion() {
    auto dir = i_ent::MakeLinearPath(std::vector<Vector3d>{
        Vector3d{-3., 0., 0.}, Vector3d{0., 0., 0.}, Vector3d{0., 3., 0.}});
    return i_ent::MakeExtrudedSurface(dir, Vector3d{0., 0., 5.});
}

/// @brief ループの各点がv方向アイソ曲線 S(u_c, v) 上に乗ることを検査する
void AssertLoopIsVIsoCurve(const i_ent::ISurface& surf, const double uc,
                           const double v0, const double v1,
                           const std::vector<Vector3d>& loop) {
    ASSERT_GE(loop.size(), 2u);
    const int div = static_cast<int>(loop.size()) - 1;
    for (int k = 0; k <= div; ++k) {
        const double v = v0 + (v1 - v0) * k / static_cast<double>(div);
        const auto p = surf.TryGetPointAt(uc, v);
        ASSERT_TRUE(p.has_value());
        EXPECT_LT((loop[k] - *p).norm(), kPosTol);
    }
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

// 多角形外周: 等間隔グリッド外の角点も評価点に含め、生成ループが各角を通過する
TEST(ComputeRestrictedSurfaceEdges, PolygonOuter_LoopPassesThroughCorners) {
    auto plane = MakePlane();
    // 既定divisions=64のグリッドに角が乗らない非対称三角形を外周にする
    auto outer = i_ent::MakeLinearPath(
        std::vector<Vector2d>{{0.1, 0.1}, {0.8, 0.2}, {0.55, 0.9}}, true);
    auto ts = BuildTrimmed(plane, outer);

    const auto edges = i_ent::ComputeRestrictedSurfaceEdges(*ts);
    ASSERT_EQ(edges.loops.size(), 1u);
    const auto& loop = edges.loops.front();

    // 境界曲線の各角点tcについて、S(B(tc))がループ頂点に厳密に存在する
    auto base = ts->GetBaseSurface();
    ASSERT_NE(base, nullptr);
    const auto corners = outer->GetCornerParams();
    ASSERT_FALSE(corners.empty());
    for (const double tc : corners) {
        const auto uv = outer->TryGetPointAt(tc);
        ASSERT_TRUE(uv.has_value());
        const auto expected = base->TryGetPointAt(uv->x(), uv->y());
        ASSERT_TRUE(expected.has_value());
        const bool found = std::any_of(
            loop.begin(), loop.end(),
            [&](const Vector3d& q) { return (q - *expected).norm() < kPosTol; });
        EXPECT_TRUE(found) << "corner at t=" << tc << " is not on the loop";
    }
}

TEST(ComputeRestrictedSurfaceEdges, ClosedBaseUntrimmed_SkipsSeam) {
    auto closed = MakeClosedBaseSurface();
    auto ts = BuildTrimmed(closed, nullptr);  // N1=0, 穴なし

    const auto edges = i_ent::ComputeRestrictedSurfaceEdges(*ts);

    // 閉基底の未トリム外周は継ぎ目を除外し、開方向の2辺のみ
    EXPECT_EQ(edges.loops.size(), 2u);
}



/**
 * ComputeSurfaceCreaseEdges: 折り目(角)の内部稜線
 */

// 滑らかな曲面 (折り目なし) ではループを生成しない
TEST(ComputeSurfaceCreaseEdges, SmoothSurface_ProducesNoLoops) {
    auto plane = MakePlane();
    EXPECT_TRUE(i_ent::ComputeSurfaceCreaseEdges(*plane).loops.empty());
}

// 段付き回転面: 内部の折り目2本ぶんのループを生成する (各 divisions+1 点)
TEST(ComputeSurfaceCreaseEdges, SteppedRevolution_ProducesOneLoopPerCorner) {
    auto surf = MakeSteppedRevolution();

    const auto edges = i_ent::ComputeSurfaceCreaseEdges(*surf);

    EXPECT_EQ(edges.loops.size(), 2u);
    for (const auto& loop : edges.loops) {
        EXPECT_EQ(loop.size(), 65u);  // 全周のため分割なし (既定 divisions=64)
    }
}

// 生成したループは折り目位置のv方向アイソ曲線 S(u_c, v) に一致する
TEST(ComputeSurfaceCreaseEdges, LoopsLieOnVIsoCurves) {
    auto surf = MakeSteppedRevolution();
    const auto vrange = surf->GetParameterRange();  // {u0,u1,v0,v1}
    const auto creases = surf->GetUCreaseParameters();

    const auto edges = i_ent::ComputeSurfaceCreaseEdges(*surf);

    // ループはGetUCreaseParameters()の順 (弧長2.0, 3.0) で並ぶ
    ASSERT_EQ(edges.loops.size(), creases.size());
    for (std::size_t i = 0; i < creases.size(); ++i) {
        AssertLoopIsVIsoCurve(*surf, creases[i], vrange[2], vrange[3],
                              edges.loops[i]);
    }
}

// L字準線の平行曲面: 内部の折り目1本ぶんのループを生成する
TEST(ComputeSurfaceCreaseEdges, LShapedExtrusion_ProducesOneLoop) {
    auto surf = MakeLShapedExtrusion();
    EXPECT_EQ(i_ent::ComputeSurfaceCreaseEdges(*surf).loops.size(), 1u);
}

// 分割数がサンプル点数に反映される
TEST(ComputeSurfaceCreaseEdges, Divisions_ControlsSampleCount) {
    auto surf = MakeSteppedRevolution();

    const auto edges = i_ent::ComputeSurfaceCreaseEdges(*surf, {8, false});

    ASSERT_FALSE(edges.loops.empty());
    for (const auto& loop : edges.loops) EXPECT_EQ(loop.size(), 9u);
}
