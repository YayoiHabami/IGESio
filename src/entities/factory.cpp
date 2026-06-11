/**
 * @file entities/factory.cpp
 * @brief 各エンティティクラスのファクトリクラス
 * @author Yayoi Habami
 * @date 2025-08-02
 * @copyright 2025 Yayoi Habami
 */
#include "igesio/entities/factory.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "igesio/entities/structures/unsupported_entity.h"

#include "igesio/entities/structures/null_entity.h"     // type 000
#include "igesio/entities/curves/circular_arc.h"        // type 100
#include "igesio/entities/curves/composite_curve.h"     // type 102
#include "igesio/entities/curves/conic_arc.h"           // type 104
#include "igesio/entities/curves/copious_data.h"        // type 106, forms  1- 3
#include "igesio/entities/curves/linear_path.h"         // type 106, forms 11-13
#include "igesio/entities/surfaces/plane.h"             // type 108, forms -1, 0, 1
#include "igesio/entities/curves/line.h"                // type 110
#include "igesio/entities/curves/parametric_spline_curve.h"         // type 112
#include "igesio/entities/curves/point.h"                           // type 116
#include "igesio/entities/surfaces/ruled_surface.h"                 // type 118
#include "igesio/entities/surfaces/surface_of_revolution.h"         // type 120
#include "igesio/entities/surfaces/tabulated_cylinder.h"            // type 122
#include "igesio/entities/transformations/transformation_matrix.h"  // type 124
#include "igesio/entities/curves/rational_b_spline_curve.h"         // type 126
#include "igesio/entities/surfaces/rational_b_spline_surface.h"     // type 128
#include "igesio/entities/curves/curve_on_a_parametric_surface.h"   // type 142
#include "igesio/entities/surfaces/trimmed_surface.h"               // type 144
#include "igesio/entities/structures/color_definition.h"            // type 314



namespace {

using IVec = igesio::IGESParameterVector;
using ET = igesio::entities::EntityType;
using DE = igesio::entities::RawEntityDE;
using p2I = igesio::pointer2ID;
namespace i_ent = igesio::entities;

}  // namespace



