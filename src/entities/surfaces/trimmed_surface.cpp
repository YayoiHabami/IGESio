/**
 * @file entities/surfaces/trimmed_surface.cpp
 * @brief TrimmedSurface (Type 144): トリム済みパラメトリック曲面エンティティの実装
 * @author Yayoi Habami
 * @date 2026-04-13
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/entities/surfaces/trimmed_surface.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/common/iges_parameter_vector.h"
#include "igesio/entities/curves/algorithms.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using i_ent::TrimmedSurface;
using igesio::Vector3d;

}  // namespace



/**
 * コンストラクタ
 */

TrimmedSurface::TrimmedSurface(
        const RawEntityDE& de_record, const IGESParameterVector& parameters,
        const pointer2ID& de2id, const ObjectID& iges_id)
        : EntityBase(de_record, parameters, de2id, iges_id),
          surface_(IDGenerator::UnsetID()),
          outer_boundary_(IDGenerator::UnsetID()) {
    InitializePD(de2id);
}

TrimmedSurface::TrimmedSurface(
        const std::shared_ptr<ISurface>& surface,
        const std::shared_ptr<CurveOnAParametricSurface>& outer)
        : TrimmedSurface(
            RawEntityDE::ByDefault(EntityType::kTrimmedSurface),
            IGESParameterVector{
                surface->GetID(),
                static_cast<int>(outer ? 1 : 0),
                0,
                outer ? outer->GetID() : IDGenerator::UnsetID()
            }, {}) {
    if (!surface) {
        throw std::invalid_argument(
                "TrimmedSurface: surface must not be nullptr.");
    }
    SetSurface(surface);
    if (outer) {
        SetOuterBoundary(outer);
    }
}



/**
 * EntityBase implementation
 */

igesio::IGESParameterVector TrimmedSurface::GetMainPDParameters() const {
    IGESParameterVector params;
    params.push_back(surface_.GetID());
    params.push_back(static_cast<int>(outer_is_boundary_of_d_ ? 0 : 1));
    params.push_back(static_cast<int>(inner_boundaries_.size()));
    if (outer_is_boundary_of_d_) {
        // N1=0: PTO はヌルポインタ
        params.push_back(IDGenerator::UnsetID());
    } else {
        params.push_back(outer_boundary_.GetID());
    }
    for (const auto& b : inner_boundaries_) {
        params.push_back(b.GetID());
    }
    return params;
}

size_t TrimmedSurface::SetMainPDParameters(const pointer2ID& de2id) {
    auto& pd = pd_parameters_;

    if (pd.size() < 4) {
        throw igesio::DataFormatError(
                "TrimmedSurface: Insufficient number of parameters.");
    }

    // PTS: トリミング対象の曲面
    try {
        surface_ = PointerContainer<false, ISurface>(
                GetObjectIDFromParameters(pd, 0, de2id));
    } catch (const std::exception& e) {
        throw igesio::DataFormatError(
                "TrimmedSurface: Invalid surface reference. "
                + std::string(e.what()));
    }

    // N1: 外側境界の種別
    outer_is_boundary_of_d_ = (pd.access_as<int>(1) == 0);

    // N2: 内側境界曲線の数
    const auto n2 = static_cast<size_t>(pd.access_as<int>(2));

    // PTO: 外側境界曲線 (N1=0 のときヌルポインタ → allow_unset_id=true)
    try {
        outer_boundary_ = PointerContainer<false, CurveOnAParametricSurface>(
                GetObjectIDFromParameters(pd, 3, de2id, true));
    } catch (const std::exception& e) {
        throw igesio::DataFormatError(
                "TrimmedSurface: Invalid outer boundary reference. "
                + std::string(e.what()));
    }

    // PTI(i): 内側境界曲線
    if (pd.size() < 4 + n2) {
        throw igesio::DataFormatError(
                "TrimmedSurface: Insufficient parameters for inner boundaries.");
    }
    inner_boundaries_.clear();
    inner_boundaries_.reserve(n2);
    for (size_t i = 0; i < n2; ++i) {
        try {
            inner_boundaries_.emplace_back(
                PointerContainer<false, CurveOnAParametricSurface>(
                    GetObjectIDFromParameters(pd, 4 + i, de2id)));
        } catch (const std::exception& e) {
            throw igesio::DataFormatError(
                    "TrimmedSurface: Invalid inner boundary[" + std::to_string(i)
                    + "] reference. " + std::string(e.what()));
        }
    }

    return 4 + n2;
}

