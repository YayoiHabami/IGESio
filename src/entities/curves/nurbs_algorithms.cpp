/**
 * @file entities/curves/nurbs_algorithms.cpp
 * @brief 任意曲線の NURBS 近似アルゴリズムの実装
 * @author Yayoi Habami
 * @date 2026-04-11
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/entities/curves/nurbs_algorithms.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <numeric>
#include <stdexcept>
#include <vector>

#include <Eigen/Dense>

#include "igesio/numerics/matrix.h"

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
    const double t_min = range[0];
    const double dt    = range[1] - range[0];

    // 利用可能な微分階数を確認する
    int max_order = 0;
    if (curve.TryGetDerivatives(t_min, 2)) {
        max_order = 2;
    } else if (curve.TryGetDerivatives(t_min, 1)) {
        max_order = 1;
    }

    // curve への参照と t_min, dt を値キャプチャしてラムダを構成する
    auto eval = [&curve, t_min, dt](double s, unsigned int n) -> Vector3d {
        const auto d = curve.TryGetDerivatives(t_min + s * dt, n);
        return d ? (*d)[n] : Vector3d::Zero();
    };

    NormalizedCurve F;
    F.max_deriv_order = max_order;
    F.point  = [eval](double s) { return eval(s, 0); };
    F.deriv1 = [eval, dt](double s) { return eval(s, 1) * dt; };
    F.deriv2 = [eval, dt](double s) { return eval(s, 2) * (dt * dt); };
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
/// @throw std::invalid_argument 総弦長がゼロの場合
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
        throw std::invalid_argument(
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

    D.d2_start =
        (F.deriv2(0.0) - sigma0pp * D.d1_start) / (sigma0 * sigma0);
    D.d2_end =
        (F.deriv2(1.0) - sigmaLpp * D.d1_end) / (sigmaL * sigmaL);
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

/// @brief 最小二乗正規方程式 (B̂ᵀB̂)X = B̂ᵀR を Cholesky 分解で解く
/// @param B_hat (L-1)×(k-2r+1) の基底行列
/// @param R     (L-1)×3 の残差行列
/// @return 内部制御点行列 3×(k-2r+1)
Matrix3Xd SolveLeastSquares(
        const MatrixXd& B_hat, const MatrixXd& R) {
    MatrixXd BtB = B_hat.transpose() * B_hat;
    const MatrixXd BtR = B_hat.transpose() * R;

    // 悪条件対策: Tikhonov正則化
    const double lambda = 1e-8 * BtB.norm();
    BtB += lambda * MatrixXd::Identity(BtB.rows(), BtB.cols());

    const Eigen::LLT<MatrixXd> llt(BtB);
    if (llt.info() != Eigen::Success) {
        // フォールバック: LDLT分解
        return BtB.ldlt().solve(BtR).transpose();
    }
    return llt.solve(BtR).transpose();  // 3×(k-2r+1)
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

/// @brief 誤差が閾値を超えるサンプル区間にノットを挿入する
/// @param knots  現在のノットベクトル（変更される）
/// @param errors サンプル毎の近似誤差
/// @param t_bar  コード長パラメータ
/// @param eps    許容誤差
/// @return ノット挿入に成功した場合は true
bool InsertKnot(
        std::vector<double>& knots,
        const std::vector<double>& errors,
        const std::vector<double>& t_bar,
        double eps) {
    // 誤差最大のサンプルを特定する
    const auto it_max = std::max_element(errors.begin(), errors.end());
    if (*it_max <= eps) return false;

    const size_t l_max =
        static_cast<size_t>(it_max - errors.begin());
    const double t = t_bar[l_max];

    // tを含むノットスパン [t_lo, t_hi] を二分探索で特定し、中点を挿入位置とする
    // 中点は既存ノットと必ず異なるため、重複チェックが不要
    const auto it_hi =
        std::upper_bound(knots.begin(), knots.end(), t);
    if (it_hi == knots.begin() || it_hi == knots.end()) return false;

    const double t_hi = *it_hi;
    const double t_lo = *std::prev(it_hi);
    if (t_hi - t_lo < 1e-12) return false;

    knots.insert(it_hi, (t_lo + t_hi) * 0.5);
    return true;
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
        const MatrixXd Rmat =
            ComputeResidual(samples, t_bar, P_fixed, m, k, r, knots);
        const Matrix3Xd X_free = SolveLeastSquares(B_hat, Rmat);
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

    const unsigned int k_final =
        static_cast<unsigned int>(ctrl.cols()) - 1;
    const std::vector<double> weights(k_final + 1, 1.0);
    return std::make_shared<RationalBSplineCurve>(
        k_final, m, knots, weights, ctrl,
        std::array<double, 2>{0.0, 1.0});
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

    const unsigned int k_final =
        static_cast<unsigned int>(ctrl.cols()) - 1;
    const std::vector<double> weights(k_final + 1, 1.0);
    return std::make_shared<RationalBSplineCurve>(
        k_final, m, knots, weights, ctrl,
        std::array<double, 2>{0.0, 1.0});
}

}  // namespace igesio::entities
