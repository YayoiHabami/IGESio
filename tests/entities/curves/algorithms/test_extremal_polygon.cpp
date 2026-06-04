/**
 * @file tests/entities/curves/algorithms/test_extremal_polygon.cpp
 * @brief src/entities/curves/algorithms/extremal_polygon.h のテスト
 * @author Yayoi Habami
 * @date 2026-05-30
 * @copyright 2026 Yayoi Habami
 *
 * テスト対象 (igesio::entities):
 *   ComputeCircumscribedPolygon(curve, n_vert, reference_normal, eps)
 *   ComputeInscribedPolygon(curve, n_vert, reference_normal, eps)
 *
 * テスト曲線:
 *   M16_solidworks_s144_flatten_part1.IGS のパラメータ空間トリミングループ4種
 *   (extremal_polygon_test_curves.h に逐語抽出済み)。いずれも z=0 平面上の
 *   閉曲線で参照法線は (0,0,1)。
 *     - MakeLoopArchBand1 / MakeLoopArchBand2: 2次NURBSアーチ×2 + 直線×2 の非凸帯
 *     - MakeLoopRect1 / MakeLoopRect2: 単位矩形 (全辺直線・角4つ) の退化ケース
 *
 * 検証方針:
 *   実装の詳細な頂点座標はハードコードせず、外包/内包多角形が満たすべき
 *   幾何学的性質 (含有関係・on_curve整合・面積順序・構造妥当性) を検証する。
 *   含有判定には曲線を高分解能サンプルした近似多角形 (fine) を真値の代理とする。
 *
 * TODO:
 *   - z≠0 平面・任意方向 reference_normal の曲線は未カバー (z=0/法線(0,0,1)のみ)
 *   - 円弧(100)など曲率連続な構成曲線を混在させたループは未カバー
 *   - eps を変化させた頂点/接点分類の境界挙動は未カバー (既定 eps のみ)
 *   - 接線・曲率計算失敗時の igesio::ComputationError 経路は誘発困難のため未カバー
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "igesio/numerics/matrix.h"
#include "igesio/numerics/polygon.h"
#include "igesio/entities/interfaces/i_curve.h"
#include "igesio/entities/curves/composite_curve.h"
#include "igesio/entities/curves/line.h"
// テスト対象 (src 配下の内部ヘッダ; igesio が src を PUBLIC include に公開)
#include "entities/curves/algorithms/extremal_polygon.h"
#include "./extremal_polygon_test_curves.h"

namespace {

namespace i_ent = igesio::entities;
namespace i_num = igesio::numerics;
namespace i_test = igesio::tests;
using igesio::Vector3d;
using i_ent::CompositeCurve;
using i_ent::ICurve;
using i_ent::Line;
using i_num::PolygonData;

/// @brief z=0 平面上の曲線に対する参照法線
const Vector3d kNormalZ(0.0, 0.0, 1.0);
/// @brief on_curve 頂点と GetPointAt の一致許容誤差
constexpr double kOnCurveTol = 1e-9;
/// @brief 外包多角形の境界内外判定の許容誤差 (接点は境界上)
constexpr double kEpsCirc = 1e-4;
/// @brief 内包多角形の境界内外判定の許容誤差 (fine 多角形の弦近似誤差を吸収)
constexpr double kEpsInsc = 1e-3;
/// @brief 曲線を近似する fine 多角形の分割数
constexpr int kFineSamples = 600;



/**
 * 2次元ジオメトリ補助 (全曲線が z=0 平面上にあるため (x, y) を用いる)
 */

/// @brief 3次元点の (x, y) を返す
std::array<double, 2> ToXY(const Vector3d& p) { return {p.x(), p.y()}; }

/// @brief PolygonData の頂点を2次元座標列に変換する
std::vector<std::array<double, 2>> PolyXY(const PolygonData& poly) {
    std::vector<std::array<double, 2>> out;
    out.reserve(poly.vertices.size());
    for (const auto& v : poly.vertices) out.push_back(ToXY(v));
    return out;
}

/// @brief 閉曲線を [t0, t1) で n 等分し2次元の近似多角形を返す
std::vector<std::array<double, 2>> SampleCurveXY(const ICurve& curve, int n) {
    const auto [t0, t1] = curve.GetParameterRange();
    const double period = t1 - t0;
    std::vector<std::array<double, 2>> out;
    out.reserve(n);
    for (int i = 0; i < n; ++i) {
        out.push_back(ToXY(curve.GetPointAt(t0 + period * i / n)));
    }
    return out;
}

