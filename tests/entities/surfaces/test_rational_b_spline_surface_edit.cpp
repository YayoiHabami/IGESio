/**
 * @file tests/entities/surfaces/test_rational_b_spline_surface_edit.cpp
 * @brief RationalBSplineSurface (Type 128) の編集機能のテスト
 * @author Yayoi Habami
 * @date 2026-06-05
 * @copyright 2026 Yayoi Habami
 *
 * テスト対象:
 *   [ファクトリ関数]
 *     MakeRationalBSplineSurface(), MakeClampedBSplineSurface(),
 *     MakeBezierSurface()
 *   [型付きコンストラクタ]
 *     RationalBSplineSurface(k1, k2, m1, m2, ...)
 *   [データ取得]
 *     NumControlPoints(), ControlPointAt(i, j), WeightAt(i, j),
 *     IsPolynomial(), IsUPeriodic(), IsVPeriodic(),
 *     IsUClosed(), IsVClosed() (PDフラグ保持と編集時再計算)
 *   [データ変更]
 *     SetControlPointAt(i, j, p), SetControlPoints(cp),
 *     SetWeightAt(i, j, w), SetWeights(w), SetUKnots(s), SetVKnots(t),
 *     SetParameterRange(r), SetUPeriodic(flag), SetVPeriodic(flag),
 *     SetSurfaceType(type), SetData(...)
 *
 * TODO:
 *   - GetMainPDParameters() 経由の書き出しラウンドトリップは protected の
 *     ため対象外 (test_igesio 側の責務)
 *   - 評価系 (TryGetDefinedDerivatives 等)・ValidatePD の網羅は既存
 *     test_rational_b_spline_surface.cpp の責務
 */
#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/common/iges_parameter_vector.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/entities/surfaces/rational_b_spline_surface.h"

namespace {

namespace i_ent = igesio::entities;
using igesio::Vector3d;
using NSurface = i_ent::RationalBSplineSurface;
using i_ent::RationalBSplineSurfaceType;
/// @brief 浮動小数点比較の許容誤差
constexpr double kTol = 1e-9;



/**
 * 検証ヘルパー
 */

/// @brief Vector3dの成分一致を検証する
void ExpectVectorNear(const Vector3d& actual, const Vector3d& expected,
                      const double tol = kTol) {
    EXPECT_NEAR(actual.x(), expected.x(), tol);
    EXPECT_NEAR(actual.y(), expected.y(), tol);
    EXPECT_NEAR(actual.z(), expected.z(), tol);
}

/// @brief std::vector<double>の全要素一致を検証する
void ExpectVectorsNear(const std::vector<double>& actual,
                       const std::vector<double>& expected) {
    ASSERT_EQ(actual.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_NEAR(actual[i], expected[i], kTol) << "index " << i;
    }
}

/// @brief 曲面上の点S(u, v)を評価する
Vector3d EvalAt(const NSurface& surface, const double u, const double v) {
    const auto pt = surface.TryGetDefinedPointAt(u, v);
    EXPECT_TRUE(pt.has_value())
        << "evaluation failed at (u, v) = (" << u << ", " << v << ")";
    return pt ? *pt : Vector3d::Zero();
}



/**
 * グリッド・ノットのビルダー
 */

/// @brief xy平面上の角ループの頂点列 (P(4)=P(0)で閉じる)
std::vector<Vector3d> SquareLoop() {
    return {Vector3d(0.0, 0.0, 0.0), Vector3d(1.0, 0.0, 0.0),
            Vector3d(1.0, 1.0, 0.0), Vector3d(0.0, 1.0, 0.0),
            Vector3d(0.0, 0.0, 0.0)};
}

/// @brief u方向に閉じた角チューブの制御点グリッド (5×2)
/// @note grid[i][j]: u方向は角ループ、v方向はz軸 (0→1) の押し出し
std::vector<std::vector<Vector3d>> MakeUClosedTubeGrid() {
    std::vector<std::vector<Vector3d>> grid;
    for (const auto& p : SquareLoop()) {
        grid.push_back({p, p + Vector3d(0.0, 0.0, 1.0)});
    }
    return grid;
}

/// @brief 角ループ用のクランプノット (M=1, K=4; 定義域[0,4])
std::vector<double> TubeLoopKnots() {
    return {0.0, 0.0, 1.0, 2.0, 3.0, 4.0, 4.0};
}

/// @brief 直線方向用のクランプノット (M=1, K=1; 定義域[0,1])
std::vector<double> LineKnots() {
    return {0.0, 0.0, 1.0, 1.0};
}

/// @brief 例外系テスト用の正常な入力一式 (degrees={1,1}, 2×2格子)
struct FactoryArgs {
    std::pair<unsigned int, unsigned int> degrees{1, 1};
    std::vector<std::vector<Vector3d>> grid{
        {Vector3d(0.0, 0.0, 0.0), Vector3d(0.0, 1.0, 0.0)},
        {Vector3d(1.0, 0.0, 0.0), Vector3d(1.0, 1.0, 0.0)}};
    std::vector<double> u_knots{0.0, 0.0, 1.0, 1.0};
    std::vector<double> v_knots{0.0, 0.0, 1.0, 1.0};
};



/**
 * テスト用インスタンスのファクトリ関数
 *
 * IGES 128 パラメータ形式:
 *   K1, K2, M1, M2, PROP1-5(bool×5), uノット(K1+M1+2個),
 *   vノット(K2+M2+2個), 重み((K1+1)(K2+1)個),
 *   制御点(3×(K1+1)(K2+1)個; 第1添字iが最速で変化),
 *   U(0), U(1), V(0), V(1)
 *
 * NOTE: セッター/ゲッターのテストがファクトリ関数の正しさに依存しない
 *       よう、基準インスタンスはPDコンストラクタ経由で構築する
 */

/// @brief 非対称双線形パッチの基準PD (既存テストと同形状)
/// K1=K2=1, M1=M2=1, ノット[0,0,1,1]×2, 重み全て1, 範囲[0,1]²
/// P(0,0)=(0,0,0), P(1,0)=(10,0,0), P(0,1)=(0,20,0), P(1,1)=(10,20,0)
/// @note u方向はx軸、v方向はy軸に対応し、u/v転置バグの検出に使う
std::shared_ptr<NSurface> MakeBilinearPatchPD() {
    const auto param = igesio::IGESParameterVector{
        1, 1, 1, 1,                        // K1, K2, M1, M2
        false, false, true, false, false,  // PROP1-5
        0., 0., 1., 1.,                    // uノット
        0., 0., 1., 1.,                    // vノット
        1., 1., 1., 1.,                    // 重み
        0., 0., 0.,                        // P(0,0)
        10., 0., 0.,                       // P(1,0)
        0., 20., 0.,                       // P(0,1)
        10., 20., 0.,                      // P(1,1)
        0., 1., 0., 1.                     // U, Vの範囲
    };
    return std::make_shared<NSurface>(param);
}

/// @brief degrees={2,1}の有理パッチの基準PD (3×2制御点)
/// K1=2, K2=1, uノット[0,0,0,1,1,1], vノット[0,0,1,1],
/// 重みW(1,j)=2のみ非1 (rational), 範囲[0,1]²
/// P(i,j): u方向に(1,0)→(1,1)→(0,1)の折れ線、v方向にz軸 (0→5) 押し出し
/// @note u/vでノット数・制御点数・重み形状が異なるため、
///       方向の取り違えはサイズ不一致として検出される
std::shared_ptr<NSurface> MakeDeg21RationalPatchPD() {
    const auto param = igesio::IGESParameterVector{
        2, 1, 2, 1,                          // K1, K2, M1, M2
        false, false, false, false, false,   // PROP1-5
        0., 0., 0., 1., 1., 1.,              // uノット
        0., 0., 1., 1.,                      // vノット
        1., 2., 1., 1., 2., 1.,              // 重み (W(1,0)=W(1,1)=2)
        1., 0., 0.,                          // P(0,0)
        1., 1., 0.,                          // P(1,0)
        0., 1., 0.,                          // P(2,0)
        1., 0., 5.,                          // P(0,1)
        1., 1., 5.,                          // P(1,1)
        0., 1., 5.,                          // P(2,1)
        0., 1., 0., 1.                       // U, Vの範囲
    };
    return std::make_shared<NSurface>(param);
}

/// @brief u方向に閉じた角チューブの基準PD (PROP1=1は幾何と整合)
/// K1=4, K2=1, M1=M2=1, uノット[0,0,1,2,3,4,4], vノット[0,0,1,1],
/// 重み全て1, 範囲[0,4]×[0,1]. 制御点はMakeUClosedTubeGrid()と同一
std::shared_ptr<NSurface> MakeClosedSquareTubePD() {
    const auto param = igesio::IGESParameterVector{
        4, 1, 1, 1,                         // K1, K2, M1, M2
        true, false, true, false, false,    // PROP1-5
        0., 0., 1., 2., 3., 4., 4.,         // uノット
        0., 0., 1., 1.,                     // vノット
        1., 1., 1., 1., 1.,                 // 重み (j=0)
        1., 1., 1., 1., 1.,                 // 重み (j=1)
        0., 0., 0.,  1., 0., 0.,  1., 1., 0.,   // P(0,0)〜P(2,0)
        0., 1., 0.,  0., 0., 0.,                // P(3,0), P(4,0)=P(0,0)
        0., 0., 1.,  1., 0., 1.,  1., 1., 1.,   // P(0,1)〜P(2,1)
        0., 1., 1.,  0., 0., 1.,                // P(3,1), P(4,1)=P(0,1)
        0., 4., 0., 1.                      // U, Vの範囲
    };
    return std::make_shared<NSurface>(param);
}

/// @brief 開いた双線形パッチに偽の閉・周期フラグを与えたPD
/// @note 幾何は開いているがPROP1=PROP2=PROP4=PROP5=1とする.
///       読込時はファイルのフラグを保持する仕様のロックイン用
std::shared_ptr<NSurface> MakeBilinearPatchWrongFlagsPD() {
    const auto param = igesio::IGESParameterVector{
        1, 1, 1, 1,                    // K1, K2, M1, M2
        true, true, true, true, true,  // PROP1-5 (幾何と不一致の偽値)
        0., 0., 1., 1.,                // uノット
        0., 0., 1., 1.,                // vノット
        1., 1., 1., 1.,                // 重み
        0., 0., 0.,                    // P(0,0)
        10., 0., 0.,                   // P(1,0)
        0., 20., 0.,                   // P(0,1)
        10., 20., 0.,                  // P(1,1)
        0., 1., 0., 1.                 // U, Vの範囲
    };
    return std::make_shared<NSurface>(param);
}

}  // namespace



