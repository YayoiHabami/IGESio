/**
 * @file entities/curves/curve_on_a_parametric_surface.cpp
 * @brief CurveOnAParametricSurface (Type 142): パラメトリックサーフェス上の曲線エンティティの定義
 * @author Yayoi Habami
 * @date 2025-11-08
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/curves/curve_on_a_parametric_surface.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/numerics/tolerance.h"
#include "igesio/entities/curves/linear_path.h"
#include "igesio/entities/curves/algorithms.h"

namespace {

/// @brief 曲線を離散化する際のトレランス
constexpr double kDiscretizationTol = 1e-6;

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using i_ent::CurveOnSurface;
using igesio::Vector3d;

/// @brief S(u,v) と B(t) から C(t) を生成する
std::shared_ptr<i_ent::ICurve> CreateCurveOnSurface(
        const std::shared_ptr<const i_ent::ISurface>& surface,
        const std::shared_ptr<const i_ent::ICurve>& base_curve) {
    // 曲面上の曲線 C(t) を生成
    auto tol = igesio::numerics::Tolerance(kDiscretizationTol);
    auto vertices = DiscretizeCurve(base_curve, tol);

    // kPolyLineを生成
    auto curve_2d = std::make_shared<i_ent::LinearPath>(vertices);
    // 各頂点を曲面上に射影
    std::vector<Vector3d> projected_vertices;
    for (const auto& uv : vertices) {
        auto pt_opt = surface->TryGetPointAt(uv.x(), uv.y());
        if (!pt_opt) {
            throw std::runtime_error("CurveOnAParametricSurface: Failed to project "
                    "base curve point onto surface.");
        }
        projected_vertices.push_back(pt_opt.value());
    }

    // 投射した頂点から3D曲線を生成
    // TODO: NURBS曲線などでC(t)を生成する. 現在の折れ線による実装では、出力
    //       されるファイルのファイルサイズが非常に大きくなるため
    return std::make_shared<i_ent::LinearPath>(projected_vertices);
}

}  // namespace



/**
 * コンストラクタ
 */

CurveOnSurface::CurveOnAParametricSurface(
        const RawEntityDE& de_record,
        const IGESParameterVector& parameters,
        const pointer2ID& de2id,
        const ObjectID& iges_id)
    : EntityBase(de_record, parameters, de2id, iges_id),
      surface_(IDGenerator::UnsetID()),
      base_curve_(IDGenerator::UnsetID()),
      curve_(IDGenerator::UnsetID()) {
    InitializePD(de2id);
}

CurveOnSurface::CurveOnAParametricSurface(
        const std::shared_ptr<ISurface>& surface,
        const std::shared_ptr<ICurve>& base_curve,
        const std::shared_ptr<ICurve>& curve)
        : CurveOnAParametricSurface(
            RawEntityDE::ByDefault(EntityType::kCurveOnAParametricSurface),
            {static_cast<int>(CurveCreationType::kUnspecified),
             surface->GetID(),
             base_curve->GetID(),
             curve->GetID(),
             static_cast<int>(PreferredRepresentation::kUnspecified)
        }, {}) {
    if (!surface) {
        throw std::invalid_argument(
                "CurveOnAParametricSurface: Surface pointer is null.");
    }
    if (!base_curve) {
        throw std::invalid_argument(
                "CurveOnAParametricSurface: Base curve pointer is null.");
    }
    if (!curve) {
        throw std::invalid_argument(
                "CurveOnAParametricSurface: Curve pointer is null.");
    }
    SetSurface(surface);
    SetCurves(base_curve, curve);
}



/**
 * EntityBase implementation
 */

igesio::IGESParameterVector CurveOnSurface::GetMainPDParameters() const {
    return {
        static_cast<int>(creation_type_),
        surface_.GetID(),
        base_curve_.GetID(),
        curve_.GetID(),
        static_cast<int>(preferred_representation_)
    };
}