/// @brief 2次元多角形の符号付き面積 (shoelace) の絶対値を返す
double PolygonArea(const std::vector<std::array<double, 2>>& p) {
    const int n = static_cast<int>(p.size());
    double a = 0.0;
    for (int i = 0; i < n; ++i) {
        const int j = (i + 1) % n;
        a += p[i][0] * p[j][1] - p[j][0] * p[i][1];
    }
    return std::abs(0.5 * a);
}

/// @brief 点 p と線分 (a, b) の距離を返す
double DistPointSegment(const std::array<double, 2>& p,
                        const std::array<double, 2>& a,
                        const std::array<double, 2>& b) {
    const double abx = b[0] - a[0], aby = b[1] - a[1];
    const double len2 = abx * abx + aby * aby;
    double t = 0.0;
    if (len2 > 0.0) {
        t = ((p[0] - a[0]) * abx + (p[1] - a[1]) * aby) / len2;
        t = std::max(0.0, std::min(1.0, t));
    }
    const double dx = p[0] - (a[0] + t * abx);
    const double dy = p[1] - (a[1] + t * aby);
    return std::sqrt(dx * dx + dy * dy);
}

/// @brief 点が単純多角形の内部または境界 (eps以内) にあるかを判定する
/// @note 境界はeps距離で判定し、内部はeven-odd ray casting (向き非依存) で判定する
bool PointInOrOnPolygon(const std::array<double, 2>& p,
                        const std::vector<std::array<double, 2>>& poly,
                        double eps) {
    const int n = static_cast<int>(poly.size());
    for (int i = 0; i < n; ++i) {
        if (DistPointSegment(p, poly[i], poly[(i + 1) % n]) <= eps) return true;
    }
    bool inside = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        const auto& pi = poly[i];
        const auto& pj = poly[j];
        if (((pi[1] > p[1]) != (pj[1] > p[1])) &&
            (p[0] < (pj[0] - pi[0]) * (p[1] - pi[1]) / (pj[1] - pi[1]) + pi[0])) {
            inside = !inside;
        }
    }
    return inside;
}



/**
 * テスト用ループ一式
 */

/// @brief 名前付きの閉ループ
struct LoopCase {
    std::string name;
    std::shared_ptr<ICurve> curve;
};

/// @brief 全4ループを返す
std::vector<LoopCase> AllLoops() {
    return {{"ArchBand1", i_test::MakeLoopArchBand1()},
            {"ArchBand2", i_test::MakeLoopArchBand2()},
            {"Rect1", i_test::MakeLoopRect1()},
            {"Rect2", i_test::MakeLoopRect2()}};
}

/// @brief 滑らかなアーチ帯ループ (2種) を返す
std::vector<LoopCase> ArchBandLoops() {
    return {{"ArchBand1", i_test::MakeLoopArchBand1()},
            {"ArchBand2", i_test::MakeLoopArchBand2()}};
}

/// @brief 矩形ループ (2種) を返す
std::vector<LoopCase> RectLoops() {
    return {{"Rect1", i_test::MakeLoopRect1()},
            {"Rect2", i_test::MakeLoopRect2()}};
}

/// @brief PolygonData の構造的整合性を検証する共通ルーチン
void ExpectStructurallyValid(const ICurve& curve, const PolygonData& poly) {
    // 各ベクトルの長さが一致すること
    ASSERT_EQ(poly.vertices.size(), poly.on_curve.size());
    ASSERT_EQ(poly.vertices.size(), poly.curve_params.size());
    // 多角形は3頂点以上であること
    EXPECT_GE(poly.Count(), 3);

    const int n = poly.Count();
    double last_param = -std::numeric_limits<double>::infinity();
    for (int i = 0; i < n; ++i) {
        // 全頂点が有限であること (NaN/Inf の混入を検出)
        EXPECT_TRUE(std::isfinite(poly.vertices[i].x()) &&
                    std::isfinite(poly.vertices[i].y()) &&
                    std::isfinite(poly.vertices[i].z()))
                << "vertex " << i << " is not finite";

        // on_curve 頂点は C(curve_params) に一致し、curve_params は昇順であること
        if (poly.on_curve[i]) {
            const Vector3d on = curve.GetPointAt(poly.curve_params[i]);
            EXPECT_LE((poly.vertices[i] - on).norm(), kOnCurveTol)
                    << "on_curve vertex " << i << " != C(curve_params)";
            EXPECT_GE(poly.curve_params[i], last_param)
                    << "curve_params not ascending at " << i;
            last_param = poly.curve_params[i];
        }
    }
}

