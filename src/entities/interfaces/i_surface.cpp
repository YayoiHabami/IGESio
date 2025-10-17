/**
 * @file entities/interfaces/i_surface.cpp
 * @brief 曲面クラスのインターフェース定義
 * @author Yayoi Habami
 * @date 2025-08-10
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/interfaces/i_surface.h"

namespace {

namespace i_ent = igesio::entities;

using igesio::Vector2d;
using igesio::Vector3d;
using i_ent::ISurface;

}  // namespace



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
 * 曲面の幾何学的情報 (ベクトル) を取得する
 */

Vector3d ISurface::GetPointAt(const double u, const double v) const {
    auto point = TryGetPointAt(u, v);
    if (!point) {
        throw std::out_of_range("Parameter (u, v) = (" +
            std::to_string(u) + ", " + std::to_string(v) +
            ") is out of range for the surface.");
    }
    return *point;
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