/**
 * MakeRationalBSplineSurface (一般形) のテスト
 */

// 入力したパラメータがそのまま格納される (グリッド[i][j]とP(i,j)の対応含む)
TEST(RationalBSplineSurfaceFactoryTest, MakeRationalBSplineSurface_StoredValues) {
    // degrees={1,2}, 2×4格子 (K1=1, K2=3)
    std::vector<std::vector<Vector3d>> grid(2);
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 4; ++j) {
            grid[i].push_back(
                Vector3d(10.0 * i, 10.0 * j, static_cast<double>(i + j)));
        }
    }
    const std::vector<double> u_knots{0.0, 0.0, 1.0, 1.0};
    const std::vector<double> v_knots{0.0, 0.0, 0.0, 0.5, 1.0, 1.0, 1.0};
    const std::vector<std::vector<double>> weights{
        {1.0, 2.0, 3.0, 4.0}, {5.0, 6.0, 7.0, 8.0}};
    const auto surface = i_ent::MakeRationalBSplineSurface(
        {1, 2}, grid, u_knots, v_knots, weights,
        std::array<double, 4>{0.0, 1.0, 0.0, 1.0});

    EXPECT_EQ(surface->Degrees(), std::make_pair(1u, 2u));
    EXPECT_EQ(surface->NumControlPoints(), std::make_pair(2u, 4u));
    ExpectVectorsNear(surface->UKnots(), u_knots);
    ExpectVectorsNear(surface->VKnots(), v_knots);
    for (unsigned int i = 0; i < 2; ++i) {
        for (unsigned int j = 0; j < 4; ++j) {
            EXPECT_NEAR(surface->WeightAt(i, j), weights[i][j], kTol)
                << "W(" << i << ", " << j << ")";
            ExpectVectorNear(surface->ControlPointAt(i, j), grid[i][j]);
        }
    }
    const auto range = surface->GetParameterRange();
    EXPECT_NEAR(range[0], 0.0, kTol);
    EXPECT_NEAR(range[1], 1.0, kTol);
    EXPECT_NEAR(range[2], 0.0, kTol);
    EXPECT_NEAR(range[3], 1.0, kTol);
    EXPECT_EQ(surface->GetSurfaceType(),
              RationalBSplineSurfaceType::kUndetermined);
    EXPECT_FALSE(surface->IsPolynomial());
    EXPECT_TRUE(surface->ValidatePD().is_valid);
}

// グリッドの[i][j]がu/v方向に正しく対応する (フラット化の転置バグ検出)
// - 非対称双線形格子で S(1,0)=grid[1][0], S(0,1)=grid[0][1] を確認する
TEST(RationalBSplineSurfaceFactoryTest,
     MakeRationalBSplineSurface_GridOrderMatchesEvaluation) {
    const std::vector<std::vector<Vector3d>> grid{
        {Vector3d(0.0, 0.0, 0.0), Vector3d(0.0, 20.0, 0.0)},
        {Vector3d(10.0, 0.0, 0.0), Vector3d(10.0, 20.0, 0.0)}};
    const auto surface = i_ent::MakeRationalBSplineSurface(
        {1, 1}, grid, LineKnots(), LineKnots());

    ExpectVectorNear(EvalAt(*surface, 1.0, 0.0), Vector3d(10.0, 0.0, 0.0));
    ExpectVectorNear(EvalAt(*surface, 0.0, 1.0), Vector3d(0.0, 20.0, 0.0));
}

// 重み省略時は全て1.0 (polynomial形式) となる
TEST(RationalBSplineSurfaceFactoryTest,
     MakeRationalBSplineSurface_EmptyWeights_PolynomialAllOnes) {
    const FactoryArgs args;
    const auto surface = i_ent::MakeRationalBSplineSurface(
        args.degrees, args.grid, args.u_knots, args.v_knots);

    for (unsigned int i = 0; i < 2; ++i) {
        for (unsigned int j = 0; j < 2; ++j) {
            EXPECT_NEAR(surface->WeightAt(i, j), 1.0, kTol);
        }
    }
    EXPECT_TRUE(surface->IsPolynomial());
}

// 非一様な重みを指定した場合はrational (PROP3=0) となる
TEST(RationalBSplineSurfaceFactoryTest,
     MakeRationalBSplineSurface_NonEqualWeights_NotPolynomial) {
    const FactoryArgs args;
    const auto surface = i_ent::MakeRationalBSplineSurface(
        args.degrees, args.grid, args.u_knots, args.v_knots,
        {{1.0, 1.0}, {1.0, 2.0}});

    EXPECT_FALSE(surface->IsPolynomial());
    EXPECT_NEAR(surface->WeightAt(1, 1), 2.0, kTol);
}

