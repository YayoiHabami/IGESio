/**
 * @file tests/entities/curves/algorithms/test_polygonal_approximation.cpp
 * @brief src/entities/curves/algorithms/polygonal_approximation.h のテスト
 * @author Yayoi Habami
 * @date 2026-06-02
 * @copyright 2026 Yayoi Habami
 *
 * テスト対象 (igesio::entities):
 *   ComputeApproximatePolygon(curve, inscribed, circumscribed, eps, max_depth)
 *
 * テスト曲線:
 *   - MakeLineSegment       : Type 110 線分 (開・直線部報告なし→終了条件②)
 *   - MakeStraightNurbs     : degree-1 NURBS 直線 (開・直線部報告あり→終了条件①)
 *   - MakeHalfCircle        : 半円 CircularArc (開・滑らか)
 *   - MakeFullCircle        : 全円 CircularArc (閉)
 *   - MakeLoopArchBand1/2   : M16 由来の直線+曲線混在 閉ループ (extremal_polygon_test_curves.h)
 *
 * 検証方針:
 *   頂点座標はハードコードせず、出力多角形が満たすべき性質 (構造妥当性・近似精度・
 *   固定点保存・閉曲線終端処理・境界クランプ) を検証する。解析的に決定論的な
 *   ケース (直線・巨大eps・max_depth=0) のみ正確なパラメータ列を固定し、特性化
 *   (高速化リファクタでの出力不変) のロックとする。非決定論的ケースの集約
 *   フィンガープリント採取は DISABLED_RecordFingerprints を参照。
 *
 * TODO:
 *   - 異常系: ComputeApproximatePolygon は正当な入力に対し例外を投げる定義された
 *     経路を持たない (範囲外パラメータは clamp、点評価失敗は graceful 打ち切り)。
 *     配列長が不整合な PolygonData は operator[] による未定義動作であり契約外の
 *     ため未カバー。
 *   - z≠0 平面・任意方向 reference_normal の曲線は未カバー (z=0/法線(0,0,1)のみ)。
 *   - 非決定論的ケース (一般曲線 eps=1e-3, NURBSループ) のビット一致ロックは
 *     性質テストと DISABLED_RecordFingerprints での基準値採取に委ねる。
 *   - max_depth 打ち切り (終了条件③) で中点距離>eps となる辺は近似精度検証
 *     (ExpectEdgesWithinEps) の対象外。
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "igesio/common/iges_parameter_vector.h"
#include "igesio/numerics/core/matrix.h"
#include "igesio/numerics/geometric/polygon.h"
#include "igesio/entities/interfaces/i_curve.h"
#include "igesio/entities/curves/line.h"
#include "igesio/entities/curves/circular_arc.h"
#include "igesio/entities/curves/composite_curve.h"
#include "igesio/entities/curves/rational_b_spline_curve.h"
#include "igesio/entities/curves/algorithms.h"
// テスト対象 (src 配下の内部ヘッダ; igesio が src を PUBLIC include に公開)
#include "entities/curves/algorithms/polygonal_approximation.h"
#include "entities/curves/algorithms/extremal_polygon.h"
#include "./extremal_polygon_test_curves.h"

namespace {

namespace i_ent = igesio::entities;
namespace i_num = igesio::numerics;
namespace i_test = igesio::tests;
using igesio::Vector2d;
using igesio::Vector3d;
using i_ent::CircularArc;
using i_ent::CompositeCurve;
using i_ent::ICurve;
using i_ent::Line;
using i_ent::RationalBSplineCurve;
using i_num::PolygonData;

/// @brief 円周率
constexpr double kPi = igesio::kPi;
/// @brief 頂点と C(param) の一致許容誤差
constexpr double kEvalTol = 1e-9;
/// @brief 解析解・特性化パラメータの一致許容誤差
constexpr double kParamTol = 1e-12;
/// @brief z=0 平面曲線の参照法線
const Vector3d kNormalZ(0.0, 0.0, 1.0);



/**
 * テスト曲線ファクトリ
 */

