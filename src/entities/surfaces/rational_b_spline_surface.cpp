/**
 * @file entities/surfaces/rational_b_spline_surface.cpp
 * @brief Rational B-Spline Surface (Type 126): 有理Bスプライン曲面エンティティの定義
 * @author Yayoi Habami
 * @date 2025-09-25
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/surfaces/rational_b_spline_surface.h"

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "igesio/numerics/core/tolerance.h"
#include "igesio/numerics/core/combinatorics.h"
#include "./../curves/nurbs_basis_function.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using RationalBSplineSurface = i_ent::RationalBSplineSurface;
using Vector3d = igesio::Vector3d;
using Matrix3Xd = igesio::Matrix3Xd;
using MatrixXd = igesio::MatrixXd;

/// @brief u, vの各方向における基底関数を計算する
/// @param t パラメータ値 (uまたはv)
/// @param is_u u方向に対して計算する場合はtrue、v方向に対して計算する場合はfalse
/// @param num_derivatives 計算する導関数の数 (0なら基底関数のみ)
/// @param surface RationalBSplineSurfaceオブジェクト
/// @return 基底関数とその導関数の情報; tが定義域外の場合はstd::nullopt
std::optional<i_ent::BasisFunctionResult>
TryComputeBasisFunctions(const double t, const bool is_u,
                         const int num_derivatives,
                         const RationalBSplineSurface& surface) {
    if (!surface.ValidatePD().is_valid) return std::nullopt;

    if (is_u) {
        // u方向の基底関数を計算
        return i_ent::TryComputeBasisFunctions(
            t, num_derivatives, surface.Degrees().first, surface.UKnots(),
            surface.GetURange());
    } else {
        // v方向の基底関数を計算
        return i_ent::TryComputeBasisFunctions(
            t, num_derivatives, surface.Degrees().second, surface.VKnots(),
            surface.GetVRange());
    }
}

/// @brief 全重みが等しい (polynomial形式; PROP3) かを判定する
/// @param weights 重み行列 ((K1+1)×(K2+1))
/// @return 全要素がW(0,0)と近似一致する場合はtrue
bool IsPolynomialWeights(const MatrixXd& weights) {
    for (int i = 0; i < static_cast<int>(weights.rows()); ++i) {
        for (int j = 0; j < static_cast<int>(weights.cols()); ++j) {
            if (!i_num::IsApproxEqual(weights(i, j), weights(0, 0))) {
                return false;
            }
        }
    }
    return true;
}

/// @brief 曲面が指定方向に閉じているか (PROP1/PROP2) を判定する
/// @param surface 判定対象の曲面
/// @param is_u u方向 (PROP1) を判定する場合はtrue、v方向 (PROP2) はfalse
/// @return 閉じている場合はtrue
/// @note クランプかつパラメータ範囲がノット定義域全体の場合は境界制御点の
///       一致と重みの比例で判定し、それ以外は両端での複数サンプル評価で
///       判定する (情報的フラグのため後者は厳密判定ではない)
bool ComputeClosedness(const RationalBSplineSurface& surface, const bool is_u) {
    const auto [k1p1, k2p1] = surface.NumControlPoints();
    if (k1p1 == 0 || k2p1 == 0) return false;

    const auto degrees = surface.Degrees();
    const unsigned int degree = is_u ? degrees.first : degrees.second;
    const auto& knots = is_u ? surface.UKnots() : surface.VKnots();
    const auto range = surface.GetParameterRange();
    // 判定方向のパラメータ範囲 {U(0), U(1)} または {V(0), V(1)}
    const double t0 = is_u ? range[0] : range[2];
    const double t1 = is_u ? range[1] : range[3];

    // クランプ判定: 両端degree+1個のノットが等しいか
    bool is_clamped = true;
    for (unsigned int i = 1; i <= degree; ++i) {
        if (!i_num::IsApproxEqual(knots[i], knots[0]) ||
            !i_num::IsApproxEqual(knots[knots.size() - 1 - i], knots.back())) {
            is_clamped = false;
            break;
        }
    }
    // パラメータ範囲がノット定義域 [S(0), S(N)] 全体か
    const bool is_full_range =
        i_num::IsApproxEqual(t0, knots[degree]) &&
        i_num::IsApproxEqual(t1, knots[knots.size() - degree - 1]);

    if (is_clamped && is_full_range) {
        // 境界制御点の一致と重みの比例で判定する: 例えばu方向の場合、
        // P(0,j) = P(K1,j) かつ W(K1,j) = c・W(0,j) (cはjによらず一定)
        // ならば境界曲線 S(U(0),v) と S(U(1),v) は厳密に一致する
        const unsigned int last = (is_u ? k1p1 : k2p1) - 1;
        const unsigned int n_other = is_u ? k2p1 : k1p1;
        const auto& weights = surface.Weights();
        const double ratio = is_u ? weights(last, 0) / weights(0, 0)
                                  : weights(0, last) / weights(0, 0);
        for (unsigned int idx = 0; idx < n_other; ++idx) {
            const Vector3d first_cp = is_u ? surface.ControlPointAt(0, idx)
                                           : surface.ControlPointAt(idx, 0);
            const Vector3d last_cp = is_u ? surface.ControlPointAt(last, idx)
                                          : surface.ControlPointAt(idx, last);
            if (!i_num::IsApproxEqual(first_cp, last_cp)) return false;
            const double w_first = is_u ? weights(0, idx) : weights(idx, 0);
            const double w_last = is_u ? weights(last, idx) : weights(idx, last);
            if (!i_num::IsApproxEqual(w_last, ratio * w_first)) return false;
        }
        return true;
    }

    // 非クランプ等の場合: 両端 t0, t1 における曲面上の点を、
    // 直交方向の複数サンプルで評価して比較する
    constexpr int kNumSamples = 5;
    const double other0 = is_u ? range[2] : range[0];
    const double other1 = is_u ? range[3] : range[1];
    for (int i = 0; i < kNumSamples; ++i) {
        const double s = other0 + (other1 - other0) * i / (kNumSamples - 1);
        auto p0 = is_u ? surface.TryGetDefinedDerivatives(t0, s, 0)
                       : surface.TryGetDefinedDerivatives(s, t0, 0);
        auto p1 = is_u ? surface.TryGetDefinedDerivatives(t1, s, 0)
                       : surface.TryGetDefinedDerivatives(s, t1, 0);
        if (!p0 || !p1) return false;
        if (!i_num::IsApproxEqual((*p0)(0, 0), (*p1)(0, 0))) return false;
    }
    return true;
}

/// @brief 制御点グリッドの矩形性を検証し、フラット行列に変換する
/// @param grid 制御点グリッド ([i][j] = P(i,j))
/// @return 変換後の制御点行列 (3 × (K1+1)(K2+1); col = i*(K2+1)+j)
/// @throw igesio::EntityValueError グリッドが空、または矩形でない場合
Matrix3Xd FlattenControlPoints(
        const std::vector<std::vector<Vector3d>>& grid) {
    if (grid.empty() || grid[0].empty()) {
        throw igesio::EntityValueError("Control point grid cannot be empty.");
    }
    const int n_u = static_cast<int>(grid.size());
    const int n_v = static_cast<int>(grid[0].size());
    Matrix3Xd cp(3, n_u * n_v);
    for (int i = 0; i < n_u; ++i) {
        if (static_cast<int>(grid[i].size()) != n_v) {
            throw igesio::EntityValueError(
                    "Control point grid must be rectangular: row 0 has " +
                    std::to_string(n_v) + " points, but row " +
                    std::to_string(i) + " has " +
                    std::to_string(grid[i].size()) + ".");
        }
        for (int j = 0; j < n_v; ++j) {
            cp.col(i * n_v + j) = grid[i][j];
        }
    }
    return cp;
}

/// @brief 重みグリッドを検証し、行列に変換する
/// @param grid 重みグリッド ([i][j] = W(i,j)); 空の場合は全て1.0として補完
/// @param n_u 期待するu方向の重み数 (K1+1)
/// @param n_v 期待するv方向の重み数 (K2+1)
/// @return 変換後の重み行列 ((K1+1)×(K2+1))
/// @throw igesio::EntityValueError グリッドの寸法が期待と異なる場合
MatrixXd FlattenWeights(const std::vector<std::vector<double>>& grid,
                        const int n_u, const int n_v) {
    MatrixXd weights(n_u, n_v);
    if (grid.empty()) {
        // 空の場合は全て1.0 (polynomial形式) として補完する
        for (int i = 0; i < n_u; ++i) {
            for (int j = 0; j < n_v; ++j) weights(i, j) = 1.0;
        }
        return weights;
    }
    if (static_cast<int>(grid.size()) != n_u) {
        throw igesio::EntityValueError(
                "Invalid number of weight rows: expected " +
                std::to_string(n_u) + ", but got " +
                std::to_string(grid.size()) + ".");
    }
    for (int i = 0; i < n_u; ++i) {
        if (static_cast<int>(grid[i].size()) != n_v) {
            throw igesio::EntityValueError(
                    "Invalid number of weights in row " + std::to_string(i) +
                    ": expected " + std::to_string(n_v) + ", but got " +
                    std::to_string(grid[i].size()) + ".");
        }
        // 正値の検証はValidateNurbsSurfaceDataで行う
        for (int j = 0; j < n_v; ++j) weights(i, j) = grid[i][j];
    }
    return weights;
}

/// @brief NURBS曲面構造パラメータの整合性を検証する
/// @param m1 u方向の次数
/// @param m2 v方向の次数
/// @param u_knots ノットベクトルS
/// @param v_knots ノットベクトルT
/// @param weights 重み行列 (寸法 (K1+1)×(K2+1) から制御点数を決定する)
/// @param cp 制御点行列 (3 × (K1+1)(K2+1))
/// @throw igesio::EntityValueError 整合性が取れない場合
void ValidateNurbsSurfaceData(
        const unsigned int m1, const unsigned int m2,
        const std::vector<double>& u_knots,
        const std::vector<double>& v_knots,
        const MatrixXd& weights,
        const Matrix3Xd& cp) {
    if (m1 == 0 || m2 == 0) {
        throw igesio::EntityValueError(
                "Degrees M1 and M2 cannot be zero, but got M1 = " +
                std::to_string(m1) + ", M2 = " + std::to_string(m2) + ".");
    }
    const auto n_u = static_cast<size_t>(weights.rows());
    const auto n_v = static_cast<size_t>(weights.cols());
    if (n_u < m1 + 1 || n_v < m2 + 1) {
        throw igesio::EntityValueError(
                "Number of control points (" + std::to_string(n_u) + ", " +
                std::to_string(n_v) + ") must be at least degree + 1 (" +
                std::to_string(m1 + 1) + ", " + std::to_string(m2 + 1) + ").");
    }
    // ノット数はK+M+2 (= 制御点数 + 次数 + 1)
    if (u_knots.size() != n_u + m1 + 1) {
        throw igesio::EntityValueError(
                "Invalid number of u knots: expected " +
                std::to_string(n_u + m1 + 1) + ", but got " +
                std::to_string(u_knots.size()) + ".");
    }
    if (v_knots.size() != n_v + m2 + 1) {
        throw igesio::EntityValueError(
                "Invalid number of v knots: expected " +
                std::to_string(n_v + m2 + 1) + ", but got " +
                std::to_string(v_knots.size()) + ".");
    }
    if (!std::is_sorted(u_knots.begin(), u_knots.end())) {
        throw igesio::EntityValueError("U knot vector must be non-decreasing.");
    }
    if (!std::is_sorted(v_knots.begin(), v_knots.end())) {
        throw igesio::EntityValueError("V knot vector must be non-decreasing.");
    }
    for (int i = 0; i < static_cast<int>(n_u); ++i) {
        for (int j = 0; j < static_cast<int>(n_v); ++j) {
            if (weights(i, j) <= 0.0) {
                throw igesio::EntityValueError(
                        "Weights must be positive real numbers, but got W(" +
                        std::to_string(i) + ", " + std::to_string(j) + ") = " +
                        std::to_string(weights(i, j)) + ".");
            }
        }
    }
    if (static_cast<size_t>(cp.cols()) != n_u * n_v) {
        throw igesio::EntityValueError(
                "Number of control points must be (K1 + 1) * (K2 + 1) = " +
                std::to_string(n_u * n_v) + ", but got " +
                std::to_string(cp.cols()) + ".");
    }
}

/// @brief パラメータ範囲を補完・検証する
/// @param range パラメータ範囲; 省略時はノット定義域全体
/// @param u_knots ノットベクトルS (検証済みであること)
/// @param v_knots ノットベクトルT (検証済みであること)
/// @param m1 u方向の次数
/// @param m2 v方向の次数
/// @return 補完後のパラメータ範囲 { U(0), U(1), V(0), V(1) }
/// @throw igesio::EntityValueError U(0) >= U(1) または V(0) >= V(1) の場合
std::array<double, 4> ResolveParameterRange(
        const std::optional<std::array<double, 4>>& range,
        const std::vector<double>& u_knots,
        const std::vector<double>& v_knots,
        const unsigned int m1, const unsigned int m2) {
    // S(0)は配列インデックスM1、S(N1)は配列インデックスK1+1 (= サイズ-M1-1)
    const auto resolved = range.value_or(std::array<double, 4>{
            u_knots[m1], u_knots[u_knots.size() - m1 - 1],
            v_knots[m2], v_knots[v_knots.size() - m2 - 1]});
    if (resolved[0] >= resolved[1]) {
        throw igesio::EntityValueError(
                "Invalid parameter range: U(0) must be less than U(1)."
                " Got U(0) = " + std::to_string(resolved[0]) +
                ", U(1) = " + std::to_string(resolved[1]) + ".");
    }
    if (resolved[2] >= resolved[3]) {
        throw igesio::EntityValueError(
                "Invalid parameter range: V(0) must be less than V(1)."
                " Got V(0) = " + std::to_string(resolved[2]) +
                ", V(1) = " + std::to_string(resolved[3]) + ".");
    }
    return resolved;
}

/// @brief クランプ一様ノットベクトルを[0,1]上に生成する
/// @param degree 次数M
/// @param num_cp 制御点数 (K+1; degree+1以上であること)
/// @return 両端を重複度M+1でクランプし、内部ノットを等間隔に配置した
///         ノットベクトル (サイズK+M+2)
std::vector<double> MakeClampedUniformKnots(
        const unsigned int degree, const size_t num_cp) {
    const size_t num_interior = num_cp - degree - 1;
    std::vector<double> knots;
    knots.reserve(num_cp + degree + 1);
    knots.insert(knots.end(), degree + 1, 0.0);
    for (size_t i = 1; i <= num_interior; ++i) {
        knots.push_back(static_cast<double>(i) / (num_interior + 1));
    }
    knots.insert(knots.end(), degree + 1, 1.0);
    return knots;
}

/// @brief NURBS曲面パラメータからIGESParameterVectorを構築する
/// @param k1 u方向の制御点の最大インデックス
/// @param k2 v方向の制御点の最大インデックス
/// @param m1 u方向の次数
/// @param m2 v方向の次数
/// @param u_knots ノットベクトルS (サイズk1+m1+2)
/// @param v_knots ノットベクトルT (サイズk2+m2+2)
/// @param weights 重み ((k1+1)×(k2+1))
/// @param cp 制御点行列 (3 × (k1+1)(k2+1); col = i*(k2+1)+j)
/// @param param_range パラメータ範囲 { U(0), U(1), V(0), V(1) }
/// @param is_u_periodic PROP4: u方向に周期的か
/// @param is_v_periodic PROP5: v方向に周期的か
/// @return 構築されたIGESParameterVector
/// @throw igesio::EntityValueError 各データの寸法が整合しない場合
/// @note PROP1/PROP2 (閉フラグ) は仮値0を設定する. 構築後に
///       UpdateClosedness()で再計算すること
igesio::IGESParameterVector BuildNurbsSurfaceParamVector(
        const unsigned int k1, const unsigned int k2,
        const unsigned int m1, const unsigned int m2,
        const std::vector<double>& u_knots,
        const std::vector<double>& v_knots,
        const MatrixXd& weights,
        const Matrix3Xd& cp,
        const std::array<double, 4>& param_range,
        const bool is_u_periodic, const bool is_v_periodic) {
    // 寸法が整合しない場合、PDパラメータの解釈位置がずれて後続データが
    // 破損するため、ここで検証する
    if (u_knots.size() != k1 + m1 + 2 || v_knots.size() != k2 + m2 + 2 ||
        static_cast<unsigned int>(weights.rows()) != k1 + 1 ||
        static_cast<unsigned int>(weights.cols()) != k2 + 1 ||
        static_cast<unsigned int>(cp.cols()) != (k1 + 1) * (k2 + 1)) {
        throw igesio::EntityValueError(
                "Inconsistent NURBS surface data sizes for K1 = " +
                std::to_string(k1) + ", K2 = " + std::to_string(k2) +
                ", M1 = " + std::to_string(m1) + ", M2 = " +
                std::to_string(m2) + ".");
    }

    igesio::IGESParameterVector params;
    params.push_back(static_cast<int>(k1));
    params.push_back(static_cast<int>(k2));
    params.push_back(static_cast<int>(m1));
    params.push_back(static_cast<int>(m2));
    // PROP1/PROP2 (閉フラグ): 仮値0 (UpdateClosednessで再計算する)
    params.push_back(0);
    params.push_back(0);
    // PROP3 (polynomial): 全重みが等しいかで判定
    params.push_back(IsPolynomialWeights(weights) ? 1 : 0);
    params.push_back(is_u_periodic ? 1 : 0);
    params.push_back(is_v_periodic ? 1 : 0);

    for (const auto& s : u_knots) params.push_back(s);
    for (const auto& t : v_knots) params.push_back(t);

    // 重みと制御点はIGESのストリーム順 (第1添字i (u方向) が最速で変化)
    // で出力する. 制御点の格納規約は col = i * (k2 + 1) + j
    const int n_i = static_cast<int>(k1) + 1;
    const int n_j = static_cast<int>(k2) + 1;
    for (int j = 0; j < n_j; ++j) {
        for (int i = 0; i < n_i; ++i) params.push_back(weights(i, j));
    }
    for (int j = 0; j < n_j; ++j) {
        for (int i = 0; i < n_i; ++i) {
            const int col = i * n_j + j;
            params.push_back(cp(0, col));
            params.push_back(cp(1, col));
            params.push_back(cp(2, col));
        }
    }

    params.push_back(param_range[0]);
    params.push_back(param_range[1]);
    params.push_back(param_range[2]);
    params.push_back(param_range[3]);
    return params;
}

}  // namespace



/**
 * コンストラクタ
 */