// 範囲省略時はノット定義域 [S(0), S(N1)]×[T(0), T(N2)] となる
TEST(RationalBSplineSurfaceFactoryTest,
     MakeRationalBSplineSurface_OmittedRange_UsesKnotDomain) {
    // u: 定義域[0,3] (非自明な内部ノット), v: 定義域[0,2]
    std::vector<std::vector<Vector3d>> grid(3);
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 2; ++j) {
            grid[i].push_back(Vector3d(1.0 * i, 2.0 * j, 0.0));
        }
    }
    const auto surface = i_ent::MakeRationalBSplineSurface(
        {1, 1}, grid, {0.0, 0.0, 1.0, 3.0, 3.0}, {0.0, 0.0, 2.0, 2.0});

    const auto range = surface->GetParameterRange();
    EXPECT_NEAR(range[0], 0.0, kTol);
    EXPECT_NEAR(range[1], 3.0, kTol);
    EXPECT_NEAR(range[2], 0.0, kTol);
    EXPECT_NEAR(range[3], 2.0, kTol);
}

// 周期フラグ (PROP4/PROP5) が指定どおり格納される (デフォルトはfalse)
TEST(RationalBSplineSurfaceFactoryTest,
     MakeRationalBSplineSurface_PeriodicFlags_Stored) {
    const FactoryArgs args;
    const auto defaulted = i_ent::MakeRationalBSplineSurface(
        args.degrees, args.grid, args.u_knots, args.v_knots);
    EXPECT_FALSE(defaulted->IsUPeriodic());
    EXPECT_FALSE(defaulted->IsVPeriodic());

    const auto u_periodic = i_ent::MakeRationalBSplineSurface(
        args.degrees, args.grid, args.u_knots, args.v_knots, {},
        std::nullopt, true, false);
    EXPECT_TRUE(u_periodic->IsUPeriodic());
    EXPECT_FALSE(u_periodic->IsVPeriodic());
}



/**
 * MakeRationalBSplineSurface: 閉判定 (PROP1/PROP2) のテスト
 */

// クランプかつ境界制御点が一致する場合、u方向のみ閉と判定される
TEST(RationalBSplineSurfaceFactoryTest,
     MakeRationalBSplineSurface_UClosed_BoundaryCPsCoincide) {
    const auto surface = i_ent::MakeRationalBSplineSurface(
        {1, 1}, MakeUClosedTubeGrid(), TubeLoopKnots(), LineKnots());

    EXPECT_TRUE(surface->IsUClosed());
    EXPECT_FALSE(surface->IsVClosed());
}

// 転置した格子ではv方向のみ閉と判定される (u/v取り違え検出)
TEST(RationalBSplineSurfaceFactoryTest,
     MakeRationalBSplineSurface_VClosed_BoundaryCPsCoincide) {
    // 2×5格子: v方向が角ループ、u方向がz軸押し出し
    std::vector<std::vector<Vector3d>> grid(2);
    for (int i = 0; i < 2; ++i) {
        for (const auto& p : SquareLoop()) {
            grid[i].push_back(p + Vector3d(0.0, 0.0, static_cast<double>(i)));
        }
    }
    const auto surface = i_ent::MakeRationalBSplineSurface(
        {1, 1}, grid, LineKnots(), TubeLoopKnots());

    EXPECT_FALSE(surface->IsUClosed());
    EXPECT_TRUE(surface->IsVClosed());
}

// 境界の重みが比例 (W(K1,j) = c・W(0,j)) なら境界曲線は一致し、閉となる
TEST(RationalBSplineSurfaceFactoryTest,
     MakeRationalBSplineSurface_UClosed_ProportionalBoundaryWeights) {
    const std::vector<std::vector<double>> weights{
        {1.0, 1.0}, {1.0, 1.0}, {1.0, 1.0}, {1.0, 1.0}, {2.0, 2.0}};
    const auto surface = i_ent::MakeRationalBSplineSurface(
        {1, 1}, MakeUClosedTubeGrid(), TubeLoopKnots(), LineKnots(), weights);

    EXPECT_TRUE(surface->IsUClosed());
}

// 境界制御点が一致しても重みが比例しない場合は閉と判定されない
TEST(RationalBSplineSurfaceFactoryTest,
     MakeRationalBSplineSurface_UClosedCPs_NonProportionalWeights_NotClosed) {
    const std::vector<std::vector<double>> weights{
        {1.0, 1.0}, {1.0, 1.0}, {1.0, 1.0}, {1.0, 1.0}, {1.0, 2.0}};
    const auto surface = i_ent::MakeRationalBSplineSurface(
        {1, 1}, MakeUClosedTubeGrid(), TubeLoopKnots(), LineKnots(), weights);

    EXPECT_FALSE(surface->IsUClosed());
}

// 非クランプノットの閉ループはサンプル評価で閉と判定される
TEST(RationalBSplineSurfaceFactoryTest,
     MakeRationalBSplineSurface_UClosed_UnclampedKnots_DetectedViaSampling) {
    // 非クランプ一様ノット (M1=1): 定義域は[S(0), S(N1)] = [1, 5].
    // 次数1のB-Splineは t=knots[i+1] で P(i) を通るため、
    // S(1,v)=P(0,・), S(5,v)=P(4,・)=P(0,・) で閉となる
    const auto surface = i_ent::MakeRationalBSplineSurface(
        {1, 1}, MakeUClosedTubeGrid(),
        {0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0}, LineKnots());

    const auto range = surface->GetParameterRange();
    EXPECT_NEAR(range[0], 1.0, kTol);
    EXPECT_NEAR(range[1], 5.0, kTol);
    EXPECT_TRUE(surface->IsUClosed());
    EXPECT_FALSE(surface->IsVClosed());
}



/**
 * MakeRationalBSplineSurface: 例外系のテスト
 */

// 次数0 (u側・v側) はEntityValueError
TEST(RationalBSplineSurfaceFactoryTest,
     MakeRationalBSplineSurface_ThrowsWhenDegreeZero) {
    const FactoryArgs args;
    EXPECT_THROW(
        i_ent::MakeRationalBSplineSurface(
            {0, 1}, args.grid, {0.0, 1.0, 2.0}, args.v_knots),
        igesio::EntityValueError);
    EXPECT_THROW(
        i_ent::MakeRationalBSplineSurface(
            {1, 0}, args.grid, args.u_knots, {0.0, 1.0, 2.0}),
        igesio::EntityValueError);
}

// 空のグリッド ({} と {{}}) はEntityValueError
TEST(RationalBSplineSurfaceFactoryTest,
     MakeRationalBSplineSurface_ThrowsWhenGridEmpty) {
    const FactoryArgs args;
    const std::vector<std::vector<Vector3d>> empty{};
    const std::vector<std::vector<Vector3d>> empty_row{{}};
    EXPECT_THROW(
        i_ent::MakeRationalBSplineSurface(
            args.degrees, empty, args.u_knots, args.v_knots),
        igesio::EntityValueError);
    EXPECT_THROW(
        i_ent::MakeRationalBSplineSurface(
            args.degrees, empty_row, args.u_knots, args.v_knots),
        igesio::EntityValueError);
}

// 行ごとに長さの異なるグリッドはEntityValueError
TEST(RationalBSplineSurfaceFactoryTest,
     MakeRationalBSplineSurface_ThrowsWhenGridNotRectangular) {
    const FactoryArgs args;
    const std::vector<std::vector<Vector3d>> jagged{
        {Vector3d(0.0, 0.0, 0.0), Vector3d(0.0, 1.0, 0.0)},
        {Vector3d(1.0, 0.0, 0.0)}};
    EXPECT_THROW(
        i_ent::MakeRationalBSplineSurface(
            args.degrees, jagged, args.u_knots, args.v_knots),
        igesio::EntityValueError);
}

