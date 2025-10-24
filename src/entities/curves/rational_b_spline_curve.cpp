/**
 * @file entities/curves/rational_b_spline_curve.cpp
 * @brief Rational B-Spline Curve (Type 126): 有理Bスプライン曲線エンティティの定義
 * @author Yayoi Habami
 * @date 2025-08-15
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/curves/rational_b_spline_curve.h"

#include <utility>
#include <vector>

#include "igesio/common/tolerance.h"
#include "igesio/numerics/combinatorics.h"
#include "./nurbs_basis_function.h"

namespace {

namespace i_ent = igesio::entities;
using RationalBSplineCurve = i_ent::RationalBSplineCurve;
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

i_ent::RationalBSplineCurveType RationalBSplineCurve::GetCurveType() const {
    return static_cast<RationalBSplineCurveType>(form_number_);
}



/**
 * EntityBase implementation
 */

igesio::IGESParameterVector RationalBSplineCurve::GetMainPDParameters() const {
    IGESParameterVector params;

    // 制御点の数 (K + 1)
    const int k = control_points_.cols() - 1;
    params.push_back(k);

    // 次数 M
    const unsigned int m = degree_;
    params.push_back(static_cast<int>(m));

    // PROP1 ~ PROP4
    params.push_back(is_planar_ ? 1 : 0);
    params.push_back(is_closed_ ? 1 : 0);
    params.push_back(is_polynomial_ ? 1 : 0);
    params.push_back(is_periodic_ ? 1 : 0);

    // ノットベクトル T
    for (const auto& knot : knots_) {
        params.push_back(knot);
    }

    // 重み W
    for (const auto& weight : weights_) {
        params.push_back(weight);
    }

    // 制御点 P = (x_i, y_i, z_i)
    for (int i = 0; i <= k; ++i) {
        params.push_back(control_points_(0, i));  // X
        params.push_back(control_points_(1, i));  // Y
        params.push_back(control_points_(2, i));  // Z
    }

    // パラメータ範囲 V = {V(0), V(1)}
    params.push_back(parameter_range_[0]);
    params.push_back(parameter_range_[1]);

    // 法線ベクトル (平面的でない場合は{0, 0, 0})
    if (normal_vector_) {
        params.push_back(normal_vector_->x());
        params.push_back(normal_vector_->y());
        params.push_back(normal_vector_->z());
    } else {
        params.push_back(0.0);
        params.push_back(0.0);
        params.push_back(0.0);
    }

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
        throw igesio::DataFormatError("Insufficient parameters for RationalBSplineCurve."
                "Expected 11 parameters at minimum, but got " + std::to_string(pd.size()) + ".");
    }

    // 制御点の数 (K + 1)
    const int k = pd.access_as<int>(0);
    if (k < 0) {
        throw igesio::DataFormatError("Invalid number of control points K."
                " Expected non-negative integer, but got " + std::to_string(k) + ".");
    }

    // 次数 M
    degree_ = static_cast<unsigned int>(pd.access_as<int>(1));
    if (degree_ == 0) {
        throw igesio::DataFormatError("Degree M cannot be zero, but got "
                + std::to_string(degree_) + ".");
    }

    // PROP1 ~ PROP4
    is_planar_ = pd.access_as<bool>(2);
    is_closed_ = pd.access_as<bool>(3);
    is_polynomial_ = pd.access_as<bool>(4);
    is_periodic_ = pd.access_as<bool>(5);

    // 必要なパラメータの総数を計算
    const size_t num_knots = k + degree_ + 2;
    const size_t num_weights = k + 1;
    const size_t num_control_points = 3 * (k + 1);
    const size_t expected_size = 6 + num_knots + num_weights + num_control_points + 2 + 3;
    if (pd.size() < expected_size) {
        throw igesio::DataFormatError("Insufficient parameters for RationalBSplineCurve."
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
        normal_vector_.reset();
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
    if (t0 > parameter_range_[0] || parameter_range_[1] > tn) {
        // See Section B.4
        errors.emplace_back("Knots T(0), T(N) must satisfy T(0) <= V(0) < V(1) <= T(N). "
                "Got T(0) = " + std::to_string(t0) +
                ", V(0) = " + std::to_string(parameter_range_[0]) +
                ", V(1) = " + std::to_string(parameter_range_[1]) +
                ", T(N) = " + std::to_string(tn) + ".");
    }

    // 重みの検証
    if (weights_.size() != k + 1) {
        errors.emplace_back("Invalid number of weights: expected " +
                            std::to_string(k + 1) + ", but got " +
                            std::to_string(weights_.size()));
    }
    if (weights_.empty()) {
        errors.emplace_back("Weights vector cannot be empty.");
    } else if (is_polynomial_) {
        // polynomial形式の場合、全ての重みが等しいことを確認
        for (const auto& weight : weights_) {
            if (!IsApproxEqual(weight, weights_[0])) {
                errors.emplace_back("All weights must be equal for polynomial form. "
                        "Got weights: " + std::to_string(weights_[0]) + " and " +
                        std::to_string(weight) + ".");
                break;
            }
        }
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
    if (is_planar_ && !normal_vector_) {
        errors.emplace_back("Normal vector is not defined, but the curve is planar.");
    }

    return MakeValidationResult(std::move(errors));
}



/**
 * ICurve implementation
 */

std::array<double, 2> RationalBSplineCurve::GetParameterRange() const {
    return parameter_range_;
}

bool RationalBSplineCurve::IsClosed() const {
    return is_closed_;
}

std::optional<i_ent::CurveDerivatives>
RationalBSplineCurve::TryGetDerivatives(const double t, const unsigned int n) const {
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
    if (IsApproxZero(denominators[0]))  return std::nullopt;

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