RationalBSplineSurface::RationalBSplineSurface(
        const RawEntityDE& de_record, const IGESParameterVector& parameters,
        const pointer2ID& de2id, const ObjectID& iges_id)
        : EntityBase(de_record, parameters, de2id, iges_id) {
    InitializePD(de2id);
}

RationalBSplineSurface::RationalBSplineSurface(
        const IGESParameterVector& parameters)
        : RationalBSplineSurface(
            RawEntityDE::ByDefault(i_ent::EntityType::kRationalBSplineSurface),
            parameters, {}, IDGenerator::UnsetID()) {}

RationalBSplineSurface::RationalBSplineSurface(
        const unsigned int k1, const unsigned int k2,
        const unsigned int m1, const unsigned int m2,
        const std::vector<double>& u_knots,
        const std::vector<double>& v_knots,
        const MatrixXd& weights,
        const Matrix3Xd& control_points,
        const std::array<double, 4>& parameter_range,
        const bool is_u_periodic, const bool is_v_periodic)
        : RationalBSplineSurface(
            BuildNurbsSurfaceParamVector(
                k1, k2, m1, m2, u_knots, v_knots, weights, control_points,
                parameter_range, is_u_periodic, is_v_periodic)) {
    // ビルダーではPROP1/PROP2は仮値のため、幾何から再計算する
    UpdateClosedness();
}