// 制御点数が次数+1未満の場合はEntityValueError
TEST(RationalBSplineSurfaceFactoryTest,
     MakeRationalBSplineSurface_ThrowsWhenTooFewControlPoints) {
    const FactoryArgs args;
    // u方向2点に対して次数2 (3点必要)
    EXPECT_THROW(
        i_ent::MakeRationalBSplineSurface(
            {2, 1}, args.grid, {0.0, 0.0, 0.0, 1.0, 1.0, 1.0}, args.v_knots),
        igesio::EntityValueError);
}

// ノット数が不正な場合はEntityValueError (u側・v側)
TEST(RationalBSplineSurfaceFactoryTest,
     MakeRationalBSplineSurface_ThrowsWhenKnotCountWrong) {
    const FactoryArgs args;
    EXPECT_THROW(
        i_ent::MakeRationalBSplineSurface(
            args.degrees, args.grid, {0.0, 0.0, 1.0}, args.v_knots),
        igesio::EntityValueError);
    EXPECT_THROW(
        i_ent::MakeRationalBSplineSurface(
            args.degrees, args.grid, args.u_knots, {0.0, 0.0, 1.0, 1.0, 1.0}),
        igesio::EntityValueError);
}

// 非減少でないノットはEntityValueError
TEST(RationalBSplineSurfaceFactoryTest,
     MakeRationalBSplineSurface_ThrowsWhenKnotsDecreasing) {
    const FactoryArgs args;
    EXPECT_THROW(
        i_ent::MakeRationalBSplineSurface(
            args.degrees, args.grid, {0.0, 1.0, 0.5, 1.0}, args.v_knots),
        igesio::EntityValueError);
    EXPECT_THROW(
        i_ent::MakeRationalBSplineSurface(
            args.degrees, args.grid, args.u_knots, {0.0, 1.0, 0.5, 1.0}),
        igesio::EntityValueError);
}

// 重みグリッドの寸法不正 (行数・行長) はEntityValueError
TEST(RationalBSplineSurfaceFactoryTest,
     MakeRationalBSplineSurface_ThrowsWhenWeightGridShapeWrong) {
    const FactoryArgs args;
    const std::vector<std::vector<double>> wrong_rows{
        {1.0, 1.0}, {1.0, 1.0}, {1.0, 1.0}};
    const std::vector<std::vector<double>> wrong_cols{
        {1.0, 1.0, 1.0}, {1.0, 1.0, 1.0}};
    EXPECT_THROW(
        i_ent::MakeRationalBSplineSurface(
            args.degrees, args.grid, args.u_knots, args.v_knots, wrong_rows),
        igesio::EntityValueError);
    EXPECT_THROW(
        i_ent::MakeRationalBSplineSurface(
            args.degrees, args.grid, args.u_knots, args.v_knots, wrong_cols),
        igesio::EntityValueError);
}

// 正でない重み (0, 負) はEntityValueError
TEST(RationalBSplineSurfaceFactoryTest,
     MakeRationalBSplineSurface_ThrowsWhenWeightNotPositive) {
    const FactoryArgs args;
    EXPECT_THROW(
        i_ent::MakeRationalBSplineSurface(
            args.degrees, args.grid, args.u_knots, args.v_knots,
            {{1.0, 1.0}, {1.0, 0.0}}),
        igesio::EntityValueError);
    EXPECT_THROW(
        i_ent::MakeRationalBSplineSurface(
            args.degrees, args.grid, args.u_knots, args.v_knots,
            {{1.0, 1.0}, {1.0, -1.0}}),
        igesio::EntityValueError);
}

// 不正なパラメータ範囲 (U(0)>=U(1), V(0)>=V(1)) はEntityValueError
TEST(RationalBSplineSurfaceFactoryTest,
     MakeRationalBSplineSurface_ThrowsWhenRangeInvalid) {
    const FactoryArgs args;
    EXPECT_THROW(
        i_ent::MakeRationalBSplineSurface(
            args.degrees, args.grid, args.u_knots, args.v_knots, {},
            std::array<double, 4>{1.0, 1.0, 0.0, 1.0}),
        igesio::EntityValueError);
    EXPECT_THROW(
        i_ent::MakeRationalBSplineSurface(
            args.degrees, args.grid, args.u_knots, args.v_knots, {},
            std::array<double, 4>{0.0, 1.0, 1.0, 0.5}),
        igesio::EntityValueError);
}



/**
 * MakeClampedBSplineSurface のテスト
 */

// クランプ一様ノットが両方向に生成される
TEST(RationalBSplineSurfaceFactoryTest, MakeClampedBSplineSurface_KnotStructure) {
    // degrees={2,1}, 4×3格子: 内部ノットはu方向1個 (0.5)、v方向1個 (0.5)
    std::vector<std::vector<Vector3d>> grid(4);
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 3; ++j) {
            grid[i].push_back(Vector3d(1.0 * i, 2.0 * j, 0.0));
        }
    }
    const auto surface = i_ent::MakeClampedBSplineSurface({2, 1}, grid);

    ExpectVectorsNear(surface->UKnots(), {0.0, 0.0, 0.0, 0.5, 1.0, 1.0, 1.0});
    ExpectVectorsNear(surface->VKnots(), {0.0, 0.0, 0.5, 1.0, 1.0});
    const auto range = surface->GetParameterRange();
    EXPECT_NEAR(range[0], 0.0, kTol);
    EXPECT_NEAR(range[1], 1.0, kTol);
    EXPECT_NEAR(range[2], 0.0, kTol);
    EXPECT_NEAR(range[3], 1.0, kTol);
    EXPECT_TRUE(surface->ValidatePD().is_valid);
}

// 制御点数 = 次数+1 の場合は内部ノットなし (Bezier型ノット)
TEST(RationalBSplineSurfaceFactoryTest,
     MakeClampedBSplineSurface_NoInteriorKnots) {
    std::vector<std::vector<Vector3d>> grid(3);
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            grid[i].push_back(Vector3d(1.0 * i, 1.0 * j, 0.0));
        }
    }
    const auto surface = i_ent::MakeClampedBSplineSurface({2, 2}, grid);

    ExpectVectorsNear(surface->UKnots(), {0.0, 0.0, 0.0, 1.0, 1.0, 1.0});
    ExpectVectorsNear(surface->VKnots(), {0.0, 0.0, 0.0, 1.0, 1.0, 1.0});
}

// クランプ曲面は四隅の制御点を通る (u/v順序の検証を兼ねる)
TEST(RationalBSplineSurfaceFactoryTest,
     MakeClampedBSplineSurface_InterpolatesCorners) {
    std::vector<std::vector<Vector3d>> grid(4);
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 3; ++j) {
            grid[i].push_back(
                Vector3d(1.0 * i, 2.0 * j, 0.5 * i * j));
        }
    }
    const auto surface = i_ent::MakeClampedBSplineSurface({2, 1}, grid);

    ExpectVectorNear(EvalAt(*surface, 0.0, 0.0), grid[0][0]);
    ExpectVectorNear(EvalAt(*surface, 1.0, 0.0), grid[3][0]);
    ExpectVectorNear(EvalAt(*surface, 0.0, 1.0), grid[0][2]);
    ExpectVectorNear(EvalAt(*surface, 1.0, 1.0), grid[3][2]);
}

