/**
 * @file entities/surfaces/rational_b_spline_surface.cpp
 * @brief Rational B-Spline Surface (Type 126): 有理Bスプライン曲面エンティティの定義
 * @author Yayoi Habami
 * @date 2025-09-25
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/surfaces/rational_b_spline_surface.h"

#include <utility>
#include <vector>

#include "igesio/common/tolerance.h"
#include "./../curves/nurbs_basis_function.h"

namespace {

namespace i_ent = igesio::entities;
using RationalBSplineSurface = i_ent::RationalBSplineSurface;
using Vector3d = igesio::Vector3d;

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

}  // namespace



/**
 * コンストラクタ
 */

RationalBSplineSurface::RationalBSplineSurface(
        const RawEntityDE& de_record, const IGESParameterVector& parameters,
        const pointer2ID& de2id, const uint64_t iges_id)
        : EntityBase(de_record, parameters, de2id, iges_id) {
    InitializePD(de2id);
}

RationalBSplineSurface::RationalBSplineSurface(
        const IGESParameterVector& parameters)
        : RationalBSplineSurface(
            RawEntityDE::ByDefault(i_ent::EntityType::kRationalBSplineSurface),
            parameters, {}, kUnsetID) {}

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

    // 重み W
    for (int i = 0; i < weights_.rows(); ++i) {
        for (int j = 0; j < weights_.cols(); ++j) {
            params.push_back(weights_(i, j));
        }
    }

    // 制御点 P = (x_ij, y_ij, z_ij)
    for (int i = 0; i < control_points_.cols(); ++i) {
        params.push_back(control_points_(0, i));
        params.push_back(control_points_(1, i));
        params.push_back(control_points_(2, i));
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
        throw igesio::DataFormatError("Insufficient number of parameters "
                "for Rational B-Spline Surface entity (Type 128). "
                "(expected at least 13, got " + std::to_string(pd.size()) + ")");
    }

    // 制御点の数 (K1 + 1, K2 + 1)
    const int k1 = pd.access_as<int>(0);
    const int k2 = pd.access_as<int>(1);
    if (k1 < 0 || k2 < 0) {
        throw igesio::DataFormatError("Invalid number of control points "
                "for Rational B-Spline Surface entity. (K1 and K2 must be "
                "non-negative, but got K1 = " + std::to_string(k1) +
                ", K2 = " + std::to_string(k2) + ")");
    }

    // 曲面の次数 (M1, M2)
    const int m1 = static_cast<unsigned int>(pd.access_as<int>(2));
    const int m2 = static_cast<unsigned int>(pd.access_as<int>(3));
    if (m1 == 0 || m2 == 0) {
        throw igesio::DataFormatError("Invalid degree for Rational B-Spline "
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
        throw igesio::DataFormatError("Insufficient number of parameters "
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
    weights_ = MatrixXd(k1 + 1, k2 + 1);
    for (int i = 0; i <= k1; ++i) {
        for (int j = 0; j <= k2; ++j) {
            weights_(i, j) = pd.access_as<double>(index++);
        }
    }

    // 制御点 P
    control_points_ = Matrix3Xd(3, (k1 + 1) * (k2 + 1));
    for (int i = 0; i <= k1; ++i) {
        for (int j = 0; j <= k2; ++j) {
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
            if (u_knots_[i] < u_knots_[i - 1] - igesio::kParameterTolerance) {
                errors.emplace_back("U knot vector must be non-decreasing, "
                    "but got decrease at index " + std::to_string(i) +
                    ": " + std::to_string(u_knots_[i - 1]) + " > "
                    + std::to_string(u_knots_[i]));
                break;
            }
        }
        // S(0) <= U(0) < U(1) <= S(N1); See Section B.6
        auto s0 = u_knots_[m1], sn = u_knots_[u_knots_.size() - m1];
        if (s0 > parameter_range_[0] || parameter_range_[1] > sn) {
            errors.emplace_back("Knots S(0), S(N1) must satisfy "
                "S(0) <= U(0) < U(1) <= S(N1), but got S(0) = "
                + std::to_string(s0) + ", U(0) = "
                + std::to_string(parameter_range_[0]) + ", U(1) = "
                + std::to_string(parameter_range_[1]) + ", S(N1) = "
                + std::to_string(sn));
        }
    }
    if (v_knots_.size() != k2 + m2 + 2) {
        errors.emplace_back("Size of v knot vector must be K2 + M2 + 2 = "
            + std::to_string(k2 + m2 + 2) + ", but got "
            + std::to_string(v_knots_.size()));
    } else {
        // 非減少列であること
        for (size_t i = 1; i < v_knots_.size(); ++i) {
            if (v_knots_[i] < v_knots_[i - 1] - igesio::kParameterTolerance) {
                errors.emplace_back("V knot vector must be non-decreasing, "
                    "but got decrease at index " + std::to_string(i) +
                    ": " + std::to_string(v_knots_[i - 1]) + " > "
                    + std::to_string(v_knots_[i]));
                break;
            }
        }
        // T(0) <= V(0) < V(1) <= T(N2); See Section B.6
        auto t0 = v_knots_[m2], tn = v_knots_[v_knots_.size() - m2];
        if (t0 > parameter_range_[2] || parameter_range_[3] > tn) {
            errors.emplace_back("Knots T(0), T(N2) must satisfy "
                "T(0) <= V(0) < V(1) <= T(N2), but got T(0) = "
                + std::to_string(t0) + ", V(0) = "
                + std::to_string(parameter_range_[2]) + ", V(1) = "
                + std::to_string(parameter_range_[3]) + ", T(N2) = "
                + std::to_string(tn));
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
                if (weights_(i, j) <= igesio::kParameterTolerance) {
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
                if (!igesio::IsApproxEqual(weights_(i, j), weights_(0, 0))) {
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
std::optional<Vector3d>
RationalBSplineSurface::TryGetDefinedPointAt(
        const double u, const double v) const {
    // パラメータ範囲チェック
    if (u < parameter_range_[0] || u > parameter_range_[1] ||
        v < parameter_range_[2] || v > parameter_range_[3]) {
        return std::nullopt;
    }

    // 基底関数の計算
    auto basis_u_opt = ::TryComputeBasisFunctions(u, true, 0, *this);
    auto basis_v_opt = ::TryComputeBasisFunctions(v, false, 0, *this);
    if (!basis_u_opt || !basis_v_opt) return std::nullopt;
    const auto& basis_u = *basis_u_opt;
    const auto& basis_v = *basis_v_opt;

    Vector3d numerator = Vector3d::Zero();
    double denominator = 0.0;

    // NURBS曲面の定義式
    for (int l = 0; l <= degrees_.second; ++l) {
        for (int k = 0; k <= degrees_.first; ++k) {
            // (l, k) が制御点の範囲外ならスキップ
            int i = basis_u.knot_span - degrees_.first + k;
            int j = basis_v.knot_span - degrees_.second + l;
            if (i < 0 || j < 0 ||
                static_cast<unsigned int>(i) >= NumControlPoints().first ||
                static_cast<unsigned int>(j) >= NumControlPoints().second) {
                continue;
            }

            double N = basis_u.values[k] * basis_v.values[l];
            numerator   += N * weights_(i, j) * ControlPointAt(i, j);
            denominator += N * weights_(i, j);
        }
    }

    // 分母がほぼ0の場合は定義されない
    if (IsApproxZero(denominator)) return std::nullopt;

    return numerator / denominator;
}

std::optional<Vector3d>
RationalBSplineSurface::TryGetDefinedNormalAt(
        const double u, const double v) const {
    // パラメータ範囲チェック
    if (u < parameter_range_[0] || u > parameter_range_[1] ||
        v < parameter_range_[2] || v > parameter_range_[3]) {
        return std::nullopt;
    }

    // 基底関数と1次導関数を計算
    auto basis_u_opt = ::TryComputeBasisFunctions(u, true, 1, *this);
    auto basis_v_opt = ::TryComputeBasisFunctions(v, false, 1, *this);
    if (!basis_u_opt || !basis_v_opt) return std::nullopt;
    const auto& basis_u = *basis_u_opt;
    const auto& basis_v = *basis_v_opt;

    // 曲面の点、u方向の接ベクトル、v方向の接ベクトルを計算
    Vector3d S = Vector3d::Zero();
    Vector3d Su = Vector3d::Zero();
    Vector3d Sv = Vector3d::Zero();
    double w = 0.0, wu = 0.0, wv = 0.0;

    for (int l = 0; l <= degrees_.second; ++l) {
        for (int k = 0; k <= degrees_.first; ++k) {
            // (l, k) が制御点の範囲外ならスキップ
            int i = basis_u.knot_span - degrees_.first + k;
            int j = basis_v.knot_span - degrees_.second + l;
            if (i < 0 || j < 0 ||
                static_cast<unsigned int>(i) >= NumControlPoints().first ||
                static_cast<unsigned int>(j) >= NumControlPoints().second) {
                continue;
            }

            double N = basis_u.values[k] * basis_v.values[l];
            double Nu = basis_u.derivatives[0][k] * basis_v.values[l];
            double Nv = basis_u.values[k] * basis_v.derivatives[0][l];
            S  += N  * weights_(i, j) * ControlPointAt(i, j);
            Su += Nu * weights_(i, j) * ControlPointAt(i, j);
            Sv += Nv * weights_(i, j) * ControlPointAt(i, j);
            w  += N  * weights_(i, j);
            wu += Nu * weights_(i, j);
            wv += Nv * weights_(i, j);
        }
    }
    // 分母がほぼ0の場合は定義されない
    if (IsApproxZero(w)) return std::nullopt;

    // NURBS曲面の微分公式から法線ベクトルを計算
    Vector3d dSdu = (Su * w - S * wu) / (w * w);
    Vector3d dSdv = (Sv * w - S * wv) / (w * w);
    Vector3d normal = dSdu.cross(dSdv);

    // 法線ベクトルがほぼ0の場合は定義されない
    if (IsApproxZero(normal.norm())) return std::nullopt;
    return normal.normalized();
}

std::optional<igesio::Vector3d>
RationalBSplineSurface::TryGetPointAt(const double u, const double v) const {
    return TransformPoint(TryGetDefinedPointAt(u, v));
}

std::optional<igesio::Vector3d>
RationalBSplineSurface::TryGetNormalAt(const double u, const double v) const {
    return TransformVector(TryGetDefinedNormalAt(u, v));
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
