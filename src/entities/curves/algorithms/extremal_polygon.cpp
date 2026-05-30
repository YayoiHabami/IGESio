/**
 * @file entities/curves/algorithms/extremal_polygon.cpp
 * @brief 閉曲線の外包/内包多角形を構築するアルゴリズムの実装
 * @author Yayoi Habami
 * @date 2026-04-10
 * @copyright 2026 Yayoi Habami
 */
#include "entities/curves/algorithms/extremal_polygon.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "igesio/numerics/matrix.h"
#include "igesio/numerics/polygon.h"
#include "igesio/numerics/optimization.h"
#include "igesio/entities/interfaces/i_curve.h"



namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using igesio::Vector3d;

// ===========================================================================
// 補助: 符号
// ===========================================================================

/// @brief 有限値の符号を返す (+∞ → +1.0, -∞ → -1.0, 0.0 → 0.0)
double FiniteSign(double v) {
    if (v > 0.0 || v == std::numeric_limits<double>::infinity()) return 1.0;
    if (v < 0.0 || v == -std::numeric_limits<double>::infinity()) return -1.0;
    return 0.0;
}

// ===========================================================================
// 平面基底・投影
// ===========================================================================

/// @brief 参照法線から平面内の正規直交基底 (u, v) を構築する
///
/// n_hat × candidateをu、n_hat × u をvとして直交基底を生成する.
///
/// @param normal 平面の法線ベクトル (正規化不要)
/// @param[out] u 平面内の基底ベクトル (正規化済み)
/// @param[out] v 平面内の基底ベクトル (正規化済み, u ⊥ v)
/// @throws std::invalid_argument normalがゼロベクトルの場合
std::pair<Vector3d, Vector3d> BuildPlaneBasis(const Vector3d& normal) {
    const double norm = normal.norm();
    if (norm < 1e-12) {
        throw std::invalid_argument(
            "reference_normal はゼロベクトルであってはなりません");
    }
    const Vector3d n_hat = normal / norm;

    // n_hatと平行でないベクトルを選ぶ
    const Vector3d candidate = (std::abs(n_hat.x()) < 0.9)
        ? Vector3d(1.0, 0.0, 0.0)
        : Vector3d(0.0, 1.0, 0.0);

    Vector3d u = n_hat.cross(candidate).normalized();
    // vについてはn_hatとuが共に単位ベクトルで直交するため正規化不要
    Vector3d v = n_hat.cross(u);
    return {u, v};
}

/// @brief 3次元点を平面に射影して2次元座標を返す
///
/// @param pt 射影する3次元点
/// @param u 平面内の基底ベクトル (正規化済み)
/// @param v 平面内の基底ベクトル (正規化済み)
/// @return (pt·u, pt·v) の2次元座標
std::array<double, 2> ProjectTo2D(const Vector3d& pt,
                                  const Vector3d& u,
                                  const Vector3d& v) {
    return {pt.dot(u), pt.dot(v)};
}

// ===========================================================================
// 2次元ジオメトリ補助
// ===========================================================================

/// @brief 2次元ベクトル (ax, ay) と (bx, by) の外積 (スカラー) を返す
double Cross2D(double ax, double ay, double bx, double by) {
    return ax * by - ay * bx;
}

/// @brief 2次元多角形の符号付き面積を返す (shoelace formula)
///
/// @param pts_2d 頂点の2次元座標列 (std::array<double, 2> のベクトル)
/// @return 符号付き面積. 正 → CCW, 負 → CW
double SignedArea2D(const std::vector<std::array<double, 2>>& pts_2d) {
    const int n = static_cast<int>(pts_2d.size());
    double area = 0.0;
    for (int i = 0; i < n; ++i) {
        const int j = (i + 1) % n;
        area += pts_2d[i][0] * pts_2d[j][1]
              - pts_2d[j][0] * pts_2d[i][1];
    }
    return 0.5 * area;
}

/// @brief 2次元線分 (a, b) と (c, d) が交差するかを判定する
///
/// 端点共有は交差とみなさない (縮退ケースはfalseを返す).
///
/// @param a, b 線分1の端点 (2次元, std::array<double, 2>)
/// @param c, d 線分2の端点 (2次元, std::array<double, 2>)
/// @return 真に交差する場合true
bool SegmentsIntersect2D(const std::array<double, 2>& a,
                         const std::array<double, 2>& b,
                         const std::array<double, 2>& c,
                         const std::array<double, 2>& d) {
    const double d1 = Cross2D(d[0]-c[0], d[1]-c[1], a[0]-c[0], a[1]-c[1]);
    const double d2 = Cross2D(d[0]-c[0], d[1]-c[1], b[0]-c[0], b[1]-c[1]);
    const double d3 = Cross2D(b[0]-a[0], b[1]-a[1], c[0]-a[0], c[1]-a[1]);
    const double d4 = Cross2D(b[0]-a[0], b[1]-a[1], d[0]-a[0], d[1]-a[1]);
    return (d1 * d2 < 0.0) && (d3 * d4 < 0.0);
}