// 制御点数が次数+1未満の場合はEntityValueError (アンダーフローガード)
TEST(RationalBSplineSurfaceFactoryTest,
     MakeClampedBSplineSurface_ThrowsWhenTooFewControlPoints) {
    const FactoryArgs args;  // 2×2格子
    EXPECT_THROW(i_ent::MakeClampedBSplineSurface({3, 1}, args.grid),
                 igesio::EntityValueError);
    EXPECT_THROW(i_ent::MakeClampedBSplineSurface({1, 3}, args.grid),
                 igesio::EntityValueError);
}

// 次数0はEntityValueError
TEST(RationalBSplineSurfaceFactoryTest,
     MakeClampedBSplineSurface_ThrowsWhenDegreeZero) {
    const FactoryArgs args;
    EXPECT_THROW(i_ent::MakeClampedBSplineSurface({0, 1}, args.grid),
                 igesio::EntityValueError);
}



/**
 * MakeBezierSurface のテスト
 */

// 次数とノットがグリッド寸法から決定される
TEST(RationalBSplineSurfaceFactoryTest, MakeBezierSurface_DegreeAndKnots) {
    std::vector<std::vector<Vector3d>> grid(3);
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 2; ++j) {
            grid[i].push_back(Vector3d(1.0 * i, 1.0 * j, 0.0));
        }
    }
    const auto surface = i_ent::MakeBezierSurface(grid);

    EXPECT_EQ(surface->Degrees(), std::make_pair(2u, 1u));
    EXPECT_EQ(surface->NumControlPoints(), std::make_pair(3u, 2u));
    ExpectVectorsNear(surface->UKnots(), {0.0, 0.0, 0.0, 1.0, 1.0, 1.0});
    ExpectVectorsNear(surface->VKnots(), {0.0, 0.0, 1.0, 1.0});
    const auto range = surface->GetParameterRange();
    EXPECT_NEAR(range[0], 0.0, kTol);
    EXPECT_NEAR(range[1], 1.0, kTol);
    EXPECT_NEAR(range[2], 0.0, kTol);
    EXPECT_NEAR(range[3], 1.0, kTol);
    EXPECT_TRUE(surface->ValidatePD().is_valid);
}

// 2×2のBezierパッチは双線形補間 (中央は四隅の平均)
TEST(RationalBSplineSurfaceFactoryTest, MakeBezierSurface_BilinearMidpoint) {
    const std::vector<std::vector<Vector3d>> grid{
        {Vector3d(0.0, 0.0, 0.0), Vector3d(0.0, 0.0, 4.0)},
        {Vector3d(2.0, 0.0, 0.0), Vector3d(2.0, 2.0, 2.0)}};
    const auto surface = i_ent::MakeBezierSurface(grid);

    // S(1/2, 1/2) = (P(0,0) + P(0,1) + P(1,0) + P(1,1)) / 4
    ExpectVectorNear(EvalAt(*surface, 0.5, 0.5), Vector3d(1.0, 0.5, 1.5));
}

// 有理Bezier: 1/4円柱面 (u方向円弧×v方向直線) の正確な評価
TEST(RationalBSplineSurfaceFactoryTest, MakeBezierSurface_RationalQuarterCylinder) {
    // u方向: 半径1の1/4円弧 (重み {1, √2/2, 1}), v方向: z軸 (0→3) の直線
    const std::vector<std::vector<Vector3d>> grid{
        {Vector3d(1.0, 0.0, 0.0), Vector3d(1.0, 0.0, 3.0)},
        {Vector3d(1.0, 1.0, 0.0), Vector3d(1.0, 1.0, 3.0)},
        {Vector3d(0.0, 1.0, 0.0), Vector3d(0.0, 1.0, 3.0)}};
    const double w = std::sqrt(2.0) / 2.0;
    const std::vector<std::vector<double>> weights{
        {1.0, 1.0}, {w, w}, {1.0, 1.0}};
    const auto surface = i_ent::MakeBezierSurface(grid, weights);

    // 曲面上の任意の点はz軸から距離1、z=3v
    for (const double u : {0.0, 0.25, 0.5, 0.75, 1.0}) {
        for (const double v : {0.0, 0.5, 1.0}) {
            const auto pt = EvalAt(*surface, u, v);
            EXPECT_NEAR(pt.x() * pt.x() + pt.y() * pt.y(), 1.0, kTol)
                << "(u, v) = (" << u << ", " << v << ")";
            EXPECT_NEAR(pt.z(), 3.0 * v, kTol)
                << "(u, v) = (" << u << ", " << v << ")";
        }
    }
}

// 各方向2点未満の格子はEntityValueError
TEST(RationalBSplineSurfaceFactoryTest, MakeBezierSurface_ThrowsWhenLessThan2x2) {
    const std::vector<std::vector<Vector3d>> one_row{
        {Vector3d(0.0, 0.0, 0.0), Vector3d(0.0, 1.0, 0.0)}};
    const std::vector<std::vector<Vector3d>> one_col{
        {Vector3d(0.0, 0.0, 0.0)}, {Vector3d(1.0, 0.0, 0.0)}};
    EXPECT_THROW(i_ent::MakeBezierSurface(one_row), igesio::EntityValueError);
    EXPECT_THROW(i_ent::MakeBezierSurface(one_col), igesio::EntityValueError);
}



/**
 * 型付きコンストラクタ (低レベル・内部表現) のテスト
 */

// 格納値の確認と、PROP1/PROP3が入力データから導出されることの確認
TEST(RationalBSplineSurfaceTypedCtorTest, StoredValuesAndDerivedProps) {
    // u方向に退化して閉じた構造: P(1,j) = P(0,j)
    igesio::Matrix3Xd cp(3, 4);  // col = i * (K2+1) + j
    cp.col(0) = Vector3d(0.0, 0.0, 0.0);  // P(0,0)
    cp.col(1) = Vector3d(0.0, 0.0, 1.0);  // P(0,1)
    cp.col(2) = Vector3d(0.0, 0.0, 0.0);  // P(1,0) = P(0,0)
    cp.col(3) = Vector3d(0.0, 0.0, 1.0);  // P(1,1) = P(0,1)
    igesio::MatrixXd weights(2, 2);
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) weights(i, j) = 1.0;
    }

    const NSurface surface(
        1, 1, 1, 1, {0.0, 0.0, 1.0, 1.0}, {0.0, 0.0, 1.0, 1.0},
        weights, cp, std::array<double, 4>{0.0, 1.0, 0.0, 1.0});

    EXPECT_EQ(surface.Degrees(), std::make_pair(1u, 1u));
    EXPECT_EQ(surface.NumControlPoints(), std::make_pair(2u, 2u));
    ExpectVectorNear(surface.ControlPointAt(0, 1), Vector3d(0.0, 0.0, 1.0));
    EXPECT_TRUE(surface.IsPolynomial());  // PROP3: 重みから導出
    EXPECT_TRUE(surface.IsUClosed());     // PROP1: 幾何から再計算
    EXPECT_FALSE(surface.IsVClosed());
    EXPECT_FALSE(surface.IsUPeriodic());
    EXPECT_FALSE(surface.IsVPeriodic());
    EXPECT_TRUE(surface.ValidatePD().is_valid);
}