i_ent::RationalBSplineSurfaceType RationalBSplineSurface::GetSurfaceType() const {
    return static_cast<RationalBSplineSurfaceType>(form_number_);
}



/**
 * EntityBase implementation
 */

igesio::IGESParameterVector RationalBSplineSurface::GetMainPDParameters() const {
    IGESParameterVector params;

    // 制御点の数 (K1 + 1, K2 + 1)
    auto [k1p1, k2p1] = NumControlPoints();
    params.push_back(static_cast<int>(k1p1) - 1);
    params.push_back(static_cast<int>(k2p1) - 1);

    // 曲面の次数 (M1, M2)
    params.push_back(static_cast<int>(degrees_.first));
    params.push_back(static_cast<int>(degrees_.second));

    // PROP1～5
    params.push_back(is_u_closed_ ? 1 : 0);
    params.push_back(is_v_closed_ ? 1 : 0);
    params.push_back(is_polynomial_ ? 1 : 0);
    params.push_back(is_u_periodic_ ? 1 : 0);
    params.push_back(is_v_periodic_ ? 1 : 0);

    // ノットベクトル S (u方向)
    for (const auto& u : u_knots_) params.push_back(u);
    // ノットベクトル T (v方向)
    for (const auto& v : v_knots_) params.push_back(v);

    // 重み W (IGESのストリーム順: 第1添字i (u方向) が最速で変化する)
    for (int j = 0; j < weights_.cols(); ++j) {
        for (int i = 0; i < weights_.rows(); ++i) {
            params.push_back(weights_(i, j));
        }
    }

    // 制御点 P = (x_ij, y_ij, z_ij) (IGESのストリーム順: 第1添字iが最速).
    // 格納規約 col = i * (K2 + 1) + j に従い、iを内側ループで出力する
    const int n_i = static_cast<int>(k1p1);  // K1 + 1
    const int n_j = static_cast<int>(k2p1);  // K2 + 1
    for (int j = 0; j < n_j; ++j) {
        for (int i = 0; i < n_i; ++i) {
            const int col = i * n_j + j;
            params.push_back(control_points_(0, col));
            params.push_back(control_points_(1, col));
            params.push_back(control_points_(2, col));
        }
    }

    // 曲面のパラメータ範囲 (U(0), U(1), V(0), V(1))
    params.push_back(parameter_range_[0]);
    params.push_back(parameter_range_[1]);
    params.push_back(parameter_range_[2]);
    params.push_back(parameter_range_[3]);

    // サイズが同じ場合に限り、元のフォーマット情報を適用
    if (params.size() == pd_parameters_.size()) {
        for (size_t i = 0; i < pd_parameters_.size(); ++i) {
            try {
                params.set_format(i, pd_parameters_.get_format(i));
            } catch (const std::invalid_argument&) {
                // フォーマットが不正な場合は無視
            }
        }
    }
    return params;
}