/// @brief Type 110 線分 (0,0,0)-(2,1,0)。開曲線・直線部は報告しない。
///        パラメータ範囲 [0, 1]。
std::shared_ptr<Line> MakeLineSegment() {
    return std::make_shared<Line>(Vector3d(0.0, 0.0, 0.0),
                                  Vector3d(2.0, 1.0, 0.0));
}

/// @brief degree-1 の直線 NURBS (0,0,0)-(1,0,0)。開曲線で GetLinearSegments() が
///        全範囲を覆う (終了条件①の検証用)。パラメータ範囲 [0, 1]。
std::shared_ptr<RationalBSplineCurve> MakeStraightNurbs() {
    return std::make_shared<RationalBSplineCurve>(
        igesio::IGESParameterVector{
            1, 1,                       // K (制御点数-1), M (次数)
            true, false, true, false,   // PROP1-4 (平面/閉/多項式/周期)
            0., 0., 1., 1.,             // ノットベクトル
            1., 1.,                     // 重み
            0., 0., 0.,                 // 制御点0
            1., 0., 0.,                 // 制御点1
            0., 1.,                     // V(0), V(1)
            0., 0., 1.                  // 定義平面の法線
        });
}

/// @brief 原点中心・半径1の半円 (開曲線)。パラメータ範囲 [0, π]、|C(t)|=1。
std::shared_ptr<CircularArc> MakeHalfCircle() {
    return i_ent::MakeCircularArc(Vector2d(0.0, 0.0), 1.0, 0.0, kPi);
}

/// @brief 原点中心・半径1の全円 (閉曲線)。パラメータ範囲 [0, 2π]、|C(t)|=1。
std::shared_ptr<CircularArc> MakeFullCircle() {
    return i_ent::MakeCircle(Vector2d(0.0, 0.0), 1.0);
}



/**
 * 補助ルーチン
 */

/// @brief 空の多角形 (固定点を持たない insc/circ)
PolygonData EmptyPolygon() { return PolygonData{}; }

/// @brief ComputeApproximatePolygon の薄いラッパー
PolygonData Approx(const ICurve& curve, const PolygonData& insc,
                   const PolygonData& circ, double eps, int max_depth = 1000) {
    return i_ent::ComputeApproximatePolygon(curve, insc, circ, eps, max_depth);
}

/// @brief 閉曲線の外包・内包多角形対 (first:外包, second:内包) を生成する
std::pair<PolygonData, PolygonData> ExtremalPair(const ICurve& curve) {
    return i_ent::ComputeExtremalPolygonPair(curve, 64, kNormalZ);
}

/// @brief 出力多角形の構造的整合性を検証する (P1: 構造, P2: 昇順, P3: 頂点一致)
/// @note P3 は頂点が C(clamp(param)) に一致することを要求する。高速化で点を
///       細分中の評価値から再利用しても、この一致が崩れないことを保証する。
void ExpectStructurallyValid(const ICurve& curve, const PolygonData& poly) {
    ASSERT_EQ(poly.vertices.size(), poly.on_curve.size());
    ASSERT_EQ(poly.vertices.size(), poly.curve_params.size());
    EXPECT_GE(poly.Count(), 2);

    const auto [t_min, t_max] = curve.GetParameterRange();
    const size_t n = poly.vertices.size();
    double last = -std::numeric_limits<double>::infinity();
    for (size_t i = 0; i < n; ++i) {
        const Vector3d& v = poly.vertices[i];
        EXPECT_TRUE(std::isfinite(v.x()) && std::isfinite(v.y()) &&
                    std::isfinite(v.z())) << "vertex " << i << " not finite";
        // 全頂点が on_curve=true であること
        EXPECT_TRUE(poly.on_curve[i]) << "on_curve false at " << i;
        // パラメータは非減少であること
        const double u = poly.curve_params[i];
        EXPECT_GE(u, last) << "curve_params not ascending at " << i;
        last = u;
        // 頂点が C(clamp(param)) に一致すること
        const Vector3d on = curve.GetPointAt(std::clamp(u, t_min, t_max));
        EXPECT_LE((v - on).norm(), kEvalTol)
                << "vertex != C(clamp(param)) at " << i;
    }
}

