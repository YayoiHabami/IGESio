/**
 * @file tests/entities/surfaces/test_restricted_surface_mesh.cpp
 * @brief TessellateRestrictedSurface (entities/surfaces/algorithms) のテスト
 * @author Yayoi Habami
 * @date 2026-06-03
 * @copyright 2026 Yayoi Habami
 * @note 対象は公開関数 entities::TessellateRestrictedSurface(const IRestrictedSurface&,
 *       params)。具象 TrimmedSurface を IRestrictedSurface として与えて検証する。
 *       検証する不変量:
 *       - 出力構造 (8行・インデックスは3の倍数・範囲内)
 *       - 制約遵守 (凸な外周トリムでは全三角形の重心が領域内 /
 *         凸な穴では被覆面積が「定義域−穴」に一致 = 穴を覆わない)
 *       - クラックフリー (辺は高々2回使用 / メッシュ境界辺が閉ループ)
 *       - 回帰ゼロ (未トリムは一様基底グリッドと一致)
 *       - 細帯の救済 (細分の有無で被覆面積が対比)
 *       - パラメータ (max_depth=0 / 不正値クランプ)
 *       - 退化 (空ドメイン / 閉曲面基底) ・平面健全性・決定性
 *
 * TODO: 境界曲線が未解決の TrimmedSurface (GetParameterRange が投げる) は本関数の
 *       対象外 (呼び出し側責務) のため未カバー。
 * TODO: 境界ちょうど上の頂点の内外判定は近似依存のため、内側に十分離れる
 *       三角形重心ベースで代替する (頂点単体の内外は未カバー)。
 * TODO: 凸な穴境界では折れ線近似の弦が穴へ微小にはみ出し、境界三角形の重心が
 *       穴内に入りうる (線形テッセレーションの正常な離散化)。そのため穴の検証は
 *       per-triangleではなく大域被覆面積で行う。
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/entities/curves/circular_arc.h"
#include "igesio/entities/curves/linear_path.h"
#include "igesio/entities/curves/curve_on_a_parametric_surface.h"
#include "igesio/entities/surfaces/trimmed_surface.h"
#include "igesio/entities/surfaces/algorithms/restricted_surface_mesh.h"
#include "./surfaces_for_testing.h"

namespace {

namespace i_ent = igesio::entities;
namespace i_test = igesio::tests;
using igesio::Vector2d;
using igesio::Vector3d;
using i_ent::TrimmedSurface;
using i_ent::CurveOnAParametricSurface;
using i_ent::RestrictedSurfaceMesh;
using i_ent::RestrictedSurfaceMeshParams;

/// @brief 位置・法線の許容誤差 (float精度)
constexpr float kGeomTol = 1e-3f;
/// @brief UV面積比較の許容誤差
constexpr double kAreaTol = 1e-9;



/**
 * テスト対象を構築するファクトリ
 */

/// @brief 基底NURBS平面 S(u,v)=(-5+10u, 5, 5-10v), D=[0,1]² を作成する
std::shared_ptr<i_ent::ISurface> MakePlane() {
    return i_test::CreateRationalBSplineSurfaces()[0].surface;
}

/// @brief V方向に閉じた全回転面 (0〜2π) を作成する
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
 * メッシュ検査ユーティリティ
 */

/// @brief 頂点列のtu,tvから実パラメータ (u,v) を復元する
std::pair<double, double> VertexUV(
        const RestrictedSurfaceMesh& mesh, const std::uint32_t col,
        const std::array<double, 4>& range) {
    const double tu = mesh.vertices(6, col);
    const double tv = mesh.vertices(7, col);
    return {range[0] + tu * (range[1] - range[0]),
            range[2] + tv * (range[3] - range[2])};
}

/// @brief メッシュ全三角形のUV面積の総和を返す
double MeshUVArea(const RestrictedSurfaceMesh& mesh,
                  const std::array<double, 4>& range) {
    double area = 0.0;
    for (size_t k = 0; k + 3 <= mesh.indices.size(); k += 3) {
        const auto [u0, v0] = VertexUV(mesh, mesh.indices[k], range);
        const auto [u1, v1] = VertexUV(mesh, mesh.indices[k + 1], range);
        const auto [u2, v2] = VertexUV(mesh, mesh.indices[k + 2], range);
        const double cross = (u1 - u0) * (v2 - v0) - (u2 - u0) * (v1 - v0);
        area += std::abs(cross) * 0.5;
    }
    return area;
}