size_t CurveOnSurface::SetMainPDParameters(const pointer2ID& de2id) {
    auto& pd = pd_parameters_;

    if (pd.size() < 5) {
        throw igesio::DataFormatError(
                "CurveOnAParametricSurface: Insufficient number of parameters.");
    }

    // 曲面上の曲線の生成方法
    creation_type_ = static_cast<CurveCreationType>(pd.access_as<int>(0));

    // 曲面 S(u,v)
    try {
        surface_ = PointerContainer<false, ISurface>(
                GetObjectIDFromParameters(pd, 1, de2id));
    } catch (const std::exception& e) {
        throw igesio::DataFormatError(
                "CurveOnAParametricSurface: Invalid surface reference."
                + std::string(e.what()));
    }

    // 曲線 B(t) = (u(t), v(t))
    try {
        base_curve_ = PointerContainer<false, ICurve>(
                GetObjectIDFromParameters(pd, 2, de2id));
    } catch (const std::exception& e) {
        throw igesio::DataFormatError(
                "CurveOnAParametricSurface: Invalid base curve reference."
                + std::string(e.what()));
    }

    // 曲線 C(t) = S(u(t), v(t))
    try {
        curve_ = PointerContainer<false, ICurve>(
                GetObjectIDFromParameters(pd, 3, de2id));
    } catch (const std::exception& e) {
        throw igesio::DataFormatError(
                "CurveOnAParametricSurface: Invalid curve reference."
                + std::string(e.what()));
    }

    // 送信システムにおける優先表現
    preferred_representation_ = static_cast<PreferredRepresentation>(pd.access_as<int>(4));

    return 5;
}

std::unordered_set<igesio::ObjectID>
CurveOnSurface::GetUnresolvedPDReferences() const {
    std::unordered_set<ObjectID> unresolved;

    if (!surface_.IsPointerSet()) {
        unresolved.insert(surface_.GetID());
    }
    if (!base_curve_.IsPointerSet()) {
        unresolved.insert(base_curve_.GetID());
    }
    if (!curve_.IsPointerSet()) {
        unresolved.insert(curve_.GetID());
    }

    return unresolved;
}

bool CurveOnSurface::SetUnresolvedPDReferences(
        const std::shared_ptr<const EntityBase>& entity) {
    // 指定されたエンティティがnullptrの場合は失敗
    if (!entity) {
        return false;
    }

    if (entity->GetID() == surface_.GetID() && !surface_.IsPointerSet()) {
        // ポインタが未設定の場合のみ設定
        if (auto ptr = std::dynamic_pointer_cast<const ISurface>(entity)) {
            return surface_.SetPointer(ptr);
        }
    } else if (entity->GetID() == base_curve_.GetID() && !base_curve_.IsPointerSet()) {
        // ポインタが未設定の場合のみ設定
        if (auto ptr = std::dynamic_pointer_cast<const ICurve>(entity)) {
            try {
                return base_curve_.SetPointer(ptr);
            } catch (const std::invalid_argument&) {
                // 失敗した場合はfalseを返す
                return false;
            }
        }
    } else if (entity->GetID() == curve_.GetID() && !curve_.IsPointerSet()) {
        // ポインタが未設定の場合のみ設定
        if (auto ptr = std::dynamic_pointer_cast<const ICurve>(entity)) {
            return curve_.SetPointer(ptr);
        }
    }

    // 指定されたエンティティと同一のIDを持つ参照がない場合
    return false;
}

std::vector<igesio::ObjectID> CurveOnSurface::GetChildIDs() const {
    std::vector<ObjectID> ids;

    ids.push_back(surface_.GetID());
    ids.push_back(base_curve_.GetID());
    ids.push_back(curve_.GetID());
    return ids;
}

