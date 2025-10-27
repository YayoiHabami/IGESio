/**
 * @file entities/test_rational_b_spline_surface.cpp
 * @brief Rational B-Spline Surfaceエンティティのテスト
 * @author Yayoi Habami
 * @date 2025-09-26
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/numerics/tolerance.h"
#include "igesio/entities/surfaces/rational_b_spline_surface.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using igesio::Vector2d;
using igesio::Vector3d;
using NSurface = igesio::entities::RationalBSplineSurface;

/// @brief 平面のNURBS曲面を生成する
/// @return Y=25上の平面
std::shared_ptr<NSurface> CreatePlane() {
    auto param = igesio::IGESParameterVector{
        1, 1,  // K1, K2 (Number of control points - 1 in U and V)
        1, 1,  // M1, M2 (Degree in U and V)
        true, true, false, true, true,  // PROP1-5
        0., 0., 1., 1.,   // Knot vector in U
        0., 0., 1., 1.,   // Knot vector in V
        1., 1., 1., 1.,   // Weights
        -25., 25., 25.,   // Control point (0,0)
        -25., 25., -25.,  // Control point (0,1)
        25., 25., 25.,    // Control point (1,0)
        25., 25., -25.,   // Control point (1,1)
        0., 1., 0., 1.    // Parameter range in U and V
    };
    auto nurbs_s = std::make_shared<i_ent::RationalBSplineSurface>(param);

    return nurbs_s;
}

}  // namespace



/**
 * ISurface関連実装のテスト
 */

// TryGetDefinedPointAtのテスト
// - 平面エンティティについて、全ての点がY=25であることを確認する
TEST(RationalBSplineSurface, TryGetDefinedPointAt) {
    auto plane = CreatePlane();
    auto result = plane->Validate();
    ASSERT_TRUE(result.is_valid) << result.Message();

    // パラメータ範囲内の点を評価
    for (double u = 0.0; u <= 1.0; u += 0.1) {
        for (double v = 0.0; v <= 1.0; v += 0.1) {
            auto pt_opt = plane->TryGetDefinedPointAt(u, v);
            ASSERT_TRUE(pt_opt.has_value());
            EXPECT_TRUE(i_num::IsApproxEqual((*pt_opt).y(), 25.0));
        }
    }
}

// TryGetDefinedNormalAtのテスト
// - 平面エンティティについて、全ての法線ベクトルが (0,1,0)であることを確認する
TEST(RationalBSplineSurface, TryGetDefinedNormalAt) {
    auto plane = CreatePlane();
    auto result = plane->Validate();
    ASSERT_TRUE(result.is_valid) << result.Message();

    // パラメータ範囲内の点を評価
    for (double u = 0.0; u <= 1.0; u += 0.1) {
        for (double v = 0.0; v <= 1.0; v += 0.1) {
            auto n_opt = plane->TryGetDefinedNormalAt(u, v);
            ASSERT_TRUE(n_opt.has_value());
            EXPECT_TRUE(i_num::IsApproxEqual((*n_opt).x(), 0.0));
            EXPECT_TRUE(i_num::IsApproxEqual((*n_opt).y(), 1.0));
            EXPECT_TRUE(i_num::IsApproxEqual((*n_opt).z(), 0.0));
        }
    }
}