// 寸法が整合しない場合はEntityValueError (PD解釈位置のずれ防止)
TEST(RationalBSplineSurfaceTypedCtorTest, ThrowsWhenSizesInconsistent) {
    igesio::Matrix3Xd cp(3, 4);
    for (int c = 0; c < 4; ++c) cp.col(c) = Vector3d(1.0 * c, 0.0, 0.0);
    igesio::MatrixXd weights(2, 2);
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) weights(i, j) = 1.0;
    }
    const std::vector<double> knots{0.0, 0.0, 1.0, 1.0};
    const std::array<double, 4> range{0.0, 1.0, 0.0, 1.0};

    // 重みの形状不正 ((K1+1)×(K2+1) = 2×2のところ2×3)
    igesio::MatrixXd wrong_weights(2, 3);
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 3; ++j) wrong_weights(i, j) = 1.0;
    }
    EXPECT_THROW(
        NSurface surf(1, 1, 1, 1, knots, knots, wrong_weights, cp, range),
        igesio::EntityValueError);

    // uノット数不正 (K1+M1+2 = 4のところ3)
    EXPECT_THROW(
        NSurface surf(1, 1, 1, 1, {0.0, 0.0, 1.0}, knots, weights, cp, range),
        igesio::EntityValueError);
}



/**
 * データ取得のテスト
 */

// NumControlPointsはPDのK1+1, K2+1と一致する
TEST(RationalBSplineSurfaceGetterTest, NumControlPoints_MatchesPD) {
    const auto surface = MakeDeg21RationalPatchPD();
    EXPECT_EQ(surface->NumControlPoints(), std::make_pair(3u, 2u));
    EXPECT_EQ(surface->Degrees(), std::make_pair(2u, 1u));
}

// 範囲外インデックスはstd::out_of_range (i側・j側それぞれ)
TEST(RationalBSplineSurfaceGetterTest,
     ControlPointAt_WeightAt_ThrowOutOfRange) {
    const auto surface = MakeDeg21RationalPatchPD();  // 3×2
    EXPECT_THROW(surface->ControlPointAt(3, 0), std::out_of_range);
    EXPECT_THROW(surface->ControlPointAt(0, 2), std::out_of_range);
    EXPECT_THROW(surface->WeightAt(3, 0), std::out_of_range);
    EXPECT_THROW(surface->WeightAt(0, 2), std::out_of_range);
}

// PROP3/PROP4/PROP5フラグがPDから読み出される
TEST(RationalBSplineSurfaceGetterTest, PropFlags_ReadFromPD) {
    const auto rational = MakeDeg21RationalPatchPD();  // PROP3-5 = 0
    EXPECT_FALSE(rational->IsPolynomial());
    EXPECT_FALSE(rational->IsUPeriodic());
    EXPECT_FALSE(rational->IsVPeriodic());

    const auto flagged = MakeBilinearPatchWrongFlagsPD();  // PROP3-5 = 1
    EXPECT_TRUE(flagged->IsPolynomial());
    EXPECT_TRUE(flagged->IsUPeriodic());
    EXPECT_TRUE(flagged->IsVPeriodic());
}

// 読込時は閉フラグ (PROP1/PROP2) を幾何と照合せずそのまま保持する
// (round-trip保全方針のロックイン)
TEST(RationalBSplineSurfaceGetterTest, ClosedFlags_PreservedFromPD) {
    // 幾何的には開いたパッチだが、PROP1=PROP2=1を保持する
    const auto flagged = MakeBilinearPatchWrongFlagsPD();
    EXPECT_TRUE(flagged->IsUClosed());
    EXPECT_TRUE(flagged->IsVClosed());

    // 幾何的に閉じたチューブのPROP1=1, PROP2=0も保持する
    const auto tube = MakeClosedSquareTubePD();
    EXPECT_TRUE(tube->IsUClosed());
    EXPECT_FALSE(tube->IsVClosed());
}



/**
 * データ変更のテスト
 */

// 制御点の変更が格納値と評価結果に反映される
TEST(RationalBSplineSurfaceSetterTest, SetControlPointAt_UpdatesStoredPoint) {
    const auto surface = MakeBilinearPatchPD();
    surface->SetControlPointAt(1, 0, Vector3d(12.0, 0.0, 3.0));

    ExpectVectorNear(surface->ControlPointAt(1, 0), Vector3d(12.0, 0.0, 3.0));
    // クランプ双線形パッチの隅 S(1,0) は制御点P(1,0)に一致する
    ExpectVectorNear(EvalAt(*surface, 1.0, 0.0), Vector3d(12.0, 0.0, 3.0));
    // 他の制御点は不変
    ExpectVectorNear(surface->ControlPointAt(0, 1), Vector3d(0.0, 20.0, 0.0));
}

// 範囲外インデックスはstd::out_of_range
TEST(RationalBSplineSurfaceSetterTest, SetControlPointAt_ThrowsOutOfRange) {
    const auto surface = MakeBilinearPatchPD();  // 2×2
    EXPECT_THROW(surface->SetControlPointAt(2, 0, Vector3d(0.0, 0.0, 0.0)),
                 std::out_of_range);
    EXPECT_THROW(surface->SetControlPointAt(0, 2, Vector3d(0.0, 0.0, 0.0)),
                 std::out_of_range);
}

// 閉じたチューブの境界制御点を動かすと閉フラグが解除され、戻すと復帰する
TEST(RationalBSplineSurfaceSetterTest,
     SetControlPointAt_OpensAndReclosesTube) {
    const auto tube = MakeClosedSquareTubePD();
    ASSERT_TRUE(tube->IsUClosed());

    // P(4,0)をP(0,0)=(0,0,0)からずらすと開く
    tube->SetControlPointAt(4, 0, Vector3d(0.5, 0.0, 0.0));
    EXPECT_FALSE(tube->IsUClosed());

    // 戻すと再び閉じる
    tube->SetControlPointAt(4, 0, Vector3d(0.0, 0.0, 0.0));
    EXPECT_TRUE(tube->IsUClosed());
    EXPECT_FALSE(tube->IsVClosed());
}

// 編集すると閉フラグはPDの値ではなく幾何から再計算される
TEST(RationalBSplineSurfaceSetterTest,
     SetControlPointAt_RecomputesFlagsEvenIfPDFlagWrong) {
    // PROP1=PROP2=1だが幾何的には開いたパッチ
    const auto surface = MakeBilinearPatchWrongFlagsPD();
    ASSERT_TRUE(surface->IsUClosed());
    ASSERT_TRUE(surface->IsVClosed());

    // 同一値の再設定でも再計算が走り、幾何由来の値に更新される
    surface->SetControlPointAt(0, 0, Vector3d(0.0, 0.0, 0.0));
    EXPECT_FALSE(surface->IsUClosed());
    EXPECT_FALSE(surface->IsVClosed());
}

// 制御点全体の差し替え (get-modify-setの往復)
TEST(RationalBSplineSurfaceSetterTest, SetControlPoints_ReplacesAll) {
    const auto surface = MakeDeg21RationalPatchPD();
    igesio::Matrix3Xd cps = surface->ControlPoints();
    cps.col(0) = Vector3d(9.0, 9.0, 9.0);  // col 0 = P(0,0)
    surface->SetControlPoints(cps);

    ExpectVectorNear(surface->ControlPointAt(0, 0), Vector3d(9.0, 9.0, 9.0));
    ExpectVectorNear(surface->ControlPointAt(1, 0), Vector3d(1.0, 1.0, 0.0));
}

// 列数が異なる場合はEntityValueErrorで状態不変
TEST(RationalBSplineSurfaceSetterTest,
     SetControlPoints_ThrowsWhenColumnCountDiffers) {
    const auto surface = MakeDeg21RationalPatchPD();  // 3×2 = 6列
    igesio::Matrix3Xd wrong(3, 5);
    for (int c = 0; c < 5; ++c) wrong.col(c) = Vector3d(0.0, 0.0, 0.0);

    EXPECT_THROW(surface->SetControlPoints(wrong), igesio::EntityValueError);
    ExpectVectorNear(surface->ControlPointAt(0, 0), Vector3d(1.0, 0.0, 0.0));
}