size_t RationalBSplineSurface::SetMainPDParameters(const pointer2ID& de2id) {
    auto& pd = pd_parameters_;
    // パラメータの数を確認
    // 制御点数によらず、K1～2, M1～2, PROP1～5, U(0), U(1), V(0), V(1) の13個は存在
    if (pd.size() < 13) {
        throw igesio::EntityParameterError("Insufficient number of parameters "
                "for Rational B-Spline Surface entity (Type 128). "
                "(expected at least 13, got " + std::to_string(pd.size()) + ")");
    }

    // 制御点の数 (K1 + 1, K2 + 1)
    const int k1 = pd.access_as<int>(0);
    const int k2 = pd.access_as<int>(1);
    if (k1 < 0 || k2 < 0) {
        throw igesio::EntityValueError("Invalid number of control points "
                "for Rational B-Spline Surface entity. (K1 and K2 must be "
                "non-negative, but got K1 = " + std::to_string(k1) +
                ", K2 = " + std::to_string(k2) + ")");
    }

    // 曲面の次数 (M1, M2)
    const int m1 = static_cast<unsigned int>(pd.access_as<int>(2));
    const int m2 = static_cast<unsigned int>(pd.access_as<int>(3));
    if (m1 == 0 || m2 == 0) {
        throw igesio::EntityValueError("Invalid degree for Rational B-Spline "
                "Surface entity. (M1 and M2 must be positive, but got M1 = " +
                std::to_string(m1) + ", M2 = " + std::to_string(m2) + ")");
    }
    degrees_ = {m1, m2};

    // PROP1～5
    is_u_closed_ = pd.access_as<bool>(4) != 0;
    is_v_closed_ = pd.access_as<bool>(5) != 0;
    is_polynomial_ = pd.access_as<bool>(6) != 0;
    is_u_periodic_ = pd.access_as<bool>(7) != 0;
    is_v_periodic_ = pd.access_as<bool>(8) != 0;

    // 必要なパラメータの総数を計算
    const size_t num_knots_u = k1 + m1 + 2;  // S(-M1), ..., S(1 + K1)
    const size_t num_knots_v = k2 + m2 + 2;  // T(-M2), ..., T(1 + K2)
    const size_t num_weights = (k1 + 1) * (k2 + 1);     // W(0,0), ..., W(K1,K2)
    const size_t num_control_points = 3 * num_weights;  // P(0,0), ..., P(K1,K2)
    // 総パラメータ数は上記に U(0), U(1), V(0), V(1) の4個を加えたもの
    const size_t expected_size =
        9 + num_knots_u + num_knots_v + num_weights + num_control_points + 4;
    if (pd.size() < expected_size) {
        throw igesio::EntityParameterError("Insufficient number of parameters "
                "for Rational B-Spline Surface entity (Type 128). "
                "(expected at least " + std::to_string(expected_size) +
                ", got " + std::to_string(pd.size()) + ")");
    }
    size_t index = 9;

    // ノットベクトル S (u方向)
    u_knots_.clear();
    u_knots_.reserve(num_knots_u);
    for (size_t i = 0; i < num_knots_u; ++i) {
        u_knots_.push_back(pd.access_as<double>(index++));
    }
    // ノットベクトル T (v方向)
    v_knots_.clear();
    v_knots_.reserve(num_knots_v);
    for (size_t i = 0; i < num_knots_v; ++i) {
        v_knots_.push_back(pd.access_as<double>(index++));
    }

    // 重み W
    // IGESのストリームは第1添字i (u方向) が最速で変化する
    // (W(0,0), W(1,0), ..., W(K1,0), W(0,1), ...) ため、iを内側ループとする
    weights_ = MatrixXd(k1 + 1, k2 + 1);
    for (int j = 0; j <= k2; ++j) {
        for (int i = 0; i <= k1; ++i) {
            weights_(i, j) = pd.access_as<double>(index++);
        }
    }

    // 制御点 P
    // 重みと同様、IGESのストリームは第1添字i (u方向) が最速で変化する
    // (P(0,0), P(1,0), ..., P(K1,0), P(0,1), ...) ため、iを内側ループとする.
    // 格納規約 col = i * (k2 + 1) + j は維持する
    control_points_ = Matrix3Xd(3, (k1 + 1) * (k2 + 1));
    for (int j = 0; j <= k2; ++j) {
        for (int i = 0; i <= k1; ++i) {
            auto col = i * (k2 + 1) + j;
            control_points_(0, col) = pd.access_as<double>(index++);
            control_points_(1, col) = pd.access_as<double>(index++);
            control_points_(2, col) = pd.access_as<double>(index++);
        }
    }

    // パラメータ範囲 U(0), U(1), V(0), V(1)
    parameter_range_[0] = pd.access_as<double>(index++);
    parameter_range_[1] = pd.access_as<double>(index++);
    parameter_range_[2] = pd.access_as<double>(index++);
    parameter_range_[3] = pd.access_as<double>(index++);

    return index;
}