std::unordered_set<igesio::ObjectID>
TrimmedSurface::GetUnresolvedPDReferences() const {
    std::unordered_set<ObjectID> unresolved;

    if (!surface_.IsPointerSet()) {
        unresolved.insert(surface_.GetID());
    }
    // N1=1 のときのみ outer_boundary_ を解決対象とする
    // (N1=0 のとき outer_boundary_ の ID は UnsetID であり解決不要)
    if (!outer_is_boundary_of_d_ && !outer_boundary_.IsPointerSet()) {
        unresolved.insert(outer_boundary_.GetID());
    }
    for (const auto& inner : inner_boundaries_) {
        if (!inner.IsPointerSet()) {
            unresolved.insert(inner.GetID());
        }
    }

    return unresolved;
}

bool TrimmedSurface::SetUnresolvedPDReferences(
        const std::shared_ptr<const EntityBase>& entity) {
    if (!entity) {
        return false;
    }

    // surface_
    if (entity->GetID() == surface_.GetID() && !surface_.IsPointerSet()) {
        if (auto ptr = std::dynamic_pointer_cast<const ISurface>(entity)) {
            return surface_.SetPointer(ptr);
        }
    }

    // outer_boundary_ (N1=1 のときのみ)
    if (!outer_is_boundary_of_d_
            && entity->GetID() == outer_boundary_.GetID()
            && !outer_boundary_.IsPointerSet()) {
        if (auto ptr = std::dynamic_pointer_cast<
                const CurveOnAParametricSurface>(entity)) {
            return outer_boundary_.SetPointer(ptr);
        }
    }

    // inner_boundaries_
    for (auto& inner : inner_boundaries_) {
        if (entity->GetID() == inner.GetID() && !inner.IsPointerSet()) {
            if (auto ptr = std::dynamic_pointer_cast<
                    const CurveOnAParametricSurface>(entity)) {
                return inner.SetPointer(ptr);
            }
        }
    }

    return false;
}

std::vector<igesio::ObjectID> TrimmedSurface::GetChildIDs() const {
    std::vector<ObjectID> ids;
    ids.push_back(surface_.GetID());
    ids.push_back(outer_boundary_.GetID());
    for (const auto& inner : inner_boundaries_) {
        ids.push_back(inner.GetID());
    }
    return ids;
}

std::shared_ptr<const i_ent::EntityBase>
TrimmedSurface::GetChildEntity(const ObjectID& id) const {
    if (surface_.GetID() == id) {
        auto ptr = surface_.TryGetEntity<EntityBase>();
        if (ptr) return ptr.value();
    }
    if (outer_boundary_.GetID() == id) {
        auto ptr = outer_boundary_.TryGetEntity<EntityBase>();
        if (ptr) return ptr.value();
    }
    for (const auto& inner : inner_boundaries_) {
        if (inner.GetID() == id) {
            auto ptr = inner.TryGetEntity<EntityBase>();
            if (ptr) return ptr.value();
        }
    }
    return nullptr;
}