std::shared_ptr<const i_ent::EntityBase>
CurveOnSurface::GetChildEntity(const ObjectID& id) const {
    if (surface_.GetID() == id) {
        auto ptr = surface_.TryGetEntity<EntityBase>();
        if (ptr) return ptr.value();
    } else if (base_curve_.GetID() == id) {
        auto ptr = base_curve_.TryGetEntity<EntityBase>();
        if (ptr) return ptr.value();
    } else if (curve_.GetID() == id) {
        auto ptr = curve_.TryGetEntity<EntityBase>();
        if (ptr) return ptr.value();
    }

    // 指定されたIDを持つ子エンティティが存在しない場合
    return nullptr;
}

igesio::ValidationResult CurveOnSurface::ValidatePD() const {
    std::vector<ValidationError> errors;

    // 曲面 S(u,v)
    if (!surface_.IsPointerSet()) {
        errors.emplace_back("Surface reference is not set.");
    } else {
        auto surf_opt = surface_.TryGetEntity<EntityBase>();
        if (!surf_opt) {
            errors.emplace_back("Surface is not an EntityBase.");
        } else {
            auto result = surf_opt.value()->Validate();
            if (!result.is_valid) {
                errors.emplace_back("Surface is invalid: " + result.Message());
            }
        }
    }

    // 曲線 B(t) = (u(t), v(t))
    if (!base_curve_.IsPointerSet()) {
        errors.emplace_back("Base curve reference is not set.");
    } else {
        auto curve_opt = base_curve_.TryGetEntity<EntityBase>();
        if (!curve_opt) {
            errors.emplace_back("Base curve is not an EntityBase.");
        } else {
            auto result = curve_opt.value()->Validate();
            if (!result.is_valid) {
                errors.emplace_back("Base curve is invalid: " + result.Message());
            }
        }
        if (!GetBaseCurve()->GetBoundingBox().IsOnZPlane()) {
            errors.emplace_back("Base curve is not on a plane parallel to the XY-plane.");
        }
    }

    // 曲線 C(t) = S(u(t), v(t))
    if (!curve_.IsPointerSet()) {
        errors.emplace_back("Curve reference is not set.");
    } else {
        auto curve_opt = curve_.TryGetEntity<EntityBase>();
        if (!curve_opt) {
            errors.emplace_back("Curve is not an EntityBase.");
        } else {
            auto result = curve_opt.value()->Validate();
            if (!result.is_valid) {
                errors.emplace_back("Curve is invalid: " + result.Message());
            }
        }
    }

    return MakeValidationResult(errors);
}



/**
 * ICurve implementation
 */

bool CurveOnSurface::IsClosed() const {
    auto curve_ptr = base_curve_.TryGetEntity<ICurve>();
    if (!curve_ptr) return false;

    return curve_ptr.value()->IsClosed();
}

std::array<double, 2> CurveOnSurface::GetParameterRange() const {
    auto curve_ptr = base_curve_.TryGetEntity<ICurve>();
    if (!curve_ptr) return {0.0, 0.0};

    return curve_ptr.value()->GetParameterRange();
}


std::optional<i_ent::CurveDerivatives>
CurveOnSurface::TryGetDerivatives(const double t, const unsigned int n) const {
    if (!base_curve_.IsPointerSet() || !surface_.IsPointerSet()) {
        return std::nullopt;
    }
    // 基底曲線の導関数とパラメータ値を取得
    auto base = GetBaseCurve();
    auto base_deriv_opt = base->TryGetDerivatives(t, n);
    if (!base_deriv_opt) return std::nullopt;
    const auto& db = base_deriv_opt.value();

    // 曲面の導関数を取得
    auto surf_deriv_opt = GetSurface()->TryGetDerivatives(db[0].x(), db[0].y(), n);
    if (!surf_deriv_opt) return std::nullopt;
    const auto& ds = surf_deriv_opt.value();

    CurveDerivatives result(n);
    // 0階導関数 (位置ベクトル)
    result[0] = ds(0, 0);
    if (n >= 1) {
        // 1階導関数
        result[1] = ds(1, 0)*db[1].x() + ds(0, 1)*db[1].y();
    }
    if (n >= 2) {
        // 2階導関数
        result[2] = ds(2, 0)*db[1].x()*db[1].x()
                  + 2.0 * ds(1, 1)*db[1].x()*db[1].y()
                  + ds(0, 2)*db[1].y()*db[1].y()
                  + ds(1, 0)*db[2].x()
                  + ds(0, 1)*db[2].y();
    }

    return result;
}