igesio::ValidationResult RationalBSplineSurface::ValidatePD() const {
    std::vector<ValidationError> errors;

    // 次数 M1, M2 の検証
    auto [m1, m2] = degrees_;
    if (m1 == 0 || m2 == 0) {
        errors.emplace_back("Degree M1 and M2 cannot be zero, but got M1 = "
            + std::to_string(m1) + ", M2 = " + std::to_string(m2));
    }

    // 制御点数 K1, K2 の検証
    auto [k1p1, k2p1] = NumControlPoints();
    auto k1 = k1p1 - 1, k2 = k2p1 - 1;
    if (k1 < 0 || k2 < 0) {
        errors.emplace_back("Number of control points K1 and K2 must be "
            "non-negative, but got K1 = " + std::to_string(k1) +
            ", K2 = " + std::to_string(k2));
    }

    // ノットベクトルの検証
    if (u_knots_.size() != k1 + m1 + 2) {
        errors.emplace_back("Size of u knot vector must be K1 + M1 + 2 = "
            + std::to_string(k1 + m1 + 2) + ", but got "
            + std::to_string(u_knots_.size()));
    } else {
        // 非減少列であること
        for (size_t i = 1; i < u_knots_.size(); ++i) {
            if (u_knots_[i] < u_knots_[i - 1] - i_num::kParameterTolerance) {
                errors.emplace_back("U knot vector must be non-decreasing, "
                    "but got decrease at index " + std::to_string(i) +
                    ": " + std::to_string(u_knots_[i - 1]) + " > "
                    + std::to_string(u_knots_[i]));
                break;
            }
        }
        // S(0) <= U(0) < U(1) <= S(N1); See Section B.6
        // CAD出力は U(0)/U(1) がノット域を僅かに外れることがある (曲線Pと同様)。
        // 評価側 TryComputeBasisFunctions は t を域内へ丸めるため描画可能。
        // よって幾何的品質の指摘 (kWarning) とし描画はブロックしない。
        auto s0 = u_knots_[m1], sn = u_knots_[u_knots_.size() - m1 - 1];
        if (s0 > parameter_range_[0] || parameter_range_[1] > sn) {
            errors.emplace_back("Knots S(0), S(N1) must satisfy "
                "S(0) <= U(0) < U(1) <= S(N1), but got S(0) = "
                + std::to_string(s0) + ", U(0) = "
                + std::to_string(parameter_range_[0]) + ", U(1) = "
                + std::to_string(parameter_range_[1]) + ", S(N1) = "
                + std::to_string(sn), igesio::ValidationSeverity::kWarning);
        }
    }
    if (v_knots_.size() != k2 + m2 + 2) {
        errors.emplace_back("Size of v knot vector must be K2 + M2 + 2 = "
            + std::to_string(k2 + m2 + 2) + ", but got "
            + std::to_string(v_knots_.size()));
    } else {
        // 非減少列であること
        for (size_t i = 1; i < v_knots_.size(); ++i) {
            if (v_knots_[i] < v_knots_[i - 1] - i_num::kParameterTolerance) {
                errors.emplace_back("V knot vector must be non-decreasing, "
                    "but got decrease at index " + std::to_string(i) +
                    ": " + std::to_string(v_knots_[i - 1]) + " > "
                    + std::to_string(v_knots_[i]));
                break;
            }
        }
        // T(0) <= V(0) < V(1) <= T(N2); (S(0)/U と同様にkWarning)
        auto t0 = v_knots_[m2], tn = v_knots_[v_knots_.size() - m2 - 1];
        if (t0 > parameter_range_[2] || parameter_range_[3] > tn) {
            errors.emplace_back("Knots T(0), T(N2) must satisfy "
                "T(0) <= V(0) < V(1) <= T(N2), but got T(0) = "
                + std::to_string(t0) + ", V(0) = "
                + std::to_string(parameter_range_[2]) + ", V(1) = "
                + std::to_string(parameter_range_[3]) + ", T(N2) = "
                + std::to_string(tn), igesio::ValidationSeverity::kWarning);
        }
    }

    // 重みの検証
    if (weights_.size() != (k1 + 1) * (k2 + 1)) {
        errors.emplace_back("Size of weights must be (K1 + 1) * (K2 + 1) = "
            + std::to_string((k1 + 1) * (k2 + 1)) + ", but got "
            + std::to_string(weights_.size()));
    } else {
        // 全て正の値であること
        for (int i = 0; i <= k1; ++i) {
            for (int j = 0; j <= k2; ++j) {
                if (weights_(i, j) <= i_num::kParameterTolerance) {
                    errors.emplace_back("All weights must be positive, "
                        "but got weight W(" + std::to_string(i) + ", "
                        + std::to_string(j) + ") = "
                        + std::to_string(weights_(i, j)));
                    break;
                }
            }
        }
    }
    if (is_polynomial_) {
        // polynomial形式の場合、全ての重みが等しいことを確認
        for (int i = 0; i <= k1; ++i) {
            for (int j = 0; j <= k2; ++j) {
                if (!i_num::IsApproxEqual(weights_(i, j), weights_(0, 0))) {
                    errors.emplace_back("In polynomial form, all weights "
                        "must be equal, but got weight W(" +
                        std::to_string(i) + ", " + std::to_string(j) +
                        ") = " + std::to_string(weights_(i, j)) +
                        " different from W(0, 0) = " +
                        std::to_string(weights_(0, 0)));
                    break;
                }
            }
        }
    }

    // 制御点の検証
    if (control_points_.cols() != (k1 + 1) * (k2 + 1)) {
        errors.emplace_back("Number of control points must be (K1 + 1) * "
            "(K2 + 1) = " + std::to_string((k1 + 1) * (k2 + 1)) + ", but got "
            + std::to_string(control_points_.cols()));
    }
    if (control_points_.size() == 0) {
        errors.emplace_back("Control points cannot be empty.");
    }

    // パラメータ範囲の検証
    if (parameter_range_[0] >= parameter_range_[1]) {
        errors.emplace_back("Parameter range U(0) must be less than U(1), "
            "but got U(0) = " + std::to_string(parameter_range_[0]) +
            ", U(1) = " + std::to_string(parameter_range_[1]));
    }
    if (parameter_range_[2] >= parameter_range_[3]) {
        errors.emplace_back("Parameter range V(0) must be less than V(1), "
            "but got V(0) = " + std::to_string(parameter_range_[2]) +
            ", V(1) = " + std::to_string(parameter_range_[3]));
    }

    return MakeValidationResult(std::move(errors));
}



