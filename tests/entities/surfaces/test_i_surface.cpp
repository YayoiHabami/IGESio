/**
 * @file entities/interfaces/test_i_surface.cpp
 * @brief entities/interfaces/i_surface.hのテスト
 * @author Yayoi Habami
 * @date 2025-10-26
 * @copyright 2025 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <memory>
#include <string>
#include <tuple>

#include "igesio/numerics/tolerance.h"
#include "igesio/entities/interfaces/i_surface.h"
#include "igesio/entities/curves/circular_arc.h"
#include "igesio/entities/curves/line.h"
#include "igesio/entities/surfaces/surface_of_revolution.h"
#include "./surfaces_for_testing.h"

namespace {

namespace i_ent = igesio::entities;
namespace i_test = igesio::tests;
using i_ent::ISurface;
using igesio::Vector2d;
using igesio::Vector3d;

/// @brief INSTANTIATE_TEST_SUITE_P用のテストパラメータ構造体
struct TestParamT {
    /// @brief テストケースの説明
    std::string desc;
    /// @brief パラメータ値 ru ∈ [0, 1]
    /// @note 実際には ru * (u_max - u_min) + u_min の値を使用すること
    double ru;
    /// @brief パラメータ値 rv ∈ [0, 1]
    /// @note 実際には rv * (v_max - v_min) + v_min の値を使用すること
    double rv;

    TestParamT(const std::string& d, const double ratio_u, const double ratio_v)
            : desc(d), ru(ratio_u), rv(ratio_v) {}
};
/// @brief 出力用のストリーム演算子
inline std::ostream& operator<<(std::ostream& os, const TestParamT& param) {
    return os << param.desc;
}

/// @brief パラメータ値t,t±εを計算する
/// @return (t, t-ε, t+ε) のタプル
std::tuple<double, double, double> ClampToRange(
        const double tmin, const double tmax,
        const double r, const double epsilon) {
    // パラメータ範囲が無限大の場合、適当な大きな値に置き換える
    double t_min = tmin, t_max = tmax;
    if (t_min == -std::numeric_limits<double>::infinity()) {
        t_min = -1e8;
    }
    if (t_max == std::numeric_limits<double>::infinity()) {
        t_max = 1e8;
    }

    double t = r * (t_max - t_min) + t_min;
    double t_minus = t - epsilon;
    double t_plus  = t + epsilon;

    // t±εが範囲外の場合、範囲内に収める
    // NOTE t±εの両方が範囲外になる場合は考慮しない
    if (t_minus < t_min) {
        t += (t_min - t_minus);
        t_plus += (t_min - t_minus);
        t_minus = t_min;
    } else if (t_plus > t_max) {
        t -= (t_plus - t_max);
        t_minus -= (t_plus - t_max);
        t_plus = t_max;
    }

    return {t, t_minus, t_plus};
}

/// @brief S(u,v) のn階偏導関数を計算し、連続性をチェックする
/// @param surface テスト対象のISurfaceオブジェクト
/// @param u_values uパラメータ値のタプル (u, u-ε, u+ε)
/// @param v_values vパラメータ値のタプル (v, v-ε, v+ε)
/// @param n 階数. nu + nv = n を満たす全ての組み合わせについてチェックを行う
/// @param tol 連続性の許容誤差
void CheckSurfaceDerivativesContinuity(
        const i_test::TestSurface& surface,
        const std::tuple<double, double, double>& u_values,
        const std::tuple<double, double, double>& v_values,
        const unsigned int n, const double tol) {
    auto [u, u_minus, u_plus] = u_values;
    auto [v, v_minus, v_plus] = v_values;

    // S(u,v) のn階偏導関数を取得
    auto d_center_opt = surface.surface->TryGetDerivatives(u, v, n);
    ASSERT_TRUE(d_center_opt.has_value())
        << "Failed to get derivatives at (u,v)=(" << u << ", " << v
        << ") for surface: " << surface.name;
    const auto& d_center = d_center_opt.value();
    ASSERT_EQ(d_center.Order(), n)
        << "Unexpected order of derivatives at (u,v)=(" << u << ", " << v
        << ") for surface: " << surface.name;

    // (u±ε, v±ε)でのn階偏導関数を取得
    auto d_right_opt = surface.surface->TryGetDerivatives(u_plus, v, n);
    auto d_left_opt  = surface.surface->TryGetDerivatives(u_minus, v, n);
    auto d_up_opt    = surface.surface->TryGetDerivatives(u, v_plus, n);
    auto d_down_opt  = surface.surface->TryGetDerivatives(u, v_minus, n);
    ASSERT_TRUE(!d_right_opt || d_left_opt || d_up_opt || d_down_opt)
        << "Failed to get derivatives at neighboring points of (u,v)=(" << u
        << ", " << v << ") for surface: " << surface.name;
    const auto& d_right = d_right_opt.value();
    const auto& d_left  = d_left_opt.value();
    const auto& d_up    = d_up_opt.value();
    const auto& d_down  = d_down_opt.value();
    ASSERT_TRUE(d_right.Order() == n || d_left.Order() == n ||
                d_up.Order() == n || d_down.Order() == n)
        << "Unexpected order of derivatives at neighboring points of (u,v)=("
        << u << ", " << v << ") for surface: " << surface.name;

    // 各偏導関数について連続性をチェック
    for (unsigned int nu = 0; nu <= n; ++nu) {
        unsigned int nv = n - nu;
        std::string prefix = "Discontinuity detected in derivative S^(" +
                             std::to_string(nu) + "," + std::to_string(nv) + ") at S(u,v)= S(" +
                             std::to_string(u) + ", " + std::to_string(v) + ") = " +
                             ToString(d_center(nu, nv).transpose()) + " / ";
        std::string suffix = " for surface: " + surface.name;

        double dist = (d_right(nu, nv) - d_center(nu, nv)).norm();
        EXPECT_LE(dist, tol) << prefix << "S(u+ε, v) = S(" << u_plus << ", " << v << ") = "
            << ToString(d_right(nu, nv).transpose()) << " (dist = " << dist << ")" << suffix;

        dist = (d_left(nu, nv) - d_center(nu, nv)).norm();
        EXPECT_LE(dist, tol) << prefix << "S(u-ε, v) = S(" << u_minus << ", " << v << ") = "
            << ToString(d_left(nu, nv).transpose()) << " (dist = " << dist << ")" << suffix;

        dist = (d_up(nu, nv) - d_center(nu, nv)).norm();
        EXPECT_LE(dist, tol) << prefix << "S(u, v+ε) = S(" << u << ", " << v_plus << ") = "
            << ToString(d_up(nu, nv).transpose()) << " (dist = " << dist << ")" << suffix;

        dist = (d_down(nu, nv) - d_center(nu, nv)).norm();
        EXPECT_LE(dist, tol) << prefix << "S(u, v-ε) = S(" << u << ", " << v_minus << ") = "
            << ToString(d_down(nu, nv).transpose()) << " (dist = " << dist << ")" << suffix;
    }
}

/// @brief S(u, v) のn階偏導関数と数値微分を比較する
/// @param surface テスト対象のISurfaceオブジェクト
/// @param u_values uパラメータ値のタプル (u, u-ε, u+ε)
/// @param v_values vパラメータ値のタプル (v, v-ε, v+ε)
/// @param n 階数. nu + nv = n を満たす全ての組み合わせについてチェックを行う
/// @param tol 許容誤差
void CheckSurfaceDerivativesNumerical(
        const i_test::TestSurface& surface,
        const std::tuple<double, double, double>& u_values,
        const std::tuple<double, double, double>& v_values,
        const unsigned int n, const double tol) {
    ASSERT_TRUE(n <= 2) << "Numerical differentiation is only implemented for n <= 2.";

    auto [u, u_minus, u_plus] = u_values;
    auto [v, v_minus, v_plus] = v_values;
    auto du = u_plus - u, dv = v_plus - v;

    // S(u,v) のn階偏導関数を取得
    auto d_center_opt = surface.surface->TryGetDerivatives(u, v, n);
    ASSERT_TRUE(d_center_opt.has_value())
        << "Failed to get derivatives at (u,v)=(" << u << ", " << v
        << ") for surface: " << surface.name;
    const auto& d_center = d_center_opt.value();
    ASSERT_EQ(d_center.Order(), n)
        << "Unexpected order of derivatives at (u,v)=(" << u << ", " << v
        << ") for surface: " << surface.name;

    // (u±ε, v±ε)での点の座標を取得 (数値微分用)
    auto p_right_opt = surface.surface->TryGetDefinedPointAt(u_plus, v);
    auto p_left_opt  = surface.surface->TryGetDefinedPointAt(u_minus, v);
    auto p_up_opt    = surface.surface->TryGetDefinedPointAt(u, v_plus);
    auto p_down_opt  = surface.surface->TryGetDefinedPointAt(u, v_minus);
    ASSERT_TRUE(p_right_opt.has_value() || p_left_opt.has_value() ||
                p_up_opt.has_value() || p_down_opt.has_value())
        << "Failed to get points at neighboring parameters of (u,v)=(" << u
        << ", " << v << ") for surface: " << surface.name;
    const auto& p_right = p_right_opt.value();
    const auto& p_left  = p_left_opt.value();
    const auto& p_up    = p_up_opt.value();
    const auto& p_down  = p_down_opt.value();

    // 各偏導関数について数値微分と比較
    if (n == 1) {
        // Su(u, v), Sv(u, v)
        const auto& su = d_center(1, 0);
        const auto& sv = d_center(0, 1);

        // Su = (S(u+ε, v) - S(u-ε, v)) / (2ε)
        const Vector3d su_numerical = (p_right - p_left) / (u_plus - u_minus);
        EXPECT_LE((su - su_numerical).norm(), tol)
            << "Mismatch in Su at (u,v)=(" << u << ", " << v << ") for surface: " << surface.name
            << "\n  Analytical: " << su.transpose()
            << "\n  Numerical:  " << su_numerical.transpose();

        // Sv = (S(u, v+ε) - S(u, v-ε)) / (2ε)
        const Vector3d sv_numerical = (p_up - p_down) / (v_plus - v_minus);
        EXPECT_LE((sv - sv_numerical).norm(), tol)
            << "Mismatch in Sv at (u,v)=(" << u << ", " << v << ") for surface: " << surface.name
            << "\n  Analytical: " << sv.transpose()
            << "\n  Numerical:  " << sv_numerical.transpose();
    } else if (n == 2) {
        // Suu(u, v), Suv(u, v), Svv(u, v)
        const auto& suu = d_center(2, 0);
        const auto& suv = d_center(1, 1);
        const auto& svv = d_center(0, 2);

        // (u±ε, v±ε)での1階偏導関数を取得
        auto d_right_opt = surface.surface->TryGetDerivatives(u_plus, v, 1);
        auto d_left_opt  = surface.surface->TryGetDerivatives(u_minus, v, 1);
        auto d_up_opt    = surface.surface->TryGetDerivatives(u, v_plus, 1);
        auto d_down_opt  = surface.surface->TryGetDerivatives(u, v_minus, 1);
        ASSERT_TRUE(d_right_opt.has_value() && d_left_opt.has_value() &&
                    d_up_opt.has_value() && d_down_opt.has_value())
            << "Failed to get 1st derivatives at neighboring points of (u,v)=(" << u
            << ", " << v << ") for surface: " << surface.name;

        const auto& d_right = d_right_opt.value();
        const auto& d_left  = d_left_opt.value();
        const auto& d_up    = d_up_opt.value();
        const auto& d_down  = d_down_opt.value();

        // Suu = (Su(u+ε, v) - Su(u-ε, v)) / (2ε)
        const Vector3d suu_numerical = (d_right(1, 0) - d_left(1, 0)) / (u_plus - u_minus);
        EXPECT_LE((suu - suu_numerical).norm(), tol)
            << "Mismatch in Suu at (u,v)=(" << u << ", " << v << ") for surface: " << surface.name
            << "\n  Analytical: " << suu.transpose()
            << "\n  Numerical:  " << suu_numerical.transpose();

        // Suv = (Su(u, v+ε) - Su(u, v-ε)) / (2ε)
        const Vector3d suv_numerical = (d_up(1, 0) - d_down(1, 0)) / (v_plus - v_minus);
        EXPECT_LE((suv - suv_numerical).norm(), tol)
            << "Mismatch in Suv at (u,v)=(" << u << ", " << v << ") for surface: " << surface.name
            << "\n  Analytical: " << suv.transpose()
            << "\n  Numerical:  " << suv_numerical.transpose();

        // Svv = (Sv(u, v+ε) - Sv(u, v-ε)) / (2ε)
        const Vector3d svv_numerical = (d_up(0, 1) - d_down(0, 1)) / (v_plus - v_minus);
        EXPECT_LE((svv - svv_numerical).norm(), tol)
            << "Mismatch in Svv at (u,v)=(" << u << ", " << v << ") for surface: " << surface.name
            << "\n  Analytical: " << svv.transpose()
            << "\n  Numerical:  " << svv_numerical.transpose();
    }
}

}  // namespace

/// @brief `TryGetDerivatives`用のテストフィクスチャ
class ISurfaceDerivativesTest : public ::testing::TestWithParam<::TestParamT> {
 protected:
    /// @brief テスト対象のISurfaceオブジェクト
    inline static igesio::tests::surface_vec surfaces_;
    /// @brief テストの初回実行時に１度だけ呼ばれる
    static void SetUpTestSuite() {
        try {
            surfaces_ = igesio::tests::CreateAllTestSurfaces();
        } catch (const std::exception& e) {
            GTEST_FAIL() << "Failed to create test surfaces: " << e.what();
        } catch (...) {
            GTEST_FAIL() << "Failed to create test surfaces: unknown error.";
        }
    }
};

// S(u,v) およびその偏導関数の連続性をチェックするテスト
TEST_P(ISurfaceDerivativesTest, ContinuityOrder) {
    // この回のテストパラメータを取得
    auto param = GetParam();

    // 許容誤差
    double tol = 1e-6;
    // 差分用の小さな値ε
    double epsilon = 1e-8;

    if (surfaces_.empty()) {
        GTEST_SKIP() << "No test surfaces available.";
    }

    // すべてのテスト対象曲面についてチェックを行う
    for (const auto& surface : surfaces_) {
        // u,vパラメータ値とその近傍値を計算
        auto range = surface.surface->GetParameterRange();
        auto u_values = ClampToRange(range[0], range[1], param.ru, epsilon);
        auto v_values = ClampToRange(range[2], range[3], param.rv, epsilon);

        SCOPED_TRACE("Surface: " + surface.name + ", Param: " + param.desc +
            " ε = " + std::to_string(epsilon) + ", tol = " + std::to_string(tol) +
            ", u_range = [" + std::to_string(range[0]) + ", " + std::to_string(range[1]) +
            "], v_range = [" + std::to_string(range[2]) + ", " + std::to_string(range[3]) + "]");

        // S(u,v) の0～2階偏導関数について連続性をチェック
        for (unsigned int n = 0; n <= 2; ++n) {
            CheckSurfaceDerivativesContinuity(surface, u_values, v_values, n, tol);
        }
    }
}

// S(u,v)の偏導関数を数値微分と比較するテスト
TEST_P(ISurfaceDerivativesTest, NumericalDerivatives) {
    // この回のテストパラメータを取得
    auto param = GetParam();

    // 許容誤差
    double tol = 1e-5;
    // 差分用の小さな値ε
    double epsilon = 1e-8;

    if (surfaces_.empty()) {
        GTEST_SKIP() << "No test surfaces available.";
    }

    // すべてのテスト対象曲面についてチェックを行う
    for (const auto& surface : surfaces_) {
        // u,vパラメータ値とその近傍値を計算
        auto range = surface.surface->GetParameterRange();
        auto u_values = ClampToRange(range[0], range[1], param.ru, epsilon);
        auto v_values = ClampToRange(range[2], range[3], param.rv, epsilon);

        SCOPED_TRACE("Surface: " + surface.name + ", Param: " + param.desc +
            " ε = " + std::to_string(epsilon) + ", tol = " + std::to_string(tol) +
            ", u_range = [" + std::to_string(range[0]) + ", " + std::to_string(range[1]) +
            "], v_range = [" + std::to_string(range[2]) + ", " + std::to_string(range[3]) + "]");

        // S(u,v) の1～2階偏導関数について数値微分と比較
        for (unsigned int n = 1; n <= 2; ++n) {
            CheckSurfaceDerivativesNumerical(surface, u_values, v_values, n, tol);
        }
    }
}



// テストパラメータの定義
INSTANTIATE_TEST_SUITE_P(
        ISurfaceTests, ISurfaceDerivativesTest,
        ::testing::Values(
            TestParamT("UMin_VMin", 0.0, 0.0),
            TestParamT("UQuarter_VMin", 0.25, 0.0),
            TestParamT("UHalf_VMin", 0.5, 0.0),
            TestParamT("UThreeQuarters_VMin", 0.75, 0.0),
            TestParamT("UMax_VMin", 1.0, 0.0),
            TestParamT("UMin_VQuarter", 0.0, 0.25),
            TestParamT("UQuarter_VQuarter", 0.25, 0.25),
            TestParamT("UHalf_VQuarter", 0.5, 0.25),
            TestParamT("UThreeQuarters_VQuarter", 0.75, 0.25),
            TestParamT("UMax_VQuarter", 1.0, 0.25),
            TestParamT("UMin_VHalf", 0.0, 0.5),
            TestParamT("UQuarter_VHalf", 0.25, 0.5),
            TestParamT("UHalf_VHalf", 0.5, 0.5),
            TestParamT("UThreeQuarters_VHalf", 0.75, 0.5),
            TestParamT("UMax_VHalf", 1.0, 0.5),
            TestParamT("UMin_VThreeQuarters", 0.0, 0.75),
            TestParamT("UQuarter_VThreeQuarters", 0.25, 0.75),
            TestParamT("UHalf_VThreeQuarters", 0.5, 0.75),
            TestParamT("UThreeQuarters_VThreeQuarters", 0.75, 0.75),
            TestParamT("UMax_VThreeQuarters", 1.0, 0.75),
            TestParamT("UMin_VMax", 0.0, 1.0),
            TestParamT("UQuarter_VMax", 0.25, 1.0),
            TestParamT("UHalf_VMax", 0.5, 1.0),
            TestParamT("UThreeQuarters_VMax", 0.75, 1.0),
            TestParamT("UMax_VMax", 1.0, 1.0)),
        testing::PrintToStringParamName()
);



// ISurface::Area() のテスト
TEST(ISurfaceTest, Area) {
    // テスト用の曲面で計算できることを確認
    auto surfaces = igesio::tests::CreateAllTestSurfaces();
    for (const auto& surface : surfaces) {
        SCOPED_TRACE("Surface: " + surface.name);
        ASSERT_TRUE(surface.surface != nullptr)
            << "Surface object is null for surface: " << surface.name;

        double area = -1.0;
        ASSERT_NO_THROW({
            area = surface.surface->Area();
        });
        ASSERT_GE(area, 0.0);
    }

    // 解析的に計算できるものについて個別に計算
    {
        // Y軸周りに原点中心の1/4円弧を回転させてできる曲面
        // 半径2の球の1/8面の面積 = 4πr^2 / 8 = 2π
        auto axis = std::make_shared<i_ent::Line>(
            Vector3d{0.0, 0.0, 0.0}, Vector3d{0.0, 1.0, 0.0});
        auto cir_arc = std::make_shared<i_ent::CircularArc>(
            Vector2d{0.0, 0.0}, Vector2d{2.0, 0.0}, Vector2d{0.0, 2.0});
        auto surface = std::make_shared<i_ent::SurfaceOfRevolution>(
            axis, cir_arc, 0.0, igesio::kPi / 2.0);

        auto area = surface->Area();
        ASSERT_NEAR(area, 2.0 * igesio::kPi, 1e-6)
            << "Mismatch in area for quarter sphere surface: expected "
            << (2.0 * igesio::kPi) << ", got " << area;
    }
}

// ISurface::Area(start_u, end_u, start_v, end_v) のテスト
TEST(ISurfaceTest, AreaParameterized) {
    // テスト用の曲面で計算できることを確認
    auto surfaces = igesio::tests::CreateAllTestSurfaces();
    for (const auto& surface : surfaces) {
        SCOPED_TRACE("Surface: " + surface.name);
        ASSERT_TRUE(surface.surface != nullptr)
            << "Surface object is null for surface: " << surface.name;

        // パラメータ範囲を取得
        auto param_range = surface.surface->GetParameterRange();
        double u_start = param_range[0];
        double u_end   = param_range[1];
        double v_start = param_range[2];
        double v_end   = param_range[3];

        // 全範囲での面積と一致することを確認
        double total_area = surface.surface->Area();
        double param_area = -1.0;
        ASSERT_NO_THROW({
            param_area = surface.surface->Area(u_start, u_end, v_start, v_end);
        });
        ASSERT_NEAR(param_area, total_area, 1e-6)
            << "Mismatch in area over full parameter range for surface: " << surface.name;

        // 範囲の一部での面積が計算できることを確認
        double mid_u = 0.5 * (u_start + u_end);
        double mid_v = 0.5 * (v_start + v_end);
        ASSERT_NO_THROW({
            param_area = surface.surface->Area(u_start, mid_u, v_start, mid_v);
        });
        ASSERT_GE(param_area, 0.0)
            << "Negative area for partial parameter range for surface: " << surface.name;
    }
}
