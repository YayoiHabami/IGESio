/**
 * @file entities/curves/curve_on_a_parametric_surface.cpp
 * @brief CurveOnAParametricSurface (Type 142): パラメトリックサーフェス上の曲線エンティティの定義
 * @author Yayoi Habami
 * @date 2025-11-08
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/curves/curve_on_a_parametric_surface.h"

#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/entities/curves/line.h"
#include "igesio/entities/curves/linear_path.h"
#include "igesio/entities/curves/nurbs_algorithms.h"
#include "igesio/entities/surfaces/algorithms/curve_surface_inversion.h"
#include "entities/curves/algorithms/polygonal_approximation.h"

namespace {

/// @brief 曲線を離散化する際のトレランス
constexpr double kDiscretizationTol = 1e-4;

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using i_ent::CurveOnSurface;
using igesio::Vector3d;
using igesio::Vector2d;

/// @brief S(u,v) と B(t) から C(t) を生成する
std::shared_ptr<i_ent::ICurve> CreateCurveOnSurface(
        const i_ent::ISurface& surface,
        const i_ent::ICurve& base_curve) {
    // 曲面上の曲線 C(t) を折れ線近似で生成
    auto polygon = i_ent::ComputeApproximatePolygon(base_curve, {}, {},
                                                    kDiscretizationTol);
    const auto& vertices = polygon.vertices;

    // kPolyLineを生成
    auto curve_2d = i_ent::MakeLinearPath(vertices);
    // 各頂点を曲面上に射影
    std::vector<Vector3d> projected_vertices;
    for (const auto& uv : vertices) {
        auto pt_opt = surface.TryGetPointAt(uv.x(), uv.y());
        if (!pt_opt) {
            throw igesio::ComputationError("CurveOnAParametricSurface: Failed to project "
                    "base curve point onto surface.");
        }
        projected_vertices.push_back(pt_opt.value());
    }

    // 端点接線を chain rule で計算: C'(t) = dS/du * u'(t) + dS/dv * v'(t)
    auto [tmin, tmax] = base_curve.GetParameterRange();
    i_ent::NurbsEndpointTangents tangents;
    for (bool is_start : {true, false}) {
        double t = is_start ? tmin : tmax;
        auto db_opt = base_curve.TryGetDerivatives(t, 1);
        if (!db_opt) continue;
        const auto& db = db_opt.value();
        auto ds_opt = surface.TryGetDerivatives(db[0].x(), db[0].y(), 1);
        if (!ds_opt) continue;
        const auto& ds = ds_opt.value();
        Vector3d tangent = ds(1, 0)*db[1].x() + ds(0, 1)*db[1].y();
        if (is_start) {
            tangents.start = tangent;
        } else {
            tangents.end = tangent;
        }
    }

    // 投射した頂点からNURBS曲線でC(t)を生成; 失敗時は折れ線で代替
    try {
        i_ent::NurbsApproxOptions opts;
        opts.tolerance = kDiscretizationTol * 10.0;
        return i_ent::ApproximateWithNurbs(projected_vertices, tangents, opts);
    } catch (const std::exception&) {
        return i_ent::MakeLinearPath(projected_vertices);
    }
}

/// @brief パラメータ空間の点列(u,v,0)から、退化を除去したベース曲線Bを構築する
///
/// 連続重複点をスケール相対の許容で除去し、`is_closed`(=モデル曲線Cが閉じているか)に
/// 応じて kPlanarLoop(閉) / kPlanarPolyline(開) の`LinearPath`を生成する。
/// 下流のトリム領域テッセレーション(`ComputeContainmentPolygons`)は閉曲線かつ非退化
/// (各点で`GetPointAt`が有効) を前提とするため、ここで両者を満たすよう整える。
///
/// @param uv_points (u, v, 0) のサンプル列
/// @param is_closed 閉ループとして構築するか
/// @return 構築したLinearPath。点が退化して構築できない場合は `nullptr`
std::shared_ptr<i_ent::LinearPath> BuildParamSpaceBaseCurve(
        const std::vector<Vector3d>& uv_points, const bool is_closed) {
    // uv の広がりからスケール相対の重複許容を決める (退化判定に用いる)
    Vector2d lo(std::numeric_limits<double>::infinity(),
                std::numeric_limits<double>::infinity());
    Vector2d hi = -lo;
    for (const auto& p : uv_points) {
        if (!std::isfinite(p.x()) || !std::isfinite(p.y())) continue;
        lo = lo.cwiseMin(Vector2d(p.x(), p.y()));
        hi = hi.cwiseMax(Vector2d(p.x(), p.y()));
    }
    if (!lo.allFinite() || !hi.allFinite()) return nullptr;
    const double tol = std::max(1e-12, 1e-7 * (hi - lo).norm());

    // 非有限点を除外しつつ連続重複を間引く
    std::vector<Vector2d> uv;
    uv.reserve(uv_points.size());
    for (const auto& p : uv_points) {
        if (!std::isfinite(p.x()) || !std::isfinite(p.y())) continue;
        const Vector2d q(p.x(), p.y());
        if (uv.empty() || (uv.back() - q).norm() > tol) uv.push_back(q);
    }
    // 閉ループ: 先頭と一致する末尾点は閉辺と二重化するため除去する
    if (is_closed && uv.size() >= 2 && (uv.front() - uv.back()).norm() <= tol) {
        uv.pop_back();
    }
    // 退化チェック (閉ループは3点以上、開折れ線は2点以上)
    if (uv.size() < (is_closed ? 3u : 2u)) return nullptr;

    return i_ent::MakeLinearPath(uv, is_closed);
}

/// @brief B(t)が曲面S(u,v)のパラメータ定義領域D内で定義されているかを検証する
/// @param surface 曲面 S(u,v)
/// @param base_curve 曲線 B(t)
/// @throw igesio::EntityValueError B(t)がD外で定義されている場合
void ValidateBaseCurveInDomain(const i_ent::ISurface& surface,
                               const i_ent::ICurve& base_curve) {
    auto [umin, umax, vmin, vmax] = surface.GetParameterRange();
    auto surf_bbox = i_num::BoundingBox(Vector3d(umin, vmin, 0),
                                        Vector3d(umax, vmax, 0));
    auto base_bbox = base_curve.GetBoundingBox();
    if (surf_bbox.Contains(base_bbox)) return;

    // bboxが完全に含まれていない場合は、50点ほどサンプリングして確認
    auto [tmin, tmax] = base_curve.GetParameterRange();
    const size_t num_samples = 50;
    for (size_t i = 0; i <= num_samples; ++i) {
        // 各tにおける点を取得
        double t = tmin + (tmax - tmin) * static_cast<double>(i) / num_samples;
        auto pt_opt = base_curve.TryGetPointAt(t);
        if (!pt_opt) continue;

        // B(t)がD (surf_bbox) 内にあるか確認
        Vector3d uv = {(*pt_opt).x(), (*pt_opt).y(), 0.0};
        if (!surf_bbox.Contains(uv)) {
            throw igesio::EntityValueError(
                    "CurveOnAParametricSurface: Base curve is not defined "
                    "within the parameter domain D: {(u,v) | u in ["
                    + std::to_string(umin) + ", " + std::to_string(umax)
                    + "], v in [" + std::to_string(vmin) + ", "
                    + std::to_string(vmax) + "]  }.");
        }
    }
}

/// @brief 値コンストラクタ用のPDパラメータを構築する
/// @param surface 曲面 S(u,v)
/// @param base_curve 曲線 B(t)
/// @param curve 曲線 C(t)
/// @return 構築されたIGESParameterVector
/// @throw std::invalid_argument いずれかがnullptrの場合
/// @note 委譲コンストラクタの初期化式でnullptrを参照しないよう、
///       GetID()の呼び出し前にここでnull検証を行う
igesio::IGESParameterVector BuildCosParameters(
        const std::shared_ptr<i_ent::ISurface>& surface,
        const std::shared_ptr<i_ent::ICurve>& base_curve,
        const std::shared_ptr<i_ent::ICurve>& curve) {
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
    return {static_cast<int>(i_ent::CurveCreationType::kUnspecified),
            surface->GetID(),
            base_curve->GetID(),
            curve->GetID(),
            static_cast<int>(i_ent::PreferredRepresentation::kUnspecified)};
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

    // 仕様上 Type 142 の Entity Use Flag (Status Number 5〜6桁) は 00 (Geometry)
    // でなければならないが、一部CAD (SolidWorks / NX 等) はトリム境界として
    // 05 (2-D Parametric) で出力する。読み込みを通すため、00 以外であれば
    // 00 へ正規化する。検証 (IsValid) は 00 を要求し続けるため (出力は厳格)、
    // ここで仕様準拠の値に寄せる (パラメータ空間性は BPTR の構造で表現され、
    // 142 本体の幾何解釈は自身の Entity Use Flag を参照しない)。
    if (GetEntityUseFlag() != i_ent::EntityUseFlag::kGeometry) {
        SetEntityUseFlag(i_ent::EntityUseFlag::kGeometry);
    }
}

CurveOnSurface::CurveOnAParametricSurface(
        const std::shared_ptr<ISurface>& surface,
        const std::shared_ptr<ICurve>& base_curve,
        const std::shared_ptr<ICurve>& curve)
        : CurveOnAParametricSurface(
            RawEntityDE::ByDefault(EntityType::kCurveOnAParametricSurface),
            // null検証は委譲前のBuildCosParameters内で行う
            // (初期化式でのGetID()参照前に検証する必要があるため)
            BuildCosParameters(surface, base_curve, curve), {}) {
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
        throw igesio::EntityParameterError(
                "CurveOnAParametricSurface: Insufficient number of parameters.");
    }

    // 曲面上の曲線の生成方法
    creation_type_ = static_cast<CurveCreationType>(pd.access_as<int>(0));

    // 曲面 S(u,v)
    try {
        surface_ = PointerContainer<false, ISurface>(
                GetObjectIDFromParameters(pd, 1, de2id));
    } catch (const std::exception& e) {
        throw igesio::ReferenceError(
                "CurveOnAParametricSurface: Invalid surface reference."
                + std::string(e.what()));
    }

    // 曲線 B(t) = (u(t), v(t))
    // BPTR=0 (ベース曲線の省略) を許容する。一部CAD (CATIA等) はモデル空間曲線 C と
    // 曲面 S のみを出力し、パラメータ空間のベース曲線 B を省略する。この場合
    // base_curve_ は UnsetID を保持し、読み込みの参照解決後に
    // ReconstructOmittedBaseCurve() で B = S^{-1}∘C として再構築する。
    try {
        base_curve_ = PointerContainer<false, ICurve>(
                GetObjectIDFromParameters(pd, 2, de2id, /*allow_unset_id=*/true));
    } catch (const std::exception& e) {
        throw igesio::ReferenceError(
                "CurveOnAParametricSurface: Invalid base curve reference."
                + std::string(e.what()));
    }

    // 曲線 C(t) = S(u(t), v(t))
    try {
        curve_ = PointerContainer<false, ICurve>(
                GetObjectIDFromParameters(pd, 3, de2id));
    } catch (const std::exception& e) {
        throw igesio::ReferenceError(
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
    // BPTR=0 (省略) のときは UnsetID を保持する。これは「未解決の参照」ではなく
    // 後段で再構築する対象のため、未解決集合には加えない (UnsetID を探さない)。
    if (!base_curve_.IsPointerSet()
            && base_curve_.GetID() != IDGenerator::UnsetID()) {
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
        // ベース曲線Bはパラメータ空間 (z=0) にあるべきだが、僅かに外れても
        // (x,y)=(u,v)として使用/描画可能。幾何的品質の指摘 (kWarning) とする。
        if (!GetBaseCurve()->GetBoundingBox().IsOnZPlane()) {
            errors.emplace_back("Base curve is not on a plane parallel to the XY-plane.",
                                igesio::ValidationSeverity::kWarning);
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
 * 直線部・角点サポート
 */

std::vector<double> CurveOnSurface::GetCornerParams() const {
    // B(t) の角点では C(t) = S(B(t)) も接線方向が不連続になる
    auto bc = base_curve_.TryGetEntity<ICurve>();
    if (!bc || !bc.value()) return {};
    return bc.value()->GetCornerParams();
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
CurveOnSurface::TryGetDefinedDerivatives(const double t, const unsigned int n) const {
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
        throw igesio::ReferenceError(
                "CurveOnAParametricSurface: Surface pointer is not set or invalid.");
    }
    return surf_opt.value();
}

std::shared_ptr<const i_ent::ICurve> CurveOnSurface::GetBaseCurve() const {
    auto curve_opt = base_curve_.TryGetEntity<ICurve>();
    if (!curve_opt) {
        throw igesio::ReferenceError(
                "CurveOnAParametricSurface: Base curve pointer is not set or invalid.");
    }
    return curve_opt.value();
}

std::shared_ptr<const i_ent::ICurve> CurveOnSurface::GetCurve() const {
    auto curve_opt = curve_.TryGetEntity<ICurve>();
    if (!curve_opt) {
        throw igesio::ReferenceError(
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
    MarkGeometryModified();
}

std::shared_ptr<i_ent::ICurve> CurveOnSurface::SetCurves(
        const std::shared_ptr<ICurve>& base_curve, const std::shared_ptr<ICurve>& curve) {
    SetBaseCurve(base_curve);
    // 基底曲線のentity use flagを5 (k2DParametric) に設定
    // 併せてSubordinateEntitySwitchをkPhysicallyDependentに設定
    auto base_curve_entity = std::dynamic_pointer_cast<EntityBase>(base_curve);
    if (base_curve_entity) {
        base_curve_entity->SetEntityUseFlag(i_ent::EntityUseFlag::k2DParametric);
        base_curve_entity->SetSubordinateEntitySwitch(
            i_ent::SubordinateEntitySwitch::kPhysicallyDependent);
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
        generated_curve = CreateCurveOnSurface(*surf, *base_curve);

        // S(B(t)) から C(t) を生成したので、優先表現を S(B(t)) に設定
        SetPreferredRepresentation(PreferredRepresentation::kSofB);
    }

    // 曲線を設定 (curve未指定の場合は自動生成したものを使用)
    const auto& effective_curve = curve ? curve : generated_curve;
    if (effective_curve->GetID() == curve_.GetID()) {
        curve_.SetPointer(effective_curve);
    } else {
        curve_.OverwritePointer(effective_curve);
    }
    // 従属状態に設定
    auto curve_bs = std::dynamic_pointer_cast<EntityBase>(effective_curve);
    if (curve_bs) {
        curve_bs->SetSubordinateEntitySwitch(i_ent::SubordinateEntitySwitch::kPhysicallyDependent);
    }

    MarkGeometryModified();
    return generated_curve;
}

bool CurveOnSurface::SetCreationType(const CurveCreationType type) {
    // CRTNは規格上informationalであり、幾何形状との一致は検証しない
    if (type == CurveCreationType::kUnspecified ||
        type == CurveCreationType::kProjection ||
        type == CurveCreationType::kIntersection ||
        type == CurveCreationType::kIsoparametric) {
        creation_type_ = type;
        MarkGeometryModified();
        return true;
    }
    return false;  // 無効なタイプ
}

bool CurveOnSurface::SetPreferredRepresentation(const PreferredRepresentation pref) {
    if (pref == PreferredRepresentation::kUnspecified ||
        pref == PreferredRepresentation::kSofB ||
        pref == PreferredRepresentation::kC ||
        pref == PreferredRepresentation::kEquallyPreferred) {
        preferred_representation_ = pref;
        // 優先表現はレンダラの表現選択 (S(B(t))/C(t)) に影響する
        MarkGeometryModified();
        return true;
    }
    return false;  // 無効な値
}

std::shared_ptr<i_ent::ICurve> CurveOnSurface::ReconstructOmittedBaseCurve() {
    // BPTR=0 (省略) かつS・Cが解決済みのときのみ再構築する
    if (base_curve_.GetID() != IDGenerator::UnsetID()) return nullptr;
    if (!surface_.IsPointerSet() || !curve_.IsPointerSet()) return nullptr;

    auto surf_opt = surface_.TryGetEntity<ISurface>();
    auto curve_opt = curve_.TryGetEntity<ICurve>();
    if (!surf_opt || !curve_opt) return nullptr;

    // 再構築はbest-effort。逆射影・近似・ドメイン包含チェック等で例外が出ても
    // 読み込み全体を止めずnullptrを返す (当該境界は非致命的にスキップ)。
    try {
        // CをSのパラメータ空間へ逆射影する (単一弧モード; Type 142は連続を仮定)
        auto arcs = InvertCurveOntoSurface(*surf_opt.value(), *curve_opt.value(), {});
        if (arcs.empty() || arcs.front().uv_points.size() < 2) return nullptr;

        // パラメータ空間のベース曲線Bを生成する。トリム境界Cが閉ループならBも閉じ、
        // 退化点列は除去する (下流テッセレーションが閉曲線・非退化を前提とするため)。
        auto base = BuildParamSpaceBaseCurve(
                arcs.front().uv_points, curve_opt.value()->IsClosed());
        if (!base) return nullptr;
        // B は曲面のパラメータ空間にあるため EUF=05 (2D Parametric)・物理従属に設定
        base->SetEntityUseFlag(i_ent::EntityUseFlag::k2DParametric);
        base->SetSubordinateEntitySwitch(
                i_ent::SubordinateEntitySwitch::kPhysicallyDependent);

        // base_curve_に設定する (新規IDのためOverwritePointer経由)
        SetBaseCurve(base);
        return base;
    } catch (const std::exception&) {
        return nullptr;
    }
}



/**
 * ファクトリ関数
 */

std::pair<std::shared_ptr<i_ent::CurveOnAParametricSurface>, std::shared_ptr<i_ent::ICurve>>
i_ent::MakeCurveOnAParametricSurface(const std::shared_ptr<ISurface>& surface,
                                     const std::shared_ptr<ICurve>& base_curve,
                                     const CurveCreationType creation_type) {
    if (!surface) {
        throw std::invalid_argument(
                "MakeCurveOnAParametricSurface: Surface pointer is null.");
    }
    if (!base_curve) {
        throw std::invalid_argument(
                "MakeCurveOnAParametricSurface: Base curve pointer is null.");
    }

    // C(t)の生成前にB(t)が定義領域D内にあるかを検証する (D外の場合、生成中の
    // 射影失敗 (ComputationError) ではなく入力値の問題 (EntityValueError)
    // として報告するため)
    ValidateBaseCurveInDomain(*surface, *base_curve);

    // 曲線 C(t) を生成
    auto curve = CreateCurveOnSurface(*surface, *base_curve);
    // CurveOnAParametricSurface エンティティを生成
    auto curve_on_surface = std::make_shared<CurveOnAParametricSurface>(
            surface, base_curve, curve);
    curve_on_surface->SetCreationType(creation_type);
    // C(t)はS(B(t))から生成したため、優先表現はkSofB (コンストラクタの
    // SetCurvesではCの明示指定経路となり設定されないため、ここで設定する)
    curve_on_surface->SetPreferredRepresentation(PreferredRepresentation::kSofB);
    return {curve_on_surface, curve};
}

std::shared_ptr<i_ent::CurveOnAParametricSurface>
i_ent::MakeCurveOnAParametricSurface(const std::shared_ptr<ISurface>& surface,
                                     const std::shared_ptr<ICurve>& base_curve,
                                     const std::shared_ptr<ICurve>& curve,
                                     const CurveCreationType creation_type,
                                     const PreferredRepresentation preferred) {
    // nullチェックはコンストラクタ (BuildCosParameters) が行う
    auto curve_on_surface = std::make_shared<CurveOnAParametricSurface>(
            surface, base_curve, curve);
    curve_on_surface->SetCreationType(creation_type);
    curve_on_surface->SetPreferredRepresentation(preferred);
    return curve_on_surface;
}

i_ent::CurveOnSurfaceParts i_ent::MakeIsoparametricCurve(
        const std::shared_ptr<ISurface>& surface,
        const IsoparametricDirection direction, const double value) {
    if (!surface) {
        throw std::invalid_argument(
                "MakeIsoparametricCurve: Surface pointer is null.");
    }

    // 固定値が定義域内かを検証し、パラメータ空間上のB(t)の端点を決める
    const auto [umin, umax, vmin, vmax] = surface->GetParameterRange();
    Vector3d start, end;
    if (direction == IsoparametricDirection::kUConstant) {
        if (value < umin || value > umax) {
            throw igesio::EntityValueError(
                    "MakeIsoparametricCurve: u = " + std::to_string(value) +
                    " is outside the parameter domain [" + std::to_string(umin) +
                    ", " + std::to_string(umax) + "].");
        }
        start = Vector3d(value, vmin, 0.0);
        end = Vector3d(value, vmax, 0.0);
    } else {
        if (value < vmin || value > vmax) {
            throw igesio::EntityValueError(
                    "MakeIsoparametricCurve: v = " + std::to_string(value) +
                    " is outside the parameter domain [" + std::to_string(vmin) +
                    ", " + std::to_string(vmax) + "].");
        }
        start = Vector3d(umin, value, 0.0);
        end = Vector3d(umax, value, 0.0);
    }
    // 非有界な定義域 (平面等) ではB(t)の端点を定められない
    if (!start.allFinite() || !end.allFinite()) {
        throw igesio::EntityValueError(
                "MakeIsoparametricCurve: The surface parameter domain is "
                "unbounded; cannot determine the endpoints of the base curve.");
    }

    // B(t): パラメータ空間上の線分 (EUF=05・物理従属はSetCurvesが設定する)
    auto base_curve = std::make_shared<Line>(start, end);

    // 本体とC(t)を生成し、CRTN=3 (アイソパラメトリック) を設定
    auto [curve_on_surface, curve] = MakeCurveOnAParametricSurface(
            surface, base_curve, CurveCreationType::kIsoparametric);
    return {curve_on_surface, base_curve, curve};
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
        ValidateBaseCurveInDomain(*surf_opt.value(), *base_curve);
    }

    const bool ok = (base_curve->GetID() == base_curve_.GetID())
            ? base_curve_.SetPointer(base_curve)
            : base_curve_.OverwritePointer(base_curve);
    if (ok) MarkGeometryModified();
    return ok;
}
