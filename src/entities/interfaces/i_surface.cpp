/**
 * @file entities/interfaces/i_surface.cpp
 * @brief 曲面クラスのインターフェース定義
 * @author Yayoi Habami
 * @date 2025-08-10
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/interfaces/i_surface.h"

#include <utility>

#include "igesio/common/tolerance.h"

namespace {

namespace i_ent = igesio::entities;

using igesio::Vector2d;
using igesio::Vector3d;
using i_ent::SurfaceDerivatives;
using i_ent::ISurface;

}  // namespace



/**
 * SurfaceDerivativesの実装
 */

SurfaceDerivatives::SurfaceDerivatives(const unsigned int order)
    : order_(order) {
    Resize(order);
}

const Vector3d& SurfaceDerivatives::operator()(
        const unsigned int i, const unsigned int j) const {
    if (i + j > order_) {
        throw std::out_of_range("SurfaceDerivatives: i + j exceeds order."
            " i = " + std::to_string(i) + ", j = " + std::to_string(j) +
            ", order = " +  std::to_string(order_));
    }
    return derivatives_[i][j];
}

Vector3d& SurfaceDerivatives::operator()(
        const unsigned int i, const unsigned int j) {
    if (i + j > order_) {
        throw std::out_of_range("SurfaceDerivatives: i + j exceeds order."
            " i = " + std::to_string(i) + ", j = " + std::to_string(j) +
            ", order = " +  std::to_string(order_));
    }
    return derivatives_[i][j];
}

void SurfaceDerivatives::Resize(const unsigned int order) {
    order_ = order;
    derivatives_.resize(order_ + 1);
    for (unsigned int i = 0; i <= order_; ++i) {
        derivatives_[i].resize(order_ - i + 1, Vector3d::Zero());
    }
}



/**
 * 基本的な情報の取得 (ISurface)
 */

std::array<double, 2> ISurface::GetURange() const {
    const auto range = GetParameterRange();
    return {range[0], range[1]};
}

std::array<double, 2> ISurface::GetVRange() const {
    const auto range = GetParameterRange();
    return {range[2], range[3]};
}

bool ISurface::HasFiniteUStart() const {
    return std::isfinite(GetURange()[0]);
}

bool ISurface::HasFiniteUEnd() const {
    return std::isfinite(GetURange()[1]);
}

bool ISurface::HasFiniteVStart() const {
    return std::isfinite(GetVRange()[0]);
}

bool ISurface::HasFiniteVEnd() const {
    return std::isfinite(GetVRange()[1]);
}

bool ISurface::IsFinite() const {
    return HasFiniteUStart() && HasFiniteUEnd() &&
           HasFiniteVStart() && HasFiniteVEnd();
}



/**
 * 曲面の幾何学的情報 (スカラー) を取得する
 */

std::optional<std::tuple<double, double, double>>
ISurface::TryGetFirstFundamentalForm(const double u, const double v) const {
    // 偏導関数を取得
    auto derivatives = TryGetDerivatives(u, v, 1);
    if (!derivatives) return std::nullopt;

    const auto& Su = (*derivatives)(1, 0);
    const auto& Sv = (*derivatives)(0, 1);

    // E, F, Gを計算して返す
    return std::make_tuple(Su.dot(Su), Su.dot(Sv), Sv.dot(Sv));
}

std::optional<std::tuple<double, double, double>>
ISurface::TryGetSecondFundamentalForm(const double u, const double v) const {
    // 偏導関数を取得
    auto derivatives = TryGetDerivatives(u, v, 2);
    if (!derivatives) return std::nullopt;

    const auto& Su = (*derivatives)(1, 0);
    const auto& Sv = (*derivatives)(0, 1);
    const auto& Suu = (*derivatives)(2, 0);
    const auto& Suv = (*derivatives)(1, 1);
    const auto& Svv = (*derivatives)(0, 2);

    // 法線ベクトルを計算
    auto N = Su.cross(Sv).normalized();
    if (IsApproxZero(N.norm(), kGeometryTolerance)) {
        // 正規化できない（接ベクトルが線形従属）場合は計算不可
        return std::nullopt;
    }

    // L, M, Nを計算して返す
    return std::make_tuple(Suu.dot(N), Suv.dot(N), Svv.dot(N));
}

std::optional<double>
ISurface::TryGetGaussianCurvature(const double u, const double v) const {
    // 第一基本形式と第二基本形式を取得
    auto first_form = TryGetFirstFundamentalForm(u, v);
    auto second_form = TryGetSecondFundamentalForm(u, v);
    if (!first_form || !second_form) return std::nullopt;
    const auto [E, F, G] = *first_form;
    const auto [L, M, N] = *second_form;

    const double denominator = E * G - F * F;
    if (IsApproxZero(denominator, kGeometryTolerance)) {
        // 第一基本形式の行列式が0の場合、ガウス曲率は定義されない
        return std::nullopt;
    }
    return (L * N - M * M) / denominator;
}