/// @brief 各辺の近似精度を検証する (P4: 直線部内 or 中点距離≤eps)
/// @note 閉曲線の継ぎ目 (末尾→先頭) は近似保証の対象外のため検証しない。
///       終了条件③ (max_depth) で停止した辺は超過しうるため、max_depth が
///       十分大きい呼び出しに対してのみ使用する。
void ExpectEdgesWithinEps(const ICurve& curve, const PolygonData& poly,
                          double eps) {
    const auto segs = curve.GetLinearSegments();
    const auto [t_min, t_max] = curve.GetParameterRange();
    const size_t n = poly.curve_params.size();
    for (size_t i = 0; i + 1 < n; ++i) {
        const double a = poly.curve_params[i];
        const double b = poly.curve_params[i + 1];
        // 直線部に完全に含まれる辺は中点判定の対象外 (終了条件①)
        bool in_linear = false;
        for (const auto& s : segs) {
            if (s[0] <= a && b <= s[1]) { in_linear = true; break; }
        }
        if (in_linear) continue;

        const double m = 0.5 * (a + b);
        const Vector3d pa = curve.GetPointAt(std::clamp(a, t_min, t_max));
        const Vector3d pb = curve.GetPointAt(std::clamp(b, t_min, t_max));
        const Vector3d pm = curve.GetPointAt(std::clamp(m, t_min, t_max));
        const double d = i_ent::PointLineDistance(pm, pa, pb);
        EXPECT_LE(d, eps + 1e-9) << "edge " << i << " midpoint deviates " << d;
    }
}

/// @brief 出力多角形のフィンガープリント (特性化用の集約スカラー)
struct Fingerprint {
    int count;
    double sum_param, min_param, max_param;
    Vector3d centroid;
};

/// @brief 多角形からフィンガープリントを計算する
Fingerprint ComputeFingerprint(const PolygonData& p) {
    Fingerprint f{p.Count(), 0.0, std::numeric_limits<double>::infinity(),
                  -std::numeric_limits<double>::infinity(), Vector3d::Zero()};
    for (double u : p.curve_params) {
        f.sum_param += u;
        f.min_param = std::min(f.min_param, u);
        f.max_param = std::max(f.max_param, u);
    }
    for (const auto& v : p.vertices) f.centroid += v;
    if (!p.vertices.empty()) {
        f.centroid /= static_cast<double>(p.vertices.size());
    }
    return f;
}

}  // namespace



/**
 * 正常系・代表値
 */

// 線分 (直線部報告なし): 中点距離0で即時終了 (終了条件②) → 端点のみ
TEST(PolygonalApproximationTest, LineSegment_EmptyPolygons_ReturnsEndpoints) {
    const auto curve = MakeLineSegment();
    const auto poly = Approx(*curve, EmptyPolygon(), EmptyPolygon(), 1e-3);
    ASSERT_EQ(poly.Count(), 2);
    EXPECT_NEAR(poly.curve_params[0], 0.0, kParamTol);
    EXPECT_NEAR(poly.curve_params[1], 1.0, kParamTol);
    ExpectStructurallyValid(*curve, poly);
}

// 直線 NURBS (直線部報告あり): 区間全体が直線部に内包 (終了条件①) → 端点のみ
TEST(PolygonalApproximationTest, StraightNurbs_ShortCircuitsLinearSegment) {
    const auto curve = MakeStraightNurbs();
    ASSERT_FALSE(curve->GetLinearSegments().empty());
    // eps を微小にしても直線部短絡により中間頂点は生成されない
    const auto poly = Approx(*curve, EmptyPolygon(), EmptyPolygon(), 1e-6);
    ASSERT_EQ(poly.Count(), 2);
    EXPECT_NEAR(poly.curve_params[0], 0.0, kParamTol);
    EXPECT_NEAR(poly.curve_params[1], 1.0, kParamTol);
    ExpectStructurallyValid(*curve, poly);
}

// 半円: 各辺の中点距離が eps 以内 (近似精度の核心)
TEST(PolygonalApproximationTest, HalfCircle_EachEdgeMidpointWithinEps) {
    const auto curve = MakeHalfCircle();
    const double eps = 1e-3;
    const auto poly = Approx(*curve, EmptyPolygon(), EmptyPolygon(), eps);
    EXPECT_GE(poly.Count(), 3);
    ExpectStructurallyValid(*curve, poly);
    ExpectEdgesWithinEps(*curve, poly, eps);
    // 全頂点が単位円上にあること (独立な正しさの確認)
    for (const auto& v : poly.vertices) EXPECT_NEAR(v.norm(), 1.0, kEvalTol);
}

