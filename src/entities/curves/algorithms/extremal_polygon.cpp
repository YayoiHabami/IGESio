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
#include <cstdint>
#include <cstring>
#include <functional>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "igesio/numerics/core/matrix.h"
#include "igesio/numerics/geometric/polygon.h"
#include "igesio/numerics/analysis/optimization.h"
#include "igesio/entities/interfaces/i_curve.h"



namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using igesio::Vector3d;

// ===========================================================================
// 補助：符号
// ===========================================================================

/// @brief 有限値の符号（+∞ → +1.0, -∞ → -1.0, 0.0 → 0.0）
double FiniteSign(double v) {
    if (v > 0.0 || v == std::numeric_limits<double>::infinity()) return 1.0;
    if (v < 0.0 || v == -std::numeric_limits<double>::infinity()) return -1.0;
    return 0.0;
}

// ===========================================================================
// 平面基底・投影
// ===========================================================================

/// @brief 参照法線から平面内の正規直交基底(u, v)を構築する
/// @param normal 平面の法線ベクトル（正規化不要）
/// @param[out] u 平面内の基底ベクトル（正規化済）
/// @param[out] v 平面内の基底ベクトル（正規化済、u ⊥ v）
/// @throws std::invalid_argument normalがゼロベクトルの場合
/// @note n_hat × candidateをu、n_hat × uをvとして直交基底を生成する.
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
/// @param pt 射影する3次元点
/// @param u 平面内の基底ベクトル（正規化済）
/// @param v 平面内の基底ベクトル（正規化済）
/// @return (pt·u, pt·v)の2次元座標
std::array<double, 2> ProjectTo2D(
        const Vector3d& pt, const Vector3d& u, const Vector3d& v) {
    return {pt.dot(u), pt.dot(v)};
}

// ===========================================================================
// 2次元ジオメトリ補助
// ===========================================================================

/// @brief 2次元ベクトル(ax, ay)と(bx, by)の外積
double Cross2D(double ax, double ay, double bx, double by) {
    return ax * by - ay * bx;
}

/// @brief 相対許容を加味した2次元向き符号を返す
///
/// 行列式値を、それを構成する2ベクトルの長さ(の二乗)を基準に評価する。
/// |value| <= rel_tol * |u| * |v| のとき共線とみなし0を返す。これにより
/// おそらくFMA縮約等の浮動小数点評価差(ARM64とx86-64の差)で符号が揺れる微小行列式を
/// 共線側へ一貫して倒し、プラットフォーム間で結果を一致させる。sqrtを避けるため
/// 二乗値で比較する。
///
/// @param value Cross2Dで得た行列式値
/// @param u_len_sq 基準ベクトルの長さの二乗
/// @param w_len_sq もう一方のベクトルの長さの二乗
/// @param rel_tol 相対許容 (sinθに対する閾値)
/// @return +1 / -1 / 0 (共線)
int OrientationSign(double value, double u_len_sq, double w_len_sq,
                    double rel_tol) {
    const double eps_sq = rel_tol * rel_tol * u_len_sq * w_len_sq;
    if (value * value <= eps_sq) return 0;
    return (value > 0.0) ? 1 : -1;
}

/// @brief 2次元多角形の符号付き面積を返す（靴ひもの公式）
/// @param pts_2d 頂点の2次元座標の配列
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

/// @brief 2次元線分(a, b)と(c, d)が交差するかを判定する
/// @param a, b 線分1の端点（2次元）
/// @param c, d 線分2の端点（2次元）
/// @return 真に交差する場合true
/// @note 端点共有は交差とみなさない（縮退ケースはfalse）
bool SegmentsIntersect2D(
        const std::array<double, 2>& a, const std::array<double, 2>& b,
        const std::array<double, 2>& c, const std::array<double, 2>& d) {
    // 行列式を構成するベクトル長の積に対する相対許容。おそらくFMA縮約の有無
    // （ARM64とx86-64の差）で符号が反転し得る微小（≒共線）行列式を共線と
    // みなす閾値。交差角がこの値より大きい交差は影響を受けない。
    constexpr double kCrossRelTol = 1e-9;

    const double cdx = d[0] - c[0], cdy = d[1] - c[1];
    const double abx = b[0] - a[0], aby = b[1] - a[1];
    const double cd_sq = cdx * cdx + cdy * cdy;
    const double ab_sq = abx * abx + aby * aby;

    // a, bが線分cdの支持直線をまたぐか
    const double acx = a[0] - c[0], acy = a[1] - c[1];
    const double bcx = b[0] - c[0], bcy = b[1] - c[1];
    const int s1 = OrientationSign(Cross2D(cdx, cdy, acx, acy),
                                   cd_sq, acx * acx + acy * acy, kCrossRelTol);
    const int s2 = OrientationSign(Cross2D(cdx, cdy, bcx, bcy),
                                   cd_sq, bcx * bcx + bcy * bcy, kCrossRelTol);
    // c, dが線分abの支持直線をまたぐか
    const double cax = c[0] - a[0], cay = c[1] - a[1];
    const double dax = d[0] - a[0], day = d[1] - a[1];
    const int s3 = OrientationSign(Cross2D(abx, aby, cax, cay),
                                   ab_sq, cax * cax + cay * cay, kCrossRelTol);
    const int s4 = OrientationSign(Cross2D(abx, aby, dax, day),
                                   ab_sq, dax * dax + day * day, kCrossRelTol);

    // 端点共有・共線（縮退）は交差とみなさない。
    // 両線分が互いの支持直線を厳密にまたぐ場合のみ交差とする。
    return (s1 != 0 && s2 != 0 && s1 != s2)
        && (s3 != 0 && s4 != 0 && s3 != s4);
}