/// @brief 多角形のゼロ長辺 (隣接する同一頂点) の本数を数える
int CountDegenerateEdges(const PolygonData& poly) {
    const int n = poly.Count();
    int count = 0;
    for (int i = 0; i < n; ++i) {
        if ((poly.vertices[i] - poly.vertices[(i + 1) % n]).norm() <= 1e-12) {
            ++count;
        }
    }
    return count;
}

}  // namespace



/**
 * 構造的整合性のテスト (正常系・代表値)
 */

// 外包多角形: 4ループすべてで構造的に妥当
TEST(ExtremalPolygonValidityTest, Circumscribed_StructurallyValid) {
    for (const auto& lc : AllLoops()) {
        SCOPED_TRACE("Loop: " + lc.name);
        const auto poly =
            i_ent::ComputeCircumscribedPolygon(*lc.curve, 64, kNormalZ);
        ExpectStructurallyValid(*lc.curve, poly);
    }
}

// 内包多角形: 4ループすべてで構造的に妥当
TEST(ExtremalPolygonValidityTest, Inscribed_StructurallyValid) {
    for (const auto& lc : AllLoops()) {
        SCOPED_TRACE("Loop: " + lc.name);
        const auto poly =
            i_ent::ComputeInscribedPolygon(*lc.curve, 64, kNormalZ);
        ExpectStructurallyValid(*lc.curve, poly);
    }
}

// 外包・内包多角形にゼロ長辺 (重複頂点) が無いこと
// NOTE: 現状の実装は角点・直線部で同一点を重複追加し、ゼロ長辺を生成する
//       (BuildPolygonVertices: cc ケースの接線交点が接点 Pa/Pb と一致するため)。
//       これを欠陥として検出する診断テスト。
TEST(ExtremalPolygonQualityTest, NoDegenerateEdges) {
    for (const auto& lc : AllLoops()) {
        SCOPED_TRACE("Loop: " + lc.name);
        EXPECT_EQ(CountDegenerateEdges(
                      i_ent::ComputeCircumscribedPolygon(*lc.curve, 64, kNormalZ)),
                  0)
                << "circumscribed polygon has degenerate (zero-length) edges";
        EXPECT_EQ(CountDegenerateEdges(
                      i_ent::ComputeInscribedPolygon(*lc.curve, 64, kNormalZ)),
                  0)
                << "inscribed polygon has degenerate (zero-length) edges";
    }
}



/**
 * 含有関係のテスト (正常系・代表値)
 * 内包多角形 ⊆ 曲線領域 ⊆ 外包多角形 を、曲線の高分解能近似 (fine) を
 * 代理として検証する
 */

// 外包多角形は曲線上の全点を内包する
TEST(ExtremalPolygonContainmentTest, Circumscribed_ContainsCurve) {
    for (const auto& lc : AllLoops()) {
        SCOPED_TRACE("Loop: " + lc.name);
        const auto poly =
            i_ent::ComputeCircumscribedPolygon(*lc.curve, 64, kNormalZ);
        const auto circ = PolyXY(poly);
        const auto fine = SampleCurveXY(*lc.curve, kFineSamples);
        for (size_t i = 0; i < fine.size(); ++i) {
            EXPECT_TRUE(PointInOrOnPolygon(fine[i], circ, kEpsCirc))
                    << "curve point " << i << " (" << fine[i][0] << ", "
                    << fine[i][1] << ") is outside the circumscribed polygon";
        }
    }
}

// 内包多角形は曲線領域の内部に収まる
TEST(ExtremalPolygonContainmentTest, Inscribed_InsideCurve) {
    for (const auto& lc : AllLoops()) {
        SCOPED_TRACE("Loop: " + lc.name);
        const auto poly =
            i_ent::ComputeInscribedPolygon(*lc.curve, 64, kNormalZ);
        const auto insc = PolyXY(poly);
        const auto fine = SampleCurveXY(*lc.curve, kFineSamples);
        for (size_t i = 0; i < insc.size(); ++i) {
            EXPECT_TRUE(PointInOrOnPolygon(insc[i], fine, kEpsInsc))
                    << "inscribed vertex " << i << " (" << insc[i][0] << ", "
                    << insc[i][1] << ") is outside the curve region";
        }
    }
}

// 内包多角形の面積は外包多角形の面積以下である
TEST(ExtremalPolygonContainmentTest, InscribedArea_LeCircumscribedArea) {
    for (const auto& lc : AllLoops()) {
        SCOPED_TRACE("Loop: " + lc.name);
        const auto circ = PolyXY(
            i_ent::ComputeCircumscribedPolygon(*lc.curve, 64, kNormalZ));
        const auto insc = PolyXY(
            i_ent::ComputeInscribedPolygon(*lc.curve, 64, kNormalZ));
        // 内包 ⊆ 外包 より面積も単調 (弦近似ゆらぎを吸収する微小スラックを許容)
        EXPECT_LE(PolygonArea(insc), PolygonArea(circ) + 1e-9);
    }
}