/**
 * ISurface implementation
 */

std::optional<i_ent::SurfaceDerivatives>
RationalBSplineSurface::TryGetDefinedDerivatives(
        const double u, const double v, const unsigned int order) const {
    // パラメータ範囲チェック
    if (u < parameter_range_[0] || u > parameter_range_[1] ||
        v < parameter_range_[2] || v > parameter_range_[3]) {
        return std::nullopt;
    }

    // 基底関数の計算
    auto basis_u_opt = ::TryComputeBasisFunctions(u, true, order, *this);
    auto basis_v_opt = ::TryComputeBasisFunctions(v, false, order, *this);
    if (!basis_u_opt || !basis_v_opt) return std::nullopt;
    const auto& basis_u = *basis_u_opt;
    const auto& basis_v = *basis_v_opt;

    // u,v方向の次数
    auto [m_u, m_v] = degrees_;

    // u∈[u_k, u_{k+1}], v∈[v_l, v_{l+1}] を満たすノットスパンのインデックス k, l
    const int u_span = basis_u.knot_span;  // k
    const int v_span = basis_v.knot_span;  // l

    // A^(nu, nv) と w^(nu, nv) を計算する
    SurfaceDerivatives numer(order);
    SurfaceDerivatives denom(order);  // xの値が分母に対応
    for (int i = 0; i <= m_u; ++i) {
        // u方向の制御点インデックス p
        const int p = u_span - m_u + i;
        for (int j = 0; j <= m_v; ++j) {
            // v方向の制御点インデックス q
            const int q = v_span - m_v + j;

            // A^(nu, nv) と w^(nu, nv) を 0 <= nu + nv <= order について計算
            for (unsigned int nu = 0; nu <= order; ++nu) {
                // b_p^(nu)(u) 基底関数のnu次導関数
                const double b_u = basis_u.GetDerivatives(nu)[i];
                for (unsigned int nv = 0; nv <= order - nu; ++nv) {
                    // b_q^(nv)(v) 基底関数のnv次導関数
                    const double b_v = basis_v.GetDerivatives(nv)[j];

                    numer(nu, nv) += WeightAt(p, q) * b_u * b_v * ControlPointAt(p, q);
                    denom(nu, nv).x() += WeightAt(p, q) * b_u * b_v;
                }
            }
        }
    }

    // 分母 w^(0,0) がほぼ0の場合は定義されない
    if (i_num::IsApproxZero(denom(0, 0).x())) return std::nullopt;

    // S^(nu, nv) を計算する
    SurfaceDerivatives result(order);
    const double w00 = denom(0, 0).x();

    // k = nu + nv について 0からorderまでループ
    for (unsigned int k = 0; k <= order; ++k) {
        for (unsigned int nu = 0; nu <= k; ++nu) {
            const unsigned int nv = k - nu;

            // S^(nu, nv) の計算
            Vector3d sum_term = Vector3d::Zero();

            // Σ_{i=0...nu, j=0...nv, (i,j)!=(nu,nv)} ...
            for (unsigned int i = 0; i <= nu; ++i) {
                for (unsigned int j = 0; j <= nv; ++j) {
                    // (i, j) == (nu, nv) の場合はスキップ
                    if (i == nu && j == nv) continue;

                    // C(nu, i) * C(nv, j) * S^(i, j) * w^(nu - i, nv - j)
                    sum_term += numerics::BinomialCoefficient<double>(nu, i) *
                                numerics::BinomialCoefficient<double>(nv, j) *
                                result(i, j) *              // S^(i, j)
                                denom(nu - i, nv - j).x();  // w^(nu - i, nv - j)
                }
            }

            // S^(nu, nv) = (A^(nu, nv) - Σ ...) / w^(0,0)
            result(nu, nv) = (numer(nu, nv) - sum_term) / w00;
        }
    }

    return result;
}