// 混在閉ループ: 実 insc/circ を与えた出力が構造的に妥当
TEST(PolygonalApproximationTest, ArchBandLoop_StructurallyValid) {
    const std::vector<std::pair<std::string, std::shared_ptr<ICurve>>> loops = {
        {"ArchBand1", i_test::MakeLoopArchBand1()},
        {"ArchBand2", i_test::MakeLoopArchBand2()}};
    for (const auto& [name, curve] : loops) {
        SCOPED_TRACE("Loop: " + name);
        auto [circ, insc] = ExtremalPair(*curve);
        const auto poly = Approx(*curve, insc, circ, 1e-3);
        ExpectStructurallyValid(*curve, poly);
        EXPECT_GE(poly.Count(), 3);
    }
}

// 固定点保存: insc/circ の on_curve パラメータは全て出力に含まれる
TEST(PolygonalApproximationTest, ArchBandLoop_PreservesFixedParams) {
    const auto curve = i_test::MakeLoopArchBand1();
    auto [circ, insc] = ExtremalPair(*curve);
    const auto poly = Approx(*curve, insc, circ, 1e-3);
    const auto [t_min, t_max] = curve->GetParameterRange();

    for (const auto* pd : {&insc, &circ}) {
        for (size_t i = 0; i < pd->vertices.size(); ++i) {
            if (!pd->on_curve[i]) continue;
            const double p = pd->curve_params[i];
            // 閉曲線では末尾 t_max が除去されうるため対象外
            if (curve->IsClosed() && std::abs(p - t_max) < 1e-9) continue;
            bool found = false;
            for (double q : poly.curve_params) {
                if (std::abs(q - p) < 1e-9) { found = true; break; }
            }
            EXPECT_TRUE(found) << "fixed param " << p << " missing from output";
        }
    }
}

// 空 insc/circ: 開曲線でも例外なく離散化される (ヘッダ記載の挙動)
TEST(PolygonalApproximationTest, EmptyPolygons_AcceptedOnOpenCurve) {
    const auto curve = MakeHalfCircle();
    PolygonData poly;
    EXPECT_NO_THROW(poly = Approx(*curve, EmptyPolygon(), EmptyPolygon(), 1e-2));
    ExpectStructurallyValid(*curve, poly);
    EXPECT_GE(poly.Count(), 2);
}



/**
 * 正常系・境界値
 */

// 巨大 eps: 全区間が即時に距離判定を満たし固定点のみ残る
TEST(PolygonalApproximationTest, HalfCircle_HugeEps_NoInteriorVertices) {
    const auto curve = MakeHalfCircle();
    const auto poly = Approx(*curve, EmptyPolygon(), EmptyPolygon(), 1e9);
    ASSERT_EQ(poly.Count(), 2);
    EXPECT_NEAR(poly.curve_params[0], 0.0, kParamTol);
    EXPECT_NEAR(poly.curve_params[1], kPi, kParamTol);
}

// eps を小さくすると頂点数は単調に増える (非減少)
TEST(PolygonalApproximationTest, HalfCircle_SmallerEps_RefinesMonotonically) {
    const auto curve = MakeHalfCircle();
    const auto coarse = Approx(*curve, EmptyPolygon(), EmptyPolygon(), 1e-1);
    const auto fine = Approx(*curve, EmptyPolygon(), EmptyPolygon(), 1e-3);
    EXPECT_GE(fine.Count(), coarse.Count());
}

// max_depth=0: 距離判定を必ず失敗させ終了条件③を誘発 → 中点1点だけ追加
TEST(PolygonalApproximationTest, HalfCircle_MaxDepthZero_AddsSingleMidpoint) {
    const auto curve = MakeHalfCircle();
    const auto poly = Approx(*curve, EmptyPolygon(), EmptyPolygon(), 1e-12, 0);
    ASSERT_EQ(poly.Count(), 3);
    EXPECT_NEAR(poly.curve_params[0], 0.0, kParamTol);
    EXPECT_NEAR(poly.curve_params[1], kPi / 2.0, kParamTol);
    EXPECT_NEAR(poly.curve_params[2], kPi, kParamTol);
    ExpectStructurallyValid(*curve, poly);
}



