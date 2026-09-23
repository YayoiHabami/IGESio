/**
 * @file entities/curves/test_nurbs_interpolation_algorithms.cpp
 * @brief InterpolateWithNurbsのテスト
 * @author Yayoi Habami
 * @date 2026-09-23
 * @copyright 2026 Yayoi Habami
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "igesio/entities/curves/nurbs_algorithms.h"

namespace {

namespace i_ent = igesio::entities;
using Vector3d = igesio::Vector3d;
constexpr double kPi = igesio::kPi;

/// @brief 接線の列
using TangentList = std::vector<std::optional<Vector3d>>;

// =========================================================================
// テスト用ヘルパー
// =========================================================================

/// @brief 区間の曲線と弦の距離の、弦長に対する比の理論上限
/// @note 区間の3次ベジエの内側の制御点は端点から弦長の1/3の距離にあり、
///       曲線は制御点の凸包内にあるため
constexpr double kMaxChordDeviationRatio = 1.0 / 3.0;
/// @brief 1区間あたりの評価点数
constexpr int kSamplesPerSegment = 32;

/// @brief 決定的な擬似乱数を生成する (SplitMix64)
/// @note 標準ライブラリの分布は処理系により結果が異なるため使わない
class DeterministicRandom {
 public:
    /// @brief 乱数列を初期化する
    /// @param seed シード値
    explicit DeterministicRandom(std::uint64_t seed) : state_(seed) {}

    /// @brief 0.0〜1.0の一様乱数を生成する
    /// @return 0.0以上1.0未満の値
    double Uniform() {
        state_ += 0x9E3779B97F4A7C15ULL;
        std::uint64_t z = state_;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        z ^= z >> 31;
        return static_cast<double>(z >> 11) * 0x1.0p-53;
    }

    /// @brief 各成分が-1.0〜1.0の一様乱数であるベクトルを生成する
    /// @return 乱数ベクトル
    Vector3d UniformVector() {
        const double x = 2.0 * Uniform() - 1.0;
        const double y = 2.0 * Uniform() - 1.0;
        const double z = 2.0 * Uniform() - 1.0;
        return Vector3d{x, y, z};
    }

 private:
    /// @brief 内部状態
    std::uint64_t state_;
};

/// @brief 螺旋上の点を弧長sから計算する
/// @param s 弧長
/// @return 半径8、ピッチ4πの螺旋上の点
Vector3d HelixPoint(double s) {
    constexpr double kRadius = 8.0;
    constexpr double kRise = 2.0;
    const double t = s / std::hypot(kRadius, kRise);
    return Vector3d{kRadius * std::cos(t), kRadius * std::sin(t), kRise * t};
}

/// @brief 螺旋上の単位接線を弧長sから計算する
/// @param s 弧長
/// @return 単位接線
Vector3d HelixTangent(double s) {
    constexpr double kRadius = 8.0;
    constexpr double kRise = 2.0;
    const double k = std::hypot(kRadius, kRise);
    const double t = s / k;
    return Vector3d{-kRadius * std::sin(t), kRadius * std::cos(t), kRise} / k;
}

/// @brief 螺旋上に等間隔の点列を生成する
/// @param n 点数
/// @param h 弧長の間隔
/// @return 点列
std::vector<Vector3d> MakeHelixPoints(int n, double h) {
    std::vector<Vector3d> pts;
    for (int i = 0; i < n; ++i) pts.push_back(HelixPoint(i * h));
    return pts;
}

/// @brief 螺旋上に不等間隔 (間隔比最大50) の弧長列を生成する
/// @param n 点数
/// @param h 基準の間隔
/// @return 弧長列
std::vector<double> MakeUnevenArcLengths(int n, double h) {
    DeterministicRandom rng(11);
    constexpr double kSteps[] = {1.0, 1.0, 0.02, 0.3};
    std::vector<double> s{0.0};
    for (int i = 1; i < n; ++i) {
        const int pick = std::min(3, static_cast<int>(rng.Uniform() * 4.0));
        s.push_back(s.back() + h * kSteps[pick]);
    }
    return s;
}

/// @brief 入力点と、返されたパラメータでの曲線上の点の最大距離を計算する
/// @param result 補間結果
/// @param pts    入力点列
/// @return 最大距離
double MaxPassError(const i_ent::NurbsInterpolation& result,
                    const std::vector<Vector3d>& pts) {
    double err = 0.0;
    for (std::size_t i = 0; i < pts.size(); ++i) {
        const auto p = result.curve->TryGetPointAt(result.parameters[i]);
        if (!p) return std::numeric_limits<double>::infinity();
        err = std::max(err, (*p - pts[i]).norm());
    }
    return err;
}

/// @brief 各区間の曲線について幾何的な指標を集計した結果
struct SegmentStats {
    /// @brief 曲線と弦の距離の、弦長に対する比の最大
    double max_chord_deviation = 0.0;
    /// @brief 曲線長 (評価点を結んだ折れ線長)
    double curve_length = 0.0;
    /// @brief 入力点列の折れ線長
    double polyline_length = 0.0;
    /// @brief 全評価点が有限か
    bool all_finite = true;
};

/// @brief 連続する相異なる入力点の間ごとに曲線を評価して集計する
/// @param result 補間結果
/// @param pts    入力点列
/// @return 集計結果
SegmentStats MeasureSegments(const i_ent::NurbsInterpolation& result,
                             const std::vector<Vector3d>& pts) {
    SegmentStats stats;
    for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
        const double u0 = result.parameters[i];
        const double u1 = result.parameters[i + 1];
        if (u1 <= u0) continue;

        const Vector3d chord = pts[i + 1] - pts[i];
        const double h = chord.norm();
        const Vector3d dir = chord / h;
        stats.polyline_length += h;
        Vector3d prev = pts[i];
        for (int j = 1; j <= kSamplesPerSegment; ++j) {
            const double u = u0 + (u1 - u0) * j / kSamplesPerSegment;
            const auto p = result.curve->TryGetPointAt(u);
            if (!p || !p->allFinite()) {
                stats.all_finite = false;
                return stats;
            }
            const Vector3d rel = *p - pts[i];
            const double dev = (rel - rel.dot(dir) * dir).norm() / h;
            stats.max_chord_deviation = std::max(stats.max_chord_deviation, dev);
            stats.curve_length += (*p - prev).norm();
            prev = *p;
        }
    }
    return stats;
}

/// @brief パラメータuでの1階微分を計算する
/// @param result 補間結果
/// @param u      パラメータ
/// @return 1階微分 (評価に失敗した場合は非有限値)
Vector3d DerivativeAt(const i_ent::NurbsInterpolation& result, double u) {
    const auto d = result.curve->TryGetDerivatives(u, 1);
    if (!d) return Vector3d::Constant(std::numeric_limits<double>::quiet_NaN());
    return (*d)[1];
}

/// @brief 2つのベクトルのなす角を計算する
/// @param a ベクトル
/// @param b ベクトル
/// @return なす角 [rad]
double Angle(const Vector3d& a, const Vector3d& b) {
    return std::atan2(a.cross(b).norm(), a.dot(b));
}

/// @brief 点pから直線 (点aを通り方向dir) への距離を計算する
/// @param p   対象の点
/// @param a   直線上の点
/// @param dir 直線の単位方向
/// @return 距離
double DistanceToLine(const Vector3d& p, const Vector3d& a,
                      const Vector3d& dir) {
    const Vector3d rel = p - a;
    return (rel - rel.dot(dir) * dir).norm();
}

/// @brief 点列全体の座標の大きさを計算する
/// @param pts 点列
/// @return 座標の絶対値の最大 (1.0未満の場合は1.0)
double CoordinateScale(const std::vector<Vector3d>& pts) {
    double scale = 1.0;
    for (const auto& p : pts) scale = std::max(scale, p.cwiseAbs().maxCoeff());
    return scale;
}

/// @brief 螺旋上の弧長列から補間し、真の螺旋との距離を計算する
/// @param s 弧長列
/// @return {端の2区間を除く内部区間での最大距離, 全区間での最大距離}
std::pair<double, double> MeasureHelixError(const std::vector<double>& s) {
    std::vector<Vector3d> pts;
    for (const double v : s) pts.push_back(HelixPoint(v));
    const auto result = i_ent::InterpolateWithNurbs(pts);

    double interior = 0.0;
    double overall = 0.0;
    for (std::size_t i = 0; i + 1 < s.size(); ++i) {
        for (int j = 1; j < kSamplesPerSegment; ++j) {
            const double r = static_cast<double>(j) / kSamplesPerSegment;
            const double u = result.parameters[i]
                + r * (result.parameters[i + 1] - result.parameters[i]);
            const Vector3d p = *result.curve->TryGetPointAt(u);
            // 螺旋上の最近点をニュートン法で求める (螺旋は弧長で単位速さ)
            double v = s[i] + r * (s[i + 1] - s[i]);
            for (int k = 0; k < 5; ++k) {
                v += (p - HelixPoint(v)).dot(HelixTangent(v));
            }
            const double d = (p - HelixPoint(v)).norm();
            overall = std::max(overall, d);
            if (i >= 2 && i + 3 < s.size()) interior = std::max(interior, d);
        }
    }
    return {interior, overall};
}

}  // namespace



// =========================================================================
// グループA: 入力バリデーション
// =========================================================================

TEST(InterpolateWithNurbsTest, Throws_WhenFewerThanTwoPoints) {
    EXPECT_THROW(i_ent::InterpolateWithNurbs({}), std::invalid_argument);
    EXPECT_THROW(i_ent::InterpolateWithNurbs({Vector3d{1.0, 2.0, 3.0}}),
                 std::invalid_argument);
}

TEST(InterpolateWithNurbsTest, Throws_WhenAllPointsCoincide) {
    const std::vector<Vector3d> pts(5, Vector3d{1.0, 2.0, 3.0});
    EXPECT_THROW(i_ent::InterpolateWithNurbs(pts), std::invalid_argument);
}

TEST(InterpolateWithNurbsTest, Throws_WhenPointIsNotFinite) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    EXPECT_THROW(i_ent::InterpolateWithNurbs(
                     {Vector3d::Zero(), Vector3d{nan, 0.0, 0.0}}),
                 std::invalid_argument);
    EXPECT_THROW(i_ent::InterpolateWithNurbs(
                     {Vector3d::Zero(), Vector3d{0.0, inf, 0.0}}),
                 std::invalid_argument);
}

TEST(InterpolateWithNurbsTest, Throws_WhenTangentCountMismatches) {
    const auto pts = MakeHelixPoints(5, 1.0);
    const TangentList tangents(4, Vector3d::UnitX());
    EXPECT_THROW(i_ent::InterpolateWithNurbs(pts, tangents),
                 std::invalid_argument);
}

TEST(InterpolateWithNurbsTest, Throws_WhenOptionIsOutOfRange) {
    const auto pts = MakeHelixPoints(5, 1.0);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    auto with = [](auto setter) {
        i_ent::NurbsInterpOptions o;
        setter(o);
        return o;
    };
    const std::vector<i_ent::NurbsInterpOptions> invalid = {
        with([](auto& o) { o.corner_angle = 0.0; }),
        with([](auto& o) { o.corner_angle = kPi / 2.0 + 1e-9; }),
        with([&](auto& o) { o.corner_angle = nan; }),
        with([](auto& o) { o.tangent_tolerance = -1e-9; }),
        with([](auto& o) { o.tangent_tolerance = kPi / 2.0 + 1e-9; }),
        with([](auto& o) { o.duplicate_tolerance = -1e-9; }),
        with([](auto& o) {
            o.duplicate_tolerance = std::numeric_limits<double>::infinity();
        }),
    };
    for (const auto& o : invalid) {
        EXPECT_THROW(i_ent::InterpolateWithNurbs(pts, {}, o),
                     std::invalid_argument);
    }
}



// =========================================================================
// グループB: 基本的な性質
// =========================================================================

TEST(InterpolateWithNurbsTest, PassesThroughAllPoints) {
    const auto s = MakeUnevenArcLengths(300, 0.5);
    std::vector<Vector3d> pts;
    for (const double v : s) pts.push_back(HelixPoint(v));

    const auto result = i_ent::InterpolateWithNurbs(pts);
    ASSERT_NE(result.curve, nullptr);
    ASSERT_EQ(result.parameters.size(), pts.size());
    EXPECT_LE(MaxPassError(result, pts), 1e-12);
}

TEST(InterpolateWithNurbsTest, ParametersAreStrictlyIncreasingFromZeroToOne) {
    const auto pts = MakeHelixPoints(50, 0.5);
    const auto result = i_ent::InterpolateWithNurbs(pts);
    EXPECT_EQ(result.parameters.front(), 0.0);
    EXPECT_EQ(result.parameters.back(), 1.0);
    for (std::size_t i = 1; i < result.parameters.size(); ++i) {
        EXPECT_LT(result.parameters[i - 1], result.parameters[i]) << "i=" << i;
    }
    const auto range = result.curve->GetParameterRange();
    EXPECT_EQ(range[0], 0.0);
    EXPECT_EQ(range[1], 1.0);
}

TEST(InterpolateWithNurbsTest, ParametersAreNormalizedCumulativeChordLengths) {
    const auto s = MakeUnevenArcLengths(100, 0.5);
    std::vector<Vector3d> pts;
    for (const double v : s) pts.push_back(HelixPoint(v));
    const auto result = i_ent::InterpolateWithNurbs(pts);

    std::vector<double> chord{0.0};
    for (std::size_t i = 1; i < pts.size(); ++i) {
        chord.push_back(chord.back() + (pts[i] - pts[i - 1]).norm());
    }
    for (std::size_t i = 0; i < pts.size(); ++i) {
        EXPECT_NEAR(result.parameters[i], chord[i] / chord.back(), 1e-15)
            << "i=" << i;
    }
}

TEST(InterpolateWithNurbsTest, ReproducesStraightLineWithUnevenSpacing) {
    const Vector3d dir = Vector3d{1.0, 2.0, -2.0}.normalized();
    std::vector<Vector3d> pts;
    for (const double s : {0.0, 0.1, 0.15, 3.0, 3.001, 7.5, 7.6, 20.0}) {
        pts.push_back(Vector3d{5.0, -1.0, 2.0} + s * dir);
    }
    const auto result = i_ent::InterpolateWithNurbs(pts);
    for (int i = 0; i <= 2000; ++i) {
        const double u = i / 2000.0;
        const auto p = result.curve->TryGetPointAt(u);
        ASSERT_TRUE(p.has_value());
        EXPECT_LE(DistanceToLine(*p, pts[0], dir), 1e-12) << "u=" << u;
        EXPECT_LE(Angle(DerivativeAt(result, u), dir), 1e-9) << "u=" << u;
    }
}

TEST(InterpolateWithNurbsTest, TwoPointsGiveLineSegment) {
    const Vector3d a{1.0, 2.0, 3.0};
    const Vector3d b{4.0, -2.0, 3.0};
    const auto result = i_ent::InterpolateWithNurbs({a, b});
    EXPECT_EQ(result.parameters, (std::vector<double>{0.0, 1.0}));
    const auto mid = result.curve->TryGetPointAt(0.5);
    ASSERT_TRUE(mid.has_value());
    EXPECT_LE((*mid - 0.5 * (a + b)).norm(), 1e-12);
}

TEST(InterpolateWithNurbsTest, AccuracyOnSmoothCurve_Uniform) {
    // 実測: 内部6.9e-6、全体2.8e-5 (最大は端の区間で、端点接線の推定による)
    std::vector<double> s;
    for (int i = 0; i < 200; ++i) s.push_back(0.5 * i);
    const auto [interior, overall] = MeasureHelixError(s);
    EXPECT_LE(interior, 2e-5);
    EXPECT_LE(overall, 1e-4);
}

TEST(InterpolateWithNurbsTest, AccuracyOnSmoothCurve_Uneven) {
    // 実測: 内部1.1e-5、全体2.8e-5。Bessel法以外の接線推定 (弦長で重み付け
    // するCatmull-Rom型) では内部3.6e-3に悪化する
    const auto [interior, overall] =
        MeasureHelixError(MakeUnevenArcLengths(300, 0.5));
    EXPECT_LE(interior, 2e-5);
    EXPECT_LE(overall, 1e-4);
}

TEST(InterpolateWithNurbsTest, IsC1ContinuousAtSmoothJoints) {
    const auto s = MakeUnevenArcLengths(100, 0.5);
    std::vector<Vector3d> pts;
    for (const double v : s) pts.push_back(HelixPoint(v));
    const auto result = i_ent::InterpolateWithNurbs(pts);

    // 接続点の両側の1階微分が (大きさも含めて) 一致する
    constexpr double kEps = 1e-9;
    for (std::size_t i = 1; i + 1 < pts.size(); ++i) {
        const double u = result.parameters[i];
        const Vector3d left = DerivativeAt(result, u - kEps);
        const Vector3d right = DerivativeAt(result, u + kEps);
        EXPECT_LE((left - right).norm(), 1e-5 * right.norm()) << "i=" << i;
    }
}

TEST(InterpolateWithNurbsTest, ParameterIsNearlyProportionalToArcLength) {
    const auto pts = MakeHelixPoints(200, 0.5);
    const auto result = i_ent::InterpolateWithNurbs(pts);
    double v_min = std::numeric_limits<double>::infinity();
    double v_max = 0.0;
    for (int i = 0; i <= 5000; ++i) {
        const double v = DerivativeAt(result, i / 5000.0).norm();
        v_min = std::min(v_min, v);
        v_max = std::max(v_max, v);
    }
    EXPECT_LE(v_max / v_min, 1.01);
}

TEST(InterpolateWithNurbsTest, DuplicatePointsShareParameter) {
    const auto base = MakeHelixPoints(20, 0.5);
    std::vector<Vector3d> pts;
    for (const auto& p : base) {
        pts.push_back(p);
        pts.push_back(p);
        // 同一点とみなす距離以内のずれ
        pts.push_back(p + Vector3d{1e-10, 0.0, 0.0});
    }
    const auto result = i_ent::InterpolateWithNurbs(pts);
    const auto reference = i_ent::InterpolateWithNurbs(base);
    ASSERT_EQ(result.parameters.size(), pts.size());
    for (std::size_t i = 0; i < base.size(); ++i) {
        for (std::size_t j = 0; j < 3; ++j) {
            EXPECT_EQ(result.parameters[3 * i + j], reference.parameters[i]);
        }
    }
    EXPECT_LE(MaxPassError(result, pts), 1e-9);
}

TEST(InterpolateWithNurbsTest, MergesPointsBelowRelativeResolution) {
    // 折れ線長に対して倍精度の分解能を下回る間隔の点は、duplicate_tolerance=0
    // でも統合される (統合しないとノットが重なり曲線が作れない)
    auto pts = MakeHelixPoints(200, 0.5);
    pts.insert(pts.begin() + 100, pts[100] + Vector3d{4e-15, 0.0, 0.0});
    i_ent::NurbsInterpOptions options;
    options.duplicate_tolerance = 0.0;
    const auto result = i_ent::InterpolateWithNurbs(pts, {}, options);
    EXPECT_EQ(result.parameters[100], result.parameters[101]);
    EXPECT_LE(MaxPassError(result, pts), 1e-12);
    const auto stats = MeasureSegments(result, pts);
    EXPECT_TRUE(stats.all_finite);
    EXPECT_LE(stats.max_chord_deviation, kMaxChordDeviationRatio);
}



// =========================================================================
// グループC: 角点
// =========================================================================

/// @brief 折れ角を変えて角点判定を検査するフィクスチャ
/// @note パラメータは点2での折れ角 [deg]
class InterpolateCornerTest : public ::testing::TestWithParam<double> {};

INSTANTIATE_TEST_SUITE_P(
    TurnAngle, InterpolateCornerTest,
    ::testing::Values(30.0, 89.0, 91.0, 135.0, 179.0, 180.0));

TEST_P(InterpolateCornerTest, SmoothBelowAndSharpAboveCornerAngle) {
    const double turn = GetParam() * kPi / 180.0;
    const Vector3d d0 = Vector3d::UnitX();
    const Vector3d d1{std::cos(turn), std::sin(turn), 0.0};
    const std::vector<Vector3d> pts = {
        Vector3d::Zero(), 5.0 * d0, 10.0 * d0,
        10.0 * d0 + 5.0 * d1, 10.0 * d0 + 10.0 * d1};
    const auto result = i_ent::InterpolateWithNurbs(pts);

    const double u = result.parameters[2];
    const Vector3d left = DerivativeAt(result, u - 1e-9);
    const Vector3d right = DerivativeAt(result, u + 1e-9);
    if (turn > kPi / 2.0) {
        // 角点では左右の接線がそれぞれの弦に一致し、各辺は直線となる
        EXPECT_LE(Angle(left, d0), 1e-6);
        EXPECT_LE(Angle(right, d1), 1e-6);
        const auto stats = MeasureSegments(result, pts);
        EXPECT_LE(stats.max_chord_deviation, 1e-12);
    } else {
        EXPECT_LE((left - right).norm(), 1e-5 * right.norm());
    }
    EXPECT_LE(MaxPassError(result, pts), 1e-12);
}

TEST(InterpolateWithNurbsTest, CornerAngleOptionIsHonored) {
    // 60度の折れは既定では滑らか、閾値を45度にすると角点になる
    const double turn = kPi / 3.0;
    const std::vector<Vector3d> pts = {
        Vector3d::Zero(), Vector3d::UnitX(),
        Vector3d::UnitX() + Vector3d{std::cos(turn), std::sin(turn), 0.0}};
    i_ent::NurbsInterpOptions options;
    options.corner_angle = kPi / 4.0;
    const auto smooth = i_ent::InterpolateWithNurbs(pts);
    const auto sharp = i_ent::InterpolateWithNurbs(pts, {}, options);
    EXPECT_EQ(smooth.curve->NumControlPoints(), 6);
    EXPECT_EQ(sharp.curve->NumControlPoints(), 7);
}



// =========================================================================
// グループD: 与えられた接線
// =========================================================================

TEST(InterpolateWithNurbsTest, ConsistentTangentsAreUsed) {
    // 粗い螺旋 (間隔2.0) に真の接線を与えると、接続点の接線が一致し精度も上がる
    constexpr int kCount = 30;
    constexpr double kStep = 2.0;
    const auto pts = MakeHelixPoints(kCount, kStep);
    TangentList tangents;
    for (int i = 0; i < kCount; ++i) {
        tangents.emplace_back(3.0 * HelixTangent(i * kStep));
    }
    const auto with = i_ent::InterpolateWithNurbs(pts, tangents);
    const auto without = i_ent::InterpolateWithNurbs(pts);

    for (int i = 0; i < kCount; ++i) {
        const Vector3d d = DerivativeAt(with, with.parameters[i]);
        EXPECT_LE(Angle(d, HelixTangent(i * kStep)), 1e-9) << "i=" << i;
    }
    auto max_error = [&](const i_ent::NurbsInterpolation& r) {
        double err = 0.0;
        for (int i = 0; i + 1 < kCount; ++i) {
            const double u = 0.5 * (r.parameters[i] + r.parameters[i + 1]);
            const auto p = r.curve->TryGetPointAt(u);
            err = std::max(err, (*p - HelixPoint((i + 0.5) * kStep)).norm());
        }
        return err;
    };
    EXPECT_LT(max_error(with), max_error(without));
}

namespace {

/// @brief 不正な接線の種類
enum class BadTangentKind {
    /// @brief 逆向き
    kReversed,
    /// @brief 弦に垂直
    kPerpendicular,
    /// @brief 非有限値
    kNan,
    /// @brief 零ベクトル
    kZero,
};

}  // namespace

/// @brief 不正な接線がすべて無視されることを検査するフィクスチャ
class InterpolateBadTangentTest
    : public ::testing::TestWithParam<BadTangentKind> {};

INSTANTIATE_TEST_SUITE_P(
    Kinds, InterpolateBadTangentTest,
    ::testing::Values(BadTangentKind::kReversed,
                      BadTangentKind::kPerpendicular,
                      BadTangentKind::kNan, BadTangentKind::kZero));

TEST_P(InterpolateBadTangentTest, BadTangentsAreIgnored) {
    constexpr int kCount = 40;
    const auto pts = MakeHelixPoints(kCount, 0.5);
    TangentList tangents;
    for (int i = 0; i < kCount; ++i) {
        const Vector3d t = HelixTangent(i * 0.5);
        switch (GetParam()) {
            case BadTangentKind::kReversed:
                tangents.emplace_back(-t);
                break;
            case BadTangentKind::kPerpendicular:
                tangents.emplace_back(t.cross(Vector3d::UnitZ()));
                break;
            case BadTangentKind::kNan:
                tangents.emplace_back(Vector3d::Constant(
                    std::numeric_limits<double>::quiet_NaN()));
                break;
            case BadTangentKind::kZero:
                tangents.emplace_back(Vector3d::Zero());
                break;
        }
    }
    const auto with = i_ent::InterpolateWithNurbs(pts, tangents);
    const auto without = i_ent::InterpolateWithNurbs(pts);
    EXPECT_EQ(with.parameters, without.parameters);
    for (int i = 0; i <= 200; ++i) {
        const auto a = with.curve->TryGetPointAt(i / 200.0);
        const auto b = without.curve->TryGetPointAt(i / 200.0);
        EXPECT_EQ(*a, *b);
    }
}

TEST(InterpolateWithNurbsTest, TangentMakingObtuseAngleWithChordIsIgnored) {
    // 80度の折れでは推定接線は両側の弦から40度ずれる。推定接線から80度回した
    // 接線は許容角度π/2の範囲内だが、出る側の弦と鈍角をなすため採用しない
    const double turn = 80.0 * kPi / 180.0;
    const std::vector<Vector3d> pts = {
        Vector3d::Zero(), Vector3d::UnitX(),
        Vector3d::UnitX() + Vector3d{std::cos(turn), std::sin(turn), 0.0}};
    const double rot = 40.0 * kPi / 180.0 - 80.0 * kPi / 180.0;
    TangentList tangents(3);
    tangents[1] = Vector3d{std::cos(rot), std::sin(rot), 0.0};
    i_ent::NurbsInterpOptions options;
    options.tangent_tolerance = kPi / 2.0;

    const auto with = i_ent::InterpolateWithNurbs(pts, tangents, options);
    const auto without = i_ent::InterpolateWithNurbs(pts);
    EXPECT_EQ(with.parameters, without.parameters);
    const Vector3d d = DerivativeAt(with, with.parameters[1]);
    EXPECT_LE(Angle(d, DerivativeAt(without, without.parameters[1])), 1e-12);
}

TEST(InterpolateWithNurbsTest, ConstantFeedDirectionTangentsStayBounded) {
    // C-SpaceCAMのPNFのように、全点に一定の送り方向を与える場合.
    // 一部は許容角度内として採用されるが、曲線は破綻しない
    const auto pts = MakeHelixPoints(100, 0.5);
    const TangentList tangents(pts.size(), Vector3d::UnitY());
    const auto result = i_ent::InterpolateWithNurbs(pts, tangents);
    const auto stats = MeasureSegments(result, pts);
    EXPECT_LE(MaxPassError(result, pts), 1e-12);
    EXPECT_LE(stats.max_chord_deviation, kMaxChordDeviationRatio);
    EXPECT_LE(stats.curve_length, 1.01 * stats.polyline_length);
}



// =========================================================================
// グループE: 非常に悪い点列
// =========================================================================

namespace {

/// @brief 悪い点列の1ケース
struct BadPointCase {
    /// @brief ケース名
    std::string name;
    /// @brief 点列
    std::vector<Vector3d> points;
};

/// @brief テスト失敗時の表示用にケース名を出力する
/// @param c  ケース
/// @param os 出力先
void PrintTo(const BadPointCase& c, std::ostream* os) {
    *os << c.name;
}

/// @brief 悪い点列のケースを生成する
/// @return ケースの一覧
std::vector<BadPointCase> MakeBadPointCases() {
    DeterministicRandom rng(7);
    std::vector<BadPointCase> cases;
    const auto helix = MakeHelixPoints(200, 0.5);

    // (1) 近重複点 (ジッタ) の混入
    const std::pair<const char*, double> jitters[] = {
        {"jitter_1e-7", 1e-7}, {"jitter_1e-5", 1e-5}, {"jitter_1e-3", 1e-3}};
    for (const auto& [name, amp] : jitters) {
        std::vector<Vector3d> pts;
        for (std::size_t i = 0; i < helix.size(); ++i) {
            pts.push_back(helix[i]);
            if (i % 10 == 5) pts.push_back(helix[i] + amp * rng.UniformVector());
        }
        cases.push_back({name, pts});
    }
    // (2) 往復 (180度反転) を繰り返す
    {
        std::vector<Vector3d> pts;
        for (int r = 0; r < 4; ++r) {
            for (int i = 0; i <= 20; ++i) {
                const double x = (r % 2 == 0) ? 0.5 * i : 10.0 - 0.5 * i;
                if (r > 0 && i == 0) continue;
                pts.emplace_back(x, 0.0, 0.0);
            }
        }
        cases.push_back({"reversal", pts});
    }
    // (3) 鋭いジグザグ
    {
        std::vector<Vector3d> pts;
        for (int i = 0; i < 40; ++i) pts.emplace_back(0.1 * i, (i % 2) * 5.0, 0.0);
        cases.push_back({"zigzag", pts});
    }
    // (4) ランダムウォーク
    {
        std::vector<Vector3d> pts{Vector3d::Zero()};
        for (int i = 0; i < 300; ++i) pts.push_back(pts.back() + rng.UniformVector());
        cases.push_back({"random_walk", pts});
    }
    // (5) 順序に意味のない乱数点群
    {
        std::vector<Vector3d> pts;
        for (int i = 0; i < 100; ++i) pts.push_back(100.0 * rng.UniformVector());
        cases.push_back({"random_cloud", pts});
    }
    // (6) 間隔比1e6の交互配置
    {
        std::vector<Vector3d> pts;
        double x = 0.0;
        for (int i = 0; i < 100; ++i) {
            x += (i % 2 == 0) ? 1.0 : 1e-6;
            pts.emplace_back(x, std::sin(x), 0.0);
        }
        cases.push_back({"spacing_ratio_1e6", pts});
    }
    // (7) 大きな座標オフセットと微小スケール
    {
        std::vector<Vector3d> offset, tiny;
        for (const auto& p : MakeHelixPoints(100, 0.5)) {
            offset.push_back(p + Vector3d::Constant(1e8));
            tiny.push_back(p * 1e-6);
        }
        cases.push_back({"offset_1e8", offset});
        cases.push_back({"tiny_1e-6", tiny});
    }
    // (8) 1e-6に丸めた粗い円弧と、始点と終点が一致する粗い閉曲線
    {
        std::vector<Vector3d> arc, closed;
        for (int i = 0; i < 30; ++i) {
            const double t = kPi * i / 29.0;
            arc.emplace_back(std::round(50.0 * std::cos(t) * 1e6) / 1e6,
                             std::round(50.0 * std::sin(t) * 1e6) / 1e6, 0.0);
        }
        for (int i = 0; i <= 12; ++i) {
            const double t = 2.0 * kPi * i / 12.0;
            closed.emplace_back(std::cos(t), std::sin(t), 0.0);
        }
        cases.push_back({"quantized_arc", arc});
        cases.push_back({"closed_coarse", closed});
    }
    // (9) 最小構成 (3点の共線・直角・反転)
    cases.push_back({"three_collinear",
                     {Vector3d::Zero(), Vector3d::UnitX(), 3.0 * Vector3d::UnitX()}});
    cases.push_back({"three_right",
                     {Vector3d::Zero(), Vector3d::UnitX(),
                      Vector3d::UnitX() + Vector3d::UnitY()}});
    cases.push_back({"three_reversal",
                     {Vector3d::Zero(), Vector3d::UnitX(), 0.5 * Vector3d::UnitX()}});
    return cases;
}

}  // namespace

/// @brief 悪い点列でも曲線が破綻しないことを検査するフィクスチャ
class InterpolateBadPointsTest
    : public ::testing::TestWithParam<BadPointCase> {};

INSTANTIATE_TEST_SUITE_P(
    Cases, InterpolateBadPointsTest,
    ::testing::ValuesIn(MakeBadPointCases()),
    [](const ::testing::TestParamInfo<BadPointCase>& info) {
        std::string name = info.param.name;
        std::replace_if(name.begin(), name.end(),
                        [](unsigned char c) { return !std::isalnum(c); },
                        '_');
        return name;
    });

TEST_P(InterpolateBadPointsTest, CurveStaysBoundedAndPassesThroughPoints) {
    const auto& pts = GetParam().points;
    const auto result = i_ent::InterpolateWithNurbs(pts);
    ASSERT_NE(result.curve, nullptr);
    ASSERT_EQ(result.parameters.size(), pts.size());

    // 判定1: 全点を通る (座標の大きさに応じた丸め誤差まで許容する)
    EXPECT_LE(MaxPassError(result, pts), 1e-12 * CoordinateScale(pts));

    // 判定2: 各区間は弦から理論上限以上に離れない
    const auto stats = MeasureSegments(result, pts);
    ASSERT_TRUE(stats.all_finite);
    EXPECT_LE(stats.max_chord_deviation, kMaxChordDeviationRatio + 1e-9);

    // 判定3: 曲線長が折れ線長から大きく乖離しない (実測最大1.05倍)
    EXPECT_LE(stats.curve_length, 1.2 * stats.polyline_length);
}

TEST(InterpolateWithNurbsTest, HandlesLargePointCount) {
    constexpr int kCount = 200000;
    const auto pts = MakeHelixPoints(kCount, 0.01);
    const auto result = i_ent::InterpolateWithNurbs(pts);
    ASSERT_EQ(result.parameters.size(), pts.size());
    for (int i = 0; i < kCount; i += 997) {
        const auto p = result.curve->TryGetPointAt(result.parameters[i]);
        ASSERT_TRUE(p.has_value());
        EXPECT_LE((*p - pts[i]).norm(), 1e-9) << "i=" << i;
    }
}
