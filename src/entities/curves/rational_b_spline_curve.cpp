/**
 * @file entities/curves/rational_b_spline_curve.cpp
 * @brief Rational B-Spline Curve (Type 126): 有理Bスプライン曲線エンティティの定義
 * @author Yayoi Habami
 * @date 2025-08-15
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/curves/rational_b_spline_curve.h"

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "igesio/numerics/core/tolerance.h"
#include "igesio/numerics/core/combinatorics.h"
#include "./nurbs_basis_function.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using RationalBSplineCurve = i_ent::RationalBSplineCurve;
using Matrix3Xd = igesio::Matrix3Xd;
using Vector3d = igesio::Vector3d;

/// @brief RationalBSplineCurveに対して、基底関数とその導関数を計算する
/// @param t パラメータ値
/// @param num_derivatives 計算する導関数の数 (0なら基底関数のみ)
/// @param curve RationalBSplineCurveオブジェクト
/// @return 基底関数とその導関数の情報; tが定義域外の場合はstd::nullopt
std::optional<i_ent::BasisFunctionResult>
TryComputeBasisFunctions(const double t, const int num_derivatives,
                         const RationalBSplineCurve& curve) {
    if (!curve.ValidatePD().is_valid) return std::nullopt;

    return i_ent::TryComputeBasisFunctions(
        t, num_derivatives, curve.Degree(), curve.Knots(),
        curve.GetParameterRange());
}

/// @brief NURBS制御点が平面上にあるかを判定し、法線ベクトルを返す
/// @param cp  制御点行列（3 × (k+1)）
/// @param k   制御点の最大インデックス
/// @return 平面の正規化法線ベクトル、または平面でない場合（3点未満・
///         全点が一直線上・全点が同一平面上でない）は std::nullopt
std::optional<Vector3d> ComputePlaneNormal(const Matrix3Xd& cp, int k) {
    if (k < 2) return std::nullopt;

    const Vector3d p0 = cp.col(0);

    // p0から始まる最初の非零ベクトルをv1とする
    Vector3d v1 = Vector3d::Zero();
    for (int i = 1; i <= k; ++i) {
        v1 = cp.col(i) - p0;
        if (v1.norm() > i_num::kGeometryTolerance) break;
    }
    if (v1.norm() <= i_num::kGeometryTolerance) return std::nullopt;
    v1.normalize();

    // v1と線形独立なベクトルを探し、外積から法線を計算する
    std::optional<Vector3d> normal;
    for (int i = 2; i <= k; ++i) {
        const Vector3d vi = cp.col(i) - p0;
        const Vector3d cross = v1.cross(vi);
        if (cross.norm() > i_num::kGeometryTolerance) {
            normal = cross.normalized();
            break;
        }
    }
    if (!normal) return std::nullopt;  // 全点が一直線上

    // 残りの制御点が同一平面上にあるか確認
    for (int i = 1; i <= k; ++i) {
        const double dist = normal->dot(cp.col(i) - p0);
        if (!i_num::IsApproxZero(dist, i_num::kGeometryTolerance)) {
            return std::nullopt;
        }
    }
    return normal;
}

/// @brief NURBS曲線パラメータからIGESParameterVectorを構築する
/// @param k           制御点の最大インデックス
/// @param m           曲線の次数
/// @param knots       ノットベクトル T（サイズ k+m+2）
/// @param weights     重み W（サイズ k+1）
/// @param cp          制御点行列（3 × (k+1)）
/// @param param_range パラメータ範囲 { V(0), V(1) }
/// @param is_periodic 周期的か
/// @return 構築された IGESParameterVector
igesio::IGESParameterVector BuildNurbsParamVector(
        unsigned int k, unsigned int m,
        const std::vector<double>& knots,
        const std::vector<double>& weights,
        const Matrix3Xd& cp,
        const std::array<double, 2>& param_range,
        bool is_periodic) {
    // PROP1/normal_vector: 平面性を判定
    const auto normal = ComputePlaneNormal(cp, static_cast<int>(k));

    igesio::IGESParameterVector params;
    params.push_back(static_cast<int>(k));
    params.push_back(static_cast<int>(m));
    // PROP1: 法線が定まれば平面的
    params.push_back(normal.has_value());
    // PROP2（is_closed）: SetMainPDParametersでは参照されないためfalseを設定
    params.push_back(false);
    // PROP3（is_polynomial）: 同上
    params.push_back(false);
    params.push_back(is_periodic);

    for (const auto& t : knots)   params.push_back(t);
    for (const auto& w : weights) params.push_back(w);
    for (int i = 0; i <= static_cast<int>(k); ++i) {
        params.push_back(cp(0, i));
        params.push_back(cp(1, i));
        params.push_back(cp(2, i));
    }
    params.push_back(param_range[0]);
    params.push_back(param_range[1]);

    // 平面的な場合は法線ベクトルを設定、そうでない場合は零ベクトル
    if (normal) {
        params.push_back(normal->x());
        params.push_back(normal->y());
        params.push_back(normal->z());
    } else {
        params.push_back(0.0);
        params.push_back(0.0);
        params.push_back(0.0);
    }
    return params;
}

/// @brief NURBS構造パラメータの整合性を検証し、重みを補完する
/// @param degree 曲線の次数M
/// @param knots ノットベクトル (サイズK+M+2)
/// @param weights 重み (サイズK+1); 空の場合は全て1.0として補完する
/// @param cp 制御点行列 (3 × (K+1))
/// @return 補完後の重み (サイズK+1)
/// @throw igesio::EntityValueError 整合性が取れない場合
std::vector<double> ValidateNurbsData(
        const unsigned int degree,
        const std::vector<double>& knots,
        const std::vector<double>& weights,
        const Matrix3Xd& cp) {
    if (degree == 0) {
        throw igesio::EntityValueError("Degree M cannot be zero.");
    }
    const auto num_cp = static_cast<size_t>(cp.cols());
    if (num_cp < degree + 1) {
        throw igesio::EntityValueError(
                "Number of control points (" + std::to_string(num_cp) +
                ") must be at least degree + 1 (" +
                std::to_string(degree + 1) + ").");
    }
    // ノット数はK+M+2 (= 制御点数 + 次数 + 1)
    const size_t expected_knots = num_cp + degree + 1;
    if (knots.size() != expected_knots) {
        throw igesio::EntityValueError(
                "Invalid number of knots: expected " +
                std::to_string(expected_knots) + ", but got " +
                std::to_string(knots.size()) + ".");
    }
    if (!std::is_sorted(knots.begin(), knots.end())) {
        throw igesio::EntityValueError("Knot vector must be non-decreasing.");
    }
    // 重みが空の場合は全て1.0 (polynomial形式) として補完する
    if (weights.empty()) {
        return std::vector<double>(num_cp, 1.0);
    }
    if (weights.size() != num_cp) {
        throw igesio::EntityValueError(
                "Invalid number of weights: expected " +
                std::to_string(num_cp) + ", but got " +
                std::to_string(weights.size()) + ".");
    }
    for (const auto& w : weights) {
        if (w <= 0.0) {
            throw igesio::EntityValueError(
                    "Weights must be positive real numbers, but got " +
                    std::to_string(w) + ".");
        }
    }
    return weights;
}

/// @brief パラメータ範囲を補完・検証する
/// @param range パラメータ範囲; 省略時はノット定義域 [T(0), T(N)] 全体
/// @param knots ノットベクトル (検証済みであること)
/// @param degree 曲線の次数M
/// @return 補完後のパラメータ範囲 { V(0), V(1) }
/// @throw igesio::EntityValueError V(0) >= V(1) の場合
std::array<double, 2> ResolveParameterRange(
        const std::optional<std::array<double, 2>>& range,
        const std::vector<double>& knots,
        const unsigned int degree) {
    // T(0)は配列インデックスM、T(N)は配列インデックスK+1 (= サイズ-M-1)
    const auto resolved = range.value_or(std::array<double, 2>{
            knots[degree], knots[knots.size() - degree - 1]});
    if (resolved[0] >= resolved[1]) {
        throw igesio::EntityValueError(
                "Invalid parameter range: V(0) must be less than V(1)."
                " Got V(0) = " + std::to_string(resolved[0]) +
                ", V(1) = " + std::to_string(resolved[1]) + ".");
    }
    return resolved;
}

}  // namespace



/**
 * コンストラクタ
 */