/// @brief 無向辺 (頂点インデックス対) ごとの使用三角形数を集計する
std::map<std::pair<std::uint32_t, std::uint32_t>, int> EdgeUseCount(
        const RestrictedSurfaceMesh& mesh) {
    std::map<std::pair<std::uint32_t, std::uint32_t>, int> count;
    const auto add = [&](const std::uint32_t a, const std::uint32_t b) {
        count[{std::min(a, b), std::max(a, b)}]++;
    };
    for (size_t k = 0; k + 3 <= mesh.indices.size(); k += 3) {
        const std::uint32_t a = mesh.indices[k];
        const std::uint32_t b = mesh.indices[k + 1];
        const std::uint32_t c = mesh.indices[k + 2];
        add(a, b); add(b, c); add(c, a);
    }
    return count;
}

/// @brief 出力構造の基本的妥当性を検査する
/// @note 頂点は8行、インデックスは3の倍数、全インデックスは列数未満
void AssertValidMeshShape(const RestrictedSurfaceMesh& mesh) {
    EXPECT_EQ(mesh.vertices.rows(), 8);
    EXPECT_EQ(mesh.indices.size() % 3, 0u);
    const auto cols = static_cast<std::uint32_t>(mesh.vertices.cols());
    for (const auto idx : mesh.indices) {
        EXPECT_LT(idx, cols);
    }
}

}  // namespace



/**
 * 出力構造の妥当性
 */

TEST(TessellateRestrictedSurface, Structure_ValidIndicesAndShape) {
    auto plane = MakePlane();
    auto ts = BuildTrimmed(plane, MakeUvRectLoop(0.1, 0.1, 0.9, 0.9),
                           {MakeUvCircle({0.5, 0.5}, 0.15)});

    const auto mesh = i_ent::TessellateRestrictedSurface(*ts);

    AssertValidMeshShape(mesh);
    EXPECT_FALSE(mesh.indices.empty());
}



/**
 * 制約遵守 (穴・外側へのはみ出しなし)
 */

TEST(TessellateRestrictedSurface, RectOuter_AllCentroidsInsideDomain) {
    auto plane = MakePlane();
    auto ts = BuildTrimmed(plane, MakeUvRectLoop(0.1, 0.1, 0.9, 0.9));
    const auto range = ts->GetParameterRange();

    const auto mesh = i_ent::TessellateRestrictedSurface(*ts);

    ASSERT_FALSE(mesh.indices.empty());
    for (size_t k = 0; k + 3 <= mesh.indices.size(); k += 3) {
        const auto [u0, v0] = VertexUV(mesh, mesh.indices[k], range);
        const auto [u1, v1] = VertexUV(mesh, mesh.indices[k + 1], range);
        const auto [u2, v2] = VertexUV(mesh, mesh.indices[k + 2], range);
        const double uc = (u0 + u1 + u2) / 3.0;
        const double vc = (v0 + v1 + v2) / 3.0;
        EXPECT_TRUE(ts->IsInDomain(uc, vc))
                << "centroid (" << uc << ", " << vc << ") outside domain";
    }
}

TEST(TessellateRestrictedSurface, CircleHole_MeshedAreaExcludesHole) {
    auto plane = MakePlane();
    constexpr double kRadius = 0.2;
    // N1=0 (外周はパラメータ矩形) + 中央の円穴
    auto ts = BuildTrimmed(plane, nullptr, {MakeUvCircle({0.5, 0.5}, kRadius)});
    const auto range = ts->GetParameterRange();

    const auto mesh = i_ent::TessellateRestrictedSurface(*ts);

    // 被覆面積 ≈ 定義域(1.0) - 穴(π r²)。穴が覆われていれば 1.0 に近づくため、
    // 大域面積で「穴が除外されている」ことを検証する。
    // (凸な穴境界の折れ線近似による微小な弦のはみ出しは大域面積には影響しない)
    ASSERT_FALSE(mesh.indices.empty());
    const double pi = std::acos(-1.0);
    const double expected = 1.0 - pi * kRadius * kRadius;
    EXPECT_NEAR(MeshUVArea(mesh, range), expected, 0.03);
}