/**
 * 退化ケースのテスト (矩形ループ)
 * 全辺直線の単位矩形では、外包・内包とも単位矩形の境界をなぞる。
 * 各辺は n_vert で分割されるため頂点は4隅に限らず境界上に並ぶ。
 */

namespace {

/// @brief 多角形が単位矩形 [0,1]^2 の境界をなぞることを検証する
///        (全頂点が境界上にあり、かつ面積が 1 に等しい)
void ExpectTracesUnitSquare(const PolygonData& poly) {
    constexpr double kTol = 1e-6;
    const auto pts = PolyXY(poly);
    ASSERT_GE(pts.size(), 4u);
    for (const auto& p : pts) {
        // 左右の辺 (x=0 or x=1, 0<=y<=1) または上下の辺 (y=0 or y=1, 0<=x<=1)
        const bool on_vert_edge =
            (std::abs(p[0]) < kTol || std::abs(p[0] - 1.0) < kTol) &&
            (p[1] > -kTol && p[1] < 1.0 + kTol);
        const bool on_horz_edge =
            (std::abs(p[1]) < kTol || std::abs(p[1] - 1.0) < kTol) &&
            (p[0] > -kTol && p[0] < 1.0 + kTol);
        EXPECT_TRUE(on_vert_edge || on_horz_edge)
                << "vertex (" << p[0] << ", " << p[1]
                << ") is not on the unit-square boundary";
    }
    // 単位矩形全体を覆う (辺の分割で頂点が増えても面積は 1)
    EXPECT_NEAR(PolygonArea(pts), 1.0, 1e-5);
}

}  // namespace

// 矩形ループの外包多角形は単位矩形の境界をなぞる
TEST(ExtremalPolygonRectTest, Circumscribed_TracesUnitSquare) {
    for (const auto& lc : RectLoops()) {
        SCOPED_TRACE("Loop: " + lc.name);
        ExpectTracesUnitSquare(
            i_ent::ComputeCircumscribedPolygon(*lc.curve, 16, kNormalZ));
    }
}

// 矩形ループの内包多角形は単位矩形の境界をなぞる
TEST(ExtremalPolygonRectTest, Inscribed_TracesUnitSquare) {
    for (const auto& lc : RectLoops()) {
        SCOPED_TRACE("Loop: " + lc.name);
        ExpectTracesUnitSquare(
            i_ent::ComputeInscribedPolygon(*lc.curve, 16, kNormalZ));
    }
}



/**
 * ロバストネスのテスト (想定バグ H1: 接線交点の発散)
 * 滑らかなアーチを細かくサンプルすると隣接接線がほぼ平行になり、外包多角形の
 * 接点間頂点が遠方に発散しうる。各頂点が曲線バウンディングボックスを
 * 過度に外れていないこと (発散・無限大でないこと) を診断的に検証する。
 * NOTE: 下限の係数は経験的。誤検出する場合は緩めること。
 */
TEST(ExtremalPolygonRobustnessTest, Circumscribed_VerticesWithinSaneBounds) {
    constexpr double kFactor = 50.0;
    for (const auto& lc : ArchBandLoops()) {
        SCOPED_TRACE("Loop: " + lc.name);
        const auto fine = SampleCurveXY(*lc.curve, kFineSamples);
        double min_x = 1e30, min_y = 1e30, max_x = -1e30, max_y = -1e30;
        for (const auto& p : fine) {
            min_x = std::min(min_x, p[0]); max_x = std::max(max_x, p[0]);
            min_y = std::min(min_y, p[1]); max_y = std::max(max_y, p[1]);
        }
        const double diag = std::sqrt((max_x - min_x) * (max_x - min_x) +
                                      (max_y - min_y) * (max_y - min_y));
        const double lo_x = min_x - kFactor * diag;
        const double hi_x = max_x + kFactor * diag;
        const double lo_y = min_y - kFactor * diag;
        const double hi_y = max_y + kFactor * diag;

        const auto poly =
            i_ent::ComputeCircumscribedPolygon(*lc.curve, 64, kNormalZ);
        for (int i = 0; i < poly.Count(); ++i) {
            const auto& v = poly.vertices[i];
            EXPECT_TRUE(std::isfinite(v.x()) && std::isfinite(v.y()))
                    << "vertex " << i << " is not finite";
            EXPECT_GE(v.x(), lo_x); EXPECT_LE(v.x(), hi_x);
            EXPECT_GE(v.y(), lo_y); EXPECT_LE(v.y(), hi_y);
        }
    }
}



