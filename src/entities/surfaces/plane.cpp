/**
 * @file entities/surfaces/plane.cpp
 * @brief Plane (Type 108): 平面エンティティの実装
 * @author Yayoi Habami
 * @date 2026-06-09
 * @copyright 2026 Yayoi Habami
 */
#include "igesio/entities/surfaces/plane.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "igesio/common/errors.h"
#include "igesio/common/iges_parameter_vector.h"
#include "igesio/numerics/core/tolerance.h"
#include "igesio/numerics/geometric/polygon.h"
#include "igesio/entities/curves/linear_path.h"

#include "entities/curves/algorithms/polygonal_approximation.h"

namespace {

namespace i_num = igesio::numerics;
namespace i_ent = igesio::entities;
using igesio::Vector2d;
using igesio::Vector3d;

/// @brief 平面係数 (a,b,c) が非ゼロであることを確認する
/// @throw igesio::EntityValueError (a,b,c) がすべてゼロの場合
void ValidatePlaneNormalOrThrow(double a, double b, double c,
                                const std::string& context) {
    if (i_num::IsApproxZero(Vector3d(a, b, c).norm())) {
        throw igesio::EntityValueError(
                context + ": plane normal (A,B,C) must not be all zero.");
    }
}

/// @brief 境界曲線がnullptrでないことを確認し、そのIDを返す
/// @throw std::invalid_argument boundaryがnullptrの場合
/// @note 委譲コンストラクタの引数リスト内で逆参照する前にnullptrを検出するヘルパー
igesio::ObjectID GetBoundaryID(
        const std::shared_ptr<i_ent::ICurve>& boundary) {
    if (boundary == nullptr) {
        throw std::invalid_argument(
                "Boundary curve of BoundedPlane cannot be nullptr.");
    }
    return boundary->GetID();
}

/// @brief PDレコードからA,B,C,Dを読み出す
/// @throw igesio::EntityParameterError パラメータ数が5未満の場合
std::array<double, 4> ReadCoefficients(igesio::IGESParameterVector& pd) {
    if (pd.size() < 5) {
        throw igesio::EntityParameterError(
                "Plane (Type 108) must have at least 5 parameters.");
    }
    return {pd.access_as<double>(0), pd.access_as<double>(1),
            pd.access_as<double>(2), pd.access_as<double>(3)};
}

/// @brief PDレコードから表示シンボルパラメータ (X,Y,Z,SIZE) を読み出す
///
/// 主パラメータ A,B,C,D,PTR (インデックス0-4) に続くインデックス5-8を表示シンボル
/// パラメータとして読み出す。表示シンボルを省略したファイルでは、インデックス5に
/// 追加ポインタブロックの先頭 (要素数を表す非デフォルトのInteger) が来るため、これを
/// 検出した場合は表示シンボルなしとして扱う。Real・空欄(デフォルト)は表示値とみなす。
///
/// @param pd PDレコード
/// @param[out] location 位置点 (X_T, Y_T, Z_T)
/// @param[out] size サイズ
/// @return 主パラメータの終了インデックス (表示シンボルなしなら5、ありなら最大9)
size_t ReadDisplaySymbol(igesio::IGESParameterVector& pd,
                         Vector3d& location, double& size) {
    location = Vector3d::Zero();
    size = 0.0;
    // インデックス5が無い、または非デフォルトのInteger (追加ポインタの要素数) の
    // 場合は表示シンボルなし
    if (pd.size() <= 5 ||
        (pd.is_type<int>(5) && !pd.get_format(5).is_default)) {
        return 5;
    }
    // X, Y, Z, SIZEをインデックス5-8から読み出す (不足分は0のまま)
    for (size_t i = 5; i < 9 && i < pd.size(); ++i) {
        const double value = pd.access_as<double>(i);
        if (i < 8) {
            location(static_cast<int>(i - 5)) = value;
        } else {
            size = value;
        }
    }
    return std::min<size_t>(pd.size(), 9);
}

/// @brief 曲線の代表寸法に基づく弦許容誤差を計算する
/// @param curve 対象曲線
/// @return 折れ線近似の許容距離 (bbox対角の1e-3、無限/退化時は1e-4)
double ComputeChordTolerance(const i_ent::ICurve& curve) {
    const auto sizes = curve.GetBoundingBox().GetSizes();
    const double scale = std::max({sizes[0], sizes[1], sizes[2]});
    if (std::isfinite(scale) && scale > 0.0) return scale * 1e-3;
    return 1e-4;
}

}  // namespace