i_num::BoundingBox RationalBSplineSurface::GetDefinedBoundingBox() const {
    Vector3d min_pt = Vector3d::Constant(std::numeric_limits<double>::infinity());
    Vector3d max_pt = Vector3d::Constant(-std::numeric_limits<double>::infinity());

    // 制御点を走査して最小・最大座標を更新
    for (int i = 0; i < control_points_.cols(); ++i) {
        Vector3d pt = control_points_.col(i);
        min_pt = min_pt.cwiseMin(pt);
        max_pt = max_pt.cwiseMax(pt);
    }
    return i_num::BoundingBox(min_pt, max_pt);
}



/**
 * 描画用
 */

double RationalBSplineSurface::WeightAt(
        const unsigned int i, const unsigned int j) const {
    if (i >= weights_.rows() || j >= weights_.cols()) {
        throw std::out_of_range("Weight indices are out of range");
    }
    return weights_(i, j);
}

std::pair<unsigned int, unsigned int>
RationalBSplineSurface::NumControlPoints() const {
    if (control_points_.cols() == 0) {
        return {0, 0};
    }
    auto [m1, m2] = degrees_;
    const int k1 = static_cast<int>(u_knots_.size()) - m1 - 2;
    const int k2 = static_cast<int>(v_knots_.size()) - m2 - 2;
    return {k1 + 1, k2 + 1};
}

Vector3d RationalBSplineSurface::ControlPointAt(
        const unsigned int i, const unsigned int j) const {
    if (i >= NumControlPoints().first || j >= NumControlPoints().second) {
        throw std::out_of_range("Control point indices are out of range");
    }
    return control_points_.col(i * (NumControlPoints().second) + j);
}



/**
 * データ変更
 */

void RationalBSplineSurface::SetControlPointAt(
        const unsigned int i, const unsigned int j, const Vector3d& point) {
    const auto [n_u, n_v] = NumControlPoints();
    if (i >= n_u || j >= n_v) {
        throw std::out_of_range("Control point indices are out of range");
    }
    control_points_.col(i * n_v + j) = point;
    UpdateClosedness();
    MarkGeometryModified();
}

void RationalBSplineSurface::SetControlPoints(const Matrix3Xd& control_points) {
    if (control_points.cols() != control_points_.cols()) {
        throw igesio::EntityValueError(
                "Number of control points must remain " +
                std::to_string(control_points_.cols()) + ", but got " +
                std::to_string(control_points.cols()) +
                ". Use SetData to change the structure.");
    }
    control_points_ = control_points;
    UpdateClosedness();
    MarkGeometryModified();
}

void RationalBSplineSurface::SetWeightAt(
        const unsigned int i, const unsigned int j, const double weight) {
    if (i >= weights_.rows() || j >= weights_.cols()) {
        throw std::out_of_range("Weight indices are out of range");
    }
    if (weight <= 0.0) {
        throw igesio::EntityValueError(
                "Weights must be positive real numbers, but got " +
                std::to_string(weight) + ".");
    }
    weights_(i, j) = weight;
    is_polynomial_ = IsPolynomialWeights(weights_);
    UpdateClosedness();
    MarkGeometryModified();
}

void RationalBSplineSurface::SetWeights(const MatrixXd& weights) {
    if (weights.rows() != weights_.rows() ||
        weights.cols() != weights_.cols()) {
        throw igesio::EntityValueError(
                "Shape of weights must remain (" +
                std::to_string(weights_.rows()) + ", " +
                std::to_string(weights_.cols()) + "), but got (" +
                std::to_string(weights.rows()) + ", " +
                std::to_string(weights.cols()) +
                "). Use SetData to change the structure.");
    }
    for (int i = 0; i < static_cast<int>(weights.rows()); ++i) {
        for (int j = 0; j < static_cast<int>(weights.cols()); ++j) {
            if (weights(i, j) <= 0.0) {
                throw igesio::EntityValueError(
                        "Weights must be positive real numbers, but got W(" +
                        std::to_string(i) + ", " + std::to_string(j) + ") = " +
                        std::to_string(weights(i, j)) + ".");
            }
        }
    }
    weights_ = weights;
    is_polynomial_ = IsPolynomialWeights(weights_);
    UpdateClosedness();
    MarkGeometryModified();
}

void RationalBSplineSurface::SetUKnots(const std::vector<double>& knots) {
    if (knots.size() != u_knots_.size()) {
        throw igesio::EntityValueError(
                "Number of u knots must remain " +
                std::to_string(u_knots_.size()) + ", but got " +
                std::to_string(knots.size()) +
                ". Use SetData to change the structure.");
    }
    if (!std::is_sorted(knots.begin(), knots.end())) {
        throw igesio::EntityValueError("U knot vector must be non-decreasing.");
    }
    u_knots_ = knots;
    UpdateClosedness();
    MarkGeometryModified();
}

void RationalBSplineSurface::SetVKnots(const std::vector<double>& knots) {
    if (knots.size() != v_knots_.size()) {
        throw igesio::EntityValueError(
                "Number of v knots must remain " +
                std::to_string(v_knots_.size()) + ", but got " +
                std::to_string(knots.size()) +
                ". Use SetData to change the structure.");
    }
    if (!std::is_sorted(knots.begin(), knots.end())) {
        throw igesio::EntityValueError("V knot vector must be non-decreasing.");
    }
    v_knots_ = knots;
    UpdateClosedness();
    MarkGeometryModified();
}

void RationalBSplineSurface::SetParameterRange(
        const std::array<double, 4>& range) {
    if (range[0] >= range[1]) {
        throw igesio::EntityValueError(
                "Invalid parameter range: U(0) must be less than U(1)."
                " Got U(0) = " + std::to_string(range[0]) +
                ", U(1) = " + std::to_string(range[1]) + ".");
    }
    if (range[2] >= range[3]) {
        throw igesio::EntityValueError(
                "Invalid parameter range: V(0) must be less than V(1)."
                " Got V(0) = " + std::to_string(range[2]) +
                ", V(1) = " + std::to_string(range[3]) + ".");
    }
    parameter_range_ = range;
    // 閉フラグは両端U(0)/U(1) (V(0)/V(1)) における一致で定義されるため再計算
    UpdateClosedness();
    MarkGeometryModified();
}