// 重みの変更と多項式フラグ (PROP3) の再計算
TEST(RationalBSplineSurfaceSetterTest, SetWeightAt_UpdatesWeightAndPolynomialFlag) {
    const auto tube = MakeClosedSquareTubePD();  // 全重み1 (PROP3=1)
    ASSERT_TRUE(tube->IsPolynomial());

    tube->SetWeightAt(1, 1, 2.0);
    EXPECT_NEAR(tube->WeightAt(1, 1), 2.0, kTol);
    EXPECT_FALSE(tube->IsPolynomial());
    // 境界 (i=0, i=4) の重みは不変のため閉フラグは維持される
    EXPECT_TRUE(tube->IsUClosed());

    tube->SetWeightAt(1, 1, 1.0);
    EXPECT_TRUE(tube->IsPolynomial());
}

// 範囲外・非正の重みは例外で状態不変
TEST(RationalBSplineSurfaceSetterTest, SetWeightAt_ThrowsAndStateUnchanged) {
    const auto surface = MakeDeg21RationalPatchPD();  // 3×2
    EXPECT_THROW(surface->SetWeightAt(3, 0, 1.0), std::out_of_range);
    EXPECT_THROW(surface->SetWeightAt(0, 2, 1.0), std::out_of_range);
    EXPECT_THROW(surface->SetWeightAt(0, 0, 0.0), igesio::EntityValueError);
    EXPECT_THROW(surface->SetWeightAt(0, 0, -1.0), igesio::EntityValueError);
    EXPECT_NEAR(surface->WeightAt(0, 0), 1.0, kTol);
}

// 重み全体の差し替え (全要素が等しくなればPROP3=1に更新)
TEST(RationalBSplineSurfaceSetterTest, SetWeights_ReplacesAll) {
    const auto surface = MakeDeg21RationalPatchPD();  // rational (W(1,j)=2)
    ASSERT_FALSE(surface->IsPolynomial());

    igesio::MatrixXd weights(3, 2);
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 2; ++j) weights(i, j) = 5.0;
    }
    surface->SetWeights(weights);

    EXPECT_NEAR(surface->WeightAt(1, 0), 5.0, kTol);
    EXPECT_TRUE(surface->IsPolynomial());
}

// 形状が異なる場合 (転置を含む) はEntityValueError
TEST(RationalBSplineSurfaceSetterTest, SetWeights_ThrowsWhenShapeDiffers) {
    const auto surface = MakeDeg21RationalPatchPD();  // 3×2
    igesio::MatrixXd transposed(2, 3);
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 3; ++j) transposed(i, j) = 1.0;
    }
    EXPECT_THROW(surface->SetWeights(transposed), igesio::EntityValueError);
    EXPECT_NEAR(surface->WeightAt(1, 0), 2.0, kTol);  // 状態不変
}

// 非正の重みを含む場合はEntityValueError
TEST(RationalBSplineSurfaceSetterTest, SetWeights_ThrowsWhenNonPositive) {
    const auto surface = MakeDeg21RationalPatchPD();
    igesio::MatrixXd weights(3, 2);
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 2; ++j) weights(i, j) = 1.0;
    }
    weights(2, 1) = 0.0;
    EXPECT_THROW(surface->SetWeights(weights), igesio::EntityValueError);
}

// uノットの差し替え
TEST(RationalBSplineSurfaceSetterTest, SetUKnots_ReplacesAll) {
    const auto surface = MakeDeg21RationalPatchPD();
    const std::vector<double> knots{0.0, 0.0, 0.0, 2.0, 2.0, 2.0};
    surface->SetUKnots(knots);
    ExpectVectorsNear(surface->UKnots(), knots);
    ExpectVectorsNear(surface->VKnots(), {0.0, 0.0, 1.0, 1.0});  // v側は不変
}

// uノットのサイズ違い (vノットのサイズを渡す = 方向取り違え)・降順は例外
TEST(RationalBSplineSurfaceSetterTest, SetUKnots_ThrowsWhenInvalid) {
    const auto surface = MakeDeg21RationalPatchPD();  // uノットは6個
    EXPECT_THROW(surface->SetUKnots({0.0, 0.0, 1.0, 1.0}),
                 igesio::EntityValueError);
    EXPECT_THROW(surface->SetUKnots({0.0, 0.0, 1.0, 0.5, 1.0, 1.0}),
                 igesio::EntityValueError);
    ExpectVectorsNear(surface->UKnots(), {0.0, 0.0, 0.0, 1.0, 1.0, 1.0});
}

// vノットの差し替え
TEST(RationalBSplineSurfaceSetterTest, SetVKnots_ReplacesAll) {
    const auto surface = MakeDeg21RationalPatchPD();
    const std::vector<double> knots{0.0, 0.0, 3.0, 3.0};
    surface->SetVKnots(knots);
    ExpectVectorsNear(surface->VKnots(), knots);
    ExpectVectorsNear(surface->UKnots(),
                      {0.0, 0.0, 0.0, 1.0, 1.0, 1.0});  // u側は不変
}

// vノットのサイズ違い (uノットのサイズを渡す)・降順は例外
TEST(RationalBSplineSurfaceSetterTest, SetVKnots_ThrowsWhenInvalid) {
    const auto surface = MakeDeg21RationalPatchPD();  // vノットは4個
    EXPECT_THROW(surface->SetVKnots({0.0, 0.0, 0.0, 1.0, 1.0, 1.0}),
                 igesio::EntityValueError);
    EXPECT_THROW(surface->SetVKnots({0.0, 1.0, 0.5, 1.0}),
                 igesio::EntityValueError);
    ExpectVectorsNear(surface->VKnots(), {0.0, 0.0, 1.0, 1.0});
}

// パラメータ範囲の変更
TEST(RationalBSplineSurfaceSetterTest, SetParameterRange_UpdatesRange) {
    const auto surface = MakeDeg21RationalPatchPD();
    surface->SetParameterRange({0.2, 0.8, 0.1, 0.9});

    const auto range = surface->GetParameterRange();
    EXPECT_NEAR(range[0], 0.2, kTol);
    EXPECT_NEAR(range[1], 0.8, kTol);
    EXPECT_NEAR(range[2], 0.1, kTol);
    EXPECT_NEAR(range[3], 0.9, kTol);
}

// 不正な範囲 (U(0)>=U(1), V(0)>=V(1)) は例外で状態不変
TEST(RationalBSplineSurfaceSetterTest, SetParameterRange_ThrowsWhenInvalid) {
    const auto surface = MakeDeg21RationalPatchPD();
    EXPECT_THROW(surface->SetParameterRange({0.5, 0.5, 0.0, 1.0}),
                 igesio::EntityValueError);
    EXPECT_THROW(surface->SetParameterRange({0.0, 1.0, 0.9, 0.1}),
                 igesio::EntityValueError);
    const auto range = surface->GetParameterRange();
    EXPECT_NEAR(range[0], 0.0, kTol);
    EXPECT_NEAR(range[1], 1.0, kTol);
}

// 範囲を部分区間に制限すると閉フラグが解除され、全域に戻すと復帰する
// (部分区間ではサンプル評価パスで判定される)
TEST(RationalBSplineSurfaceSetterTest,
     SetParameterRange_SubRange_RecomputesClosedness) {
    const auto tube = MakeClosedSquareTubePD();
    ASSERT_TRUE(tube->IsUClosed());

    // S(0.5,v)とS(3.5,v)は異なる点のため、uは閉でなくなる
    tube->SetParameterRange({0.5, 3.5, 0.0, 1.0});
    EXPECT_FALSE(tube->IsUClosed());

    // 全域に戻すと再び閉と判定される
    tube->SetParameterRange({0.0, 4.0, 0.0, 1.0});
    EXPECT_TRUE(tube->IsUClosed());
}