double CurveOnSurface::Length(const double start, const double end) const {
    auto curve_ptr = curve_.TryGetEntity<ICurve>();
    if (!curve_ptr) {
        return ICurve::Length(start, end);
    }
    return curve_ptr.value()->Length(start, end);
}

i_num::BoundingBox CurveOnSurface::GetDefinedBoundingBox() const {
    auto curve_ptr = curve_.TryGetEntity<ICurve>();
    if (!curve_ptr) {
        return i_num::BoundingBox();
    }

    // bboxをkDiscretizeTol*1e+3だけ全方向に拡大する
    auto diff = kDiscretizationTol * 1e+3;
    auto bbox = curve_ptr.value()->GetDefinedBoundingBox();

    // サイズの拡大
    auto sizes = bbox.GetSizes();
    auto is_line = bbox.GetIsLines();
    for (size_t i = 0; i < 3; ++i) {
        if (is_line[i]) continue;
        bbox.SetSize(i, sizes[i] + 2*diff, is_line[i]);
    }
    // 制御点の移動
    auto p0 = bbox.GetControl();
    auto directions = bbox.GetDirections();
    p0 = p0 - directions[0]*diff - directions[1]*diff - directions[2]*diff;
    bbox.SetControl(p0);

    return bbox;
}



/**
 * 構成要素の操作
 */

std::shared_ptr<const i_ent::ISurface> CurveOnSurface::GetSurface() const {
    auto surf_opt = surface_.TryGetEntity<ISurface>();
    if (!surf_opt) {
        throw std::runtime_error(
                "CurveOnAParametricSurface: Surface pointer is not set or invalid.");
    }
    return surf_opt.value();
}

std::shared_ptr<const i_ent::ICurve> CurveOnSurface::GetBaseCurve() const {
    auto curve_opt = base_curve_.TryGetEntity<ICurve>();
    if (!curve_opt) {
        throw std::runtime_error(
                "CurveOnAParametricSurface: Base curve pointer is not set or invalid.");
    }
    return curve_opt.value();
}

std::shared_ptr<const i_ent::ICurve> CurveOnSurface::GetCurve() const {
    auto curve_opt = curve_.TryGetEntity<ICurve>();
    if (!curve_opt) {
        throw std::runtime_error(
                "CurveOnAParametricSurface: Curve pointer is not set or invalid.");
    }
    return curve_opt.value();
}

void CurveOnSurface::SetSurface(const std::shared_ptr<ISurface>& surface) {
    if (surface == nullptr) {
        throw std::invalid_argument(
                "CurveOnAParametricSurface: Surface pointer cannot be null.");
    }
    if (surface->GetID() == surface_.GetID()) {
        surface_.SetPointer(surface);
    } else {
        surface_.OverwritePointer(surface);
    }
}

