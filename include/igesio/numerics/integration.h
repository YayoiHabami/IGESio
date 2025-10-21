/**
 * @file numerics/integration.h
 * @brief 数値積分に関するユーティリティ関数群
 * @author Yayoi Habami
 * @date 2025-10-20
 * @copyright 2025 Yayoi Habami
 * @note 念のため個別手法用の関数（`GaussLegendreIntegrate`など）も公開APIとして提供するが、
 *       基本的には精度・計算コストの面で優位である、`Integrate`関数を使用することを推奨する.
 */
#ifndef IGESIO_NUMERICS_INTEGRATION_H_
#define IGESIO_NUMERICS_INTEGRATION_H_

#include <array>
#include <functional>
#include <limits>

namespace igesio::numerics {

/// @brief ガウス=ルジャンドル求積法による数値積分のサポート点の最大数
constexpr size_t kGaussLegendreIntegrateMaxPoints = 5;

/// @brief 積分方法の列挙型
enum class IntegrateMethod {
    /// @brief ガウス=ルジャンドル求積法
    kGaussLegendre,
};

/// @brief 数値積分の手法とパラメータを指定する構造体
struct IntegrationOptions {
    /// @brief 積分手法（デフォルトはガウス=ルジャンドル求積法）
    IntegrateMethod method = IntegrateMethod::kGaussLegendre;
    /// @brief ガウス点の数（デフォルトは5点、ガウス=ルジャンドル求積法用）
    size_t n_points = kGaussLegendreIntegrateMaxPoints;

    /// @brief 最大再帰深度 (無限ループ防止用)
    size_t max_recursion_depth = 20;

    /// @brief ガウス=ルジャンドル求積法の推奨設定
    /// @param n_points ガウス点の数（デフォルトは最大値）
    /// @param max_recursion_depth 最大再帰深度 (無限ループ防止用)
    /// @return ガウス=ルジャンドル求積法用の`IntegrationOptions`
    static IntegrationOptions GaussLegendre(
            const size_t n_points = kGaussLegendreIntegrateMaxPoints,
            const size_t max_recursion_depth = 20);

    /// @brief 積分手法の確認
    /// @param options 積分オプション
    /// @throw std::invalid_argument サポートされていない積分手法が指定された場合、
    ///        またはパラメータが不正な場合
    void Check() const;
};

/// @brief 計算の許容誤差
/// @note 絶対許容誤差と相対許容誤差の両方を指定する
struct Tolerance {
    /// @brief 絶対許容誤差
    /// @note 基本的にはこちらが優先される. 例えば更新前と後との値の差 diff
    ///       (絶対値)がabs_tol以下であれば収束とみなす.
    double abs_tol = 1e-4;
    /// @brief 相対許容誤差
    /// @note 更新前や後の値が非常に大きい場合（例えば1e14など）には、
    ///       double型の精度限界によりabs_tol以下の差が出ないことがある.
    ///       そのような場合にこちらの相対許容誤差を使用する.
    /// @note 例えば更新前後での値の差 diff (絶対値)が、更新前の値の
    ///       絶対値 * rel_tol以下であれば収束とみなす.
    double rel_tol = 1e-10;

    /// @brief デフォルトコンストラクタ
    Tolerance() : Tolerance(1e-4, 1e-10) {}

    /// @brief コンストラクタ
    /// @param abs_tol_ 絶対許容誤差
    /// @param rel_tol_ 相対許容誤差 (デフォルトは1e-10)
    explicit Tolerance(const double abs_tol_, const double rel_tol_ = 1e-10)
        : abs_tol(abs_tol_), rel_tol(rel_tol_) {}
};

/// @brief ガウス=ルジャンドル求積法による数値積分
/// @param f 被積分関数 f(x) (x ∈ [x_min, x_max])
/// @param range 積分区間の範囲 [x_min, x_max]
/// @param n_intervals 積分区間の分割数;
///        0を指定した場合、常に0を返す (自動推定は行わない).
/// @param n_points ガウス点の数（デフォルトは5点）
/// @return 積分値 ∫[x_min, x_max] f(x) dx
/// @throw std::invalid_argument n_pointsがサポート範囲外の場合
/// @note n_intervalsだけ積分区間 [x_min, x_max] を等分割し、
///       各区間でガウス=ルジャンドル求積法を適用して総和を取る.
/// @note 特に指定がなければ、`Integrate`関数の使用を推奨する.
double GaussLegendreIntegrate(const std::function<double(double)>&,
        const std::array<double, 2>&, const size_t,
        const size_t = kGaussLegendreIntegrateMaxPoints);

/// @brief 数値積分 ∫ f(x) dx を行う
/// @param f 被積分関数 f(x) (x ∈ [x_min, x_max])
/// @param range 積分区間の範囲 [x_min, x_max]
/// @param tolerance 許容誤差
/// @param options 積分オプション
/// @return 積分値 ∫[x_min, x_max] f(x) dx
/// @throw std::invalid_argument サポートされていない積分手法が指定された場合等
/// @note tolerance=1e-4の場合、単位長の1e-4以下の誤差 (1μm) が許容値となる
/// @note 積分区間を自動的に分割し、各区間で指定された手法による数値積分を行う.
///       この際、曲線が複雑な区間は細かく、単純な区間は粗く分割されるよう計算する.
///       このため、`GaussLegendreIntegrate`などで単に`n_intervals`を指定して積分する
///       よりも (同じ計算コストで) 高精度な積分が期待できる.
double Integrate(const std::function<double(double)>&,
                 const std::array<double, 2>&, const Tolerance& = Tolerance(),
                 const IntegrationOptions& = IntegrationOptions::GaussLegendre());

/// @brief 数値積分 ∫∫ f(x,y) dx dy を行う
/// @param f 被積分関数 f(x, y) (x ∈ [x_min, x_max], y ∈ [y_min, y_max])
/// @param range 積分区間のx,y方向の範囲 [x_min, x_max, y_min, y_max]
/// @param tolerance 許容誤差
/// @param options 積分オプション
/// @return 積分値 ∫[y_min, y_max] ∫[x_min, x_max] f(x, y) dx dy
/// @throw std::invalid_argument サポートされていない積分手法が指定された場合等
/// @note tolerance=1e-4の場合、単位長の1e-4以下の誤差 (1μm) が許容値となる
/// @note 積分区間を自動的に分割し、各区間で指定された手法による数値積分を行う.
double Integrate(const std::function<double(double, double)>&,
                 const std::array<double, 4>&, const Tolerance& = Tolerance(),
                 const IntegrationOptions& = IntegrationOptions::GaussLegendre());

}  // namespace igesio::numerics

#endif  // IGESIO_NUMERICS_INTEGRATION_H_