/**
 * クラックフリー / 整合性
 */

TEST(TessellateRestrictedSurface, Conforming_NoNonManifoldEdges) {
    auto plane = MakePlane();
    // 外周+オフセンターの穴でルート深さに差を作り、ハンギングノードを誘発する
    auto ts = BuildTrimmed(plane, MakeUvRectLoop(0.1, 0.1, 0.9, 0.9),
                           {MakeUvCircle({0.35, 0.6}, 0.12)});

    const auto mesh = i_ent::TessellateRestrictedSurface(*ts);
    const auto edges = EdgeUseCount(mesh);

    ASSERT_FALSE(edges.empty());
    for (const auto& [edge, used] : edges) {
        EXPECT_LE(used, 2) << "non-manifold edge used " << used << " times";
    }
}

TEST(TessellateRestrictedSurface, Conforming_BoundaryEdgesFormClosedLoops) {
    auto plane = MakePlane();
    auto ts = BuildTrimmed(plane, MakeUvRectLoop(0.1, 0.1, 0.9, 0.9),
                           {MakeUvCircle({0.35, 0.6}, 0.12)});

    const auto mesh = i_ent::TessellateRestrictedSurface(*ts);
    const auto edges = EdgeUseCount(mesh);

    // 1回しか使われない辺 (=メッシュ境界) における頂点接続数を集計する
    std::map<std::uint32_t, int> incidence;
    for (const auto& [edge, used] : edges) {
        if (used == 1) {
            incidence[edge.first]++;
            incidence[edge.second]++;
        }
    }
    // クラック(T字接合)が無ければ境界辺は閉ループとなり、各頂点の接続数は偶数
    ASSERT_FALSE(incidence.empty());
    for (const auto& [vertex, deg] : incidence) {
        EXPECT_EQ(deg % 2, 0)
                << "vertex " << vertex << " has odd boundary degree (crack)";
    }
}



/**
 * 回帰ゼロ (未トリムは一様基底グリッド)
 */

TEST(TessellateRestrictedSurface, Untrimmed_EqualsUniformBaseGrid) {
    auto plane = MakePlane();
    auto ts = BuildTrimmed(plane, nullptr);  // N1=0, 穴なし → 未トリム
    const RestrictedSurfaceMeshParams params{/*base_div=*/8, /*max_depth=*/2};

    const auto mesh = i_ent::TessellateRestrictedSurface(*ts, params);

    // 細分は一切起きず、(8+1)² 頂点・8²×2 三角形の一様グリッドとなる
    EXPECT_EQ(mesh.vertices.cols(), 81);
    EXPECT_EQ(mesh.indices.size(), static_cast<size_t>(8 * 8 * 2 * 3));
}



/**
 * 細い形状の救済 (細分の有無で対比)
 */

TEST(TessellateRestrictedSurface, ThinBand_RecoveredByRefinement) {
    auto plane = MakePlane();
    // グリッドセル(1/32≈0.031)より細く、格子点を一切含まない帯
    auto ts = BuildTrimmed(plane, MakeUvRectLoop(0.02, 0.505, 0.98, 0.520));
    const auto range = ts->GetParameterRange();
    const double band_area = (0.98 - 0.02) * (0.520 - 0.505);

    const auto mesh_flat = i_ent::TessellateRestrictedSurface(
            *ts, {/*base_div=*/32, /*max_depth=*/0});
    const auto mesh_ref = i_ent::TessellateRestrictedSurface(
            *ts, {/*base_div=*/32, /*max_depth=*/2});

    // 細分なし: 帯が格子の隙間を抜けて脱落する (被覆面積ほぼゼロ)
    EXPECT_LT(MeshUVArea(mesh_flat, range), 0.2 * band_area);
    // 細分あり: 帯が救済され、被覆面積が帯面積に近づく
    const double area_ref = MeshUVArea(mesh_ref, range);
    EXPECT_GT(area_ref, 0.5 * band_area);
    EXPECT_LT(area_ref, 1.5 * band_area);
}



/**
 * パラメータ
 */

TEST(TessellateRestrictedSurface, Params_MaxDepthZero_ProducesValidMesh) {
    auto plane = MakePlane();
    auto ts = BuildTrimmed(plane, MakeUvRectLoop(0.1, 0.1, 0.9, 0.9));

    const auto mesh = i_ent::TessellateRestrictedSurface(
            *ts, {/*base_div=*/16, /*max_depth=*/0});

    AssertValidMeshShape(mesh);
    EXPECT_FALSE(mesh.indices.empty());
}