/// @brief 点が2次元単純多角形の内部または境界（eps以内）にあるか
/// @param p 判定する点（2次元）
/// @param poly 多角形の頂点列（2次元）
/// @param eps 境界とみなす距離の閾値
/// @return 内部または境界上にある場合true
/// @note 境界は辺までの距離<= epsで判定し、内部は even-odd ray casting（向き非依存）
///       で判定する. 含有不変条件の検証（FindContainmentViolations）に用いる.
bool PointInOrOnPolygon2D(const std::array<double, 2>& p,
                          const std::vector<std::array<double, 2>>& poly,
                          double eps) {
    const int n = static_cast<int>(poly.size());
    if (n < 3) return false;

    // 境界判定: 各辺までの距離がeps以下なら境界上とみなす
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

    // 内部判定：even-odd ray casting
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
// 直線交差（3D）
// ===========================================================================

/// @brief 3次元空間の2直線の交点を求める
/// @param p1 直線1上の基準点
/// @param d1 直線1の方向ベクトル
/// @param p2 直線2上の基準点
/// @param d2 直線2の方向ベクトル
/// @return 交点の座標
/// @note 直線1（p1 + s*d1）と直線2（p2 + t*d2）の交点を返す. 共面かつ
///       非平行であるものとし、平行（|d1 × d2|² < 1e-24）の場合は中点を返す.
Vector3d LineIntersect(const Vector3d& p1, const Vector3d& d1,
                       const Vector3d& p2, const Vector3d& d2) {
    const Vector3d cross = d1.cross(d2);
    const double cross_sq = cross.squaredNorm();

    // 平行または一致なら中点を返す
    if (cross_sq < 1e-24) return 0.5 * (p1 + p2);

    // s = ((p2 - p1) × d2) · (d1 × d2) / |d1 × d2|²
    const double s = (p2 - p1).cross(d2).dot(cross) / cross_sq;
    return p1 + s * d1;
}

// ===========================================================================
// 曲線のキャッシュ（角点/直線区間の取得と評価結果をキャッシュし、計算回数を削減する）
// ===========================================================================

/// @brief 多角形構築用の、曲線の計算結果のキャッシュ
/// @note `ICurve::IsCorner`/`IsInLinearSegment`は呼ぶたびに角点/直線区間を
///       再計算するため、本クラスの構築時に一度だけ取得してキャッシュする.
///       また、符号付き曲率と曲線上の点は同じパラメータで繰り返し評価されるため,
///       一度計算した結果もキャッシュする. 各メンバ名は`ICurve`と同じとする.
class CurveCache {
 public:
    /// @brief 対象曲線と参照法線からキャッシュを構築する
    /// @param curve 対象曲線
    /// @param reference_normal 符号付き曲率の基準法線 (正規化は不要）
    CurveCache(const i_ent::ICurve& curve, const Vector3d& reference_normal)
        : curve_(curve),
          corners_(curve.GetCornerParams()),
          linear_segments_(curve.GetLinearSegments()) {
        std::sort(corners_.begin(), corners_.end());
        const auto range = curve.GetParameterRange();
        t0_ = range[0];
        t1_ = range[1];
        const double n_norm = reference_normal.norm();
        has_normal_ = n_norm >= 1e-12;
        if (has_normal_) n_hat_ = reference_normal / n_norm;
    }

    /// @brief 対象曲線を取得する
    const i_ent::ICurve& Curve() const { return curve_; }
    /// @brief パラメータ範囲の下限を取得する
    double T0() const { return t0_; }
    /// @brief パラメータ範囲の上限を取得する
    double T1() const { return t1_; }
    /// @brief 角点のパラメータ列（昇順）を取得する
    const std::vector<double>& Corners() const { return corners_; }
    /// @brief 直線区間のリストを取得する
    const std::vector<std::array<double, 2>>& LinearSegments() const {
        return linear_segments_;
    }

    /// @brief パラメータtが角点か（`ICurve::IsCorner`に同）
    bool IsCorner(const double t, const double eps = 1e-9) const {
        auto it = std::lower_bound(corners_.begin(), corners_.end(), t - eps);
        for (; it != corners_.end() && *it < t + eps; ++it) {
            if (std::abs(*it - t) < eps) return true;
        }
        return false;
    }

    /// @brief パラメータtが直線区間内か（`ICurve::IsInLinearSegment`に同）
    bool IsInLinearSegment(const double t, const double eps = 1e-9) const {
        for (const auto& seg : linear_segments_) {
            if (t >= seg[0] - eps && t <= seg[1] + eps) return true;
        }
        return false;
    }

    /// @brief 符号付き曲率を計算する（`ICurve::TryGetSignedCurvature`に同）
    /// @note 同じtに対する計算結果はキャッシュし、2回目以降は再計算しない
    std::optional<double> SignedCurvature(const double t) const {
        const uint64_t key = Key(t);
        const auto found = curvature_cache_.find(key);
        if (found != curvature_cache_.end()) return found->second;
        const auto value = ComputeSignedCurvature(t);
        curvature_cache_.emplace(key, value);
        return value;
    }

    /// @brief 曲線上の点を取得する（`ICurve::GetPointAt`に同）
    /// @throw std::out_of_range tが範囲外の場合
    /// @note 同じtに対する計算結果はキャッシュし、2回目以降は再計算しない
    const Vector3d& PointAt(const double t) const {
        const uint64_t key = Key(t);
        const auto found = point_cache_.find(key);
        if (found != point_cache_.end()) return found->second;
        return point_cache_.emplace(key, curve_.GetPointAt(t)).first->second;
    }

    /// @brief 前進方向単位接線ベクトルを取得する
    /// @note 角点では右側接線T⁺(t)、通常点では接線を用いる.
    ///       同じtに対する結果はキャッシュし、2回目以降は再計算しない
    std::optional<Vector3d> ForwardTangentAt(const double t) const {
        const uint64_t key = Key(t);
        const auto found = forward_cache_.find(key);
        if (found != forward_cache_.end()) return found->second;
        const auto value = IsCorner(t) ? curve_.TryGetRightTangentAt(t)
                                       : curve_.TryGetTangentAt(t);
        forward_cache_.emplace(key, value);
        return value;
    }

    /// @brief 到達方向単位接線ベクトルを取得する
    /// @note 角点では左側接線T⁻(t)、通常点では接線を用いる. 平滑点では
    ///       前進方向と一致するが、角点では弧側（到達側）の接線となる.
    ///       同じtに対する結果はキャッシュし、2回目以降は再計算しない
    std::optional<Vector3d> BackwardTangentAt(const double t) const {
        const uint64_t key = Key(t);
        const auto found = backward_cache_.find(key);
        if (found != backward_cache_.end()) return found->second;
        const auto value = IsCorner(t) ? curve_.TryGetLeftTangentAt(t)
                                       : curve_.TryGetTangentAt(t);
        backward_cache_.emplace(key, value);
        return value;
    }

 private:
    /// @brief doubleのビット表現からキャッシュ用辞書のキーを計算する
    static uint64_t Key(const double t) {
        uint64_t key = 0;
        std::memcpy(&key, &t, sizeof(key));
        return key;
    }

    /// @brief 符号付き曲率を計算する（`ICurve::TryGetSignedCurvature`に同）
    std::optional<double> ComputeSignedCurvature(const double t) const {
        if (!has_normal_) return std::nullopt;

        if (IsCorner(t)) {
            // 角点では、外角αに基づいて符号付き曲率を定義する
            const auto alpha = curve_.CornerExteriorAngle(t, n_hat_);
            if (!alpha.has_value()) return std::nullopt;
            if (*alpha > 0) return  std::numeric_limits<double>::infinity();
            if (*alpha < 0) return -std::numeric_limits<double>::infinity();
            return 0.0;
        }

        // 直線部では曲率は0
        if (IsInLinearSegment(t)) return 0.0;

        const auto deriv = curve_.TryGetDefinedDerivatives(t, 2);
        if (!deriv.has_value()) return std::nullopt;
        const auto& c1 = (*deriv)[1];
        const auto& c2 = (*deriv)[2];
        const double speed3 = std::pow(c1.norm(), 3);
        if (speed3 < 1e-14) return std::nullopt;
        return c1.cross(c2).dot(n_hat_) / speed3;
    }

    /// @brief 対象曲線
    const i_ent::ICurve& curve_;
    /// @brief 角点のパラメータ列（昇順）
    std::vector<double> corners_;
    /// @brief 直線区間のリスト
    std::vector<std::array<double, 2>> linear_segments_;
    /// @brief パラメータ範囲の下限
    double t0_ = 0.0;
    /// @brief パラメータ範囲の上限
    double t1_ = 0.0;
    /// @brief 参照法線が有効（非ゼロ）か
    bool has_normal_ = false;
    /// @brief 正規化した参照法線
    Vector3d n_hat_ = Vector3d::Zero();
    /// @brief 符号付き曲率のキャッシュ（キーはtのビット表現）
    mutable std::unordered_map<uint64_t, std::optional<double>> curvature_cache_;
    /// @brief 曲線上の点のキャッシュ（キーはtのビット表現）
    mutable std::unordered_map<uint64_t, Vector3d> point_cache_;
    /// @brief 前進方向接線のキャッシュ（キーはtのビット表現）
    mutable std::unordered_map<uint64_t, std::optional<Vector3d>> forward_cache_;
    /// @brief 到達方向接線のキャッシュ（キーはtのビット表現）
    mutable std::unordered_map<uint64_t, std::optional<Vector3d>> backward_cache_;
};

// ===========================================================================
// 符号付き曲率のゼロ点探索
// ===========================================================================

/// @brief 区間[ta, tb]で符号付き曲率κ_s(t) ≈ 0となるtを探す
/// @param cache 対象曲線のキャッシュと符号付き曲率の基準法線
/// @param ta 区間始端
/// @param tb 区間終端（ta <= tb; 折り返し時はtb + periodとして渡す）
/// @param eps 許容誤差
/// @return κ_s ≈ 0となるt. 見つからない場合は区間中点
/// @note 閉曲線の周期的折り返し（ta > tbのケース）に対応する.
///       Brentq（TOMS 748）を使用して精密化する.
double FindZeroCurvature(const CurveCache& cache,
                         double ta, double tb,
                         double eps = 1e-9) {
    const double t_min = cache.T0(), t_max = cache.T1();
    const double period = t_max - t_min;

    // 折り返しケース（ta > tb）：tbを一周分延ばして単調区間に変換する
    if (ta > tb) tb = t_max + (tb - t_min);

    // κ_sを計算する（t > t_maxの場合に正規化する）
    auto kappa = [&](double t) -> double {
        double t_eval = t;
        if (t_eval > t_max) t_eval -= period;
        const auto k = cache.SignedCurvature(t_eval);
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
        // バックする（∞をboostのルート探索に渡すとNaNになるのを防ぐ）。
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

/// @brief パラメータtが直線部の内部（端点を除く）に含まれるか
bool InLinear(const std::vector<std::array<double, 2>>& segs,
              double t, double eps = 1e-9) {
    for (const auto& seg : segs) {
        if (seg[0] + eps < t && t < seg[1] - eps) return true;
    }
    return false;
}

/// @brief 区間[ta, tb]が直線部の始端・終端と一致するか
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

/// @brief 十分に近い点を重複とみなして除去する
/// @param pts 重複除去前のパラメータ値列（昇順であること）
/// @param is_closed 閉曲線か否か
/// @param period 曲線の周期（GetParameterRangeの幅）
/// @param merge_tol 重複とみなす閾値（periodに対する比率）
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

/// @brief 点数が少ない場合にn_vert付近になるよう各間隔を等間隔に補間する
/// @param ts 補間前のパラメータ値列
/// @param t0 パラメータ範囲の下限
/// @param t1 パラメータ範囲の上限
/// @param n_vert 目標頂点数
/// @param skip_fn 区間(ta, tb)をスキップするか判定する関数.
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

        // 直線区間はスキップ（始端のみ追加）
        if (skip_fn && skip_fn(start_t, end_t_for_check)) {
            result.push_back(start_t);
            continue;
        }

        // 各区間をn_int個に分割して追加
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
std::vector<double> InsertCorners(const CurveCache& cache,
                                  std::vector<double> ts) {
    const double t0 = cache.T0(), t1 = cache.T1();
    for (double tc : cache.Corners()) {
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
/// @note 端点を[t0, t1)にクランプする. 継ぎ目t==t1は閉曲線でC(t1)=C(t0)と重複し,
///       ゼロ長辺の原因となるため除外する（InsertCorners と整合）
std::vector<double> InsertLinearEndpoints(
        const std::vector<std::array<double, 2>>& segs,
        std::vector<double> ts, double t0, double t1) {
    const double seam_tol = 1e-9 * (t1 - t0);
    for (const auto& seg : segs) {
        for (const double endpoint : {seg[0], seg[1]}) {
            // 範囲外・継ぎ目（t1≡t0）は除外する
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

/// @brief 実効閉性の判定結果
struct ClosureCheckResult {
    /// @brief 実効的に閉曲線とみなせるか
    bool closed = false;
    /// @brief 始終点間のギャップ（端点が取得できない場合はNaN）
    double gap = std::numeric_limits<double>::quiet_NaN();
    /// @brief 適用した絶対許容値（closure_rel_tol×サンプル点のAABB対角長）
    double tol_abs = 0.0;
};

/// @brief 曲線が実効的に閉じているかを判定する
/// @param curve 対象曲線
/// @param sample_pts 均等サンプリングの3次元点列
/// @param closure_rel_tol 閉性判定の相対許容（0なら厳密判定のみ）
/// @return 判定結果（実効閉性・ギャップ・適用許容値）
/// @note `IsClosed()`による厳密判定に加え、始終点ギャップが
///       closure_rel_tol×曲線の広がり（サンプル点のAABB対角長）以下の場合も
///       閉曲線とみなす。実CADが出力する境界ループの継ぎ目には微小ギャップが
///       存在しうるため、トリム領域構築等ではこの相対判定を併用する。
///       多角形構築は最終サンプル点から先頭点への辺を暗黙に張るため,
///       許容内のギャップは追加処理なしで自動的に閉じられる。
ClosureCheckResult CheckEffectiveClosure(
        const i_ent::ICurve& curve,
        const std::vector<Vector3d>& sample_pts,
        const double closure_rel_tol) {
    ClosureCheckResult result;
    result.closed = curve.IsClosed();

    // 始終点ギャップ（相対判定のほか、閉じていない際の診断メッセージに用いる）
    const auto start = curve.TryGetStartPoint();
    const auto end = curve.TryGetEndPoint();
    if (start && end) result.gap = (*end - *start).norm();

    if (result.closed || closure_rel_tol <= 0.0) return result;
    // 端点が取得できない場合は相対判定をスキップ（厳密判定の結果に従う）
    if (!start || !end) return result;

    // 曲線の広がり：サンプル点のAABB対角長（退化時は相対判定をスキップ）
    Vector3d lo = Vector3d::Constant(std::numeric_limits<double>::infinity());
    Vector3d hi = -lo;
    for (const auto& p : sample_pts) {
        lo = lo.cwiseMin(p);
        hi = hi.cwiseMax(p);
    }
    if (!lo.allFinite() || !hi.allFinite()) return result;
    const double extent = (hi - lo).norm();
    if (!(extent > 0.0)) return result;

    result.tol_abs = closure_rel_tol * extent;
    result.closed = result.gap <= result.tol_abs;
    return result;
}

/// @brief 均等サンプリング点列を用いて曲線の向きと自己交差の有無を検証する
/// @param closure 実効閉性の判定結果
/// @param sample_pts 均等サンプリングの3次元点列（n_init個）
/// @param normal 平面の参照法線ベクトル
/// @return orientation_sign: +1.0（reference_normal から見てCCW）/ -1.0（CW）
/// @throws std::invalid_argument 閉曲線でない場合、自己交差が存在する場合
/// @note 以下の検証を行う:
///       (1) 実効閉性（CheckEffectiveClosureの結果）のチェック,
///       (2) 符号付き面積（shoelace）に基づく向き符号の計算,
///       (3) 近似多角形の自己交差判定（O(N²)）
double CheckCurveProperties(const ClosureCheckResult& closure,
                            const std::vector<Vector3d>& sample_pts,
                            const Vector3d& normal) {
    // 閉曲線チェック（微小な継ぎ目ギャップは許容値以内なら閉とみなす）
    if (!closure.closed) {
        std::ostringstream oss;
        oss << std::scientific << std::setprecision(3)
            << "曲線が閉じていません (始終点ギャップ=" << closure.gap
            << ", 許容=" << closure.tol_abs
            << ")。閉曲線にのみ対応しています。";
        throw std::invalid_argument(oss.str());
    }

    // 平面基底を構築して全点を2次元に射影する
    auto [u, v] = BuildPlaneBasis(normal);

    const int n = static_cast<int>(sample_pts.size());
    std::vector<std::array<double, 2>> pts_2d(n);
    for (int i = 0; i < n; ++i) {
        pts_2d[i] = ProjectTo2D(sample_pts[i], u, v);
    }

    // 向き符号：符号付き面積の符号
    const double area = SignedArea2D(pts_2d);
    const double orient_sign = (area >= 0.0) ? 1.0 : -1.0;

    // 自己交差チェック：非隣接辺ペアのうち、辺のAABBが重なるものだけを確認する
    // （真に交差する2辺のバウンディングボックスは必ず重なるため）.
    // 辺をx下限で整列し、x範囲が重なる間だけ走査する
    std::vector<double> x_lo(n), x_hi(n), y_lo(n), y_hi(n);
    std::vector<int> order(n);
    for (int i = 0; i < n; ++i) {
        const auto& a = pts_2d[i];
        const auto& b = pts_2d[(i + 1) % n];
        x_lo[i] = std::min(a[0], b[0]);
        x_hi[i] = std::max(a[0], b[0]);
        y_lo[i] = std::min(a[1], b[1]);
        y_hi[i] = std::max(a[1], b[1]);
        order[i] = i;
    }
    std::sort(order.begin(), order.end(),
              [&x_lo](const int a, const int b) { return x_lo[a] < x_lo[b]; });
    for (int oi = 0; oi < n; ++oi) {
        const int i = order[oi];
        for (int oj = oi + 1; oj < n; ++oj) {
            const int j = order[oj];
            if (x_lo[j] > x_hi[i]) break;
            if (y_lo[j] > y_hi[i] || y_hi[j] < y_lo[i]) continue;
            // 隣接辺（共有端点を持つ辺）は除く
            if ((i + 1) % n == j || (j + 1) % n == i) continue;
            if (SegmentsIntersect2D(pts_2d[i], pts_2d[(i + 1) % n],
                                    pts_2d[j], pts_2d[(j + 1) % n])) {
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

/// @brief 符号付き曲率κ_s(t)の極大・極小点を探索する
/// @param cache 対象曲線のキャッシュおよび符号付き曲率の基準法線
/// @param t_uniform 均等分割されたパラメータ値列（初期サンプリング点）
/// @param tol 精密化の許容誤差
/// @param merge_tol 重複除去の閾値（periodに対する比率）
/// @return (maxima, minima)：極大点と極小点のパラメータ値列
std::pair<std::vector<double>, std::vector<double>> FindCurvatureExtrema(
        const CurveCache& cache,
        const std::vector<double>& t_uniform,
        double tol = 1e-9,
        double merge_tol = 1e-4) {
    const double t0 = cache.T0(), t1 = cache.T1();
    const double period = t1 - t0;
    const auto& linear_segs = cache.LinearSegments();

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
        const auto k = cache.SignedCurvature(ts[i]);
        kappa[i] = k.value_or(0.0);
    }

    // dt：均等サンプリングの間隔を近似する
    const double dt = period / n;

    std::vector<double> maxima, minima;

    auto kappa_fn = [&](double t) -> double {
        const auto k = cache.SignedCurvature(t);
        return k.value_or(0.0);
    };

    for (int i = 0; i < n; ++i) {
        const int prev = (i - 1 + n) % n;
        const int nxt  = (i + 1) % n;
        const double k_p = kappa[prev];
        const double k_i = kappa[i];
        const double k_n = kappa[nxt];

        // 角点では曲率が±∞となる（TryGetSignedCurvature）。∞は平滑な曲率の
        // 極値ではなく、角点はInsertCornersで別途追加されるため、ここでの
        // 極値探索の対象から除外する。∞をboostのMinimizeScalarに渡すと
        // 放物線補間でNaNのt_optが返り、後段のGetPointAtで例外となるのを防ぐ。
        if (!std::isfinite(k_i)) continue;

        const double lb = ts[i] - dt;
        const double ub = ts[i] + dt;

        // 局所極大：-κを最小化
        if (k_i > k_p && k_i > k_n) {
            try {
                const auto res = i_num::MinimizeScalar(
                    [&](double t) { return -kappa_fn(t); },
                    lb, ub, tol);
                // ブラケット[lb,ub]が角点（∞）を含む場合、boostが非有限な
                // t_optを返しうる。その場合はサンプル点にフォールバックする。
                maxima.push_back(std::isfinite(res.t_opt) ? res.t_opt : ts[i]);
            } catch (const std::exception&) {
                maxima.push_back(ts[i]);
            }
        }

        // 局所極小：κを最小化
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

    // tを[t0, t1)に正規化する
    auto normalize = [&](double t) -> double {
        while (t < t0) t += period;
        while (t >= t1) t -= period;
        return t;
    };
    for (auto& t : maxima) t = normalize(t);
    for (auto& t : minima) t = normalize(t);

    const bool is_closed = cache.Curve().IsClosed();
    return {Dedup(maxima, is_closed, period, merge_tol),
            Dedup(minima, is_closed, period, merge_tol)};
}

// ===========================================================================
// サンプリング
// ===========================================================================

/// @brief 多角形構築のためのサンプル点を生成する
/// @param cache 対象曲線のキャッシュおよび符号付き曲率の基準法線
/// @param t_uniform 均等分割されたパラメータ値列（CheckCurvePropertiesと共用）
/// @param n_vert 目標頂点数
/// @return (ts, pts)：サンプル点のパラメータ値列と3次元座標列
/// @note 曲率極値点を起点に、InterpolateClosedPoints → InsertCorners →
///       InsertLinearEndpointsの順に適用する.
///       極値が見つからない場合は均等サンプリングにフォールバックする.
std::pair<std::vector<double>, std::vector<Vector3d>>
SamplePoints(const CurveCache& cache,
             const std::vector<double>& t_uniform,
             int n_vert) {
    const double t0 = cache.T0(), t1 = cache.T1();
    const double period = t1 - t0;
    const auto& linear_segs = cache.LinearSegments();

    auto [maxima, minima] = FindCurvatureExtrema(cache, t_uniform);

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
        // 極値がない場合は均等サンプリング（直線部内部は除外）
        ts.reserve(n_vert);
        for (int i = 0; i < n_vert; ++i) {
            const double t = t0 + period * i / n_vert;
            if (!InLinear(linear_segs, t)) ts.push_back(t);
        }
    }

    ts = InsertCorners(cache, ts);
    ts = InsertLinearEndpoints(linear_segs, ts, t0, t1);

    // 3次元座標を計算する。tは曲率極値・角点由来で閉曲線の周期正規化により
    // [t0, t1]を僅かに外れうるため、GetPointAtの範囲外例外を避けてクランプする。
    std::vector<Vector3d> pts;
    pts.reserve(ts.size());
    for (double t : ts) pts.push_back(cache.PointAt(std::clamp(t, t0, t1)));
    return {ts, pts};
}

// ===========================================================================
// 点分類
// ===========================================================================

/// @brief 各サンプル点が頂点か接点かを分類する
/// @param cache 対象曲線キャッシュおよび符号付き曲率の基準法線
/// @param ts サンプル点のパラメータ値列
/// @param circumscribed 外包なら true, 内包なら false
/// @param orient_sign 向き符号（+1: CCW, -1: CW）
/// @param eps 曲率判定の閾値
/// @return 各点の分類結果（true: 接点, false: 頂点）
/// @note 外包（circumscribed=true）の場合、内側に凸なら頂点、外側に凸なら節点.
///       内包の場合、外側に凸なら頂点、内側に凸なら節点。凸性は以下のように判定する.
///       (i) is_outward = (κ_s * orient_sign) > eps,
///       (i) is_inward  = (κ_s * orient_sign) < -eps
std::vector<bool> ClassifyPoints(
        const CurveCache& cache, const std::vector<double>& ts,
        bool circumscribed, double orient_sign, double eps = 1e-9) {
    std::vector<bool> result;
    result.reserve(ts.size());

    for (double t : ts) {
        const auto k_opt = cache.SignedCurvature(t);
        if (!k_opt.has_value()) {
            // 計算不能の場合は保守的に頂点とする
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
/// @param cache 対象曲線キャッシュおよび符号付き曲率の基準法線
/// @param ts サンプル点のパラメータ値列
/// @param pts サンプル点の3次元座標列
/// @param is_contact 各点の分類（true: 接点, false: 頂点）
/// @return 構築された多角形データ
/// @note 各隣接ペア(i, j)の組について、以下の通りに頂点を追加する：
///       (1) (vertex,vertex)：Paを追加する,
///       (2) (contact,contact)：接線交点を頂点とする,
///       (3) (vertex,contact)：ゼロ曲率点Pmを挿入しPm-Pb接線の交点を頂点とする,
///       (4) (contact,vertex)：ゼロ曲率点Pmを挿入しPa-Pm接線の交点を頂点とする
i_num::PolygonData BuildPolygonVertices(
        const CurveCache& cache, const std::vector<double>& ts,
        const std::vector<Vector3d>& pts, const std::vector<bool>& is_contact) {
    const int n = static_cast<int>(ts.size());
    const auto& linear_segs = cache.LinearSegments();

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

        // 直線区間のペアではPaを追加する
        if (IsLinearInterval(linear_segs, ta, tb)) {
            add(Pa, /*on_curve=*/true, ta);
            continue;
        }

        // Paは前進方向（出発側）、Pbは後退方向（到達側）の接線を取得する。
        // 角点では出発側=T⁺ / 到達側=T⁻となり、Pbが接合コーナーの場合に
        // 弧側の接線で囲えるようにする（平滑点では両者は一致）。
        const auto da_opt = cache.ForwardTangentAt(ta);
        const auto db_opt = cache.BackwardTangentAt(tb);

        if (!da_opt.has_value() || !db_opt.has_value()) {
            // 接線取得に失敗した場合は、保守的にPaのみ追加
            add(Pa, /*on_curve=*/true, ta);
            continue;
        }
        const Vector3d da = *da_opt;
        const Vector3d db = *db_opt;

        if (!ca && !cb) {
            // 両方頂点の場合は線分 (Paを追加し、次ループでPbが追加される)
            add(Pa, /*on_curve=*/true, ta);

        } else if (ca && cb) {
            // 両方接点の場合、Paからの接線とPbからの逆接線の交点を頂点とする
            const Vector3d vertex = LineIntersect(Pa, da, Pb, -db);
            add(Pa,     /*on_curve=*/true,  ta);
            add(vertex, /*on_curve=*/false);

        } else if (!ca && cb) {
            // Paが頂点, Pbが接点の場合
            // ゼロ曲率点Pmを挿入し、Pm接線とPb逆接線の交点を頂点とする
            const double t_mid = FindZeroCurvature(cache, ta, tb);
            const Vector3d Pm = cache.PointAt(t_mid);
            const auto dm_opt = cache.ForwardTangentAt(t_mid);
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
            // Paが接点、Pbが頂点の場合
            // ゼロ曲率点Pmを挿入し、Pm逆接線とPa接線の交点を頂点とする
            const double t_mid = FindZeroCurvature(cache, ta, tb);
            const Vector3d Pm = cache.PointAt(t_mid);
            const auto dm_opt = cache.ForwardTangentAt(t_mid);
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
// 後処理：重複除去・包含の検証
// ===========================================================================

/// @brief 多角形の隣接・継ぎ目で一致する頂点を統合して長さゼロの辺を除去する
/// @param poly 入力多角形
/// @return 長さゼロの辺を除去した多角形
/// @note 接合点や角点でcc/vc/cvが接点と一致する頂点を出す、また直線端点が継ぎ目で
///       重複する等により生じる長さゼロの辺（隣接する同一頂点）を除去する.
///       一致する頂点同士はon_curve=true側を優先して残す.
i_num::PolygonData DedupPolygon(const i_num::PolygonData& poly) {
    constexpr double kDedupTol = 1e-9;
    i_num::PolygonData out;
    const int n = poly.Count();
    if (n == 0) return out;

    for (int i = 0; i < n; ++i) {
        if (!out.vertices.empty() &&
            (out.vertices.back() - poly.vertices[i]).norm() <= kDedupTol) {
            // 直前と一致した場合はon_curve=true側を優先して残す
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

    // 継ぎ目（先頭と末尾）の一致を除去する
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
/// @param poly 検証対象の多角形
/// @param circumscribed 外包ならtrue、内包ならfalse
/// @param u, v 平面基底
/// @param t_uniform 曲線領域の点列のパラメータ（均等サンプル＋角点）
/// @param region2d 曲線領域（領域点列の2次元射影; 外包では曲線点列としても使用）
/// @param period パラメータ範囲の幅
/// @param t1 パラメータ範囲の上限
/// @param eps_geom 境界とみなす距離の閾値
/// @param[out] inside_cache 内包の辺中点に対する領域内外判定のキャッシュ
///        （キーは中点座標のビット表現）. 細分の反復間で多くの辺が変化しないため、
///        同じ中点の判定を繰り返さないよう呼び出し側で保持する
/// @return 追加すべき曲線パラメータの列（空なら違反なし）
/// @note 外包の場合、曲線点（均等サンプル）が多角形の外側にあれば、その曲線パラメータを
///       追加する. 追加点は次の再構築で接点（接線）として分類され、凸部を接線で
///       囲うようになる.
/// @note 内包の場合、多角形の辺中点が曲線領域の外側にあれば、その辺の曲線区間中点を追加する.
std::vector<double> FindContainmentViolations(
        const i_num::PolygonData& poly, bool circumscribed,
        const Vector3d& u, const Vector3d& v,
        const std::vector<double>& t_uniform,
        const std::vector<std::array<double, 2>>& region2d,
        double period, double t1, double eps_geom,
        std::map<std::pair<uint64_t, uint64_t>, bool>& inside_cache) {
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
            const auto mid2d = ProjectTo2D(mid, u, v);
            std::pair<uint64_t, uint64_t> key;
            std::memcpy(&key.first, &mid2d[0], sizeof(uint64_t));
            std::memcpy(&key.second, &mid2d[1], sizeof(uint64_t));
            auto found = inside_cache.find(key);
            if (found == inside_cache.end()) {
                found = inside_cache.emplace(
                    key, PointInOrOnPolygon2D(mid2d, region2d, eps_geom)).first;
            }
            if (found->second) continue;
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
// メイン処理
// ===========================================================================

/// @brief 外包/内包多角形で共有できる前処理の結果
/// @note circumscribedに依存しない値をまとめ、外包・内包で再計算を避ける.
struct ExtremalPolygonSharedData {
    /// @brief パラメータ範囲の下限
    double t0 = 0.0;
    /// @brief パラメータ範囲の上限
    double t1 = 0.0;
    /// @brief パラメータ範囲の幅 (t1 - t0)
    double period = 0.0;
    /// @brief 均等分割されたパラメータ値列
    std::vector<double> t_uniform;
    /// @brief 曲線領域を表す点列のパラメータ値（均等サンプル＋角点、昇順）
    std::vector<double> t_region;
    /// @brief 実効的に閉曲線とみなすか（許容内の継ぎ目ギャップを含む）
    bool is_closed = false;
    /// @brief 向き符号（符号付き面積の符号）
    double orient_sign = 1.0;
    /// @brief 含有検証用の平面基底（u軸）
    Vector3d plane_u = Vector3d::Zero();
    /// @brief 含有検証用の平面基底（v軸）
    Vector3d plane_v = Vector3d::Zero();
    /// @brief 曲線領域（均等点の2次元射影）
    std::vector<std::array<double, 2>> region2d;
    /// @brief 多角形構築のための初期サンプル点（パラメータ値列）
    std::vector<double> ts_initial;
};

/// @brief 外包/内包多角形で共有する前処理（circumscribed非依存）
/// @param cache 対象曲線のキャッシュおよび符号付き曲率の基準法線
/// @param n_vert 初期分割数
/// @param normal 平面の参照法線ベクトル
/// @param closure_rel_tol 閉性判定の相対許容（0なら厳密判定のみ）
/// @return 前処理の結果
/// @throws std::invalid_argument n_vertが3未満の場合
ExtremalPolygonSharedData PrepareExtremalPolygonData(
        const CurveCache& cache,
        int n_vert, const Vector3d& normal, double closure_rel_tol) {
    if (n_vert < 3) {
        throw std::invalid_argument(
            "n_vertは3以上でなければなりません。n_vert: "
            + std::to_string(n_vert));
    }

    // 均等サンプリング（CheckCurvePropertiesとFindCurvatureExtremaで共用）
    constexpr int kNInit = 500;
    ExtremalPolygonSharedData s;
    const double t0 = cache.T0(), t1 = cache.T1();
    s.t0 = t0;
    s.t1 = t1;
    s.period = t1 - t0;

    s.t_uniform.resize(kNInit);
    for (int i = 0; i < kNInit; ++i) {
        s.t_uniform[i] = t0 + s.period * i / kNInit;
    }

    // 曲線領域の点列：均等サンプルに角点を加える. 折れ線のように角点の多い曲線では
    // 均等サンプルだけでは形状を捉えられず、含有検証が実際の曲線と矛盾して細分が
    // 収束しない（辺の中点を追加しても常に領域外と判定される）ため
    s.t_region = s.t_uniform;
    for (const double tc : cache.Corners()) {
        if (tc >= t0 && tc < t1) s.t_region.push_back(tc);
    }
    std::sort(s.t_region.begin(), s.t_region.end());
    s.t_region.erase(std::unique(s.t_region.begin(), s.t_region.end()),
                     s.t_region.end());
    std::vector<Vector3d> pts_region;
    pts_region.reserve(s.t_region.size());
    for (const double t : s.t_region) pts_region.push_back(cache.PointAt(t));

    // 実効閉性を判定する（許容内の継ぎ目ギャップは閉とみなす）
    const auto closure =
        CheckEffectiveClosure(cache.Curve(), pts_region, closure_rel_tol);
    s.is_closed = closure.closed;

    // 閉曲線チェック・向き符号計算・自己交差チェック
    s.orient_sign = CheckCurveProperties(closure, pts_region, normal);

    // 含有検証用の平面基底と曲線領域（領域点列の射影）を用意する
    const auto [u, v] = BuildPlaneBasis(normal);
    s.plane_u = u;
    s.plane_v = v;
    s.region2d.resize(pts_region.size());
    for (size_t i = 0; i < pts_region.size(); ++i) {
        s.region2d[i] = ProjectTo2D(pts_region[i], u, v);
    }

    // 多角形構築のためのサンプル点を生成する
    s.ts_initial = SamplePoints(cache, s.t_uniform, n_vert).first;

    return s;
}

/// @brief 細分しながら外包/内包多角形を構築する
/// @param cache 対象曲線のキャッシュおよび符号付き曲率の基準法線
/// @param circumscribed 外包ならtrue, 内包ならfalse
/// @param eps 曲率判定の閾値
/// @param s 前処理の結果（PrepareExtremalPolygonDataの結果）
/// @return 多角形データ
i_num::PolygonData RefineExtremalPolygon(
        const CurveCache& cache,
        bool circumscribed,
        double eps,
        const ExtremalPolygonSharedData& s) {
    // 含有不変条件を満たすまで、違反点をサンプルに追加して再構築する。
    constexpr int kMaxRefine = 6;
    constexpr double kViolationEps = 1e-7;
    // 細分は外包・内包で独立に行うため、初期サンプルをコピーして用いる
    std::vector<double> ts = s.ts_initial;
    i_num::PolygonData polygon;
    std::map<std::pair<uint64_t, uint64_t>, bool> inside_cache;
    for (int iter = 0; ; ++iter) {
        std::vector<Vector3d> pts;
        pts.reserve(ts.size());
        // tは曲率極値・違反点由来で[t0, t1]を僅かに外れ得るためクランプする
        for (double t : ts) {
            pts.push_back(cache.PointAt(std::clamp(t, s.t0, s.t1)));
        }

        // 各点を頂点/接点に分類し、多角形を構築・重複除去する
        const auto is_contact = ClassifyPoints(
            cache, ts, circumscribed, s.orient_sign, eps);
        polygon = DedupPolygon(
            BuildPolygonVertices(cache, ts, pts, is_contact));

        if (iter >= kMaxRefine) break;

        // 含有違反を検出し、違反があれば違反点をサンプルへ追加して再試行する
        const auto extra = FindContainmentViolations(
            polygon, circumscribed, s.plane_u, s.plane_v, s.t_region,
            s.region2d, s.period, s.t1, kViolationEps, inside_cache);
        if (extra.empty()) break;

        ts.insert(ts.end(), extra.begin(), extra.end());
        ts = Dedup(ts, s.is_closed, s.period, 1e-6);
    }

    return polygon;
}

/// @brief 外包/内包多角形の頂点列を計算する内部実装
///
/// @param curve 対象の閉曲線 (自己交差なし)
/// @param n_vert 初期分割数
/// @param circumscribed 外包なら true, 内包なら false
/// @param normal 平面の参照法線ベクトル
/// @param eps 曲率判定の閾値
/// @param closure_rel_tol 閉性判定の相対許容 (0なら厳密判定のみ)
/// @return 多角形データ
i_num::PolygonData ComputeExtremalPolygon(
        const i_ent::ICurve& curve,
        int n_vert,
        bool circumscribed,
        const Vector3d& normal,
        double eps,
        double closure_rel_tol) {
    const CurveCache cache(curve, normal);
    const ExtremalPolygonSharedData s =
        PrepareExtremalPolygonData(cache, n_vert, normal, closure_rel_tol);
    return RefineExtremalPolygon(cache, circumscribed, eps, s);
}

}  // namespace



namespace igesio::entities {

i_num::PolygonData ComputeCircumscribedPolygon(
        const ICurve& curve,
        int n_vert,
        const Vector3d& reference_normal,
        double eps,
        double closure_rel_tol) {
    return ComputeExtremalPolygon(
        curve, n_vert, /*circumscribed=*/true, reference_normal, eps,
        closure_rel_tol);
}

i_num::PolygonData ComputeInscribedPolygon(
        const ICurve& curve,
        int n_vert,
        const Vector3d& reference_normal,
        double eps,
        double closure_rel_tol) {
    return ComputeExtremalPolygon(
        curve, n_vert, /*circumscribed=*/false, reference_normal, eps,
        closure_rel_tol);
}

std::pair<i_num::PolygonData, i_num::PolygonData>
ComputeExtremalPolygonPair(
        const ICurve& curve,
        int n_vert,
        const Vector3d& reference_normal,
        double eps,
        double closure_rel_tol) {
    // 外包/内包に依存しない前処理を一度だけ計算して共有する
    const CurveCache cache(curve, reference_normal);
    const ExtremalPolygonSharedData s = PrepareExtremalPolygonData(
        cache, n_vert, reference_normal, closure_rel_tol);

    i_num::PolygonData circumscribed = RefineExtremalPolygon(
        cache, /*circumscribed=*/true, eps, s);
    i_num::PolygonData inscribed = RefineExtremalPolygon(
        cache, /*circumscribed=*/false, eps, s);
    return {std::move(circumscribed), std::move(inscribed)};
}

}  // namespace igesio::entities