// 周期フラグのトグル (u/v独立)
TEST(RationalBSplineSurfaceSetterTest, SetUPeriodic_SetVPeriodic_TogglesFlag) {
    const auto surface = MakeBilinearPatchPD();
    ASSERT_FALSE(surface->IsUPeriodic());
    ASSERT_FALSE(surface->IsVPeriodic());

    surface->SetUPeriodic(true);
    EXPECT_TRUE(surface->IsUPeriodic());
    EXPECT_FALSE(surface->IsVPeriodic());

    surface->SetVPeriodic(true);
    surface->SetUPeriodic(false);
    EXPECT_FALSE(surface->IsUPeriodic());
    EXPECT_TRUE(surface->IsVPeriodic());
}

// 曲面の種類 (フォーム番号) の設定
TEST(RationalBSplineSurfaceSetterTest, SetSurfaceType_UpdatesFormNumber) {
    const auto surface = MakeBilinearPatchPD();
    ASSERT_EQ(surface->GetSurfaceType(),
              RationalBSplineSurfaceType::kUndetermined);

    EXPECT_TRUE(surface->SetSurfaceType(RationalBSplineSurfaceType::kPlane));
    EXPECT_EQ(surface->GetSurfaceType(), RationalBSplineSurfaceType::kPlane);

    EXPECT_TRUE(surface->SetSurfaceType(
        RationalBSplineSurfaceType::kGeneralQuadricSurface));
    EXPECT_EQ(surface->GetSurfaceType(),
              RationalBSplineSurfaceType::kGeneralQuadricSurface);
}

// 無効な種類はfalseを返しフォーム番号は不変
TEST(RationalBSplineSurfaceSetterTest, SetSurfaceType_InvalidValue_ReturnsFalse) {
    const auto surface = MakeBilinearPatchPD();
    ASSERT_TRUE(surface->SetSurfaceType(RationalBSplineSurfaceType::kPlane));

    EXPECT_FALSE(surface->SetSurfaceType(
        static_cast<RationalBSplineSurfaceType>(10)));
    EXPECT_EQ(surface->GetSurfaceType(), RationalBSplineSurfaceType::kPlane);
}



/**
 * SetData (一括差し替え) のテスト
 */

// 構造全体の差し替え (2×2双線形 → 3×2有理); IDとフォーム番号は保持される
TEST(RationalBSplineSurfaceSetDataTest, SetData_ReplacesWholeStructure_KeepsID) {
    const auto surface = MakeBilinearPatchPD();
    const auto id_before = surface->id_;

    std::vector<std::vector<Vector3d>> grid(3);
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 2; ++j) {
            grid[i].push_back(Vector3d(1.0 * i, 1.0 * j, 2.0 * i * j));
        }
    }
    const std::vector<double> u_knots{0.0, 0.0, 0.0, 1.0, 1.0, 1.0};
    const std::vector<double> v_knots{0.0, 0.0, 1.0, 1.0};
    const std::vector<std::vector<double>> weights{
        {1.0, 1.0}, {2.0, 2.0}, {1.0, 1.0}};
    surface->SetData({2, 1}, u_knots, v_knots, weights, grid,
                     std::array<double, 4>{0.0, 1.0, 0.0, 1.0});

    EXPECT_EQ(surface->Degrees(), std::make_pair(2u, 1u));
    EXPECT_EQ(surface->NumControlPoints(), std::make_pair(3u, 2u));
    ExpectVectorsNear(surface->UKnots(), u_knots);
    ExpectVectorsNear(surface->VKnots(), v_knots);
    EXPECT_NEAR(surface->WeightAt(1, 1), 2.0, kTol);
    ExpectVectorNear(surface->ControlPointAt(2, 1), grid[2][1]);
    EXPECT_FALSE(surface->IsPolynomial());
    EXPECT_TRUE(surface->ValidatePD().is_valid);
    EXPECT_TRUE(surface->id_ == id_before);
    EXPECT_EQ(surface->GetSurfaceType(),
              RationalBSplineSurfaceType::kUndetermined);
}

// 範囲省略時はノット定義域となる
TEST(RationalBSplineSurfaceSetDataTest, SetData_OmittedRange_UsesKnotDomain) {
    const auto surface = MakeBilinearPatchPD();
    std::vector<std::vector<Vector3d>> grid(3);
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 2; ++j) {
            grid[i].push_back(Vector3d(1.0 * i, 1.0 * j, 0.0));
        }
    }
    surface->SetData({1, 1}, {0.0, 0.0, 1.0, 4.0, 4.0}, {0.0, 0.0, 2.0, 2.0},
                     {}, grid);

    const auto range = surface->GetParameterRange();
    EXPECT_NEAR(range[0], 0.0, kTol);
    EXPECT_NEAR(range[1], 4.0, kTol);
    EXPECT_NEAR(range[2], 0.0, kTol);
    EXPECT_NEAR(range[3], 2.0, kTol);
}

// 重み省略時は全て1.0 (polynomial形式) となる
TEST(RationalBSplineSurfaceSetDataTest, SetData_EmptyWeights_AllOnes) {
    const auto surface = MakeDeg21RationalPatchPD();  // rational
    ASSERT_FALSE(surface->IsPolynomial());

    const FactoryArgs args;
    surface->SetData({1, 1}, args.u_knots, args.v_knots, {}, args.grid);

    for (unsigned int i = 0; i < 2; ++i) {
        for (unsigned int j = 0; j < 2; ++j) {
            EXPECT_NEAR(surface->WeightAt(i, j), 1.0, kTol);
        }
    }
    EXPECT_TRUE(surface->IsPolynomial());
}

// 差し替え後のデータから閉フラグが再計算される
TEST(RationalBSplineSurfaceSetDataTest, SetData_RecomputesClosedness) {
    const auto surface = MakeBilinearPatchPD();  // 開 (PROP1=PROP2=0)
    ASSERT_FALSE(surface->IsUClosed());

    surface->SetData({1, 1}, TubeLoopKnots(), LineKnots(), {},
                     MakeUClosedTubeGrid());
    EXPECT_TRUE(surface->IsUClosed());
    EXPECT_FALSE(surface->IsVClosed());
}

// 不正データでは例外を送出し、状態は一切変更されない (強い例外保証)
TEST(RationalBSplineSurfaceSetDataTest,
     SetData_ThrowsEntityValueErrorWhenInvalid_StateUnchanged) {
    const auto surface = MakeDeg21RationalPatchPD();

    // uノット数不正 (2×2格子の次数{1,1}なら4個必要だが3個)
    const FactoryArgs args;
    EXPECT_THROW(
        surface->SetData({1, 1}, {0.0, 0.0, 1.0}, args.v_knots, {}, args.grid),
        igesio::EntityValueError);

    // 全メンバが差し替え前のまま
    EXPECT_EQ(surface->Degrees(), std::make_pair(2u, 1u));
    EXPECT_EQ(surface->NumControlPoints(), std::make_pair(3u, 2u));
    ExpectVectorsNear(surface->UKnots(), {0.0, 0.0, 0.0, 1.0, 1.0, 1.0});
    ExpectVectorsNear(surface->VKnots(), {0.0, 0.0, 1.0, 1.0});
    EXPECT_NEAR(surface->WeightAt(1, 0), 2.0, kTol);
    ExpectVectorNear(surface->ControlPointAt(1, 0), Vector3d(1.0, 1.0, 0.0));
    const auto range = surface->GetParameterRange();
    EXPECT_NEAR(range[0], 0.0, kTol);
    EXPECT_NEAR(range[1], 1.0, kTol);
}