TEST(TessellateRestrictedSurface, Params_ClampsNonPositiveArgs) {
    auto plane = MakePlane();
    auto ts = BuildTrimmed(plane, nullptr);  // 未トリム

    // base_div=0 → 1, max_depth=-1 → 0 にクランプされ、1セル(2三角形)になる
    RestrictedSurfaceMesh mesh;
    ASSERT_NO_THROW(mesh = i_ent::TessellateRestrictedSurface(
            *ts, {/*base_div=*/0, /*max_depth=*/-1}));
    AssertValidMeshShape(mesh);
    EXPECT_EQ(mesh.vertices.cols(), 4);
    EXPECT_EQ(mesh.indices.size(), 6u);
}



/**
 * 退化
 */

TEST(TessellateRestrictedSurface, EmptyDomain_ProducesEmptyMesh) {
    auto plane = MakePlane();
    // 外側境界より大きい穴を意図的に与え、有効領域を空にする退化構成。
    // (両ループとも定義域 [0,1]² 内に収まり、C(t)の自動生成は成立する)
    auto ts = BuildTrimmed(plane, MakeUvRectLoop(0.35, 0.35, 0.65, 0.65),
                           {MakeUvRectLoop(0.3, 0.3, 0.7, 0.7)});

    const auto mesh = i_ent::TessellateRestrictedSurface(*ts);

    EXPECT_TRUE(mesh.indices.empty());
}

TEST(TessellateRestrictedSurface, ClosedBaseSurface_Untrimmed_ProducesValidMesh) {
    auto closed = MakeClosedBaseSurface();  // V方向に閉じた回転面
    auto ts = BuildTrimmed(closed, nullptr);

    const auto mesh = i_ent::TessellateRestrictedSurface(*ts);

    AssertValidMeshShape(mesh);
    EXPECT_FALSE(mesh.indices.empty());
}



/**
 * 幾何の健全性 (平面基底) ・決定性
 */

TEST(TessellateRestrictedSurface, PlanarBase_VerticesOnPlaneWithConsistentNormals) {
    auto plane = MakePlane();  // S(u,v)=(-5+10u, 5, 5-10v), 法線 (0,1,0)
    auto ts = BuildTrimmed(plane, MakeUvRectLoop(0.1, 0.1, 0.9, 0.9));

    const auto mesh = i_ent::TessellateRestrictedSurface(*ts);

    ASSERT_GT(mesh.vertices.cols(), 0);
    for (int c = 0; c < mesh.vertices.cols(); ++c) {
        // 全頂点が平面 y=5 上にある
        EXPECT_NEAR(mesh.vertices(1, c), 5.0f, kGeomTol);
        // 法線が平面法線 (0,±1,0) に平行
        const float nx = mesh.vertices(3, c);
        const float ny = mesh.vertices(4, c);
        const float nz = mesh.vertices(5, c);
        EXPECT_NEAR(std::abs(ny), 1.0f, kGeomTol);
        EXPECT_NEAR(nx, 0.0f, kGeomTol);
        EXPECT_NEAR(nz, 0.0f, kGeomTol);
    }
}

TEST(TessellateRestrictedSurface, Deterministic_SameInputProducesSameMesh) {
    auto plane = MakePlane();
    auto ts = BuildTrimmed(plane, MakeUvRectLoop(0.1, 0.1, 0.9, 0.9),
                           {MakeUvCircle({0.5, 0.5}, 0.15)});

    const auto mesh1 = i_ent::TessellateRestrictedSurface(*ts);
    const auto mesh2 = i_ent::TessellateRestrictedSurface(*ts);

    EXPECT_EQ(mesh1.vertices.cols(), mesh2.vertices.cols());
    EXPECT_EQ(mesh1.indices, mesh2.indices);
    if (mesh1.vertices.cols() == mesh2.vertices.cols() &&
        mesh1.vertices.cols() > 0) {
        EXPECT_NEAR((mesh1.vertices - mesh2.vertices).cwiseAbs().maxCoeff(),
                    0.0f, kAreaTol);
    }
}