/// @brief 点が2次元単純多角形の内部または境界(eps以内)にあるかを判定する
///
/// 境界は辺までの距離 <= eps で判定し、内部は even-odd ray casting (向き非依存)
/// で判定する. 含有不変条件の検証(FindContainmentViolations)に用いる.
///
/// @param p 判定する点 (2次元)
/// @param poly 多角形の頂点列 (2次元)
/// @param eps 境界とみなす距離の閾値
/// @return 内部または境界上にある場合 true
bool PointInOrOnPolygon2D(const std::array<double, 2>& p,
                          const std::vector<std::array<double, 2>>& poly,
                          double eps) {
    const int n = static_cast<int>(poly.size());
    if (n < 3) return false;

    // 境界判定: 各辺までの距離が eps 以下なら境界上とみなす
    for (int i = 0; i < n; ++i) {
        const auto& a = poly[i];
        const auto& b = poly[(i + 1) % n];
        const double abx = b[0] - a[0], aby = b[1] - a[1];
        const double len2 = abx * abx + aby * aby;
        double s = 0.0;
        if (len2 > 0.0) {
            s = ((p[0] - a[0]) * abx + (p[1] - a[1]) * aby) / len2;
            s = std::max(0.0, std::min(1.0, s));
        }
        const double dx = p[0] - (a[0] + s * abx);
        const double dy = p[1] - (a[1] + s * aby);
        if (dx * dx + dy * dy <= eps * eps) return true;
    }

    // 内部判定: even-odd ray casting
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

// ===========================================================================
// 直線交差 (3D)
// ===========================================================================

/// @brief 3次元空間の2直線の交点を求める
///
/// 直線1: p1 + s*d1, 直線2: p2 + t*d2 の交点を返す.
/// 共面かつ非平行であることを前提とする.
/// 平行 (|d1 × d2|² < 1e-24) の場合は中点を返す.
///
/// @param p1 直線1上の基準点
/// @param d1 直線1の方向ベクトル
/// @param p2 直線2上の基準点
/// @param d2 直線2の方向ベクトル
/// @return 交点の座標
Vector3d LineIntersect(const Vector3d& p1, const Vector3d& d1,
                       const Vector3d& p2, const Vector3d& d2) {
    const Vector3d cross = d1.cross(d2);
    const double cross_sq = cross.squaredNorm();

    // 平行または一致 → 中点を返す
    if (cross_sq < 1e-24) return 0.5 * (p1 + p2);

    // s = ((p2 - p1) × d2) · (d1 × d2) / |d1 × d2|²
    const double s = (p2 - p1).cross(d2).dot(cross) / cross_sq;
    return p1 + s * d1;
}

// ===========================================================================
// 接線取得
// ===========================================================================

/// @brief 曲線上の点tにおける「前進方向」単位接線ベクトルを返す
///
/// 角点では右側接線T⁺(t) (TryGetRightTangentAt) を使用し、通常点では
/// TryGetTangentAtを使用する.
///
/// @param curve 対象曲線
/// @param t パラメータ値
/// @return 前進方向単位接線ベクトル. 取得できない場合はstd::nullopt
std::optional<Vector3d>
GetForwardTangentAt(const i_ent::ICurve& curve, double t) {
    if (curve.IsCorner(t)) return curve.TryGetRightTangentAt(t);
    return curve.TryGetTangentAt(t);
}

/// @brief 曲線上の点tにおける「到達方向」単位接線ベクトルを返す
///
/// 角点では左側接線T⁻(t) (TryGetLeftTangentAt) を使用し、通常点では
/// TryGetTangentAtを使用する. cc/vc で「Pbへ到達する側」の接線として用いる.
/// 平滑点ではT⁻=T⁺で前進方向と一致するが、角点では弧側(到達側)の接線となる.
///
/// @param curve 対象曲線
/// @param t パラメータ値
/// @return 到達方向の単位接線ベクトル. 取得できない場合はstd::nullopt
std::optional<Vector3d>
GetBackwardTangentAt(const i_ent::ICurve& curve, double t) {
    if (curve.IsCorner(t)) return curve.TryGetLeftTangentAt(t);
    return curve.TryGetTangentAt(t);
}

// ===========================================================================
// 符号付き曲率のゼロ点探索
// ===========================================================================

/// @brief 区間 [ta, tb] で符号付き曲率 κ_s(t) ≈ 0 となるtを探す
///
/// 閉曲線の周期的折り返し (ta > tb のケース) に対応する.
/// Brentq (TOMS 748) を使用して精密化する.
///
/// @param curve 対象曲線
/// @param ta 区間始端
/// @param tb 区間終端 (ta <= tb; 折り返し時は tb + period として渡す)
/// @param normal 符号付き曲率の基準法線
/// @param eps 許容誤差
/// @return κ_s ≈ 0 となるt. 見つからない場合は区間中点
double FindZeroCurvature(const i_ent::ICurve& curve,
                         double ta, double tb,
                         const Vector3d& normal,
                         double eps = 1e-9) {
    const auto t_range = curve.GetParameterRange();
    const double t_min = t_range[0], t_max = t_range[1];
    const double period = t_max - t_min;

    // 折り返しケース (ta > tb): tbを一周分延ばして単調区間に変換する
    if (ta > tb) tb = t_max + (tb - t_min);

    // κ_sを評価するラムダ: t > t_max の場合に正規化する
    auto kappa = [&](double t) -> double {
        double t_eval = t;
        if (t_eval > t_max) t_eval -= period;
        const auto k = curve.TryGetSignedCurvature(t_eval, normal);
        return k.value_or(0.0);
    };

    double fa = kappa(ta);
    double fb = kappa(tb);

    // 符号が同じなら細かくサンプリングして符号変化点を探す
    if (FiniteSign(fa) == FiniteSign(fb)) {
        constexpr int kNSample = 64;
        std::vector<double> ts(kNSample + 1);
        std::vector<double> ks(kNSample + 1);
        for (int i = 0; i <= kNSample; ++i) {
            ts[i] = ta + (tb - ta) * i / kNSample;
            ks[i] = kappa(ts[i]);
        }
        bool found = false;
        for (int i = 0; i < kNSample; ++i) {
            if (FiniteSign(ks[i]) != FiniteSign(ks[i + 1])) {
                ta = ts[i];
                tb = ts[i + 1];
                fa = ks[i];
                fb = ks[i + 1];
                found = true;
                break;
            }
        }
        if (!found) {
            // 符号変化なし → 区間中点を返す
            double t_mid = 0.5 * (ta + tb);
            if (t_mid > t_max) t_mid -= period;
            return t_mid;
        }
    }

    // ±∞の端点を少しずらして有限値にする
    const double h = 1e-8 * (tb - ta);
    if (!std::isfinite(fa)) ta += h;
    if (!std::isfinite(fb)) tb -= h;

    // 再符号確認
    fa = kappa(ta);
    fb = kappa(tb);
    if (fa * fb > 0.0) {
        double t_mid = 0.5 * (ta + tb);
        if (t_mid > t_max) t_mid -= period;
        return t_mid;
    }

    try {
        double result = i_num::FindRootScalar(
            kappa, ta, tb, eps * std::abs(tb - ta), 200);
        if (result > t_max) result -= period;
        // ブラケット[ta,tb]内に角点(曲率∞)が含まれると、toms748の補間が
        // 非有限なresultを返し得る。その場合は契約どおり区間中点へフォール
        // バックする(∞をboostのルート探索に渡すとNaNになるのを防ぐ)。
        if (!std::isfinite(result)) {
            double t_mid = 0.5 * (ta + tb);
            if (t_mid > t_max) t_mid -= period;
            return t_mid;
        }
        return result;
    } catch (const std::exception&) {
        double t_mid = 0.5 * (ta + tb);
        if (t_mid > t_max) t_mid -= period;
        return t_mid;
    }
}

// ===========================================================================
// 直線区間判定
// ===========================================================================

/// @brief パラメータtが直線部の内部 (端点を除く) に含まれるかを判定する
bool InLinear(const std::vector<std::array<double, 2>>& segs,
              double t, double eps = 1e-9) {
    for (const auto& seg : segs) {
        if (seg[0] + eps < t && t < seg[1] - eps) return true;
    }
    return false;
}

/// @brief 区間 [ta, tb] が直線部の始端・終端と一致するかを判定する
bool IsLinearInterval(const std::vector<std::array<double, 2>>& segs,
                      double ta, double tb, double eps = 1e-9) {
    for (const auto& seg : segs) {
        if (std::abs(ta - seg[0]) < eps && std::abs(tb - seg[1]) < eps) {
            return true;
        }
    }
    return false;
}

// ===========================================================================
// 重複除去
// ===========================================================================

/// @brief 近すぎる点を重複とみなして除去する
///
/// @param pts 重複除去前のパラメータ値列 (昇順であること)
/// @param is_closed 閉曲線か否か
/// @param period 曲線の周期 (GetParameterRangeの幅)
/// @param merge_tol 重複とみなす閾値 (periodに対する比率)
/// @return 重複除去後のパラメータ値列
std::vector<double> Dedup(const std::vector<double>& pts,
                          bool is_closed,
                          double period,
                          double merge_tol) {
    if (pts.empty()) return pts;

    std::vector<double> sorted_pts = pts;
    std::sort(sorted_pts.begin(), sorted_pts.end());

    std::vector<double> merged;
    merged.push_back(sorted_pts[0]);

    for (size_t i = 1; i < sorted_pts.size(); ++i) {
        double gap = sorted_pts[i] - merged.back();
        if (is_closed) gap = std::min(gap, period - gap);
        if (gap > merge_tol * period) merged.push_back(sorted_pts[i]);
    }

    // 閉曲線: 先頭と末尾が周期的に近い場合を除去
    if (is_closed && merged.size() >= 2) {
        const double circ_gap = (merged[0] + period) - merged.back();
        if (circ_gap < merge_tol * period) merged.pop_back();
    }
    return merged;
}

// ===========================================================================
// 補間・挿入
// ===========================================================================

/// @brief 点数が少ない場合に n_vert 付近になるよう各間隔を等間隔に補間する
///
/// @param ts 補間前のパラメータ値列
/// @param t0 パラメータ範囲の下限
/// @param t1 パラメータ範囲の上限
/// @param n_vert 目標頂点数
/// @param skip_fn 区間 (ta, tb) をスキップするか判定する関数.
///        nullptrなら全区間を補間
/// @return 補間後のパラメータ値列
std::vector<double> InterpolateClosedPoints(
        const std::vector<double>& ts,
        double t0, double t1, int n_vert,
        const std::function<bool(double, double)>& skip_fn = nullptr) {
    const int num_pts = static_cast<int>(ts.size());
    if (num_pts == 0) return {};

    const double n = static_cast<double>(n_vert) / num_pts;
    if (n <= 1.0) return ts;

    const int n_int = static_cast<int>(std::floor(n));
    std::vector<double> result;

    for (int i = 0; i < num_pts; ++i) {
        const double start_t = ts[i];
        double total_gap = 0.0;
        double end_t_for_check = 0.0;

        if (i < num_pts - 1) {
            end_t_for_check = ts[i + 1];
            total_gap = end_t_for_check - start_t;
        } else {
            end_t_for_check = ts[0];
            total_gap = (t1 - start_t) + (ts[0] - t0);
        }

        // 直線区間はスキップ (始端のみ追加)
        if (skip_fn && skip_fn(start_t, end_t_for_check)) {
            result.push_back(start_t);
            continue;
        }

        // 各区間を n_int 個に分割して追加
        for (int j = 0; j < n_int; ++j) {
            const double delta = total_gap * j / n_int;
            double t_val = start_t + delta;

            if (t_val > t1) {
                t_val = t0 + (t_val - t1);
            } else if (t_val < t0) {
                t_val = t1 - (t0 - t_val);
            }
            result.push_back(t_val);
        }
    }
    return result;
}

/// @brief 角点のパラメータ値をサンプル点列に追加して昇順に返す
std::vector<double> InsertCorners(const i_ent::ICurve& curve,
                                  std::vector<double> ts) {
    const auto [t0, t1] = curve.GetParameterRange();
    for (double tc : curve.GetCornerParams()) {
        if (tc < t0 || tc >= t1) continue;
        bool already = false;
        for (double t : ts) {
            if (std::abs(t - tc) < 1e-12) {
                already = true;
                break;
            }
        }
        if (!already) ts.push_back(tc);
    }
    std::sort(ts.begin(), ts.end());
    return ts;
}

/// @brief 直線部の始端・終端をサンプル点列に追加して昇順に返す
/// @param segs 直線区間のリスト
/// @param ts 追加先のパラメータ値列
/// @param t0 パラメータ範囲の下限
/// @param t1 パラメータ範囲の上限
/// @note 端点を [t0, t1) にクランプする. 継ぎ目 t==t1 は閉曲線で C(t1)=C(t0) と
///       重複し、ゼロ長辺の原因となるため除外する (InsertCorners と整合)
std::vector<double> InsertLinearEndpoints(
        const std::vector<std::array<double, 2>>& segs,
        std::vector<double> ts, double t0, double t1) {
    const double seam_tol = 1e-9 * (t1 - t0);
    for (const auto& seg : segs) {
        for (const double endpoint : {seg[0], seg[1]}) {
            // 範囲外・継ぎ目 (t1≡t0) は除外する
            if (endpoint < t0 || endpoint >= t1 - seam_tol) continue;
            bool already = false;
            for (double t : ts) {
                if (std::abs(t - endpoint) < 1e-12) {
                    already = true;
                    break;
                }
            }
            if (!already) ts.push_back(endpoint);
        }
    }
    std::sort(ts.begin(), ts.end());
    return ts;
}

// ===========================================================================
// 曲線の基本性質の検証
// ===========================================================================

/// @brief 均等サンプリング点列を用いて曲線の向きと自己交差を検証する
///
/// 以下の検証を行う:
///   1. IsClosed() による閉曲線チェック
///   2. 符号付き面積 (shoelace) から向き符号を計算
///   3. 近似多角形の自己交差チェック (O(N²))
///
/// @param curve 対象曲線
/// @param sample_pts 均等サンプリングの3次元点列 (n_init 個)
/// @param normal 平面の参照法線ベクトル
/// @return orientation_sign: +1.0 (reference_normal から見て CCW) / -1.0 (CW)
/// @throws std::invalid_argument 閉曲線でない場合, 自己交差が検出された場合
double CheckCurveProperties(const i_ent::ICurve& curve,
                             const std::vector<Vector3d>& sample_pts,
                             const Vector3d& normal) {
    // 閉曲線チェック
    if (!curve.IsClosed()) {
        throw std::invalid_argument(
            "曲線が閉じていません。閉曲線にのみ対応しています。");
    }

    // 平面基底を構築して全点を2次元に射影する
    auto [u, v] = BuildPlaneBasis(normal);

    const int n = static_cast<int>(sample_pts.size());
    std::vector<std::array<double, 2>> pts_2d(n);
    for (int i = 0; i < n; ++i) {
        pts_2d[i] = ProjectTo2D(sample_pts[i], u, v);
    }

    // 向き符号: 符号付き面積の符号
    const double area = SignedArea2D(pts_2d);
    const double orient_sign = (area >= 0.0) ? 1.0 : -1.0;

    // 自己交差チェック: 非隣接辺ペアを全て確認する
    for (int i = 0; i < n; ++i) {
        const int i_next = (i + 1) % n;
        // j は i および i の隣接辺を除く
        for (int j = i + 2; j < n; ++j) {
            if (i == 0 && j == n - 1) continue;  // 始端と末端の共有辺
            const int j_next = (j + 1) % n;
            if (SegmentsIntersect2D(pts_2d[i], pts_2d[i_next],
                                    pts_2d[j], pts_2d[j_next])) {
                throw std::invalid_argument(
                    "曲線に自己交差が検出されました。"
                    "自己交差のない曲線にのみ対応しています。");
            }
        }
    }

    return orient_sign;
}

// ===========================================================================
// 曲率極値探索
// ===========================================================================

/// @brief 符号付き曲率 κ_s(t) の極大・極小点を探索する
///
/// @param curve 対象曲線
/// @param normal 符号付き曲率の基準法線
/// @param t_uniform 均等分割されたパラメータ値列 (初期サンプリング点)
/// @param tol 精密化の許容誤差
/// @param merge_tol 重複除去の閾値 (period に対する比率)
/// @return (maxima, minima): 極大点と極小点のパラメータ値列
std::pair<std::vector<double>, std::vector<double>> FindCurvatureExtrema(
        const i_ent::ICurve& curve,
        const Vector3d& normal,
        const std::vector<double>& t_uniform,
        double tol = 1e-9,
        double merge_tol = 1e-4) {
    const auto t_range = curve.GetParameterRange();
    const double t0 = t_range[0], t1 = t_range[1];
    const double period = t1 - t0;
    const auto linear_segs = curve.GetLinearSegments();

    // 直線部を除外してから曲率サンプリングを行う
    std::vector<double> ts;
    ts.reserve(t_uniform.size());
    for (double t : t_uniform) {
        if (!InLinear(linear_segs, t)) ts.push_back(t);
    }
    if (ts.empty()) return {{}, {}};

    const int n = static_cast<int>(ts.size());
    std::vector<double> kappa(n);
    for (int i = 0; i < n; ++i) {
        const auto k = curve.TryGetSignedCurvature(ts[i], normal);
        kappa[i] = k.value_or(0.0);
    }

    // dt: 均等サンプリングの間隔を近似する
    const double dt = period / n;

    std::vector<double> maxima, minima;

    auto kappa_fn = [&](double t) -> double {
        const auto k = curve.TryGetSignedCurvature(t, normal);
        return k.value_or(0.0);
    };

    for (int i = 0; i < n; ++i) {
        const int prev = (i - 1 + n) % n;
        const int nxt  = (i + 1) % n;
        const double k_p = kappa[prev];
        const double k_i = kappa[i];
        const double k_n = kappa[nxt];

        // 角点では曲率が±∞となる (TryGetSignedCurvature)。∞は平滑な曲率の
        // 極値ではなく、角点はInsertCornersで別途追加されるため、ここでの
        // 極値探索の対象から除外する。∞をMinimizeScalar (boost) に渡すと
        // 放物線補間でNaNのt_optが返り、後段のGetPointAtで例外となるのを防ぐ。
        if (!std::isfinite(k_i)) continue;

        const double lb = ts[i] - dt;
        const double ub = ts[i] + dt;

        // 局所極大: -κ を最小化
        if (k_i > k_p && k_i > k_n) {
            try {
                const auto res = i_num::MinimizeScalar(
                    [&](double t) { return -kappa_fn(t); },
                    lb, ub, tol);
                // ブラケット[lb,ub]が角点(∞)を含む場合、boostが非有限な
                // t_optを返し得る。その場合はサンプル点にフォールバックする。
                maxima.push_back(std::isfinite(res.t_opt) ? res.t_opt : ts[i]);
            } catch (const std::exception&) {
                maxima.push_back(ts[i]);
            }
        }

        // 局所極小: κ を最小化
        if (k_i < k_p && k_i < k_n) {
            try {
                const auto res = i_num::MinimizeScalar(
                    kappa_fn, lb, ub, tol);
                minima.push_back(std::isfinite(res.t_opt) ? res.t_opt : ts[i]);
            } catch (const std::exception&) {
                minima.push_back(ts[i]);
            }
        }
    }

    // t を [t0, t1) に正規化する
    auto normalize = [&](double t) -> double {
        while (t < t0) t += period;
        while (t >= t1) t -= period;
        return t;
    };
    for (auto& t : maxima) t = normalize(t);
    for (auto& t : minima) t = normalize(t);

    return {Dedup(maxima, curve.IsClosed(), period, merge_tol),
            Dedup(minima, curve.IsClosed(), period, merge_tol)};
}

// ===========================================================================
// サンプリング
// ===========================================================================

/// @brief 多角形構築のためのサンプル点を生成する
///
/// 曲率極値点を起点に、InterpolateClosedPoints → InsertCorners →
/// InsertLinearEndpoints の順に適用する.
/// 極値が見つからない場合は均等サンプリングにフォールバックする.
///
/// @param curve 対象曲線
/// @param normal 符号付き曲率の基準法線
/// @param t_uniform 均等分割されたパラメータ値列 (CheckCurveProperties と共用)
/// @param n_vert 目標頂点数
/// @return (ts, pts): サンプル点のパラメータ値列と3次元座標列
std::pair<std::vector<double>, std::vector<Vector3d>>
SamplePoints(const i_ent::ICurve& curve,
             const Vector3d& normal,
             const std::vector<double>& t_uniform,
             int n_vert) {
    const auto [t0, t1] = curve.GetParameterRange();
    const double period = t1 - t0;
    const auto linear_segs = curve.GetLinearSegments();

    auto [maxima, minima] = FindCurvatureExtrema(curve, normal, t_uniform);

    std::vector<double> ts;

    if (!maxima.empty() || !minima.empty()) {
        // 極値点を起点に補間する
        ts = maxima;
        ts.insert(ts.end(), minima.begin(), minima.end());
        std::sort(ts.begin(), ts.end());

        ts = InterpolateClosedPoints(ts, t0, t1, n_vert,
                                     [&](double ta, double tb) {
                return IsLinearInterval(linear_segs, ta, tb);
            });
    } else {
        // 極値なし → 均等サンプリング (直線部内部は除外)
        ts.reserve(n_vert);
        for (int i = 0; i < n_vert; ++i) {
            const double t = t0 + period * i / n_vert;
            if (!InLinear(linear_segs, t)) ts.push_back(t);
        }
    }

    ts = InsertCorners(curve, ts);
    ts = InsertLinearEndpoints(linear_segs, ts, t0, t1);

    // 3次元座標を計算する
    std::vector<Vector3d> pts;
    pts.reserve(ts.size());
    for (double t : ts) pts.push_back(curve.GetPointAt(t));
    return {ts, pts};
}

// ===========================================================================
// 点分類
// ===========================================================================

/// @brief 各サンプル点が頂点 (false) か接点 (true) かを分類する
///
/// 外包 (circumscribed=true):
///   内側に凸 → 頂点 (false), 外側に凸 → 接点 (true)
/// 内包 (circumscribed=false):
///   外側に凸 → 頂点 (false), 内側に凸 → 接点 (true)
///
/// 凸性判定:
///   is_outward = (κ_s * orient_sign) > eps
///   is_inward  = (κ_s * orient_sign) < -eps
///
/// @param curve 対象曲線
/// @param ts サンプル点のパラメータ値列
/// @param circumscribed 外包なら true, 内包なら false
/// @param normal 符号付き曲率の基準法線
/// @param orient_sign 向き符号 (+1: CCW, -1: CW)
/// @param eps 曲率判定の閾値
/// @return 各点の分類結果 (true: 接点, false: 頂点)
std::vector<bool> ClassifyPoints(const i_ent::ICurve& curve,
                                 const std::vector<double>& ts,
                                 bool circumscribed,
                                 const Vector3d& normal,
                                 double orient_sign,
                                 double eps = 1e-9) {
    std::vector<bool> result;
    result.reserve(ts.size());

    for (double t : ts) {
        const auto k_opt = curve.TryGetSignedCurvature(t, normal);
        if (!k_opt.has_value()) {
            // 計算不能 → 保守的に頂点とする
            result.push_back(false);
            continue;
        }
        const double kappa_s = *k_opt;
        const double kappa_oriented = kappa_s * orient_sign;

        const bool is_outward = kappa_oriented > eps;
        const bool is_inward  = kappa_oriented < -eps;

        if (circumscribed) {
            // 外包: inward → 頂点(false), outward → 接点(true)
            result.push_back(is_outward);
        } else {
            // 内包: outward → 頂点(false), inward → 接点(true)
            result.push_back(is_inward);
        }
    }
    return result;
}

// ===========================================================================
// 多角形頂点の構築
// ===========================================================================

/// @brief 隣接ペアのパターンに従って多角形の頂点列を構築する
///
/// 各隣接ペア (i, j) のパターン:
///   vv (vertex-vertex): Pa を追加するのみ
///   cc (contact-contact): 接線交点を頂点とする
///   vc (vertex-contact): ゼロ曲率点 Pm を挿入し Pm-Pb 接線の交点を頂点とする
///   cv (contact-vertex): ゼロ曲率点 Pm を挿入し Pa-Pm 接線の交点を頂点とする
///
/// @param curve 対象曲線
/// @param ts サンプル点のパラメータ値列
/// @param pts サンプル点の3次元座標列
/// @param is_contact 各点の分類 (true: 接点, false: 頂点)
/// @param normal 符号付き曲率の基準法線 (ゼロ曲率点探索に使用)
/// @return 構築された多角形データ
i_num::PolygonData BuildPolygonVertices(
        const i_ent::ICurve& curve,
        const std::vector<double>& ts,
        const std::vector<Vector3d>& pts,
        const std::vector<bool>& is_contact,
        const Vector3d& normal) {
    const int n = static_cast<int>(ts.size());
    const auto linear_segs = curve.GetLinearSegments();

    i_num::PolygonData result;
    result.vertices.reserve(n * 2);
    result.on_curve.reserve(n * 2);
    result.curve_params.reserve(n * 2);

    // 頂点追加ヘルパー
    auto add = [&](const Vector3d& pt, bool on_c, double param = 0.0) {
        result.vertices.push_back(pt);
        result.on_curve.push_back(on_c);
        result.curve_params.push_back(on_c ? param : 0.0);
    };

    for (int i = 0; i < n; ++i) {
        const int j = (i + 1) % n;
        const double ta = ts[i], tb = ts[j];
        const Vector3d& Pa = pts[i];
        const Vector3d& Pb = pts[j];
        const bool ca = is_contact[i];
        const bool cb = is_contact[j];

        // 直線区間ペアは Pa を追加するだけ
        if (IsLinearInterval(linear_segs, ta, tb)) {
            add(Pa, /*on_curve=*/true, ta);
            continue;
        }

        // Pa は前進方向(出発側)、Pb は後退方向(到達側)の接線を取得する。
        // 角点では出発側=T⁺ / 到達側=T⁻ となり、Pb が接合コーナーの場合に
        // 弧側の接線で囲えるようにする(平滑点では両者一致)。
        const auto da_opt = GetForwardTangentAt(curve, ta);
        const auto db_opt = GetBackwardTangentAt(curve, tb);

        if (!da_opt.has_value() || !db_opt.has_value()) {
            // 接線取得失敗 → 保守的に Pa のみ追加
            add(Pa, /*on_curve=*/true, ta);
            continue;
        }
        const Vector3d da = *da_opt;
        const Vector3d db = *db_opt;

        if (!ca && !cb) {
            // vv: 両方頂点 → 線分 (Pa を追加し、次ループで Pb が追加される)
            add(Pa, /*on_curve=*/true, ta);

        } else if (ca && cb) {
            // cc: 両方接点 → Pa からの接線と Pb からの逆接線の交点を頂点とする
            const Vector3d vertex = LineIntersect(Pa, da, Pb, -db);
            add(Pa,     /*on_curve=*/true,  ta);
            add(vertex, /*on_curve=*/false);

        } else if (!ca && cb) {
            // vc: Pa が頂点, Pb が接点
            // → ゼロ曲率点 Pm を挿入し、Pm 接線と Pb 逆接線の交点を頂点とする
            const double t_mid = FindZeroCurvature(curve, ta, tb, normal);
            const Vector3d Pm = curve.GetPointAt(t_mid);
            const auto dm_opt = GetForwardTangentAt(curve, t_mid);
            if (!dm_opt.has_value()) {
                add(Pa, /*on_curve=*/true, ta);
                continue;
            }
            const Vector3d dm = *dm_opt;
            const Vector3d vertex = LineIntersect(Pm, dm, Pb, -db);
            add(Pa,     /*on_curve=*/true,  ta);
            add(Pm,     /*on_curve=*/true,  t_mid);
            add(vertex, /*on_curve=*/false);

        } else {
            // cv: Pa が接点, Pb が頂点
            // → ゼロ曲率点 Pm を挿入し、Pm 逆接線と Pa 接線の交点を頂点とする
            const double t_mid = FindZeroCurvature(curve, ta, tb, normal);
            const Vector3d Pm = curve.GetPointAt(t_mid);
            const auto dm_opt = GetForwardTangentAt(curve, t_mid);
            if (!dm_opt.has_value()) {
                add(Pa, /*on_curve=*/true, ta);
                continue;
            }
            const Vector3d dm = *dm_opt;
            const Vector3d vertex = LineIntersect(Pm, -dm, Pa, da);
            add(Pa,     /*on_curve=*/true,  ta);
            add(vertex, /*on_curve=*/false);
            add(Pm,     /*on_curve=*/true,  t_mid);
        }
    }
    return result;
}

// ===========================================================================
// 後処理: 重複除去・含有検証
// ===========================================================================

/// @brief 多角形の隣接・継ぎ目で一致する頂点を統合してゼロ長辺を除去する
///
/// 接合点や角点で cc/vc/cv が接点と一致する頂点を出す、また直線端点が継ぎ目で
/// 重複する等により生じるゼロ長辺(隣接する同一頂点)を除去する.
/// 一致する頂点同士は on_curve=true 側(曲線パラメータ付き)を優先して残す.
///
/// @param poly 入力多角形
/// @return ゼロ長辺を除去した多角形
i_num::PolygonData DedupPolygon(const i_num::PolygonData& poly) {
    constexpr double kDedupTol = 1e-9;
    i_num::PolygonData out;
    const int n = poly.Count();
    if (n == 0) return out;

    for (int i = 0; i < n; ++i) {
        if (!out.vertices.empty() &&
            (out.vertices.back() - poly.vertices[i]).norm() <= kDedupTol) {
            // 直前と一致 → on_curve=true 側を優先して残す
            const size_t last = out.vertices.size() - 1;
            if (poly.on_curve[i] && !out.on_curve[last]) {
                out.vertices[last] = poly.vertices[i];
                out.on_curve[last] = true;
                out.curve_params[last] = poly.curve_params[i];
            }
            continue;
        }
        out.vertices.push_back(poly.vertices[i]);
        out.on_curve.push_back(poly.on_curve[i]);
        out.curve_params.push_back(poly.curve_params[i]);
    }

    // 継ぎ目 (先頭と末尾) の一致を除去する
    while (out.Count() >= 2 &&
           (out.vertices.front() - out.vertices.back()).norm() <= kDedupTol) {
        const size_t last = out.vertices.size() - 1;
        if (out.on_curve[last] && !out.on_curve[0]) {
            out.vertices[0] = out.vertices[last];
            out.on_curve[0] = true;
            out.curve_params[0] = out.curve_params[last];
        }
        out.vertices.pop_back();
        out.on_curve.pop_back();
        out.curve_params.pop_back();
    }
    return out;
}

/// @brief 含有不変条件の違反を検出し、サンプルに追加すべき曲線パラメータを返す
///
/// 外包: 曲線点(均等サンプル)が多角形の外側にあれば、その曲線パラメータを追加する.
///       追加点は次の再構築で接点(接線)として分類され、凸部を接線で囲うようになる.
/// 内包: 多角形の辺中点が曲線領域の外側にあれば、その辺の曲線区間中点を追加する.
///
/// @param poly 検証対象の多角形
/// @param circumscribed 外包なら true, 内包なら false
/// @param u, v 平面基底
/// @param t_uniform 均等サンプルのパラメータ列
/// @param region2d 曲線領域 (均等点の2次元射影; 外包では曲線点列としても使用)
/// @param period パラメータ範囲の幅
/// @param t1 パラメータ範囲の上限
/// @param eps_geom 境界とみなす距離の閾値
/// @return 追加すべき曲線パラメータの列 (空なら違反なし)
std::vector<double> FindContainmentViolations(
        const i_num::PolygonData& poly,
        bool circumscribed,
        const Vector3d& u, const Vector3d& v,
        const std::vector<double>& t_uniform,
        const std::vector<std::array<double, 2>>& region2d,
        double period, double t1, double eps_geom) {
    const int n = poly.Count();
    if (n < 3) return {};

    std::vector<std::array<double, 2>> poly2d(n);
    for (int i = 0; i < n; ++i) {
        poly2d[i] = ProjectTo2D(poly.vertices[i], u, v);
    }

    std::vector<double> add;
    if (circumscribed) {
        // 曲線点が多角形の外なら、その曲線パラメータを追加する
        for (size_t k = 0; k < t_uniform.size(); ++k) {
            if (!PointInOrOnPolygon2D(region2d[k], poly2d, eps_geom)) {
                add.push_back(t_uniform[k]);
            }
        }
    } else {
        // 多角形の辺中点が曲線領域の外なら、辺の曲線区間中点を追加する
        for (int i = 0; i < n; ++i) {
            const Vector3d mid =
                0.5 * (poly.vertices[i] + poly.vertices[(i + 1) % n]);
            if (PointInOrOnPolygon2D(ProjectTo2D(mid, u, v),
                                     region2d, eps_geom)) {
                continue;
            }
            std::pair<int, int> idx;
            try {
                idx = poly.GetCurveParamIndex(i);
            } catch (const std::exception&) {
                continue;
            }
            double pa = poly.curve_params[idx.first];
            double pb = poly.curve_params[idx.second];
            if (pb < pa) pb += period;  // 周期跨ぎ
            double tc = 0.5 * (pa + pb);
            if (tc >= t1) tc -= period;
            add.push_back(tc);
        }
    }
    return add;
}

// ===========================================================================
// 主関数
// ===========================================================================

/// @brief 外包/内包多角形の頂点列を計算する内部実装
///
/// @param curve 対象の閉曲線 (自己交差なし)
/// @param n_vert 初期分割数
/// @param circumscribed 外包なら true, 内包なら false
/// @param normal 平面の参照法線ベクトル
/// @param eps 曲率判定の閾値
/// @return 多角形データ
i_num::PolygonData ComputeExtremalPolygon(
        const i_ent::ICurve& curve,
        int n_vert,
        bool circumscribed,
        const Vector3d& normal,
        double eps) {
    if (n_vert < 3) {
        throw std::invalid_argument(
            "n_vert は 3 以上でなければなりません。n_vert: "
            + std::to_string(n_vert));
    }

    // 均等サンプリング (CheckCurveProperties と FindCurvatureExtrema で共用)
    constexpr int kNInit = 500;
    const auto [t0, t1] = curve.GetParameterRange();
    const double period = t1 - t0;

    std::vector<double> t_uniform(kNInit);
    std::vector<Vector3d> pts_uniform(kNInit);
    for (int i = 0; i < kNInit; ++i) {
        t_uniform[i] = t0 + period * i / kNInit;
        pts_uniform[i] = curve.GetPointAt(t_uniform[i]);
    }

    // 閉曲線チェック・向き符号計算・自己交差チェック
    const double orient_sign = CheckCurveProperties(
        curve, pts_uniform, normal);

    // 含有検証用の平面基底と曲線領域(均等点の射影)を用意する
    const auto [u, v] = BuildPlaneBasis(normal);
    std::vector<std::array<double, 2>> region2d(kNInit);
    for (int i = 0; i < kNInit; ++i) {
        region2d[i] = ProjectTo2D(pts_uniform[i], u, v);
    }

    // 多角形構築のためのサンプル点を生成する
    std::vector<double> ts = SamplePoints(
        curve, normal, t_uniform, n_vert).first;

    // 含有不変条件を満たすまで、違反点をサンプルに追加して再構築する。
    // (案A: 分類/構成が不変条件を満たすことを仮定せず、構築後に強制する。
    //  これにより変曲点・接合コーナー・粗標本化に起因する内包性違反を解消する。)
    constexpr int kMaxRefine = 6;
    constexpr double kViolationEps = 1e-7;
    i_num::PolygonData polygon;
    for (int iter = 0; ; ++iter) {
        std::vector<Vector3d> pts;
        pts.reserve(ts.size());
        for (double t : ts) pts.push_back(curve.GetPointAt(t));

        // 各点を頂点/接点に分類し、多角形を構築・重複除去する
        const auto is_contact = ClassifyPoints(
            curve, ts, circumscribed, normal, orient_sign, eps);
        polygon = DedupPolygon(
            BuildPolygonVertices(curve, ts, pts, is_contact, normal));

        if (iter >= kMaxRefine) break;

        // 含有違反を検出し、違反があれば違反点をサンプルへ追加して再試行する
        const auto extra = FindContainmentViolations(
            polygon, circumscribed, u, v, t_uniform, region2d,
            period, t1, kViolationEps);
        if (extra.empty()) break;

        ts.insert(ts.end(), extra.begin(), extra.end());
        ts = Dedup(ts, curve.IsClosed(), period, 1e-6);
    }

    return polygon;
}

}  // namespace



namespace igesio::entities {

i_num::PolygonData ComputeCircumscribedPolygon(
        const ICurve& curve,
        int n_vert,
        const Vector3d& reference_normal,
        double eps) {
    return ComputeExtremalPolygon(
        curve, n_vert, /*circumscribed=*/true, reference_normal, eps);
}

i_num::PolygonData ComputeInscribedPolygon(
        const ICurve& curve,
        int n_vert,
        const Vector3d& reference_normal,
        double eps) {
    return ComputeExtremalPolygon(
        curve, n_vert, /*circumscribed=*/false, reference_normal, eps);
}

}  // namespace igesio::entities
