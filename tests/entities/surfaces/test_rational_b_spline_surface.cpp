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
#include "igesio/numerics/core/tolerance.h"
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
        // 制御点はIGES規格どおり第1添字i (u方向) が最速で並べる
        // (P(0,0), P(1,0), P(0,1), P(1,1))
        -25., 25., 25.,   // Control point (0,0)
        25., 25., 25.,    // Control point (1,0)
        -25., 25., -25.,  // Control point (0,1)
        25., 25., -25.,   // Control point (1,1)
        0., 1., 0., 1.    // Parameter range in U and V
    };
    auto nurbs_s = std::make_shared<i_ent::RationalBSplineSurface>(param);

    return nurbs_s;
}

/// @brief 非対称な双線形パッチ (u方向とv方向で別の向きに伸びる) を生成する
/// @return 4隅が P(0,0)=(0,0,0), P(1,0)=(10,0,0), P(0,1)=(0,20,0),
///         P(1,1)=(10,20,0) の平面パッチ. u方向はx軸、v方向はy軸に対応する.
/// @note 制御点はIGES規格どおり第1添字i (u方向) が最速で並べる.
///       u/vを取り違えるとS(1,0)とS(0,1)が入れ替わるため、転置バグの検出に使う.
std::shared_ptr<NSurface> CreateAsymmetricBilinearPatch() {
    auto param = igesio::IGESParameterVector{
        1, 1,  // K1, K2 (Number of control points - 1 in U and V)
        1, 1,  // M1, M2 (Degree in U and V)
        false, false, true, false, false,  // PROP1-5
        0., 0., 1., 1.,   // Knot vector in U
        0., 0., 1., 1.,   // Knot vector in V
        1., 1., 1., 1.,   // Weights
        0.,  0.,  0.,     // Control point (0,0)
        10., 0.,  0.,     // Control point (1,0)
        0.,  20., 0.,     // Control point (0,1)
        10., 20., 0.,     // Control point (1,1)
        0., 1., 0., 1.    // Parameter range in U and V
    };
    return std::make_shared<i_ent::RationalBSplineSurface>(param);
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

// TryGetDefinedPointAtのテスト (u/v方向の取り違え検出)
// - 非対称な双線形パッチについて、S(1,0)がu方向の制御点P(1,0)=(10,0,0)に、
//   S(0,1)がv方向の制御点P(0,1)=(0,20,0)に一致することを確認する.
//   制御点をv最速で読む転置バグがあると両者が入れ替わって失敗する.
TEST(RationalBSplineSurface, TryGetDefinedPointAt_RespectsUVOrdering) {
    auto patch = CreateAsymmetricBilinearPatch();
    auto result = patch->Validate();
    ASSERT_TRUE(result.is_valid) << result.Message();

    auto p10 = patch->TryGetDefinedPointAt(1.0, 0.0);  // S(U(1), V(0)) = P(1,0)
    auto p01 = patch->TryGetDefinedPointAt(0.0, 1.0);  // S(U(0), V(1)) = P(0,1)
    ASSERT_TRUE(p10.has_value());
    ASSERT_TRUE(p01.has_value());

    // S(1,0) は u方向の制御点 P(1,0) = (10, 0, 0)
    EXPECT_TRUE(i_num::IsApproxEqual(p10->x(), 10.0)) << "S(1,0) = " << p10->transpose();
    EXPECT_TRUE(i_num::IsApproxEqual(p10->y(), 0.0)) << "S(1,0) = " << p10->transpose();
    EXPECT_TRUE(i_num::IsApproxEqual(p10->z(), 0.0)) << "S(1,0) = " << p10->transpose();
    // S(0,1) は v方向の制御点 P(0,1) = (0, 20, 0)
    EXPECT_TRUE(i_num::IsApproxEqual(p01->x(), 0.0)) << "S(0,1) = " << p01->transpose();
    EXPECT_TRUE(i_num::IsApproxEqual(p01->y(), 20.0)) << "S(0,1) = " << p01->transpose();
    EXPECT_TRUE(i_num::IsApproxEqual(p01->z(), 0.0)) << "S(0,1) = " << p01->transpose();
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

// P (曲面): U(0)がノット域S(0)=0を超えて外れる (CATIA等) → 評価側 (clamp済み基底)
// が描画可能なため、ValidatePDは警告 (kWarning) を出すがis_valid=true (描画ブロックしない)。
TEST(RationalBSplineSurface, KnotRangeOutside_IsValidWithWarning) {
    auto param = igesio::IGESParameterVector{
        1, 1, 1, 1,                       // K1, K2, M1, M2
        true, true, false, true, true,    // PROP1-5
        0., 0., 1., 1.,                   // u knots → S(0)=0, S(N1)=1
        0., 0., 1., 1.,                   // v knots
        1., 1., 1., 1.,                   // weights
        -25., 25., 25.,                   // P(0,0)
        25., 25., 25.,                    // P(1,0)
        -25., 25., -25.,                  // P(0,1)
        25., 25., -25.,                   // P(1,1)
        -0.01, 1., 0., 1.                 // U(0)=-0.01 (ノット域S(0)=0を超過)
    };
    const i_ent::RationalBSplineSurface surf(param);
    const auto result = surf.ValidatePD();
    EXPECT_TRUE(result.is_valid);  // 描画はブロックしない
    bool has_warning = false;
    for (const auto& e : result.errors) {
        if (e.severity == igesio::ValidationSeverity::kWarning) has_warning = true;
    }
    EXPECT_TRUE(has_warning);
}

namespace {

/// @brief ValidatePDの結果に警告 (kWarning) が含まれるかを判定する
bool HasWarning(const NSurface& surf) {
    for (const auto& e : surf.ValidatePD().errors) {
        if (e.severity == igesio::ValidationSeverity::kWarning) return true;
    }
    return false;
}

}  // namespace

// 回帰: U(1)の上限はS(N1) (= u_knots[size-M1-1]) と比較される。
// 以前はS(N1+1)を参照しており (off-by-one)、S(N1) < U(1) <= S(N1+1)
// の範囲外れが警告されなかった。
TEST(RationalBSplineSurface, KnotRangeUpperBound_WarnsWhenUEndExceedsSN1) {
    // M1=1, K1=2: u_knots=[0,0,1,2,3] → S(0)=0, S(N1)=2, S(N1+1)=3
    const auto build = [](const double u_end) {
        return igesio::IGESParameterVector{
            2, 1, 1, 1,                        // K1, K2, M1, M2
            false, false, true, false, false,  // PROP1-5
            0., 0., 1., 2., 3.,                // u knots
            0., 0., 1., 1.,                    // v knots
            1., 1., 1., 1., 1., 1.,            // weights
            0., 0., 0.,  1., 0., 0.,  2., 0., 0.,  // P(0,0)〜P(2,0)
            0., 1., 0.,  1., 1., 0.,  2., 1., 0.,  // P(0,1)〜P(2,1)
            0., u_end, 0., 1.                  // U=[0,u_end], V=[0,1]
        };
    };

    // U(1)=2.5はS(N1)=2を超えるため警告 (修正前はS(N1+1)=3と比較し素通り)
    const NSurface outside(build(2.5));
    EXPECT_TRUE(outside.ValidatePD().is_valid);
    EXPECT_TRUE(HasWarning(outside));

    // 境界ちょうど (U(1)=S(N1)=2) は警告なし
    const NSurface boundary(build(2.0));
    EXPECT_TRUE(boundary.ValidatePD().is_valid);
    EXPECT_FALSE(HasWarning(boundary));
}

// 回帰: V(1)の上限もT(N2) (= v_knots[size-M2-1]) と比較される (u側と同様)
TEST(RationalBSplineSurface, KnotRangeUpperBound_WarnsWhenVEndExceedsTN2) {
    // M2=1, K2=2: v_knots=[0,0,1,2,3] → T(0)=0, T(N2)=2, T(N2+1)=3
    const auto build = [](const double v_end) {
        return igesio::IGESParameterVector{
            1, 2, 1, 1,                        // K1, K2, M1, M2
            false, false, true, false, false,  // PROP1-5
            0., 0., 1., 1.,                    // u knots
            0., 0., 1., 2., 3.,                // v knots
            1., 1., 1., 1., 1., 1.,            // weights
            0., 0., 0.,  1., 0., 0.,           // P(0,0), P(1,0)
            0., 1., 0.,  1., 1., 0.,           // P(0,1), P(1,1)
            0., 2., 0.,  1., 2., 0.,           // P(0,2), P(1,2)
            0., 1., 0., v_end                  // U=[0,1], V=[0,v_end]
        };
    };

    // V(1)=2.5はT(N2)=2を超えるため警告
    const NSurface outside(build(2.5));
    EXPECT_TRUE(outside.ValidatePD().is_valid);
    EXPECT_TRUE(HasWarning(outside));

    // 境界ちょうど (V(1)=T(N2)=2) は警告なし
    const NSurface boundary(build(2.0));
    EXPECT_TRUE(boundary.ValidatePD().is_valid);
    EXPECT_FALSE(HasWarning(boundary));
}



/**
 * エラーケース: コンストラクタの例外型
 */

// 境界: パラメータ数が最小値13のすぐ外 (12個) はEntityParameterError
TEST(RationalBSplineSurfaceErrorTest,
     Constructor_ThrowsEntityParameterErrorWhenTooFewParams) {
    auto param = igesio::IGESParameterVector{
        1, 1, 1, 1,                     // K1, K2, M1, M2
        true, true, false, true, true,  // PROP1-5
        0., 0., 1.                      // 12個目で打ち切り
    };
    EXPECT_THROW(NSurface surf(param), igesio::EntityParameterError);
}

// K1/M1から計算した必要数に満たない場合もEntityParameterError
TEST(RationalBSplineSurfaceErrorTest,
     Constructor_ThrowsEntityParameterErrorWhenInsufficientForDegrees) {
    // 先頭13個のみ (K1=K2=M1=M2=1の必要数37に対して不足)
    auto param = igesio::IGESParameterVector{
        1, 1, 1, 1,                     // K1, K2, M1, M2
        true, true, false, true, true,  // PROP1-5
        0., 0., 1., 1.                  // 13個 (最小数ちょうど)
    };
    EXPECT_THROW(NSurface surf(param), igesio::EntityParameterError);
}

// 制御点数K1/K2が負の場合はEntityValueError (パラメータ数ではなく値の制約違反)
TEST(RationalBSplineSurfaceErrorTest,
     Constructor_ThrowsEntityValueErrorWhenControlPointCountIsNegative) {
    auto param = igesio::IGESParameterVector{
        -1, 1, 1, 1,                    // K1=-1 (負値)
        true, true, false, true, true,  // PROP1-5
        0., 0., 1., 1.                  // 計13個
    };
    EXPECT_THROW(NSurface surf(param), igesio::EntityValueError);
}

// 次数M1/M2が0の場合はEntityValueError
TEST(RationalBSplineSurfaceErrorTest,
     Constructor_ThrowsEntityValueErrorWhenDegreeIsZero) {
    auto param = igesio::IGESParameterVector{
        1, 1, 0, 1,                     // M1=0
        true, true, false, true, true,  // PROP1-5
        0., 0., 1., 1.                  // 計13個
    };
    EXPECT_THROW(NSurface surf(param), igesio::EntityValueError);
}