/**
 * エラー系のテスト
 */

namespace {

/// @brief 2本のLineからなる開いた (閉じていない) CompositeCurve
/// (0,0,0)→(1,0,0)→(1,1,0)
std::shared_ptr<CompositeCurve> MakeOpenChain() {
    auto cc = std::make_shared<CompositeCurve>();
    cc->AddCurve(std::make_shared<Line>(
        Vector3d{0.0, 0.0, 0.0}, Vector3d{1.0, 0.0, 0.0}));
    cc->AddCurve(std::make_shared<Line>(
        Vector3d{1.0, 0.0, 0.0}, Vector3d{1.0, 1.0, 0.0}));
    return cc;
}

/// @brief 自己交差する閉じたCompositeCurve (8の字: 対角線が交差)
/// (0,0,0)→(2,2,0)→(2,0,0)→(0,2,0)→(0,0,0)
std::shared_ptr<CompositeCurve> MakeSelfIntersecting() {
    auto cc = std::make_shared<CompositeCurve>();
    cc->AddCurve(std::make_shared<Line>(
        Vector3d{0.0, 0.0, 0.0}, Vector3d{2.0, 2.0, 0.0}));
    cc->AddCurve(std::make_shared<Line>(
        Vector3d{2.0, 2.0, 0.0}, Vector3d{2.0, 0.0, 0.0}));
    cc->AddCurve(std::make_shared<Line>(
        Vector3d{2.0, 0.0, 0.0}, Vector3d{0.0, 2.0, 0.0}));
    cc->AddCurve(std::make_shared<Line>(
        Vector3d{0.0, 2.0, 0.0}, Vector3d{0.0, 0.0, 0.0}));
    return cc;
}

}  // namespace

// 閉じていない曲線 → std::invalid_argument
TEST(ExtremalPolygonErrorTest, ThrowsInvalidArgumentWhenNotClosed) {
    auto open_curve = MakeOpenChain();
    EXPECT_THROW(i_ent::ComputeCircumscribedPolygon(*open_curve, 16, kNormalZ),
                 std::invalid_argument);
    EXPECT_THROW(i_ent::ComputeInscribedPolygon(*open_curve, 16, kNormalZ),
                 std::invalid_argument);
}

// 自己交差する閉曲線 → std::invalid_argument
TEST(ExtremalPolygonErrorTest, ThrowsInvalidArgumentWhenSelfIntersecting) {
    auto crossed = MakeSelfIntersecting();
    EXPECT_THROW(i_ent::ComputeCircumscribedPolygon(*crossed, 16, kNormalZ),
                 std::invalid_argument);
    EXPECT_THROW(i_ent::ComputeInscribedPolygon(*crossed, 16, kNormalZ),
                 std::invalid_argument);
}

// ゼロ法線 → std::invalid_argument (境界: ノルム 1e-12)
TEST(ExtremalPolygonErrorTest, ThrowsInvalidArgumentWhenNormalIsZero) {
    auto curve = i_test::MakeLoopRect2();
    // ノルムが 1e-12 未満 → 例外
    EXPECT_THROW(
        i_ent::ComputeCircumscribedPolygon(*curve, 16, Vector3d(0.0, 0.0, 0.0)),
        std::invalid_argument);
    EXPECT_THROW(
        i_ent::ComputeCircumscribedPolygon(
            *curve, 16, Vector3d(0.0, 0.0, 1e-13)),
        std::invalid_argument);
    // 有効な法線 → 例外なし
    EXPECT_NO_THROW(
        i_ent::ComputeCircumscribedPolygon(*curve, 16, kNormalZ));
}

// n_vert < 3 → std::invalid_argument (境界: 3 は許容)
TEST(ExtremalPolygonErrorTest, ThrowsInvalidArgumentWhenNVertLessThan3) {
    auto curve = i_test::MakeLoopRect2();
    EXPECT_THROW(i_ent::ComputeCircumscribedPolygon(*curve, 2, kNormalZ),
                 std::invalid_argument);
    EXPECT_THROW(i_ent::ComputeInscribedPolygon(*curve, 2, kNormalZ),
                 std::invalid_argument);
    // 境界値 3 は許容
    EXPECT_NO_THROW(i_ent::ComputeCircumscribedPolygon(*curve, 3, kNormalZ));
    EXPECT_NO_THROW(i_ent::ComputeInscribedPolygon(*curve, 3, kNormalZ));
}