igesio::ValidationResult TrimmedSurface::ValidatePD() const {
    std::vector<ValidationError> errors;

    // surface_
    if (!surface_.IsPointerSet()) {
        errors.emplace_back("Surface pointer is not set.");
    } else {
        auto surf_opt = surface_.TryGetEntity<EntityBase>();
        if (!surf_opt) {
            errors.emplace_back("Surface entity (ID: ")
                << ToString(surface_.GetID()) << ") is not an EntityBase.";
        } else {
            auto result = surf_opt.value()->Validate();
            if (!result.is_valid) {
                errors.emplace_back("Surface entity (ID: ")
                    << ToString(surface_.GetID())
                    << ") is invalid: " << result.Message();
            }
        }
    }

    // outer_boundary_ (N1=1 のとき必須)
    if (!outer_is_boundary_of_d_) {
        if (!outer_boundary_.IsPointerSet()) {
            errors.emplace_back("Outer boundary pointer is not set (N1=1).");
        } else {
            auto ob_opt = outer_boundary_.TryGetEntity<EntityBase>();
            if (!ob_opt) {
                errors.emplace_back("Outer boundary entity (ID: ")
                    << ToString(outer_boundary_.GetID())
                    << ") is not an EntityBase.";
            } else {
                auto result = ob_opt.value()->Validate();
                if (!result.is_valid) {
                    errors.emplace_back("Outer boundary entity (ID: ")
                        << ToString(outer_boundary_.GetID())
                        << ") is invalid: " << result.Message();
                }
            }
        }
    } else {
        // N1=0 のとき outer_boundary_ のIDは UnsetID であるべき
        if (outer_boundary_.GetID() != IDGenerator::UnsetID()) {
            errors.emplace_back(
                    "Outer boundary ID must be unset when N1=0 "
                    "(outer_is_boundary_of_d=true).");
        }
    }

    // inner_boundaries_
    for (size_t i = 0; i < inner_boundaries_.size(); ++i) {
        const auto& inner = inner_boundaries_[i];
        if (!inner.IsPointerSet()) {
            errors.emplace_back("Inner boundary[")
                << std::to_string(i) << "] pointer is not set.";
        } else {
            auto ib_opt = inner.TryGetEntity<EntityBase>();
            if (!ib_opt) {
                errors.emplace_back("Inner boundary[")
                    << std::to_string(i) << "] entity (ID: "
                    << ToString(inner.GetID()) << ") is not an EntityBase.";
            } else {
                auto result = ib_opt.value()->Validate();
                if (!result.is_valid) {
                    errors.emplace_back("Inner boundary[")
                        << std::to_string(i) << "] entity (ID: "
                        << ToString(inner.GetID())
                        << ") is invalid: " << result.Message();
                }
            }
        }
    }

    return MakeValidationResult(errors);
}



/**
 * ISurface implementation
 */

bool TrimmedSurface::IsUClosed() const {
    auto surf_opt = surface_.TryGetEntity<ISurface>();
    if (!surf_opt) return false;
    return surf_opt.value()->IsUClosed();
}

bool TrimmedSurface::IsVClosed() const {
    auto surf_opt = surface_.TryGetEntity<ISurface>();
    if (!surf_opt) return false;
    return surf_opt.value()->IsVClosed();
}

std::array<double, 4> TrimmedSurface::GetParameterRange() const {
    auto surf_opt = surface_.TryGetEntity<ISurface>();
    if (!surf_opt) return {0.0, 1.0, 0.0, 1.0};
    return surf_opt.value()->GetParameterRange();
}

std::optional<i_ent::SurfaceDerivatives>
TrimmedSurface::TryGetDerivatives(
        const double u, const double v, const unsigned int order) const {
    // トリム領域外は nullopt
    if (!IsInTrimmedDomain(u, v)) return std::nullopt;

    auto surf_opt = surface_.TryGetEntity<ISurface>();
    if (!surf_opt) return std::nullopt;
    return surf_opt.value()->TryGetDerivatives(u, v, order);
}

i_num::BoundingBox TrimmedSurface::GetDefinedBoundingBox() const {
    auto surf_opt = surface_.TryGetEntity<ISurface>();
    if (!surf_opt) return i_num::BoundingBox();
    return surf_opt.value()->GetDefinedBoundingBox();
}



/**
 * 構成要素の操作
 */

std::shared_ptr<const i_ent::ISurface> TrimmedSurface::GetSurface() const {
    auto surf_opt = surface_.TryGetEntity<ISurface>();
    if (!surf_opt) {
        throw std::runtime_error(
                "TrimmedSurface: Surface pointer is not set or invalid.");
    }
    return surf_opt.value();
}

void TrimmedSurface::SetSurface(const std::shared_ptr<ISurface>& surface) {
    if (!surface) {
        throw std::invalid_argument(
                "TrimmedSurface: surface must not be nullptr.");
    }
    if (surface->GetID() == surface_.GetID()) {
        surface_.SetPointer(surface);
    } else {
        surface_.OverwritePointer(surface);
    }
    if (auto eb = std::dynamic_pointer_cast<EntityBase>(surface)) {
        eb->SetSubordinateEntitySwitch(
                SubordinateEntitySwitch::kPhysicallyDependent);
    }
    domain_cache_ = std::nullopt;
}

std::shared_ptr<const i_ent::CurveOnAParametricSurface>
TrimmedSurface::GetOuterBoundary() const {
    if (outer_is_boundary_of_d_) return nullptr;
    auto ob_opt = outer_boundary_.TryGetEntity<CurveOnAParametricSurface>();
    if (!ob_opt) return nullptr;
    return ob_opt.value();
}