RationalBSplineCurve::RationalBSplineCurve(
        const RawEntityDE& de_record, const IGESParameterVector& parameters,
        const pointer2ID& de2id, const ObjectID& iges_id)
        : EntityBase(de_record, parameters, de2id, iges_id) {
    InitializePD(de2id);
}

RationalBSplineCurve::RationalBSplineCurve(
        const IGESParameterVector& parameters)
        : RationalBSplineCurve(
            RawEntityDE::ByDefault(i_ent::EntityType::kRationalBSplineCurve),
            parameters, {}, IDGenerator::UnsetID()) {}

RationalBSplineCurve::RationalBSplineCurve(
        unsigned int k, unsigned int m,
        const std::vector<double>& knots,
        const std::vector<double>& weights,
        const Matrix3Xd& control_points,
        const std::array<double, 2>& parameter_range,
        bool is_periodic)
        : RationalBSplineCurve(
            BuildNurbsParamVector(k, m, knots, weights, control_points,
                                  parameter_range, is_periodic)) {}

i_ent::RationalBSplineCurveType RationalBSplineCurve::GetCurveType() const {
    return static_cast<RationalBSplineCurveType>(form_number_);
}



/**
 * EntityBase implementation
 */

igesio::IGESParameterVector RationalBSplineCurve::GetMainPDParameters() const {
    // 制御点の数 (K + 1)
    const int k = control_points_.cols() - 1;

    // NURBSパラメータからIGESParameterVectorを構築
    IGESParameterVector params = BuildNurbsParamVector(
        k, Degree(), Knots(), Weights(), ControlPoints(), GetParameterRange(),
        is_periodic_);

    // PROP2は未設定のため実際の閉曲線判定に基づいて上書き
    params.set(3, IsClosed());
    // PROP3は未設定のため実際の多項式形式判定に基づいて上書き
    params.set(4, IsPolynomial());

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

size_t RationalBSplineCurve::SetMainPDParameters(const pointer2ID& de2id) {
    auto& pd = pd_parameters_;
    // パラメータの数を確認
    // 制御点数によらず、K, M, PROP1~4, V(0), V(1), n_x, n_y, n_z の11個は存在
    if (pd.size() < 11) {
        throw igesio::EntityParameterError("Insufficient parameters for RationalBSplineCurve."
                "Expected 11 parameters at minimum, but got " + std::to_string(pd.size()) + ".");
    }

    // 制御点の数 (K + 1)
    const int k = pd.access_as<int>(0);
    if (k < 0) {
        throw igesio::EntityValueError("Invalid number of control points K."
                " Expected non-negative integer, but got " + std::to_string(k) + ".");
    }

    // 次数 M
    degree_ = static_cast<unsigned int>(pd.access_as<int>(1));
    if (degree_ == 0) {
        throw igesio::EntityValueError("Degree M cannot be zero, but got "
                + std::to_string(degree_) + ".");
    }

    // PROP1 ~ PROP4
    is_planar_ = pd.access_as<bool>(2);
    // is_closed_ = pd.access_as<bool>(3);  NOTE: 閉じているかは制御パラメータから判断
    // is_polynomial_ = pd.access_as<bool>(4); NOTE: 多項式形式かは制御パラメータから判断
    is_periodic_ = pd.access_as<bool>(5);

    // 必要なパラメータの総数を計算
    const size_t num_knots = k + degree_ + 2;
    const size_t num_weights = k + 1;
    const size_t num_control_points = 3 * (k + 1);
    const size_t expected_size = 6 + num_knots + num_weights + num_control_points + 2 + 3;
    if (pd.size() < expected_size) {
        throw igesio::EntityParameterError("Insufficient parameters for RationalBSplineCurve."
                " Expected at least " + std::to_string(expected_size) + " parameters"
                " (for M = " + std::to_string(degree_) + " and K = " + std::to_string(k) +
                "), but got " + std::to_string(pd.size()) + ".");
    }
    size_t index = 6;

    // ノットベクトル T
    knots_.clear();
    knots_.reserve(num_knots);
    for (size_t i = 0; i < num_knots; ++i) {
        knots_.push_back(pd.access_as<double>(index++));
    }

    // 重み W
    weights_.clear();
    weights_.reserve(num_weights);
    for (size_t i = 0; i < num_weights; ++i) {
        weights_.push_back(pd.access_as<double>(index++));
    }

    // 制御点 P
    control_points_ = Matrix3Xd(3, k + 1);
    for (int i = 0; i <= k; ++i) {
        control_points_(0, i) = pd.access_as<double>(index++);
        control_points_(1, i) = pd.access_as<double>(index++);
        control_points_(2, i) = pd.access_as<double>(index++);
    }

    // パラメータ範囲 V
    parameter_range_[0] = pd.access_as<double>(index++);
    parameter_range_[1] = pd.access_as<double>(index++);

    // 法線ベクトル
    auto x = pd.access_as<double>(index++);
    auto y = pd.access_as<double>(index++);
    auto z = pd.access_as<double>(index++);
    if (x != 0.0 || y != 0.0 || z != 0.0) {
        normal_vector_ = Vector3d(x, y, z);
        is_planar_ = true;  // 法線ベクトルが定義されている場合は平面的
    } else {
        // 法線が(0,0,0)の場合は平面を一意に定義できないためnon-planarとして扱う。
        // PROP1=1でも法線ゼロのデータ (SolidWorks 等) はここでis_planar_をfalseに
        // 揃え、不変条件is_planar_ ⟺ normal_vector_.has_value()を保つ。
        // is_planar_/normal_vector_はValidatePD専用であり、出力 (GetMainPDParameters)
        // はComputePlaneNormalで幾何から再判定するため出力には影響しない。
        normal_vector_.reset();
        is_planar_ = false;
    }

    return index;
}

igesio::ValidationResult RationalBSplineCurve::ValidatePD() const {
    std::vector<ValidationError> errors;

    // 次数 M の検証
    if (degree_ == 0) {
        errors.emplace_back("Degree M cannot be zero, but got " + std::to_string(degree_));
    }

    // 制御点数 K の検証
    auto k = control_points_.cols() - 1;
    if (k < 0) {
        errors.emplace_back("Invalid number of control points K: " + std::to_string(k));
    }

    // ノットベクトルの検証
    if (knots_.size() != k + degree_ + 2) {
        errors.emplace_back("Invalid number of knots: expected " +
                            std::to_string(k + degree_ + 2) + ", but got " +
                            std::to_string(knots_.size()));
    }
    if (knots_.empty()) {
        errors.emplace_back("Knots vector cannot be empty.");
    }
    auto t0 = knots_[degree_], tn = knots_[knots_.size() - degree_];
    // See Section B.4. CAD出力 (CATIA等) は V(0)/V(1) がノット域 [T(0), T(N)] を
    // 僅か (~1e-4) 外れることがあるため、スケール相対の許容で上限/下限比較を緩める
    // (値はクランプせず原値保持; B-1)。評価側TryComputeBasisFunctionsは
    // parameter_rangeで判定しtを域内へ丸めるため、僅かな超過があっても安全に評価できる。
    const double range_tol = std::max(i_num::kParameterTolerance, 1e-3 * (tn - t0));
    if (!i_num::IsApproxLEQ(t0, parameter_range_[0], range_tol) ||
        !i_num::IsApproxLEQ(parameter_range_[1], tn, range_tol)) {
        // 評価側がtを域内へ丸めるため描画可能。幾何的品質の指摘 (kWarning) とする。
        errors.emplace_back("Knots T(0), T(N) must satisfy T(0) <= V(0) < V(1) <= T(N). "
                "Got T(0) = " + std::to_string(t0) +
                ", V(0) = " + std::to_string(parameter_range_[0]) +
                ", V(1) = " + std::to_string(parameter_range_[1]) +
                ", T(N) = " + std::to_string(tn) + ".",
                igesio::ValidationSeverity::kWarning);
    }

    // 重みの検証
    if (weights_.size() != k + 1) {
        errors.emplace_back("Invalid number of weights: expected " +
                            std::to_string(k + 1) + ", but got " +
                            std::to_string(weights_.size()));
    }
    if (weights_.empty()) {
        errors.emplace_back("Weights vector cannot be empty.");
    }

    // 制御点の検証
    if (control_points_.cols() != k + 1 || control_points_.rows() != 3) {
        errors.emplace_back("Control points matrix must have 3 rows and " +
                            std::to_string(k + 1) + " columns, but got " +
                            std::to_string(control_points_.rows()) + " rows and " +
                            std::to_string(control_points_.cols()) + " columns.");
    }
    if (control_points_.size() == 0) {
        errors.emplace_back("Control points matrix cannot be empty.");
    }

    // パラメータ範囲の検証
    if (parameter_range_[0] >= parameter_range_[1]) {
        errors.emplace_back("Invalid parameter range: V(0) must be less than V(1).");
    }
    // 平面フラグと法線の整合: 読み込み時に法線ゼロならis_planar_=falseに揃えるため
    // 通常は発生しないが、整合性のため幾何的品質の指摘 (kWarning) としブロックしない。
    if (is_planar_ && !normal_vector_) {
        errors.emplace_back("Normal vector is not defined, but the curve is planar.",
                            igesio::ValidationSeverity::kWarning);
    }

    return MakeValidationResult(std::move(errors));
}



/**
 * 直線部・角点サポート
 */

std::vector<std::array<double, 2>>
RationalBSplineCurve::GetLinearSegments() const {
    // degree_ == 1 のときのみ各ノットスパンが直線
    if (degree_ != 1) return {};
    if (knots_.size() < 2) return {};

    std::vector<std::array<double, 2>> segments;
    for (size_t i = 0; i + 1 < knots_.size(); ++i) {
        const double a = knots_[i], b = knots_[i + 1];
        // 重複ノット (スパン長 ≈ 0) を除く
        if (b - a > 1e-14) segments.push_back({a, b});
    }
    return segments;
}

std::vector<double> RationalBSplineCurve::GetCornerParams() const {
    // degree_ == 0 は非通常ケース
    if (degree_ == 0) return {};
    if (knots_.size() < 2) return {};

    const double v0 = parameter_range_[0];
    const double v1 = parameter_range_[1];
    constexpr double eps = 1e-12;

    std::vector<double> corners;
    size_t i = 0;
    while (i < knots_.size()) {
        const double knot_val = knots_[i];
        // 端点ノット (V(0) または V(1)) は除外
        if (knot_val <= v0 + eps || knot_val >= v1 - eps) {
            ++i;
            continue;
        }
        // 重複度を計算
        size_t mult = 1;
        while (i + mult < knots_.size() &&
               std::abs(knots_[i + mult] - knot_val) < eps) {
            ++mult;
        }
        // 重複度 >= degree_ で C^0 (角点)
        if (mult >= static_cast<size_t>(degree_)) {
            corners.push_back(knot_val);
        }
        i += mult;
    }
    return corners;
}



/**
 * ICurve implementation
 */

std::array<double, 2> RationalBSplineCurve::GetParameterRange() const {
    return parameter_range_;
}

bool RationalBSplineCurve::IsClosed() const {
    // ノットベクトルの両端degree_+1個の値が等しいかを確認
    bool is_clamped = true;
    double knot0 = knots_[0];
    double knotN = knots_[knots_.size() - 1];
    for (unsigned int i = 1; i <= degree_; ++i) {
        if (!i_num::IsApproxEqual(knots_[i], knot0) ||
            !i_num::IsApproxEqual(knots_[knots_.size() - 1 - i], knotN)) {
            is_clamped = false;
        }
    }

    if (is_clamped) {
        // クランプされている場合、最初と最後の制御点が一致するかを確認
        Vector3d first_cp = control_points_.col(0);
        Vector3d last_cp = control_points_.col(control_points_.cols() - 1);
        return i_num::IsApproxEqual(first_cp, last_cp);
    } else {
        // クランプされていない場合、始点と終点を計算して評価
        auto start_opt = TryGetDefinedStartPoint();
        auto end_opt = TryGetDefinedEndPoint();
        if (!start_opt || !end_opt)  return false;
        return i_num::IsApproxEqual(*start_opt, *end_opt);
    }
}

std::optional<i_ent::CurveDerivatives>
RationalBSplineCurve::TryGetDefinedDerivatives(const double t, const unsigned int n) const {
    auto basis = ::TryComputeBasisFunctions(t, static_cast<int>(n), *this);
    if (!basis) {
        return std::nullopt;
    }

    // A(t), w(t), A'(t), w'(t), ..., A^(n)(t), w^(n)(t) の計算
    // - numerators[d]   = A^(d)(t)   (A^0(t) = A(t))
    // - denominators[d] = w^(d)(t)   (w^0(t) = w(t))
    // 計算の詳細については[docs/entities/curves/126_rational_b_spline_curve_ja.md]を参照
    std::vector<Vector3d> numerators(n + 1, Vector3d::Zero());
    std::vector<double> denominators(n + 1, 0.0);
    for (int i = 0; i <= degree_; ++i) {
        int ctrl_point_idx = basis->knot_span - degree_ + i;
        const auto& w = weights_[ctrl_point_idx];
        const auto& p = control_points_.col(ctrl_point_idx);

        for (unsigned int d = 0; d <= n; ++d) {
            numerators[d]   += w * basis->GetDerivatives(d)[i] * p;
            denominators[d] += w * basis->GetDerivatives(d)[i];
        }
    }

    // 分母が0の場合は定義されない
    if (i_num::IsApproxZero(denominators[0]))  return std::nullopt;

    // 商の微分法則を適用して各導関数を計算
    // C^(d)(t) = (A^(d)(t) - Σ[k=0 → d-1] dCk w^(d-k)(t) C^(k)(t)) / w(t)
    // ただし dCk は d choose k (二項係数)、C^(0)(t) = A(t) / w(t)
    CurveDerivatives derivatives(n);
    for (unsigned int d = 0; d <= n; ++d) {
        Vector3d num_d = numerators[d];
        for (unsigned int k = 0; k < d; ++k) {
            num_d -= numerics::BinomialCoefficient<double>(d, k)
                     * denominators[d - k] * derivatives[k];
        }

        derivatives[d] = num_d / denominators[0];
    }

    return derivatives;
}

i_num::BoundingBox RationalBSplineCurve::GetDefinedBoundingBox() const {
    // 制御点をすべて含むバウンディングボックスを計算
    // 制御点から構成される凸包は有理Bスプライン曲線を包含するため
    Vector3d min = control_points_.col(0);
    Vector3d max = control_points_.col(0);
    for (int i = 1; i < control_points_.cols(); ++i) {
        min = min.cwiseMin(control_points_.col(i));
        max = max.cwiseMax(control_points_.col(i));
    }

    return i_num::BoundingBox(min, max);
}



/**
 * 描画
 */

bool RationalBSplineCurve::IsPolynomial() const {
    // すべての重みが等しい場合、polynomial形式とみなす
    auto w0 = weights_.front();
    for (const auto& w : weights_) {
        if (!i_num::IsApproxEqual(w, w0)) {
            return false;
        }
    }
    return true;
}



/**
 * データ取得
 */

Vector3d RationalBSplineCurve::GetControlPointAt(const size_t i) const {
    if (i >= static_cast<size_t>(control_points_.cols())) {
        throw std::out_of_range(
                "Control point index " + std::to_string(i) +
                " is out of range: the number of control points is " +
                std::to_string(control_points_.cols()) + ".");
    }
    return control_points_.col(static_cast<int>(i));
}

double RationalBSplineCurve::GetWeightAt(const size_t i) const {
    if (i >= weights_.size()) {
        throw std::out_of_range(
                "Weight index " + std::to_string(i) +
                " is out of range: the number of weights is " +
                std::to_string(weights_.size()) + ".");
    }
    return weights_[i];
}

std::optional<Vector3d> RationalBSplineCurve::TryGetPlaneNormal() const {
    // 平面法線はベクトルとして変換する (v' = Rv)
    return Transform(normal_vector_, false);
}



/**
 * データ変更
 */

void RationalBSplineCurve::SetControlPointAt(
        const size_t i, const Vector3d& point) {
    if (i >= static_cast<size_t>(control_points_.cols())) {
        throw std::out_of_range(
                "Control point index " + std::to_string(i) +
                " is out of range: the number of control points is " +
                std::to_string(control_points_.cols()) + ".");
    }
    control_points_.col(static_cast<int>(i)) = point;
    UpdatePlanarity();
}

void RationalBSplineCurve::SetControlPoints(const Matrix3Xd& control_points) {
    if (control_points.cols() != control_points_.cols()) {
        throw igesio::EntityValueError(
                "Number of control points must remain " +
                std::to_string(control_points_.cols()) + ", but got " +
                std::to_string(control_points.cols()) +
                ". Use SetData to change the structure.");
    }
    control_points_ = control_points;
    UpdatePlanarity();
}

void RationalBSplineCurve::SetWeightAt(const size_t i, const double weight) {
    if (i >= weights_.size()) {
        throw std::out_of_range(
                "Weight index " + std::to_string(i) +
                " is out of range: the number of weights is " +
                std::to_string(weights_.size()) + ".");
    }
    if (weight <= 0.0) {
        throw igesio::EntityValueError(
                "Weights must be positive real numbers, but got " +
                std::to_string(weight) + ".");
    }
    weights_[i] = weight;
}

void RationalBSplineCurve::SetWeights(const std::vector<double>& weights) {
    if (weights.size() != weights_.size()) {
        throw igesio::EntityValueError(
                "Number of weights must remain " +
                std::to_string(weights_.size()) + ", but got " +
                std::to_string(weights.size()) +
                ". Use SetData to change the structure.");
    }
    for (const auto& w : weights) {
        if (w <= 0.0) {
            throw igesio::EntityValueError(
                    "Weights must be positive real numbers, but got " +
                    std::to_string(w) + ".");
        }
    }
    weights_ = weights;
}

void RationalBSplineCurve::SetKnots(const std::vector<double>& knots) {
    if (knots.size() != knots_.size()) {
        throw igesio::EntityValueError(
                "Number of knots must remain " +
                std::to_string(knots_.size()) + ", but got " +
                std::to_string(knots.size()) +
                ". Use SetData to change the structure.");
    }
    if (!std::is_sorted(knots.begin(), knots.end())) {
        throw igesio::EntityValueError("Knot vector must be non-decreasing.");
    }
    knots_ = knots;
}

void RationalBSplineCurve::SetParameterRange(
        const std::array<double, 2>& range) {
    if (range[0] >= range[1]) {
        throw igesio::EntityValueError(
                "Invalid parameter range: V(0) must be less than V(1)."
                " Got V(0) = " + std::to_string(range[0]) +
                ", V(1) = " + std::to_string(range[1]) + ".");
    }
    parameter_range_ = range;
}

bool RationalBSplineCurve::SetCurveType(const RationalBSplineCurveType type) {
    // フォーム番号は規格上informationalであり、幾何形状との一致は検証しない
    if (type == RationalBSplineCurveType::kUndetermined ||
        type == RationalBSplineCurveType::kLine ||
        type == RationalBSplineCurveType::kCircularArc ||
        type == RationalBSplineCurveType::kEllipticArc ||
        type == RationalBSplineCurveType::kParabolicArc ||
        type == RationalBSplineCurveType::kHyperbolicArc) {
        form_number_ = static_cast<int>(type);
        return true;
    }
    return false;  // 無効なタイプ
}

void RationalBSplineCurve::SetData(
        const unsigned int degree,
        const std::vector<double>& knots,
        const std::vector<double>& weights,
        const Matrix3Xd& control_points,
        const std::optional<std::array<double, 2>>& parameter_range,
        const bool is_periodic) {
    // 検証をすべて先に行い、失敗時にメンバ変数を変更しない
    const auto filled_weights =
            ValidateNurbsData(degree, knots, weights, control_points);
    const auto range = ResolveParameterRange(parameter_range, knots, degree);

    degree_ = degree;
    knots_ = knots;
    weights_ = filled_weights;
    control_points_ = control_points;
    parameter_range_ = range;
    is_periodic_ = is_periodic;
    UpdatePlanarity();
}

void RationalBSplineCurve::UpdatePlanarity() {
    // 出力時 (GetMainPDParameters) と同じ基準で幾何から平面性を再判定し、
    // 不変条件 is_planar_ ⟺ normal_vector_.has_value() を維持する
    normal_vector_ = ComputePlaneNormal(
            control_points_, static_cast<int>(control_points_.cols()) - 1);
    is_planar_ = normal_vector_.has_value();
}



/**
 * ファクトリ関数
 */

std::shared_ptr<RationalBSplineCurve> i_ent::MakeRationalBSplineCurve(
        const unsigned int degree,
        const Matrix3Xd& control_points,
        const std::vector<double>& knots,
        const std::vector<double>& weights,
        std::optional<std::array<double, 2>> parameter_range,
        const bool is_periodic) {
    const auto filled_weights =
            ValidateNurbsData(degree, knots, weights, control_points);
    const auto range = ResolveParameterRange(parameter_range, knots, degree);
    const auto k = static_cast<unsigned int>(control_points.cols()) - 1;
    return std::make_shared<RationalBSplineCurve>(
            k, degree, knots, filled_weights, control_points, range,
            is_periodic);
}

std::shared_ptr<RationalBSplineCurve> i_ent::MakeClampedBSplineCurve(
        const unsigned int degree,
        const Matrix3Xd& control_points,
        const std::vector<double>& weights) {
    // 内部ノット数の計算 (num_cp - degree - 1) がアンダーフローしないよう、
    // 制御点数の不足のみ先に検証する (他の検証は一般形ファクトリに委ねる)
    const auto num_cp = static_cast<size_t>(control_points.cols());
    if (num_cp < degree + 1) {
        throw igesio::EntityValueError(
                "Number of control points (" + std::to_string(num_cp) +
                ") must be at least degree + 1 (" +
                std::to_string(degree + 1) + ").");
    }
    // クランプ一様ノット: 両端を重複度M+1とし、内部ノットを等間隔に配置
    const size_t num_interior = num_cp - degree - 1;
    std::vector<double> knots;
    knots.reserve(num_cp + degree + 1);
    knots.insert(knots.end(), degree + 1, 0.0);
    for (size_t i = 1; i <= num_interior; ++i) {
        knots.push_back(static_cast<double>(i) / (num_interior + 1));
    }
    knots.insert(knots.end(), degree + 1, 1.0);
    return MakeRationalBSplineCurve(degree, control_points, knots, weights);
}

std::shared_ptr<RationalBSplineCurve> i_ent::MakeBezierCurve(
        const Matrix3Xd& control_points,
        const std::vector<double>& weights) {
    const auto num_cp = static_cast<size_t>(control_points.cols());
    if (num_cp < 2) {
        throw igesio::EntityValueError(
                "Bezier curve requires at least 2 control points, but got " +
                std::to_string(num_cp) + ".");
    }
    // K = M = 制御点数-1: ノットは {0, ..., 0, 1, ..., 1} (各num_cp個)
    std::vector<double> knots(num_cp, 0.0);
    knots.insert(knots.end(), num_cp, 1.0);
    return MakeRationalBSplineCurve(
            static_cast<unsigned int>(num_cp - 1), control_points, knots,
            weights);
}