std::optional<double>
ISurface::TryGetMeanCurvature(const double u, const double v) const {
    // 第一基本形式と第二基本形式を取得
    auto first_form = TryGetFirstFundamentalForm(u, v);
    auto second_form = TryGetSecondFundamentalForm(u, v);
    if (!first_form || !second_form) return std::nullopt;
    const auto [E, F, G] = *first_form;
    const auto [L, M, N] = *second_form;

    const double denominator = E * G - F * F;
    if (IsApproxZero(denominator, kGeometryTolerance)) {
        // 第一基本形式の行列式が0の場合、平均曲率は定義されない
        return std::nullopt;
    }
    return (E * N - 2 * F * M + G * L) / (2 * denominator);
}

std::optional<std::pair<double, double>>
ISurface::TryGetPrincipalCurvatures(
        const double u, const double v) const {
    // ガウス曲率と平均曲率を取得
    auto K_opt = TryGetGaussianCurvature(u, v);
    auto H_opt = TryGetMeanCurvature(u, v);
    if (!K_opt || !H_opt) return std::nullopt;
    const double K = *K_opt;
    const double H = *H_opt;

    // 主曲率を計算
    const double discriminant = H * H - K;
    if (discriminant < 0) {
        // 判別式が負の場合、主曲率は実数として定義されない
        return std::nullopt;
    }

    const double sqrt_discriminant = std::sqrt(discriminant);
    const double k1 = H + sqrt_discriminant;
    const double k2 = H - sqrt_discriminant;

    return std::make_pair(k1, k2);
}



/**
 * 曲面の幾何学的情報 (ベクトル) を取得する
 */

std::optional<Vector3d>
ISurface::TryGetDefinedPointAt(const double u, const double v) const {
    // 偏導関数を取得
    auto derivatives = TryGetDerivatives(u, v, 0);
    if (!derivatives) return std::nullopt;

    return (*derivatives)(0, 0);
}

std::optional<std::pair<Vector3d, Vector3d>>
ISurface::TryGetDefinedTangentAt(const double u, const double v) const {
    // 偏導関数を取得
    auto derivatives = TryGetDerivatives(u, v, 1);
    if (!derivatives) return std::nullopt;

    const auto& Su = (*derivatives)(1, 0);
    const auto& Sv = (*derivatives)(0, 1);

    return std::make_pair(Su.normalized(), Sv.normalized());
}

std::optional<Vector3d>
ISurface::TryGetDefinedNormalAt(const double u, const double v) const {
    // 偏導関数を取得
    auto derivatives = TryGetDerivatives(u, v, 1);
    if (!derivatives) return std::nullopt;

    const auto& Su = (*derivatives)(1, 0);
    const auto& Sv = (*derivatives)(0, 1);

    // 法線ベクトルを計算
    auto N = Su.cross(Sv);
    if (IsApproxZero(N.norm(), kGeometryTolerance)) {
        // 正規化できない（接ベクトルが線形従属）場合は計算不可
        return std::nullopt;
    }
    return N.normalized();
}

std::optional<Vector3d>
ISurface::TryGetPointAt(const double u, const double v) const {
    return Transform(TryGetDefinedPointAt(u, v), true);
}

std::optional<std::pair<Vector3d, Vector3d>>
ISurface::TryGetTangentAt(const double u, const double v) const {
    auto tangents = TryGetDefinedTangentAt(u, v);
    if (!tangents) return std::nullopt;

    // 変換を適用
    return std::make_pair(
            Transform(tangents->first, false).value(),
            Transform(tangents->second, false).value());
}

std::optional<Vector3d>
ISurface::TryGetNormalAt(const double u, const double v) const {
    return Transform(TryGetDefinedNormalAt(u, v), false);
}

Vector3d ISurface::GetPointAt(const double u, const double v) const {
    auto point = TryGetPointAt(u, v);
    if (!point) {
        throw std::out_of_range("Parameter (u, v) = (" +
            std::to_string(u) + ", " + std::to_string(v) +
            ") is out of range for the surface.");
    }
    return *point;
}

std::pair<Vector3d, Vector3d> ISurface::GetTangentAt(
        const double u, const double v) const {
    auto tangents = TryGetTangentAt(u, v);
    if (!tangents) {
        throw std::out_of_range("Parameter (u, v) = (" +
            std::to_string(u) + ", " + std::to_string(v) +
            ") is out of range for the surface.");
    }
    return *tangents;
}

Vector3d ISurface::GetNormalAt(const double u, const double v) const {
    auto normal = TryGetNormalAt(u, v);
    if (!normal) {
        throw std::out_of_range("Parameter (u, v) = (" +
            std::to_string(u) + ", " + std::to_string(v) +
            ") is out of range for the surface.");
    }
    return *normal;
}

