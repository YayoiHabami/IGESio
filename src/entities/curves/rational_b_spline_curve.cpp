/**
 * @file entities/curves/rational_b_spline_curve.cpp
 * @brief Rational B-Spline Curve (Type 126): 有理Bスプライン曲線エンティティの定義
 * @author Yayoi Habami
 * @date 2025-08-15
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/curves/rational_b_spline_curve.h"

#include <array>
#include <utility>
#include <vector>

#include "igesio/numerics/tolerance.h"
#include "igesio/numerics/combinatorics.h"
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
    // is_closed_ = pd.access_as<bool>(3);  NOTE: 閉じているかは制御パラメータから判断
    // is_polynomial_ = pd.access_as<bool>(4); NOTE: 多項式形式かは制御パラメータから判断
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