void TrimmedSurface::SetOuterBoundary(
        const std::shared_ptr<CurveOnAParametricSurface>& outer) {
    if (!outer) {
        // N1=0: Dの境界を外側境界として使用する
        outer_is_boundary_of_d_ = true;
        outer_boundary_ = PointerContainer<false, CurveOnAParametricSurface>(
                IDGenerator::UnsetID());
        domain_cache_ = std::nullopt;
        return;
    }
    outer_is_boundary_of_d_ = false;
    if (outer->GetID() == outer_boundary_.GetID()) {
        outer_boundary_.SetPointer(outer);
    } else {
        outer_boundary_.OverwritePointer(outer);
    }
    if (auto eb = std::dynamic_pointer_cast<EntityBase>(outer)) {
        eb->SetSubordinateEntitySwitch(
                SubordinateEntitySwitch::kPhysicallyDependent);
    }
    domain_cache_ = std::nullopt;
}

std::shared_ptr<const i_ent::CurveOnAParametricSurface>
TrimmedSurface::GetInnerBoundaryAt(const size_t index) const {
    if (index >= inner_boundaries_.size()) {
        throw std::out_of_range(
                "TrimmedSurface: Inner boundary index " + std::to_string(index)
                + " is out of range (size=" + std::to_string(inner_boundaries_.size())
                + ").");
    }
    auto ib_opt = inner_boundaries_[index]
            .TryGetEntity<CurveOnAParametricSurface>();
    if (!ib_opt) return nullptr;
    return ib_opt.value();
}

void TrimmedSurface::AddInnerBoundary(
        const std::shared_ptr<CurveOnAParametricSurface>& boundary) {
    if (!boundary) {
        throw std::invalid_argument(
                "TrimmedSurface: boundary must not be nullptr.");
    }
    inner_boundaries_.emplace_back(IDGenerator::UnsetID());
    inner_boundaries_.back().OverwritePointer(boundary);
    if (auto eb = std::dynamic_pointer_cast<EntityBase>(boundary)) {
        eb->SetSubordinateEntitySwitch(
                SubordinateEntitySwitch::kPhysicallyDependent);
    }
    domain_cache_ = std::nullopt;
}

void TrimmedSurface::RemoveInnerBoundaryAt(size_t index) {
    if (index >= inner_boundaries_.size()) {
        throw std::out_of_range(
                "TrimmedSurface: Inner boundary index " + std::to_string(index)
                + " is out of range (size=" + std::to_string(inner_boundaries_.size())
                + ").");
    }
    inner_boundaries_.erase(inner_boundaries_.begin()
                            + static_cast<std::ptrdiff_t>(index));
    domain_cache_ = std::nullopt;
}



/**
 * private methods
 */

void TrimmedSurface::BuildDomainCache() const {
    if (domain_cache_) return;

    DomainCache cache;

    // 外側境界 (N1=1 のとき)
    if (!outer_is_boundary_of_d_) {
        auto ob_opt = outer_boundary_.TryGetEntity<CurveOnAParametricSurface>();
        if (ob_opt) {
            cache.outer = i_ent::ComputeContainmentPolygons(
                    *ob_opt.value()->GetBaseCurve(),
                    kContainmentPolygonDivisions,
                    Vector3d(0, 0, 1));
        }
    }

    // 内側境界
    for (const auto& inner : inner_boundaries_) {
        auto ib_opt = inner.TryGetEntity<CurveOnAParametricSurface>();
        if (ib_opt) {
            cache.inner.push_back(i_ent::ComputeContainmentPolygons(
                    *ib_opt.value()->GetBaseCurve(),
                    kContainmentPolygonDivisions,
                    Vector3d(0, 0, 1)));
        }
    }

    domain_cache_ = std::move(cache);
}

bool TrimmedSurface::IsInTrimmedDomain(const double u, const double v) const {
    // 最速パス: 境界なし
    if (outer_is_boundary_of_d_ && inner_boundaries_.empty()) return true;

    BuildDomainCache();

    const Vector3d pt(u, v, 0);

    // 外側境界チェック (N1=1 かつキャッシュが有効なとき)
    if (!outer_is_boundary_of_d_ && domain_cache_->outer) {
        if (!i_num::IsPointInPolygon(pt, *domain_cache_->outer)) return false;
    }

    // 内側境界チェック (穴の内部なら false)
    for (const auto& inner_poly : domain_cache_->inner) {
        if (i_num::IsPointInPolygon(pt, inner_poly)) return false;
    }

    return true;
}
