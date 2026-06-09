/**
 * @file entities/curves/parametric_spline_curve.cpp
 * @brief Parametric Spline Curve (Type 112): パラメトリックスプライン曲線エンティティの定義
 * @author Yayoi Habami
 * @date 2025-10-12
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/curves/parametric_spline_curve.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "igesio/numerics/core/tolerance.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using i_ent::ParametricSplineCurve;
using Vector3d = igesio::Vector3d;

/// @brief 型付きパラメータからPDパラメータベクトルを構築する
/// @note 事前検証 (例外送出) もここで行う. 末端のTP値は最終セグメントの
///       係数から自動計算するため、常に係数と整合する
igesio::IGESParameterVector BuildPDParameters(
        const i_ent::ParametricSplineCurveType curve_type,
        const unsigned int degree,
        const std::vector<double>& breakpoints,
        const std::vector<igesio::Matrix34d>& coefficients,
        const unsigned int n_dim) {
    if (coefficients.empty()) {
        throw igesio::EntityValueError(
                "ParametricSplineCurve requires at least one segment.");
    }
    if (breakpoints.size() != coefficients.size() + 1) {
        throw igesio::EntityValueError(
                "Number of breakpoints must be N+1 = "
                + std::to_string(coefficients.size() + 1) + ", but got "
                + std::to_string(breakpoints.size()) + ".");
    }
    for (size_t i = 1; i < breakpoints.size(); ++i) {
        if (breakpoints[i] <= breakpoints[i - 1]) {
            throw igesio::EntityValueError(
                    "Breakpoints must be in strictly increasing order.");
        }
    }
    if (degree > 3) {
        throw igesio::EntityValueError(
                "Degree H must be between 0 and 3, but got "
                + std::to_string(degree) + ".");
    }
    if (n_dim != 2 && n_dim != 3) {
        throw igesio::EntityValueError(
                "N_DIM must be 2 or 3, but got " + std::to_string(n_dim) + ".");
    }

    igesio::IGESParameterVector pd{
            static_cast<int>(curve_type), static_cast<int>(degree),
            static_cast<int>(n_dim), static_cast<int>(coefficients.size())};
    pd.reserve(17 + 13 * coefficients.size());
    for (const auto t_i : breakpoints) {
        pd.push_back(t_i);
    }
    for (const auto& segment : coefficients) {
        for (int p = 0; p < 3; ++p) {
            for (int j = 0; j < 4; ++j) {
                pd.push_back(segment(p, j));
            }
        }
    }

    // 末端 u=T(N+1) (s=T(N+1)-T(N)) における関数値~3次導関数値
    const auto& last = coefficients.back();
    const double s = breakpoints[breakpoints.size() - 1]
                   - breakpoints[breakpoints.size() - 2];
    for (int p = 0; p < 3; ++p) {
        const double b = last(p, 1), c = last(p, 2), d = last(p, 3);
        pd.push_back(last(p, 0) + s * (b + s * (c + s * d)));  // 関数値
        pd.push_back(b + s * (2.0 * c + 3.0 * d * s));         // 1次導関数
        pd.push_back(c + 3.0 * d * s);                         // 2次導関数/2!
        pd.push_back(d);                                       // 3次導関数/3!
    }
    return pd;
}

/// @brief i番目のセグメントにおけるsに対する関数値~3次導関数値を計算する
/// @param coefficients i番目のセグメントにおける多項式係数 (3x4行列)
/// @param s セグメントの始点からの距離 (s = u - T(i))
/// @param order 導関数の階数 (0: 関数値, 1: 1次導関数, 2: 2次導関数, 3: 3次導関数)
/// @return 関数値または導関数値 (3次元ベクトル)
Vector3d ComputeSegmentValue(
        const igesio::Matrix<double, 3, 4>& coefficients,
        const double s, const unsigned int order) {
    igesio::Vector4d s_vector;
    switch (order) {
        case 0:
            s_vector << 1, s, s * s, s * s * s;
            break;
        case 1:
            s_vector << 0, 1, 2 * s, 3 * s * s;
            break;
        case 2:
            s_vector << 0, 0, 2, 6 * s;
            break;
        case 3:
            s_vector << 0, 0, 0, 6;
            break;
        default:
            return Vector3d::Zero();
    }
    return coefficients * s_vector;
}

}  // namespace



/**
 * コンストラクタ
 */