namespace igesio::entities {

/**
 * 平面フレーム
 */

PlaneFrame MakePlaneFrame(double a, double b, double c, double d) {
    const Vector3d n(a, b, c);
    const double len = n.norm();
    if (i_num::IsApproxZero(len)) {
        throw igesio::EntityValueError(
                "MakePlaneFrame: plane normal (A,B,C) must not be all zero.");
    }

    PlaneFrame frame;
    frame.normal = n / len;
    // 定義空間原点からの垂線の足 (n·origin = Dを満たす)
    frame.origin = (d / (len * len)) * n;

    // 法線と最も非平行な座標軸を選び、面内に射影してe_uを作る (安定化)
    const Vector3d na = frame.normal.cwiseAbs();
    Vector3d axis;
    if (na.x() <= na.y() && na.x() <= na.z()) {
        axis = Vector3d::UnitX();
    } else if (na.y() <= na.z()) {
        axis = Vector3d::UnitY();
    } else {
        axis = Vector3d::UnitZ();
    }
    frame.e_u = (axis - axis.dot(frame.normal) * frame.normal).normalized();
    // e_v = n̂ × e_u (単位ベクトル)。これにより e_u × e_v = n̂ となる
    frame.e_v = frame.normal.cross(frame.e_u);
    return frame;
}



/**
 * Plane (Type 108, Form 0)
 */

Plane::Plane(const RawEntityDE& de_record, const IGESParameterVector& parameters,
             const pointer2ID& de2id, const ObjectID& iges_id)
        : EntityBase(de_record, parameters, de2id, iges_id) {
    InitializePD(de2id);
}

Plane::Plane(double a, double b, double c, double d)
        : Plane(RawEntityDE::ByDefault(EntityType::kPlane),
                IGESParameterVector{a, b, c, d, IDGenerator::UnsetID(),
                                    0.0, 0.0, 0.0, 0.0}, {}) {
    ValidatePlaneNormalOrThrow(a, b, c, "Plane");
}

PlaneFrame Plane::GetFrame() const {
    return MakePlaneFrame(coefficients_[0], coefficients_[1],
                          coefficients_[2], coefficients_[3]);
}

IGESParameterVector Plane::GetMainPDParameters() const {
    IGESParameterVector params;
    params.push_back(coefficients_[0]);
    params.push_back(coefficients_[1]);
    params.push_back(coefficients_[2]);
    params.push_back(coefficients_[3]);
    // Form 0ではPTRはヌルポインタ
    params.push_back(IDGenerator::UnsetID());
    params.push_back(symbol_location_(0));
    params.push_back(symbol_location_(1));
    params.push_back(symbol_location_(2));
    params.push_back(symbol_size_);
    return params;
}

size_t Plane::SetMainPDParameters(const pointer2ID&) {
    auto& pd = pd_parameters_;
    coefficients_ = ReadCoefficients(pd);
    // インデックス4のPTRはForm 0では0のため読み捨てる
    return ReadDisplaySymbol(pd, symbol_location_, symbol_size_);
}

ValidationResult Plane::ValidatePD() const {
    std::vector<ValidationError> errors;
    if (i_num::IsApproxZero(Vector3d(coefficients_[0], coefficients_[1],
                                     coefficients_[2]).norm())) {
        errors.emplace_back("Plane: normal (A,B,C) must not be all zero.");
    }
    if (form_number_ != 0) {
        errors.emplace_back(
                "Plane: form number must be 0 for an unbounded plane.");
    }
    return MakeValidationResult(std::move(errors));
}

std::array<double, 4> Plane::GetParameterRange() const {
    const double inf = std::numeric_limits<double>::infinity();
    return {-inf, inf, -inf, inf};
}

std::optional<SurfaceDerivatives> Plane::TryGetDefinedDerivatives(
        const double u, const double v, const unsigned int order) const {
    if (i_num::IsApproxZero(Vector3d(coefficients_[0], coefficients_[1],
                                     coefficients_[2]).norm())) {
        return std::nullopt;
    }
    const PlaneFrame f = GetFrame();
    SurfaceDerivatives deriv(order);
    deriv(0, 0) = f.origin + u * f.e_u + v * f.e_v;
    if (order >= 1) {
        deriv(1, 0) = f.e_u;
        deriv(0, 1) = f.e_v;
    }
    // 2階以上はゼロ初期化のまま (アフィン曲面)
    return deriv;
}

i_num::BoundingBox Plane::GetDefinedBoundingBox() const {
    if (i_num::IsApproxZero(Vector3d(coefficients_[0], coefficients_[1],
                                     coefficients_[2]).norm())) {
        return i_num::BoundingBox();
    }
    const PlaneFrame f = GetFrame();
    const double inf = std::numeric_limits<double>::infinity();
    // 面内2方向は無限直線 (kLine)、法線方向は厚みゼロの2次元無限スラブ
    return i_num::BoundingBox(f.origin, {f.e_u, f.e_v, f.normal},
                              {inf, inf, 0.0}, {true, true, false});
}



/**
 * BoundedPlane (Type 108, Form 1 / -1)
 */

BoundedPlane::BoundedPlane(
        const RawEntityDE& de_record, const IGESParameterVector& parameters,
        const pointer2ID& de2id, const ObjectID& iges_id)
        : EntityBase(de_record, parameters, de2id, iges_id),
          boundary_(IDGenerator::UnsetID()) {
    InitializePD(de2id);
}

BoundedPlane::BoundedPlane(double a, double b, double c, double d,
                           const std::shared_ptr<ICurve>& boundary,
                           bool negative)
        : BoundedPlane(RawEntityDE::ByDefault(EntityType::kPlane,
                                              negative ? -1 : 1),
                       IGESParameterVector{a, b, c, d, GetBoundaryID(boundary),
                                           0.0, 0.0, 0.0, 0.0}, {}) {
    ValidatePlaneNormalOrThrow(a, b, c, "BoundedPlane");
    SetBoundaryCurve(boundary);
}

PlaneFrame BoundedPlane::GetFrame() const {
    return MakePlaneFrame(coefficients_[0], coefficients_[1],
                          coefficients_[2], coefficients_[3]);
}

void BoundedPlane::InvalidateGeometryCaches() const noexcept {
    base_surface_.reset();
    uv_boundary_.reset();
    defined_bbox_.reset();
    InvalidateDomainCache();
}

IGESParameterVector BoundedPlane::GetMainPDParameters() const {
    IGESParameterVector params;
    params.push_back(coefficients_[0]);
    params.push_back(coefficients_[1]);
    params.push_back(coefficients_[2]);
    params.push_back(coefficients_[3]);
    params.push_back(boundary_.GetID());
    params.push_back(symbol_location_(0));
    params.push_back(symbol_location_(1));
    params.push_back(symbol_location_(2));
    params.push_back(symbol_size_);
    return params;
}

size_t BoundedPlane::SetMainPDParameters(const pointer2ID& de2id) {
    auto& pd = pd_parameters_;
    coefficients_ = ReadCoefficients(pd);

    // PTR: 境界閉曲線 (非ヌル)
    try {
        boundary_ = PointerContainer<false, ICurve>(
                GetObjectIDFromParameters(pd, 4, de2id));
    } catch (const std::exception& e) {
        throw igesio::ReferenceError(
                "BoundedPlane: invalid boundary curve reference. "
                + std::string(e.what()));
    }
    outer_is_boundary_of_d_ = false;

    return ReadDisplaySymbol(pd, symbol_location_, symbol_size_);
}

std::unordered_set<igesio::ObjectID>
BoundedPlane::GetUnresolvedPDReferences() const {
    std::unordered_set<ObjectID> unresolved;
    if (!boundary_.IsPointerSet()) {
        unresolved.insert(boundary_.GetID());
    }
    return unresolved;
}

bool BoundedPlane::SetUnresolvedPDReferences(
        const std::shared_ptr<const EntityBase>& entity) {
    if (!entity) return false;
    if (entity->GetID() == boundary_.GetID() && !boundary_.IsPointerSet()) {
        if (auto ptr = std::dynamic_pointer_cast<const ICurve>(entity)) {
            if (boundary_.SetPointer(ptr)) {
                InvalidateGeometryCaches();
                return true;
            }
        }
    }
    return false;
}

ValidationResult BoundedPlane::ValidatePD() const {
    std::vector<ValidationError> errors;
    if (i_num::IsApproxZero(Vector3d(coefficients_[0], coefficients_[1],
                                     coefficients_[2]).norm())) {
        errors.emplace_back("BoundedPlane: normal (A,B,C) must not be all zero.");
    }
    if (form_number_ != 1 && form_number_ != -1) {
        errors.emplace_back(
                "BoundedPlane: form number must be 1 or -1 for a bounded plane.");
    }

    // Validate()はEntityBase、IsClosed()はICurveのメソッドのため別々に取得する
    auto boundary_entity = boundary_.TryGetEntity<EntityBase>();
    if (!boundary_entity) {
        errors.emplace_back("BoundedPlane: boundary curve reference is not set.");
    } else {
        auto result = boundary_entity.value()->Validate();
        if (!result.is_valid) {
            errors.emplace_back("BoundedPlane: boundary curve is invalid: "
                                + result.Message());
        } else {
            auto boundary_curve = boundary_.TryGetEntity<ICurve>();
            if (boundary_curve && !boundary_curve.value()->IsClosed()) {
                errors.emplace_back(ValidationError(
                        "BoundedPlane: boundary curve should be a closed curve.",
                        ValidationSeverity::kWarning));
            }
        }
    }
    return MakeValidationResult(std::move(errors));
}

std::vector<igesio::ObjectID> BoundedPlane::GetChildIDs() const {
    return {boundary_.GetID()};
}

std::shared_ptr<const i_ent::EntityBase>
BoundedPlane::GetChildEntity(const ObjectID& id) const {
    if (boundary_.GetID() == id) {
        auto ptr = boundary_.TryGetEntity<EntityBase>();
        if (ptr) return ptr.value();
    }
    return nullptr;
}

void BoundedPlane::PrepareGeometryCache() const {
    EnsureBoundaryCache();
    BuildDomainCache();
}

std::shared_ptr<const i_ent::ISurface> BoundedPlane::GetBaseSurface() const {
    if (!base_surface_) {
        if (i_num::IsApproxZero(Vector3d(coefficients_[0], coefficients_[1],
                                         coefficients_[2]).norm())) {
            return nullptr;
        }
        base_surface_ = std::make_shared<Plane>(
                coefficients_[0], coefficients_[1],
                coefficients_[2], coefficients_[3]);
    }
    return base_surface_;
}

std::shared_ptr<const i_ent::ICurve> BoundedPlane::GetOuterUVBoundary() const {
    EnsureBoundaryCache();
    return uv_boundary_;
}

std::shared_ptr<const i_ent::ICurve>
BoundedPlane::GetInnerUVBoundaryAt(std::size_t index) const {
    throw std::out_of_range(
            "BoundedPlane has no inner boundary (requested index "
            + std::to_string(index) + ").");
}

i_num::BoundingBox BoundedPlane::GetDefinedBoundingBox() const {
    EnsureBoundaryCache();
    if (defined_bbox_) return *defined_bbox_;
    // 境界未解決時は基底曲面 (無限平面) のbboxにフォールバック
    auto base = GetBaseSurface();
    if (base) return base->GetDefinedBoundingBox();
    return i_num::BoundingBox();
}

void BoundedPlane::EnsureBoundaryCache() const {
    if (uv_boundary_) return;

    auto boundary = boundary_.TryGetEntity<ICurve>();
    if (!boundary) return;  // 境界未解決
    if (i_num::IsApproxZero(Vector3d(coefficients_[0], coefficients_[1],
                                     coefficients_[2]).norm())) {
        return;  // 退化した平面
    }
    const PlaneFrame f = GetFrame();

    // 平面フレームをモデル空間へ変換 (originは点、e_u/e_vはベクトル)
    const auto origin_m = Transform(std::optional<Vector3d>(f.origin), true);
    const auto eu_m = Transform(std::optional<Vector3d>(f.e_u), false);
    const auto ev_m = Transform(std::optional<Vector3d>(f.e_v), false);
    if (!origin_m || !eu_m || !ev_m) return;

    // 境界曲線をモデル空間で折れ線近似する
    const double eps = ComputeChordTolerance(*boundary.value());
    i_num::PolygonData poly;
    try {
        poly = ComputeApproximatePolygon(*boundary.value(), {}, {}, eps);
    } catch (const std::exception&) {
        return;
    }
    if (poly.vertices.size() < 2) return;

    // 各サンプルをモデル空間フレームへ内積射影して(u, v)化する
    std::vector<Vector2d> uv;
    std::vector<Vector3d> def_points;
    uv.reserve(poly.vertices.size());
    def_points.reserve(poly.vertices.size());
    for (const auto& p : poly.vertices) {
        const Vector3d delta = p - *origin_m;
        const double u = delta.dot(*eu_m);
        const double v = delta.dot(*ev_m);
        uv.emplace_back(u, v);
        def_points.push_back(f.origin + u * f.e_u + v * f.e_v);
    }
    // 閉曲線で始終点が重複する場合は末尾を除く (閉じる辺はLinearPathが補う)
    if (uv.size() >= 3 && (uv.front() - uv.back()).norm() <= eps) {
        uv.pop_back();
        def_points.pop_back();
    }
    if (uv.size() < 2) return;

    try {
        uv_boundary_ = MakeLinearPath(uv, /*is_closed=*/true);
    } catch (const std::exception&) {
        return;
    }

    // 定義空間点群の軸平行bbox
    Vector3d mn = def_points.front();
    Vector3d mx = def_points.front();
    for (const auto& p : def_points) {
        mn = mn.cwiseMin(p);
        mx = mx.cwiseMax(p);
    }
    defined_bbox_ = i_num::BoundingBox(mn, mx);
}

std::shared_ptr<const i_ent::ICurve> BoundedPlane::GetBoundaryCurve() const {
    auto ptr = boundary_.TryGetEntity<ICurve>();
    if (!ptr) {
        throw igesio::ReferenceError(
                "BoundedPlane: boundary curve reference is not set.");
    }
    return ptr.value();
}

void BoundedPlane::SetBoundaryCurve(const std::shared_ptr<ICurve>& boundary) {
    if (boundary == nullptr) {
        throw std::invalid_argument(
                "Boundary curve of BoundedPlane cannot be nullptr.");
    }
    if (boundary->GetID() == boundary_.GetID()) {
        boundary_.SetPointer(boundary);
    } else {
        boundary_.OverwritePointer(boundary);
    }
    if (auto entity_base = std::dynamic_pointer_cast<EntityBase>(boundary)) {
        entity_base->SetSubordinateEntitySwitch(
                SubordinateEntitySwitch::kPhysicallyDependent);
    }
    outer_is_boundary_of_d_ = false;
    InvalidateGeometryCaches();
    MarkGeometryModified();
}



/**
 * ファクトリ関数
 */

std::shared_ptr<Plane> MakePlane(double a, double b, double c, double d) {
    ValidatePlaneNormalOrThrow(a, b, c, "MakePlane");
    return std::make_shared<Plane>(a, b, c, d);
}

std::shared_ptr<Plane> MakePlane(const Vector3d& normal, const Vector3d& point) {
    if (i_num::IsApproxZero(normal.norm())) {
        throw igesio::EntityValueError("MakePlane: normal must not be zero.");
    }
    // A·X+B·Y+C·Z = D, D = n·point
    return std::make_shared<Plane>(normal(0), normal(1), normal(2),
                                   normal.dot(point));
}

std::shared_ptr<BoundedPlane> MakeBoundedPlane(
        double a, double b, double c, double d,
        const std::shared_ptr<ICurve>& boundary, bool negative) {
    if (boundary == nullptr) {
        throw std::invalid_argument(
                "MakeBoundedPlane: boundary must not be nullptr.");
    }
    ValidatePlaneNormalOrThrow(a, b, c, "MakeBoundedPlane");
    return std::make_shared<BoundedPlane>(a, b, c, d, boundary, negative);
}

}  // namespace igesio::entities