std::unordered_map<ET, i_ent::EntityFactory::CreateFunction>&
i_ent::EntityFactory::BuiltinCreators() {
    // 関数内static: 初回呼び出し時に一度だけ構築される. 静的初期化子からの
    // 参照 (自動登録) でも初期化順に依存しない
    static std::unordered_map<ET, CreateFunction> creators = [] {
        std::unordered_map<ET, CreateFunction> m;

        // エンティティタイプと作成関数のマッピングを設定
        // 0 - Null Entity
        m[ET::kNull] = [](const DE& de, const IVec& p,
                                  const p2I& d2i, const ObjectID& iid) {
            return std::make_shared<i_ent::NullEntity>(de, p, d2i, iid);
        };
        // 100 - Circular Arc
        m[ET::kCircularArc] = [](const DE& de, const IVec& p,
                                         const p2I& d2i, const ObjectID& iid) {
            return std::make_shared<i_ent::CircularArc>(de, p, d2i, iid);
        };
        // 102 - Composite Curve
        m[ET::kCompositeCurve] = [](const DE& de, const IVec& p,
                                            const p2I& d2i, const ObjectID& iid) {
            return std::make_shared<i_ent::CompositeCurve>(de, p, d2i, iid);
        };
        // 104 - Conic Arc
        m[ET::kConicArc] = [](const DE& de, const IVec& p,
                                      const p2I& d2i, const ObjectID& iid) {
            return std::make_shared<i_ent::ConicArc>(de, p, d2i, iid);
        };
        // 106 - Copious Data (CopiousDataBaseとして返す)
        m[ET::kCopiousData] = [](const DE& de, const IVec& p,
                                         const p2I& d2i, const ObjectID& iid) {
            if (de.form_number <= static_cast<int>(i_ent::CopiousDataType::kSextuples)) {
                return std::static_pointer_cast<i_ent::CopiousDataBase>(
                    std::make_shared<i_ent::CopiousData>(de, p, d2i, iid));
            } else if (de.form_number <= static_cast<int>(
                        i_ent::CopiousDataType::kPolylineAndVectors)) {
                return std::static_pointer_cast<i_ent::CopiousDataBase>(
                    std::make_shared<i_ent::LinearPath>(de, p, d2i, iid));
            }
            return std::make_shared<i_ent::CopiousDataBase>(de, p, d2i, iid);
        };

        // 108 - Plane (form 0: 無限平面 Plane, form ±1: 有界平面 BoundedPlane)
        m[ET::kPlane] = [](const DE& de, const IVec& p, const p2I& d2i,
                                   const ObjectID& iid)
                -> std::shared_ptr<i_ent::EntityBase> {
            if (de.form_number == 0) {
                return std::make_shared<i_ent::Plane>(de, p, d2i, iid);
            }
            return std::make_shared<i_ent::BoundedPlane>(de, p, d2i, iid);
        };
        // 110 - Line
        m[ET::kLine] = [](const DE& de, const IVec& p,
                                  const p2I& d2i, const ObjectID& iid) {
            return std::make_shared<i_ent::Line>(de, p, d2i, iid);
        };
        // 112 - Parametric Spline Curve
        m[ET::kParametricSplineCurve] = [](const DE& de, const IVec& p,
                                                   const p2I& d2i, const ObjectID& iid) {
            return std::make_shared<i_ent::ParametricSplineCurve>(de, p, d2i, iid);
        };
        // 116 - Point
        m[ET::kPoint] = [](const DE& de, const IVec& p,
                                   const p2I& d2i, const ObjectID& iid) {
            return std::make_shared<i_ent::Point>(de, p, d2i, iid);
        };
        // 118 - Ruled Surface
        m[ET::kRuledSurface] = [](const DE& de, const IVec& p,
                                          const p2I& d2i, const ObjectID& iid) {
            return std::make_shared<i_ent::RuledSurface>(de, p, d2i, iid);
        };
        // 120 - Surface of Revolution
        m[ET::kSurfaceOfRevolution] = [](const DE& de, const IVec& p,
                                                 const p2I& d2i, const ObjectID& iid) {
            return std::make_shared<i_ent::SurfaceOfRevolution>(de, p, d2i, iid);
        };
        // 122 - Tabulated Cylinder
        m[ET::kTabulatedCylinder] = [](const DE& de, const IVec& p,
                                               const p2I& d2i, const ObjectID& iid) {
            return std::make_shared<i_ent::TabulatedCylinder>(de, p, d2i, iid);
        };
        // 124 - Transformation Matrix
        m[ET::kTransformationMatrix] = [](const DE& de, const IVec& p,
                                                  const p2I& d2i, const ObjectID& iid) {
            return std::make_shared<i_ent::TransformationMatrix>(de, p, d2i, iid);
        };
        // 126 - Rational B-Spline Curve
        m[ET::kRationalBSplineCurve] = [](const DE& de, const IVec& p,
                                                  const p2I& d2i, const ObjectID& iid) {
            return std::make_shared<i_ent::RationalBSplineCurve>(de, p, d2i, iid);
        };
        // 128 - Rational B-Spline Surface
        m[ET::kRationalBSplineSurface] = [](const DE& de, const IVec& p,
                                                    const p2I& d2i, const ObjectID& iid) {
            return std::make_shared<i_ent::RationalBSplineSurface>(de, p, d2i, iid);
        };
        // 142 - Curve on A Parametric Surface
        m[ET::kCurveOnAParametricSurface] = [](const DE& de, const IVec& p,
                                                       const p2I& d2i, const ObjectID& iid) {
            return std::make_shared<i_ent::CurveOnAParametricSurface>(de, p, d2i, iid);
        };
        // 144 - Trimmed Surface
        m[ET::kTrimmedSurface] = [](const DE& de, const IVec& p,
                                            const p2I& d2i, const ObjectID& iid) {
            return std::make_shared<i_ent::TrimmedSurface>(de, p, d2i, iid);
        };


        // 314 - Color Definition
        m[ET::kColorDefinition] = [](const DE& de, const IVec& p,
                                             const p2I& d2i, const ObjectID& iid) {
            return std::make_shared<i_ent::ColorDefinition>(de, p, d2i, iid);
        };

        return m;
    }();
    return creators;
}

std::unordered_map<int, i_ent::EntityFactory::CreateFunction>&
i_ent::EntityFactory::UserCreators() {
    // 関数内static: 静的初期化子からの登録でも初期化順に依存しない
    static std::unordered_map<int, CreateFunction> creators;
    return creators;
}



std::shared_ptr<i_ent::EntityBase> i_ent::EntityFactory::CreateEntity(
        const RawEntityDE& de, const IVec& parameters,
        const pointer2ID& de2id, const ObjectID& iges_id) {
    auto& builtins = BuiltinCreators();
    auto& users = UserCreators();

    // ユーザー定義エンティティ (kUserDefined) は実番号で引く
    if (de.entity_type == ET::kUserDefined) {
        auto user_it = users.find(de.user_type_number);
        if (user_it != users.end()) {
            return user_it->second(de, parameters, de2id, iges_id);
        }
        // 未登録のユーザー定義番号は生データを保持し、往復出力可能とする
        return std::make_shared<i_ent::UnsupportedEntity>(de, parameters, de2id);
    }

    auto it = builtins.find(de.entity_type);
    if (it != builtins.end()) {
        return it->second(de, parameters, de2id, iges_id);
    }

    // ユーザー登録の作成関数を探す (キーはtype番号)
    auto user_it = users.find(static_cast<int>(de.entity_type));
    if (user_it != users.end()) {
        return user_it->second(de, parameters, de2id, iges_id);
    }

    // 対応するエンティティが存在しない場合はUnsupportedEntityを返す
    return std::make_shared<i_ent::UnsupportedEntity>(de, parameters, de2id);
}

