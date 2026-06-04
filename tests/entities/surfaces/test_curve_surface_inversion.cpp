/**
 * @file entities/surfaces/test_curve_surface_inversion.cpp
 * @brief curve_surface_inversion.h のテスト
 * @author Yayoi Habami
 * @date 2026-05-31
 * @copyright 2026 Yayoi Habami
 *
 * ### 対象関数
 * - igesio::entities::InvertPointOntoSurface
 * - igesio::entities::InvertCurveOntoSurface
 *
 * TODO: seam/pole による弧分割 (split_at_discontinuities=true) の検証.
 *       seam を跨ぐ「曲面上の曲線」フィクスチャの構築が複雑なため、
 *       Type 141/143 (Boundary / Bounded Surface) 実装時に併せて整備する.
 * TODO: TrimmedSurface の穴領域 (TryGetDerivatives が nullopt) の検証.
 */
#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/numerics/core/tolerance.h"
#include "igesio/entities/curves/line.h"
#include "igesio/entities/surfaces/rational_b_spline_surface.h"
#include "igesio/entities/surfaces/surface_of_revolution.h"
#include "igesio/entities/surfaces/algorithms/curve_surface_inversion.h"

namespace {

namespace i_ent = igesio::entities;
using i_ent::InvertPointOntoSurface;
using i_ent::InvertCurveOntoSurface;
using igesio::Vector3d;

/// @brief 位置一致の許容誤差 (受理許容より十分小さい)
constexpr double kUVTol = 1e-5;

/// @brief y=5 の平面 (RationalBSplineSurface, u/v ∈ [0,1], x/z ∈ [-5,5])
/// @note 変換なし (定義空間 = ワールド空間). 矩形パッチのため (u,v)→(x,z) は線形
std::shared_ptr<i_ent::RationalBSplineSurface> MakeYFivePlane() {
    igesio::IGESParameterVector param{
        1, 1,
        1, 1,
        false, false, true, false, false,
        0., 0., 1., 1.,
        0., 0., 1., 1.,
        1., 1., 1., 1.,
        -5., 5.,  5.,
        -5., 5., -5.,
         5., 5.,  5.,
         5., 5., -5.,
        0., 1., 0., 1.
    };
    return std::make_shared<i_ent::RationalBSplineSurface>(param);
}

/// @brief z軸を回転軸とする半径2・高さ4の円柱面 (SurfaceOfRevolution)
/// @note S(u,v) = (2*cos(v), 2*sin(v), 4*u), u∈[0,1], v∈[0,2π]
std::shared_ptr<i_ent::SurfaceOfRevolution> MakeCylinder() {
    auto axis = std::make_shared<i_ent::Line>(
        Vector3d{0., 0., 0.}, Vector3d{0., 0., 1.});
    auto generatrix = std::make_shared<i_ent::Line>(
        Vector3d{2., 0., 0.}, Vector3d{2., 0., 4.});
    return std::make_shared<i_ent::SurfaceOfRevolution>(
        axis, generatrix, 0.0, 2.0 * igesio::kPi);
}

}  // namespace



/// @brief 平面上の既知点を逆射影し、元の (u,v) を復元できる (代表値)
TEST(CurveSurfaceInversion, InvertPoint_RecoversKnownUV_OnPlane) {
    auto surface = MakeYFivePlane();
    const std::array<std::array<double, 2>, 3> uvs = {{
        {0.2, 0.3}, {0.5, 0.5}, {0.85, 0.1}}};
    for (const auto& q : uvs) {
        const Vector3d p = surface->GetPointAt(q[0], q[1]);
        const auto sol = InvertPointOntoSurface(*surface, p, {0.5, 0.5});
        ASSERT_TRUE(sol.has_value());
        EXPECT_NEAR((*sol)[0], q[0], kUVTol);
        EXPECT_NEAR((*sol)[1], q[1], kUVTol);
    }
}

/// @brief 平面上の境界値 (u,v) = (0,1) を逆射影で復元できる
TEST(CurveSurfaceInversion, InvertPoint_RecoversBoundaryUV_OnPlane) {
    auto surface = MakeYFivePlane();
    const Vector3d p = surface->GetPointAt(0.0, 1.0);
    const auto sol = InvertPointOntoSurface(*surface, p, {0.5, 0.5});
    ASSERT_TRUE(sol.has_value());
    EXPECT_NEAR((*sol)[0], 0.0, kUVTol);
    EXPECT_NEAR((*sol)[1], 1.0, kUVTol);
}

/// @brief 曲面 (円柱) 上の既知点を逆射影し、元の (u,v) を復元できる
TEST(CurveSurfaceInversion, InvertPoint_RecoversKnownUV_OnCylinder) {
    auto surface = MakeCylinder();
    const Vector3d p = surface->GetPointAt(0.5, 1.0);
    // 真値近傍を初期値に与える (周期方向の別解を避ける)
    const auto sol = InvertPointOntoSurface(*surface, p, {0.4, 0.9});
    ASSERT_TRUE(sol.has_value());
    EXPECT_NEAR((*sol)[0], 0.5, kUVTol);
    EXPECT_NEAR((*sol)[1], 1.0, kUVTol);
}

/// @brief 曲面から大きく離れた点は逆射影に失敗する (nullopt)
TEST(CurveSurfaceInversion, InvertPoint_FailsWhenPointFarFromSurface) {
    auto surface = MakeYFivePlane();
    // y=5 平面から大きく離れた点 (残差が受理許容を超える)
    const auto sol = InvertPointOntoSurface(
        *surface, Vector3d(0.0, 100.0, 0.0), {0.5, 0.5});
    EXPECT_FALSE(sol.has_value());
}

/// @brief 平面上のモデル空間直線を逆射影し、端点の (u,v) を復元できる
TEST(CurveSurfaceInversion, InvertCurve_RecoversEndpointUV_OnPlane) {
    auto surface = MakeYFivePlane();
    const Vector3d p0 = surface->GetPointAt(0.2, 0.3);
    const Vector3d p1 = surface->GetPointAt(0.7, 0.6);
    const auto curve = std::make_shared<i_ent::Line>(p0, p1);

    const auto arcs = InvertCurveOntoSurface(*surface, *curve);
    // split 既定 off のため弧は1本
    ASSERT_EQ(arcs.size(), 1u);
    const auto& uv = arcs.front().uv_points;
    ASSERT_GE(uv.size(), 2u);
    EXPECT_NEAR(uv.front().x(), 0.2, kUVTol);
    EXPECT_NEAR(uv.front().y(), 0.3, kUVTol);
    EXPECT_NEAR(uv.back().x(), 0.7, kUVTol);
    EXPECT_NEAR(uv.back().y(), 0.6, kUVTol);
    // パラメータ空間サンプルは z=0 平面上にある
    for (const auto& q : uv) EXPECT_NEAR(q.z(), 0.0, 1e-12);
}