bool RationalBSplineSurface::SetSurfaceType(
        const RationalBSplineSurfaceType type) {
    // フォーム番号は規格上informationalであり、幾何形状との一致は検証しない
    switch (type) {
        case RationalBSplineSurfaceType::kUndetermined:
        case RationalBSplineSurfaceType::kPlane:
        case RationalBSplineSurfaceType::kRightCircularCylinder:
        case RationalBSplineSurfaceType::kCone:
        case RationalBSplineSurfaceType::kSphere:
        case RationalBSplineSurfaceType::kTorus:
        case RationalBSplineSurfaceType::kSurfaceOfRevolution:
        case RationalBSplineSurfaceType::kTabulatedCylinder:
        case RationalBSplineSurfaceType::kRuledSurface:
        case RationalBSplineSurfaceType::kGeneralQuadricSurface:
            form_number_ = static_cast<int>(type);
            MarkGeometryModified();
            return true;
    }
    return false;  // 無効なタイプ
}

void RationalBSplineSurface::SetData(
        const std::pair<unsigned int, unsigned int> degrees,
        const std::vector<double>& u_knots,
        const std::vector<double>& v_knots,
        const std::vector<std::vector<double>>& weights,
        const std::vector<std::vector<Vector3d>>& control_points,
        const std::optional<std::array<double, 4>>& parameter_range,
        const bool is_u_periodic, const bool is_v_periodic) {
    // 検証をすべて先に行い、失敗時にメンバ変数を変更しない
    const auto cp = FlattenControlPoints(control_points);
    const auto w = FlattenWeights(
            weights, static_cast<int>(control_points.size()),
            static_cast<int>(control_points[0].size()));
    ValidateNurbsSurfaceData(degrees.first, degrees.second,
                             u_knots, v_knots, w, cp);
    const auto range = ResolveParameterRange(
            parameter_range, u_knots, v_knots, degrees.first, degrees.second);

    degrees_ = degrees;
    u_knots_ = u_knots;
    v_knots_ = v_knots;
    weights_ = w;
    control_points_ = cp;
    parameter_range_ = range;
    is_u_periodic_ = is_u_periodic;
    is_v_periodic_ = is_v_periodic;
    is_polynomial_ = IsPolynomialWeights(weights_);
    UpdateClosedness();
    MarkGeometryModified();
}

void RationalBSplineSurface::UpdateClosedness() {
    is_u_closed_ = ComputeClosedness(*this, true);
    is_v_closed_ = ComputeClosedness(*this, false);
}



/**
 * ファクトリ関数
 */

std::shared_ptr<RationalBSplineSurface> i_ent::MakeRationalBSplineSurface(
        const std::pair<unsigned int, unsigned int> degrees,
        const std::vector<std::vector<Vector3d>>& control_points,
        const std::vector<double>& u_knots,
        const std::vector<double>& v_knots,
        const std::vector<std::vector<double>>& weights,
        std::optional<std::array<double, 4>> parameter_range,
        const bool is_u_periodic, const bool is_v_periodic) {
    const auto cp = FlattenControlPoints(control_points);
    const auto n_u = static_cast<int>(control_points.size());
    const auto n_v = static_cast<int>(control_points[0].size());
    const auto w = FlattenWeights(weights, n_u, n_v);
    ValidateNurbsSurfaceData(degrees.first, degrees.second,
                             u_knots, v_knots, w, cp);
    const auto range = ResolveParameterRange(
            parameter_range, u_knots, v_knots, degrees.first, degrees.second);
    return std::make_shared<RationalBSplineSurface>(
            static_cast<unsigned int>(n_u - 1),
            static_cast<unsigned int>(n_v - 1),
            degrees.first, degrees.second, u_knots, v_knots, w, cp, range,
            is_u_periodic, is_v_periodic);
}

std::shared_ptr<RationalBSplineSurface> i_ent::MakeClampedBSplineSurface(
        const std::pair<unsigned int, unsigned int> degrees,
        const std::vector<std::vector<Vector3d>>& control_points,
        const std::vector<std::vector<double>>& weights) {
    if (control_points.empty() || control_points[0].empty()) {
        throw igesio::EntityValueError("Control point grid cannot be empty.");
    }
    // 内部ノット数の計算 (num_cp - degree - 1) がアンダーフローしないよう、
    // 制御点数の不足のみ先に検証する (他の検証は一般形ファクトリに委ねる)
    const size_t n_u = control_points.size();
    const size_t n_v = control_points[0].size();
    if (n_u < degrees.first + 1 || n_v < degrees.second + 1) {
        throw igesio::EntityValueError(
                "Number of control points (" + std::to_string(n_u) + ", " +
                std::to_string(n_v) + ") must be at least degree + 1 (" +
                std::to_string(degrees.first + 1) + ", " +
                std::to_string(degrees.second + 1) + ").");
    }
    // クランプ一様ノット: 両端を重複度M+1とし、内部ノットを等間隔に配置
    const auto u_knots = MakeClampedUniformKnots(degrees.first, n_u);
    const auto v_knots = MakeClampedUniformKnots(degrees.second, n_v);
    return MakeRationalBSplineSurface(degrees, control_points,
                                      u_knots, v_knots, weights);
}

std::shared_ptr<RationalBSplineSurface> i_ent::MakeBezierSurface(
        const std::vector<std::vector<Vector3d>>& control_points,
        const std::vector<std::vector<double>>& weights) {
    if (control_points.empty() || control_points[0].empty()) {
        throw igesio::EntityValueError("Control point grid cannot be empty.");
    }
    const size_t n_u = control_points.size();
    const size_t n_v = control_points[0].size();
    if (n_u < 2 || n_v < 2) {
        throw igesio::EntityValueError(
                "Bezier surface requires at least 2 control points in each "
                "direction, but got (" + std::to_string(n_u) + ", " +
                std::to_string(n_v) + ").");
    }
    // K = M = 制御点数-1: ノットは {0, ..., 0, 1, ..., 1} (各num_cp個)
    std::vector<double> u_knots(n_u, 0.0);
    u_knots.insert(u_knots.end(), n_u, 1.0);
    std::vector<double> v_knots(n_v, 0.0);
    v_knots.insert(v_knots.end(), n_v, 1.0);
    return MakeRationalBSplineSurface(
            {static_cast<unsigned int>(n_u - 1),
             static_cast<unsigned int>(n_v - 1)},
            control_points, u_knots, v_knots, weights);
}