std::shared_ptr<i_ent::EntityBase> i_ent::EntityFactory::CreateEntity(
        const RawEntityDE& de, const RawEntityPD& pd,
        const pointer2ID& de2id, const ObjectID& iges_id) {
    return CreateEntity(de, ToIGESParameterVector(pd), de2id, iges_id);
}

void i_ent::EntityFactory::RegisterEntityCreator(
        const EntityType type, CreateFunction creator) {
    if (!creator) {
        throw std::invalid_argument(
                "Creator function must not be empty (entity type " +
                std::to_string(static_cast<int>(type)) + ")");
    }
    // ユーザー定義番号の登録はRegisterUserEntityCreatorで行う
    if (type == ET::kUserDefined) {
        throw std::invalid_argument(
                "Use RegisterUserEntityCreator to register creators "
                "for user-defined type numbers");
    }
    // enumに存在しない値のキャスト (無効なtype番号) を弾く
    if (!ToEntityType(static_cast<int>(type)).has_value()) {
        throw std::invalid_argument(
                "Invalid entity type number: " +
                std::to_string(static_cast<int>(type)));
    }
    // 上書き禁止 (組み込み実装・ユーザー登録とも)
    auto& builtins = BuiltinCreators();
    auto& users = UserCreators();
    if (builtins.find(type) != builtins.end() ||
        users.find(static_cast<int>(type)) != users.end()) {
        throw std::invalid_argument(
                "Creator for entity type " +
                std::to_string(static_cast<int>(type)) +
                " is already registered");
    }

    users[static_cast<int>(type)] = std::move(creator);
}

bool i_ent::EntityFactory::UnregisterEntityCreator(const EntityType type) {
    // 組み込み実装 (BuiltinCreators) には触れない
    return UserCreators().erase(static_cast<int>(type)) > 0;
}

bool i_ent::EntityFactory::IsEntityCreatorRegistered(const EntityType type) {
    auto& builtins = BuiltinCreators();
    auto& users = UserCreators();
    return builtins.find(type) != builtins.end() ||
           users.find(static_cast<int>(type)) != users.end();
}

void i_ent::EntityFactory::RegisterUserEntityCreator(
        const int type_number, CreateFunction creator) {
    if (!creator) {
        throw std::invalid_argument(
                "Creator function must not be empty (entity type " +
                std::to_string(type_number) + ")");
    }
    if (!IsUserDefinedEntityNumber(type_number)) {
        throw std::invalid_argument(
                "Not a user-defined entity type number "
                "(600-699, 10000-99999): " + std::to_string(type_number));
    }
    // 上書き禁止
    auto& users = UserCreators();
    if (users.find(type_number) != users.end()) {
        throw std::invalid_argument(
                "Creator for entity type " + std::to_string(type_number) +
                " is already registered");
    }

    users[type_number] = std::move(creator);
}

bool i_ent::EntityFactory::UnregisterUserEntityCreator(const int type_number) {
    return UserCreators().erase(type_number) > 0;
}

bool i_ent::EntityFactory::IsUserEntityCreatorRegistered(const int type_number) {
    return UserCreators().find(type_number) != UserCreators().end();
}

std::shared_ptr<i_ent::EntityBase> i_ent::CloneEntity(const EntityBase& entity) {
    // 複製元自身と被参照エンティティに一時的なDEポインタを割り当てる
    igesio::id2pointer id2de;   // ID -> ポインタ
    igesio::pointer2ID de2id;   // ポインタ -> ID (元の被参照エンティティへ解決させる)
    unsigned int next = 1;
    auto assign = [&](const ObjectID& oid) {
        if (id2de.find(oid) == id2de.end()) {
            id2de[oid] = next;
            de2id[next] = oid;
            next += 2;
        }
    };
    const auto self_id = entity.GetID();
    assign(self_id);
    for (const auto& rid : entity.GetReferencedEntityIDs()) assign(rid);

    // DE/PDへシリアライズし、iges_id未指定 (UnsetID) で再構築することで新IDを採番する
    auto de = entity.GetRawEntityDE(id2de);
    de.sequence_number = id2de.at(self_id);
    // GetTypeNumber: ユーザー定義エンティティ (kUserDefined) は実番号で構築する
    auto pd = ToRawEntityPD(
            entity.GetTypeNumber(), self_id, entity.GetParameters(), id2de);
    pd.sequence_number = de.sequence_number;
    return EntityFactory::CreateEntity(de, pd, de2id, IDGenerator::UnsetID());
}
