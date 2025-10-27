/**
 * @file entities/interfaces/test_i_curve.cpp
 * @brief entities/interfaces/i_curve.hのテスト
 * @author Yayoi Habami
 * @date 2025-10-24
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <string>
#include <tuple>

#include "igesio/numerics/tolerance.h"
#include "igesio/entities/interfaces/i_curve.h"
#include "./curves_for_testing.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using i_ent::ICurve;
using igesio::Vector3d;

/// @brief INSTANTIATE_TEST_SUITE_P用のテストパラメータ構造体
struct TestParamT {
    /// @brief テストケースの説明
    std::string desc;
    /// @brief パラメータ値 r ∈ [0, 1]
    /// @note 実際には r * (t_max - t_min) + t_min の値を使用すること
    double r;

    TestParamT(const std::string& d, const double ratio)
            : desc(d), r(ratio) {}
};
/// @brief 出力用のストリーム演算子
inline std::ostream& operator<<(std::ostream& os, const TestParamT& param) {
    return os << param.desc;
}

/// @brief パラメータ値t,t±εを計算する
/// @return (t, t-ε, t+ε) のタプル
std::tuple<double, double, double> ClampToRange(
        const std::array<double, 2>& range,
        const double r, const double epsilon) {
    // パラメータ範囲が無限大の場合、適当な大きな値に置き換える
    auto [tmin, tmax] = range;
    if (tmin == -std::numeric_limits<double>::infinity()) {
        tmin = -1e8;
    }
    if (tmax == std::numeric_limits<double>::infinity()) {
        tmax = 1e8;
    }

    double t = r * (tmax - tmin) + tmin;
    double t_minus = t - epsilon;
    double t_plus  = t + epsilon;

    // t±εが範囲外の場合、範囲内に収める
    // NOTE t±εの両方が範囲外になる場合は考慮しない
    if (t_minus < tmin) {
        t += (tmin - t_minus);
        t_plus += (tmin - t_minus);
        t_minus = tmin;
    } else if (t_plus > tmax) {
        t -= (t_plus - tmax);
        t_minus -= (t_plus - tmax);
        t_plus = tmax;
    }

    return {t, t_minus, t_plus};
}

}  // namespace

/// @brief `TryGetDerivatives`用のテストフィクスチャ
class ICurveDerivativesTest : public ::testing::TestWithParam<::TestParamT> {
 protected:
    /// @brief テスト対象のICurveオブジェクト
    inline static igesio::tests::curve_vec curves_;
    /// @brief テストの初回実行時に１度だけ呼ばれる
    static void SetUpTestSuite() {
        try {
            curves_ = igesio::tests::CreateAllTestCurves();
        } catch (const std::exception& e) {
            GTEST_FAIL() << "Failed to create test curves: " << e.what();
        } catch (...) {
            GTEST_FAIL() << "Failed to create test curves: unknown error.";
        }
    }
};

// C(t), C'(t), C''(t)の連続性のテスト
TEST_P(ICurveDerivativesTest, ContinuityOrder) {
    // この回のテストで使用するパラメータ値tを取得
    const auto param = GetParam();

    // 許容誤差
    double tol = 1e-6;
    // 微分用の小さな値ε (差分を取るだけなので小さめに設定)
    double epsilon = 1e-10;

    if (curves_.empty())  GTEST_SKIP() << "No test curves available.";

    // すべての曲線についてテストを実行
    for (const auto& curve : curves_) {
        // SCOPED_TRACEで失敗時にどの曲線・パラメータ値で失敗したか分かるようにする
        SCOPED_TRACE("Curve: " + curve.name + ", Param: " + param.desc);
        ASSERT_TRUE(curve.curve != nullptr)
                << "Curve '" << curve.name << "' is nullptr.";
        // C0連続未満の曲線はスキップ
        if (curve.continuity_order < 0) continue;

        // t、t ± εを計算＆範囲内に収める
        auto t_range = curve.curve->GetParameterRange();
        auto [t, t_minus, t_plus] = ClampToRange(t_range, param.r, epsilon);

        // C(t)を取得
        auto result_t = curve.curve->TryGetDefinedPointAt(t);
        ASSERT_TRUE(result_t.has_value())
                << "TryGetDefinedPointAt returned std::nullopt at t = " << t;

        // C(t ± ε)を取得
        auto result_t_minus = curve.curve->TryGetDefinedPointAt(t_minus);
        ASSERT_TRUE(result_t_minus.has_value())
                << "TryGetDefinedPointAt returned std::nullopt at t - ε = " << t_minus;
        auto result_t_plus = curve.curve->TryGetDefinedPointAt(t_plus);
        ASSERT_TRUE(result_t_plus.has_value())
                << "TryGetDefinedPointAt returned std::nullopt at t + ε = " << t_plus;

        // C(t)の連続性を確認
        ASSERT_TRUE(i_num::IsApproxEqual(*result_t_minus, *result_t, tol))
                << "C(t) is not continuous at t = " << t
                << "\n  C(t - ε): " << result_t_minus->transpose()
                << "\n  C(t):     " << result_t->transpose();
        ASSERT_TRUE(i_num::IsApproxEqual(*result_t_plus, *result_t, tol))
                << "C(t) is not continuous at t = " << t
                << "\n  C(t + ε): " << result_t_plus->transpose()
                << "\n  C(t):     " << result_t->transpose();

        // C'(t)の連続性を確認（C1連続以上の場合のみ）
        // -> 基本的にどの関数もnulloptを返さなければ2階微分までを計算できるため
        //    ここでまとめて取得する
        if (curve.continuity_order < 1) continue;
        auto deriv_t = curve.curve->TryGetDerivatives(t, 2);
        ASSERT_TRUE(deriv_t.has_value())
                << "TryGetDerivatives returned std::nullopt at t = " << t;
        auto deriv_t_minus = curve.curve->TryGetDerivatives(t_minus, 2);
        ASSERT_TRUE(deriv_t_minus.has_value())
                << "TryGetDerivatives returned std::nullopt at t - ε = " << t_minus;
        auto deriv_t_plus = curve.curve->TryGetDerivatives(t_plus, 2);
        ASSERT_TRUE(deriv_t_plus.has_value())
                << "TryGetDerivatives returned std::nullopt at t + ε = " << t_plus;
        // 確認
        ASSERT_TRUE(i_num::IsApproxEqual(deriv_t_minus->derivatives[1],
                                          deriv_t->derivatives[1], tol))
                << "C'(t) is not continuous at t = " << t
                << "\n  C'(t - ε): " << deriv_t_minus->derivatives[1].transpose()
                << "\n  C'(t):     " << deriv_t->derivatives[1].transpose();
        ASSERT_TRUE(i_num::IsApproxEqual(deriv_t_plus->derivatives[1],
                                          deriv_t->derivatives[1], tol))
                << "C'(t) is not continuous at t = " << t
                << "\n  C'(t + ε): " << deriv_t_plus->derivatives[1].transpose()
                << "\n  C'(t):     " << deriv_t->derivatives[1].transpose();

        // C''(t)の連続性を確認（C2連続以上の場合のみ）
        if (curve.continuity_order < 2) continue;
        ASSERT_TRUE(i_num::IsApproxEqual(deriv_t_minus->derivatives[2],
                                          deriv_t->derivatives[2], tol))
                << "C''(t) is not continuous at t = " << t
                << "\n  C''(t - ε): " << deriv_t_minus->derivatives[2].transpose()
                << "\n  C''(t):     " << deriv_t->derivatives[2].transpose();
        ASSERT_TRUE(i_num::IsApproxEqual(deriv_t_plus->derivatives[2],
                                          deriv_t->derivatives[2], tol))
                << "C''(t) is not continuous at t = " << t
                << "\n  C''(t + ε): " << deriv_t_plus->derivatives[2].transpose()
                << "\n  C''(t):     " << deriv_t->derivatives[2].transpose();
    }
}

// C'(t) のテスト
// C(t) は信頼できるものとして（個別の検証は個別のエンティティのテストで実施）、
// 数値微分で導関数を計算し、TryGetDerivativesの結果と比較する
TEST_P(ICurveDerivativesTest, TryGetFirstDerivatives) {
    // この回のテストで使用するパラメータ値tを取得
    const auto param = GetParam();

    // 許容誤差 [rad] と微分用の小さな値ε
    // ((c_+ - c_-) / (2*ε) を計算するため、やや大きめに設定)
    double tol = 1e-6;
    double epsilon = 1e-6;

    if (curves_.empty())  GTEST_SKIP() << "No test curves available.";

    // すべての曲線についてテストを実行
    for (const auto& curve : curves_) {
        // SCOPED_TRACEで失敗時にどの曲線・パラメータ値で失敗したか分かるようにする
        SCOPED_TRACE("Curve: " + curve.name + ", Param: " + param.desc);
        ASSERT_TRUE(curve.curve != nullptr)
                << "Curve '" << curve.name << "' is nullptr.";
        // C1連続未満の曲線はスキップ
        if (curve.continuity_order < 1) continue;

        // t、t ± εを計算＆範囲内に収める
        auto t_range = curve.curve->GetParameterRange();
        auto [t, t_minus, t_plus] = ClampToRange(t_range, param.r, epsilon);

        // C(t), C'(t), C''(t)を取得
        auto result_t = curve.curve->TryGetDerivatives(t, 1);
        ASSERT_TRUE(result_t.has_value())
                << "TryGetDerivatives returned std::nullopt at t = " << t;
        ASSERT_TRUE(result_t->derivatives.size() == 1 + 1)
                << "TryGetDerivatives returned incorrect number of derivatives at t = " << t;

        // 数値微分用の C(t ± ε) を取得
        auto pt_plus  = curve.curve->TryGetDefinedPointAt(t_plus);
        auto pt_minus = curve.curve->TryGetDefinedPointAt(t_minus);
        ASSERT_TRUE(pt_plus.has_value())
                << "TryGetDefinedPointAt returned std::nullopt at t + ε = " << t_plus;
        ASSERT_TRUE(pt_minus.has_value())
                << "TryGetDefinedPointAt returned std::nullopt at t - ε = " << t_minus;

        // C'(t) の数値微分を計算
        auto num_deriv_1 = (*pt_plus - *pt_minus) / (2.0 * epsilon);
        auto angle_1 = igesio::AngleBetween(result_t->derivatives[1], num_deriv_1);
        ASSERT_TRUE(i_num::IsApproxZero(angle_1, tol))
                << "First derivative mismatch at t = " << t
                << "\n  Expected: " << num_deriv_1.transpose()
                << "\n  Actual:   " << result_t->derivatives[1].transpose();
    }
}

// C''(t) のテスト
// C(t) は信頼できるものとして（個別の検証は個別のエンティティのテストで実施）、
// 数値微分で導関数を計算し、TryGetDerivativesの結果と比較する
TEST_P(ICurveDerivativesTest, TryGetSecondDerivatives) {
    // この回のテストで使用するパラメータ値tを取得
    const auto param = GetParam();

    // 許容誤差 [rad] と微分用の小さな値ε
    //  ((c_+ - 2*c' + c_-) / (ε^2) を計算するため、大きめに設定)
    double tol = 1e-3;
    double epsilon = 1e-6;

    if (curves_.empty())  GTEST_SKIP() << "No test curves available.";

    // すべての曲線についてテストを実行
    for (const auto& curve : curves_) {
        // SCOPED_TRACEで失敗時にどの曲線・パラメータ値で失敗したか分かるようにする
        SCOPED_TRACE("Curve: " + curve.name + ", Param: " + param.desc);
        ASSERT_TRUE(curve.curve != nullptr) << "Curve '" << curve.name << "' is nullptr.";
        // C2連続未満の曲線はスキップ
        if (curve.continuity_order < 2) continue;

        // t、t ± εを計算＆範囲内に収める
        auto t_range = curve.curve->GetParameterRange();
        auto [t, t_minus, t_plus] = ClampToRange(t_range, param.r, epsilon);

        // C(t), C'(t), C''(t)を取得
        auto result_t = curve.curve->TryGetDerivatives(t, 2);
        ASSERT_TRUE(result_t.has_value())
                << "TryGetDerivatives returned std::nullopt at t = " << t;
        ASSERT_TRUE(result_t->derivatives.size() == 2 + 1)
                << "TryGetDerivatives returned incorrect number of derivatives at t = " << t;

        // 数値微分用の C(t ± ε) を取得
        auto pt_plus  = curve.curve->TryGetDefinedPointAt(t_plus);
        auto pt_minus = curve.curve->TryGetDefinedPointAt(t_minus);
        ASSERT_TRUE(pt_plus.has_value())
                << "TryGetDefinedPointAt returned std::nullopt at t + ε = " << t_plus;
        ASSERT_TRUE(pt_minus.has_value())
                << "TryGetDefinedPointAt returned std::nullopt at t - ε = " << t_minus;

        // C''(t) の数値微分を計算
        auto num_deriv_2 = (*pt_plus - 2.0 * result_t->derivatives[0] + *pt_minus)
                         / (epsilon * epsilon);
        auto angle_2 = igesio::AngleBetween(result_t->derivatives[2], num_deriv_2);
        ASSERT_TRUE(i_num::IsApproxZero(angle_2, tol))
                << "Second derivative mismatch at t = " << t
                << "\n  Expected: " << num_deriv_2.transpose()
                << "\n  Actual:   " << result_t->derivatives[2].transpose();
    }
}

// テストパラメータの定義
INSTANTIATE_TEST_SUITE_P(
        ICurveTests,
        ICurveDerivativesTest,
        ::testing::Values(TestParamT("Start_r0", 0.0),
                          TestParamT("End_r1", 1.0),
                          TestParamT("Mid_r05", 0.5),
                          TestParamT("Quarter_r025", 0.25),
                          TestParamT("ThreeQuarters_r075", 0.75)),
        testing::PrintToStringParamName()
);