/**
 * 正常系・退化
 */

// 全円 (閉): 末尾 t_max (= t_min と同一点) が除去される
TEST(PolygonalApproximationTest, FullCircle_DropsTrailingDuplicate) {
    const auto curve = MakeFullCircle();
    ASSERT_TRUE(curve->IsClosed());
    const auto [t_min, t_max] = curve->GetParameterRange();
    const auto poly = Approx(*curve, EmptyPolygon(), EmptyPolygon(), 1e-2);
    EXPECT_GE(poly.Count(), 3);
    // 末尾パラメータが t_max でないこと (重複点が除去済み)
    EXPECT_GT(std::abs(poly.curve_params.back() - t_max), 1e-9)
            << "trailing t_max not removed";
    // 先頭と末尾の頂点が異なること
    EXPECT_GT((poly.vertices.front() - poly.vertices.back()).norm(), 1e-6);
    ExpectStructurallyValid(*curve, poly);
}

// 固定点が範囲外: clamp により例外なく評価され、生パラメータは保持される
// (高速化での点再利用が境界クランプ挙動を崩さないことのロック)
TEST(PolygonalApproximationTest, FixedParamOutsideRange_ClampsWithoutThrow) {
    const auto curve = MakeHalfCircle();
    const auto [t_min, t_max] = curve->GetParameterRange();
    // on_curve パラメータをわずかに範囲外へ置いた内包多角形を手構成する
    PolygonData insc;
    insc.vertices = {Vector3d(0.0, 0.0, 0.0), Vector3d(0.0, 0.0, 0.0)};
    insc.on_curve = {true, true};
    insc.curve_params = {t_min - 1e-9, t_max + 1e-9};

    PolygonData poly;
    ASSERT_NO_THROW(poly = Approx(*curve, insc, EmptyPolygon(), 1e-2));
    ExpectStructurallyValid(*curve, poly);
    // 生パラメータは範囲外のまま保持され、頂点は端点へクランプ評価される
    EXPECT_NEAR(poly.curve_params.front(), t_min - 1e-9, kParamTol);
    EXPECT_NEAR(poly.curve_params.back(), t_max + 1e-9, kParamTol);
    EXPECT_LE((poly.vertices.front() - curve->GetPointAt(t_min)).norm(), kEvalTol);
    EXPECT_LE((poly.vertices.back() - curve->GetPointAt(t_max)).norm(), kEvalTol);
}



/**
 * 特性化フィンガープリント採取 (非決定論的ケースの基準値取得用)
 *
 * 既定では無効。高速化リファクタの前後で出力不変を確認したい場合は
 * --gtest_also_run_disabled_tests で実行し、出力された (count, sum/min/max
 * param, centroid) を比較する。
 */
TEST(PolygonalApproximationTest, DISABLED_RecordFingerprints) {
    struct Case {
        std::string name;
        std::shared_ptr<ICurve> curve;
        bool closed_loop;  // true なら ExtremalPair で insc/circ を生成
        double eps;
    };
    std::vector<Case> cases = {
        {"HalfCircle@1e-3", MakeHalfCircle(), false, 1e-3},
        {"ArchBand1@1e-3", i_test::MakeLoopArchBand1(), true, 1e-3},
        {"ArchBand2@1e-3", i_test::MakeLoopArchBand2(), true, 1e-3}};

    for (const auto& c : cases) {
        PolygonData insc, circ;
        if (c.closed_loop) std::tie(circ, insc) = ExtremalPair(*c.curve);
        const auto poly = Approx(*c.curve, insc, circ, c.eps);
        const auto f = ComputeFingerprint(poly);
        std::cout << "[fingerprint] " << c.name
                  << " count=" << f.count
                  << " sum=" << f.sum_param
                  << " min=" << f.min_param
                  << " max=" << f.max_param
                  << " centroid=(" << f.centroid.x() << ", "
                  << f.centroid.y() << ", " << f.centroid.z() << ")\n";
    }
}