std::shared_ptr<i_ent::ICurve> CurveOnSurface::SetCurves(
        const std::shared_ptr<ICurve>& base_curve, const std::shared_ptr<ICurve>& curve) {
    SetBaseCurve(base_curve);
    // 基底曲線のentity use flagを5 (k2DParametric) に設定
    auto base_curve_entity = std::dynamic_pointer_cast<EntityBase>(base_curve);
    if (base_curve_entity) {
        base_curve_entity->SetEntityUseFlag(i_ent::EntityUseFlag::k2DParametric);
    }

    // 曲線 C(t) を設定: 未指定であれば折れ線近似のもと生成する
    std::shared_ptr<ICurve> generated_curve = nullptr;
    if (curve == nullptr) {
        if (!surface_.IsPointerSet()) {
            // 基底曲線の生成には曲面が設定されている必要がある
            throw std::invalid_argument(
                    "CurveOnAParametricSurface: Surface must be set to generate curve.");
        }
        auto surf = GetSurface();

        // 投射した頂点から3D曲線を生成
        generated_curve = CreateCurveOnSurface(surf, base_curve);
        // S(B(t)) から C(t) を生成したので、優先表現を S(B(t)) に設定
        SetPreferredRepresentation(PreferredRepresentation::kSofB);
        curve_.OverwritePointer(generated_curve);
    }

    // 曲線を設定
    if (curve->GetID() == curve_.GetID()) {
        curve_.SetPointer(curve);
    } else {
        curve_.OverwritePointer(curve);
    }
    // 従属状態に設定
    auto curve_bs = std::dynamic_pointer_cast<EntityBase>(curve);
    if (curve_bs) {
        curve_bs->SetSubordinateEntitySwitch(i_ent::SubordinateEntitySwitch::kPhysicallyDependent);
    }

    return generated_curve;
}



/**
 * インスタンスの作成
 */

std::pair<std::shared_ptr<i_ent::CurveOnAParametricSurface>, std::shared_ptr<i_ent::ICurve>>
i_ent::MakeCurveOnAParametricSurface(const std::shared_ptr<ISurface>& surface,
                                     const std::shared_ptr<ICurve>& base_curve) {
    // 曲線 C(t) を生成
    auto curve = CreateCurveOnSurface(surface, base_curve);
    // CurveOnAParametricSurface エンティティを生成
    auto curve_on_surface = std::make_shared<CurveOnAParametricSurface>(
            surface, base_curve, curve);
    return {curve_on_surface, curve};
}



/**
 * protected methods
 */

bool CurveOnSurface::SetBaseCurve(const std::shared_ptr<const ICurve>& base_curve) {
    if (base_curve == nullptr) {
        throw std::invalid_argument(
                "CurveOnAParametricSurface: Base curve pointer cannot be null.");
    }

    auto surf_opt = surface_.TryGetEntity<ISurface>();
    if (surf_opt) {
        // S(u,v)の定義領域D内で定義されているか確認
        auto surface = surf_opt.value();
        auto [umin, umax, vmin, vmax] = surface->GetParameterRange();
        auto surf_bbox = i_num::BoundingBox(Vector3d(umin, vmin, 0),
                                            Vector3d(umax, vmax, 0));
        auto base_bbox = base_curve->GetBoundingBox();
        if (!surf_bbox.Contains(base_bbox)) {
            // bboxが完全に含まれていない場合は、50点ほどサンプリングして確認
            auto [tmin, tmax] = base_curve->GetParameterRange();
            const size_t num_samples = 50;
            for (size_t i = 0; i <= num_samples; ++i) {
                // 各tにおける点を取得
                double t = tmin + (tmax - tmin) * static_cast<double>(i) / num_samples;
                auto pt_opt = base_curve->TryGetPointAt(t);
                if (!pt_opt) continue;

                // B(t)がD (surf_bbox) 内にあるか確認
                Vector3d uv = {(*pt_opt).x(), (*pt_opt).y(), 0.0};
                if (!surf_bbox.Contains(uv)) {
                    throw std::invalid_argument(
                            "CurveOnAParametricSurface: Base curve is not defined "
                            "within the parameter domain D: {(u,v) | u in ["
                            + std::to_string(umin) + ", " + std::to_string(umax)
                            + "], v in [" + std::to_string(vmin) + ", "
                            + std::to_string(vmax) + "]  }.");
                }
            }
        }
    }

    if (base_curve->GetID() == base_curve_.GetID()) {
        return base_curve_.SetPointer(base_curve);
    } else {
        return base_curve_.OverwritePointer(base_curve);
    }

    return true;
}
