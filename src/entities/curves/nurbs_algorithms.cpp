/**
 * @file entities/curves/nurbs_algorithms.cpp
 * @brief 任意曲線の NURBS 近似・補間アルゴリズムの実装
 * @author Yayoi Habami
 * @date 2026-04-11
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/entities/curves/nurbs_algorithms.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iterator>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <vector>

#include <Eigen/Dense>

#include "igesio/common/errors.h"
#include "igesio/numerics/core/matrix.h"

namespace {

namespace i_ent = igesio::entities;
using igesio::kPi;
using igesio::Matrix3Xd;
using igesio::MatrixXd;
using igesio::Vector3d;



// =========================================================================
// 内部構造体
// =========================================================================

/// @brief 正規化された曲線 F(s): s ∈ [0,1]
struct NormalizedCurve {
    /// @brief F(s)の評価関数
    std::function<Vector3d(double)> point;
    /// @brief F'(s)の評価関数 (max_deriv_order >= 1 のとき有効)
    std::function<Vector3d(double)> deriv1;
    /// @brief F''(s)の評価関数 (max_deriv_order >= 2 のとき有効)
    std::function<Vector3d(double)> deriv2;
    /// @brief 利用可能な最大微分階数 (0, 1, 2)
    int max_deriv_order = 0;
};

/// @brief 端点でのNURBSパラメータに関する微分値
struct EndpointDerivatives {
    /// @brief 始点での1階微分 dC/dt|_{t=0}
    Vector3d d1_start = Vector3d::Zero();
    /// @brief 始点での2階微分 d²C/dt²|_{t=0}
    Vector3d d2_start = Vector3d::Zero();
    /// @brief 終点での1階微分 dC/dt|_{t=1}
    Vector3d d1_end = Vector3d::Zero();
    /// @brief 終点での2階微分 d²C/dt²|_{t=1}
    Vector3d d2_end = Vector3d::Zero();
};



// =========================================================================
// Step 0: 対象曲線の正規化
// =========================================================================

/// @brief ICurveを s ∈ [0,1] に正規化したNormalizedCurveを構築する
/// @param curve 対象の曲線
/// @param range パラメータ範囲 {t_min, t_max}
/// @return 正規化された曲線（利用可能な微分階数付き）
NormalizedCurve NormalizeParam(
        const i_ent::ICurve& curve,
        const std::array<double, 2>& range) {
    const double t_min = range[0], t_max = range[1];
    const double dt    = range[1] - range[0];

    // 利用可能な微分階数を確認する
    int max_order = 0;
    if (curve.TryGetDerivatives(t_min, 2)) {
        max_order = 2;
    } else if (curve.TryGetDerivatives(t_min, 1)) {
        max_order = 1;
    }

    NormalizedCurve F;
    F.max_deriv_order = max_order;

    // evalを介さず、各std::functionに直接ロジックを記述する
    // (clangのreleaseビルドで計算結果が異なる問題への対策)
    F.point = [&curve, t_min, t_max, dt](double s) {
        double t = std::clamp(t_min + s * dt, t_min, t_max);  // 精度誤差対策
        auto d = curve.TryGetDerivatives(t, 0);
        return d ? (*d)[0] : Vector3d::Zero();
    };

    F.deriv1 = [&curve, t_min, t_max, dt](double s) -> Vector3d {
        const double t = std::clamp(t_min + s * dt, t_min, t_max);
        const auto d = curve.TryGetDerivatives(t, 1);
        if (!d) return Vector3d::Zero();
        return (*d)[1] * dt;
    };

    if (max_order >= 2) {
        F.deriv2 = [&curve, t_min, t_max, dt](double s) -> Vector3d {
            const double t = std::clamp(t_min + s * dt, t_min, t_max);
            const auto d = curve.TryGetDerivatives(t, 2);
            if (!d) return Vector3d::Zero();
            return ((*d)[2] * dt) * dt;
        };
    }
    return F;
}



// =========================================================================
// サンプリングとコード長パラメータ化
// =========================================================================

/// @brief [0,1] をL等分してサンプル点列を得る
/// @param F 正規化された曲線
/// @param L サンプリング分割数
/// @return サンプル点列 Q_0, ..., Q_L (サイズL+1)
std::vector<Vector3d> SampleCurve(
        const NormalizedCurve& F, unsigned int L) {
    std::vector<Vector3d> samples(L + 1);
    for (unsigned int l = 0; l <= L; ++l) {
        samples[l] = F.point(static_cast<double>(l) / L);
    }
    return samples;
}

/// @brief コード長パラメータ化：各サンプル点にt̄_lを割り当てる
/// @param samples サンプル点列 Q_0, ..., Q_L
/// @return コード長パラメータt̄_l (サイズ L+1, t̄_0=0, t̄_L=1)
/// @throw igesio::ComputationError 総弦長がゼロの場合
std::vector<double> ChordLengthParams(
        const std::vector<Vector3d>& samples) {
    const auto L = static_cast<unsigned int>(samples.size() - 1);
    std::vector<double> t_bar(L + 1, 0.0);

    double total = 0.0;
    for (unsigned int l = 1; l <= L; ++l) {
        total += (samples[l] - samples[l - 1]).norm();
        t_bar[l] = total;
    }
    if (total < 1e-15) {
        throw igesio::ComputationError(
            "ChordLengthParams: 総弦長がゼロです（点が重複しています）。");
    }
    for (unsigned int l = 1; l <= L; ++l) {
        t_bar[l] /= total;
    }
    t_bar[L] = 1.0;  // 数値誤差を補正
    return t_bar;
}

/// @brief 粗いサンプリングで ||F''||_∞ を推定する
/// @param F 正規化された曲線
/// @param n サンプル数（デフォルト20）
/// @return ||F''(s)||_∞ の推定値。max_deriv_order < 2 の場合は0.0
double CoarseSampleMaxSecondDeriv(
        const NormalizedCurve& F, unsigned int n = 20) {
    if (F.max_deriv_order < 2) return 0.0;
    double max_d2 = 0.0;
    for (unsigned int i = 0; i <= n; ++i) {
        const double s = static_cast<double>(i) / n;
        max_d2 = std::max(max_d2, F.deriv2(s).norm());
    }
    return max_d2;
}

/// @brief 端点でのNURBSパラメータ微分値を計算する
/// @param F     正規化された曲線（max_deriv_order に応じて使用する式を制限）
/// @param t_bar コード長パラメータ列（サイズ >= 4）
/// @return 端点微分値（利用可能な階数分のみ計算）
EndpointDerivatives ComputeEndpointDerivatives(
        const NormalizedCurve& F,
        const std::vector<double>& t_bar) {
    EndpointDerivatives D;
    if (F.max_deriv_order < 1) return D;

    const double L      = static_cast<double>(t_bar.size() - 1);
    const double sigma0 = L * t_bar[1];
    const double sigmaL = L * (1.0 - t_bar[t_bar.size() - 2]);

    if (sigma0 < 1e-15 || sigmaL < 1e-15) return D;

    D.d1_start = F.deriv1(0.0) / sigma0;
    D.d1_end   = F.deriv1(1.0) / sigmaL;

    if (F.max_deriv_order < 2) return D;

    // 2階差分係数 σ_0'', σ_L''
    const double sigma0pp =
        L * L * (t_bar[2] - 2.0 * t_bar[1]);
    const double sigmaLpp =
        L * L * (1.0 - 2.0 * t_bar[t_bar.size() - 2]
                     + t_bar[t_bar.size() - 3]);

    // スケール因子dt^2が大きい場合、d2 が巨大になりすぎるのを防ぐ
    // 座標系のスケール（サンプルのバウンディングボックス等）と比較して
    // 異常に大きい場合は、2階微分の拘束を放棄するかクランプする
    Vector3d d2s = (F.deriv2(0.0) - sigma0pp * D.d1_start) / (sigma0 * sigma0);
    Vector3d d2e = (F.deriv2(1.0) - sigmaLpp * D.d1_end) / (sigmaL * sigmaL);

    // 数値的安定性のための簡易的なガード（必要に応じて閾値を調整）
    if (d2s.norm() < 1e10) D.d2_start = d2s;
    if (d2e.norm() < 1e10) D.d2_end = d2e;

    return D;
}



// =========================================================================
// 次数と制御点数の決定
// =========================================================================

/// @brief サンプル点の方向変化量から初期制御点数kを推定する
/// @param samples   サンプル点列
/// @param r         端点で固定する制御点数（片側）
/// @param m         B-スプラインの次数
/// @param theta_tol 1制御点あたりの許容方向変化角 [rad]
/// @return 初期 k の推定値（k_min以上）
unsigned int EstimateK(
        const std::vector<Vector3d>& samples,
        unsigned int r, unsigned int m, double theta_tol) {
    const unsigned int k_min = std::max(m, 2 * r);

    double theta = 0.0;
    for (size_t l = 1; l + 1 < samples.size(); ++l) {
        const Vector3d a = samples[l]     - samples[l - 1];
        const Vector3d b = samples[l + 1] - samples[l];
        const double na = a.norm(), nb = b.norm();
        if (na < 1e-15 || nb < 1e-15) continue;
        const double cos_t =
            std::clamp(a.dot(b) / (na * nb), -1.0, 1.0);
        theta += std::acos(cos_t);
    }

    if (theta_tol < 1e-15) theta_tol = kPi / 4.0;
    const auto seg = static_cast<unsigned int>(
        std::ceil(theta / theta_tol));
    return std::max(k_min, 2 * r + seg);
}

/// @brief サンプリング分割数Lを決定する
/// @param k      制御点の最大インデックス
/// @param r      端点で固定する制御点数（片側）
/// @param max_d2 ||F''||_∞ の推定値（0の場合は曲率ベース下限を無視）
/// @param eps    許容誤差
/// @return サンプリング分割数 L
unsigned int ComputeL(
        unsigned int k, unsigned int r,
        double max_d2, double eps) {
    // 最小二乗系がoverdeterminedとなる下限
    const unsigned int L_min =
        (k >= 2 * r) ? (k - 2 * r + 2) : 3u;
    // 初期目安: 制御点数の20倍
    unsigned int L = std::max(L_min, 20 * (k + 1));

    // サンプリング誤差が許容誤差を下回る下限
    if (max_d2 > 0.0 && eps > 0.0) {
        const auto L_acc = static_cast<unsigned int>(
            std::ceil(std::sqrt(max_d2 / (8.0 * eps))));
        L = std::max(L, L_acc);
    }
    return L;
}



// =========================================================================
// ノットベクトルの構築
// =========================================================================

/// @brief クランプ型ノットベクトルを平均化法で構築する
/// @param m     B-スプラインの次数
/// @param k     制御点の最大インデックス
/// @param t_bar コード長パラメータ列（サイズ >= k）
/// @return ノットベクトル（サイズ k+m+2）
std::vector<double> BuildKnotVector(
        unsigned int m, unsigned int k,
        const std::vector<double>& t_bar) {
    std::vector<double> knots(k + m + 2, 0.0);

    // 末尾 m+1 個を 1.0 に設定（端点クランプ）
    for (unsigned int i = k + 1; i <= k + m + 1; ++i) {
        knots[i] = 1.0;
    }

    // 内部ノット: Piegl & Tillerの式に基づきt_barを均等間隔で参照する
    // d = (L+1)/(k-m+1) を刻みとして線形補間で位置を決定することで、
    // t_bar[1..k] しか使わない旧実装の偏りを解消する
    if (k > m) {
        const double L   = static_cast<double>(t_bar.size() - 1);
        const double d   = (L + 1.0) / static_cast<double>(k - m + 1);
        for (unsigned int j = 1; j <= k - m; ++j) {
            const double frac  = j * d;
            const auto   idx   = static_cast<unsigned int>(frac);
            const double alpha = frac - idx;
            const unsigned int i0 = std::min(idx,     static_cast<unsigned int>(L));
            const unsigned int i1 = std::min(idx + 1, static_cast<unsigned int>(L));
            knots[m + j] = (1.0 - alpha) * t_bar[i0] + alpha * t_bar[i1];
        }
    }

    // 単調非減少を強制補正
    for (unsigned int i = 1; i <= k; ++i) {
        knots[i] = std::max(knots[i - 1], knots[i]);
    }
    return knots;
}



// =========================================================================
// ノットスパン探索と B-スプライン基底関数評価
// =========================================================================

/// @brief ノットスパンのインデックスを返す
/// @param t     パラメータ値 t ∈ [0,1]
/// @param m     B-スプラインの次数
/// @param k     制御点の最大インデックス
/// @param knots ノットベクトル（サイズk+m+2）
/// @return j ∈ [m, k] s.t. knots[j] <= t < knots[j+1]
int FindKnotSpan(
        double t, int m, int k,
        const std::vector<double>& knots) {
    // t = 1.0 の特殊処理: 末端クランプの手前のスパンを返す
    if (t >= 1.0) {
        for (int j = k; j >= m; --j) {
            if (knots[j] < 1.0) return j;
        }
        return k;
    }
    // 二分探索で knots[j] <= t < knots[j+1] を満たすjを求める
    int lo = m, hi = k;
    while (lo < hi) {
        const int mid = (lo + hi + 1) / 2;
        if (knots[mid] <= t) lo = mid;
        else                  hi = mid - 1;
    }
    return lo;
}

/// @brief B-スプライン基底関数 B_{i,m}(t) を評価する（de Boor 三角スキーム）
/// @param i     制御点インデックス (0 <= i <= k)
/// @param m     B-スプラインの次数
/// @param t     パラメータ値
/// @param knots ノットベクトル（0-indexed, サイズ k+m+2）
/// @return B_{i,m}(t) の値
double EvalBasis(
        int i, int m, double t,
        const std::vector<double>& knots) {
    const int k = static_cast<int>(knots.size()) - m - 2;

    // 三角スキームの底辺: d[j] = B_{i+j,0}(t)
    std::vector<double> d(m + 1, 0.0);
    if (t >= 1.0) {
        // t=1.0 はFindKnotSpanで求めたスパンspanにのみ寄与する
        const int span = FindKnotSpan(1.0, m, k, knots);
        const int j_active = span - i;
        if (j_active >= 0 && j_active <= m) d[j_active] = 1.0;
    } else {
        for (int j = 0; j <= m; ++j) {
            const double lo = knots[i + j];
            const double hi = knots[i + j + 1];
            d[j] = (lo <= t && t < hi) ? 1.0 : 0.0;
        }
    }

    // de Boor 再帰
    for (int p = 1; p <= m; ++p) {
        for (int j = 0; j <= m - p; ++j) {
            const double t0 = knots[i + j];
            const double t1 = knots[i + j + p];
            const double t2 = knots[i + j + 1];
            const double t3 = knots[i + j + p + 1];
            const double a =
                (t1 > t0) ? (t - t0) / (t1 - t0) : 0.0;
            const double b =
                (t3 > t2) ? (t3 - t) / (t3 - t2) : 0.0;
            d[j] = a * d[j] + b * d[j + 1];
        }
    }
    return d[0];
}



// =========================================================================
// 端点拘束による制御点の直接決定
// =========================================================================

/// @brief 端点周辺の制御点を解析的に決定する
/// @param p0    始点 F(0)
/// @param pk    終点 F(1)
/// @param D     端点微分値
/// @param m     B-スプラインの次数
/// @param k     制御点の最大インデックス
/// @param r     端点で固定する制御点数（片側, 1〜3）
/// @param knots ノットベクトル
/// @return 3×(k+1) の制御点行列（端点インデックス 0..r-1, k-r+1..k のみ有効）
Matrix3Xd FixEndpointControls(
        const Vector3d& p0, const Vector3d& pk,
        const EndpointDerivatives& D,
        unsigned int m, unsigned int k, unsigned int r,
        const std::vector<double>& knots) {
    Matrix3Xd P = Matrix3Xd::Zero(3, static_cast<int>(k + 1));

    // r >= 1: 位置の固定
    P.col(0)              = p0;
    P.col(static_cast<int>(k)) = pk;
    if (r < 2) return P;

    // r >= 2: 1階微分の一致
    // C'(0) = m*(P_1-P_0)/t_1 → P_1 = P_0 + (t_1/m)*D1_start
    // C'(1) = m*(P_k-P_{k-1})/(1-t_{k-m}) → P_{k-1} = P_k - ...
    const double t1  = knots[m + 1];       // t_1
    const double tkm = knots[k];           // t_{k-m}
    const double dm  = static_cast<double>(m);
    P.col(1)                    = p0 + (t1 / dm) * D.d1_start;
    P.col(static_cast<int>(k) - 1) =
        pk - ((1.0 - tkm) / dm) * D.d1_end;
    if (r < 3) return P;

    // r >= 3: 2階微分の一致
    const double t2   = knots[m + 2];     // t_2
    const double tkm1 = knots[k - 1];     // t_{k-1-m}
    P.col(2) = P.col(1)
        + t2 * ((P.col(1) - P.col(0)) / t1
                + (t1 / (dm * (dm - 1.0))) * D.d2_start);
    P.col(static_cast<int>(k) - 2) = P.col(static_cast<int>(k) - 1)
        - (1.0 - tkm1)
          * ((P.col(static_cast<int>(k))
              - P.col(static_cast<int>(k) - 1)) / (1.0 - tkm)
             - ((1.0 - tkm) / (dm * (dm - 1.0))) * D.d2_end);
    return P;
}



// =========================================================================
// 内部制御点の最小二乗求解
// =========================================================================

/// @brief 最小二乗用の残差行列Rを計算する
/// @param samples  サンプル点列
/// @param t_bar    コード長パラメータ
/// @param P_fixed  端点制御点行列（端点インデックスのみ有効）
/// @param m        B-スプラインの次数
/// @param k        制御点の最大インデックス
/// @param r        端点で固定する制御点数（片側）
/// @param knots    ノットベクトル
/// @return (L-1)×3 の残差行列R
MatrixXd ComputeResidual(
        const std::vector<Vector3d>& samples,
        const std::vector<double>& t_bar,
        const Matrix3Xd& P_fixed,
        unsigned int m, unsigned int k, unsigned int r,
        const std::vector<double>& knots) {
    const int L = static_cast<int>(t_bar.size()) - 1;
    MatrixXd R = MatrixXd::Zero(L - 1, 3);

    for (int l = 1; l < L; ++l) {
        const double t  = t_bar[l];
        Vector3d residual = samples[l];

        // 始点側固定制御点の寄与を除く
        for (unsigned int i = 0; i < r; ++i) {
            residual -= EvalBasis(static_cast<int>(i), m, t, knots)
                        * P_fixed.col(static_cast<int>(i));
        }
        // 終点側固定制御点の寄与を除く
        for (unsigned int i = k - r + 1; i <= k; ++i) {
            residual -= EvalBasis(static_cast<int>(i), m, t, knots)
                        * P_fixed.col(static_cast<int>(i));
        }
        R.row(l - 1) = residual.transpose();
    }
    return R;
}

/// @brief 基底行列 B̂ を構築する
/// @param m     B-スプラインの次数
/// @param k     制御点の最大インデックス
/// @param r     端点で固定する制御点数（片側）
/// @param t_bar コード長パラメータ（内部点 t_bar[1..L-1] を使用）
/// @param knots ノットベクトル
/// @return (L-1)×(k-2r+1) の基底行列 B̂
MatrixXd BuildBasisMatrix(
        unsigned int m, unsigned int k, unsigned int r,
        const std::vector<double>& t_bar,
        const std::vector<double>& knots) {
    const int L      = static_cast<int>(t_bar.size()) - 1;
    const int n_free =
        static_cast<int>(k) - 2 * static_cast<int>(r) + 1;
    MatrixXd B_hat = MatrixXd::Zero(L - 1, n_free);

    for (int l = 1; l < L; ++l) {
        const double t = t_bar[l];
        for (int j = 0; j < n_free; ++j) {
            B_hat(l - 1, j) = EvalBasis(
                static_cast<int>(r) + j, m, t, knots);
        }
    }
    return B_hat;
}

/// @brief 端点制御点と内部制御点を合成して全制御点行列を返す
/// @param P_fixed 端点制御点（端点インデックスのみ有効）
/// @param X_free  内部制御点 3×(k-2r+1)
/// @param k       制御点の最大インデックス
/// @param r       端点で固定する制御点数（片側）
/// @return 全制御点行列 3×(k+1)
Matrix3Xd MergeControlPoints(
        const Matrix3Xd& P_fixed,
        const Matrix3Xd& X_free,
        unsigned int k, unsigned int r) {
    Matrix3Xd P = P_fixed;
    const int n_free =
        static_cast<int>(k) - 2 * static_cast<int>(r) + 1;
    for (int j = 0; j < n_free; ++j) {
        P.col(static_cast<int>(r) + j) = X_free.col(j);
    }
    return P;
}



// =========================================================================
// 誤差評価と適応的ノット挿入
// =========================================================================

/// @brief NURBS 曲線C(t)を評価する
/// @param m     B-スプラインの次数
/// @param k     制御点の最大インデックス
/// @param knots ノットベクトル
/// @param P     制御点行列 3×(k+1)
/// @param t     パラメータ値
/// @return C(t)の座標値
Vector3d EvalNurbs(
        unsigned int m, unsigned int k,
        const std::vector<double>& knots,
        const Matrix3Xd& P, double t) {
    Vector3d C = Vector3d::Zero();
    for (unsigned int i = 0; i <= k; ++i) {
        C += EvalBasis(static_cast<int>(i), m, t, knots) * P.col(i);
    }
    return C;
}

/// @brief 各サンプル点での近似誤差を計算する
/// @param samples サンプル点列
/// @param t_bar   コード長パラメータ
/// @param m       B-スプラインの次数
/// @param k       制御点の最大インデックス
/// @param knots   ノットベクトル
/// @param P       制御点行列 3×(k+1)
/// @return 各サンプルの誤差ベクトル (サイズ L+1)
std::vector<double> ComputeErrors(
        const std::vector<Vector3d>& samples,
        const std::vector<double>& t_bar,
        unsigned int m, unsigned int k,
        const std::vector<double>& knots,
        const Matrix3Xd& P) {
    std::vector<double> errors(samples.size());
    for (size_t l = 0; l < samples.size(); ++l) {
        errors[l] =
            (samples[l] - EvalNurbs(m, k, knots, P, t_bar[l])).norm();
    }
    return errors;
}

/// @brief ノットスパンの最小幅、および挿入位置と既存ノットの最小間隔
constexpr double kMinSpanWidth = 1e-12;

/// @brief スパンを分割するノット位置を決定する
/// @param beg  スパン内部 (t_lo, t_hi) にあるサンプルの先頭
/// @param end  同、終端
/// @param t_lo スパン下端
/// @param t_hi スパン上端
/// @return 挿入位置。分割不能な場合はstd::nullopt
/// @note 分割後の両半スパンに内部サンプルが1点以上残る位置のみを返す。
///       重複点により同一パラメータのサンプルが複数存在しうるため、
///       判定は相異なるパラメータ値の上で行う
std::optional<double> FindSplitPosition(
        std::vector<double>::const_iterator beg,
        std::vector<double>::const_iterator end,
        double t_lo, double t_hi) {
    // スパン内部の相異なるパラメータ値の個数を数える
    std::size_t n_distinct = 0;
    for (auto it = beg; it != end; it = std::upper_bound(it, end, *it)) {
        ++n_distinct;
    }
    // 相異なる値が2点未満のスパンは、どこで割っても片側が空になる
    if (n_distinct < 2) return std::nullopt;

    // 中点分割で両半スパンに内部サンプルが残るなら中点へ（従来と同一の挙動）
    const double mid = (t_lo + t_hi) * 0.5;
    double pos = mid;
    if (!(*beg < mid && mid < *std::prev(end))) {
        // 残らない場合は、相異なる値の中央にある隣接2値の中間へ挿入する
        auto it = beg;
        for (std::size_t c = 0; c + 1 < n_distinct / 2; ++c) {
            it = std::upper_bound(it, end, *it);
        }
        pos = 0.5 * (*it + *std::upper_bound(it, end, *it));
    }
    if (pos - t_lo < kMinSpanWidth || t_hi - pos < kMinSpanWidth) {
        return std::nullopt;
    }
    return pos;
}

/// @brief 誤差が閾値を超えるサンプルのスパンにノットを挿入する
/// @param knots  現在のノットベクトル（挿入に成功した場合のみ変更される）
/// @param errors サンプル毎の近似誤差（t_barと同数）
/// @param t_bar  コード長パラメータ（昇順）
/// @param eps    許容誤差
/// @return ノット挿入に成功した場合は true
/// @note 内部サンプルを持たないスパンにのみ台をもつ基底は最小二乗で拘束されず、
///       B̂の該当列が微小値となって制御点が発散する（接線不連続を含む点列で
///       実際に発生した）。そのため分割後の両半スパンに内部サンプルが残る
///       分割のみを行い、対象スパンが分割不能な場合は誤差降順に
///       次のサンプルのスパンを試す
bool InsertKnot(
        std::vector<double>& knots,
        const std::vector<double>& errors,
        const std::vector<double>& t_bar,
        double eps) {
    // 許容誤差を超えるサンプルのみを誤差降順に並べる
    std::vector<std::size_t> order;
    order.reserve(errors.size());
    for (std::size_t l = 0; l < errors.size(); ++l) {
        if (errors[l] > eps) order.push_back(l);
    }
    std::sort(order.begin(), order.end(),
              [&errors](std::size_t a, std::size_t b) {
                  return errors[a] > errors[b];
              });

    for (const auto l : order) {
        // t_bar[l]を含むノットスパン [t_lo, t_hi) を二分探索で特定する
        const auto it_hi =
            std::upper_bound(knots.begin(), knots.end(), t_bar[l]);
        if (it_hi == knots.begin() || it_hi == knots.end()) continue;
        const double t_hi = *it_hi;
        const double t_lo = *std::prev(it_hi);
        if (t_hi - t_lo < kMinSpanWidth) continue;

        // スパン内部 (t_lo, t_hi) にあるサンプルの範囲 [beg, end) を求める
        const auto beg = std::upper_bound(t_bar.begin(), t_bar.end(), t_lo);
        const auto end = std::lower_bound(t_bar.begin(), t_bar.end(), t_hi);

        const auto pos = FindSplitPosition(beg, end, t_lo, t_hi);
        if (!pos) continue;

        knots.insert(it_hi, *pos);
        return true;
    }
    return false;
}



// =========================================================================
// 反復近似ループ
// =========================================================================

/// @brief 反復ループを実行し、制御点とノットを確定する
/// @param samples   サンプル点列
/// @param t_bar     コード長パラメータ
/// @param D         端点微分値
/// @param m         B-スプラインの次数
/// @param k_init    初期制御点最大インデックス
/// @param r         端点で固定する制御点数（片側）
/// @param options   近似オプション
/// @param knots_out 確定したノットベクトル（出力）
/// @param ctrl_out  確定した制御点行列（出力）
void RunApproxLoop(
        const std::vector<Vector3d>& samples,
        const std::vector<double>& t_bar,
        const EndpointDerivatives& D,
        unsigned int m, unsigned int k_init, unsigned int r,
        const i_ent::NurbsApproxOptions& options,
        std::vector<double>& knots_out,
        Matrix3Xd& ctrl_out) {
    unsigned int k = k_init;
    const Vector3d p0 = samples.front();
    const Vector3d pk = samples.back();

    // Step 3: 初期ノットベクトルを平均化法で構築する
    auto knots = BuildKnotVector(m, k, t_bar);

    while (true) {
        // Step 4
        const Matrix3Xd P_fixed =
            FixEndpointControls(p0, pk, D, m, k, r, knots);
        // Step 5
        const MatrixXd B_hat =
            BuildBasisMatrix(m, k, r, t_bar, knots);
        const MatrixXd R_mat =
            ComputeResidual(samples, t_bar, P_fixed, m, k, r, knots);
        const Matrix3Xd X_free =
            B_hat.colPivHouseholderQr().solve(R_mat).transpose();
        const Matrix3Xd P =
            MergeControlPoints(P_fixed, X_free, k, r);

        // Step 6: 誤差評価
        const auto errors =
            ComputeErrors(samples, t_bar, m, k, knots, P);
        const double e_max =
            *std::max_element(errors.begin(), errors.end());

        // 終了判定: 精度達成、制御点上限、またはサンプル数に対して劣決定になる場合
        if (e_max <= options.tolerance
                || k + 1 >= options.max_control_points
                || k + 1 >= static_cast<unsigned int>(t_bar.size())) {
            knots_out = std::move(knots);
            ctrl_out  = P;
            return;
        }

        // ノット挿入とkのインクリメント
        if (!InsertKnot(knots, errors, t_bar, options.tolerance)) {
            knots_out = std::move(knots);
            ctrl_out  = P;
            return;
        }
        ++k;
    }
}



// =========================================================================
// 局所エルミート補間: 点列の前処理
// =========================================================================

/// @brief 点列の相対的な同一点判定の係数 (折れ線長に対する比)
/// @note ノットの間隔が倍精度の分解能を下回らないようにする
constexpr double kRelativeDuplicateTolerance = 1e-12;

/// @brief 連続重複点を統合した点列
struct DistinctPoints {
    /// @brief 相異なる点列 P_0, ..., P_n
    std::vector<Vector3d> points;
    /// @brief 各入力点が統合された先のインデックス (入力点と同数)
    std::vector<std::size_t> index_of_input;
};

/// @brief 補間オプションの値を検証する
/// @param options 補間オプション
/// @throw std::invalid_argument いずれかの値が範囲外または非有限の場合
void ValidateInterpOptions(const i_ent::NurbsInterpOptions& options) {
    if (!(options.corner_angle > 0.0 && options.corner_angle <= kPi / 2.0)) {
        throw std::invalid_argument(
            "InterpolateWithNurbs: corner_angle must be in (0, pi/2].");
    }
    if (!(options.tangent_tolerance >= 0.0
          && options.tangent_tolerance <= kPi / 2.0)) {
        throw std::invalid_argument(
            "InterpolateWithNurbs: tangent_tolerance must be in [0, pi/2].");
    }
    if (!(options.duplicate_tolerance >= 0.0
          && std::isfinite(options.duplicate_tolerance))) {
        throw std::invalid_argument(
            "InterpolateWithNurbs: duplicate_tolerance must be"
            " finite and non-negative.");
    }
}

/// @brief 連続重複点を統合する
/// @param points    入力点列 (全点が有限であること)
/// @param tolerance 同一点とみなす距離の絶対値
/// @return 統合後の点列と入力点との対応
/// @note 距離の閾値にはtoleranceと折れ線長の相対値の大きい方を使う。
///       統合した点の座標は、最初に現れた入力点のものとする
DistinctPoints MergeDuplicatePoints(
        const std::vector<Vector3d>& points, double tolerance) {
    double polyline_length = 0.0;
    for (std::size_t i = 1; i < points.size(); ++i) {
        polyline_length += (points[i] - points[i - 1]).norm();
    }
    const double tol = std::max(
        tolerance, kRelativeDuplicateTolerance * polyline_length);

    DistinctPoints result;
    result.points.reserve(points.size());
    result.index_of_input.reserve(points.size());
    for (const auto& p : points) {
        if (result.points.empty() || (p - result.points.back()).norm() > tol) {
            result.points.push_back(p);
        }
        result.index_of_input.push_back(result.points.size() - 1);
    }
    return result;
}

/// @brief 2つのベクトルのなす角を計算する
/// @param a 零でないベクトル
/// @param b 零でないベクトル
/// @return なす角 [rad] (0.0〜π)
double AngleBetween(const Vector3d& a, const Vector3d& b) {
    return std::atan2(a.cross(b).norm(), a.dot(b));
}

/// @brief 点列の各点が角点かを判定する
/// @param dirs         各弦の単位方向ベクトル u_0, ..., u_{n-1}
/// @param corner_angle 角点とみなす折れ角の閾値 [rad]
/// @return 点ごとの判定結果 (サイズn+1、両端点は常にfalse)
std::vector<bool> DetectCorners(
        const std::vector<Vector3d>& dirs, double corner_angle) {
    std::vector<bool> corners(dirs.size() + 1, false);
    for (std::size_t k = 1; k < dirs.size(); ++k) {
        corners[k] = AngleBetween(dirs[k - 1], dirs[k]) > corner_angle;
    }
    return corners;
}



// =========================================================================
// 局所エルミート補間: 接線の決定
// =========================================================================

/// @brief 各点の左右の単位接線
/// @note 角点以外ではincomingとoutgoingは等しい。始点のincomingと
///       終点のoutgoingは使わない
struct PointTangents {
    /// @brief 点に入る側の接線 (前の区間の終端接線)
    std::vector<Vector3d> incoming;
    /// @brief 点から出る側の接線 (次の区間の始端接線)
    std::vector<Vector3d> outgoing;
};

/// @brief 3点を通る放物線の始端の単位接線を推定する
/// @param u0 始端側の弦の単位方向
/// @param u1 次の弦の単位方向
/// @param h0 始端側の弦長
/// @param h1 次の弦長
/// @return 始端の単位接線
/// @note u0とu1のなす角がπ/2以下であれば、戻り値はu0と鋭角をなす
Vector3d EstimateEndTangent(
        const Vector3d& u0, const Vector3d& u1, double h0, double h1) {
    const double a = h0 / (h0 + h1);
    return ((1.0 + a) * u0 - a * u1).normalized();
}

/// @brief 3点を通る放物線の中央点での単位接線を推定する (Bessel法)
/// @param u_in  点に入る弦の単位方向
/// @param u_out 点から出る弦の単位方向
/// @param h_in  点に入る弦の長さ
/// @param h_out 点から出る弦の長さ
/// @return 単位接線 (u_inとu_outの正係数の線形結合)
Vector3d EstimateInteriorTangent(
        const Vector3d& u_in, const Vector3d& u_out,
        double h_in, double h_out) {
    return (h_out * u_in + h_in * u_out).normalized();
}

/// @brief 角点で区切った各区間列の接線を推定する
/// @param dirs    各弦の単位方向ベクトル (サイズn)
/// @param lengths 各弦の長さ (サイズn)
/// @param corners 各点が角点か (サイズn+1)
/// @return 各点の左右の単位接線
/// @note 区間列の端 (始点・終点・角点) では放物線端条件を、内部では
///       Bessel法を使う。区間列が1区間のみの場合は弦の方向とする
PointTangents EstimateTangents(
        const std::vector<Vector3d>& dirs,
        const std::vector<double>& lengths,
        const std::vector<bool>& corners) {
    const std::size_t n = dirs.size();
    PointTangents result{std::vector<Vector3d>(n + 1, Vector3d::Zero()),
                         std::vector<Vector3d>(n + 1, Vector3d::Zero())};
    std::size_t lo = 0;
    while (lo < n) {
        // 次の角点または終点までを1つの区間列とする
        std::size_t hi = lo + 1;
        while (hi < n && !corners[hi]) ++hi;

        if (hi - lo == 1) {
            result.outgoing[lo] = dirs[lo];
            result.incoming[hi] = dirs[lo];
        } else {
            result.outgoing[lo] = EstimateEndTangent(
                dirs[lo], dirs[lo + 1], lengths[lo], lengths[lo + 1]);
            result.incoming[hi] = -EstimateEndTangent(
                -dirs[hi - 1], -dirs[hi - 2],
                lengths[hi - 1], lengths[hi - 2]);
        }
        for (std::size_t k = lo + 1; k < hi; ++k) {
            const Vector3d t = EstimateInteriorTangent(
                dirs[k - 1], dirs[k], lengths[k - 1], lengths[k]);
            result.incoming[k] = t;
            result.outgoing[k] = t;
        }
        lo = hi;
    }
    return result;
}

/// @brief 与えられた接線が推定接線と整合するかを判定する
/// @param given     与えられた単位接線
/// @param estimated 推定された単位接線
/// @param chord     隣接する弦の単位方向
/// @param tolerance 推定接線との角度差の上限 [rad]
/// @return 整合する場合はtrue
bool IsConsistentTangent(
        const Vector3d& given, const Vector3d& estimated,
        const Vector3d& chord, double tolerance) {
    return AngleBetween(given, estimated) <= tolerance
        && given.dot(chord) >= 0.0;
}

/// @brief 与えられた接線のうち推定接線と整合するものを採用する
/// @param estimated 推定した各点の左右の単位接線
/// @param tangents  入力点ごとの接線方向 (空、または入力点と同数)
/// @param distinct  統合後の点列と入力点との対応
/// @param dirs      各弦の単位方向ベクトル
/// @param corners   各点が角点か
/// @param tolerance 推定接線との角度差の上限 [rad]
/// @return 採用後の各点の左右の単位接線
/// @note 統合された点では、最初に現れた有効な接線のみを候補とする
PointTangents ApplyGivenTangents(
        PointTangents estimated,
        const std::vector<std::optional<Vector3d>>& tangents,
        const DistinctPoints& distinct,
        const std::vector<Vector3d>& dirs,
        const std::vector<bool>& corners,
        double tolerance) {
    const std::size_t n = dirs.size();
    std::vector<bool> visited(n + 1, false);
    for (std::size_t i = 0; i < tangents.size(); ++i) {
        const std::size_t k = distinct.index_of_input[i];
        const auto& t = tangents[i];
        if (visited[k] || corners[k] || !t || !t->allFinite()
                || !(t->norm() > 0.0)) {
            continue;
        }
        visited[k] = true;

        const Vector3d g = t->normalized();
        const bool ok_in = k == 0 || IsConsistentTangent(
            g, estimated.incoming[k], dirs[k - 1], tolerance);
        const bool ok_out = k == n || IsConsistentTangent(
            g, estimated.outgoing[k], dirs[k], tolerance);
        if (!ok_in || !ok_out) continue;

        estimated.incoming[k] = g;
        estimated.outgoing[k] = g;
    }
    return estimated;
}



// =========================================================================
// 局所エルミート補間: B-スプラインの構築
// =========================================================================

/// @brief エルミート区間を並べたB-スプライン
struct HermiteBSpline {
    /// @brief 制御点行列
    Matrix3Xd control_points;
    /// @brief ノットベクトル (0.0〜1.0でクランプ)
    std::vector<double> knots;
    /// @brief 各点P_kのパラメータ (サイズn+1)
    std::vector<double> node_params;
};

/// @brief 各区間の3次ベジエを連結したB-スプラインを構築する
/// @param points   相異なる点列 P_0, ..., P_n
/// @param lengths  各弦の長さ h_0, ..., h_{n-1}
/// @param tangents 各点の左右の単位接線
/// @param corners  各点が角点か
/// @return 制御点、ノット、各点のパラメータ
/// @note 区間kのベジエ制御点をP_k + (h_k/3)T_k、P_{k+1} - (h_k/3)T_{k+1}とし、
///       パラメータ幅をh_kに比例させることで、角点以外の接続点でC¹連続となる。
///       そのため接続点を2重ノットとして制御点から除き、角点のみ3重ノット
///       として点自体を制御点に含める
HermiteBSpline BuildHermiteBSpline(
        const std::vector<Vector3d>& points,
        const std::vector<double>& lengths,
        const PointTangents& tangents,
        const std::vector<bool>& corners) {
    const std::size_t n = lengths.size();
    HermiteBSpline result;
    result.node_params.assign(n + 1, 0.0);
    std::partial_sum(lengths.begin(), lengths.end(),
                     result.node_params.begin() + 1);
    const double total = result.node_params.back();
    for (auto& u : result.node_params) u /= total;

    std::vector<Vector3d> ctrl{points[0]};
    result.knots.assign(4, 0.0);
    for (std::size_t k = 0; k < n; ++k) {
        const double leg = lengths[k] / 3.0;
        ctrl.push_back(points[k] + leg * tangents.outgoing[k]);
        ctrl.push_back(points[k + 1] - leg * tangents.incoming[k + 1]);
        if (k + 1 == n) break;

        const int multiplicity = corners[k + 1] ? 3 : 2;
        if (corners[k + 1]) ctrl.push_back(points[k + 1]);
        result.knots.insert(result.knots.end(), multiplicity,
                            result.node_params[k + 1]);
    }
    ctrl.push_back(points[n]);
    result.knots.insert(result.knots.end(), 4, 1.0);

    result.control_points.resize(3, static_cast<int>(ctrl.size()));
    for (std::size_t i = 0; i < ctrl.size(); ++i) {
        result.control_points.col(static_cast<int>(i)) = ctrl[i];
    }
    return result;
}

}  // namespace



namespace igesio::entities {

// =========================================================================
// 公開 API
// =========================================================================

std::shared_ptr<RationalBSplineCurve> ApproximateWithNurbs(
        const std::vector<Vector3d>& points,
        const NurbsEndpointTangents& tangents,
        const NurbsApproxOptions& options) {
    if (points.size() < 2) {
        throw std::invalid_argument(
            "ApproximateWithNurbs: 点が2点未満です。");
    }

    const unsigned int m = options.degree;
    // 両端に接線が提供された場合のみ r=2 とする
    const unsigned int r =
        (tangents.start && tangents.end) ? 2u : 1u;

    // コード長パラメータ化
    const auto t_bar = ChordLengthParams(points);

    // 端点微分値: 提供された接線方向 + 弦長スケールで推定する
    EndpointDerivatives D;
    if (r >= 2 && t_bar[1] > 1e-15) {
        // dC/dt|_{t=0} ≈ (Q_1-Q_0) / t̄_1; 方向は提供値を優先する
        const double mag0 =
            (points[1] - points[0]).norm() / t_bar[1];
        const double magL =
            (points.back() - points[points.size() - 2]).norm()
            / (1.0 - t_bar[t_bar.size() - 2]);
        D.d1_start = tangents.start->normalized() * mag0;
        D.d1_end   = tangents.end->normalized()   * magL;
    }

    // 初期制御点数の推定
    // BuildKnotVector の事前条件 (t_bar.size() >= k+1) を保証するため、
    // k_init を points.size()-1 でクランプする。
    // k_min 自体が points.size()-1 を超える場合は点数不足として例外を送出する。
    const unsigned int k_min = std::max(m, 2 * r);
    if (static_cast<unsigned int>(points.size()) < k_min + 1) {
        throw std::invalid_argument(
            "ApproximateWithNurbs: degree と拘束数に対して入力点数が不足しています。");
    }
    const unsigned int k_init = std::min(
        EstimateK(points, r, m, options.angle_per_segment),
        static_cast<unsigned int>(points.size()) - 1);

    // 反復ループ
    std::vector<double> knots;
    Matrix3Xd ctrl;
    RunApproxLoop(points, t_bar, D, m, k_init, r, options,
                  knots, ctrl);

    // 重みは省略 (全1.0の多項式形式)
    return MakeRationalBSplineCurve(
        m, ctrl, knots, {}, std::array<double, 2>{0.0, 1.0});
}

std::shared_ptr<RationalBSplineCurve> ApproximateWithNurbs(
        const ICurve& curve,
        std::optional<std::array<double, 2>> param_range,
        const NurbsApproxOptions& options) {
    // パラメータ範囲の解決
    const auto range =
        param_range.value_or(curve.GetParameterRange());
    if (!std::isfinite(range[0]) || !std::isfinite(range[1])
            || range[0] >= range[1]) {
        throw std::invalid_argument(
            "ApproximateWithNurbs: パラメータ範囲が無効です。");
    }

    // 正規化
    const NormalizedCurve F = NormalizeParam(curve, range);
    const unsigned int m = options.degree;
    // 利用可能な微分階数に応じて r を決定する（0→1, 1→2, 2→3）
    // ただし r > m の場合、端点付近のノット挿入時に固定制御点が連鎖変化して
    // 収束を阻害するため、r を次数 m でクランプする
    const unsigned int r = std::min(
        static_cast<unsigned int>(1 + F.max_deriv_order), m);

    // 前処理: ||F''||_∞ の粗推定とサンプル数の決定
    const double max_d2 = CoarseSampleMaxSecondDeriv(F);
    const auto coarse   = SampleCurve(F, 20u);
    // max_control_points の上限を超えないようにクランプする
    const unsigned int k_cap =
        options.max_control_points > 0u
        ? options.max_control_points - 1u : 0u;
    const unsigned int k_init = std::min(
        EstimateK(coarse, r, m, options.angle_per_segment),
        k_cap);

    // 精細サンプリングとコード長パラメータ化
    const unsigned int L =
        ComputeL(k_init, r, max_d2, options.tolerance);
    const auto samples = SampleCurve(F, L);
    const auto t_bar   = ChordLengthParams(samples);

    // 端点微分値の計算
    const EndpointDerivatives D =
        ComputeEndpointDerivatives(F, t_bar);

    // 反復ループ
    std::vector<double> knots;
    Matrix3Xd ctrl;
    RunApproxLoop(samples, t_bar, D, m, k_init, r, options,
                  knots, ctrl);

    // 重みは省略 (全1.0の多項式形式)
    return MakeRationalBSplineCurve(
        m, ctrl, knots, {}, std::array<double, 2>{0.0, 1.0});
}

NurbsInterpolation InterpolateWithNurbs(
        const std::vector<Vector3d>& points,
        const std::vector<std::optional<Vector3d>>& tangents,
        const NurbsInterpOptions& options) {
    ValidateInterpOptions(options);
    if (!tangents.empty() && tangents.size() != points.size()) {
        throw std::invalid_argument(
            "InterpolateWithNurbs: tangents must be empty"
            " or have the same size as points.");
    }
    for (const auto& p : points) {
        if (!p.allFinite()) {
            throw std::invalid_argument(
                "InterpolateWithNurbs: points must be finite.");
        }
    }

    const auto distinct =
        MergeDuplicatePoints(points, options.duplicate_tolerance);
    const auto& pts = distinct.points;
    if (pts.size() < 2) {
        throw std::invalid_argument(
            "InterpolateWithNurbs: at least 2 distinct points are required.");
    }

    // 弦の方向と長さ、角点を求めて各点の接線を決める
    std::vector<Vector3d> dirs(pts.size() - 1);
    std::vector<double> lengths(pts.size() - 1);
    for (std::size_t k = 0; k + 1 < pts.size(); ++k) {
        const Vector3d d = pts[k + 1] - pts[k];
        lengths[k] = d.norm();
        dirs[k] = d / lengths[k];
    }
    const auto corners = DetectCorners(dirs, options.corner_angle);
    const auto point_tangents = ApplyGivenTangents(
        EstimateTangents(dirs, lengths, corners), tangents, distinct,
        dirs, corners, options.tangent_tolerance);

    const auto spline =
        BuildHermiteBSpline(pts, lengths, point_tangents, corners);
    NurbsInterpolation result;
    result.curve = MakeRationalBSplineCurve(
        3, spline.control_points, spline.knots, {},
        std::array<double, 2>{0.0, 1.0});
    result.parameters.reserve(points.size());
    for (const auto k : distinct.index_of_input) {
        result.parameters.push_back(spline.node_params[k]);
    }
    return result;
}

}  // namespace igesio::entities
