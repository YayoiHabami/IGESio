/**
 * @file entities/interfaces/i_restricted_surface.cpp
 * @brief パラメータ範囲の一部が領域外となりうる曲面のインターフェース実装
 * @author Yayoi Habami
 * @date 2026-06-03
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/entities/interfaces/i_restricted_surface.h"

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <utility>

#include "igesio/entities/curves/algorithms.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using igesio::Vector3d;
using i_ent::IRestrictedSurface;
using i_ent::SurfaceDerivatives;

}  // namespace



/**
 * ISurface実装 (基底曲面への委譲＋ドメイン制限)
 */

bool IRestrictedSurface::IsUClosed() const {
    // 外側境界が明示指定の場合、U方向の継ぎ目(seam)をまたぐ閉性を
    // 正確に推定できないため保守的にfalseを返す
    if (!outer_is_boundary_of_d_) return false;
    auto base = GetBaseSurface();
    if (!base) return false;
    return base->IsUClosed();
}

bool IRestrictedSurface::IsVClosed() const {
    if (!outer_is_boundary_of_d_) return false;
    auto base = GetBaseSurface();
    if (!base) return false;
    return base->IsVClosed();
}

std::array<double, 4> IRestrictedSurface::GetParameterRange() const {
    auto base = GetBaseSurface();
    if (!base) return {0.0, 1.0, 0.0, 1.0};
    return base->GetParameterRange();
}

std::optional<SurfaceDerivatives>
IRestrictedSurface::TryGetDefinedDerivatives(
        const double u, const double v, const unsigned int order) const {
    // ドメイン外はnullopt
    if (!IsInDomain(u, v)) return std::nullopt;

    auto base = GetBaseSurface();
    if (!base) return std::nullopt;
    // トリム面の定義空間は基底曲面のモデル空間(M_base適用済み)とする
    return base->TryGetDerivatives(u, v, order);
}

i_num::BoundingBox IRestrictedSurface::GetDefinedBoundingBox() const {
    auto base = GetBaseSurface();
    if (!base) return i_num::BoundingBox();
    return base->GetDefinedBoundingBox();
}

bool IRestrictedSurface::IsInDomain(const double u, const double v) const {
    // 最速パス: 境界なし
    if (outer_is_boundary_of_d_ && GetInnerBoundaryCount() == 0) return true;

    BuildDomainCache();

    const Vector3d pt(u, v, 0);

    // 外側境界チェック (明示指定かつキャッシュが有効なとき)
    if (!outer_is_boundary_of_d_ && domain_cache_->outer) {
        if (!i_num::IsPointInPolygon(pt, *domain_cache_->outer)) return false;
    }

    // 内側境界チェック (穴の内部ならfalse)
    for (const auto& inner_poly : domain_cache_->inner) {
        if (i_num::IsPointInPolygon(pt, inner_poly)) return false;
    }

    return true;
}



/**
 * 領域情報のアクセサ
 */

const std::optional<i_num::CurveContainmentPolygons>&
IRestrictedSurface::GetOuterDomainPolygon() const {
    BuildDomainCache();
    return domain_cache_->outer;
}

const std::vector<i_num::CurveContainmentPolygons>&
IRestrictedSurface::GetInnerDomainPolygons() const {
    BuildDomainCache();
    return domain_cache_->inner;
}



/**
 * キャッシュの構築
 */

void IRestrictedSurface::BuildDomainCache() const {
    if (domain_cache_) return;

    DomainCache cache;

    // 外側境界 (明示指定のとき)。境界が未解決/退化/非閉でテッセレーションが例外を
    // 投げても処理全体を止めない (グレースフル劣化; 当該境界はスキップ)。
    if (!outer_is_boundary_of_d_) {
        try {
            auto outer = GetOuterUVBoundary();
            if (outer) {
                cache.outer = i_ent::ComputeContainmentPolygons(
                        *outer, kContainmentPolygonDivisions, Vector3d(0, 0, 1));
            }
        } catch (const std::exception&) {
            // 当該境界はスキップ (cache.outerは空のまま)
        }
    }

    // 内側境界 (同上)
    const std::size_t n = GetInnerBoundaryCount();
    cache.inner.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        try {
            auto inner = GetInnerUVBoundaryAt(i);
            if (inner) {
                cache.inner.push_back(i_ent::ComputeContainmentPolygons(
                        *inner, kContainmentPolygonDivisions, Vector3d(0, 0, 1)));
            }
        } catch (const std::exception&) {
            // 当該境界はスキップ
        }
    }

    domain_cache_ = std::move(cache);
}



/**
 * ドメインUV範囲の取得 (自由関数)
 */

std::optional<std::array<double, 4>>
igesio::entities::GetRestrictedDomainUVBounds(
        const IRestrictedSurface& surface, const double margin_ratio) {
    constexpr double kInf = std::numeric_limits<double>::infinity();
    double u_lo = kInf, u_hi = -kInf, v_lo = kInf, v_hi = -kInf;
    bool found = false;

    auto accumulate = [&](const i_num::PolygonData& poly) {
        for (const auto& p : poly.vertices) {
            u_lo = std::min(u_lo, p.x());
            u_hi = std::max(u_hi, p.x());
            v_lo = std::min(v_lo, p.y());
            v_hi = std::max(v_hi, p.y());
            found = true;
        }
    };
    auto accumulate_ccp = [&](const i_num::CurveContainmentPolygons& ccp) {
        accumulate(ccp.circumscribed);  // ドメインを外側から包む多角形を含める
        accumulate(ccp.approximate);
    };

    if (const auto& outer = surface.GetOuterDomainPolygon()) accumulate_ccp(*outer);
    for (const auto& inner : surface.GetInnerDomainPolygons()) accumulate_ccp(inner);

    if (!found || !(u_hi > u_lo) || !(v_hi > v_lo)) return std::nullopt;

    const double um = (u_hi - u_lo) * margin_ratio;
    const double vm = (v_hi - v_lo) * margin_ratio;
    return std::array<double, 4>{u_lo - um, u_hi + um, v_lo - vm, v_hi + vm};
}