ParametricSplineCurve::ParametricSplineCurve(
        const RawEntityDE& de_record, const IGESParameterVector& parameters,
        const pointer2ID& de2id, const ObjectID& iges_id)
        : EntityBase(de_record, parameters, de2id, iges_id) {
    InitializePD(de2id);
}

ParametricSplineCurve::ParametricSplineCurve(
        const IGESParameterVector& parameters)
        : ParametricSplineCurve(
            RawEntityDE::ByDefault(i_ent::EntityType::kParametricSplineCurve),
            parameters, {}, IDGenerator::UnsetID()) {}

ParametricSplineCurve::ParametricSplineCurve(
        const ParametricSplineCurveType curve_type, const unsigned int degree,
        const std::vector<double>& breakpoints,
        const std::vector<Matrix34d>& coefficients,
        const unsigned int n_dim)
        : ParametricSplineCurve(BuildPDParameters(
                curve_type, degree, breakpoints, coefficients, n_dim)) {}

i_ent::ParametricSplineCurveType
ParametricSplineCurve::GetCurveType() const noexcept {
    return curve_type_;
}



/**
 * EntityBase implementation
 */

igesio::IGESParameterVector ParametricSplineCurve::GetMainPDParameters() const {
    IGESParameterVector params;
    const auto n_segments = NumberOfSegments();
    params.reserve(17 + 13 * n_segments);

    // CTYPE (int)
    params.push_back(static_cast<int>(curve_type_));
    // 次数 H
    params.push_back(static_cast<int>(degree_));
    // N_DIM
    params.push_back(static_cast<int>(n_dim_));
    // セグメント数 N
    params.push_back(static_cast<int>(n_segments));

    // ブレークポイント T(1), ..., T(N+1) (N+1個)
    for (const auto& t : breakpoints_) params.push_back(t);

    // 各セグメントにおける多項式係数
    for (unsigned int i = 0; i < n_segments; ++i) {
        for (unsigned int j = 0; j < 3; ++j) {
            // 各x, y, zについて、A, B, C, Dの順に追加
            params.push_back(coefficients_(j, i * 4 + 0));
            params.push_back(coefficients_(j, i * 4 + 1));
            params.push_back(coefficients_(j, i * 4 + 2));
            params.push_back(coefficients_(j, i * 4 + 3));
        }
    }

    // TPX0~TPX3, TPY0~TPY3, TPZ0~TPZ3
    // 末端 u=T(N+1) における関数値 ~ 3次導関数値
    // u = T(N+1) における s = T(N+1) - T(N)
    double s = breakpoints_.back() - breakpoints_[breakpoints_.size() - 2];
    auto coefficients = Coefficients(n_segments - 1);
    // 関数値 ~ 3次導関数値 (規格に従い、2次以上は階乗で割った値を格納する)
    auto values = ComputeSegmentValue(coefficients, s, 0);
    auto first_derivative = ComputeSegmentValue(coefficients, s, 1);
    auto second_derivative = ComputeSegmentValue(coefficients, s, 2);
    auto third_derivative = ComputeSegmentValue(coefficients, s, 3);
    for (unsigned int j = 0; j < 3; ++j) {
        params.push_back(values(j));
        params.push_back(first_derivative(j));
        params.push_back(second_derivative(j) / 2.0);  // 2次導関数/2!
        params.push_back(third_derivative(j) / 6.0);   // 3次導関数/3!
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

size_t ParametricSplineCurve::SetMainPDParameters(
        const pointer2ID& de2id) {
    auto& pd = pd_parameters_;

    // パラメータ数の最小値チェック
    if (pd.size() < 17) {
        throw igesio::EntityParameterError(
            "Insufficient number of parameters for ParametricSplineCurve."
            " Minimum is 17, but got " + std::to_string(pd.size()) + ".");
    }

    // CTYPE (int)
    const int ctype = pd.access_as<int>(0);
    curve_type_ = static_cast<ParametricSplineCurveType>(ctype);

    // 次数 H
    const int h = pd.access_as<int>(1);
    degree_ = static_cast<unsigned int>(h);

    // N_DIM
    const int n_dim = pd.access_as<int>(2);
    n_dim_ = static_cast<unsigned int>(n_dim);

    // セグメント数 N
    const int n_segments = pd.access_as<int>(3);
    if (n_segments < 1) {
        throw igesio::EntityValueError("Number of segments N must be at least 1,"
            " but got " + std::to_string(n_segments) + ".");
    } else if (pd.size() < 17 + 13 * n_segments) {
        throw igesio::EntityParameterError(
            "Insufficient number of parameters for the specified number of segments N="
            + std::to_string(n_segments) + ". Required at least "
            + std::to_string(17 + 13 * n_segments) + ", but got "
            + std::to_string(pd.size()) + ".");
    }

    // ブレークポイント T(1), ..., T(N+1) (N+1個)
    breakpoints_.clear();
    breakpoints_.reserve(n_segments + 1);
    for (unsigned int i = 0; i < n_segments + 1; ++i) {
        breakpoints_.push_back(pd.access_as<double>(4 + i));
    }

    // 各セグメントにおける多項式係数
    size_t index = 4 + (n_segments + 1);
    coefficients_.resize(NoChange, 4 * n_segments);
    for (unsigned int i = 0; i < n_segments; ++i) {
        for (unsigned int j = 0; j < 3; ++j) {
            // 各x, y, zについて、A, B, C, Dの順に読み込み
            coefficients_(j, i * 4 + 0) = pd.access_as<double>(index++);
            coefficients_(j, i * 4 + 1) = pd.access_as<double>(index++);
            coefficients_(j, i * 4 + 2) = pd.access_as<double>(index++);
            coefficients_(j, i * 4 + 3) = pd.access_as<double>(index++);
        }
    }

    // TPX0~TPX3, TPY0~TPY3, TPZ0~TPZ3 (検証は行わない)
    // 末端 u=T(N+1) における関数値 ~ 3次導関数値
    for (unsigned int j = 0; j < 3; ++j) {
        for (unsigned int order = 0; order < 4; ++order) {
            end_derivatives_(j, order) = pd.access_as<double>(index++);
        }
    }

    return index;
}

igesio::ValidationResult ParametricSplineCurve::ValidatePD() const {
    std::vector<ValidationError> errors;

    // CTYPE
    auto ctype = static_cast<int>(curve_type_);
    if (ctype < 1 || ctype > 6) {
        errors.emplace_back("Invalid CTYPE value for ParametricSplineCurve."
            " Must be between 1 and 6, but got " + std::to_string(ctype) + ".");
    }

    // 次数 H
    if (degree_ < 0 || degree_ > 3) {
        errors.emplace_back("Degree H must be between 0 and 3,"
            " but got " + std::to_string(degree_) + ".");
    }

    // N_DIM
    if (n_dim_ != 2 && n_dim_ != 3) {
        errors.emplace_back("N_DIM must be 2 or 3, but got " + std::to_string(n_dim_) + ".");
    }

    // ブレークポイントの数
    auto n_segments = NumberOfSegments();
    if (breakpoints_.size() != n_segments + 1) {
        errors.emplace_back("Number of breakpoints must be N+1 = "
            + std::to_string(n_segments + 1) + ", but got "
            + std::to_string(breakpoints_.size()) + ".");
    }
    // ブレークポイントの単調増加性チェック
    for (unsigned int i = 1; i < breakpoints_.size(); ++i) {
        if (breakpoints_[i] <= breakpoints_[i - 1]) {
            throw igesio::EntityValueError("Breakpoints must be in strictly"
                " increasing order, but T(" + std::to_string(i) + ") = "
                + std::to_string(breakpoints_[i]) + " is not greater than T("
                + std::to_string(i - 1) + ") = "
                + std::to_string(breakpoints_[i - 1]) + ".");
        }
    }

    // 各セグメントにおける多項式係数の数
    if (coefficients_.cols() != 4 * n_segments) {
        errors.emplace_back("Number of polynomial coefficients must be 4*N = "
            + std::to_string(4 * n_segments) + ", but got "
            + std::to_string(coefficients_.cols()) + ".");
    }
    for (unsigned int i = 0; i < n_segments; ++i) {
        bool is_all_zero = true;
        for (unsigned int j = 0; j < 3; ++j) {
            auto b_ij = coefficients_(j, i * 4 + 1);
            auto c_ij = coefficients_(j, i * 4 + 2);
            auto d_ij = coefficients_(j, i * 4 + 3);

            if (b_ij != 0.0 || c_ij != 0.0 || d_ij != 0.0) {
                is_all_zero = false;
            }
        }

        // 縮退を防ぐため、x,y,zのいずれかのB, C, Dが非ゼロであること
        if (is_all_zero) {
            errors.emplace_back("At least one of B, C, or D coefficients"
                " must not be zero in each segment to avoid degeneration,"
                " but got B, C, D all zero in segment "
                + std::to_string(i + 1) + ".");
        }
    }
    // NDIM = 2の場合、Bz, Cz, Dzはすべて0かつ、Azはすべて同じ値であること
    if (n_dim_ == 2) {
        double az_value = coefficients_(2, 0);
        for (unsigned int i = 0; i < n_segments; ++i) {
            if (coefficients_(2, i * 4 + 1) != 0.0 ||
                coefficients_(2, i * 4 + 2) != 0.0 ||
                coefficients_(2, i * 4 + 3) != 0.0) {
                errors.emplace_back("For NDIM=2, Bz, Cz, and Dz"
                    " coefficients must be zero in all segments, but got"
                    " non-zero value in segment " + std::to_string(i + 1) + ".");
            }
            if (!i_num::IsApproxEqual(coefficients_(2, i * 4 + 0), az_value)) {
                errors.emplace_back("For NDIM=2, Az coefficients"
                    " must be the same in all segments, but got different"
                    " values in segment " + std::to_string(i + 1) + ".");
            }
        }
    }

    // 各ブレークポイントにおける連続性、接線、曲率の連続性チェック
    for (unsigned int k = 1; k < n_segments; ++k) {
        double t_k = breakpoints_[k];

        // セグメント k-1 末端における関数値、1次導関数値、2次導関数値
        auto s_m1 = t_k - breakpoints_[k - 1];
        auto coeffs_k_m1 = Coefficients(k - 1);
        auto value_k_m1 = ComputeSegmentValue(coeffs_k_m1, s_m1, 0);
        auto first_derivative_k_m1 = ComputeSegmentValue(coeffs_k_m1, s_m1, 1);
        auto second_derivative_k_m1 = ComputeSegmentValue(coeffs_k_m1, s_m1, 2);

        // セグメント k 先頭における関数値、1次導関数値、2次導関数値
        auto coeffs_k = Coefficients(k);
        auto value_k = ComputeSegmentValue(coeffs_k, 0, 0);
        auto first_derivative_k = ComputeSegmentValue(coeffs_k, 0, 1);
        auto second_derivative_k = ComputeSegmentValue(coeffs_k, 0, 2);

        // 関数値・接線・曲率の連続性は幾何的品質の指摘 (kWarning) とし描画は
        // ブロックしない (隙間/角があっても折れ線として描画できる)。
        // 関数値の連続性
        if (!i_num::IsApproxEqual(value_k_m1, value_k)) {
            errors.emplace_back("Discontinuity in function value at breakpoint T("
                + std::to_string(k + 1) + ") = " + std::to_string(t_k) + "; "
                + igesio::ToString(value_k_m1, true) + " != "
                + igesio::ToString(value_k, true) + ".",
                igesio::ValidationSeverity::kWarning);
        }

        // 接線の連続性 (H >= 1)
        if (degree_ >= 1) {
            if (!i_num::IsApproxEqual(first_derivative_k_m1, first_derivative_k)) {
                errors.emplace_back("Discontinuity in tangent vector at breakpoint T("
                    + std::to_string(k + 1) + ") = " + std::to_string(t_k)
                    + "; " + igesio::ToString(first_derivative_k_m1, true) + " != "
                    + igesio::ToString(first_derivative_k, true) + " for degree H >= 1.",
                    igesio::ValidationSeverity::kWarning);
            }
        }
        // 曲率の連続性 (H >= 2)
        if (degree_ >= 2) {
            if (!i_num::IsApproxEqual(second_derivative_k_m1, second_derivative_k)) {
                errors.emplace_back("Discontinuity in curvature vector at breakpoint T("
                    + std::to_string(k + 1) + ") = " + std::to_string(t_k)
                    + "; " + igesio::ToString(second_derivative_k_m1, true) + " != "
                    + igesio::ToString(second_derivative_k, true) + " for degree H >= 2.",
                    igesio::ValidationSeverity::kWarning);
            }
        }
    }

    return MakeValidationResult(std::move(errors));
}



/**
 * 直線部・角点サポート
 */

std::vector<std::array<double, 2>>
ParametricSplineCurve::GetLinearSegments() const {
    // kLinearのときのみ全セグメントが直線
    if (curve_type_ != ParametricSplineCurveType::kLinear) return {};
    if (breakpoints_.size() < 2) return {};

    std::vector<std::array<double, 2>> segments;
    segments.reserve(breakpoints_.size() - 1);
    for (size_t i = 0; i + 1 < breakpoints_.size(); ++i) {
        segments.push_back({breakpoints_[i], breakpoints_[i + 1]});
    }
    return segments;
}

std::vector<double> ParametricSplineCurve::GetCornerParams() const {
    // H=0 (C^0連続のみ) の場合は内部ブレークポイントが角点
    if (degree_ != 0) return {};
    if (breakpoints_.size() < 3) return {};

    // 内部ブレークポイント (先頭・末尾を除く)
    std::vector<double> corners(breakpoints_.begin() + 1,
                                breakpoints_.end() - 1);
    return corners;
}



/**
 * ICurve implementation
 */

std::array<double, 2> ParametricSplineCurve::GetParameterRange() const {
    if (breakpoints_.empty()) return {0.0, 0.0};
    return {breakpoints_.front(), breakpoints_.back()};
}

bool ParametricSplineCurve::IsClosed() const {
    auto start = TryGetDefinedStartPoint();
    auto end = TryGetDefinedEndPoint();
    if (start && end) {
        return i_num::IsApproxEqual(*start, *end);
    }
    return false;
}

std::optional<i_ent::CurveDerivatives>
ParametricSplineCurve::TryGetDefinedDerivatives(const double t, const unsigned int n) const {
    // tが定義域内にあるか確認
    auto i_s_ptr = FindSegmentIndex(t);
    if (!i_s_ptr) return std::nullopt;
    auto [i, s] = *i_s_ptr;

    CurveDerivatives result(n);
    for (unsigned int k = 0; k <= n; ++k) {
        result[k] = ComputeSegmentValue(Coefficients(i), s, k);
    }

    return result;
}

i_num::BoundingBox ParametricSplineCurve::GetDefinedBoundingBox() const {
    // 計算の詳細については[docs/entities/curves/112_parametric_spline_curve_ja.md]を参照
    auto inf = std::numeric_limits<double>::infinity();
    Vector3d min_pt(inf, inf, inf),  max_pt(-inf, -inf, -inf);

    // 各セグメントごとに、始点/終点の座標値と、極値の座標値を計算
    Matrix34d coef;
    const unsigned int n_segments = NumberOfSegments();
    for (unsigned int i = 0; i < n_segments; ++i) {
        coef = Coefficients(i);

        // セグメントの始点
        auto start_pt = ComputeSegmentValue(coef, 0.0, 0);
        min_pt = min_pt.cwiseMin(start_pt);
        max_pt = max_pt.cwiseMax(start_pt);

        // 各座標軸ごとに極値を計算
        for (unsigned int j = 0; j < 3; ++j) {
            const double b = coef(j, 1);
            const double c = coef(j, 2);
            const double d = coef(j, 3);
            const double segment_length = breakpoints_[i + 1] - breakpoints_[i];

            if (d != 0.0) {
                // 3次式: 導関数が2次式 3ds^2 + 2cs + b = 0
                const double discriminant = c * c - 3.0 * b * d;
                if (discriminant >= 0.0) {
                    const double sqrt_d = std::sqrt(discriminant);
                    const double s1 = (-c + sqrt_d) / (3.0 * d);
                    const double s2 = (-c - sqrt_d) / (3.0 * d);

                    if (s1 > 0.0 && s1 < segment_length) {
                        auto pt = ComputeSegmentValue(coef, s1, 0);
                        min_pt = min_pt.cwiseMin(pt);
                        max_pt = max_pt.cwiseMax(pt);
                    }
                    if (s2 > 0.0 && s2 < segment_length) {
                        auto pt = ComputeSegmentValue(coef, s2, 0);
                        min_pt = min_pt.cwiseMin(pt);
                        max_pt = max_pt.cwiseMax(pt);
                    }
                }
            } else if (c != 0.0) {
                // 2次式: 導関数が1次式 2cs + b = 0
                const double s = -b / (2.0 * c);
                if (s > 0.0 && s < segment_length) {
                    auto pt = ComputeSegmentValue(coef, s, 0);
                    min_pt = min_pt.cwiseMin(pt);
                    max_pt = max_pt.cwiseMax(pt);
                }
            }
            // 1次式または定数の場合、極値は区間内部にない
        }
    }
    // 最後のセグメントの終点
    auto end_pt = ComputeSegmentValue(coef,
            breakpoints_.back() - breakpoints_[n_segments - 1], 0);
    min_pt = min_pt.cwiseMin(end_pt);
    max_pt = max_pt.cwiseMax(end_pt);

    return i_num::BoundingBox(min_pt, max_pt);
}



/**
 * private methods
 */

std::optional<std::pair<unsigned int, double>>
ParametricSplineCurve::FindSegmentIndex(const double t) const {
    // tが定義域内にあるか確認
    if (i_num::IsApproxLessThan(t, GetParameterRange()[0])
        || i_num::IsApproxGreaterThan(t, GetParameterRange()[1])) {
        return std::nullopt;
    }

    // tがブレークポイント T(k+1) に一致する場合は、セグメント k を返す
    for (unsigned int i = 0; i < NumberOfSegments(); ++i) {
        if (i_num::IsApproxEqual(t, breakpoints_[i + 1])) {
            double s = t - breakpoints_[i];
            return std::make_pair(i, s);
        }
        if (t < breakpoints_[i + 1]) {
            double s = t - breakpoints_[i];
            return std::make_pair(i, s);
        }
    }

    // ここには到達しないはず
    return std::nullopt;
}


/**
 * ファクトリ関数
 */

std::shared_ptr<ParametricSplineCurve> i_ent::MakeParametricSplineCurve(
        const ParametricSplineCurveType curve_type, const unsigned int degree,
        const std::vector<double>& breakpoints,
        const std::vector<Matrix34d>& coefficients,
        const unsigned int n_dim) {
    return std::make_shared<ParametricSplineCurve>(
            curve_type, degree, breakpoints, coefficients, n_dim);
}

std::shared_ptr<ParametricSplineCurve> i_ent::MakeCubicSplineCurve(
        const std::vector<Vector3d>& points,
        const std::vector<Vector3d>& tangents,
        const std::vector<double>& breakpoints) {
    if (points.size() < 2) {
        throw igesio::EntityValueError(
                "MakeCubicSplineCurve: At least two points are required.");
    }
    if (tangents.size() != points.size() ||
        breakpoints.size() != points.size()) {
        throw igesio::EntityValueError(
                "MakeCubicSplineCurve: points, tangents and breakpoints must"
                " have the same size.");
    }

    // 各セグメントでエルミート基底からべき基底へ変換する
    std::vector<Matrix34d> coefficients;
    coefficients.reserve(points.size() - 1);
    for (size_t i = 0; i + 1 < points.size(); ++i) {
        const double h = breakpoints[i + 1] - breakpoints[i];
        if (h <= 0.0) {
            throw igesio::EntityValueError(
                    "MakeCubicSplineCurve: Breakpoints must be in strictly"
                    " increasing order.");
        }
        const Vector3d dp = points[i + 1] - points[i];
        Matrix34d segment;
        segment.col(0) = points[i];    // A: 始点
        segment.col(1) = tangents[i];  // B: 始点接線
        // C, D: 終点の位置・接線の一致条件から決まる係数
        segment.col(2) = 3.0 * dp / (h * h)
                - (2.0 * tangents[i] + tangents[i + 1]) / h;
        segment.col(3) = -2.0 * dp / (h * h * h)
                + (tangents[i] + tangents[i + 1]) / (h * h);
        coefficients.push_back(segment);
    }

    // 隣接セグメントが通過点の接線を共有するため、勾配連続 (H=1)
    return MakeParametricSplineCurve(
            ParametricSplineCurveType::kCubic, 1, breakpoints, coefficients);
}
